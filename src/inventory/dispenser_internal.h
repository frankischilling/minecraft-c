#ifndef MC_INVENTORY_DISPENSER_INTERNAL_H
#define MC_INVENTORY_DISPENSER_INTERNAL_H

#include "mc/inventory/dispenser.h"

/* Fixed production AZ leaves are linked with simulation. The core Inventory
 * owner does not call World/Tile APIs or depend back on simulation. */
typedef struct inv_dispenser_leaf {
    void (*release)(void *);
    mc_inventory_status (*end_calls)(void *, mc_inventory_error *);
    mc_inventory_status (*size)(void *, int32_t *, mc_inventory_error *);
    mc_inventory_status (*borrow)(void *, int32_t, const mc_item_stack **, mc_inventory_error *);
    mc_inventory_status (*set)(void *, int32_t, mc_item_stack *, mc_inventory_error *);
    mc_inventory_status (*split)(void *, int32_t, int32_t, mc_item_stack **, mc_inventory_error *);
    mc_inventory_status (*changed)(void *, mc_inventory_error *);
    mc_inventory_status (*usable)(void *, mc_entity *, bool *, mc_inventory_error *);
    size_t (*bytes)(const void *);
    const mc_inventory_error *(*cleanup)(const void *);
    mc_inventory_status (*admit)(const void *, mc_inventory_error *);
} inv_dispenser_leaf;

mc_inventory_status inv_dispenser_leaf_create(mc_registry_side, void *, mc_tile_entity *,
                                              const inv_dispenser_leaf *,
                                              const mc_inventory_limits *,
                                              mc_inventory_dispenser **, mc_inventory_error *);

typedef struct inv_dispenser_node_view {
    const mc_inventory_dispenser *owner;
    mc_tile_entity *tile;
    size_t native_holds, allocation_bytes;
    bool active, terminal_sealed;
} inv_dispenser_node_view;

/* Passive native graph proof; one adapter owns exactly one actual Tile right.
 * No source call, allocation, retain or change of admission occurs. */
mc_inventory_status inv_dispenser_node_get(const mc_inventory_dispenser *,
                                           inv_dispenser_node_view *, mc_inventory_error *);
/* Admit over the same exclusive graph before the infallible native seal. The
 * outer World/Player/Tile/external occurrence proof belongs to the caller. */
mc_inventory_status inv_dispenser_terminal_admit(const mc_inventory_dispenser *, bool sealed,
                                                 mc_inventory_error *);
void inv_dispenser_terminal_seal(mc_inventory_dispenser *);

#endif
