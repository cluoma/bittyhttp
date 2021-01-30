/*
 *  header.c
 *  bittyhttp
 *
 *  Created by Colin Luoma on 2021-01-30.
 *  Copyright (c) 2021 Colin Luoma. All rights reserved.
 */

#include "header.h"

bhttp_header *
bhttp_header_new()
{
    bhttp_header *h = malloc(sizeof(bhttp_header));
    if (h == NULL)
        return NULL;
    bstr_init(&(h->field));
    bstr_init(&(h->value));
    return h;
}

void
bhttp_header_free(bhttp_header *h)
{
    bstr_free_contents(&(h->field));
    bstr_free_contents(&(h->value));
    free(h);
}

int
bhttp_header_name_verify(const char *hfn)
/* checks that a header field names contains only valid characters
 * returns 0 on success 1 on failure */
{
/* illegal characters from (https://www.ietf.org/rfc/rfc2616.txt)
    | "(" | ")" | "<" | ">" | "@"
    | "," | ";" | ":" | "\" | <">
    | "/" | "[" | "]" | "?" | "="
    | "{" | "}" | SP | HT
    CTL            = <any US-ASCII control character (octets 0 - 31) and DEL (127)>
    SP             = <US-ASCII SP, space (32)>
    HT             = <US-ASCII HT, horizontal-tab (9)>
*/
    unsigned char c = *hfn;
    while(c != '\0')
    {
        if (c >= 127) return 1;
        if (c >= 0 && c <= 32) return 1;
        switch(c)
        {
            case '(':
            case ')':
            case '<':
            case '>':
            case '@':
            case ',':
            case ';':
            case ':':
            case '\\':
            case '"':
            case '/':
            case '[':
            case ']':
            case '?':
            case '=':
            case '{':
            case '}':
            case 9: // horizontal-tab
                return 1;
            default:
                break;
        }
        c = *(++hfn);
    }
    return 0;
}