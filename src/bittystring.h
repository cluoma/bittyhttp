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

#define BS_MAX_CAPACITY (UINT64_MAX >> 1)
#define BS_MAX_SSO_CAPACITY 23

#define bstr_const_str(X) (X), ((uint64_t)sizeof(X)-1)

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

/*
 * Creators
 */
bstr * bstr_new(void);
bstr * bstr_new_from_cstring(const char *cs, uint64_t len);
/* move gives bittystring ownership of the string
 * shortstring is not utilized here */
bstr * bstr_new_move(char *cs, uint64_t len);
void bstr_init(bstr *bs);
const char * bstr_error_string(bstr_ret_val r);
/*
 * Freers
 */
void bstr_free(bstr *bs);
void bstr_free_contents(bstr *bs);
/*
 * Accessors
 */
uint64_t bstr_size(const bstr *bs);
uint64_t bstr_capacity(const bstr *bs);
const char * bstr_cstring(const bstr *bs);
/*
 * Modifiers
 */
int bstr_append_cstring(bstr *bs, const char *cs, uint64_t len);
int bstr_append_cstring_nolen(bstr *bs, const char *cs);
int bstr_append_char(bstr *bs, const char c);
int bstr_append_printf(bstr *bs, const char * format, ...);
int bstr_prepend_cstring(bstr *bs, const char *cs, uint64_t len);
int bstr_prepend_cstring_nolen(bstr *bs, const char *cs);
int bstr_prepend_char(bstr *bs, const char c);
int bstr_prepend_printf(bstr *bs, const char * format, ...);
/* move gives bittystring ownership of the string
 * shortstring is not utilized here */
void bstr_replace_move(bstr *bs, char *cs, uint64_t len);
void bstr_replace_move_nolen(bstr *bs, char *cs);

#endif //BITTYSTRING_BITTYSTRING_H
