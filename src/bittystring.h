//
// bittystring.h
//
// Created by colin on 2020-10-29.
//

#ifndef BITTYSTRING_BITTYSTRING_H
#define BITTYSTRING_BITTYSTRING_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#define BS_MAX_CAPACITY 0x7FFFFFFFFFFFFFFF
#define BS_MAX_SSO_CAPACITY 23

#define RETURN_CODES    C(BS_SUCCESS, "All good\n")             \
                        C(BS_FAIL, "Function failed\n")
#define C(k, v) k,
typedef enum { RETURN_CODES } bstr_ret_val;
#undef C

struct bstr_long_string
{
    char* buf;
    uint64_t size;  // not including null terminator
    uint64_t capacity;
};
struct bstr_short_string
{
    char short_str[24-1];
    uint8_t short_size;  // not including null terminator
};
typedef union
{
    struct bstr_long_string ls;
    struct bstr_short_string ss;
} bstr;

/* creators */
bstr *bstr_new(void);
bstr * bstr_new_from_cstring(const char *cs, uint64_t len);
void bstr_init(bstr *bs);
const char * bstr_error_string(bstr_ret_val r);
/* freers */
void bstr_free(bstr *bs);
void bstr_free_contents(bstr *bs);
/* accessors */
uint64_t bstr_size(bstr *bs);
uint64_t bstr_capacity(bstr *bs);
const char * bstr_cstring(bstr *bs);
/* modifiers */
int bstr_append_cstring(bstr *bs, const char *cs, uint64_t len);
int bstr_append_printf(bstr *bs, const char * format, ...);
int bstr_prepend_cstring(bstr *bs, const char *cs, uint64_t len);
int bstr_prepend_printf(bstr *bs, const char * format, ...);

#endif //BITTYSTRING_BITTYSTRING_H
