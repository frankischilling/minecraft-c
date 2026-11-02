#ifndef MC_CLIENT_GUI_SOURCE_SCREEN_TRANSITIONS_TERMINAL_INTERNAL_H
#define MC_CLIENT_GUI_SOURCE_SCREEN_TRANSITIONS_TERMINAL_INTERNAL_H

#include "../world/load_world_terminal_internal.h"
#include "source_screen_transitions.h"

typedef struct mc_client_bootstrap_terminal_scope mc_client_bootstrap_terminal_scope;

/* Selected whole-root native preparation only. The exact embedded bootstrap
 * scope must freshly match this router's App and its inactive, selected menu
 * owner; bootstrap getters have already ended. The router borrows that same
 * token through retirement and refuses another token on retry.
 *
 * Release known UE/closed-OO occurrences before a live OO handoff. The latter
 * checks its actual retained Load/Player/World/Registry/IX and publication
 * ledger, retires reached native children, and transfers only its actual IT
 * owning slot to the sealed Player sidecar. No source close, store, drop or
 * save runs. Ordinary preparation/Screen retirement keep their BUSY guard.
 * Checked native progress is one-way; failure keeps every remaining right
 * live for retry and never supplies a hold-count exemption. */
mc_client_screen_status
mc_client_screen_transitions_terminal_prepare_root(mc_client_screen_transitions *,
                                                   const mc_client_bootstrap_terminal_scope *,
                                                   mc_client_screen_error *);

/* Inspect every actual retained node/call after terminal preparation and verify
 * the namespace's exact call count. EMPTY_STORAGE is an explicit absence proof,
 * not a successful empty World audit. SELECTED_LOAD requires one or more actual
 * successful selected epochs in the complete retained chain; terminal_admit
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
