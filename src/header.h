/*
 *  header.h
 *  bittyhttp
 *
 *  Created by Colin Luoma on 2021-01-30.
 *  Copyright (c) 2021 Colin Luoma. All rights reserved.
 */

#ifndef BITTYHTTP_HEADER_H
#define BITTYHTTP_HEADER_H

#include "bittystring.h"

typedef struct bhttp_header {
    bstr field;
    bstr value;
} bhttp_header;

typedef struct bhttp_headers {

} bhttp_headers;

bhttp_header * bhttp_header_new();
void bhttp_header_free(bhttp_header *h);
int bhttp_header_name_verify(const char *hfn);

#endif //BITTYHTTP_HEADER_H
