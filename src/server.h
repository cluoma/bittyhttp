//
//  server.h
//  MiniHTTP
//
//  Created by Colin Luoma on 2016-07-03.
//  Copyright (c) 2016 Colin Luoma. All rights reserved.
//

#ifndef BITTYHTTP_SERVER_H
#define BITTYHTTP_SERVER_H

#include <sys/socket.h>
#include "request.h"

/* bhttp_server structures stores information about the current server */
typedef struct
{
    /* basic config */
    char *port;
    char *docroot;
    char *log_file;
    char *default_file;
    int backlog;

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

/* http server init and begin functions */
bhttp_server http_server_new(void);
int http_server_start(bhttp_server *server);
void http_server_run(bhttp_server *server);

/* Write to server logfile */
void write_log(bhttp_server *server, bhttp_request *request, char *client_ip);

/* Reaps child processes from when requests are finished */
void sigchld_handler(int s);

#endif /* BITTYHTTP_SERVER_H */
