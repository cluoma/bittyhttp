//
// Created by colin on 15/06/23.
//

#ifndef BITTYHTTP_COOKIE_H
#define BITTYHTTP_COOKIE_H

typedef struct bhttp_cookie bhttp_cookie;

bhttp_cookie * bhttp_cookie_new();
void bhttp_cookie_free(bhttp_cookie * c);
int bhttp_cookie_parse(bhttp_cookie * c, const char * s);
void bhttp_cookie_print(bhttp_cookie * c);

#endif //BITTYHTTP_COOKIE_H
