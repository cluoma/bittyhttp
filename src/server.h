//
//  server.h
//  bittyhttp
//
//  Created by Colin Luoma on 2016-07-03.
//  Copyright (c) 2016 Colin Luoma. All rights reserved.
//

#ifndef BITTYHTTP_SERVER_H
#define BITTYHTTP_SERVER_H

#include <sys/socket.h>
#include "request.h"
#include "respond.h"
#include "mime_types.h"

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
    pthread_t threads;
    pthread_attr_t attr;
    bhttp_server *server;
    int sock;
} thread_args;

typedef struct bhttp_req_handler
{
    bstr uri;
    int (*f)(bhttp_request *req, bhttp_response *res)
} bhttp_req_handler;

/* http server init and begin functions */
bhttp_server http_server_new(void);
int http_server_start(bhttp_server *server);
void http_server_run(bhttp_server *server);

/* add handlers */
int bhttp_server_add_handler(bhttp_server *server, const char * uri, int (*cb)(bhttp_request *req, bhttp_response *res));

#endif /* BITTYHTTP_SERVER_H */
