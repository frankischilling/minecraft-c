#ifndef MC_CLIENT_GUI_SOURCE_SCREEN_TRANSITIONS_TERMINAL_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_SCREEN_TRANSITIONS_TERMINAL_INTERNAL_H

#include "mc/world/runtime/entities.h"
#include "source_screen_transitions.h"

/* Passive first-scope World admission through this actual transition owner.
 * Requires its completed terminal prefix and exactly one retained selected
 * load. Supplies the real controller owning slot; no caller provides count
 * exemptions or reconstructed anchors. The complete graph stays live. Outputs
 * must be disjoint; out is required, error optional, and out changes only on
 * success. Remaining globals can still make the lower World audit BUSY. */
mc_world_storage_status mc_client_screen_transitions_terminal_admit(
    const mc_client_screen_transitions *, const mc_world_terminal_limits *,
    mc_world_terminal_inventory *, mc_world_storage_error *);

#endif
