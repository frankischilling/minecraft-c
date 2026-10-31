#ifndef MC_WORLD_RUNTIME_RAYTRACE_INTERNAL_H
#define MC_WORLD_RUNTIME_RAYTRACE_INTERNAL_H

#include "collision/blocks_internal.h"
#include "construction_internal.h"
#include "mc/entity/hit_result.h"
#include "mc/world/runtime/raytrace.h"
#include "mc/world/storage/source_chunk_loader.h"
#include "runtime_internal.h"

typedef struct mc_world_raytrace_work {
    mc_world_raytrace_state state;
    mc_world_raytrace_error failure, cleanup;
    mc_world_collision_error collision;
    mc_world_source_chunk_error source_chunk;
    mc_world_storage_error world_error;
    mc_registry_error registry;
    mc_entity_error entity, entity_cleanup;
    mc_entity_vector_error vector, vector_cleanup;
    mc_entity_geometry_error geometry;
    mc_entity_bounds_error bounds;
    mc_entity_hit_error hit, hit_cleanup;
    mc_world_runtime *world;
    mc_world_runtime_classes *classes;
    mc_entity_classes *entities;
    bool world_held, start_held, end_held;
    mc_entity_vector *local[2], *generated[6], *point, *scratch, *pending_scratch;
    mc_entity_bounds *box;
    mc_entity_hit_result *candidate;
    mc_world_access_failure captures[16];
    size_t capture_count;
    mc_world_raytrace_site wrapper, block_parent;
} mc_world_raytrace_work;

struct mc_world_raytrace_call {
    mc_world_raytrace_work work;
};

mc_world_raytrace_site mc_world_raytrace_core_site(unsigned int pc);
mc_world_raytrace_status mc_world_raytrace_fail(mc_world_raytrace_work *, mc_world_raytrace_status,
                                                const char *, mc_world_raytrace_site, bool native);
mc_world_raytrace_status mc_world_raytrace_null(mc_world_raytrace_work *, mc_world_raytrace_site);
mc_world_raytrace_status mc_world_raytrace_collision_failed(mc_world_raytrace_work *,
                                                            mc_world_raytrace_site);
mc_world_raytrace_status mc_world_raytrace_geometry_failed(mc_world_raytrace_work *,
                                                           mc_world_raytrace_site);
mc_world_raytrace_status mc_world_raytrace_vector_failed(mc_world_raytrace_work *,
                                                         mc_world_raytrace_site);
mc_world_raytrace_status mc_world_raytrace_hit_failed(mc_world_raytrace_work *,
                                                      mc_world_raytrace_site);
mc_world_raytrace_status mc_world_raytrace_block_locals_retire(mc_world_raytrace_work *);
mc_world_raytrace_status mc_world_raytrace_block_can_collide_held(mc_world_raytrace_work *,
                                                                  const mc_block *, int32_t, bool,
                                                                  bool *);
mc_world_raytrace_status mc_world_raytrace_block_held(mc_world_raytrace_work *, const mc_block *,
                                                      int32_t, int32_t, int32_t, mc_entity_vector *,
                                                      mc_entity_vector *, mc_entity_hit_result **);

#endif
