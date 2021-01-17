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
    /* first line */
    int method;
    bstr uri;
//    char *version;
//    size_t version_len;
    /* headers */
    bvec headers;
    /* body */
    bstr body;

    /* parser */
    struct http_parser parser;
    struct http_parser_settings settings;
    struct http_parser_url parser_url;
    /* keep-alive */
    unsigned int keep_alive;
    /* request is done being parsed */
    unsigned int done;
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

char *request_header_val(http_request *request, const char*header_key);

/* Free memory used by http_request */
void init_request(http_request *request);
void free_request(http_request *request);

/* Main functions to read request */
void receive_data(http_request *request, int sock);

#endif /* BITTYHTTP_REQUEST_H */
