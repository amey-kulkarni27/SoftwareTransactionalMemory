#pragma once

#include <stdio.h>

#include <data_structures.h>

SegmentNode* getNode(MemoryRegion* region, void *segment_start){
    SegmentNode* curNode = region -> alloced_segments;
    while(curNode != NULL){
        if(curNode->shared_segment == segment_start)
            break;
        curNode = curNode -> next;
    }
    return curNode;
}