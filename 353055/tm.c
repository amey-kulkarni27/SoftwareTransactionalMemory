/**
 * @file   tm.c
 * @author [...]
 *
 * @section LICENSE
 *
 * [...]
 *
 * @section DESCRIPTION
 *
 * Implementation of your own transaction manager.
 * You can completely rewrite this file (and create more files) as you wish.
 * Only the interface (i.e. exported symbols and semantic) must be preserved.
**/

// Requested features
#define _GNU_SOURCE
#define _POSIX_C_SOURCE   200809L
#ifdef __STDC_NO_ATOMICS__
    #error Current C11 compiler does not support atomic operations
#endif

// External headers
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>

// Internal headers
#include <tm.h>
#include "data_structures.h"
#include "helper_functions.h"
#include "readers_writer.h"
#include "bloom_filter.h"

#include "macros.h"

/** Create (i.e. allocate + init) a new shared memory region, with one first non-free-able allocated segment of the requested size and alignment.
 * @param size  Size of the first shared segment of memory to allocate (in bytes), must be a positive multiple of the alignment
 * @param align Alignment (in bytes, must be a power of 2) that the shared memory region must support
 * @return Opaque shared memory region handle, 'invalid_shared' on failure
**/
shared_t tm_create(size_t size, size_t align) {
    // TODO: tm_create(size_t, size_t)
    MemoryRegion* region = (MemoryRegion*) malloc(sizeof(MemoryRegion));
    if (unlikely(!region)) {
        return invalid_shared;
    }
    region -> global_clock = 0;
    region -> size = size;
    region -> align = align;
    region -> num_allocs = 1;
    region -> max_size = 1000;

    // We allocate the shared memory buffer such that its words are correctly aligned

    SegmentNode* first_segment = initNode(region, size);
    if(!first_segment)
        return invalid_shared;
    // initRWLock(&region->allocation_lock);
    pthread_mutex_init(&(region->allocation_lock), NULL);
    region -> segments_list = (SegmentNode**)malloc(region->max_size * sizeof(SegmentNode*));
    if(unlikely(!(region->segments_list))){
        free(region);
        return invalid_shared;
    }
    region -> segments_list[region->num_allocs] = first_segment;
    region -> num_allocs++;
    region -> start_segment = first_segment -> segment_start;

    return (shared_t) region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) {
    // printf("Destroy\n");
    // TODO: tm_destroy(shared_t)
    MemoryRegion *region = (MemoryRegion *)shared;
    cleanSegments(region);
    pthread_mutex_destroy(&(region->allocation_lock));
    // destroyRWLock(&region->allocation_lock);
    free(region);
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t unused(shared)) {
    // TODO: tm_start(shared_t)
    return (void*)(1ll<<48);
}

/** [thread-safe] Return the size (in bytes) of the first allocated segment of the shared memory region.
 * @param shared Shared memory region to query
 * @return First allocated segment size
**/
size_t tm_size(shared_t shared) {
    // TODO: tm_size(shared_t)
    return ((MemoryRegion*) shared) -> size;
}

/** [thread-safe] Return the alignment (in bytes) of the memory accesses on the given shared memory region.
 * @param shared Shared memory region to query
 * @return Alignment used globally
**/
size_t tm_align(shared_t shared) {
    // TODO: tm_align(shared_t)
    return ((MemoryRegion*) shared) -> align;
}

/** [thread-safe] Begin a new transaction on the given shared memory region.
 * @param shared Shared memory region to start a transaction on
 * @param is_ro  Whether the transaction is read-only
 * @return Opaque transaction ID, 'invalid_tx' on failure
**/
tx_t tm_begin(shared_t shared, bool is_ro) {
    // TODO: tm_begin(shared_t)

    MemoryRegion* region = (MemoryRegion*) shared;

    // Initialising the transaction
    Transaction* t = (Transaction*) malloc(sizeof(Transaction));
    if(unlikely(!t))
        return invalid_tx;
    t -> region = region;
    t -> is_ro = is_ro;
    t -> rv = region -> global_clock; // Sampling the global clock for the read phase
    // the following will update the number of reads and writes as they see them
    t -> num_reads = 0;
    t -> num_writes = 0;
    // the numbers will denote the number of items in the respective linked list, not the actual read/writes of the transaction seen so far
    t -> read_addresses = NULL;
    t -> write_addresses = NULL;
    t -> filter = initialiseBloomFilter(200, 4);

    return (tx_t)t;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t shared, tx_t tx) {
    // TODO: tm_end(shared_t, tx_t)
    MemoryRegion* region = (MemoryRegion*) shared;
    Transaction* t = (Transaction*) tx;

    if(t->is_ro){
        cleanTransaction(t);
        return true;
    }
    
    if(!(t->write_addresses)){
        cleanTransaction(t);
        return true; // cannot have a write transaction without any write addresses
    }

    // Remove duplicates if required (current implementation duplicates are never added in the first place)
    SegmentNode* req_node = t -> write_addresses -> corresponding_segment;
    assert(req_node);
    // Acquire all the locks for the write set
    LLNode* write_node = t -> write_addresses;
    assert(write_node);
    if(!acquireLocks(write_node)){
        cleanTransaction(t);
        return false;
    }


    // Increment and store global clock
    long wv = atomic_fetch_add(&(region->global_clock), 1);
    wv++;

    // Validate the read set
    // if rv + 1 = wv, we're good
    if(wv == (t->rv) + 1);
    else{
        // go to each read memory location, check if the lock is either free or taken by the current transaction and its version number is ≤ rv
        LLNode* read_node = t -> read_addresses;
        while(read_node){
            if(!validate(read_node, t->write_addresses, t->rv)){
                // release locks
                releaseLocks(write_node, NULL); // all locks have been acquired if we have reached the validate stage
                cleanTransaction(t);
                return false;
            }
            read_node = read_node -> next;
        }
    }

    // Commit
    // Set value at shared location to current value
    // Update the version to wv
    // Clear the lock bit
    writeToLocations(write_node, region->align, wv);

    cleanTransaction(t);

    return true;
}

