#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>

#include "macros.h"
#include "tm.h"
#include "data_structures.h"

void putVals(shared_t r, tx_t t, void* seg2, void* buffer, size_t size){
    long* buf = (long*)(buffer);
    
    // writing start values
    tm_write(r, t, (void*)(buf), size, (void*)seg2);
}

void readVals(shared_t r, tx_t t, void* seg2, void* buffer, size_t size){
    long* buf = (long*)(buffer);
    tm_read(r, t, (void*)seg2, size, (void*)(buf));
    for(int i = 0; i < size / 8; i++){
        printf("%ld ", buf[0]);
    }
    printf("\n");
}

void swapInsideTransaction(shared_t r, tx_t t, void* seg2, void* buffer, size_t size){

    long* buf = (long*)buffer;
    // printf("S1 %p\n", seg1);
    // printf("S2 %p\n", seg2);
    // printf("S3 %p\n", seg3);
    
    // swapping through the read and write
    tm_read(r, t, (void*)((char*)(seg2) + 8*0), size, buf); // read 1
    tm_read(r, t, (void*)((char*)(seg2) + 8*1), size, (void*)((char*)(buf) + 8)); // read 2
    tm_write(r, t, buf, size, (void*)((char*)(seg2) + 8*1));
    tm_write(r, t, (void*)((char*)buf + 8), size, (void*)((char*)(seg2) + 8*0));
}

void single_playground(){
    shared_t r = tm_create(64, 8);
    long* buffer = (long*)malloc(2*sizeof(long));
    buffer[0] = 79;
    buffer[1] = 3;
    void* seg1;
    void* seg2;
    void* seg3;
    tx_t t = tm_begin(r, false);
    tm_alloc(r, t, 16, &seg2);
    tm_alloc(r, t, 256, &seg3);
    putVals(r, t, seg2, buffer, 16);
    swapInsideTransaction(r, t, seg2, buffer, 8);
    readVals(r, t, seg2, buffer, 16);
    tm_free(r, t, seg3);
    tm_alloc(r, t, 32, &seg1);
    tm_end(r, t);
    tx_t t2 = tm_begin(r, false);
    swapInsideTransaction(r, t2, seg2, buffer, 8);
    readVals(r, t2, seg2, buffer, 16);
    swapInsideTransaction(r, t2, seg2, buffer, 8);
    readVals(r, t2, seg2, buffer, 16);
    tm_end(r, t2);
    free(buffer);

    tm_destroy(r);
}

typedef struct ThreadArgs{
    shared_t r;
    tx_t t;
    void* seg;
    int id;
}ThreadArgs;

void* addSub(void* args_){
    ThreadArgs* args = (ThreadArgs*)args_;
    long* buffer = (long*)malloc(2*sizeof(long));
    buffer[0] = 1;
    buffer[1] = -1;
    
    // Loop 1 times
    for(int i = 0; i < 1; i++){
        // pick two indices out of the four
        srand((unsigned int)(time(NULL)+args->id));
        int num1 = rand() % 4;
        int num2 = rand() % 4;
        printf("%d %d\n", num1, num2);
        tm_read(args->r, args->t, (void*)((char*)(args->seg) + 8*num1), 8, buffer); // read 1
        tm_read(args->r, args->t, (void*)((char*)(args->seg) + 8*num2), 8, &buffer[1]); // read 2
        // printf("B1: %d B2: %d\n", buffer[0], buffer[1]);
        int first_first = rand() % 2; // pick whether to add at first or second index, subtract at the other
        if(true){
            buffer[0]++;
            buffer[1]--;
        }
        else{
            buffer[0]--;
            buffer[1]++;
        }
        tm_write(args->r, args->t, buffer, 8, (void*)((char*)(args->seg) + 8*num1));
        tm_write(args->r, args->t, &buffer[1], 8, (void*)((char*)(args->seg) + 8*num2)); // read 2
    }
    long* ans = (long*)malloc(4*sizeof(long));
    printf("Read all vals\n");
    readVals(args->r, args->t, (void*)(args->seg), (void*)ans, 32);
    tm_end(args->r, args->t);
    free(buffer);
    return NULL;
}

void two_threads(){
    shared_t r = tm_create(64, 8);
    tx_t t = tm_begin(r, false);
    void* seg1;
    tm_alloc(r, t, 32, &seg1); // 4 words
    tm_end(r, t);
    // Create the two threads
    pthread_t thread1, thread2;
    tx_t t1 = tm_begin(r, false);
    tx_t t2 = tm_begin(r, false);
    ThreadArgs ta1 = {r, t1, seg1, 1};
    ThreadArgs ta2 = {r, t2, seg1, 2};
    pthread_create(&thread1, NULL, addSub, &ta1);
    pthread_create(&thread2, NULL, addSub, &ta2);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    tx_t tr = tm_begin(r, true);
    long* buffer = (long*)malloc(4*sizeof(long));
    readVals(r, tr, seg1, buffer, 32);

    tm_destroy(r);
}

int main(){
    // single_playground();
    two_threads();
    assert(NULL);
    printf("Success\n");
    return 0;
}