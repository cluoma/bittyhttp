/*
 *  respond.c
 *  bittyhttp
 *
 *  Created by Colin Luoma on 2021-01-30.
 *  Copyright (c) 2021 Colin Luoma. All rights reserved.
 */

#include <string.h>
#include <strings.h>
#include <errno.h>

#include "request.h"
#include "respond.h"
#include "header.h"
#include "http_parser.h"

#define C(k, v) [k] = (v),
static const char * bhttp_res_codes_string[] = { BHTTP_RES_CODES };
#undef C

void
bhttp_response_init(bhttp_response *res)
{
    bvec_init(&res->headers, (void (*)(void *)) bhttp_header_free);
    bstr_init(&res->body);
    res->response_code = BHTTP_200_OK;
    res->bodytype = BHTTP_RES_BODY_EMPTY;
}

void
bhttp_response_free(bhttp_response *res)
{
    bvec_free_contents(&res->headers);
    bstr_free_contents(&res->body);
    res->bodytype = BHTTP_RES_BODY_EMPTY;
}

int
bhttp_res_add_header(bhttp_response *res, const char *field, const char *value)
{
    /* first check that field name is valid */
    if (bhttp_header_name_verify(field))
        return 1;
    bhttp_header *h = bhttp_header_new();
    if (h == NULL) return 1;
    if (bstr_append_cstring_nolen(&(h->field), field) != 0 ||
        bstr_append_cstring_nolen(&(h->value), value) != 0)
    {
        bhttp_header_free(h);
        return 1;
    }
    bvec_add(&res->headers, h);
    return 0;
}

bhttp_header *
bhttp_res_get_header(bhttp_response *res, const char *field)
{
    bvec *headers = &res->headers;
    for (int i = 0; i < bvec_count(headers); i++)
    {
        bhttp_header *cur = (bhttp_header *)bvec_get(headers, i);
        bstr *hf = &cur->field;
        if (strcasecmp(field, bstr_cstring(hf)) == 0)
            return cur;
    }
    return NULL;
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
    bstr *header_text = bstr_new();
    if (header_text == NULL) return NULL;
    bstr_append_printf(header_text, "HTTP/1.1 %s\r\n", bhttp_res_codes_string[res->response_code]);
    for (int i = 0; i < bvec_count(&res->headers); i++)
    {
        bhttp_header *h = bvec_get(&res->headers, i);
//        bstr_append_printf(header_text, "%s: %s\r\n", bstr_cstring(&h->field), bstr_cstring(&h->value));
        bstr_append_cstring(header_text, bstr_cstring(&h->field), bstr_size(&h->field));
        bstr_append_cstring(header_text, bstr_const_str(": "));
        bstr_append_cstring(header_text, bstr_cstring(&h->value), bstr_size(&h->value));
        bstr_append_cstring(header_text, bstr_const_str("\r\n"));
    }
    bstr_append_cstring(header_text, "\r\n", 2);
    return header_text;
}

int
default_file_handler(bhttp_request *req, bhttp_response *res)
{
    res->response_code = BHTTP_200_OK;
    return bhttp_res_set_body_file_rel(res, bstr_cstring(&req->uri_path));
}
