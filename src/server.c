/*
 *  server.c
 *  bittyhttp
 *
 *  Created by Colin Luoma on 2021-01-30.
 *  Copyright (c) 2021 Colin Luoma. All rights reserved.
 */

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>

#include "server.h"
#include "respond.h"
#include "http_parser.h"
#ifdef LUA
#include "lua_interface.h"
#endif

#define MAX_REGEX_MATCHES 10

#define WRITE_LOCK(X)   pthread_rwlock_wrlock(&((X)->rwlock))
#define READ_LOCK(X)    pthread_rwlock_rdlock(&((X)->rwlock))
#define UNLOCK(X)       pthread_rwlock_unlock(&((X)->rwlock))

typedef struct file_stats {
    int found;
    int isdir;
    long long bytes;
    char *extension;
} file_stats;

typedef struct
{
    pthread_t thread;
    pthread_attr_t attr;
    bhttp_server *server;
    char ipstr[INET6_ADDRSTRLEN];
    int sock;
} thread_args;

typedef enum {
    BH_HANDLER_OK = 0,
    BH_HANDLER_NZ,          // handler returned non-zero
    BH_HANDLER_NO_MATCH     // could not find a matching handler
} bhttp_handler_err_code;

typedef enum {
    BHTTP_HANDLER_SIMPLE = 0,
    BHTTP_HANDLER_REGEX,
    BHTTP_HANDLER_LUA
} bhttp_handler_type;

union handler_callback {
    int (*f_simple)(bhttp_request *req, bhttp_response *res);
    int (*f_regex)(bhttp_request *req, bhttp_response *res, bvec *args);
    int (*f_lua)(bhttp_request *req, bhttp_response *res,
                 bstr *lua_file, bstr *lua_cb);
};
typedef struct bhttp_handler
{
    bhttp_handler_type type;
    /* accepted http methods */
    uint32_t methods;
    /* string used to match requests */
    bstr match;
    union handler_callback cb;
    /* for regex */
    regex_t regex_buf;
    /* for lua */
    bstr *lua_file;
    bstr *lua_cb_func;
} bhttp_handler;

#define C(k, v) [k] = (v),
static const char * bhttp_res_codes_string[] = { BHTTP_RES_CODES };
#undef C

/* TODO: handle HEAD requests properly */

static bhttp_handler *
bhttp_handler_new(int bhttp_handler_type, const char * uri, int (*cb)())
{
    bhttp_handler *handler = malloc(sizeof(bhttp_handler));
    if (handler == NULL) return NULL;
    handler->lua_file = NULL;
    handler->lua_cb_func = NULL;

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
            handler->cb.f_simple = cb;
            break;
        case BHTTP_HANDLER_REGEX:
            handler->cb.f_regex = cb;
            if (regcomp(&handler->regex_buf, bstr_cstring(&handler->match), REG_EXTENDED) != 0)
            {
                bstr_free_contents(&handler->match);
                free(handler);
                return NULL;
            }
            break;
        case BHTTP_HANDLER_LUA:
            handler->cb.f_lua = cb;
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
    if (h->type == BHTTP_HANDLER_REGEX)
        regfree(&h->regex_buf);
    if (h->lua_file != NULL) bstr_free(h->lua_file);
    if (h->lua_cb_func != NULL) bstr_free(h->lua_cb_func);
    free(h);
}

int
bhttp_add_simple_handler(bhttp_server *server, uint32_t methods, const char *uri,
                         int (*cb)(bhttp_request *, bhttp_response *))
{
    bhttp_handler *h = bhttp_handler_new(BHTTP_HANDLER_SIMPLE, uri, cb);
    if (h == NULL) return 1;
    h->methods = methods;
    /* add handler to server */
    WRITE_LOCK(server);
    bvec_add(&server->handlers, h);
    UNLOCK(server);
    return 0;
}

int
bhttp_add_regex_handler(bhttp_server *server, uint32_t methods, const char *uri,
                        int (*cb)(bhttp_request *, bhttp_response *, bvec *))
{
    bhttp_handler *h = bhttp_handler_new(BHTTP_HANDLER_REGEX, uri, cb);
    if (h == NULL) return 1;
    h->methods = methods;
    /* add handler to server */
    WRITE_LOCK(server);
    bvec_add(&server->handlers, h);
    UNLOCK(server);
    return 0;
}

