#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "server.h"

#include <curl/curl.h>

static void
parse_args(int argc, char **argv, bhttp_server *server)
{
    int c;
    while ((c = getopt(argc, argv, "p:d:b:f:al:s")) != -1) {
        switch (c) {
            case 'p':
                server->port = optarg;
                break;
            case 'd':
                server->docroot = optarg;
                break;
            case 'b':
                server->backlog = atoi(optarg);
                break;
            case 'f':
                server->default_file = optarg;
                break;
            case 'a':
                server->daemon = 1;
                break;
            case 'l':
                server->log_file = optarg;
                break;
            case 's':
                server->use_sendfile = 1;
                break;
            case '?':
                if (optopt == 'c' || optopt == 'd' || optopt == 'b')
                {
                    fprintf(stderr, "Error: -%c option missing\n", optopt);
                    exit(1);
                }
                else
                {
                    fprintf(stderr, "Error: -%c unknown option\n", optopt);
                    exit(1);
                }
                break;
            default:
                fprintf(stderr, "Error parsing options\nExiting...");
                exit(1);
        }
    }

}

int
helloworld_handler(bhttp_request *req, bhttp_response *res)
{
    bhttp_res_add_header(res, "content-type", "text/html");
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
main(int argc, char **argv)
{
//    bhttp_server server = http_server_new();
    bhttp_server server = HTTP_SERVER_DEFAULT; bvec_init(&server.handlers, NULL);
    parse_args(argc, argv, &server);

    if (server.daemon)
    {
        if(daemon(1, 1) == -1)
        {
            perror("Daemon:");
            exit(1);
        }
    }

    printf("Starting bittyhttp with:\n port: %s\n backlog: %d\n docroot: %s\n logfile: %s\n default file: %s\n\n",
           server.port, server.backlog, server.docroot, server.log_file, server.default_file);

    if (http_server_start(&server) != 0)
      return 1;

    fflush(stdout);

    bhttp_add_simple_handler(&server, BHTTP_GET, "/helloworld", helloworld_handler);
    bhttp_add_regex_handler(&server, BHTTP_GET, "^/api/([^/]*)$", helloworld_regex_handler);
    bhttp_add_regex_handler(&server, BHTTP_GET | BHTTP_HEAD, "^/api/([^/]+)/([^/]+)$", helloworld_regex_handler);
    bhttp_add_regex_handler(&server, BHTTP_GET, "^/curl$", curl_handler);
    printf("count: %d\n", bvec_count(&server.handlers));

    http_server_run(&server);
//    http_server_close()

    return 0;
}
