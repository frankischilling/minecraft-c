#ifndef MC_WORLD_RUNTIME_COLLISION_CLASS_ENTITIES_INTERNAL_H
#define MC_WORLD_RUNTIME_COLLISION_CLASS_ENTITIES_INTERNAL_H

#include "../runtime_internal.h"
#include "mc/world/runtime/collision/class_entities.h"

/* Actual virtual Chunk.a(Class,AABB,List). Caller holds/enters the same World
 * and keeps the actual state live through this call. Blank dispatch is literal
 * return0 before every normal-header or nullable argument access, including a
 * NULL World. Normal dispatch resolves and leases that exact store-owned Chunk.
 * target and bounds retain their source null-use order. destination is NULL or
 * a live fresh result owner from this World, checked only at reached add137;
 * no clear occurs. Partial appends remain in an existing supplied result on
 * failure. Unknown List descriptors are never adopted as owning storage. */
mc_world_storage_status mc_world_class_entities_gather_held(mc_world_runtime *world,
                                                            mc_chunk_state *state,
                                                            const mc_entity_class_literal *target,
                                                            const mc_entity_bounds *bounds,
                                                            mc_world_class_entity_list *destination,
                                                            mc_world_nearby_error *error);

#endif
