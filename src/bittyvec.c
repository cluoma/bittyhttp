//
//  vec.c
//  bittyblog
//
//  Created by Colin Luoma on 2016-11-19.
//  Copyright © 2016 Colin Luoma. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include "bittyvec.h"

void
bvec_init(bvec* vec, void (*f)(void *d))
{
    vec->capacity = 0;
    vec->size = 0;
    vec->data = NULL;
    vec->f = f;
}

int
bvec_count(bvec* vec)
{
    return(vec->size);
}

void
bvec_add(bvec* vec, void* data)
{
    if(vec->size == 0)
    {
        vec->data = calloc(10, sizeof(void*));
        vec->capacity = 10;
    }

    if(vec->size == vec->capacity)
    {
        vec->data = realloc(vec->data, vec->capacity * sizeof(void*) * 2);
        vec->capacity = vec->capacity * 2;
    }

    vec->data[vec->size] = data;
    vec->size++;
}

void*
bvec_get(bvec* vec, int i)
{
    return(vec->data[i]);
}

void
bvec_free(bvec* vec)
{
    // Free contents
    for (int i = 0; i < vec->size; i++)
    {
        if (vec->f == NULL)
        {
            free(vec->data[i]);
        } else {
            vec->f(vec->data[i]);
        }
    }

    // Free array
    if (vec->data != NULL) free(vec->data);

    // Free itself
    free(vec);
}
