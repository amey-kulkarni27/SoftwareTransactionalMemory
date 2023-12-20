#pragma once

#include <stdio.h>
#include <stdatomic.h>
#include <pthread.h>


// Every memory location (some unit) should have a lock that has a lock bit and a lock version number
// This version number denotes the last timestamp at which the data was written to


typedef struct SegmentNode {
    struct SegmentNode* prev;
    struct SegmentNode* next;
    size_t size;
    void* segment_start; // actual segment where the reads and writes happen
    uint32_t num_words;
    atomic_bool* lock_bit; // each word has a lock bit
    uint32_t* lock_version_number; // each word lock has a version number denoting the last timestamp when it was written to
} SegmentNode;


typedef struct MemoryRegion{
	atomic_long global_clock; // global clock for TL2
	void* start_segment; // pointer to non-deallocable first segment
    struct SegmentNode* alloced_segments; // segments alloced, points to head
    pthread_mutex_t allocation_lock; // since (de)allocations can happen concurrently
    size_t size;        // Size of the non-deallocable memory segment (in bytes)
    size_t align;       // Size of a word in the shared memory region (in bytes)
}MemoryRegion;

typedef struct LLNode{
    uint32_t word_num; // word number along with start of the segment gives us all the necessary location
    void* location; // pointer to the address of the memory location to be written
    void* value; // pointer to the value written to this memory location
    struct LLNode* next;
    SegmentNode* corresponding_segment;
}LLNode;


typedef struct Transaction
{
    MemoryRegion* region;
    bool is_ro;
    uint32_t rv;
    // read set
    uint32_t num_reads; // size of read LL
    LLNode* read_addresses; // head of read-set addresses
    // write set
    uint32_t num_writes; // size of write LL
    LLNode* write_addresses; // head of write-set addresses (nodes contain value as well)
}Transaction;
