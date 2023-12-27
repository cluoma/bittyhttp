/*
 *  respond.c
 *  bittyhttp
 *
 *  Created by Colin Luoma on 2021-01-30.
 *  Copyright (c) 2021 Colin Luoma. All rights reserved.
 */

#include <string.h>
#include <strings.h>

#include "request.h"
#include "respond.h"
#include "header.h"
#include "http_parser.h"

void
bhttp_response_init(bhttp_response *res)
{
    bvec_init(&res->headers, (void (*)(void *)) bhttp_header_free);
    res->cookie = bhttp_cookie_new();
    bstr_init(&res->body);
    res->response_code = BHTTP_200_OK;
    res->bodytype = BHTTP_RES_BODY_EMPTY;
}

void
bhttp_response_free(bhttp_response *res)
{
    bvec_free_contents(&res->headers);
    bhttp_cookie_free(res->cookie);
    bstr_free_contents(&res->body);
    res->bodytype = BHTTP_RES_BODY_EMPTY;
}

/* **********************************
 * Headers
 * **********************************
 */
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

const bhttp_header *
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

const bvec *
bhttp_res_get_all_headers(bhttp_response *res)
{
    return &res->headers;
}

/* **********************************
 * Cookies
 * **********************************
 */
int
bhttp_res_add_cookie(bhttp_response *res, const char *field, const char *value)
{
    return bhttp_cookie_add_entry(res->cookie, field, value);
}

const bvec *
bhttp_res_get_cookies(bhttp_response *res)
{
    return bhttp_cookie_get_entries(res->cookie);
}

/* **********************************
 * Headers
 * **********************************
 */
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

int
default_file_handler(bhttp_request *req, bhttp_response *res)
{
    res->response_code = BHTTP_200_OK;
    return bhttp_res_set_body_file_rel(res, bstr_cstring(&req->uri_path));
}
