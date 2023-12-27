//
// Created by colin on 15/06/23.
//

#ifndef BITTYHTTP_COOKIE_H
#define BITTYHTTP_COOKIE_H

#include "bittyvec.h"
#include "bittystring.h"

typedef struct bhttp_cookie_entry {
    bstr field;
    bstr value;
} bhttp_cookie_entry;

typedef struct bhttp_cookie bhttp_cookie;

bhttp_cookie * bhttp_cookie_new();
void bhttp_cookie_free(bhttp_cookie * c);
int bhttp_cookie_parse(bhttp_cookie * c, const char * s);
int bhttp_cookie_add_entry(bhttp_cookie * c, const char * field, const char * value);
const bvec * bhttp_cookie_get_entries(bhttp_cookie * c);
void bhttp_cookie_print(bhttp_cookie * c);

#endif //BITTYHTTP_COOKIE_H
