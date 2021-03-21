/*
 *  request.c
 *  bittyhttp
 *
 *  Created by Colin Luoma on 2021-01-30.
 *  Copyright (c) 2021 Colin Luoma. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "request.h"
#include "server.h"

#define REQUEST_BUF_SIZE 1024

static const char unhex_tbl[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        0, 1, 2, 3, 4, 5, 6, 7,  8, 9,-1,-1,-1,-1,-1,-1,
        -1,10,11,12,13,14,15,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,10,11,12,13,14,15,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1
};

/*
 * Request parsing callback functions
 * all callbacks return 0 on success, non-zero otherwise
 */
int start_cb(http_parser* parser);
int url_cb(http_parser* parser, const char *at, size_t length);
int header_field_cb(http_parser* parser, const char *at, size_t length);
int header_value_cb(http_parser* parser, const char *at, size_t length);
int header_end_cb(http_parser* parser);
int body_cb(http_parser* parser, const char *at, size_t length);
int message_end_cb(http_parser* parser);

static void
init_parser(bhttp_request *request)
{
    /* main parser */
    http_parser_init(&request->parser, HTTP_REQUEST);
    /* url parser */
    http_parser_url_init(&request->parser_url);
    /* callbacks */
    http_parser_settings_init(&request->settings);
    request->settings.on_message_begin    = start_cb;
    request->settings.on_url              = url_cb;
    request->settings.on_header_field     = header_field_cb;
    request->settings.on_header_value     = header_value_cb;
    request->settings.on_headers_complete = header_end_cb;
    request->settings.on_body             = body_cb;
    request->settings.on_message_complete = message_end_cb;
}

void
bhttp_request_init(bhttp_request *request)
{
    request->keep_alive = BHTTP_CLOSE;
    bstr_init(&request->uri);
    bstr_init(&request->uri_path);
    bstr_init(&request->uri_query);
    bvec_init(&request->headers, (void (*)(void *)) &bhttp_header_free);
    bstr_init(&request->body);
    request->done = 0;
    init_parser(request);
    request->parser.data = request;
}

void
bhttp_request_free(bhttp_request *request)
{
    bstr_free_contents(&request->uri);
    bstr_free_contents(&request->uri_path);
    bstr_free_contents(&request->uri_query);
    bvec_free_contents(&request->headers);
    bstr_free_contents(&request->body);
}

static void
print_headers(bhttp_request *request)
/* prints headers for debugging */
{
    bvec *headers = &request->headers;
    for (int i = 0; i < bvec_count(headers); i++)
    {
        bhttp_header *h = (bhttp_header *)bvec_get(headers, i);
        printf("%s: %s\n", bstr_cstring(&h->field), bstr_cstring(&h->value));
    }
    printf("\n");
}

static int
url_decode(const char *source, bstr *dest, size_t length)
/* decodes URL string and append to dest (eg '+' -> ' ' and % hex codes) */
{
    size_t n = 0;
    char c, hex1, hex2;
    if (source == NULL || length == 0)
    {
        return 0;
    }

    while((c = *source) != '\0' && n < length)
    {
        if(c == '%')
        {
            if((hex1 = unhex_tbl[(unsigned char)*++source]) < 0 ||
               (hex2 = unhex_tbl[(unsigned char)*++source]) < 0) {
                return 1;
            }
            c = (hex1 << 4) | hex2;
            n += 2;
        }
        else if (c == '+')
        {
            c = ' ';
        }
        bstr_append_char(dest, c);
        n++;
        source ++;
    }
    return 0;
}

static int
url_to_path_and_query(bhttp_request *req)
{
    /* check url validity */
    if (http_parser_parse_url(bstr_cstring(&req->uri),
                              bstr_size(&req->uri),
                              0,
                              &req->parser_url) != 0)
        return 1;
    /* extract path */
    if ((req->parser_url.field_set >> UF_PATH) & 1)
    {
        int r = url_decode(bstr_cstring(&req->uri)+(req->parser_url.field_data[UF_PATH].off),
                           &req->uri_path,
                           req->parser_url.field_data[UF_PATH].len);
        if (r != 0) return 1;
    }
    else
        return 1;
    /* extract query string */
    if ((req->parser_url.field_set >> UF_QUERY) & 1)
    {
        int r = bstr_append_cstring(&req->uri_query,
                                    bstr_cstring(&req->uri)+(req->parser_url.field_data[UF_QUERY].off),
                                    req->parser_url.field_data[UF_QUERY].len);
        if (r != 0) return 1;
    }
    return 0;
}

