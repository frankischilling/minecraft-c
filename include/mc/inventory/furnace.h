#ifndef MC_INVENTORY_FURNACE_H
#define MC_INVENTORY_FURNACE_H

#include "mc/inventory/errors.h"

typedef struct mc_entity mc_entity;
typedef struct mc_tile_entity mc_tile_entity;
typedef struct mc_inventory_furnace mc_inventory_furnace;

typedef struct mc_inventory_furnace_state {
    mc_registry_side side;
    mc_tile_entity *tile; /* Borrowed actual SK; one Tile right belongs to this adapter. */
    size_t native_holds, allocation_bytes;
    bool active, terminal_sealed;
} mc_inventory_furnace_state;

/* Fixed actual SK LW receiver. The adapter owns one Tile occurrence; native
 * release never calls source dirty/close. Supplied owners remain exclusive. */
mc_inventory_status mc_inventory_furnace_from_tile(mc_registry_side, mc_tile_entity *,
                                                   const mc_inventory_limits *,
                                                   mc_inventory_furnace **, mc_inventory_error *);
mc_inventory_status mc_inventory_furnace_retain(mc_inventory_furnace *, mc_inventory_error *);
void mc_inventory_furnace_release(mc_inventory_furnace *);
mc_inventory_status mc_inventory_furnace_retire(mc_inventory_furnace **, mc_inventory_error *);
mc_inventory_status mc_inventory_furnace_end_calls(mc_inventory_furnace *, mc_inventory_error *);
bool mc_inventory_furnace_read_state(const mc_inventory_furnace *, mc_inventory_furnace_state *);
mc_inventory_status mc_inventory_furnace_tile_borrow(mc_inventory_furnace *, mc_tile_entity **,
                                                     mc_inventory_error *);
mc_inventory_status mc_inventory_furnace_size(mc_inventory_furnace *, int32_t *,
                                              mc_inventory_error *);
mc_inventory_status mc_inventory_furnace_name(mc_inventory_furnace *, const char **,
                                              mc_inventory_error *);
mc_inventory_status mc_inventory_furnace_limit(mc_inventory_furnace *, int32_t *,
                                               mc_inventory_error *);
mc_inventory_status mc_inventory_furnace_borrow(mc_inventory_furnace *, int32_t,
                                                const mc_item_stack **, mc_inventory_error *);
mc_inventory_status mc_inventory_furnace_get(mc_inventory_furnace *, int32_t, mc_item_stack **,
                                             mc_inventory_error *);
/* SK set and split do not invoke dirty. GP changed is a distinct reached call. */
mc_inventory_status mc_inventory_furnace_set(mc_inventory_furnace *, int32_t, mc_item_stack *,
                                             mc_inventory_error *);
mc_inventory_status mc_inventory_furnace_split(mc_inventory_furnace *, int32_t, int32_t,
                                               mc_item_stack **, mc_inventory_error *);
mc_inventory_status mc_inventory_furnace_changed(mc_inventory_furnace *, mc_inventory_error *);
mc_inventory_status mc_inventory_furnace_usable(mc_inventory_furnace *, mc_entity *, bool *,
                                                mc_inventory_error *);
mc_inventory_status mc_inventory_furnace_burning(mc_inventory_furnace *, bool *,
                                                 mc_inventory_error *);
mc_inventory_status mc_inventory_furnace_cook_scale(mc_inventory_furnace *, int32_t, int32_t *,
                                                    mc_inventory_error *);
mc_inventory_status mc_inventory_furnace_burn_scale(mc_inventory_furnace *, int32_t, int32_t *,
                                                    mc_inventory_error *);

#endif
