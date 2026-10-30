#ifndef MC_CLIENT_WORLD_LOAD_WORLD_TERMINAL_INTERNAL_H
#define MC_CLIENT_WORLD_LOAD_WORLD_TERMINAL_INTERNAL_H

#include "load_world.h"
#include "mc/world/runtime/entities.h"

typedef struct mc_client_singleplayer_controller mc_client_singleplayer_controller;

/* Passive first-scope admission for a fully published selected World after the
 * real terminal prefix. The transition owner supplies the address of its actual
 * owned base-controller slot, never App.controller's borrowed source alias.
 * Other captured controllers, failed/partial loads and partially consumed tails
 * remain unsupported. This checks actual controller/effect/Input occurrences
 * and passes actual Entity/World owning slots to the lower audit. No permission
 * is cached and no owner, source failure or cleanup receipt changes. Outputs
 * must be disjoint; out is required and error is optional. Keep the exclusive
 * graph unchanged through any subsequent cleanup. This does not authorize
 * handler/format/App/services destruction or retire global dispatchers. */
mc_world_storage_status mc_client_world_load_terminal_admit(
    const mc_client_world_load_call *, mc_client_singleplayer_controller *const *controller_owner,
    const mc_world_terminal_limits *, mc_world_terminal_inventory *, mc_world_storage_error *);

#endif
