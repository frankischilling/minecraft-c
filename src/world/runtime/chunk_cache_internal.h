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

/* The same source block read and native admission, with complete diagnostics
 * only on failure. Success leaves error untouched. Render callers inspect it
 * only after a failed status; the public accessor retains its success record. */
mc_client_chunk_cache_status mc_client_chunk_cache_block_failure(mc_client_chunk_cache *, int32_t,
                                                                 int32_t, int32_t, int32_t *,
                                                                 mc_client_chunk_cache_error *);

/* Same metadata admission and source read. Success leaves error untouched;
 * every failure supplies the complete public record without a second read. */
mc_client_chunk_cache_status mc_client_chunk_cache_metadata_failure(mc_client_chunk_cache *,
                                                                    int32_t, int32_t, int32_t,
                                                                    int32_t *,
                                                                    mc_client_chunk_cache_error *);

#endif
