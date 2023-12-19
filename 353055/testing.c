#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include "macros.h"
#include "tm.h"
#include "data_structures.h"

void putVals(shared_t r, tx_t t, void* seg2){
    long* buf2 = (long*)malloc(2*sizeof(long));
    buf2[0] = 79;
    buf2[1] = 3;
    // writing start values
    tm_write(r, t, (void*)(buf2), 16, (void*)seg2);
    free(buf2);
}

void readVals(shared_t r, tx_t t, void* seg2){
    long* buf1 = (long*)malloc(2*sizeof(long));
    tm_read(r, t, (void*)seg2, 16, (void*)(buf1));
    printf("Num1 %ld\n", buf1[0]);
    printf("Num2 %ld\n", buf1[1]);
    free(buf1);
}

void swapInsideTransaction(shared_t r, tx_t t, void* seg2){

    long* buf1 = (long*)malloc(2*sizeof(long));
    // printf("S1 %p\n", seg1);
    // printf("S2 %p\n", seg2);
    // printf("S3 %p\n", seg3);
    
    // swapping through the read and write
    tm_read(r, t, (void*)((char*)(seg2) + 8*0), 8, buf1); // read 1
    tm_read(r, t, (void*)((char*)(seg2) + 8*1), 8, (void*)((char*)(buf1) + 8)); // read 2
    tm_write(r, t, buf1, 8, (void*)((char*)(seg2) + 8*1));
    tm_write(r, t, (void*)((char*)buf1 + 8), 8, (void*)((char*)(seg2) + 8*0));
    // read the values finally into buf2
    long* buf2 = (long*)malloc(2*sizeof(long));
    tm_read(r, t, (void*)seg2, 16, (void*)(buf2));

    free(buf1);
}

void single_playground(){
    shared_t r = tm_create(64, 8);
    tx_t t = tm_begin(r, false);
    void* seg1;
    void* seg2;
    void* seg3;
    tm_alloc(r, t, 16, &seg2);
    tm_alloc(r, t, 256, &seg3);
    putVals(r, t, seg2);
    swapInsideTransaction(r, t, seg2);
    readVals(r, t, seg2);
    tm_free(r, t, seg3);
    tm_alloc(r, t, 32, &seg1);
    tm_end(r, t);
    tx_t t2 = tm_begin(r, false);
    swapInsideTransaction(r, t2, seg2);
    readVals(r, t2, seg2);
    swapInsideTransaction(r, t2, seg2);
    readVals(r, t2, seg2);
    tm_end(r, t2);

    tm_destroy(r);
}

typedef struct ThreadArgs{
    shared_t r;
    tx_t t;
    void* seg;
}ThreadArgs;

void addSub(void* args_){
    ThreadArgs* args = (ThreadArgs*)args_;
    long* write_buffer = (long*)malloc(2*sizeof(long));
    write_buffer[0] = 1;
    write_buffer[1] = -1;
    long* read_buffer = (long*)malloc(2*sizeof(long));
    
    // Loop 1000 times
    for(int i = 0; i < 1000; i++){
        // pick two indices out of the four
        srand((unsigned int)time(NULL));
        int num1 = rand() % 4;
        int num2 = rand() % 4;
        int first_first = rand() % 2; // pick whether to add at first or second index, subtract at the other
        if(first_first){
            tm_read(args->r, args->t, (void*)((char*)(args->seg) + 8*num1), 8, write_buffer); // read 1
            tm_read(args->r, args->t, (void*)((char*)(args->seg) + 8*num2), 8, (void*)((char*)write_buffer + 1)); // read 2
            write_buffer[0]++;
            write_buffer[1]--;
            tm_write(args->r, args->t, write_buffer, 8, (void*)((char*)(args->seg) + 8*num1));
            tm_write(args->r, args->t, (void*)((char*)write_buffer + 1), 8, (void*)((char*)(args->seg) + 8*num2)); // read 2
        };
    }

    
}

void two_threads(){
    shared_t r = tm_create(64, 8);
    tx_t t = tm_begin(r, false);
    void* seg1;
    tm_alloc(r, t, 32, &seg1); // 4 words
    // Create the two threads
    pthread_t thread1, thread2;
    tx_t t1 = tm_begin(r, false);
    tx_t t2 = tm_begin(r, false);
    ThreadArgs ta1 = {r, t1, seg1};
    ThreadArgs ta2 = {r, t2, seg1};
    pthread_create(&thread1, NULL, addSub, &ta1);
    pthread_create(&thread2, NULL, addSub, &ta2);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    tm_destroy(r);
}

int main(){
    single_playground();
    printf("Success\n");
    return 0;
}