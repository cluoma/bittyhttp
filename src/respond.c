//
//  respond.c
//  bittyhttp
//
//  Created by Colin Luoma on 2016-07-03.
//  Copyright (c) 2016 Colin Luoma. All rights reserved.
//

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef __linux__
#include <sys/sendfile.h>
#endif
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "request.h"
#include "respond.h"
#include "header.h"
#include "http_parser.h"
#include "mime_types.h"

void
bhttp_response_init(bhttp_response *res)
{
    bstr_init(&res->first_line);
    bvec_init(&res->headers, (void (*)(void *)) http_header_free);
    bstr_init(&res->body);
    res->bodytype = BHTTP_RES_BODY_EMPTY;
}

void
bhttp_response_free(bhttp_response *res)
{
    bstr_free_contents(&res->first_line);
    bvec_free_contents(&res->headers);
    bstr_free_contents(&res->body);
    res->bodytype = BHTTP_RES_BODY_EMPTY;
}

int
bhttp_res_add_header(bhttp_response *res, const char *field, const char *value)
{
    /* TODO: error handling */
    bhttp_header *h = http_header_new();
    bstr_append_cstring_nolen(&(h->field), field);
    bstr_append_cstring_nolen(&(h->value), value);
    bvec_add(&(res->headers), h);
    return 0;
}

static int
bhttp_res_set_body(bhttp_response *res, const char *s, uint64_t len)
{
    bstr_free_contents(&res->body);
    bstr_init(&res->body);
    if (bstr_append_cstring(&res->body, s, len) == BS_SUCCESS)
        return 0;
    else
        return 1;
}

int
bhttp_res_set_body_text(bhttp_response *res, const char *s)
{
    res->bodytype = BHTTP_RES_BODY_TEXT;
    return bhttp_res_set_body(res, s, (uint64_t)strlen(s));
}

int
bhttp_res_set_body_file(bhttp_response *res, const char *s, int isabs)
{
    res->bodytype = isabs ? BHTTP_RES_BODY_FILE_ABS : BHTTP_RES_BODY_FILE_REL;
    return bhttp_res_set_body(res, s, (uint64_t)strlen(s));
}
int
bhttp_res_set_body_file_rel(bhttp_response *res, const char *s)
{
    return bhttp_res_set_body_file(res, s, 0);
}
int
bhttp_res_set_body_file_abs(bhttp_response *res, const char *s)
{
    return bhttp_res_set_body_file(res, s, 1);
}

bstr *
bhttp_res_headers_to_string(bhttp_response *res)
{
    /* TODO: error handling */
    bstr *header_text = bstr_new();
    bstr_append_cstring(header_text, bstr_cstring(&res->first_line), bstr_size(&res->first_line));
    for (int i = 0; i < bvec_count(&res->headers); i++)
    {
        bhttp_header *h = bvec_get(&res->headers, i);
        bstr_append_printf(header_text, "%s: %s\r\n", bstr_cstring(&h->field), bstr_cstring(&h->value));
    }
    bstr_append_cstring(header_text, "\r\n", 2);
    return header_text;
}

//void
//send_header(int sock, bhttp_request *request, response_header *rh, file_stats *fs)
//{
//    /* TODO:
//     * make this cleaner
//     */
//    bstr headers;
//    bstr_init(&headers);
//
//    bstr_append_printf(&headers, "%s %s %s\r\nServer: bittyhttp\r\n", rh->status.version, rh->status.status_code, rh->status.status);
//
//    // Keep Alive
//    if (request->keep_alive == BHTTP_KEEP_ALIVE)
//    {
//        bstr_append_printf(&headers, "Connection: Keep-Alive\r\nKeep-Alive: timeout=5\r\n");
//    }
//    else
//    {
//        bstr_append_printf(&headers, "Connection: Close\r\n");
//    }
//    // File content
//    bstr_append_printf(&headers, "Content-Type: %s\r\nContent-Length: %lld\r\n\r\n", mime_from_ext(fs->extension), (long long int)fs->bytes);
//    send(sock, bstr_cstring(&headers), bstr_size(&headers), 0);
//    bstr_free_contents(&headers);
//}

