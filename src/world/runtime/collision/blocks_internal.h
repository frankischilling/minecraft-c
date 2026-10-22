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

/* Same complete virtual getter with its actual defining code owner separate
 * from the captured, nullable World argument. Caller keeps code_classes and
 * Block live/exclusive, and keeps a nonnull source World physically alive and
 * exclusive. A reached metadata call enters/leaves that actual World lazily;
 * the public Tile getter owns its entry. No outer World entry is required.
 * The Block must belong to code_classes->registry. Side, Registry,
 * FP admission and pooled bounds use this code owner; World-independent null
 * and pooled shapes do not inspect world. Metadata/Tile-dependent getters use
 * only the supplied World when reached. A nonnull result owns one bounds hold
 * in code_classes->entities, which the caller releases through that same owner.
 * No Entity binding is required until an actual pooled factory is reached. */
mc_world_storage_status mc_world_collision_block_box_in_classes_held(
    mc_world_runtime_classes *code_classes, mc_world_runtime *world, const mc_block *block,
    int32_t x, int32_t y, int32_t z, mc_entity_bounds **out, mc_world_collision_error *error);

/* Actual virtual a(BlockAccess,III), including all paired Block overrides and
 * uu/na's inherited empty body. This mutates the real singleton's bs..bx; it
 * neither obtains nor resets pooled bounds. The World wrapper has the same
 * entered GAME World ownership contract as block_box_held. Its explicit-code
 * variant admits a nullable World until a source Tile/ID/metadata getter is
 * reached, under the same lazy-entry and lifetime rules as block_box above.
 * MovingPiston retains the actual typed lookup result and captured carrier,
 * compares carrier object identity, calls its complete virtual state-shape,
 * then reads progress/extending/facing. Six independent Direction reads write
 * bs,bt,bu,bv,bw,bx in that order. Failure keeps all completed carrier/Moving
 * mutations, acquisition effects and initializer publications. piston_bytecode
 * identifies the enclosing MovingPiston state-shape call; block_class/bytecode
 * identify the deepest reached operation. No Entity binding is required. */
mc_world_storage_status mc_world_collision_block_state_shape_held(mc_world_runtime *world,
                                                                  const mc_block *block, int32_t x,
                                                                  int32_t y, int32_t z,
                                                                  mc_world_collision_error *error);
mc_world_storage_status mc_world_collision_block_state_shape_in_classes_held(
    mc_world_runtime_classes *code_classes, mc_world_runtime *world, const mc_block *block,
    int32_t x, int32_t y, int32_t z, mc_world_collision_error *error);

/* Actual MovingPiston a(World,IIIIFI), distinct from its Tile lookup in e.
 * The carrier's virtual getter supplies the same held box mutated in source
 * order. A late failure keeps coordinate/pool prefixes and preserves out.
 * ID0/self and genuine null shapes return before any Direction active use. */
mc_world_storage_status
mc_world_collision_piston_shifted_box_held(mc_world_runtime *world, const mc_block *moving,
                                           int32_t x, int32_t y, int32_t z, int32_t carried_id,
                                           float progress, int32_t facing, mc_entity_bounds **out,
                                           mc_world_collision_error *error);

/* Same shifted helper using explicit defining classes and the unchanged
 * nullable source World argument. Ownership and reached getter rules match
 * block_box_in_classes_held above. ID0/self return before Registry/Direction
 * active use or World/pool operations; a genuine null getter skips Direction.
 * Registry-only failure reports its complete frame without touching World
 * diagnostics. Existing World-only wrappers retain their legacy admission and
 * World-diagnostic behavior. All six independent Direction reads and their
 * coordinate/pool prefix remain in original order. */
mc_world_storage_status mc_world_collision_piston_shifted_box_in_classes_held(
    mc_world_runtime_classes *code_classes, mc_world_runtime *world, const mc_block *moving,
    int32_t x, int32_t y, int32_t z, int32_t carried_id, float progress, int32_t facing,
    mc_entity_bounds **out, mc_world_collision_error *error);

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
