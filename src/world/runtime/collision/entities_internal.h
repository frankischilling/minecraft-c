#ifndef MC_WORLD_RUNTIME_COLLISION_ENTITIES_INTERNAL_H
#define MC_WORLD_RUNTIME_COLLISION_ENTITIES_INTERNAL_H

#include "../runtime_internal.h"
#include "mc/world/runtime/collision/entities.h"

/* Native ownership of the captured constructor R254 descriptor. Independent
 * of current source R. Validation never adopts or frees unknown projections.
 * release is terminal, infallible after complete quiescent validation and
 * retains the descriptor for World final free; it frees its backing/holds. */
bool mc_world_nearby_owned_valid(const mc_world_runtime *world);
void mc_world_nearby_owned_release(mc_world_runtime *world);

/* Reached real provider getter shared by the two distinct Entity queries.
 * Caller holds/enters World; preserve its outer query PC and complete fresh
 * per-attempt acquisition descendants. No presence query or List use here. */
bool mc_world_entity_query_acquire_held(mc_world_runtime *world, int32_t x, int32_t z,
                                        mc_chunk_state **out, mc_world_nearby_error *error);

/* Actual virtual gather. Caller holds/enters this World and keeps state live
 * through this synchronous call. Blank dispatch precedes every normal-header
 * and nullable argument access. Normal headers are resolved by actual state
 * identity and leased through the call. Destination may be NULL until add131;
 * nonnull destinations must be this World's actual captured R when reached.
 * No clear occurs here; normal gather appends and Blank is literal return0. */
mc_world_storage_status mc_world_nearby_gather_held(mc_world_runtime *world, mc_chunk_state *state,
                                                    mc_entity *excluded,
                                                    const mc_entity_bounds *bounds,
                                                    mc_world_reference_list *destination,
                                                    mc_world_nearby_error *error);

#endif
