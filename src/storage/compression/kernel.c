#include "kernel.h"
#include <stdlib.h>
#include <string.h>

void compression_allocation_failure(compression_allocator *allocator,
                                    mc_compression_status failure) {
    if (allocator->failure == MC_COMPRESSION_OK) {
        allocator->failure = failure;
    }
}

void *compression_allocate(compression_allocator *allocator, size_t count, size_t size) {
    if (size != 0 && count > SIZE_MAX / size) {
        compression_allocation_failure(allocator, MC_COMPRESSION_LIMIT);
        return NULL;
    }
    size_t payload_bytes = count * size;
    if (payload_bytes > SIZE_MAX - sizeof(compression_allocation)) {
        compression_allocation_failure(allocator, MC_COMPRESSION_LIMIT);
        return NULL;
    }
    size_t bytes = sizeof(compression_allocation) + payload_bytes;
    if (bytes > allocator->limit - allocator->live_bytes) {
        compression_allocation_failure(allocator, MC_COMPRESSION_LIMIT);
        return NULL;
    }
    compression_allocation *allocation = malloc(bytes);
    if (allocation == NULL) {
        compression_allocation_failure(allocator, MC_COMPRESSION_ALLOCATION_FAILED);
        return NULL;
    }
    allocation->bookkeeping.previous = NULL;
    allocation->bookkeeping.next = allocator->allocations;
    allocation->bookkeeping.bytes = bytes;
    if (allocator->allocations != NULL) {
        allocator->allocations->bookkeeping.previous = allocation;
    }
    allocator->allocations = allocation;
    allocator->live_bytes += bytes;
    void *payload = allocation + 1;
    memset(payload, 0, payload_bytes);
    return payload;
}

void compression_detach(compression_allocator *allocator, compression_allocation *allocation) {
    if (allocation->bookkeeping.previous != NULL) {
        allocation->bookkeeping.previous->bookkeeping.next = allocation->bookkeeping.next;
    } else {
        allocator->allocations = allocation->bookkeeping.next;
    }
    if (allocation->bookkeeping.next != NULL) {
        allocation->bookkeeping.next->bookkeeping.previous = allocation->bookkeeping.previous;
    }
    allocator->live_bytes -= allocation->bookkeeping.bytes;
    allocation->bookkeeping.previous = NULL;
    allocation->bookkeeping.next = NULL;
}

void compression_release(compression_allocator *allocator, void *payload) {
    if (payload != NULL) {
        compression_allocation *allocation = (compression_allocation *)payload - 1;
        compression_detach(allocator, allocation);
        free(allocation);
    }
}

void compression_release_all(compression_allocator *allocator) {
    while (allocator->allocations != NULL) {
        compression_allocation *allocation = allocator->allocations;
        compression_detach(allocator, allocation);
        free(allocation);
    }
}

voidpf compression_zalloc(voidpf opaque, uInt count, uInt size) {
    return compression_allocate(opaque, count, size);
}

void compression_zfree(voidpf opaque, voidpf payload) {
    compression_release(opaque, payload);
}

mc_compression_status compression_backend_status(int result,
                                                 const compression_allocator *allocator) {
    switch (result) {
        case Z_DATA_ERROR:
        case Z_NEED_DICT:
            return MC_COMPRESSION_INVALID_STREAM;
        case Z_MEM_ERROR:
            return allocator->failure == MC_COMPRESSION_OK ? MC_COMPRESSION_ALLOCATION_FAILED
                                                           : allocator->failure;
        default:
            return MC_COMPRESSION_BACKEND_ERROR;
    }
}