// Needs a lot of work
void
send_file(int sock, char *file_path, int file_size, int use_sendfile)
{
    if (use_sendfile)
    {
        int f = open(file_path, O_RDONLY);
        if ( f <= 0 )
        {
            printf("Cannot open file %d\n", errno);
            return;
        }

        off_t len = 0;
        #ifdef __APPLE__
        if ( sendfile(f, sock, 0, &len, NULL, 0) < 0 )
        {
            printf("Mac: Sendfile error: %d\n", errno);
        }
        #elif __linux__
        size_t sent = 0;
        ssize_t ret;
        while ( (ret = sendfile(sock, f, &len, file_size - sent)) > 0 )
        {
            sent += ret;
            if (sent >= file_size) break;
        }
        #endif
        close(f);
    }
    else
    {
        FILE *f = fopen(file_path, "rb");
        if ( f == NULL )
        {
            printf("Cannot open file %d\n", errno);
            return;
        }

        size_t len = 0;
        char *buf = malloc(TRANSFER_BUFFER);
        while ( (len = fread(buf, 1, TRANSFER_BUFFER, f)) > 0 )
        {
            ssize_t sent = 0;
            ssize_t ret  = 0;
            while ( (ret = send(sock, buf+sent, len-sent, 0)) > 0 )
            {
                sent += ret;
                if (sent >= file_size) break;
            }
            if (ret < 0)
            {
                printf("ERROR!!!\n");
                break;
            }

            // Check for being done, either fread error or eof
            if (feof(f) || ferror(f)) {break;}
        }
        free(buf);
        fclose(f);
    }
}

// Needs a lot of work
static void
build_header(bhttp_response *res, file_stats *fs)
{
    bstr_append_cstring_nolen(&(res->first_line), "HTTP/1.1 404 Not Found");
    bhttp_header *h;
    h = http_header_new();
    bstr_append_cstring_nolen(&(h->field), "content-type");
    bstr_append_cstring_nolen(&(h->value), "text/html");
    bvec_add(&(res->headers), h);
}

int
default_404_handler(bhttp_request *req, bhttp_response *res)
{
    bstr_append_cstring_nolen(&(res->first_line), "HTTP/1.1 404 Not Found");
    bhttp_header *h;
    h = http_header_new();
    bstr_append_cstring_nolen(&(h->field), "content-type");
    bstr_append_cstring_nolen(&(h->value), "text/html");
    bvec_add(&(res->headers), h);
    bstr_append_cstring_nolen(&(res->body), "<html><p>404 Not Found</p></html>");
}

int
default_file_handler(bhttp_request *req, bhttp_response *res)
{
    return bhttp_res_set_body_file_rel(res, bstr_cstring(&req->uri_path));
}

int
helloworld_text_handler(bhttp_request *req, bhttp_response *res)
{
    bstr bs;
    bstr_init(&bs);
    bstr_append_printf(&bs, "<html><p>Hello, world! from URL: %s</p><p>%s</p><p>%s</p></html>",
                       bstr_cstring(&req->uri),
                       bstr_cstring(&req->uri_path),
                       bstr_cstring(&req->uri_query));
    bhttp_res_set_body_text(res, bstr_cstring(&bs));
    bstr_free_contents(&bs);
    return 0;
}

void
handle_request(bhttp_request *req, bhttp_response *res)
{
    response_header rh;
    rh.status.version = "HTTP/1.1";

    switch (req->method) {
        case HTTP_POST:
        case HTTP_GET:
        {
            /* match handlers here */
            default_file_handler(req, res);
//            helloworld_text_handler(req, res);
        }
            break;
    }
}

