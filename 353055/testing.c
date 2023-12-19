#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <assert.h>
#include <pthread.h>

#include "macros.h"
#include "tm.h"
#include "data_structures.h"

int main(){
    shared_t x = tm_create(64, 8);
    printf("%zu\n", tm_size(x));
    tm_destroy(x);
    return 0;
}