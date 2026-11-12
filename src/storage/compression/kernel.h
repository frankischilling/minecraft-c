#ifndef MC_STORAGE_COMPRESSION_KERNEL_H
#define MC_STORAGE_COMPRESSION_KERNEL_H
#include "mc/storage/compression/compression.h"
#include <zlib.h>
/* The payload needs the alignment of zlib state and the supported desktop C
 * scalar types. Do not depend on max_align_t, absent from some Windows CRTs. */
typedef union compression_allocation compression_allocation;

union compression_allocation {
    struct {
        compression_allocation *previous;
        compression_allocation *next;
        size_t bytes;
    } bookkeeping;

    z_stream stream_alignment;
    long double scalar_alignment;
    void *pointer_alignment;
};

typedef struct compression_allocator {
    compression_allocation *allocations;
    size_t live_bytes;
    size_t limit;
    mc_compression_status failure;
} compression_allocator;

void compression_allocation_failure(compression_allocator *, mc_compression_status);
void *compression_allocate(compression_allocator *, size_t, size_t);
void compression_detach(compression_allocator *, compression_allocation *);
void compression_release(compression_allocator *, void *);
void compression_release_all(compression_allocator *);
voidpf compression_zalloc(voidpf, uInt, uInt);
void compression_zfree(voidpf, voidpf);
mc_compression_status compression_backend_status(int, const compression_allocator *);
#endif
