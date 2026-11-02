#ifndef MC_INVENTORY_PLAYER_TERMINAL_INTERNAL_H
#define MC_INVENTORY_PLAYER_TERMINAL_INTERNAL_H

#include "mc/inventory/container.h"

/* Native-only final ownership of one actual IX, constructor AA and live IT.
 * The Player outer owner proves canonical READY Entity/Registry/World identity;
 * this module proves the exact local Inventory graph before changing seals.
 * Exclusive graph access and a disjoint empty output are required. */
typedef struct mc_inventory_player_terminal {
    mc_inventory_player *inventory;
    mc_container *default_container;
    mc_container *active_container;
    mc_entity *player;
    bool sealed;
} mc_inventory_player_terminal;

/* No allocation, source invocation/store, Stack restriction or partial seal.
 * Failure preserves every source/native field and output, with site-empty
 * native diagnostics. Success seals IX/AA/IT and their grid/result children.
 * New mutations/owning holds/bindings are refused afterward; passive borrows,
 * inspection and release of existing rights remain allowed. */
mc_inventory_status mc_inventory_player_terminal_prepare(mc_inventory_player *, mc_entity *,
                                                         mc_registry_classes *, mc_world_runtime *,
                                                         mc_container *, mc_container *,
                                                         mc_inventory_player_terminal *,
                                                         mc_inventory_error *);

/* Repeat the passive local graph proof and sealed-state consistency check.
 * Registry/World and actual Player storage remain live, exclusive and intact. */
mc_inventory_status mc_inventory_player_terminal_admit(const mc_inventory_player_terminal *,
                                                       mc_registry_classes *, mc_world_runtime *,
                                                       mc_inventory_error *);

/* Final Player zero-hold destruction only, after outer lifetime admission.
 * Infallible native frees in IT, AA, IX order; no source field stores or close,
 * callback, save/drop work. Consumes/zeros the record; empty is a no-op. */
void mc_inventory_player_terminal_dispose(mc_inventory_player_terminal *);

#endif
