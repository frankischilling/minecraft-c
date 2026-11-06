#ifndef MC_WORLD_RUNTIME_COLLISION_BOXES_INTERNAL_H
#define MC_WORLD_RUNTIME_COLLISION_BOXES_INTERNAL_H

#include "../runtime_internal.h"
#include "mc/world/runtime/collision/boxes.h"

bool mc_world_collision_owned_valid(const mc_world_runtime *world);
void mc_world_collision_owned_release(mc_world_runtime *world);

/* Caller holds/enters this World. Append retains an independent pooled-box
 * occurrence; caller keeps and later releases its original factory hold.
 * destination is the actual captured K descriptor, possibly null until this
 * reached add. No source field reload or preflight of later work occurs. */
mc_world_storage_status mc_world_collision_append_pool_held(mc_world_runtime *world,
                                                            mc_world_reference_list *destination,
                                                            mc_entity_bounds *bounds,
                                                            mc_world_collision_error *error);

/* A genuine permanent bounds reference keeps the actual owning Entity live.
 * The current source bounds field must name this exact box. Caller retains
 * its own argument hold; append publishes a separate occurrence hold. */
mc_world_storage_status mc_world_collision_append_entity_held(mc_world_runtime *world,
                                                              mc_world_reference_list *destination,
                                                              mc_entity *entity,
                                                              mc_entity_bounds *bounds,
                                                              mc_world_collision_error *error);

#endif
