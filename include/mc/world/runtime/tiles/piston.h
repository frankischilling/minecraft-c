#ifndef MC_WORLD_RUNTIME_TILES_PISTON_H
#define MC_WORLD_RUNTIME_TILES_PISTON_H

#include "mc/entity/movement/movement.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/tiles/tiles.h"

typedef struct mc_piston_tick_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int tick_bytecode, push_bytecode;
    size_t entity_index;
    mc_live_tile_error tile;
    mc_registry_error registry;
    mc_entity_error entity;
    mc_world_collision_error collision;
    mc_world_nearby_error nearby;
    mc_entity_move_error movement;
    mc_world_mutation_error mutation;
} mc_piston_tick_error;

mc_piston_tick_error mc_piston_tick_empty_error(void);

/* Actual uk.n_()/mu.g_() and its private a(FF) push body on the held READY
 * Piston. Copy current to previous before any floating-point or World call.
 * Completed progress pushes first, then removes/invalidates/restores; advancing
 * progress adds .5, caps only values >=1, and pushes only when extending.
 * Neither invalidity nor a nullable source World adds an early source guard.
 *
 * Push uses the defining classes' actual live moving Block, carrier geometry,
 * acquiring nearby query and shared Piston List. Duplicates and failed-prefix
 * leftovers retain their order and independent native holds. Clear occurs only
 * after normal iteration. Each Entity uses its actual supported movement
 * dispatch; missing concrete descendants fail at the reached virtual call.
 * The separate MovingPiston singleton state-shape method is not invoked here.
 *
 * Keep the coordinated classes, Tile and every supplying World/owner live and
 * exclusive through reentry. Native holds preserve lifetime, not field values,
 * List contents or pooled coordinates. No implicit pool reset or later World
 * tick runs. move_limits is forwarded to actual Entity movement; NULL uses its
 * documented native work ceiling, with exhaustion preserving completed effects.
 * Error and limits storage must be disjoint from owners. Every attempt supplies
 * fresh diagnostics; success returns the empty value. Original tick/push
 * runtime qualification and other Entity kinds remain separate coverage. */
mc_world_storage_status mc_tile_piston_tick(mc_tile *tile, const mc_entity_move_limits *move_limits,
                                            mc_piston_tick_error *error);

/* Complete explicit uk.l()/mu.k(), distinct from tick and Entity pushing.
 * The caller supplies a live, exclusively accessed READY actual Piston owner.
 * This call retains that same Tile through removal and synchronous reentry.
 * Previous progress <1 enters; NaN, 1 and greater return without a World call.
 * No floating environment or World preflight precedes that source guard.
 *
 * Entered execution stores current=1 then previous=1, removes the actual Tile
 * at the current World/coordinates, invalidates this receiver, and separately
 * reloads World/coordinates for its block query. Only equality with the live
 * Block.ad.bn invokes the notifying ID+metadata setter. Its boolean, including
 * false, is discarded. World/coordinate/carrier fields remain live between
 * calls; completed effects survive any later failure without rollback.
 *
 * NULL World is admitted until a source invocation actually reaches it.
 * error is optional, fresh value storage disjoint from owners/arguments. It
 * retains complete reached Tile/query/Registry/setter causes with separate
 * current/deepest Piston PCs. Success returns an empty error for this attempt.
 * Keep the coordinated classes and every supplying owner live/exclusive.
 * Tick/push is the separate operation above; collision owns singleton state-shape. */
mc_world_storage_status mc_tile_piston_finalize(mc_tile *tile, mc_world_mutation_error *error);

#endif
