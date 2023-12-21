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

    // We allocate the shared memory buffer such that its words are correctly
    // aligned.
    pthread_mutex_init(&(region->allocation_lock), NULL);
    SegmentNode* first_segment = initNode(region, size);
    region -> start_segment = first_segment -> segment_start;
    pthread_mutex_lock(&(region->allocation_lock));
    region -> alloced_segments = first_segment;
    pthread_mutex_unlock(&(region->allocation_lock));
    region->transactions_complete = 0; // this will be used to regularly remove dirty segments

    return (shared_t) region;
}

/** Destroy (i.e. clean-up + free) a given shared memory region.
 * @param shared Shared memory region to destroy, with no running transaction
**/
void tm_destroy(shared_t shared) {
    // TODO: tm_destroy(shared_t)
    MemoryRegion *region = (MemoryRegion *)shared;
    while(region -> alloced_segments){
        SegmentNode *nxt = region -> alloced_segments -> next;
        free(region -> alloced_segments);
        region -> alloced_segments = nxt;
    }
    pthread_mutex_destroy(&(region->allocation_lock));
    free(region);
}

/** [thread-safe] Return the start address of the first allocated segment in the shared memory region.
 * @param shared Shared memory region to query
 * @return Start address of the first allocated segment
**/
void* tm_start(shared_t shared) {
    // TODO: tm_start(shared_t)
    return ((MemoryRegion*) shared) -> start_segment;
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

    return (tx_t)t;
}

/** [thread-safe] End the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to end
 * @return Whether the whole transaction committed
**/
bool tm_end(shared_t shared, tx_t tx) {
    // printf("End\n");
    // TODO: tm_end(shared_t, tx_t)
    MemoryRegion* region = (MemoryRegion*) shared;
    Transaction* t = (Transaction*) tx;

    if(t->is_ro){
        cleanTransaction(region, t, true);
        return true;
    }
    
    if(!(t->write_addresses))
        return false; // cannot have a write transaction without any write addresses

    // Remove duplicates if required (current implementation duplicates are never added in the first place)
    SegmentNode* req_node = t -> write_addresses -> corresponding_segment;
    assert(req_node);
    // Acquire all the locks for the write set
    LLNode* write_node = t -> write_addresses;
    assert(write_node);
    if(!acquireLocks(write_node)){
        cleanTransaction(region, t, false);
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
                cleanTransaction(region, t, false);
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

    cleanTransaction(region, t, false);

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

    SegmentNode* req_node = nodeFromWordAddress(region, source_bytes);
    // if(!req_node)
    //     printf("Source Address: %p, added: %p\n", source_bytes, source_bytes+2072);
    if(!req_node)
        return false;
    assert(req_node);
    size_t diff = source_bytes - (char *)(req_node->segment_start);
    size_t start_word = diff / (region->align), num_words = size / (region->align);
    assert(req_node->lock_version_number);
    assert(req_node->lock_bit);
    if(t -> is_ro){
        for(size_t i = 0; i < num_words; i++){
            size_t cur_word = start_word + i;
            // sample lock bit and version number
            uint32_t v_before = (req_node->lock_version_number)[cur_word];
            memcpy(target_bytes, source_bytes, region->align);
            if(((req_node->lock_bit)[cur_word]) || ((req_node->lock_version_number)[cur_word] != v_before) || ((req_node->lock_version_number)[cur_word] > (t->rv)))
                return false;
            source_bytes += region->align;
            target_bytes += region->align;
        }
    }
    else{
        for(size_t i = 0; i < num_words; i++){
            size_t cur_word = start_word + i;
            
            // If we have already written at this address
            LLNode* writtenNode = getWriteNode(source_bytes, t->write_addresses); // returns NULL if this address does not exist

            // sample lock bit and version number
            uint32_t v_before = (req_node->lock_version_number)[cur_word];
            if(writtenNode){
                memcpy(target_bytes, writtenNode->value, region->align);
            }
            else{
                memcpy(target_bytes, source_bytes, region->align);
            }
            if(((req_node->lock_bit)[cur_word]) || ((req_node->lock_version_number)[cur_word] != v_before) || ((req_node->lock_version_number)[cur_word] > (t->rv))){
                return false;
            }
            // Create a new node for reading the value
            LLNode* newReadNode = (LLNode*) malloc(sizeof(LLNode));
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

    SegmentNode* req_node = nodeFromWordAddress(region, target_bytes);
    assert(req_node);


    size_t diff = target_bytes - (char *)(req_node->segment_start);
    size_t start_word = diff / (region->align), num_words = size / (region->align);
    for(size_t i = 0; i < num_words; i++){
        size_t cur_word = start_word + i;

        // clock_t start, end;
        // start = clock();
        LLNode* writtenNode = getWriteNode(target_bytes, t->write_addresses); // returns NULL if this address does not exist
        // end = clock();
        // printf("Time taken: %f\n", ((double) (end - start)) / CLOCKS_PER_SEC);
        // If we have already written at this address
        if(writtenNode)
            memcpy(writtenNode -> value, source_bytes, region->align);
        else{
            // Create a new node for writing the value
            LLNode* newWriteNode = (LLNode*) malloc(sizeof(LLNode));
            newWriteNode -> word_num = cur_word;
            newWriteNode -> location = target_bytes;
            void* buffer = (void*) malloc(sizeof(region->align));
            memcpy(buffer, source_bytes, region->align);
            newWriteNode -> value = buffer; // storing the value in this buffer
            newWriteNode -> corresponding_segment = req_node;
            newWriteNode -> next = t -> write_addresses;
            t -> write_addresses = newWriteNode;
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
    if(!s_node)
        return nomem_alloc;

    *target = s_node -> segment_start;
    // given an address, we have to do a linear search through the nodes of segments to find the right one
    printf("To allocate %p\n", s_node->segment_start);

    return success_alloc;
}

/** [thread-safe] Memory freeing in the given transaction.
 * @param shared Shared memory region associated with the transaction
 * @param tx     Transaction to use
 * @param target Address of the first byte of the previously allocated segment to deallocate
 * @return Whether the whole transaction can continue
**/
bool tm_free(shared_t shared, tx_t unused(tx), void* target) {
    // TODO: tm_free(shared_t, tx_t, void*)
    printf("To deallocate %p\n", target);

    MemoryRegion* region = (MemoryRegion*) shared;

    SegmentNode* req_node = getNode(region, target);
    assert(req_node);
    req_node->dirty = true;
    // printf("Deallocated %p\n", target);

    return true;
}
