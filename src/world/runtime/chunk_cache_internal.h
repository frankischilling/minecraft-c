#ifndef MC_WORLD_RUNTIME_CHUNK_CACHE_INTERNAL_H
#define MC_WORLD_RUNTIME_CHUNK_CACHE_INTERNAL_H

#include "mc/world/runtime/chunk_cache.h"

typedef struct mc_client_chunk_cache_occurrence {
    mc_chunk_state *state;
    mc_world_chunk *object;
    mc_world_store *store;
    bool leased;
} mc_client_chunk_cache_occurrence;

struct mc_client_chunk_cache {
    mc_client_chunk_cache_state state;
    mc_client_chunk_cache_error failure, cleanup;
    mc_chunk_state ***rows;
    mc_chunk_state **cells;
    mc_client_chunk_cache_occurrence *occurrences;
    size_t cell_count;
    mc_client_chunk_cache_limits limits;
    bool world_held;
};

#endif
