#ifndef MC_WORLD_RUNTIME_COLLISION_BLOCKS_INTERNAL_H
#define MC_WORLD_RUNTIME_COLLISION_BLOCKS_INTERNAL_H

#include "boxes_internal.h"

/* Caller holds/enters the live GAME World. These are the actual bounded
 * block/metadata reads, including provider acquisition, concrete normal/Blank
 * dispatch and reached source projections. Each attempt has fresh lower
 * diagnostics; outer World/Block PCs remain untouched. Failure preserves out. */
mc_world_storage_status mc_world_collision_block_value_held(mc_world_runtime *world, int32_t x,
                                                            int32_t y, int32_t z, bool metadata,
                                                            int32_t *out,
                                                            mc_world_collision_error *error);

/* Actual virtual e(World,III), distinct from multipart contribution. A genuine
 * nonnull result owns one pooled bounds hold; the caller releases it through
 * this World's canonical Entity classes. NULL is an actual completed source
 * result. The same live Block may mutate its singleton bounds. Missing concrete
 * descendants fail at their reached call, preserving source effects and out. */
mc_world_storage_status mc_world_collision_block_box_held(mc_world_runtime *world,
                                                          const mc_block *block, int32_t x,
                                                          int32_t y, int32_t z,
                                                          mc_entity_bounds **out,
                                                          mc_world_collision_error *error);

/* Actual MovingPiston a(World,IIIIFI), distinct from its Tile lookup in e.
 * The carrier's virtual getter supplies the same held box mutated in source
 * order. A late failure keeps coordinate/pool prefixes and preserves out.
 * ID0/self and genuine null shapes return before any Direction active use. */
mc_world_storage_status
mc_world_collision_piston_shifted_box_held(mc_world_runtime *world, const mc_block *moving,
                                           int32_t x, int32_t y, int32_t z, int32_t carried_id,
                                           float progress, int32_t facing, mc_entity_bounds **out,
                                           mc_world_collision_error *error);

/* Actual virtual contribution a(World,III,AABB,ArrayList). destination is the
 * captured source argument: no K field reload occurs here. Successful appends
 * retain their own pooled-box hold; each private factory hold is released on
 * every return. Multipart final shape resets occur only on source completion. */
mc_world_storage_status
mc_world_collision_block_contribute_held(mc_world_runtime *world, const mc_block *block, int32_t x,
                                         int32_t y, int32_t z, const mc_entity_bounds *query,
                                         mc_world_reference_list *destination,
                                         mc_world_collision_error *error);

#endif
