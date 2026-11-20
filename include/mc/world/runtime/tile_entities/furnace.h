#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_FURNACE_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_FURNACE_H

#include "mc/world/runtime/tile_entities/tile_entities.h"

typedef struct mc_furnace_fields {
    int32_t burn_remaining, fuel_duration, cook_progress;
} mc_furnace_fields;

typedef enum mc_furnace_field {
    MC_FURNACE_BURN_REMAINING,
    MC_FURNACE_FUEL_DURATION,
    MC_FURNACE_COOK_PROGRESS
} mc_furnace_field;

/* Actual SK fields/three-cell array. Get/remove publish an independent hold
 * on the same nullable Stack; whole removal transfers the slot occurrence.
 * Set/remove do not dirty. Set retains before store and clamps only count>64;
 * negative counts/amounts and Stack aliases follow the actual source bodies. */
mc_tile_entity_status mc_tile_entity_furnace_size(const mc_tile_entity *, int32_t *);
mc_tile_entity_status mc_tile_entity_furnace_limit(const mc_tile_entity *, int32_t *);
mc_tile_entity_status mc_tile_entity_furnace_name(const mc_tile_entity *, const char **);
mc_tile_entity_status mc_tile_entity_furnace_get(mc_tile_entity *, int32_t, mc_item_stack **);
mc_tile_entity_status mc_tile_entity_furnace_set(mc_tile_entity *, int32_t, mc_item_stack *);
mc_tile_entity_status mc_tile_entity_furnace_remove(mc_tile_entity *, int32_t, int32_t,
                                                    mc_item_stack **);
mc_tile_entity_status mc_tile_entity_furnace_fields_get(const mc_tile_entity *,
                                                        mc_furnace_fields *);
/* Native access to the actual public source int field, without dirty/tick. */
mc_tile_entity_status mc_tile_entity_furnace_field_set(mc_tile_entity *, mc_furnace_field, int32_t);
mc_tile_entity_status mc_tile_entity_furnace_burning(const mc_tile_entity *, bool *);
mc_tile_entity_status mc_tile_entity_furnace_cook_scale(const mc_tile_entity *, int32_t, int32_t *);
/* Source c(I) stores fuel_duration=200 when zero before the division. */
mc_tile_entity_status mc_tile_entity_furnace_burn_scale(mc_tile_entity *, int32_t, int32_t *);

/* Defining Registry is the actual source namespace, independent of nullable
 * World. Reached source fields/static arrays/recipe singleton are read fresh;
 * errors preserve their actual registry/item cause and SK getter bytecode.
 * Success preserves out until the whole body completes. No World dirty is
 * supplied by fuel/can_smelt/smelt; the original tick decides when to call it. */
mc_tile_entity_status mc_tile_entity_furnace_fuel(mc_registry_classes *, const mc_item_stack *,
                                                  int32_t *, mc_tile_entity_error *);
mc_tile_entity_status mc_tile_entity_furnace_can_smelt(mc_tile_entity *, mc_registry_classes *,
                                                       bool *, mc_tile_entity_error *);
mc_tile_entity_status mc_tile_entity_furnace_smelt(mc_tile_entity *, mc_registry_classes *,
                                                   mc_tile_entity_error *);

#endif
