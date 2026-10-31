#ifndef MC_CLIENT_WORLD_LOAD_WORLD_TERMINAL_INTERNAL_H
#define MC_CLIENT_WORLD_LOAD_WORLD_TERMINAL_INTERNAL_H

#include "load_world.h"
#include "mc/world/runtime/entities.h"

typedef struct mc_client_singleplayer_controller mc_client_singleplayer_controller;

typedef enum mc_client_terminal_storage_scope {
    MC_CLIENT_TERMINAL_EMPTY_STORAGE,
    MC_CLIENT_TERMINAL_SELECTED_LOAD
} mc_client_terminal_storage_scope;

/* Passive complete-namespace check. owned_call_count is derived by the actual
 * transition owner from its bounded live node slots, never a hold allowance.
 * Zero calls additionally proves that this App has no retained storage graph.
 * No cleanup or owner diagnostic mutation; error is optional and disjoint. */
mc_world_storage_status
mc_client_world_load_classes_terminal_validate(const mc_client_world_load_classes *,
                                               client_application *expected_app,
                                               size_t owned_call_count, mc_world_storage_error *);

/* Classify this actual prepared call without authorizing World destruction.
 * EMPTY_STORAGE requires no acquired or consumed World/Handler/Player/Input
 * graph and no captured Format-owned Handler. Owned non-Handler Format prefixes
 * may remain for the existing checked cleanup. SELECTED_LOAD requires an intact
 * successful selected load; the existing strict admission must still run fresh.
 * Partial/consumed tails remain unsupported. out is required, error optional;
 * both are disjoint from owners/each other, and failure preserves *out. */
mc_world_storage_status mc_client_world_load_terminal_classify(
    const mc_client_world_load_call *, const mc_client_world_load_classes *expected_classes,
    mc_client_terminal_storage_scope *out, mc_world_storage_error *);

/* Passive first-scope admission for a fully published selected World after the
 * real terminal prefix. The transition owner supplies the address of its actual
 * owned base-controller slot, never App.controller's borrowed source alias.
 * Other captured controllers, failed/partial loads and partially consumed tails
 * remain unsupported. This checks actual controller/effect/Input occurrences
 * and passes actual Entity/World owning slots to the lower audit. The Handler
 * audit counts the Load slot and the actual World/HC/loader Store slots while
 * checking the loader's independent holds and source input quiescence. No permission
 * is cached and no owner, source failure or cleanup receipt changes. Outputs
 * must be disjoint; out is required and error is optional. Keep the exclusive
 * graph unchanged through any subsequent cleanup. This does not authorize
 * final Format/App/services destruction or retire global dispatchers. */
mc_world_storage_status mc_client_world_load_terminal_admit(
    const mc_client_world_load_call *, mc_client_singleplayer_controller *const *controller_owner,
    const mc_world_terminal_limits *, mc_world_terminal_inventory *, mc_world_storage_error *);

#endif
