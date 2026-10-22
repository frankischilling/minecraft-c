#ifndef MC_WORLD_RUNTIME_TILES_PISTON_INTERNAL_H
#define MC_WORLD_RUNTIME_TILES_PISTON_INTERNAL_H

#include "mc/world/runtime/tiles/piston.h"
#include "tiles_internal.h"

/* Actual newly evaluated Tile World/XYZ block query. The same Tile is already
 * held; the reached World call owns its lease and complete typed acquisition
 * cause. This helper adds no finalizer/tick frame or prior World preflight. */
bool mc_tile_piston_current_block_held(mc_tile *tile, int32_t *out, mc_world_mutation_error *error);

#endif
