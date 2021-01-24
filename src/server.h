//
//  server.h
//  bittyhttp
//
//  Created by Colin Luoma on 2016-07-03.
//  Copyright (c) 2016 Colin Luoma. All rights reserved.
//

#ifndef BITTYHTTP_SERVER_H
#define BITTYHTTP_SERVER_H

#include <regex.h>
#include <sys/socket.h>
#include "request.h"
#include "respond.h"
#include "mime_types.h"

//#define BHTTP_METHOD_MAP(XX)         \
//XX(0,  DELETE,      DELETE)       \
//XX(1,  GET,         GET)          \
//XX(2,  HEAD,        HEAD)         \
//XX(3,  POST,        POST)         \
//XX(4,  PUT,         PUT)
//
//enum bhttp_method
//{
//#define XX(num, name, string) BHTTP_##name = 1 << num,
//    BHTTP_METHOD_MAP(XX)
//#undef XX
//};

/* bhttp_server structures stores information about the current server */
typedef struct bhttp_server
{
    /* basic config */
    char *port;
    char *docroot;
    char *log_file;
    char *default_file;
    int backlog;

    /* request handlers */
    bvec handlers;

    /* not-so-basic config */
    int use_sendfile;
    int daemon;

    /* main socket */
    int sock;
} bhttp_server;

/* Default bhttp_server values for when arguments are missing */
static const bhttp_server HTTP_SERVER_DEFAULT = {
    .port = "3490",
    .backlog = 10,
    .docroot = "./docroot",
    .log_file = "./bittblog.log",
    .default_file = "index.html",
    .use_sendfile = 0,
    .daemon = 0,
    .sock = 0
};

typedef struct
{
    pthread_t thread;
    pthread_attr_t attr;
    bhttp_server *server;
    int sock;
} thread_args;

typedef enum {
    BHTTP_HANDLER_SIMPLE = 0,
    BHTTP_HANDLER_REGEX,
} bhttp_handler_type;

typedef struct bhttp_handler
{
    bhttp_handler_type type;
    uint32_t method;
    bstr match;
    int (*f_simple)(bhttp_request *req, bhttp_response *res);
    int (*f_regex)(bhttp_request *req, bhttp_response *res, bvec *args);
    /* for regex */
    regex_t regex_buf;
} bhttp_handler;

/* http server init and begin functions */
bhttp_server http_server_new(void);
int http_server_start(bhttp_server *server);
void http_server_run(bhttp_server *server);

/* add handlers */
int bhttp_add_simple_handler(bhttp_server *server,
                             const char * uri,
                             int (*cb)(bhttp_request *req, bhttp_response *res));
int bhttp_add_regex_handler(bhttp_server *server,
                            const char * uri,
                            int (*cb)(bhttp_request *req, bhttp_response *res, bvec *args));

#endif /* BITTYHTTP_SERVER_H */
