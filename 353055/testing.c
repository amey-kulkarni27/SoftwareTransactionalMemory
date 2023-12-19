#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>

#include "macros.h"
#include "tm.h"
#include "data_structures.h"

bool single_thread(){
    shared_t r = tm_create(64, 8);
    tx_t t = tm_begin(r, false);
    void* seg1;
    void* seg2;
    void* seg3;
    tm_alloc(r, t, 32, &seg1);
    tm_alloc(r, t, 128, &seg2);
    tm_alloc(r, t, 256, &seg3);

    long* buf1 = (long*)malloc(2*sizeof(long));
    long* buf2 = (long*)malloc(2*sizeof(long));
    buf2[0] = 79;
    buf2[1] = 3;
    long *y = (long*)((char*)(buf2)+8);
    tm_write(r, t, (void*)(buf2 + 1), 8, seg1);
    Transaction* tr = (Transaction*)t;
    LLNode* nd = (LLNode*)(tr->write_addresses);
    long* lptr = (long*)(nd->value);
    tm_read(r, t, seg1, 16, buf1);
    tm_free(r, t, seg2);
    tm_free(r, t, seg3);
    tm_end(r, t);
    MemoryRegion* region = (MemoryRegion*)r;
    tm_destroy(r);
    return true;
}

void two_threads(){

}

int main(){
    single_thread();
    printf("Success\n");
    return 0;
}