/** [thread-safe] Read operation in the given transaction, source in the shared region and target in a private region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in the shared region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in a private region)
 * @return Whether the whole transaction can continue
**/
bool tm_read(shared_t shared, tx_t tx, void const* source, size_t size, void* target) {
    // printf("Read\n");
    // TODO: tm_read(shared_t, tx_t, void const*, size_t, void*)

    MemoryRegion* region = (MemoryRegion*) shared;
    Transaction* t = (Transaction*) tx;

    // Convert to char* pointers, so that the difference of the pointers represents the bytes in between
    char* source_bytes = (char*)source;
    char* target_bytes = (char*)target;
    assert(source == source_bytes);

    size_t s_no = segFromWordAddress(source_bytes);
    SegmentNode* req_node = region->segments_list[s_no];
    assert(req_node);

    uint64_t sno48 = (uint64_t)s_no;
    size_t diff = source_bytes - (char *)(sno48<<48);
    source_bytes = (char*)req_node->segment_start + diff;
    // if(!req_node)
    //     printf("Source Address: %p, added: %p\n", source_bytes, source_bytes+2072);
    // assert(req_node);
    size_t start_word = diff / (region->align), num_words = size / (region->align);
    assert(req_node->lock_version_number);
    assert(req_node->lock_bit);
    if(t -> is_ro){
        for(size_t i = 0; i < num_words; i++){
            size_t cur_word = start_word + i;
            // sample lock bit and version number
            uint32_t v_before = (req_node->lock_version_number)[cur_word];
            memcpy(target_bytes, source_bytes, region->align);
            if(((req_node->lock_bit)[cur_word]) || ((req_node->lock_version_number)[cur_word] != v_before) || ((req_node->lock_version_number)[cur_word] > (t->rv))){
                cleanTransaction(t);
                return false;
            }
            source_bytes += region->align;
            target_bytes += region->align;
        }
    }
    else{
        for(size_t i = 0; i < num_words; i++){
            size_t cur_word = start_word + i;
            
            // If we have already written at this address
            uint32_t v_before = (req_node->lock_version_number)[cur_word];
            bool seen = isInBloomFilter(t->filter, source_bytes);
            // bool seen = true;
            if(!seen){
                memcpy(target_bytes, source_bytes, region->align);
            }
            else{
                LLNode* writtenNode = getWriteNode(source_bytes, t->write_addresses); // returns NULL if this address does not exist

                // sample lock bit and version number
                if(writtenNode){
                    memcpy(target_bytes, writtenNode->value, region->align);
                }
                else{
                    memcpy(target_bytes, source_bytes, region->align);
                }
            }
            
            if(((req_node->lock_bit)[cur_word]) || ((req_node->lock_version_number)[cur_word] != v_before) || ((req_node->lock_version_number)[cur_word] > (t->rv))){
                cleanTransaction(t);
                return false;
            }
            // Create a new node for reading the value
            LLNode* newReadNode = (LLNode*) malloc(sizeof(LLNode));
            if(unlikely(!newReadNode)){
                cleanTransaction(t);
                return false;
            }
            newReadNode -> word_num = cur_word;
            newReadNode -> location = source_bytes;
            newReadNode -> corresponding_segment = req_node;
            newReadNode -> next = t -> read_addresses;
            t -> read_addresses = newReadNode;

            source_bytes += region->align;
            target_bytes += region->align;
            
        }
    }

    return true;
}

