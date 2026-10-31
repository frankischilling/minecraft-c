#ifndef MC_WORLD_RUNTIME_COLLISION_SPAWNING_H
#define MC_WORLD_RUNTIME_COLLISION_SPAWNING_H

#include "mc/world/runtime/collision/entities.h"

typedef struct mc_world_spawn_collision_error {
    mc_world_storage_status status;
    const char *operation, *exception_class;
    unsigned int bytecode;
    bool native_policy, source_exception;
    size_t index;
    mc_world_nearby_error nearby;
} mc_world_spawn_collision_error;

/* Actual client fd.a(eq)Z. Query the genuine nearby List with NULL exclusion,
 * then test each occurrence's be before aF. The returned bool is independent
 * of native status. Every successful List lease is released, including false
 * and failure paths; reached List/Chunk effects are never rolled back.
 * Keep the GAME World, bounds owner and output/error live, exclusive and
 * disjoint. RAW/failed World prefixes are admitted; no Entity READY gate is
 * inserted. Failure preserves out. */
mc_world_storage_status mc_world_runtime_no_entity_collision(mc_world_runtime *,
                                                             const mc_entity_bounds *, bool *,
                                                             mc_world_spawn_collision_error *);

#endif
