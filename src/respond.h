//
//  respond.h
//  bittyhttp
//
//  Created by Colin Luoma on 2016-07-03.
//  Copyright (c) 2016 Colin Luoma. All rights reserved.
//

#ifndef BITTYHTTP_RESPOND_H
#define BITTYHTTP_RESPOND_H

#include <stdio.h>

#define TRANSFER_BUFFER 10240

typedef struct status_line status_line;
struct status_line {
    char *version;
    char *status_code;
    char *status;
};

typedef struct header_line header_line;
struct header_line {
    char *field;
    char *value;
};

typedef struct response_header response_header;
struct response_header {
    status_line status;
    header_line *headers;
};

typedef struct file_stats file_stats;
struct file_stats {
    int found;
    int isdir;
    long long bytes;
    char *name;
    char *extension;
};

typedef enum {
    BHTTP_RES_BODY_EMPTY = 0,
    BHTTP_RES_BODY_TEXT,
    BHTTP_RES_BODY_FILE_REL,
    BHTTP_RES_BODY_FILE_ABS
} bhttp_response_body_type;

typedef struct bhttp_response {
    /* first line */
    bstr first_line;
    /* headers */
    bvec headers;
    /* body */
    bhttp_response_body_type bodytype;
    bstr body;
} bhttp_response;

void bhttp_response_init(bhttp_response *res);
void bhttp_response_free(bhttp_response *res);

void handle_request(bhttp_request *req, bhttp_response *res);
bstr * bhttp_res_headers_to_string(bhttp_response *res);

/* interface for handlers */
int bhttp_res_add_header(bhttp_response *res, const char *field, const char *value);
int bhttp_res_set_body_text(bhttp_response *res, const char *s);
int bhttp_res_set_body_file(bhttp_response *res, const char *s, int isabs);
int bhttp_res_set_body_file_rel(bhttp_response *res, const char *s);
int bhttp_res_set_body_file_abs(bhttp_response *res, const char *s);

int send_file(int sock, char *file_path, size_t file_size, int use_sendfile);
int default_file_handler(bhttp_request *req, bhttp_response *res);

#endif /* BITTYHTTP_RESPOND_H */
