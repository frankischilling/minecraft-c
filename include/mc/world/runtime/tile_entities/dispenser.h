#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_DISPENSER_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_DISPENSER_H

#include "mc/world/runtime/tile_entities/tile_entities.h"

/* Actual AZ nine-cell array and literal LW size9/limit64/name "Trap". Get
 * publishes an independent hold on the same nullable Stack. The supplied owner
 * graph stays live/exclusive. Native owner/array admission is separate from
 * Source index, Stack and dirty execution. */
mc_tile_entity_status mc_tile_entity_dispenser_size(const mc_tile_entity *, int32_t *);
mc_tile_entity_status mc_tile_entity_dispenser_limit(const mc_tile_entity *, int32_t *);
mc_tile_entity_status mc_tile_entity_dispenser_name(const mc_tile_entity *, const char **);
mc_tile_entity_status mc_tile_entity_dispenser_get(mc_tile_entity *, int32_t, mc_item_stack **);
/* Store before count clamp, then actual OW dirty at AZ31. Split preserves
 * signed counts/amounts and calls dirty37/73 only on a nonempty slot. Earlier
 * mutations survive failure; an unreturned result never replaces out. The
 * detailed failure borrows actual WorldAccess producers; an enclosing retained
 * caller captures them before continuing and expires them before World teardown. */
mc_tile_entity_status mc_tile_entity_dispenser_set_detailed(mc_tile_entity *, int32_t,
                                                            mc_item_stack *,
                                                            mc_live_tile_entity_error *);
mc_tile_entity_status mc_tile_entity_dispenser_set(mc_tile_entity *, int32_t, mc_item_stack *);
mc_tile_entity_status mc_tile_entity_dispenser_remove_detailed(mc_tile_entity *, int32_t, int32_t,
                                                               mc_item_stack **,
                                                               mc_live_tile_entity_error *);
mc_tile_entity_status mc_tile_entity_dispenser_remove(mc_tile_entity *, int32_t, int32_t,
                                                      mc_item_stack **);

#endif
