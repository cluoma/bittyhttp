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

#include "server.h"
#include "respond.h"
#include "http_parser.h"

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

void
http_server_run(bhttp_server *server)
{
    // setup SIGCHLD signal handling
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // Client's address information
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];

    int conn_fd;

    // Wait for connections forever
    while(1) {
        sin_size = sizeof their_addr;
        conn_fd = accept(server->sock, (struct sockaddr *)&their_addr, &sin_size);
        if (conn_fd == -1) {
            perror("accept");
            continue;
        }

        // Store client IP address into string s
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);

        // Fork and handle request
        if (!fork()) { // this is the child process
            close(server->sock); // child doesn't need the listener

            http_request request;
            init_request(&request);
            /* handle request while keep-alive requested */
            while (request.keep_alive == HTTP_KEEP_ALIVE)
            {
                /* read a new request */
                init_request(&request);
                receive_data(&request, conn_fd);

                /* handle request if no error returned */
                if (request.keep_alive != HTTP_ERROR)
                {
                    //write_log(server, &request, s);
                    handle_request(conn_fd, server, &request);
                }

                free_request(&request);
            }

            /* cleanup */
            close(conn_fd);
            exit(0);
        }
        close(conn_fd);  // parent doesn't need this
    }
}

// get sockaddr, IPv4 or IPv6:
void *
get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Reap zombie processes
void
sigchld_handler(int s)
{
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

//
void
write_log(bhttp_server *server, http_request *request, char *client_ip)
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
