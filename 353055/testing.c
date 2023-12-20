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
    tm_read(r, t, seg2, size, buffer);
    for(int i = 0; i < size / 8; i++){
        printf("%ld ", ((long*)(buffer))[i]);
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

void WRW(){
    shared_t r = tm_create(64, 8);
    long* write_buffer = (long*)malloc(sizeof(long));
    long* read_buffer = (long*)malloc(sizeof(long));
    write_buffer[0] = 27;
    void* seg2;
    tx_t t = tm_begin(r, false);
    tm_alloc(r, t, 8, &seg2);
    tm_write(r, t, write_buffer, 8, seg2);
    tm_read(r, t, seg2, 8, read_buffer);
    write_buffer[0] = 14;
    tm_write(r, t, write_buffer, 8, seg2);
    tm_end(r, t);
    printf("Value in read buffer %ld\n", read_buffer[0]);
    tx_t t2 = tm_begin(r, false);
    tm_read(r, t, seg2, 8, read_buffer);
    printf("Value in read buffer now %ld\n", read_buffer[0]);
    tm_end(r, t2);
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
    tx_t t3 = tm_begin(r, true);
    tm_read(r, t3, seg2, 16, buffer);
    printf("Buffers: %ld %ld\n", buffer[0], buffer[1]);
    free(buffer);

    tm_destroy(r);
}

typedef struct ThreadArgs{
    shared_t r;
    void* seg;
    int id;
}ThreadArgs;

void* addSub(void* args_){
    ThreadArgs* args = (ThreadArgs*)args_;
    long* buffer = (long*)malloc(2*sizeof(long));
    
    // Loop 1 times
    for(int i = 0; i < 50; i++){
        // pick two indices out of the four
        tx_t t = tm_begin(args->r, false);
        srand((unsigned int)(time(NULL)+args->id) + i);
        int num1 = rand() % 4;
        int num2 = rand() % 4;
        if(num1 == num2){
            i--;
            continue;
        }
        // printf("Thread %d: %d %d\n", args->id, num1, num2);
        int a1 = tm_read(args->r, t, (void*)((char*)(args->seg) + 8*num1), 8, buffer); // read 1
        if(!a1)
            continue;
        int a2 = tm_read(args->r, t, (void*)((char*)(args->seg) + 8*num2), 8, &buffer[1]); // read 2
        if(!a2)
            continue;
        int first_first = rand() % 2; // pick whether to add at first or second index, subtract at the other
        if(first_first){
            buffer[0]++;
            buffer[1]--;
        }
        else{
            buffer[0]--;
            buffer[1]++;
        }
        // printf("B1: %ld B2: %ld\n", buffer[0], buffer[1]);
        int a3 = tm_write(args->r, t, buffer, 8, (void*)((char*)(args->seg) + 8*num1));
        if(!a3)
            continue;
        int a4 = tm_write(args->r, t, &buffer[1], 8, (void*)((char*)(args->seg) + 8*num2)); // read 2
        if(!a4)
            continue;
        // printf("Bool vals %d %d %d %d\n", a1, a2, a3, a4);
        int x = tm_end(args->r, t);
        if(!x)
            continue;
        printf("Thread: %d, Num: %d\n", args->id, i+1);
        long sum = 0;
        for(int i = 0; i < 4; i++){
            long *lptr = (long*)((char*)(args->seg) + 8*i);
            // printf("%p holds %ld\n", lptr, *lptr);
            printf("%ld ", *lptr);
            sum += (*lptr);
        }
        printf("\n\n");
        if(sum != 0){
            printf("%d %d\n", num1, num2);
            for(int i = 0; i < 4; i++){
                long *lptr = (long*)((char*)(args->seg) + 8*i);
                // printf("%p holds %ld\n", lptr, *lptr);
                printf("%ld ", *lptr);
                sum += (*lptr);
            }
        }
        assert(sum == 0);
    }
    // printf("Is thread %d a success? %d\n", args->id, tm_end(args->r, t));
    // Read values from the segment

    // printf("Read all vals\n");
    // for(int i = 0; i < 4; i++){
    //     long *lptr = (long*)((char*)(args->seg) + 8*i);
    //     printf("%p holds %ld\n", lptr, *lptr);
    // }
    free(buffer);
    return NULL;
}

SegmentNode* nodeFromWordAddress2(MemoryRegion* region, char* address_search){
    SegmentNode* cur_node = region -> alloced_segments;
    while(cur_node != NULL){
        char* node_start = (char*)(cur_node->segment_start);
        size_t difference = address_search - node_start;
        if((node_start <= address_search) && (difference < (cur_node->size))){
            break;
        }
        cur_node = cur_node -> next;
    }
    assert(cur_node);
    return cur_node;
}

void two_threads(){
    shared_t r = tm_create(64, 8);
    tx_t t = tm_begin(r, false);
    void* seg1; // segment start
    tm_alloc(r, t, 32, &seg1); // 4 words
    printf("Segment start loc %p\n", seg1);
    tm_end(r, t);
    // Create the two threads
    pthread_t thread1, thread2;
    ThreadArgs ta1 = {r, seg1, 1};
    ThreadArgs ta2 = {r, seg1, 2};
    pthread_create(&thread1, NULL, addSub, &ta1);
    pthread_create(&thread2, NULL, addSub, &ta2);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    tx_t tr = tm_begin(r, true);
    long* buffer = (long*)malloc(4*sizeof(long));
    SegmentNode* seg = nodeFromWordAddress2((MemoryRegion*)r, (char*)seg1);
    printf("Lock status %d %d %d %d\n", seg->lock_bit[0], seg->lock_bit[1], seg->lock_bit[2], seg->lock_bit[3]);
    int y = tm_read(r, tr, seg1, 32, buffer);
    printf("Read %d\n", y);
    int x = tm_end(r, tr);
    printf("End %d\n", x);
    long sum = 0;
    for(int i = 0; i < 4; i++){
        sum += buffer[i];
        printf("%ld ", buffer[i]);
    }
    printf("\n");
    for(int i = 0; i < 4; i++){
        long *lptr = (long*)((char*)(seg1) + 8*i);
        // printf("%p holds %ld\n", lptr, *lptr);
        printf("%ld ", *lptr);
        sum += (*lptr);
    }
    printf("\n");
    free(buffer);
    printf("Sum %ld\n", sum);
    // assert(sum == 0);

    tm_destroy(r);
}

int main(){
    // single_playground();
    two_threads();
    // WRW();
    printf("Success\n");
    return 0;
}