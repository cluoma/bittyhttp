//
// Created by colin on 15/06/23.
//

#include "cookie.h"
#include "bittystring.h"
#include "bittyvec.h"

typedef struct bhttp_cookie_entry {
    bstr field;
    bstr value;
} bhttp_cookie_entry;

void
bhttp_cookie_entry_free(bhttp_cookie_entry * ce)
{
    bstr_free_contents(&(ce->field));
    bstr_free_contents(&(ce->value));
    free(ce);
}

typedef struct bhttp_cookie {
    bvec * entries;
} bhttp_cookie;

bhttp_cookie *
bhttp_cookie_new()
{
    bhttp_cookie * c = malloc(sizeof(bhttp_cookie));
    if (c == NULL)
        return NULL;
    c->entries = malloc(sizeof(bvec));

    bvec_init(c->entries, (void (*)(void *)) &bhttp_cookie_entry_free);

    return c;
}

void
bhttp_cookie_free_contents(bhttp_cookie * c)
{
    bvec_free(c->entries);
}

void
bhttp_cookie_free(bhttp_cookie * c)
{
    bhttp_cookie_free_contents(c);
    free(c);
}

int
bhttp_cookie_parse(bhttp_cookie * c, const char * s)
{
    bhttp_cookie_entry * ce = malloc(sizeof(bhttp_cookie_entry));
    bstr_init(&ce->field);
    bstr_init(&ce->value);

    enum state {FIELD, VALUE, DELIM};
    enum state current_state = FIELD;
    while (*s != '\0')
    {
        if (*s == '=')
        /* field-value delimiter */
        {
            if (current_state == FIELD)
                current_state = VALUE;
            else
                goto bad;
        }
        else if (*s == ';')
        /* cookie-pair delimiter, first character */
        {
            if (current_state == VALUE)
                current_state = DELIM;
            else
                goto bad;
        }
        else if (*s == ' ')
        /* cookie-pair delimiter, second character, start new cookie-pair */
        {
            if (current_state == DELIM)
            {
                bvec_add(c->entries, ce);
                ce = malloc(sizeof(bhttp_cookie_entry));
                bstr_init(&ce->field);
                bstr_init(&ce->value);
                current_state = FIELD;
            }
            else
                goto bad;
        }
        else
        /* add character to cookie field or value */
        {
            if (current_state == FIELD)
                bstr_append_char(&ce->field, *s);
            else
                bstr_append_char(&ce->value, *s);
        }
        s++;
    }

    /* commit current value */
    if (current_state == VALUE || current_state == DELIM)
    {
        bvec_add(c->entries, ce);
    }

    return 0;

    bad:
    bhttp_cookie_free_contents(c);
    bhttp_cookie_entry_free(ce);
    return 1;
}

void bhttp_cookie_print(bhttp_cookie * c)
{
    bvec * c_entries = c->entries;
    printf("cookie count: %d\n", bvec_count(c_entries));

    printf("{");
    for (int i = 0; i < bvec_count(c_entries); i++)
    {
        bhttp_cookie_entry * ce = bvec_get(c_entries, i);
        printf("%s=%s", bstr_cstring(&ce->field), bstr_cstring(&ce->value));
        if (i != bvec_count(c_entries) - 1)
            printf("; ");
    }
    printf("}\n");
}