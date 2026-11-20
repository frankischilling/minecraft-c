#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_PISTON_INTERNAL_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_PISTON_INTERNAL_H

#include "mc/world/runtime/tile_entities/piston.h"
#include "tile_entities_internal.h"

/* Actual newly evaluated TileEntity World/XYZ block query. The same TileEntity is already
 * held; the reached World call owns its lease and complete typed acquisition
 * cause. This helper adds no finalizer/tick frame or prior World preflight. */
bool mc_tile_entity_piston_current_block_held(mc_tile_entity *tile_entity, int32_t *out,
                                              mc_world_mutation_error *error);

#endif
