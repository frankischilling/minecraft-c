#ifndef MC_CLIENT_GUI_SOURCE_SCREEN_TRANSITIONS_TERMINAL_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_SCREEN_TRANSITIONS_TERMINAL_INTERNAL_H

#include "../world/load_world_terminal_internal.h"
#include "source_screen_transitions.h"

/* Inspect every actual retained node/call after terminal preparation and verify
 * the namespace's exact call count. EMPTY_STORAGE is an explicit absence proof,
 * not a successful empty World audit. SELECTED_LOAD requires one or more actual
 * successful selected epochs within the native8-call bound; terminal_admit
 * below remains mandatory for every epoch before any World cleanup.
 * No source/cleanup/owner diagnostic mutation. out is required, error optional,
 * all output storage is disjoint, and failure preserves *out. */
mc_world_storage_status
mc_client_screen_transitions_terminal_classify(const mc_client_screen_transitions *,
                                               mc_client_terminal_storage_scope *out,
                                               mc_world_storage_error *);

/* Passive first-scope World admission through this actual transition owner.
 * Requires its completed terminal prefix and a bounded retained epoch graph.
 * Audits every selected World with that epoch's real controller owning slot
 * and remaining source roots. Aggregate values are checked sums; scratch is
 * released between audits (limits apply to each actual World, peak memory
 * does not accumulate). No caller provides count
 * exemptions or reconstructed anchors. The complete graph stays live. Outputs
 * must be disjoint; out is required, error optional, and out changes only on
 * success. Remaining globals can still make the lower World audit BUSY. */
mc_world_storage_status mc_client_screen_transitions_terminal_admit(
    const mc_client_screen_transitions *, const mc_world_terminal_limits *,
    mc_world_terminal_inventory *, mc_world_storage_error *);

#endif
