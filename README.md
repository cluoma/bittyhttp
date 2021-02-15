# bittyhttp

A threaded HTTP library and basic webserver for creating REST services in C.

## Basic Usage

```c
#include "server.h"

int helloworld_handler(bhttp_request *req, bhttp_response *res);

int
main(int argc, char **argv)
{
    bhttp_server *server = bhttp_server_new();
    if (server == NULL) return 1;
    
    bhttp_server_set_ip(server, "0.0.0.0");
    bhttp_server_set_port(server, "8989");
    bhttp_server_set_docroot(server, "./www");
    bhttp_server_set_dfile(server, "index.html");
    
    if (bhttp_server_bind(server) != 0) return 1;
    
    bhttp_add_simple_handler(server,
                             BHTTP_GET | BHTTP_POST,  // declare supported http methods
                             "/helloworld",           // pattern to match uri path
                             helloworld_handler);     // callback function pointer
    
    bhttp_server_run(server);
    
    return 0;
}
```

## Handlers

In addition to simply serving files, `bittyhttp` also has several different handler types that the user can define. Check out `main.c` for even more examples.

Handlers are matched in the order they are added. If two handlers would match the same uri path, then the handler added first will get the callback.

### Simple Handler

Simple handlers must match the uri path exactly.

```c
int
helloworld_handler(bhttp_request *req, bhttp_response *res)
{
    /* business logic */
    bstr bs;
    bstr_init(&bs);
    bstr_append_printf(&bs, "<html><p>Hello, world! from URL: %s</p><p>%s</p><p>%s</p></html>",
                       bstr_cstring(&req->uri),
                       bstr_cstring(&req->uri_path),
                       bstr_cstring(&req->uri_query));
    bhttp_res_set_body_text(res, bstr_cstring(&bs));
    bstr_free_contents(&bs);
    
    /* add custom headers and response code */
    bhttp_res_add_header(res, "content-type", "text/html");
    res->response_code = BHTTP_200_OK;
    return 0;
}

bhttp_add_simple_handler(&server, BHTTP_GET, "/helloworld", helloworld_handler);
```

### Regex Handler

Regex handlers use Linux's POSIX regex library to match on the uri path. Any matched groups will also be passed to the handler function.

```c
int
helloworld_regex_handler(bhttp_request *req, bhttp_response *res, bvec *args)
{
    /* business logic */
    bstr bs;
    bstr_init(&bs);
    bstr_append_printf(&bs, "<html><p>Hello, Regex world! from URL: %s</p><p>%s</p><p>%s</p>",
                       bstr_cstring(&req->uri),
                       bstr_cstring(&req->uri_path),
                       bstr_cstring(&req->uri_query));
    /* check the request for a specific header */
    bhttp_header *h = bhttp_req_get_header(req, "accept-encoding");
    if (h)
        bstr_append_printf(&bs, "<p><b>accept-encoding</b>: %s</p>", bstr_cstring(&h->value));
    /* add all Regex matched groups to our output */
    for (int i = 0; i < bvec_count(args); i++)
    {
        bstr *arg = bvec_get(args, i);
        bstr_append_printf(&bs, "<p>arg: %d: %s</p>", i, bstr_cstring(arg));
    }
    bstr_append_cstring_nolen(&bs, "</html>");

    bhttp_res_set_body_text(res, bstr_cstring(&bs));
    bstr_free_contents(&bs);
    /* custom headers and response code */
    bhttp_res_add_header(res, "content-type", "text/html");
    res->response_code = BHTTP_200_OK;
    return 0;
}

bhttp_add_regex_handler(&server, BHTTP_GET | BHTTP_HEAD, "^/api/([^/]+)/([^/]+)$", helloworld_regex_handler);
```

### File Handlers

Instead of using `bhttp_res_set_body_text`, we can use the function `bhttp_set_body_file_rel/abs` to return a file. This is more efficient than than supplying the binary data ourselves because `sendfile` can avoid unecessary data copying.

`bhttp_res_set_body_file_rel` will append the uri path to `bittyhttp`s docroot. `bhttp_res_set_body_file_abs` will take the given filepath as is, and try to serve it.

If `bittyhttp` cannot read the file or the file is not found, a 404 message is returned.

```c
int
rel_file_handler(bhttp_request *req, bhttp_response *res)
{
    bhttp_res_set_body_file_rel(res, "/hugo/404.html");
    res->response_code = BHTTP_200_OK;
    return 0;
}

bhttp_add_simple_handler(&server, BHTTP_GET, "/rel_file", rel_file_handler);
```

## Sites Using bittyhttp

* [squid poll](https://squidpoll.com/) - create and share polls for fun. it's squidtastic!

## Roadmap

In the future I would like to add the following features to `bittyhttp`:

* file/multipart upload support
* Lua integration for handlers

## Use of other code

This project would not be possible without the following resources:

* [Beej's Guide to Network Programming](http://beej.us/guide/bgnet/output/print/bgnet_USLetter_2.pdf)    
* [HTTP Parser](https://github.com/nodejs/http-parser)
* [lighttpd 1.4](https://redmine.lighttpd.net/projects/lighttpd/wiki)
