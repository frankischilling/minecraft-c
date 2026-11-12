#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "file_overlap.h"
#include <stdint.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static void *overlap_calloc(size_t count, size_t size) {
    void *allocated = calloc(count, size);
    if (allocated != NULL && atomic_load_explicit(&stats_overlap_armed, memory_order_acquire) &&
        !thrd_equal(thrd_current(), stats_overlap_foreground)) {
        atomic_store_explicit(&stats_overlap_allocated, true, memory_order_release);
        if (!stats_overlap_wait(&stats_overlap_cache))
            atomic_store_explicit(&stats_overlap_failed, true, memory_order_release);
    }
    return allocated;
}

ptrdiff_t stats_overlap_native_read(int descriptor, uint8_t *bytes, size_t count) {
#ifdef _WIN32
    const ptrdiff_t actual = (ptrdiff_t)_read(descriptor, bytes, (unsigned int)count);
#else
    const ptrdiff_t actual = (ptrdiff_t)read(descriptor, bytes, count);
#endif
    if (atomic_load_explicit(&stats_overlap_armed, memory_order_acquire) &&
        !thrd_equal(thrd_current(), stats_overlap_foreground))
        atomic_store_explicit(&stats_overlap_read, true, memory_order_release);
    return actual;
}

#define calloc overlap_calloc
#define MC_JAVA_FILE_NATIVE_READ stats_overlap_native_read
#include "../../../src/world/storage/java_file_io.c"
#undef MC_JAVA_FILE_NATIVE_READ
#undef calloc
