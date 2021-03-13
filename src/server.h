/*
 *  server.h
 *  bittyhttp
 *
 *  Created by Colin Luoma on 2021-01-30.
 *  Copyright (c) 2021 Colin Luoma. All rights reserved.
 */

#ifndef BITTYHTTP_SERVER_H
#define BITTYHTTP_SERVER_H

#include <regex.h>
#include <sys/socket.h>
#include "request.h"
#include "respond.h"
#include "mime_types.h"

#define SEND_BUFFER_SIZE 4096

#define BHTTP_METHOD_MAP(C) \
C(0,  DELETE)       \
C(1,  GET)          \
C(2,  HEAD)         \
C(3,  POST)         \
C(4,  PUT)          \
C(5,  CONNECT)      \
C(6,  OPTIONS)      \
C(7,  TRACE)

#define C(num, name) BHTTP_##name = 1 << (num),
typedef enum { BHTTP_UNSUPPORTED_METHOD = 0, BHTTP_METHOD_MAP(C) } bhttp_method;
#undef C

/* bhttp_server structures stores information about the current server */
typedef struct bhttp_server
{
    /* basic config */
    char *ip;
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

/* http server init and begin functions */
bhttp_server * bhttp_server_new(void);
void bhttp_server_free(bhttp_server *server);
int bhttp_server_set_ip(bhttp_server *server, const char *ip);
int bhttp_server_set_port(bhttp_server *server, const char *port);
int bhttp_server_set_docroot(bhttp_server *server, const char *docroot);
int bhttp_server_set_dfile(bhttp_server *server, const char *dfile);
int bhttp_server_bind(bhttp_server *server);
void bhttp_server_run(bhttp_server *server);

/* add handlers */
int bhttp_add_simple_handler(bhttp_server *server,
                             uint32_t methods,
                             const char *uri,
                             int (*cb)(bhttp_request *, bhttp_response *));
int bhttp_add_regex_handler(bhttp_server *server,
                            uint32_t methods,
                            const char *uri,
                            int (*cb)(bhttp_request *, bhttp_response *, bvec *));
#ifdef LUA
int bhttp_add_lua_handler(bhttp_server *server,
                          uint32_t methods,
                          const char *uri,
                          const char * lua_script_path, const char * lua_cb_func_name);
#endif

#endif /* BITTYHTTP_SERVER_H */
