#ifndef MC_WORLD_RUNTIME_PROVIDER_STATE_INTERNAL_H
#define MC_WORLD_RUNTIME_PROVIDER_STATE_INTERNAL_H

#include "provider_internal.h"

/* The one actual provider owner, shared only by its concrete method bodies. */
struct mc_world_provider {
    mc_world_runtime *world;
    mc_chunk_acquire_policy policy;
    bool force_load;
    mc_world_store *store;
    mc_world_dimension_biome *biome;
    /* Native supplier hold for the current actual v's Registry arena. */
    mc_world_dimension_biome *v_owner;
    mc_biome_manager *manager;
    mc_normal_generator generator;
    mc_world_provider_generation_fields generation;
    /* Actual nullable yf/pk.w. Each field owns one temperature-array hold. */
    mc_temperature_array *w;
    mc_temperature_owner *temperature_allocations;
    mc_temperature_limits temperature_limits;
    int32_t constructor_grid[MC_WORLD_PROVIDER_GRID_WIDTH][MC_WORLD_PROVIDER_GRID_WIDTH];
    mc_chunk_pending pending;
    mc_java_int_set *pending_history;
    mc_java_int_set_limits history_limits;
    int32_t *pending_keys;
    mc_chunk_cache cache;
    mc_chunk_cache_entry *entries;
    mc_chunk_state **insertions;
    mc_chunk_data *fallback_data;
    uint8_t *fallback_blocks;
    size_t max_chunks, direct_bytes, max_direct_bytes;
};

#endif
