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
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>

#include "server.h"
#include "respond.h"
#include "http_parser.h"

#define MAX_REGEX_MATCHES 10

/* TODO: handle HEAD requests properly */
/* TODO: add keep-alive header when needed */

static void *
get_in_addr(struct sockaddr *sa)
/* get network address structure */
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static bhttp_handler *
bhttp_handler_new(int bhttp_handler_type, const char * uri, int (*cb)())
{
    bhttp_handler *handler = malloc(sizeof(bhttp_handler));
    if (handler == NULL) return NULL;

    bstr_init(&handler->match);
    if (bstr_append_cstring_nolen(&handler->match, uri) != BS_SUCCESS)
    {
        bstr_free_contents(&handler->match);
        free(handler);
        return NULL;
    }

    handler->type = bhttp_handler_type;
    switch(bhttp_handler_type)
    {
        case BHTTP_HANDLER_SIMPLE:
            handler->f_simple = cb;
            break;
        case BHTTP_HANDLER_REGEX:
            handler->f_regex = cb;
            if (regcomp(&handler->regex_buf, bstr_cstring(&handler->match), REG_EXTENDED) != 0)
            {
                bstr_free_contents(&handler->match);
                free(handler);
                return NULL;
            }
            break;
        default:
            bstr_free_contents(&handler->match);
            free(handler);
            return NULL;
    }
    return handler;
}
static void
bhttp_handler_free(bhttp_handler *h)
{
    bstr_free_contents(&h->match);
    free(h);
}

int
bhttp_add_simple_handler(bhttp_server *server, uint32_t methods, const char *uri,
                         int (*cb)(bhttp_request *, bhttp_response *))
{
    bhttp_handler *h = bhttp_handler_new(BHTTP_HANDLER_SIMPLE, uri, cb);
    if (h == NULL) return 1;
    h->methods = methods;
    bvec_add(&server->handlers, h);
    return 0;
}

int
bhttp_add_regex_handler(bhttp_server *server, uint32_t methods, const char *uri,
                        int (*cb)(bhttp_request *, bhttp_response *, bvec *))
{
    bhttp_handler *h = bhttp_handler_new(BHTTP_HANDLER_REGEX, uri, cb);
    if (h == NULL) return 1;
    h->methods = methods;
    bvec_add(&server->handlers, h);
    return 0;
}

bhttp_server
http_server_new()
{
    bhttp_server server = HTTP_SERVER_DEFAULT;
    bvec_init(&server.handlers, (void (*)(void *)) bhttp_handler_free);
    return server;
}

