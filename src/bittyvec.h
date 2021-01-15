//
//  bittyvec.h
//  bittyvec
//
//  Created by Colin Luoma on 2016-11-19.
//  Copyright © 2016 Colin Luoma. All rights reserved.
//

#ifndef BITTYVEC_BITTYVEC_H
#define BITTYVEC_BITTYVEC_H

/* bvec struct */
typedef struct {
    int capacity;
    int size;
    void** data;
    void (*f) (void *);
} bvec;

void bvec_init(bvec* vec, void (*f)(void *));
void bvec_free(bvec* vec);
void bvec_free_contents(bvec *vec);

int bvec_count(bvec* vec);
void bvec_add(bvec* vec, void* data);
void* bvec_get(bvec* vec, int i);

#endif /* BITTYVEC_BITTYVEC_H */
