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
#include <sys/stat.h>

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
    bvec_init(&server.handlers, NULL);
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

// Needs a lot of work
static file_stats
get_file_stats(const char *file_path)
{
    file_stats fs;
    struct stat s;

    if (stat(file_path, &s) == -1) {  // Error in stat
        fs.found = 0;
        fs.isdir = 0;
    } else if (S_ISDIR(s.st_mode)) {  // Found a directory
        fs.found = 1;
        fs.isdir = 1;
    } else if (S_ISREG(s.st_mode)) {  // Found a file
        fs.found = 1;
        fs.isdir = 0;
        fs.bytes = s.st_size;
        fs.extension = strrchr(file_path, '.') + 1; // +1 because of '.'
        fs.name = NULL;
    } else {  // Anything else we pretend we didn't find it
        fs.found = 0;
        fs.isdir = 0;
    }

    return fs;
}

static void
send_404_response(int sock, bhttp_response *res)
{
    /* add our own headers and set 404 message */
    bstr_append_cstring_nolen(&res->first_line, "HTTP/1.1 404 NOT FOUND\r\n");
    bhttp_res_set_body_text(res, "<html><p>bittyhttp: 404 - NOT FOUND</p><html>");
    bhttp_res_add_header(res, "content-type", "text/html");
    bstr tmp; bstr_init(&tmp); bstr_append_printf(&tmp, "%d", bstr_size(&res->body));
    bhttp_res_add_header(res, "content-length", bstr_cstring(&tmp));
    bstr_free_contents(&tmp);

    /* send header */
    bstr *header_text = bhttp_res_headers_to_string(res);
    printf("%s", bstr_cstring(header_text));
    send(sock, bstr_cstring(header_text), bstr_size(header_text), 0);
    bstr_free(header_text);
    /* send body */
    send(sock, bstr_cstring(&res->body), bstr_size(&res->body), 0);
}

static void
write_response(int sock, bhttp_response *res)
{
    bhttp_res_add_header(res, "server", "bittyhttp");

    if (res->bodytype == BHTTP_RES_BODY_TEXT)
    {
        /* add our own headers */
        bstr_append_cstring_nolen(&(res->first_line), "HTTP/1.1 200 OK\r\n");
        bhttp_res_add_header(res, "content-type", "text/html");
        bstr tmp; bstr_init(&tmp); bstr_append_printf(&tmp, "%d", bstr_size(&res->body));
        bhttp_res_add_header(res, "content-length", bstr_cstring(&tmp));
        bstr_free_contents(&tmp);

        /* send header */
        bstr *header_text = bhttp_res_headers_to_string(res);
        printf("%s", bstr_cstring(header_text));
        send(sock, bstr_cstring(header_text), bstr_size(header_text), 0);
        bstr_free(header_text);
        /* send body */
        send(sock, bstr_cstring(&res->body), bstr_size(&res->body), 0);
    }
    else if (res->bodytype == BHTTP_RES_BODY_FILE_REL ||
             res->bodytype == BHTTP_RES_BODY_FILE_ABS)
    {

        bstr *file_path = bstr_new();
        if (res->bodytype == BHTTP_RES_BODY_FILE_REL)
            bstr_append_cstring_nolen(file_path, "/home/colin/Documents/bittyhttp/www/");
        bstr_append_cstring_nolen(file_path, bstr_cstring(&res->body));
        file_stats fs = get_file_stats(bstr_cstring(file_path));
        /* found directory, look for default file */
        if (fs.found && fs.isdir) {
            bstr_append_char(file_path, '/');
            bstr_append_cstring_nolen(file_path, "index.html");
            fs = get_file_stats(bstr_cstring(file_path));
        }
        /* found file */
        if (fs.found && !fs.isdir)
        {
            /* add our own headers */
            bstr_append_cstring_nolen(&(res->first_line), "HTTP/1.1 200 OK\r\n");
            bhttp_res_add_header(res, "content-type", mime_from_ext(fs.extension));
            bstr tmp; bstr_init(&tmp); bstr_append_printf(&tmp, "%d", fs.bytes);
            bhttp_res_add_header(res, "content-length", bstr_cstring(&tmp));
            bstr_free_contents(&tmp);
        }
        else
        {
            send_404_response(sock, res);
            return;
        }
        /* send header */
        bstr *header_text = bhttp_res_headers_to_string(res);
        printf("%s", bstr_cstring(header_text));
        send(sock, bstr_cstring(header_text), bstr_size(header_text), 0);
        bstr_free(header_text);
        /* send file contents */
        send_file(sock, bstr_cstring(file_path), fs.bytes, 1);

        bstr_free(file_path);
    }
}

void
match_handler(bhttp_server *server, bhttp_request *req, bhttp_response *res)
{
    response_header rh;
    rh.status.version = "HTTP/1.1";

    switch (req->method) {
        case HTTP_POST:
        case HTTP_GET:
        {
            /* match handlers here */
            for (int i = 0; i < bvec_count(&server->handlers); i++)
            {
                bhttp_req_handler *handler = bvec_get(&server->handlers, i);
                printf("handler: %s\n", bstr_cstring(&handler->uri));
                printf("URI: %s\n", bstr_cstring(&req->uri_path));
                if (strcmp(bstr_cstring(&handler->uri), bstr_cstring(&req->uri_path)) == 0)
                    handler->f(req, res);
            }
//            default_file_handler(req, res);
//            helloworld_text_handler(req, res);
        }
            break;
    }
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
        int rcve = receive_data(&request, conn_fd);

        /* handle request if no error returned */
        if (rcve != BHTTP_REQ_OK)
        {
            free_request(&request);
            break;
        }
        else
        {
            bhttp_response res;
            bhttp_response_init(&res);
            match_handler(server, &request, &res);
//            handle_request(&request, &res);
            write_response(conn_fd, &res);
            bhttp_response_free(&res);
        }
        //write_log(server, &request, s);
        free_request(&request);
        if (request.keep_alive == BHTTP_CLOSE)
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

int
bhttp_server_add_handler(bhttp_server *server, const char * uri, int (*cb)(bhttp_request *req, bhttp_response *res))
{
    bhttp_req_handler *handler = malloc(sizeof(struct bhttp_req_handler));
    bstr_init(&handler->uri);
    bstr_append_cstring_nolen(&handler->uri, uri);
    handler->f = cb;
    bvec_add(&server->handlers, handler);
    return 0;
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