int
http_server_start(bhttp_server *server)
{
    struct addrinfo hints = {0};
    struct addrinfo *servinfo, *p;

    int yes = 1;

    hints.ai_family = AF_UNSPEC; // ipv4/6 don't care
    //hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    /* get info for us */
    int r;
    if ((r = getaddrinfo(NULL, server->port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "server: getaddrinfo: %s\n", gai_strerror(r));
        goto fail_start;
    }

    /* try to find a socket to bind to */
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

    if (p == NULL || listen(server->sock, server->backlog) == -1)  {
        goto fail_start;
    }

    return 0;

fail_start:
    perror("Failed to start HTTP server");
    return 1;
}

// Needs a lot of work
static file_stats
get_file_stats(const char *file_path)
{
    file_stats fs = {0};
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

/*
 * Modified buffer_path_simplify from lighttpd
 * lighttpd1.4/src/buffer.c
 * Distributed under BSD-3-Clause License
 * Copyright (c) 2004, Jan Kneschke, incremental
 * All rights reserved.
 *
 * - special case: empty string returns empty string
 * - on windows or cygwin: replace \ with /
 * - strip leading spaces
 * - prepends "/" if not present already
 *  My Comment: it doesn't actually do this, maybe it's a bug?
 * - resolve "/../", "//" and "/./" the usual way:
 *   the first one removes a preceding component, the other two
 *   get compressed to "/".
 * - "/." and "/.." at the end are similar, but always leave a trailing
 *   "/"
 *
 * /blah/..         gets  /
 * /blah/../foo     gets  /foo
 * /abc/./xyz       gets  /abc/xyz
 * /abc//xyz        gets  /abc/xyz
 *
 * NOTE: src and dest can point to the same buffer, in which case,
 *       the operation is performed in-place.
 */
int
clean_filepath(bstr *dest, const bstr *path)
/* removes '//' '/./' and '/../' from path and appends to dest */
{
    /* current character, the one before, and the one before that from input */
    char c, pre1, pre2;
    const char *walk;
    char *start, *slash, *out;

    walk  = bstr_cstring(path);
    start = calloc(bstr_size(path)+1, 1);
    if (start == NULL)
        return 1;
    out   = start;
    slash = start;

    /* skip leading spaces */
    while (*walk == ' ') {
        walk++;
    }
    if (*walk == '.') {
        if (walk[1] == '/' || walk[1] == '\0')
            ++walk;
        else if (walk[1] == '.' && (walk[2] == '/' || walk[2] == '\0'))
            walk+=2;
    }

    pre1 = 0;
    c = *(walk++);

    while (c != '\0') {
        /* assert((src != dest || out <= walk) && slash <= out); */
        /* the following comments about out and walk are only interesting if
         * src == dest; otherwise the memory areas don't overlap anyway.
         */
        pre2 = pre1;
        pre1 = c;

        /* possibly: out == walk - need to read first */
        c    = *walk;
        *out = pre1;

        out++;
        walk++;
        /* (out <= walk) still true; also now (slash < out) */

        if (c == '/' || c == '\0') {
            const size_t toklen = out - slash;
            if (toklen == 3 && pre2 == '.' && pre1 == '.' && *slash == '/') {
                /* "/../" or ("/.." at end of string) */
                out = slash;
                /* if there is something before "/..", there is at least one
                 * component, which needs to be removed */
                if (out > start) {
                    out--;
                    while (out > start && *out != '/') out--;
                }

                /* don't kill trailing '/' at end of path */
                if (c == '\0') out++;
                /* slash < out before, so out_new <= slash + 1 <= out_before <= walk */
            } else if (toklen == 1 || (pre2 == '/' && pre1 == '.')) {
                /* "//" or "/./" or (("/" or "/.") at end of string) */
                out = slash;
                /* don't kill trailing '/' at end of path */
                if (c == '\0') out++;
                /* slash < out before, so out_new <= slash + 1 <= out_before <= walk */
            }

            slash = out;
        }
    }
    if (bstr_append_cstring(dest, start, out - start) != BS_SUCCESS)
    {
        free(start);
        return 1;
    }
    free(start);
    return 0;
}

int
send_headers(int sock, bhttp_response *res)
{
    bstr *header_text = bhttp_res_headers_to_string(res);
    //printf("%s", bstr_cstring(header_text));
    ssize_t sent = send(sock, bstr_cstring(header_text), bstr_size(header_text), 0);
    if (sent < bstr_size(header_text))
        return 1;
    bstr_free(header_text);
    return 0;
}

static void
send_500_response(int sock, bhttp_response *res)
{
    res->response_code = BHTTP_500;
    /* send header */
    send_headers(sock, res);
}

static void
send_404_response(int sock, bhttp_response *res)
{
    /* add our own headers and set 404 message */
    res->response_code = BHTTP_404;
    bhttp_res_set_body_text(res, "<html><p>bittyhttp: 404 - NOT FOUND</p><html>");
    bhttp_res_add_header(res, "content-type", "text/html");
    bstr tmp; bstr_init(&tmp); bstr_append_printf(&tmp, "%d", bstr_size(&res->body));
    bhttp_res_add_header(res, "content-length", bstr_cstring(&tmp));
    bstr_free_contents(&tmp);

    /* send header */
    send_headers(sock, res);
    /* send body */
    send(sock, bstr_cstring(&res->body), bstr_size(&res->body), 0);
}

static void
write_response(bhttp_server *server, bhttp_response *res, bhttp_request *req, int sock)
{
    bhttp_res_add_header(res, "server", "bittyhttp");
    if (req->keep_alive == BHTTP_KEEP_ALIVE)
        bhttp_res_add_header(res, "connection", "keep-alive");

    if (res->bodytype == BHTTP_RES_BODY_TEXT)
    {
        /* check 'content-type', add default if missing */
        bhttp_header *h = bhttp_res_get_header(res, "content-type");
        if (h == NULL)
            bhttp_res_add_header(res, "content-type", "text/plain");
        /* add 'content-length' based on text in body */
        bstr tmp; bstr_init(&tmp); bstr_append_printf(&tmp, "%d", bstr_size(&res->body));
        bhttp_res_add_header(res, "content-length", bstr_cstring(&tmp));
        bstr_free_contents(&tmp);
        /* send full HTTP response header */
        send_headers(sock, res);
        /* send body */
        send(sock, bstr_cstring(&res->body), bstr_size(&res->body), 0);
    }
    else if (res->bodytype == BHTTP_RES_BODY_FILE_REL ||
             res->bodytype == BHTTP_RES_BODY_FILE_ABS)
    {
        bstr *file_path = bstr_new();
        if (res->bodytype == BHTTP_RES_BODY_FILE_REL)
        {
            bstr_append_cstring_nolen(file_path, server->docroot);
            clean_filepath(file_path, &res->body);
        }
        else
        {
            bstr_append_cstring_nolen(file_path, bstr_cstring(&res->body));
        }

        file_stats fs = get_file_stats(bstr_cstring(file_path));
        /* found directory, append default file and try again */
        if (fs.found && fs.isdir) {
            bstr_append_char(file_path, '/');
            bstr_append_cstring_nolen(file_path, server->default_file);
            fs = get_file_stats(bstr_cstring(file_path));
        }
        /* found file */
        if (fs.found && !fs.isdir)
        {
            /* add our own headers */
            bhttp_res_add_header(res, "content-type", mime_from_ext(fs.extension));
            bstr tmp; bstr_init(&tmp); bstr_append_printf(&tmp, "%d", fs.bytes);
            bhttp_res_add_header(res, "content-length", bstr_cstring(&tmp));
            bstr_free_contents(&tmp);

            /* send header */
            send_headers(sock, res);
            /* send file contents */
            send_file(sock, bstr_cstring(file_path), fs.bytes, 1);
        }
        else
        {
            send_404_response(sock, res);
        }
        bstr_free(file_path);
    }
}

bvec *
regex_match_handler(bhttp_handler *handler, bstr *uri_path)
{
    regmatch_t matches[MAX_REGEX_MATCHES];
    regex_t *preg = &handler->regex_buf;

    int r = regexec(preg, bstr_cstring(uri_path), MAX_REGEX_MATCHES, matches, 0);
    /* no matches or error in regexec */
    if (r != 0) return NULL;

    bvec *matched_parts = malloc(sizeof(bvec));
    if (matched_parts == NULL) return NULL;

    bvec_init(matched_parts, (void (*)(void *)) bstr_free);
    for (int i = 0; i < MAX_REGEX_MATCHES; i++)
    {
        if (matches[i].rm_so == -1) break;
        bstr *part = bstr_new();
        bstr_append_cstring(part,
                            bstr_cstring(uri_path)+matches[i].rm_so,
                            matches[i].rm_eo-matches[i].rm_so);
        //printf("match %d: %s\n", i, bstr_cstring(part));
        bvec_add(matched_parts, part);
    }
    return matched_parts;
}

static int
match_handler(bhttp_server *server, bhttp_request *req, bhttp_response *res)
{
    /* match handlers here */
    for (int i = 0; i < bvec_count(&server->handlers); i++)
    {
        bhttp_handler *handler = bvec_get(&server->handlers, i);
        if (!(handler->methods & req->method)) continue;
        //printf("handler: %s\n", bstr_cstring(&handler->match));
        //printf("URI: %s\n", bstr_cstring(&req->uri_path));

        bvec *args;
        int r;
        switch(handler->type)
        {
            case BHTTP_HANDLER_SIMPLE:
                if (strcmp(bstr_cstring(&handler->match), bstr_cstring(&req->uri_path)) == 0)
                    return handler->f_simple(req, res);
                break;
            case BHTTP_HANDLER_REGEX:
                args = regex_match_handler(handler, &req->uri_path);
                if (args != NULL)
                {
                    r = handler->f_regex(req, res, args);
                    bvec_free(args);
                    return r;
                }
                break;
        }
    }
    if (req->method & BHTTP_GET || req->method & BHTTP_HEAD)
        default_file_handler(req, res);
    else
        /* TODO: create method not supported handler */
        /* TODO: recheck flow to make sure requests are handled properly */
        return 0;
    return 0;
}

static void *
do_connection(void * arg)
{
    bhttp_server *server = ((thread_args *)arg)->server;
    int conn_fd = ((thread_args *)arg)->sock;

    bhttp_request request;
    /* handle request while keep-alive requested */
    while (1)
    {
        /* read a new request */
        bhttp_request_init(&request);
        int r = receive_data(&request, conn_fd);
        /* error handling request, break, don't bother being nice */
        if (r != BHTTP_REQ_OK)
        {
            bhttp_request_free(&request);
            break;
        }
        /* handle request if no error returned */
        else
        {
            bhttp_response res;
            bhttp_response_init(&res);
            if (match_handler(server, &request, &res) == 0)
                write_response(server, &res, &request, conn_fd);
            bhttp_response_free(&res);
        }
        //write_log(server, &request, s);
        bhttp_request_free(&request);
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
        pthread_attr_init(&args->attr);
        pthread_attr_setdetachstate(&args->attr, PTHREAD_CREATE_DETACHED);
        int ret = pthread_create(&args->thread, &args->attr, do_connection, (void *)args);
        if (ret != 0)
        {
            printf("THREAD ERROR\n");
            return;
        }
//        pthread_join(args->thread, NULL);
//        return;
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

    // Log methods
    fwrite(http_method_str(request->method), 1, strlen(http_method_str(request->method)), f);
    fwrite(" ", 1, 1, f);

    // Log client ip
    fwrite(client_ip, 1, strlen(client_ip), f);
    fwrite(" ", 1, 1, f);

    // Log URI
    if (strcmp(http_method_str(request->method), "<unknown>") != 0)
    {
        fwrite(bstr_cstring(&(request->uri)), 1, bstr_size(&(request->uri)), f);
    }
    fwrite(" ", 1, 1, f);

    // Log GMT timestamp
    fwrite(buffer, 1, strlen(buffer), f);
    fwrite("\n", 1, 1, f);

    fclose(f);
}
