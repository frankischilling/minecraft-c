#ifndef MC_CLIENT_WORLD_LOAD_WORLD_TERMINAL_INTERNAL_H
#define MC_CLIENT_WORLD_LOAD_WORLD_TERMINAL_INTERNAL_H

#include "load_world.h"
#include "mc/world/runtime/entities.h"

typedef struct mc_client_singleplayer_controller mc_client_singleplayer_controller;

typedef enum mc_client_terminal_storage_scope {
    MC_CLIENT_TERMINAL_EMPTY_STORAGE,
    MC_CLIENT_TERMINAL_SELECTED_LOAD
} mc_client_terminal_storage_scope;

/* First pass for all genuine epochs before any N terminal detach: end only
 * completed/failed owned call records. Keep source fields and full graphs.
 * Ordinary renderer reuse never selects this terminal policy. */
mc_client_world_load_status mc_client_world_load_terminal_records(mc_client_world_load_call *);

/* Passive complete-namespace check. owned_call_count is derived by the actual
 * transition owner from its bounded live node slots, never a hold allowance.
 * Zero calls additionally proves that this App has no retained storage graph.
 * No cleanup or owner diagnostic mutation; error is optional and disjoint. */
mc_world_storage_status
mc_client_world_load_classes_terminal_validate(const mc_client_world_load_classes *,
                                               client_application *expected_app,
                                               size_t owned_call_count, mc_world_storage_error *);

/* Passive exact newest-to-oldest ownership check. NULL newer means the actual
 * class latest slot; otherwise both reciprocal links must match. Together with
 * the complete namespace count/chain check, one call per node proves unique
 * ownership in linear time without an allocation or a fixed session limit.
 * Call owners must remain live, prepared, exclusive and disjoint from error. */
mc_world_storage_status
mc_client_world_load_terminal_follows(const mc_client_world_load_call *,
                                      const mc_client_world_load_call *newer,
                                      mc_world_storage_error *);

/* Same passive reciprocal ownership proof before Load child cleanup. The
 * namespace must already have ended new call admission, and its exact count
 * and complete chain must separately pass classes_terminal_validate. This
 * permits the selected root to identify actual live Screen/Player/World
 * owners before retiring reached Screen children that may lease a Load's
 * World. It neither classifies a selected epoch nor admits destruction. */
mc_world_storage_status
mc_client_world_load_terminal_owned_follows(const mc_client_world_load_call *,
                                            const mc_client_world_load_call *newer,
                                            mc_world_storage_error *);

/* Classify this actual prepared call without authorizing World destruction.
 * EMPTY_STORAGE requires no acquired or consumed World/Handler/Player/Input
 * graph and no captured Format-owned Handler. Owned non-Handler Format prefixes
 * may remain for the existing checked cleanup. SELECTED_LOAD requires a
 * successful selected load and either its original App slots or only the exact
 * i16/f69/h355 stores traced by actual source leave calls. A failed leave may
 * retain its reached prefix; no source call is replayed. The existing strict
 * admission must still run fresh. Other partial/consumed tails remain unsupported.
 * out is required, error optional;
 * both are disjoint from owners/each other, and failure preserves *out. */
mc_world_storage_status mc_client_world_load_terminal_classify(
    const mc_client_world_load_call *, const mc_client_world_load_classes *expected_classes,
    mc_client_terminal_storage_scope *out, mc_world_storage_error *);

/* Passive per-epoch admission for a successful selected World after the
 * real terminal prefix. The transition owner supplies the address of its actual
 * owned base-controller slot, never App.controller's borrowed source alias.
 * Each supplied controller slot belongs to this epoch; App.controller is only
 * the newest source alias and is validated by the actual transition owner.
 * Other captured controllers, failed/partial loads and untraced consumed tails
 * remain unsupported. Actual leave stores may have consumed App.i/f/h, while
 * the original null-target branch keeps N/DN World references until a genuine
 * successor replaces them. Superseded epochs require that actual successful
 * successor and own only their retained Player/Input/World/Handler graph.
 * This checks actual
 * controller/effect/Input occurrences and passes actual Entity/World owning slots to the lower
 * audit. The Handler audit counts the Load slot and the actual World/HC/loader Store slots while
 * checking the loader's independent holds and source input quiescence. No permission
 * is cached and no owner, source failure or cleanup receipt changes. Outputs
 * must be disjoint; out is required and error is optional. Keep the exclusive
 * graph unchanged through any subsequent cleanup. This does not authorize
 * final Format/App/services destruction or retire global dispatchers. */
mc_world_storage_status mc_client_world_load_terminal_admit(
    const mc_client_world_load_call *, mc_client_singleplayer_controller *const *controller_owner,
    const mc_world_terminal_limits *, mc_world_terminal_inventory *, mc_world_storage_error *);

#endif
