//
//  respond.h
//  MiniHTTP
//
//  Created by Colin Luoma on 2016-07-03.
//  Copyright (c) 2016 Colin Luoma. All rights reserved.
//

#ifndef BITTYHTTP_RESPOND_H
#define BITTYHTTP_RESPOND_H

#include <stdio.h>

#define MAX(a,b) \
({ __typeof__ (a) _a = (a); \
__typeof__ (b) _b = (b); \
_a > _b ? _a : _b; })

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

void handle_request(int sock, bhttp_server *server, bhttp_request *request);

void send_header(int sock, bhttp_request *request, response_header *rh, file_stats *fs);
void send_file(int sock, char *file_path, file_stats *fs, int use_sendfile);

file_stats get_file_stats(char *file_path);
void build_header(response_header *header, file_stats *fs);

/* Get properly formatted url path from request */
char *url_path(bhttp_request *request);

/* Decodes url hex codes to plaintext */
void html_to_text(const char *source, char *dest, size_t length);

char *sanitize_path(char *path);

#endif /* BITTYHTTP_RESPOND_H */
