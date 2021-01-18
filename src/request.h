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

typedef struct bhttp_header {
    bstr field;
    bstr value;
} bhttp_header;

/* struct is populated during receive_data calls */
typedef struct bhttp_request {
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
} bhttp_request;

/* init and free */
void init_request(bhttp_request *request);
void free_request(bhttp_request *request);

/* main functions to read request */
void receive_data(bhttp_request *request, int sock);

/* returns a pointer to the value of header_key */
const char *request_header_val(bhttp_request *request, const char *header_key);

#endif /* BITTYHTTP_REQUEST_H */
