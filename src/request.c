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

void
receive_data(int sock, http_parser *parser)
{
    http_request *request = parser->data;
    init_request(request);

    // Init http parser settings
    http_parser_settings settings;
    http_parser_settings_init(&settings);
    settings.on_message_begin    = start_cb;
    settings.on_url              = url_cb;
    settings.on_header_field     = header_field_cb;
    settings.on_header_value     = header_value_cb;
    settings.on_headers_complete = header_end_cb;
    settings.on_body             = body_cb;

    char *str = malloc(REQUEST_BUF_SIZE+1);
    if (str == NULL && errno == ENOMEM) {
        goto bad;
    }

    ssize_t t_recvd = 0;
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
          (n_recvd = read_chunk(sock, &str, t_recvd, REQUEST_BUF_SIZE)) > 0)
    {
        t_recvd += n_recvd;

        // Reinitialize timeout and select set
	FD_ZERO(&set);
	FD_SET(sock, &set);
        timeout.tv_sec = 5;
        timeout.tv_usec = 50000;

        // Got end of headers, break out
        char *tmp;
        if ((tmp = strstr(str, "\r\n\r\n")) != NULL) {
            request->header_length = tmp - str + 4;
            http_parser_execute(parser, &settings, str, request->header_length);
            break;
        }
    }

    // Reinitialize timeout and select set
    FD_ZERO(&set);
    FD_SET(sock, &set);
    timeout.tv_sec = 5;
    timeout.tv_usec = 50000;

    // Do we need more data based on content-length?
    while ((t_recvd < request->content_length + request->header_length) &&
           (sel = select(sock+1, &set, NULL, NULL, &timeout) > 0) &&
           (n_recvd = read_chunk(sock, &str, t_recvd, REQUEST_BUF_SIZE)) > 0)
    {
        t_recvd += n_recvd;

        // Reinitialize timeout and select set
	FD_ZERO(&set);
	FD_SET(sock, &set);
        timeout.tv_sec = 5;
        timeout.tv_usec = 50000;
    }

    // Something went wrong
    // Connection closed by client, recv error
    // Select timeout
    if (sel == 0)
    {
        //perror("receive data timeout");
        goto bad;
    }
    if (sel < 0) {
        perror("receive data select error");
        goto bad;
    }
    if (n_recvd == 0) {
        // Connection closed
        goto bad;
    }
    if (n_recvd < 0) {
        perror("receive data amount error");
        goto bad;
    }

    // Parse the rest of the input
    http_parser_execute(parser, &settings,
                        str+(request->header_length),
                        t_recvd-(request->header_length));

    // Store request string, and length
    request->request = str;
    request->request_len = t_recvd;

    // Was a keep-alive requested?
    set_keep_alive(request);

    print_headers(request);

    return;

bad:
    // Free string and mark an error in request
    if (str != NULL)
        free(str);
    request->keep_alive = HTTP_ERROR;
    return;
}

// Reads maximum of 'chunk_size' bytes from socket into str
// Returns actual number of bytes read
ssize_t
read_chunk(int sock, char **str, ssize_t t_recvd, size_t chunk_size)
{
    char *tmp = (*str);
    ssize_t n_recvd = recv(sock, tmp+t_recvd, chunk_size, 0);

    if (n_recvd == 0) // Connection closed by client
    {
        return n_recvd;
    }

    if (n_recvd == -1) { // recv error
        //fprintf(stderr, "RECV\n");
        perror("RECV");
        fprintf(stderr, "n_recvd: %d\n", (int)n_recvd);
        return n_recvd;
    }

    tmp = realloc(tmp, t_recvd + n_recvd + chunk_size + 1);
    if (tmp == NULL && errno == ENOMEM) { // realloc error
        fprintf(stderr, "REALLOC\n");
        return -1;
    }

    tmp[t_recvd+n_recvd] = '\0';
    (*str) = tmp;
//    printf("postdata: %s\n", (*str));
    return n_recvd;
}

// Checks HTTP headers for a keep-alive request
void
set_keep_alive(http_request *request)
{
//    for (size_t i = 0; i < request->header_fields; i++)
//    {
//        if (strncasecmp(request->header_field[i], "Connection", request->header_field_len[i]) == 0)
//        {
//            if (strncasecmp(request->header_value[i], "keep-alive", request->header_value_len[i]) == 0)
//            {
//                request->keep_alive = HTTP_KEEP_ALIVE;
//            }
//            else
//            {
//                request->keep_alive = HTTP_CLOSE;
//            }
//            return;
//        }
//    }
    request->keep_alive = HTTP_CLOSE;
}

//// Return the header value for a given header key
//// Caller must free afterwards
//char *
//request_header_val(http_request *request, const char*header_key)
//{
//    for (size_t i = 0; i < request->header_fields; i++)
//    {
//        if (strncasecmp(request->header_field[i], header_key, request->header_field_len[i]) == 0)
//        {
//            char *header_val = calloc(1, request->header_value_len[i] + 1);
//            memcpy(header_val, request->header_value[i], request->header_value_len[i]);
//            return header_val;
//        }
//    }
//    return NULL;
//}

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
    bstr_free_buf(&(h->field));
    bstr_free_buf(&(h->value));
    free(h);
}

void
init_request(http_request *request)
{
    request->keep_alive = HTTP_KEEP_ALIVE;
    request->request = NULL;
    request->request_len = 0;
    bstr_init(&(request->uri));
    request->content_length = 0;
    request->header_length = 0;
    bvec_init(&(request->headers), (void (*)(void *)) &http_header_free);
    request->body = NULL;
    request->body_len = 0;

    http_parser_url_init(&(request->parser_url));
}

void
free_request(http_request *request)
{
    if (request->request != NULL)
        free(request->request);

    bvec_free_contents(&(request->headers));

    // Free request body
    if (request->body != NULL)
        free(request->body);

    bstr_free_buf(&(request->uri));
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
    http_request *request = parser->data;
    if (bstr_append_cstring(&(request->uri), at, length) != BS_SUCCESS)
        return 1;
    if (http_parser_parse_url(at, length, 0, &(request->parser_url)) != 0)
        return 1;
    return 0;
}

int
header_field_cb(http_parser* parser, const char *at, size_t length)
{
    http_request *request = parser->data;
    bvec *headers = &(request->headers);
    http_header *h = http_header_new();
    if (h == NULL)
        return 1;
    if (bstr_append_cstring(&(h->field), at, length) != BS_SUCCESS)
        return 1;
    bvec_add(headers, h);
    return 0;
}

int
header_value_cb(http_parser* parser, const char *at, size_t length)
{
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

    // Get content length
    if (parser->content_length < ULLONG_MAX)
    {
        request->content_length = (size_t)parser->content_length;
    }

    // Get http method
    request->method = parser->method;

    return 0;
}

int
body_cb(http_parser* parser, const char *at, size_t length)
{
    if (length == 0) return 0;

    http_request *request = parser->data;
    request->body = realloc(request->body, request->body_len + length);
    memcpy(request->body + request->body_len, at, length);
    request->body_len += length;
    return 0;
}
