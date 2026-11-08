#include "player_ally_retire_adapter.h"

#include <stdlib.h>

static size_t failed_allocations;

static void *ally_retire_refused_scratch(size_t count, size_t bytes) {
    (void)count;
    (void)bytes;
    ++failed_allocations;
    return NULL;
}

/* Translation-unit-only fault injection into the genuine optional scratch
 * request. The entire actual retirement still runs through the same core;
 * ordinary runtime binaries have no test switch or alternate allocator. */
#define calloc ally_retire_refused_scratch
#include "../../src/entity/player/ally_alert_occurrences.c"
#undef calloc

const char *mc_benchmark_ally_retire_adapter(void) {
    return "fallback";
}

size_t mc_benchmark_ally_retire_failed_allocations(void) {
    return failed_allocations;
}
