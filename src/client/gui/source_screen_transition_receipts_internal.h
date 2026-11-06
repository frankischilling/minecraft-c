#ifndef MC_CLIENT_GUI_SOURCE_SCREEN_TRANSITION_RECEIPTS_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_SCREEN_TRANSITION_RECEIPTS_INTERNAL_H

#include "source_inventory_screen.h"
#include "source_screen_initialize.h"

/* Native diagnostic ownership only. These are scalar copies of actual
 * matching source receipts, never replacement Screen/Container invocations.
 * Remaining owner pointers are identity tokens while that owner stays live. */
typedef struct mc_screen_transition_receipts {
    mc_client_inventory_screen_error inventory;
    mc_client_screen_init_error initialization;
    const mc_client_inventory_screen_error *inventory_source;
    const mc_client_screen_init_error *initialization_source;
} mc_screen_transition_receipts;

bool mc_screen_transition_inventory_capture(mc_screen_transition_receipts *,
                                            mc_client_screen_error *,
                                            const mc_client_inventory_screen_error *actual_owner);
void mc_screen_transition_inventory_after(mc_screen_transition_receipts *,
                                          const mc_client_inventory_screen_error *remaining_owner);
bool mc_screen_transition_initialization_capture(mc_screen_transition_receipts *,
                                                 mc_client_screen_error *,
                                                 const mc_client_screen_init_error *actual_owner);
void mc_screen_transition_initialization_after(mc_screen_transition_receipts *,
                                               const mc_client_screen_init_error *remaining_owner);

#endif
