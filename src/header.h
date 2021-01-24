//
//  header.h
//  bittyhttp
//
//  Created by Colin Luoma on 2016-06-27.
//  Copyright (c) 2016 Colin Luoma. All rights reserved.
//

#ifndef BITTYHTTP_HEADER_H
#define BITTYHTTP_HEADER_H

#include "bittystring.h"

typedef struct bhttp_header {
    bstr field;
    bstr value;
} bhttp_header;

typedef struct bhttp_headers {

} bhttp_headers;

bhttp_header * http_header_new();
void http_header_free(bhttp_header *h);

#endif //BITTYHTTP_HEADER_H
