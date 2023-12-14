#pragma once

#include <stdio.h>
#include <stdatomic.h>


struct SegmentNode {
    struct SegmentNode* prev;
    struct SegmentNode* next;
    // uint8_t segment[] // segment of dynamic size
};


typedef struct MemoryRegion{
	atomic_long globalClock; // global clock for TL2
	void *startSegment; // pointer to non-deallocable first segment

}MemoryRegion;
