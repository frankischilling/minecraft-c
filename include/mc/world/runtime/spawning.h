#ifndef MC_WORLD_RUNTIME_SPAWNING_H
#define MC_WORLD_RUNTIME_SPAWNING_H
#include "mc/world/biome/registry.h"
#include "mc/world/runtime/blocks.h"

typedef struct mc_world_spawn_pick {
    const mc_biome_spawn *entry;
    int32_t total_weight, ticket, remainder;
    size_t index;
} mc_world_spawn_pick;

typedef struct mc_world_spawn_pick_error {
    mc_world_storage_status status;
    const char *operation, *exception_class;
    unsigned int bytecode, helper_bytecode;
    bool native_policy;
    mc_world_storage_error cause;
    mc_registry_error registry;
    mc_world_solid_support_error solid;
    mc_world_material_error material;
} mc_world_spawn_pick_error;

/* Exact cq.a(World,ZZ) fragment290..440: actual nullable kd.a(lk) List,
 * isEmpty, wrapped weight sum, one current World.r.nextInt(sum), first entry,
 * then subtraction in live List order until negative. A null/empty List is an
 * ordinary no-work result with entry=NULL and no Random call. No Class active
 * use, entity allocation, position draw, terrain read or World tick occurs.
 * Require exclusive client GAME/Registry owners and disjoint output/error.
 * Success publishes the borrowed SAME arena entry; reached failures preserve
 * earlier Random effects and leave out unchanged. */
mc_world_storage_status mc_world_runtime_pick_spawn_entry(mc_world_runtime *, const mc_biome *,
                                                          int32_t category,
                                                          mc_world_spawn_pick *out,
                                                          mc_world_spawn_pick_error *);
#endif
