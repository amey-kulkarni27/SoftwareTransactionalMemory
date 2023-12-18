#pragma once

#include <stdio.h>
#include <stdatomic.h>

// Every memory location (some unit) should have a lock that has a lock bit and a lock version number
// This version number denotes the last timestamp at which the data was written to


typedef struct SegmentNode {
    struct SegmentNode* prev;
    struct SegmentNode* next;
    size_t size;
    void* shared_segment; // actual segment where the reads and writes happen
    uint32_t num_words;
    atomic_bool* lock_bit; // each word has a lock bit
    uint32_t* lock_version_number; // each word lock has a version number denoting the last timestamp when it was written to
} SegmentNode;


typedef struct MemoryRegion{
	atomic_long global_clock; // global clock for TL2
	void* start_segment; // pointer to non-deallocable first segment
    SegmentNode* alloced_segments; // segments alloced through tm_alloc, points to head
    size_t size;        // Size of the non-deallocable memory segment (in bytes)
    size_t align;       // Size of a word in the shared memory region (in bytes)
}MemoryRegion;

typedef struct LLNode{
    void* data; // pointer to either a stored address or a stored value
    LLNode* next;
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
    LLNode* write_addresses; // head of write-set addresses
    LLNode* write_vals; // head of write-set values
}Transaction;
