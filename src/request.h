//
//  request.h
//  MiniHTTP
//
//  Created by Colin Luoma on 2016-06-27.
//  Copyright (c) 2016 Colin Luoma. All rights reserved.
//

#ifndef BITTYHTTP_REQUEST_H
#define BITTYHTTP_REQUEST_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "http_parser.h"
#include "bittystring.h"
#include "bittyvec.h"

#define REQUEST_BUF_SIZE 500

#define HTTP_KEEP_ALIVE     0
#define HTTP_CLOSE          1
#define HTTP_ERROR          2

typedef struct http_header {
    bstr field;
    bstr value;
} http_header;

/*
 * Struct is populated when parsing
 */
typedef struct http_request http_request;
struct http_request {
    // Request buffer
    char *request;
    size_t request_len;

    // First line
    int method;
    bstr uri;
    struct http_parser_url parser_url;

    // HTTP version
    char *version;
    size_t version_len;

    size_t content_length;
    size_t header_length;

    // Keep-alive
    unsigned int keep_alive;

    // Headers
    bvec headers;

    // Body
    char *body;
    size_t body_len;
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

/* Set keep-alive status from client request */
void set_keep_alive(http_request *request);

char *request_header_val(http_request *request, const char*header_key);

/* Free memory used by http_request */
void init_request(http_request *request);
void free_request(http_request *request);

/* Main functions to read request */
void receive_data(int sock, http_parser *parser);
ssize_t read_chunk(int sock, char **str, ssize_t t_recvd, size_t chunk_size);


#endif /* BITTYHTTP_REQUEST_H */
