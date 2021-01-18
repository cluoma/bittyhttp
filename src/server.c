//
//  server.c
//  bittyhttp
//
//  Created by Colin Luoma on 2016-07-05.
//  Copyright (c) 2016 Colin Luoma. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#include "server.h"
#include "respond.h"
#include "http_parser.h"

static void *
get_in_addr(struct sockaddr *sa)
/* get network address structure */
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

bhttp_server
http_server_new()
{
    bhttp_server server = HTTP_SERVER_DEFAULT;
    return server;
}

int
http_server_start(bhttp_server *server)
{
    struct addrinfo hints, *servinfo, *p;

    int rv;
    int yes = 1;

    // Fill hints, all unused elements must be 0 or null
    memset(&hints, 0, sizeof hints);
    //hints.ai_family = AF_USPEC; // ipv4/6 don't care
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    // Get info for us
    if ((rv = getaddrinfo(NULL, server->port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "server: getaddrinfo: %s\n", gai_strerror(rv));
        goto fail_start;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((server->sock = socket(p->ai_family, p->ai_socktype,
                                   p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        if (setsockopt(server->sock, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            goto fail_start;
        }
        if (bind(server->sock, p->ai_addr, p->ai_addrlen) == -1) {
            close(server->sock);
            perror("server: bind");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo);

    if (p == NULL ||
        listen(server->sock, server->backlog) == -1)  {
        goto fail_start;
    }

    return 0;

fail_start:
    perror("Failed to start HTTP server");
    return -1;
}

static void *
do_connection(void * arg)
{
    //printf("New thread...\n");
    bhttp_server *server = ((thread_args *)arg)->server;
    int conn_fd = ((thread_args *)arg)->sock;

    bhttp_request request;
    /* handle request while keep-alive requested */
    while (1)
    {
        /* read a new request */
        init_request(&request);
        receive_data(&request, conn_fd);

        /* handle request if no error returned */
        if (request.keep_alive == HTTP_ERROR)
        {
            free_request(&request);
            break;
        }
        else
        {
            handle_request(conn_fd, server, &request);
        }
        //write_log(server, &request, s);
        free_request(&request);
        if (request.keep_alive == HTTP_CLOSE)
            break;
    }
    /* cleanup */
    close(conn_fd);
    free(arg);
    return NULL;
}

void
http_server_run(bhttp_server *server)
/* start accepting connections */
{
    /* file descriptor for accepted connections */
    int conn_fd;
    /* client address information */
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];

    /* wait for connections forever */
    while(1) {
        sin_size = sizeof their_addr;
        //printf("Waiting on connection...\n");
        conn_fd = accept(server->sock, (struct sockaddr *)&their_addr, &sin_size);
        if (conn_fd == -1) {
            perror("accept");
            continue;
        }

        // Store client IP address into string s
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);

        /* start new thread to handle connection */
        thread_args *args = malloc(sizeof(thread_args));
        args->server = server;
        args->sock = conn_fd;
        pthread_attr_init(&(args->attr));
        pthread_attr_setdetachstate(&(args->attr), PTHREAD_CREATE_DETACHED);
        int ret = pthread_create(&(args->threads), &(args->attr), do_connection, (void *)args);
        if (ret != 0)
        {
            printf("THREAD ERROR\n");
            return;
        }
    }
}

void
write_log(bhttp_server *server, bhttp_request *request, char *client_ip)
{
    FILE *f = fopen(server->log_file, "a"); // open for writing
    if (f == NULL) return;

    // Get current time
    time_t timer;
    char buffer[26];
    struct tm* tm_info;
    time(&timer);
    tm_info = gmtime(&timer);
    strftime(buffer, 26, "%Y:%m:%d-%H:%M:%S", tm_info);

    // Log method
    fwrite(http_method_str(request->method), 1, strlen(http_method_str(request->method)), f);
    fwrite(" ", 1, 1, f);

    // Log client ip
    fwrite(client_ip, 1, strlen(client_ip), f);
    fwrite(" ", 1, 1, f);

    // Log URI
    if (strcmp(http_method_str(request->method), "<unknown>") != 0)
    {
//        fwrite(request->uri, 1, request->uri_len, f);
        fwrite(bstr_cstring(&(request->uri)), 1, bstr_size(&(request->uri)), f);
    }
    fwrite(" ", 1, 1, f);

    // Log GMT timestamp
    fwrite(buffer, 1, strlen(buffer), f);
    fwrite("\n", 1, 1, f);

    fclose(f);
}
