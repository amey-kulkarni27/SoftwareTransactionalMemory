#pragma once

#include <stdio.h>
#include <stdbool.h>

#include "data_structures.h"

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
        if(cur_node->location == source_address)
            break;
        cur_node = cur_node->next;
    }
    return cur_node;
}

bool acquireLocks(LLNode* write_node){
    LLNode* cur = write_node;
    bool success = true;
    while(cur){
        SegmentNode* segment = cur -> corresponding_segment;
        size_t word = cur -> word_num;
        atomic_bool expected = false;
        if(!atomic_compare_exchange_strong(&(segment->lock_bit[word]), &expected, true)){
            success = false;
            break;
        }
        cur = cur -> next;
    }
    if(!success){
        LLNode* remove_node = write_node;
        while(remove_node != cur){
            size_t word = remove_node -> word_num;
            SegmentNode* segment = remove_node -> corresponding_segment;
            atomic_store(&(segment->lock_bit[word]), 0);
            remove_node = remove_node -> next;
        }
    }
    return success;
}

void cleanAddresses(LLNode* node){
    LLNode* cur = node;
    while(cur){
        LLNode* nxt = cur -> next;
        free(cur->value);
        free(cur);
        cur = nxt;
    }
}

void cleanTransaction(Transaction* t){
    cleanAddresses(t->read_addresses);
    cleanAddresses(t->write_addresses);
    free(t);
}

bool validate(LLNode* read_node, LLNode* write_node, u_int32_t rv){
    SegmentNode* read_segment = read_node -> corresponding_segment;
    size_t word = read_node -> word_num;
    if((read_segment->lock_bit)[word] == 1){
        // If hasn't been locked by the same transaction then false
        if(!getWriteNode(read_node->location, write_node))
            return false;
    }
    if((read_segment->lock_version_number)[word] > rv)
        return false;
    return true;
}

void writeToLocations(LLNode* write_node, size_t align, size_t wv){
    LLNode* cur = write_node;
    while(cur){
        memcpy(cur->location, cur->value, align);
        SegmentNode* segment = cur -> corresponding_segment;
        size_t word = cur -> word_num;
        segment->lock_version_number[word] = wv;
        atomic_store(&(segment->lock_bit[word]), 0); // not really needed
        cur = cur -> next;
    }
}