#ifdef LUA
int
bhttp_add_lua_handler(bhttp_server *server, uint32_t methods, const char *uri,
                      const char * lua_script_path, const char * lua_cb_func_name)
{
    bhttp_handler *h = bhttp_handler_new(BHTTP_HANDLER_LUA, uri, bhttp_lua_handler_callback);
    if (h == NULL) return 1;
    h->methods = methods;
    /* add name of lua script and lua callback */
    if ( (h->lua_file = bstr_new_from_cstring(lua_script_path, strlen(lua_script_path))) == NULL ||
         (h->lua_cb_func = bstr_new_from_cstring(lua_cb_func_name, strlen(lua_cb_func_name))) == NULL )
    {
        bhttp_handler_free(h);
        return 1;
    }
    /* add handler to server vector of handlers */
    WRITE_LOCK(server);
    bvec_add(&server->handlers, h);
    UNLOCK(server);
    return 0;
}
#endif

bhttp_server *
bhttp_server_new()
{
    bhttp_server *server = malloc(sizeof(bhttp_server));
    server->ip = NULL;
    server->port = strdup("3490");
    server->docroot = strdup("./www");
    server->log_file = strdup("./bittblog.log");
    server->default_file = strdup("index.html");
    server->backlog = 10;
    server->use_sendfile = 1;
    server->sock = 0;
    bvec_init(&server->handlers, (void (*)(void *)) bhttp_handler_free);

    if (server->port == NULL || server->docroot == NULL ||
        server->log_file == NULL || server->default_file == NULL)
    {
        bhttp_server_free(server);
        return NULL;
    }

    if (pthread_rwlock_init(&server->rwlock, NULL) != 0)
    {
        bhttp_server_free(server);
        return NULL;
    }
    server->state = BHTTP_SERVER_STATE_OFF;

    return server;
}

void
bhttp_server_free(bhttp_server *server)
{
    if (server->ip != NULL) free(server->ip);
    if (server->port != NULL) free(server->port);
    if (server->docroot != NULL) free(server->docroot);
    if (server->log_file != NULL) free(server->log_file);
    if (server->default_file != NULL) free(server->default_file);
    bvec_free_contents(&server->handlers);
    pthread_rwlock_destroy(&server->rwlock);
    free(server);
}

int
bhttp_server_set_ip(bhttp_server *server, const char *ip)
{
    /* return value, default 0=success, 1=failure */
    int r = 0;

    READ_LOCK(server);
    if (server->state != BHTTP_SERVER_STATE_OFF)
    {
        fprintf(stderr, "bhttp: Cannot set IP address in current state\n");
        r = 1;
        goto exit;
    }
    UNLOCK(server);

    WRITE_LOCK(server);
    if (server->ip != NULL)
        free(server->ip);
    if (ip == NULL)
    {
        server->ip = NULL;
        goto exit;
    }
    server->ip = strdup(ip);
    if (server->ip == NULL)
    {
        r = 1;
        goto exit;
    }

exit:
    UNLOCK(server);
    return r;
}

static int
arg_replace(char **dest, const char *src)
{
    if (src == NULL)
        return 1;

    char *new = strdup(src);
    if (new == NULL)
        return 1;

    free(*dest);
    *dest = new;
    return 0;
}

int
bhttp_server_set_port(bhttp_server *server, const char *port)
{
    /* return value, default 0=success, 1=failure */
    int r = 0;

    READ_LOCK(server);
    if (server->state != BHTTP_SERVER_STATE_OFF)
    {
        fprintf(stderr, "bhttp: Cannot set port in current state\n");
        r = 1;
        goto exit;
    }
    UNLOCK(server);

    WRITE_LOCK(server);
    r = arg_replace(&server->port, port);

exit:
    UNLOCK(server);
    return r;
}

