//
// bittystring.c
//
// Created by colin on 2020-10-29.
//

#include "bittystring.h"

#define C(k, v) [k] = (v),
static const char * bstr_ret_string[] = { RETURN_CODES };
#undef C

#define BS_MALLOC(X) malloc(X)
#define BS_REALLOC(X, Y) realloc((X), (Y))
#define BS_FREE(X) free(X)

#define SSO_SET_MASK 0x80
#define SSO_UNSET_MASK 0x7F

#define BSTR_IS_SSO(X) ((X)->ss.short_size >> 7)
#define BSTR_SET_SSO(X) ((X)->ss.short_size |= SSO_SET_MASK)
#define BSTR_UNSET_SSO(X) ((X)->ss.short_size &= SSO_UNSET_MASK)
#define BSTR_SSO_SIZE(X) ((X)->ss.short_size & SSO_UNSET_MASK)

static uint64_t
next_power2(uint64_t x)
// Returns the smallest power of 2 greater than x
// GCC builtin is undefined for 0. Don't use this function for 0.
{
    // if (x == 0) return 0;
#ifdef __GNUC__
//    printf("unsigned long long: %llu bytes\n", (uint64_t)sizeof(unsigned long long)*8);
//    printf("x: %llu\n", x);
//    printf("builtin of x: %d\n", __builtin_clzll(x));
//    printf("bytes minus builtin: %llu\n", sizeof(unsigned long long)*8 - (uint64_t)__builtin_clzll(x));
//    printf("next pow 2: %llu\n", (uint64_t)1 << ((sizeof(unsigned long long)*8) - __builtin_clzll(x)));
    return (uint64_t)1 << ((sizeof(unsigned long long)*8) - (unsigned long)__builtin_clzll(x));
#else
    uint32_t power = 0;
    while (x > 0)
    {
        x >>= 1;
        power += 1;
    }
    return 1 << power;
#endif
}

static void
bstr_set_sso_size(bstr *bs, uint8_t size)
{
    bs->ss.short_size = size;
    BSTR_SET_SSO(bs);
}

uint64_t
bstr_size(bstr *bs)
{
    if (BSTR_IS_SSO(bs))
    {
        return (uint64_t)BSTR_SSO_SIZE(bs);
    }
    else
    {
        return bs->ls.size;
    }
}

uint64_t
bstr_capacity(bstr *bs)
{
    if (BSTR_IS_SSO(bs))
    {
        return (uint64_t)BS_MAX_SSO_CAPACITY;
    }
    else
    {
        return bs->ls.capacity;
    }
}

const char *
bstr_cstring(bstr *bs)
{
    if (BSTR_IS_SSO(bs))
    {
        return bs->ss.short_str;
    }
    else
    {
        return bs->ls.buf;
    }
}

bstr *
bstr_new(void)
{
    bstr *bs = BS_MALLOC(sizeof(bstr));
    if (bs == NULL)
        return NULL;

    bstr_init(bs);

    return bs;
}

bstr *
bstr_new_from_cstring(const char *cs, uint64_t len)
{
    bstr *bs = bstr_new();
    if (bs == NULL)
        return NULL;

    if (bstr_append_cstring(bs, cs, len) != BS_SUCCESS)
    {
        bstr_free(bs);
        return NULL;
    }

    return bs;
}

void
bstr_init(bstr *bs)
{
    memset(bs, 0, sizeof(bstr));
    BSTR_SET_SSO(bs);
    bstr_set_sso_size(bs, 0);
}

static int
bstr_resize_buffer(bstr *bs, uint64_t new_capacity)
{
    char * new_buf = BS_REALLOC(bs->ls.buf, new_capacity);
    if (new_buf == NULL)
    {
        return BS_FAIL;
    }

    bs->ls.buf = new_buf;
    bs->ls.capacity = new_capacity;
    return BS_SUCCESS;
}

static uint64_t
bstr_next_capacity(uint64_t size)
{
    uint64_t capacity = next_power2(size);
    if (capacity > BS_MAX_CAPACITY)
        capacity = BS_MAX_CAPACITY;
    return capacity;
}

static int
bstr_expand_by(bstr *bs, uint64_t len)
{
#ifdef DEBUG
    if (BSTR_IS_SSO(bs))
        printf("original size: %" PRIu8 "\n", BSTR_SSO_SIZE(bs));
    else
        printf("original size: %" PRIu64 "\n", bs->size);
#endif
    if (BSTR_IS_SSO(bs))
    {
        uint8_t sso_size = BSTR_SSO_SIZE(bs);
        if (sso_size + len + 1 > BS_MAX_SSO_CAPACITY)
        {
            uint64_t initial_capacity = bstr_next_capacity(sso_size + len + 1);
            if (sso_size + len + 1 > initial_capacity)
            {
                fprintf(stderr, "Request length is too large\n");
                return BS_FAIL;
            }

            char * buf = BS_MALLOC(initial_capacity);
            if (!buf)
            {
                fprintf(stderr, "Failed to allocate buffer\n");
                return BS_FAIL;
            }
            memcpy(buf, bs->ss.short_str, sso_size);
            buf[sso_size] = '\0';
            bs->ls.buf = buf;
            bs->ls.size = sso_size;
            bs->ls.capacity = initial_capacity;
        }
    }
    else
    {
        if (bs->ls.size + len + 1 > bs->ls.capacity)
        {
            uint64_t next_capacity = bstr_next_capacity(bs->ls.size + len + 1);
            if (bs->ls.size + len + 1 > next_capacity)
            {
                fprintf(stderr, "Request length is too large\n");
                return BS_FAIL;
            }
            if (bstr_resize_buffer(bs, next_capacity) != BS_SUCCESS)
            {
                fprintf(stderr, "Failed to expand buffer\n");
                return BS_FAIL;
            }
        }
    }
#ifdef DEBUG
    if (BSTR_IS_SSO(bs))
        printf("new size: %" PRIu8 "\n", BSTR_SSO_SIZE(bs));
    else
        printf("new size: %" PRIu64 "\n", bs->size);
#endif
    return BS_SUCCESS;
}

