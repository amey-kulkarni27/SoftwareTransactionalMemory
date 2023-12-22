#pragma once

#include <stdio.h>
#include <stdbool.h>

#include "data_structures.h"
#include "readers_writer.h"
#include "bloom_filter.h"
#include "macros.h"

// A bit unsure about this implementation
size_t segFromWordAddress(char* address_search){
    uint64_t address_num = (uint64_t)address_search;
    size_t seg_num = (size_t)(address_num>>48);
    return seg_num;
}


LLNode* getWriteNode(void* source_address, LLNode* cur_node){
    while(cur_node){
        if(cur_node->location == source_address)
            break;
        cur_node = cur_node->next;
    }
    return cur_node;
}

void releaseLocks(LLNode* write_node, LLNode* until){
    // we release a prefix of locks up to until
    // until can be NULL, which means we have to release all locks
    LLNode* remove_node = write_node;
    while(remove_node != until){
        assert(remove_node);
        size_t word = remove_node -> word_num;
        SegmentNode* segment = remove_node -> corresponding_segment;
        assert(segment);
        assert(segment->lock_bit);
        atomic_store(&(segment->lock_bit[word]), 0);
        remove_node = remove_node -> next;
    }
}

bool acquireLocks(LLNode* write_node){
    LLNode* cur = write_node;
    bool success = true;
    while(cur){
        SegmentNode* segment = cur -> corresponding_segment;
        size_t word = cur -> word_num;
        bool expected = false;
        if(!atomic_compare_exchange_strong(&(segment->lock_bit[word]), &expected, true)){
            success = false;
            break;
        }
        cur = cur -> next;
    }
    if(!success){
        releaseLocks(write_node, cur);
    }
    return success;
}

void cleanAddresses(LLNode* node, bool is_write){
    LLNode* cur = node;
    while(cur){
        LLNode* nxt = cur -> next;
        if(is_write)
            free(cur->value);
        free(cur);
        cur = nxt;
    }
}

void cleanSegments(MemoryRegion* region){
    for(size_t i = 1; i < region->num_allocs; i++){
        if(region->segments_list[i]){
            if(region->segments_list[i]->lock_bit)
                free(region->segments_list[i]->lock_bit);
            if(region->segments_list[i]->lock_version_number)
                free(region->segments_list[i]->lock_version_number);
            if(region->segments_list[i]->segment_start)
                free(region->segments_list[i]->segment_start);
            free(region->segments_list[i]);
        }
    }
    free(region->segments_list);
}

void cleanTransaction(Transaction* t){
    cleanAddresses(t->read_addresses, false);
    cleanAddresses(t->write_addresses, true);
    freeBloomFilter(t->filter);
    free(t);
}

bool validate(LLNode* read_node, LLNode* write_node, u_int32_t rv){
    SegmentNode* read_segment = read_node -> corresponding_segment;
    assert(read_segment);
    size_t word = read_node -> word_num;
    if((read_segment->lock_version_number)[word] > rv)
        return false;
    if((read_segment->lock_bit)[word] == 1){
        // If hasn't been locked by the same transaction then false
        if(!getWriteNode(read_node->location, write_node))
            return false;
    }
    
    return true;
}

void writeToLocations(LLNode* write_node, size_t align, size_t wv){
    LLNode* cur = write_node;
    while(cur){
        memcpy(cur->location, cur->value, align);
        SegmentNode* segment = cur -> corresponding_segment;
        size_t word = cur -> word_num;
        segment->lock_version_number[word] = wv;
        atomic_store(&(segment->lock_bit[word]), 0); // not really needed to be done atomically
        cur = cur -> next;
    }
}

SegmentNode* initNode(MemoryRegion* region, size_t size){

    SegmentNode* s_node = (SegmentNode*) malloc(sizeof(SegmentNode));
    if(unlikely(!s_node))
        return NULL;
    

    s_node -> prev = NULL;
    s_node -> next = NULL;

    s_node -> size = size;
    if(unlikely(posix_memalign(&(s_node->segment_start), region->align, size) != 0)){
        free(s_node);
        return NULL;
    }
    memset(s_node->segment_start, 0, size); // initialising the segment with 0s
    // printf("Node Start Address: %p, size: %zu\n", s_node->segment_start, size);
    s_node -> num_words = size / (region->align);

    s_node -> lock_bit = (atomic_bool*) malloc((s_node->num_words) * sizeof(atomic_bool));
    if(unlikely(!(s_node->lock_bit))){
        free(s_node);
        free(s_node->segment_start);
        return NULL;
    }
    memset(s_node->lock_bit, 0, (s_node->num_words) * sizeof(atomic_bool)); // lock bits initially set to 0
    
    s_node -> lock_version_number = (uint32_t*) malloc((s_node->num_words) * sizeof(uint32_t));
    if(unlikely(!(s_node->lock_version_number))){
        free(s_node);
        free(s_node->segment_start);
        free(s_node->lock_bit);
        return NULL;
    }
    memset(s_node->lock_version_number, 0, (s_node->num_words) * sizeof(uint32_t)); // version is initially set to 0

    return s_node;
}
