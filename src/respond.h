/*
 *  respond.h
 *  bittyhttp
 *
 *  Created by Colin Luoma on 2021-01-30.
 *  Copyright (c) 2021 Colin Luoma. All rights reserved.
 */

#ifndef BITTYHTTP_RESPOND_H
#define BITTYHTTP_RESPOND_H

#include <stdio.h>

#define TRANSFER_BUFFER 10240

#define BHTTP_RES_CODES C(BHTTP_200_OK, "200 OK")                   \
                        C(BHTTP_400, "400 Bad Request")             \
                        C(BHTTP_404, "404 Not Found")               \
                        C(BHTTP_500, "500 Internal Server Error")   \
                        C(BHTTP_501, "501 Not Implemented")
#define C(k, v) k,
typedef enum { BHTTP_RES_CODES } bhttp_res_codes;
#undef C

typedef enum {
    BHTTP_RES_BODY_EMPTY = 0,
    BHTTP_RES_BODY_TEXT,
    BHTTP_RES_BODY_FILE_REL,
    BHTTP_RES_BODY_FILE_ABS
} bhttp_response_body_type;

typedef struct bhttp_response {
    /* first line */
    int response_code;
    /* headers */
    bvec headers;
    /* body */
    bhttp_response_body_type bodytype;
    bstr body;
} bhttp_response;

void bhttp_response_init(bhttp_response *res);
void bhttp_response_free(bhttp_response *res);

bstr * bhttp_res_headers_to_string(bhttp_response *res);

/* interface for handlers */
int bhttp_res_add_header(bhttp_response *res, const char *field, const char *value);
bhttp_header *bhttp_res_get_header(bhttp_response *res, const char *field);
int bhttp_res_set_body_text(bhttp_response *res, const char *s);
int bhttp_res_set_body_file_rel(bhttp_response *res, const char *s);
int bhttp_res_set_body_file_abs(bhttp_response *res, const char *s);

int default_404_handler(bhttp_request *req, bhttp_response *res);
int default_file_handler(bhttp_request *req, bhttp_response *res);

#endif /* BITTYHTTP_RESPOND_H */
