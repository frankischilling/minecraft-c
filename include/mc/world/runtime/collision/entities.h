#ifndef MC_WORLD_RUNTIME_COLLISION_ENTITIES_H
#define MC_WORLD_RUNTIME_COLLISION_ENTITIES_H

#include "mc/entity/entity.h"
#include "mc/world/runtime/queries.h"
#include "mc/world/runtime/tile_entities/errors.h"

/* A native lease on one actual List identity, not a snapshot of its contents.
 * Query again to mutate that same source List; previously returned views see
 * the new contents. Do not copy a held view or release it more than once.
 * The owner and descriptor remain live until release, including if source R
 * is replaced. Retirement/free stays BUSY while a view is held. */
typedef struct mc_world_entity_list_view {
    mc_world_runtime *owner;
    const mc_world_reference_list *identity;
} mc_world_entity_list_view;

typedef struct mc_world_nearby_error {
    mc_world_query_error query;
    mc_entity_error entity;
    /* Exact per-attempt acquisition descendants, without an ambient error
     * lookup. The lower records are included only when genuinely reached. */
    mc_live_tile_entity_error tile_entity;
    mc_world_entity_batch_error batch;
    mc_world_entity_load_error load_entity;
    unsigned int activation_bytecode, activation_world_bytecode;
    unsigned int load_bytecode, load_builder_bytecode;
    size_t slice, index;
} mc_world_nearby_error;

/* Actual fd/dj.b(Entity,AABB): clear current R, scan inclusive chunk X/Z in
 * source order, test nonloading presence, then use the real acquiring getter
 * and actual normal/Blank gather override. Preserve identity-only exclusion,
 * duplicate occurrences and partial appends on failure. No dead/player/World
 * filter, sorting, deduplication, shape query or AABB pool operation is added.
 *
 * Admit live GAME raw/constructing/failed owners; reject native retirement.
 * excluded may be NULL or any live native Entity; its ID, phase, construction
 * World and class do not select matches. bounds may be NULL until its reached
 * minX read, after clear. Caller keeps a nonnull bounds and its actual owner
 * live across the entire call, independently of R's occurrences. Strict Java
 * floating environment is required at the first arithmetic operation.
 *
 * out must be fresh disjoint storage; failure preserves it and reached source
 * effects. Success supplies one lease, released below. Returned NULL List is
 * representable; count/get then fail. Unknown replacement Lists are explicit
 * reached native dependencies, never adopted as trusted owning storage.
 * Acquisition uses this World's installed concrete provider coordinator;
 * an unrelated replacement provider stops explicitly at its reached getter.
 * Source inclusive loops use wrapping Java increments and have no work cap.
 * No claim about Java ArrayList OOM timing is made by native allocation limits.
 */
mc_world_storage_status mc_world_runtime_nearby_entities(mc_world_runtime *world,
                                                         mc_entity *excluded,
                                                         const mc_entity_bounds *bounds,
                                                         mc_world_entity_list_view *out,
                                                         mc_world_nearby_error *error);

/* Reads borrow the current source occurrence; get adds no Entity hold. Keep
 * the same held view, exclusive owner and List contents live until that borrow
 * ends, or retain the returned Entity before another query/clear. Failure
 * preserves out. Validation is local to the descriptor and reached occurrence;
 * full typed ownership validation is reserved for clear/native retirement. */
mc_world_storage_status mc_world_entity_list_view_count(const mc_world_entity_list_view *view,
                                                        size_t *out);
mc_world_storage_status mc_world_entity_list_view_get(const mc_world_entity_list_view *view,
                                                      size_t index, mc_entity **out);
void mc_world_entity_list_view_release(mc_world_entity_list_view *view);

/* Passive in-place initialization; NULL is a no-op. */
void mc_world_nearby_error_init(mc_world_nearby_error *error);
mc_world_nearby_error mc_world_nearby_empty_error(void);

#endif
