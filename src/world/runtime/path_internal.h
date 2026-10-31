#ifndef MC_WORLD_RUNTIME_PATH_INTERNAL_H
#define MC_WORLD_RUNTIME_PATH_INTERNAL_H

#include "../path/finder_internal.h"
#include "mc/world/runtime/path.h"
#include "mc/world/storage/source_chunk_loader.h"

struct mc_world_path_call {
    mc_world_path_call_state state;
    mc_world_path_error failure, cleanup;
    mc_world_path_limits limits;
    mc_path_budget budget;
    mc_path_finder *finder;
    mc_client_chunk_cache *cache;
    mc_client_chunk_cache_error cache_error, cache_cleanup;
    mc_path_finder_error finder_error;
    mc_path_error path_error;
    mc_world_source_chunk_error source_chunk;
    mc_world_storage_error world_error;
    mc_registry_error registry_error, registry_cleanup;
    mc_entity_error entity_error;
    mc_math_class_error math_error, math_cleanup;
    mc_world_access_failure receipts[16];
    size_t receipt_count, cache_bytes;
    mc_registry_classes *registry;
    mc_entity_classes *entities;
    mc_math_classes *math;
    bool world_held, source_held, target_held, registry_held, math_held;
};

#endif
