//
//  header.c
//  bittyhttp
//
//  Created by Colin Luoma on 2016-06-27.
//  Copyright (c) 2016 Colin Luoma. All rights reserved.
//

#include "header.h"

bhttp_header *
http_header_new()
{
    bhttp_header *h = malloc(sizeof(bhttp_header));
    if (h == NULL)
        return NULL;
    bstr_init(&(h->field));
    bstr_init(&(h->value));
    return h;
}

void
http_header_free(bhttp_header *h)
{
    bstr_free_contents(&(h->field));
    bstr_free_contents(&(h->value));
    free(h);
}
