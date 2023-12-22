#pragma once

#include "data_structures.h"
#include "stdbool.h"

BloomFilter* initialiseBloomFilter(size_t size, size_t num_hashes) {
    BloomFilter* filter = (BloomFilter*)malloc(sizeof(BloomFilter));
    if (!filter) {
        return NULL;
    }

    filter->size = size;
    filter->bit_array = (uint8_t*)calloc((size + 7) / 8, sizeof(uint8_t)); // making sure we don't floor by /7
    if (!filter->bit_array) {
        free(filter);
        return NULL;
    }
    filter->num_hashes = num_hashes;

    return filter;
}

uint32_t hashFunction(const char* element, size_t seed) {
    uint32_t hash = 42 + seed;
    while (*element != '\0') {
        hash = (hash << 5) + hash + *element++;
    }
    return hash;
}

void addToBloomFilter(BloomFilter* filter, const char* element) {
    for (size_t i = 0; i < filter->num_hashes; ++i) {
        uint32_t hash = hashFunction(element, i) % filter->size;
        filter->bit_array[hash / 8] |= (1 << (hash % 8));
    }
}

bool isInBloomFilter(const BloomFilter* filter, const char* element) {
    for (size_t i = 0; i < filter->num_hashes; ++i) {
        uint32_t hash = hashFunction(element, i) % filter->size;
        if ((filter->bit_array[hash / 8] & (1 << (hash % 8))) == 0) {
            return false; // One of the bits is not set
        }
    }
    return true; // All bits are set
}

void freeBloomFilter(BloomFilter* filter) {
    assert(filter);
    assert(filter->bit_array);
    free(filter->bit_array);
    free(filter);
}