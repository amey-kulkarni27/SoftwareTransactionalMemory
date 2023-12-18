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

// A bit unsure about this implementation
SegmentNode* nodeFromWordAddress(MemoryRegion* region, const char* address_search){
    SegmentNode* curNode = region -> alloced_segments;
    while(curNode != NULL){
        size_t difference = (char*)curNode - address_search;
        if(difference + curNode->shared_segment == address_search)
            break;
        curNode = curNode -> next;
    }
    return curNode;
}


LLNode* getWriteNode(void* source_address, LLNode* cur_node){
    while(cur_node){
        if(cur_node->location == cur_node)
            break;
        cur_node = cur_node->next;
    }
    return cur_node;
}