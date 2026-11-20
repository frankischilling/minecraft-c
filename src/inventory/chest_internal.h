#ifndef MC_INVENTORY_CHEST_INTERNAL_H
#define MC_INVENTORY_CHEST_INTERNAL_H

#include "internal.h"

/* Fixed JS adapter leaves, linked with simulation. Core OG/CD never reaches
 * into a World or TileEntity. The private table also permits exact controlled
 * LW observations in portable tests without changing the public receiver set. */
typedef struct inv_chest_leaf {
    void (*release)(void *);
    mc_inventory_status (*size)(void *, int32_t *, mc_inventory_error *);
    mc_inventory_status (*name)(void *, const char **, mc_inventory_error *);
    mc_inventory_status (*limit)(void *, int32_t *, mc_inventory_error *);
    mc_inventory_status (*borrow)(void *, int32_t, const mc_item_stack **, mc_inventory_error *);
    mc_inventory_status (*set)(void *, int32_t, mc_item_stack *, mc_inventory_error *);
    mc_inventory_status (*split)(void *, int32_t, int32_t, mc_item_stack **, mc_inventory_error *);
    mc_inventory_status (*changed)(void *, mc_inventory_error *);
    mc_inventory_status (*usable)(void *, mc_entity *, bool *, mc_inventory_error *);
    void (*end_calls)(void *);
} inv_chest_leaf;

mc_inventory_status inv_chest_leaf_create(mc_registry_side, void *, mc_tile_entity *,
                                          const inv_chest_leaf *, const mc_inventory_limits *,
                                          mc_inventory_chest **, mc_inventory_error *);

/* Passive source graph paths: duplicate OG edges yield duplicate rows. A JS
 * leaf view owns exactly one TileEntity right, regardless of its native holds;
 * deduplicate owner identities to count those rights. No source calls, owner
 * mutation, allocation or retain. All graph/context owners remain exclusive. */
typedef struct inv_chest_leaf_view {
    const mc_inventory_chest *owner;
    mc_tile_entity *tile;
    size_t native_holds;
} inv_chest_leaf_view;

mc_inventory_status inv_chest_leaf_count(const mc_inventory_chest *, size_t *,
                                         mc_inventory_error *);
mc_inventory_status inv_chest_leaf_at(const mc_inventory_chest *, size_t, inv_chest_leaf_view *,
                                      mc_inventory_error *);

typedef enum inv_chest_phase { INV_CHEST_RAW, INV_CHEST_READY, INV_CHEST_FAILED } inv_chest_phase;

typedef struct inv_chest_node_view {
    const mc_inventory_chest *owner, *left, *right;
    mc_tile_entity *tile;
    size_t native_holds;
    bool large, active, terminal_sealed;
    inv_chest_phase phase;
} inv_chest_node_view;

mc_inventory_status inv_chest_node_count(const mc_inventory_chest *, size_t *,
                                         mc_inventory_error *);
mc_inventory_status inv_chest_node_at(const mc_inventory_chest *, size_t, inv_chest_node_view *,
                                      mc_inventory_error *);
/* Local native proof only; outer World/Tile/Player/external-root rights remain
 * the caller's authority. All existing HP/CD rights may release after sealing.
 * seal is the infallible commit after admit over the same exclusive graph. */
mc_inventory_status inv_chest_terminal_admit(const mc_inventory_chest *, bool sealed,
                                             mc_inventory_error *);
void inv_chest_terminal_seal(mc_inventory_chest *);

#endif
