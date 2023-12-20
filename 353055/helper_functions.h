#pragma once

#include <stdio.h>
#include <stdbool.h>

#include "data_structures.h"
#include "macros.h"

SegmentNode* getNode(MemoryRegion* region, void *segment_start){
    SegmentNode* cur_node = region -> alloced_segments;
    while(cur_node != NULL){
        if(cur_node->segment_start == segment_start)
            break;
        cur_node = cur_node -> next;
    }
    return cur_node;
}

// A bit unsure about this implementation
SegmentNode* nodeFromWordAddress(MemoryRegion* region, char* address_search){
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

SegmentNode* init_node(MemoryRegion* region, size_t size){

    SegmentNode* s_node = (SegmentNode*) malloc(sizeof(SegmentNode));
    if(unlikely(!s_node))
        return NULL;
    

    s_node -> prev = NULL;
    // Since region is shared by all segments
    pthread_mutex_lock(&(region->allocation_lock));
    s_node -> next = region -> alloced_segments;
    if(s_node -> next)
        s_node -> next -> prev = s_node;
    region -> alloced_segments = s_node;
    pthread_mutex_unlock(&(region->allocation_lock));

    s_node -> size = size;
    if(posix_memalign(&(s_node->segment_start), region->align, size) != 0)
        return NULL;
    memset(s_node->segment_start, 0, size); // initialising the segment with 0s
    s_node -> num_words = size / (region->align);

    s_node -> lock_bit = (atomic_bool*) malloc((s_node->num_words) * sizeof(atomic_bool));
    if(!(s_node->lock_bit))
        return NULL;
    memset(s_node->lock_bit, 0, (s_node->num_words) * sizeof(atomic_bool)); // lock bits initially set to 0
    
    s_node -> lock_version_number = (uint32_t*) malloc((s_node->num_words) * sizeof(uint32_t));
    if(!(s_node->lock_version_number))
        return NULL;
    memset(s_node->lock_version_number, 0, (s_node->num_words) * sizeof(uint32_t)); // version is initially set to 0

    return s_node;
}