static int
wait_for_sock(int sock)
/* waits on socket to be ready for up to timeout before returning */
{
    fd_set set;
    struct timeval timeout;
    FD_ZERO (&set);
    FD_SET (sock, &set);
    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = 0;

    int sel = select(sock+1, &set, NULL, NULL, &timeout);
    return sel;
}

int
receive_data(bhttp_request *request, int sock)
/* reads data from socket */
{
    http_parser *parser = &(request->parser);
    http_parser_settings *settings = &(request->settings);
    char buf[REQUEST_BUF_SIZE];
    ssize_t n_recvd = 0;
    int sel;

    /* wait on socket, read chunk, parse, repeat */
    while((sel = wait_for_sock(sock) > 0) &&
          (n_recvd = recv(sock, buf, REQUEST_BUF_SIZE, 0)) > 0)
    {
        http_parser_execute(parser, settings, buf, n_recvd);
        if (parser->http_errno != HPE_OK)
        {
            fprintf(stderr, "parser error: %s\n", http_errno_description(parser->http_errno));
            return BHTTP_REQ_ERROR;
        }
        //print_headers(request);

        if (request->done)
            break;
    }

    /* why did we break */
    if (sel == 0)
    {
        perror("select timeout");
        return BHTTP_REQ_ERROR;
    }
    else if (sel < 0) {
        perror("select error");
        return BHTTP_REQ_ERROR;
    }
    else if (n_recvd == 0) {
        perror("recv client closed connection");
        return BHTTP_REQ_ERROR;
    }
    else if (n_recvd < 0) {
        perror("recv error");
        return BHTTP_REQ_ERROR;
    }
    return BHTTP_REQ_OK;
}

bhttp_header *
bhttp_req_get_header(bhttp_request *req, const char *field)
/* returns the bhttp_header request header with the given field */
{
    bvec *headers = &req->headers;
    for (int i = 0; i < bvec_count(headers); i++)
    {
        bhttp_header *cur = (bhttp_header *)bvec_get(headers, i);
        bstr *hf = &cur->field;
        if (strcasecmp(field, bstr_cstring(hf)) == 0)
            return cur;
    }
    return NULL;
}

/*
 * HTTP Header parsing callbacks
 */
int
start_cb(http_parser* parser)
{
    return 0;
}

int
url_cb(http_parser* parser, const char *at, size_t length)
{
    if (length == 0) return 0;
    bhttp_request *request = parser->data;
    if (bstr_append_cstring(&(request->uri), at, length) != BS_SUCCESS)
        return 1;
    return 0;
}

int
header_field_cb(http_parser* parser, const char *at, size_t length)
{
    if (length == 0) return 0;
    bhttp_request *request = parser->data;
    bvec *headers = &(request->headers);

    int num_headers = bvec_count(headers);
    bhttp_header *h = num_headers > 0 ? bvec_get(headers, num_headers - 1) : NULL;
    /* if this is first header, or last cb was a value, make a new field */
    if (h == NULL || bstr_size(&h->value) > 0)
    {
        h = bhttp_header_new();
        if (h == NULL)
            return 1;
        bvec_add(headers, h);
    }

    for (size_t i = 0; i < length; i++)
    {
        if (bstr_append_char(&h->field, (char)tolower((unsigned char)at[i])) != BS_SUCCESS)
            return 1;
    }
    return 0;
}

int
header_value_cb(http_parser* parser, const char *at, size_t length)
{
    if (length == 0) return 0;
    bhttp_request *request = parser->data;
    bvec *headers = &(request->headers);
    bhttp_header *h = (bhttp_header *)bvec_get(headers, bvec_count(headers) - 1);
    if (bstr_append_cstring(&(h->value), at, length) != BS_SUCCESS)
        return 1;
    return 0;
}

int
header_end_cb(http_parser* parser)
{
    bhttp_request *req = parser->data;
    /* parse URI */
    if (url_to_path_and_query(req) != 0)
        return 1;

    /* get http methods */
    req->method = (int)parser->method > HTTP_TRACE ? BHTTP_UNSUPPORTED_METHOD : 1 << (int)parser->method;
    /* check keep-live */
    if (http_should_keep_alive(parser) == 0)
        req->keep_alive = BHTTP_CLOSE;
    else
        req->keep_alive = BHTTP_KEEP_ALIVE;

    return 0;
}

int
body_cb(http_parser* parser, const char *at, size_t length)
{
    if (length == 0) return 0;
    bhttp_request *request = parser->data;
    if (bstr_append_cstring(&(request->body), at, length) != BS_SUCCESS)
        return 1;
    return 0;
}

int
message_end_cb(http_parser* parser)
{
    bhttp_request *request = parser->data;
    request->done = 1;
    return 0;
}
