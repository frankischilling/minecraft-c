#ifndef MC_WORLD_RUNTIME_TILES_PISTON_H
#define MC_WORLD_RUNTIME_TILES_PISTON_H

#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/tiles/tiles.h"

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
 * Tick/push and singleton state-shape work are separate unfinished operations. */
mc_world_storage_status mc_tile_piston_finalize(mc_tile *tile, mc_world_mutation_error *error);

#endif
