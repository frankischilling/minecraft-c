#ifndef MC_WORLD_RUNTIME_PLAYERS_H
#define MC_WORLD_RUNTIME_PLAYERS_H

#include "mc/entity/nbt/errors.h"
#include "mc/world/runtime/entities.h"

typedef struct mc_world_player_attachment_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode;
    mc_world_storage_error cause;
    mc_world_info_status info;
    mc_entity_nbt_error nbt;
    mc_world_entity_error publication;
} mc_world_player_attachment_error;

/* Actual fd.y(): J=!d.isEmpty before iterator capture, then gs.N on actual
 * occurrences until the first awake Player sets J=false and stops. Real gs.N
 * reads only u. No callback, health/timer test, sleep transition or tick occurs.
 * Completed J stores survive errors; malformed/null receivers keep source PCs.
 * Hold the exact live client GAME World throughout; RAW/failed prefixes remain real.
 * Native container retirement and composition are rejected before source work. */
mc_world_storage_status mc_world_runtime_update_player_sleeping(mc_world_runtime *world,
                                                                mc_world_player_sleep_error *error);

/* Actual client fd.a(gs), distinct from fd.a(Entity) weather publication.
 * Read the actual Info.Player even when NULL. A nonnull captured tag enters
 * inherited Entity NBT load; only complete success permits Info.Player clear.
 * Canonical dc uses the same-owner explicit code Registry for the actual
 * Inventory read. Sleeping=false loads can finish; Sleeping=true retains the
 * genuine wake93 frontier after its preceding fields/coordinate stores.
 * Never clear that tag or publish after an incomplete/failed source read.
 *
 * Actual current ok/jb map-cache provenance makes instanceof kx false. Cache
 * policy, grid storage and coordinates never select centered-grid behavior;
 * no kx constructor/owner or substitute center callback is supplied here.
 * Then call actual fd.b(Entity), discarding its ordinary boolean. Player
 * publication keeps d append, sleeping state, Chunk, b and live callbacks in
 * source order. Duplicates have independent occurrence holds at every owner.
 *
 * Source catches java.lang.Exception and prints its stack at88. This native
 * API reports the reached explicit error/frontier, preserving effects; it does
 * not emulate Throwable/PrintStream or turn unsupported NBT into success.
 * Native errors preserve their policy/ownership meaning. Keep all owners and
 * input/error storage live, exclusive and disjoint. A typed NULL Player reaches
 * its source dereference; arbitrary Entity subclasses are native admission errors.
 * No Player reset/controller/input/tick/render operation is implied. */
mc_world_storage_status mc_world_runtime_attach_player(mc_world_runtime *world, mc_entity *player,
                                                       mc_world_player_attachment_error *error);

#endif