int
bhttp_server_set_docroot(bhttp_server *server, const char *docroot)
{
    /* return value, default 0=success, 1=failure */
    int r = 0;

    READ_LOCK(server);
    if (server->state != BHTTP_SERVER_STATE_OFF)
    {
        fprintf(stderr, "bhttp: Cannot set docroot in current state\n");
        r = 1;
        goto exit;
    }
    UNLOCK(server);

    WRITE_LOCK(server);
    r = arg_replace(&server->docroot, docroot);
exit:
    UNLOCK(server);
    return r;
}

int
bhttp_server_set_dfile(bhttp_server *server, const char *dfile)
/* sets the default file to look for in a directory */
{
    /* return value, default 0=success, 1=failure */
    int r = 0;

    READ_LOCK(server);
    if (server->state != BHTTP_SERVER_STATE_OFF)
    {
        fprintf(stderr, "bhttp: Cannot set default file in current state\n");
        r = 1;
        goto exit;
    }
    UNLOCK(server);

    WRITE_LOCK(server);
    r = arg_replace(&server->default_file, dfile);
exit:
    UNLOCK(server);
    return r;
}

int
bhttp_server_bind(bhttp_server *server)
{
    struct addrinfo hints = {0};
    struct addrinfo *servinfo, *p;

    /* ipv4 or 6 */
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    /* use self ip */
    hints.ai_flags = AI_PASSIVE;

    /* get info */
    int r;
    if ((r = getaddrinfo(server->ip, server->port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "error getting addrinfo: %s\n", gai_strerror(r));
        return 1;
    }

    /* try to find a socket to bind to */
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((server->sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("error creating socket");
            continue;
        }
        int turnon = 1;
        if (setsockopt(server->sock, SOL_SOCKET, SO_REUSEADDR, &turnon, sizeof(int)) == -1) {
            close(server->sock);
            freeaddrinfo(servinfo);
            return 1;
        }
        if (bind(server->sock, p->ai_addr, p->ai_addrlen) == -1) {
            close(server->sock);
            perror("error binding socket");
            continue;
        }
        break;
    }
    freeaddrinfo(servinfo);

    if (p == NULL || listen(server->sock, server->backlog) == -1)  {
        perror("failed to start server");
        return 1;
    }

    return 0;
}

static file_stats
get_file_stats(const char *file_path)
/* returns basic information on the file at file_path */
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
    /* store return codes */
    int r;
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
    r = bstr_append_cstring(dest, start, out - start);
    free(start);
    if (r != BS_SUCCESS)
        return 1;
    else
        return 0;
}

static int
send_buffer(int sock, const char *buf, size_t len)
{
    /* send buffer data */
    ssize_t sent = send(sock, buf, len, MSG_NOSIGNAL);
    if ( sent < 0 ) return 1;
    if (sent < len) return 1;
    return 0;
}

static int
send_headers(int sock, bhttp_response *res)
/* includes the final /r/n after the header block */
{
    int r;
    bstr *header_text = bstr_new();
    if (header_text == NULL) return 1;

    const bvec *headers = bhttp_res_get_all_headers(res);
    bstr_append_printf(header_text, "HTTP/1.1 %s\r\n", bhttp_res_codes_string[res->response_code]);
    for (int i = 0; i < bvec_count(headers); i++)
    {
        bhttp_header *h = bvec_get(headers, i);
        bstr_append_cstring(header_text, bstr_cstring(&h->field), bstr_size(&h->field));
        bstr_append_cstring(header_text, bstr_const_str(": "));
        bstr_append_cstring(header_text, bstr_cstring(&h->value), bstr_size(&h->value));
        bstr_append_cstring(header_text, bstr_const_str("\r\n"));
    }

    /* add cookies */
    const bvec *cookies = bhttp_res_get_cookies(res);
    for (int i = 0; i < bvec_count(cookies); i++)
    {
        bstr_append_cstring(header_text, bstr_const_str("set-cookie: "));
        bhttp_cookie_entry *ce = bvec_get(cookies, i);
        bstr_append_cstring(header_text, bstr_cstring(&ce->field), bstr_size(&ce->field));
        bstr_append_cstring(header_text, bstr_const_str("="));
        bstr_append_cstring(header_text, bstr_cstring(&ce->value), bstr_size(&ce->value));
        /* TODO: add support here for max-age, etc. */
        bstr_append_cstring(header_text, bstr_const_str("\r\n"));
    }

    bstr_append_cstring(header_text, bstr_const_str("\r\n"));

    r = send_buffer(sock, bstr_cstring(header_text), (size_t)bstr_size(header_text));
    bstr_free(header_text);
    if (r != 0)
        return 1;
    return 0;
}

