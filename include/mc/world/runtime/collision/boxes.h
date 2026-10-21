#ifndef MC_WORLD_RUNTIME_COLLISION_BOXES_H
#define MC_WORLD_RUNTIME_COLLISION_BOXES_H

#include "mc/world/runtime/collision/entities.h"

/* One lease on the actual mutable K List, not a coordinate snapshot. Each
 * occurrence keeps its actual pooled box or owning Entity alive. A later
 * query may clear/refill the same List and pooled boxes may be overwritten
 * by an explicit pool reset/reuse even while this view is held. Do not copy
 * a held view; release each successful lease exactly once. */
typedef struct mc_world_collision_list_view {
    mc_world_runtime *owner;
    const mc_world_reference_list *identity;
} mc_world_collision_list_view;

typedef struct mc_world_collision_error {
    mc_world_nearby_error nearby;
    mc_entity_bounds_error bounds;
    const char *block_class;
    unsigned int world_bytecode, block_bytecode;
    /* Moving-piston e and its shifted-box helper remain independent enclosing
     * frames when the carrier getter, Tile or Direction operation fails. */
    unsigned int piston_bytecode, shifted_bytecode;
    int32_t x, y, z;
} mc_world_collision_error;

/* Actual fd/dj.a(Entity,AABB): clear current K, gather Block contributions,
 * then nearby candidate and pair boxes in source order. No sorting, merging,
 * deduplication, implicit pool reset or coordinate copying occurs. Reached
 * failure preserves the completed List/pool/Block prefix and leaves out alone.
 * Arguments and their owners stay live/exclusive throughout the call. out
 * must be fresh storage disjoint from owners, arguments and error storage. Native
 * views block retirement/free but permit later synchronous source mutation.
 * Unsupported concrete descendants fail at their reached call, never as an
 * invented successful null shape. A returned null source List is representable.
 */
mc_world_storage_status mc_world_runtime_collision_boxes(mc_world_runtime *world, mc_entity *moving,
                                                         const mc_entity_bounds *bounds,
                                                         mc_world_collision_list_view *out,
                                                         mc_world_collision_error *error);
mc_world_storage_status mc_world_collision_list_view_count(const mc_world_collision_list_view *view,
                                                           size_t *out);
/* get borrows the actual box, adding no hold. Keep the view, List contents and
 * box owner live until the borrow ends. No six-coordinate snapshot is made. */
mc_world_storage_status mc_world_collision_list_view_get(const mc_world_collision_list_view *view,
                                                         size_t index,
                                                         const mc_entity_bounds **out);
void mc_world_collision_list_view_release(mc_world_collision_list_view *view);
mc_world_collision_error mc_world_collision_empty_error(void);

#endif
