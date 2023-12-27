/*
 *  main.c
 *  bittyhttp
 *
 *  Created by Colin Luoma on 2021-01-30.
 *  Copyright (c) 2021 Colin Luoma. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../src/server.h"

#include <curl/curl.h>

int
abs_file_handler(bhttp_request *req, bhttp_response *res)
{
    bhttp_res_set_body_file_abs(res, "/home/colin/pics_to_video.txt");
    res->response_code = BHTTP_200_OK;
    return 0;
}

int
rel_file_handler(bhttp_request *req, bhttp_response *res)
{
    bhttp_res_set_body_file_rel(res, "/rel_file_test.html");
    res->response_code = BHTTP_200_OK;
    return 0;
}

int
helloworld_handler(bhttp_request *req, bhttp_response *res)
{
    bhttp_res_add_header(res, "content-type", "text/html");
    bhttp_res_add_header(res, "content-t?ype", "text/html");
    bstr bs;
    bstr_init(&bs);
    bstr_append_printf(&bs, "<html><p>Hello, world! from URL: %s</p><p>%s</p><p>%s</p></html>",
                       bstr_cstring(&req->uri),
                       bstr_cstring(&req->uri_path),
                       bstr_cstring(&req->uri_query));
    bhttp_res_set_body_text(res, bstr_cstring(&bs));
    bstr_free_contents(&bs);
    res->response_code = BHTTP_200_OK;
    return 0;
}

int
helloworld_regex_handler(bhttp_request *req, bhttp_response *res, bvec *args)
{
    bstr bs;
    bstr_init(&bs);
    bstr_append_printf(&bs, "<html><p>Hello, Regex world! from URL: %s</p><p>%s</p><p>%s</p>",
                       bstr_cstring(&req->uri),
                       bstr_cstring(&req->uri_path),
                       bstr_cstring(&req->uri_query));

    bhttp_header *h = bhttp_req_get_header(req, "accept-encoding");
    if (h)
        bstr_append_printf(&bs, "<p><b>accept-encoding</b>: %s</p>", bstr_cstring(&h->value));

    for (int i = 0; i < bvec_count(args); i++)
    {
        bstr *arg = bvec_get(args, i);
        bstr_append_printf(&bs, "<p>arg: %d: %s</p>", i, bstr_cstring(arg));
    }
    bstr_append_cstring_nolen(&bs, "</html>");

    bhttp_res_set_body_text(res, bstr_cstring(&bs));
    bstr_free_contents(&bs);
    bhttp_res_add_header(res, "content-type", "text/html");
    res->response_code = BHTTP_200_OK;
    return 0;
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, bstr *s)
{
    bstr_append_cstring(s, ptr, nmemb);
    return nmemb;
}
int
curl_handler(bhttp_request *req, bhttp_response *res, bvec *args)
{
    CURL *curl;
    CURLcode result;
    bstr *bs = bstr_new();

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://curl.se/libcurl/c/simple.html");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, bs);


        result = curl_easy_perform(curl);
        if (result != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(result));
        curl_easy_cleanup(curl);
    }

    bhttp_res_set_body_text(res, bstr_cstring(bs));
    bstr_free(bs);
    bhttp_res_add_header(res, "content-type", "text/html");
    res->response_code = BHTTP_200_OK;
    return 0;
}

int
iptest_handler(bhttp_request *req, bhttp_response *res)
{
    bstr bs;
    bstr_init(&bs);
    bstr_append_printf(&bs, "<html><p>Hello, your ip address is: '%s'</p></html>",
                       req->ip);

    bhttp_res_set_body_text(res, bstr_cstring(&bs));
    bstr_free_contents(&bs);
    bhttp_res_add_header(res, "content-type", "text/html");
    res->response_code = BHTTP_200_OK;
    return 0;
}

int
hello_cookie_handler(bhttp_request *req, bhttp_response *res)
{
    bhttp_res_add_header(res, "content-type", "text/html");
    bhttp_res_add_header(res, "content-t?ype", "text/html");
    bhttp_res_add_cookie(res, "cookietest1", "this is the value");

    bhttp_cookie * cookie = bhttp_req_get_cookie(req);
    if (cookie != NULL)
        bhttp_cookie_print(cookie);

    bstr bs;
    bstr_init(&bs);
    bstr_append_printf(&bs, "<html><p>Hello, world! from URL: %s</p><p>%s</p><p>%s</p></html>",
                       bstr_cstring(&req->uri),
                       bstr_cstring(&req->uri_path),
                       bstr_cstring(&req->uri_query));

    bhttp_res_set_body_text(res, bstr_cstring(&bs));
    bstr_free_contents(&bs);
    res->response_code = BHTTP_200_OK;
    return 0;
}

int
main(int argc, char **argv)
{
    bhttp_server *server = bhttp_server_new();
    if (server == NULL)
        return 1;

    //parse_args(argc, argv, server);
    bhttp_server_set_port(server, "8989");
    bhttp_server_set_docroot(server, "./examples/www");
    bhttp_server_set_dfile(server, "index.html");

    printf("Starting bittyhttp with:\n port: %s\n backlog: %d\n docroot: %s\n logfile: %s\n default file: %s\n\n",
           server->port, server->backlog, server->docroot, server->log_file, server->default_file);

    fflush(stdout);

    bhttp_add_simple_handler(server, BHTTP_GET, "/ip", iptest_handler);
    bhttp_add_simple_handler(server, BHTTP_GET, "/abs_file", abs_file_handler);
    bhttp_add_simple_handler(server, BHTTP_GET, "/rel_file", rel_file_handler);
    bhttp_add_simple_handler(server, BHTTP_GET, "/helloworld", helloworld_handler);
    bhttp_add_regex_handler(server, BHTTP_GET, "^/api/([^/]*)$", helloworld_regex_handler);
    bhttp_add_regex_handler(server, BHTTP_GET | BHTTP_HEAD, "^/api/([^/]+)/([^/]+)$", helloworld_regex_handler);
    bhttp_add_regex_handler(server, BHTTP_GET, "^/curl$", curl_handler);
    bhttp_add_simple_handler(server, BHTTP_GET, "/hellocookie", hello_cookie_handler);
    printf("count: %d\n", bvec_count(&server->handlers));

    bhttp_server_start(server, 0);

    return 0;
}