int
bstr_append_cstring(bstr *bs, const char *cs, uint64_t len)
{
#ifdef DEBUG
    for (int i = 0; i < 24; i++)
    {
        printf("[%d]: %" PRIu8 "\n", i, bs->short_str[i]);
    }
    printf("[23]: %" PRIu64 "\n", bs->capacity);
#endif
    if (len == 0) return BS_SUCCESS;
    if (len <  0) return BS_FAIL;

    if (bstr_expand_by(bs, len) != BS_SUCCESS)
        return BS_FAIL;

    if (BSTR_IS_SSO(bs))
    {
        uint8_t sso_size = BSTR_SSO_SIZE(bs);
        memcpy(bs->ss.short_str + sso_size, cs, len);
        bs->ss.short_str[sso_size + len] = '\0';
        bstr_set_sso_size(bs, sso_size + len);
    }
    else
    {
        memcpy(bs->ls.buf + bs->ls.size, cs, len);
        bs->ls.buf[bs->ls.size + len] = '\0';
        bs->ls.size += len;
    }

    return BS_SUCCESS;
}

int
bstr_prepend_cstring(bstr *bs, const char *cs, uint64_t len)
{
#ifdef DEBUG
    for (int i = 0; i < 24; i++)
    {
        printf("[%d]: %" PRIu8 "\n", i, bs->short_str[i]);
    }
    printf("[23]: %" PRIu64 "\n", bs->capacity);
#endif
    if (len == 0) return BS_SUCCESS;
    if (len <  0) return BS_FAIL;

    if (bstr_expand_by(bs, len) != BS_SUCCESS)
        return BS_FAIL;

    if (BSTR_IS_SSO(bs))
    {
        uint8_t sso_size = BSTR_SSO_SIZE(bs);
        memmove(bs->ss.short_str + len, bs->ss.short_str, sso_size);
        memcpy(bs->ss.short_str, cs, len);
        bs->ss.short_str[sso_size + len] = '\0';
        bstr_set_sso_size(bs, sso_size + len);
    }
    else
    {
        memmove(bs->ls.buf + len, bs->ls.buf, bs->ls.size);
        memcpy(bs->ls.buf, cs, len);
        bs->ls.buf[bs->ls.size + len] = '\0';
        bs->ls.size += len;
    }

    return BS_SUCCESS;
}

int
bstr_append_printf(bstr *bs, const char * format, ...)
{
    va_list ap;
    va_start(ap, format);
    int len = vsnprintf(NULL, 0, format, ap);
    va_end(ap);
    if (len == -1)
        return BS_FAIL;

    if (bstr_expand_by(bs, (uint64_t) len) != BS_SUCCESS)
        return BS_FAIL;

    va_start(ap, format);
    if (BSTR_IS_SSO(bs))
    {
        uint8_t sso_size = BSTR_SSO_SIZE(bs);
        len = vsnprintf(bs->ss.short_str + sso_size, len+1, format, ap);
        if (len == -1)
            return BS_FAIL;
        bstr_set_sso_size(bs, sso_size + (uint8_t)len);
    }
    else
    {
        len = vsnprintf(bs->ls.buf + bs->ls.size, len+1, format, ap);
        if (len == -1)
            return BS_FAIL;
        bs->ls.size += (uint64_t)len;
    }
    va_end(ap);

    return BS_SUCCESS;
}

int
bstr_prepend_printf(bstr *bs, const char * format, ...)
{
    va_list ap;
    va_start(ap, format);
    int len = vsnprintf(NULL, 0, format, ap);
    va_end(ap);
    if (len == -1)
        return BS_FAIL;

    if (bstr_expand_by(bs, (uint64_t) len) != BS_SUCCESS)
        return BS_FAIL;

    va_start(ap, format);
    if (BSTR_IS_SSO(bs))
    {
        uint8_t sso_size = BSTR_SSO_SIZE(bs);
        memmove(bs->ss.short_str + len, bs->ss.short_str, sso_size);
        char save = bs->ss.short_str[len];
        len = vsnprintf(bs->ss.short_str, len+1, format, ap);
        bs->ss.short_str[len] = save;
        bs->ss.short_str[sso_size + len] = '\0';
        if (len == -1)
            return BS_FAIL;
        bstr_set_sso_size(bs, sso_size + (uint8_t)len);
    }
    else
    {
        memmove(bs->ls.buf + len, bs->ls.buf, bs->ls.size);
        char save = bs->ls.buf[len];
        len = vsnprintf(bs->ls.buf, len+1, format, ap);
        bs->ls.buf[len] = save;
        bs->ls.buf[bs->ls.size + len] = '\0';
        if (len == -1)
            return BS_FAIL;
        bs->ls.size += (uint64_t)len;
    }
    va_end(ap);

    return BS_SUCCESS;
}

void
bstr_free_buf(bstr *bs)
{
    if (!BSTR_IS_SSO(bs) && bs->ls.buf)
        BS_FREE(bs->ls.buf);
}

void
bstr_free(bstr *bs)
{
    bstr_free_buf(bs);
    BS_FREE(bs);
}

const char * bstr_error_string(bstr_ret_val r)
{
    return bstr_ret_string[r];
}