static int
send_file(int sock, const char *file_path, size_t file_size, int use_sendfile)
/* makes sure the send an entire file to sock */
{
    ssize_t sent = 0;
    if (use_sendfile)
    {
        int f = open(file_path, O_RDONLY);
        if ( f <= 0 )
        {
            fprintf(stderr, "Cannot open file %d\n", errno);
            return 1;
        }
        off_t len = 0;
        ssize_t ret;
        while ((ret = sendfile(sock, f, &len, file_size-sent)) > 0)
        {
            sent += ret;
            if (sent >= (ssize_t)file_size) break;
        }
        close(f);
        if (ret == -1)
        {
            perror("sendfile error");
            return 1;
        }
    }
    else
    {
        FILE *f = fopen(file_path, "rb");
        if ( f == NULL )
        {
            fprintf(stderr, "Cannot open file %d\n", errno);
            return 1;
        }
        size_t len;
        char buf[SEND_BUFFER_SIZE];
        int bad = 0;
        while ((len = fread(buf, 1, SEND_BUFFER_SIZE, f)) > 0)
        {
            if (len < SEND_BUFFER_SIZE)
            {
                /* check error or end-of-file */
                if (ferror(f))
                {
                    perror("fread error");
                    bad = 1;
                    break;
                }
                else if (feof(f)) {}
            }
            sent = 0;
            ssize_t ret;
            while ((ret = send(sock, buf+sent, len-sent, MSG_NOSIGNAL)) > 0)
            {
                sent += ret;
                if (sent >= (ssize_t)file_size) break;
            }
            if (ret == -1)
            {
                perror("send error");
                bad = 1;
                break;
            }
        }
        fclose(f);
        if (bad) return 1;
    }
    return 0;
}

static void
send_500_response(int sock, bhttp_response *res)
{
    res->response_code = BHTTP_500;
    /* send header */
    bhttp_res_add_header(res, "server", "bittyhttp");
    bhttp_res_add_header(res, "content-length", "0");
    send_headers(sock, res);
}
static void
send_501_response(int sock, bhttp_response *res)
{
    res->response_code = BHTTP_501;
    /* send header */
    bhttp_res_add_header(res, "server", "bittyhttp");
    bhttp_res_add_header(res, "content-length", "0");
    send_headers(sock, res);
}

static void
send_404_response(int sock, bhttp_response *res)
{
    /* add our own headers and set 404 message */
    res->response_code = BHTTP_404;
    bhttp_res_set_body_text(res, "<html><p>bittyhttp: 404 - NOT FOUND</p></html>");
    bhttp_res_add_header(res, "content-type", "text/html");
    bstr tmp; bstr_init(&tmp); bstr_append_printf(&tmp, "%d", bstr_size(&res->body));
    bhttp_res_add_header(res, "content-length", bstr_cstring(&tmp));
    bstr_free_contents(&tmp);

    /* send header */
    send_headers(sock, res);
    /* send body */
    send_buffer(sock, bstr_cstring(&res->body), (size_t)bstr_size(&res->body));
}