/** [thread-safe] Write operation in the given transaction, source in a private region and target in the shared region.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param source Source start address (in a private region)
 * @param size   Length to copy (in bytes), must be a positive multiple of the alignment
 * @param target Target start address (in the shared region)
 * @return Whether the whole transaction can continue
**/
bool tm_write(shared_t shared, tx_t tx, void const* source, size_t size, void* target) {
    // printf("Write\n");
    // TODO: tm_write(shared_t, tx_t, void const*, size_t, void*)

    MemoryRegion* region = (MemoryRegion*) shared;
    Transaction* t = (Transaction*) tx;
    
    // keep inserting write addresses and values to the start
    // remove duplicates in end, keep the most recent (closer to the start)

    // for now, search if the same address already exists before adding every element

    const char* source_bytes = (const char*)source;
    char* target_bytes = (char*)target;

    size_t s_no = segFromWordAddress(target_bytes);
    SegmentNode* req_node = region->segments_list[s_no];
    assert(req_node);

    uint64_t sno48 = (uint64_t)s_no;
    size_t diff = target_bytes - (char *)(sno48<<48);
    target_bytes = (char*)req_node->segment_start + diff;
    size_t start_word = diff / (region->align), num_words = size / (region->align);
    for(size_t i = 0; i < num_words; i++){
        size_t cur_word = start_word + i;

        bool seen = isInBloomFilter(t->filter, target_bytes);
        // bool seen = true;
        if(!seen){
            // Create a new node for writing the value
            LLNode* newWriteNode = (LLNode*) malloc(sizeof(LLNode));
            if(unlikely(!newWriteNode)){
                cleanTransaction(t);
                return false;
            }
            newWriteNode -> word_num = cur_word;
            newWriteNode -> location = target_bytes;
            void* buffer = (void*) malloc(sizeof(region->align));
            if(unlikely(!buffer)){
                free(newWriteNode);
                cleanTransaction(t);
                return false;
            }
            memcpy(buffer, source_bytes, region->align);
            newWriteNode -> value = buffer; // storing the value in this buffer
            newWriteNode -> corresponding_segment = req_node;
            newWriteNode -> next = t -> write_addresses;
            t -> write_addresses = newWriteNode;
            addToBloomFilter(t->filter, target_bytes);
        }
        else{
            LLNode* writtenNode = getWriteNode(target_bytes, t->write_addresses); // returns NULL if this address does not exist
            // If we have already written at this address
            if(writtenNode)
                memcpy(writtenNode -> value, source_bytes, region->align);
            else{
                // Create a new node for writing the value
                LLNode* newWriteNode = (LLNode*) malloc(sizeof(LLNode));
                if(unlikely(!newWriteNode)){
                    cleanTransaction(t);
                    return false;
                }
                newWriteNode -> word_num = cur_word;
                newWriteNode -> location = target_bytes;
                void* buffer = (void*) malloc(sizeof(region->align));
                if(unlikely(!buffer)){
                    free(newWriteNode);
                    cleanTransaction(t);
                    return false;
                }
                memcpy(buffer, source_bytes, region->align);
                newWriteNode -> value = buffer; // storing the value in this buffer
                newWriteNode -> corresponding_segment = req_node;
                newWriteNode -> next = t -> write_addresses;
                t -> write_addresses = newWriteNode;
                addToBloomFilter(t->filter, target_bytes);
        }
        }

        source_bytes += region->align;
        target_bytes += region->align;
    }

    return true;
}

/** [thread-safe] Memory allocation in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param size   Allocation requested size (in bytes), must be a positive multiple of the alignment
 * @param target Pointer in private memory receiving the address of the first byte of the newly allocated, aligned segment
 * @return Whether the whole transaction can continue (success/nomem), or not (abort_alloc)
**/
alloc_t tm_alloc(shared_t shared, tx_t unused(tx), size_t size, void** target) {
    // TODO: tm_alloc(shared_t, tx_t, size_t, void**)

    MemoryRegion* region = (MemoryRegion*) shared;

    SegmentNode* s_node = initNode(region, size);
    if(unlikely(!s_node))
        return nomem_alloc;

    pthread_mutex_lock(&(region->allocation_lock));
    // since we are told that no segment can be larger than 48 bits, we can just let the last 48 bits represent objects inside a segment
    uint64_t s_no = (uint64_t)(region->num_allocs);
    if(region->max_size == region->num_allocs){
        region->max_size *= 2;
        region->segments_list = (SegmentNode**)realloc(region->segments_list, region->max_size * sizeof(SegmentNode*));
        if(unlikely(!region->segments_list))
            return nomem_alloc;
    }
    region->segments_list[s_no] = s_node;
    region->num_allocs++;
    *target = (void*)(s_no<<48); // we can get the segment number by looking at the largest 16 bits
    // it is assumed that there are going to be ≤ 2^16 total allocs
    pthread_mutex_unlock(&(region->allocation_lock));

    return success_alloc;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t unused(shared), tx_t unused(tx), void* unused(target)) {
    // TODO: tm_free(shared_t, tx_t, void*)
    // printf("To deallocate %p\n", target);

    return true;
}
