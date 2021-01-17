//
//  request.c
//  MiniHTTP
//
//  Created by Colin Luoma on 2016-06-27.
//  Copyright (c) 2016 Colin Luoma. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <limits.h>
#include "request.h"

#define REQUEST_BUF_SIZE 1024

static void
print_headers(http_request *request)
{
    bvec *headers = &(request->headers);
    for (int i = 0; i < bvec_count(headers); i++)
    {
        http_header *h = (http_header *)bvec_get(headers, i);
        printf("%s: %s\n", bstr_cstring(&(h->field)), bstr_cstring(&(h->value)));
    }
    printf("\n");
}

static ssize_t
read_chunk(int sock, char *buf, size_t buf_size)
/* reads up to buf_size from sock into buf and return bytes read */
{
    ssize_t n_recvd = recv(sock, buf, buf_size, 0);

    if (n_recvd == 0) // Connection closed by client
    {
        return n_recvd;
    }

    if (n_recvd == -1) { // recv errorr
        perror("RECV");
        fprintf(stderr, "n_recvd: %d\n", (int)n_recvd);
        return n_recvd;
    }

    return n_recvd;
}

void
receive_data(http_request *request, int sock)
{
    http_parser *parser = &(request->parser);
    http_parser_settings *settings = &(request->settings);
    char buf[REQUEST_BUF_SIZE];
    ssize_t n_recvd = 0;
    int sel;

    // Structures for select
    fd_set set;
    struct timeval timeout;
    FD_ZERO (&set);
    FD_SET (sock, &set);
    timeout.tv_sec = 5; //1 seconds
    timeout.tv_usec = 50000; //0.4 seconds

    // Read up to end of header received
    while((sel = select(sock+1, &set, NULL, NULL, &timeout) > 0) &&
          (n_recvd = read_chunk(sock, buf, REQUEST_BUF_SIZE)) > 0)
    {
        http_parser_execute(parser, settings, buf, n_recvd);
        if (parser->http_errno != HPE_OK)
        {
            printf("\nPARSER ERROR\n%s\n", http_errno_description(parser->http_errno));
            goto bad;
        }
        //print_headers(request);

        if (request->done)
            break;

        // Reinitialize timeout and select set
	    FD_ZERO(&set);
	    FD_SET(sock, &set);
        timeout.tv_sec = 5;
        timeout.tv_usec = 50000;
    }

    /* Something went wrong */
    if (sel == 0)
    {
        perror("receive data timeout");
        goto bad;
    }
    if (sel < 0) {
        perror("receive data select error");
        goto bad;
    }
    if (n_recvd <= 0) {
        perror("receive data amount error or client closed connection");
        goto bad;
    }
    return;

bad:
    request->keep_alive = HTTP_ERROR;
}

// Return the header value for a given header key
// Caller must free afterwards
char *
request_header_value(http_request *request, const char * header_field)
{
//    for (size_t i = 0; i < bvec_count(&(request->headers)); i++)
//    {
//        bstr *hf = (bstr *)bvec_get(&(request->headers), i);
//        if (strncasecmp())
//    }
//    for (size_t i = 0; i < request->header_fields; i++)
//    {
//        if (strncasecmp(request->header_field[i], header_key, request->header_field_len[i]) == 0)
//        {
//            char *header_val = calloc(1, request->header_value_len[i] + 1);
//            memcpy(header_val, request->header_value[i], request->header_value_len[i]);
//            return header_val;
//        }
//    }
    return NULL;
}

static http_header *
http_header_new()
{
    http_header *h = malloc(sizeof(http_header));
    if (h == NULL)
        return NULL;
    bstr_init(&(h->field));
    bstr_init(&(h->value));
    return h;
}

static void
http_header_free(http_header *h)
{
    bstr_free_contents(&(h->field));
    bstr_free_contents(&(h->value));
    free(h);
}

static void
init_parser(http_request *request)
{
    /* main parser */
    http_parser_init(&(request->parser), HTTP_REQUEST);
    /* url parser */
    http_parser_url_init(&(request->parser_url));
    /* callbacks */
    http_parser_settings_init(&(request->settings));
    request->settings.on_message_begin    = start_cb;
    request->settings.on_url              = url_cb;
    request->settings.on_header_field     = header_field_cb;
    request->settings.on_header_value     = header_value_cb;
    request->settings.on_headers_complete = header_end_cb;
    request->settings.on_body             = body_cb;
    request->settings.on_message_complete = message_end_cb;
}

void
init_request(http_request *request)
{
    request->keep_alive = HTTP_KEEP_ALIVE;
    bstr_init(&(request->uri));
    bvec_init(&(request->headers), (void (*)(void *)) &http_header_free);
    bstr_init(&(request->body));
    request->done = 0;
    init_parser(request);
    request->parser.data = request;
}

void
free_request(http_request *request)
{
    bstr_free_contents(&(request->uri));
    bvec_free_contents(&(request->headers));
    bstr_free_contents(&(request->body));
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
    http_request *request = parser->data;
    if (bstr_append_cstring(&(request->uri), at, length) != BS_SUCCESS)
        return 1;
    return 0;
}

int
header_field_cb(http_parser* parser, const char *at, size_t length)
{
    if (length == 0) return 0;
    http_request *request = parser->data;
    bvec *headers = &(request->headers);

    int num_headers = bvec_count(headers);
    http_header *h = num_headers > 0 ? bvec_get(headers, num_headers-1) : NULL;
    /* if first header, or last cb was a value, make a new field */
    if (h == NULL || bstr_size(&(h->value)) > 0)
    {
        h = http_header_new();
        if (h == NULL)
            return 1;
        bvec_add(headers, h);
    }

    if (bstr_append_cstring(&(h->field), at, length) != BS_SUCCESS)
        return 1;

    return 0;
}

int
header_value_cb(http_parser* parser, const char *at, size_t length)
{
    if (length == 0) return 0;
    http_request *request = parser->data;
    bvec *headers = &(request->headers);
    http_header *h = (http_header *)bvec_get(headers, bvec_count(headers)-1);
    bstr_append_cstring(&(h->value), at, length);
    return 0;
}

int
header_end_cb(http_parser* parser)
{
    http_request *request = parser->data;
    /* parse URI */
    if (http_parser_parse_url(bstr_cstring(&(request->uri)),
                              bstr_size(&(request->uri)),
                              0,
                              &(request->parser_url)) != 0)
        return 1;

    /* get http method */
    request->method = parser->method;
    /* check keep-live */
    if (http_should_keep_alive(parser) == 0)
        request->keep_alive = HTTP_CLOSE;
    else
        request->keep_alive = HTTP_KEEP_ALIVE;

    return 0;
}

int
body_cb(http_parser* parser, const char *at, size_t length)
{
    if (length == 0) return 0;
    http_request *request = parser->data;
    bstr_append_cstring(&(request->body), at, length);
    return 0;
}

int
message_end_cb(http_parser* parser)
{
    http_request *request = parser->data;
    request->done = 1;
    return 0;
}