static void
write_response(bhttp_server *server, bhttp_response *res, bhttp_request *req, int sock)
{
    bhttp_res_add_header(res, "server", "bittyhttp");
    if (req->keep_alive == BHTTP_KEEP_ALIVE)
        bhttp_res_add_header(res, "connection", "keep-alive");

    if (res->bodytype == BHTTP_RES_BODY_EMPTY)
    {
        /* send full HTTP response header */
        bhttp_res_add_header(res, "content-length", "0");
        send_headers(sock, res);
    }
    else if (res->bodytype == BHTTP_RES_BODY_TEXT)
    {
        /* check 'content-type', add default if missing */
        const bhttp_header *h = bhttp_res_get_header(res, "content-type");
        if (h == NULL)
            bhttp_res_add_header(res, "content-type", "text/plain");
        /* add 'content-length' based on text in body */
        bstr tmp; bstr_init(&tmp); bstr_append_printf(&tmp, "%d", bstr_size(&res->body));
        bhttp_res_add_header(res, "content-length", bstr_cstring(&tmp));
        bstr_free_contents(&tmp);
        /* send full HTTP response header */
        send_headers(sock, res);
        /* send body */
        send_buffer(sock, bstr_cstring(&res->body), (size_t)bstr_size(&res->body));
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
            bstr_append_cstring(file_path, bstr_cstring(&res->body), bstr_size(&res->body));
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
            send_file(sock, bstr_cstring(file_path), fs.bytes, server->use_sendfile);
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
    if (r) return NULL;

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
    /* return value from handlers */
    int r;
    /* TODO: should actually try and return 405 for matched paths */
    /* match handlers here */
    READ_LOCK(server);
    for (int i = 0; i < bvec_count(&server->handlers); i++)
    {
        bhttp_handler *handler = bvec_get(&server->handlers, i);
        if (!(handler->methods & req->method)) continue;
        //printf("handler: %s\n", bstr_cstring(&handler->match));
        //printf("URI: %s\n", bstr_cstring(&req->uri_path));

        bvec *args; /* in case we need to store matches from a regex handler */
        switch(handler->type)
        {
            case BHTTP_HANDLER_SIMPLE:
                if (strcmp(bstr_cstring(&handler->match), bstr_cstring(&req->uri_path)) == 0)
                {
                    r = handler->cb.f_simple(req, res) ?
                            BH_HANDLER_NZ : BH_HANDLER_OK;
                    goto exit;
                }
                break;
            case BHTTP_HANDLER_REGEX:
                args = regex_match_handler(handler, &req->uri_path);
                if (args != NULL)
                {
                    r = handler->cb.f_regex(req, res, args);
                    bvec_free(args);
                    r = r ? BH_HANDLER_NZ : BH_HANDLER_OK;
                    goto exit;
                }
                break;
            case BHTTP_HANDLER_LUA:
                if (strcmp(bstr_cstring(&handler->match), bstr_cstring(&req->uri_path)) == 0)
                {
                    r = handler->cb.f_lua(req, res, handler->lua_file, handler->lua_cb_func) ?
                            BH_HANDLER_NZ : BH_HANDLER_OK;
                    goto exit;
                }
                break;
        }
    }
    /* no handler found, try serving a file */
    if (req->method & BHTTP_GET || req->method & BHTTP_HEAD)
    {
        r = default_file_handler(req, res) ?
                BH_HANDLER_NZ : BH_HANDLER_OK;
        goto exit;
    }
    else
        r = BH_HANDLER_NO_MATCH;
exit:
    UNLOCK(server);
    return r;
}

static void *
do_connection(void * arg)
{
    bhttp_server *server = ((thread_args *)arg)->server;
    int sock = ((thread_args *)arg)->sock;
    const char *ipstr = ((thread_args *)arg)->ipstr;

    bhttp_request req;
    /* handle req while keep-alive requested */
    do
    {
        /* read a new req */
        bhttp_request_init(&req);
        req.ip = ipstr;
        int r = receive_data(&req, sock);
        /* error handling req, break, don't bother being nice */
        if (r != BHTTP_REQ_OK)
        {
            bhttp_request_free(&req);
            break;
        }
        /* handle req if no error returned */
        else
        {
            bhttp_response res;
            bhttp_response_init(&res);
            /* check http method */
            if (req.method != BHTTP_UNSUPPORTED_METHOD)
            {
                int hr = match_handler(server, &req, &res);
                if (hr == BH_HANDLER_OK)
                {
                    write_response(server, &res, &req, sock);
                }
                else if (hr == BH_HANDLER_NZ)
                {
                    send_500_response(sock, &res);
                }
                else if (hr == BH_HANDLER_NO_MATCH)
                {
                    send_404_response(sock, &res);
                }
            }
            /* unsupported http method */
            else
            {
                send_501_response(sock, &res);
            }
            bhttp_response_free(&res);
        }
        //write_log(server, &req, s);
        bhttp_request_free(&req);
    } while (req.keep_alive == BHTTP_KEEP_ALIVE);
    /* cleanup */
    close(sock);
    free(arg);
    return NULL;
}

int
fill_ip(struct sockaddr_storage *addr, char *dest, size_t size)
/* writes in ip from addr into dest string, size is size of dest buffer */
{
    void * inaddr;
    if (addr->ss_family == AF_INET) {
        inaddr = &(((struct sockaddr_in*)addr)->sin_addr);
    }
    else if (addr->ss_family == AF_INET6)
    {
        inaddr = &(((struct sockaddr_in6*)addr)->sin6_addr);
    }
    else
    {
        return 1;
    }
    const char * r = inet_ntop(addr->ss_family, inaddr, dest, size);
    if (r == NULL) return 1;
    return 0;
}

void
bhttp_server_run(bhttp_server *server)
/* start accepting connections */
{
    pthread_attr_t detached_attr;
    pthread_attr_init(&detached_attr);
    pthread_attr_setdetachstate(&detached_attr, PTHREAD_CREATE_DETACHED);

    /* file descriptor for accepted connections */
    int con;
    /* client address information */
    struct sockaddr_storage their_addr;
    socklen_t sin_size;

    /* wait for connections forever */
    while(1) {
        sin_size = sizeof their_addr;
        //printf("Waiting on connection...\n");
        con = accept(server->sock, (struct sockaddr *)&their_addr, &sin_size);
        if (con == -1) {
            perror("accept");
            continue;
        }

        /* start new thread to handle connection */
        thread_args *args = malloc(sizeof(thread_args));
        if (args != NULL)
        {
            args->server = server;
            args->sock = con;
            if (fill_ip(&their_addr, args->ipstr, sizeof args->ipstr) != 0)
            {
                fprintf(stderr, "Could not get IP address of client\n");
                free(args);
                /* will stop the server */
                return;
            }

            int ret = pthread_create(&args->thread, &detached_attr, do_connection, (void *)args);
            if (ret != 0)
            {
                fprintf(stderr, "Could not create thread to handle connection\n");
                if (ret == EPERM) fprintf(stderr, "attr");
                free(args);
                /* will stop the server */
                return;
            }
        }
        else
        {
            fprintf(stderr, "Could not allocate data for connection\n");
            /* will stop the server */
            return;
        }
    }
}

int
bhttp_server_start(bhttp_server *server, int own_thread)
{
    if (bhttp_server_bind(server))
    /* first try to bind to ip and port */
    {
        fprintf(stderr, "Unable to bind ip: %s on port: %s\n", server->ip, server->port);
        return 1;
    }

    if (!own_thread)
    /* start bittyhttp and never return unless there's an error */
    {
        bhttp_server_run(server);
        return 1;
    }

    /* start bittyhttp on its own thread */
    if (WRITE_LOCK(server))
    {
        fprintf(stderr, "could not get mutex rwlock on server\n");
        return 1;
    }
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    int ret = pthread_create(&server->thread_id, &attr, (void *(*)(void *)) bhttp_server_run, server);
    pthread_attr_destroy(&attr);
    if (ret)
    {
        fprintf(stderr, "unable to start bittyhttp server on a own_thread thread\n");
        return 1;
    }
    server->state = BHTTP_SERVER_STATE_RUNNING;
    UNLOCK(server);
    return 0;
}

int
bhttp_server_stop(bhttp_server *server)
{
    if (WRITE_LOCK(server))
    {
        fprintf(stderr, "could not get mutex rwlock on server\n");
        return 1;
    }
    if (pthread_cancel(server->thread_id))
    {
        fprintf(stderr, "error cancelling bittyhttp server thread\n");
        return 1;
    }
    pthread_join(server->thread_id, NULL);
    server->state = BHTTP_SERVER_STATE_OFF;
    UNLOCK(server);
    return 0;
}
