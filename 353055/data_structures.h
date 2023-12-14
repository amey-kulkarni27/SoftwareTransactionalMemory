#pragma once

#include <stdio.h>
#include <stdatomic.h>


typedef struct SegmentNode {
    struct SegmentNode* prev;
    struct SegmentNode* next;
} SegmentNode;


typedef struct MemoryRegion{
	atomic_long globalClock; // global clock for TL2
	void *startSegment; // pointer to non-deallocable first segment
    SegmentNode *allocedSegments; // segments alloced through tm_alloc
    size_t size;        // Size of the non-deallocable memory segment (in bytes)
    size_t align;       // Size of a word in the shared memory region (in bytes)
}MemoryRegion;

typedef struct Transaction
{
    bool is_ro;
}Transaction;
