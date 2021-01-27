//
//  request.h
//  bittyhttp
//
//  Created by Colin Luoma on 2016-06-27.
//  Copyright (c) 2016 Colin Luoma. All rights reserved.
//

#ifndef BITTYHTTP_REQUEST_H
#define BITTYHTTP_REQUEST_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "header.h"
#include "http_parser.h"
#include "bittystring.h"
#include "bittyvec.h"

#define BHTTP_KEEP_ALIVE     0
#define BHTTP_CLOSE          1

#define TIMEOUT_SECONDS     5

typedef enum {
    BHTTP_REQ_OK = 0,
    BHTTP_REQ_ERROR
} bhttp_request_ret;

/* struct is populated during receive_data calls */
typedef struct bhttp_request {
    /* first line */
    int method;
    bstr uri;
    bstr uri_path;
    bstr uri_query;
//    char *version;
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
void bhttp_request_init(bhttp_request *request);
void bhttp_request_free(bhttp_request *request);

/* main functions to read request */
int receive_data(bhttp_request *request, int sock);

/* returns a pointer to the value of header_key */
bhttp_header *bhttp_req_get_header(bhttp_request *req, const char *field);

#endif /* BITTYHTTP_REQUEST_H */
