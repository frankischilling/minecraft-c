#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_SOURCE_DETAILED_INTERNAL_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_SOURCE_DETAILED_INTERNAL_H

#include "mc/world/runtime/tile_entities/tile_entities.h"

typedef struct mc_world_runtime_acquisition_error mc_world_runtime_acquisition_error;

/* SAME ordinary acquiring World TileEntity getter, with its one actual get's
 * complete acquisition copied before public Tile error projection. This does
 * not select an execution context, change Registry/factory lookup, retry a get
 * or consult ambient last_error. out and all three diagnostic outputs are
 * required fresh/disjoint caller storage. Failure preserves out. The caller
 * initializes acquisition before entry; it remains untouched unless the real
 * runtime get is reached. provider_null starts false and becomes true only at
 * the reached null World.v provider receiver, never from native admission or
 * inferred status. It does not imply that acquisition was entered.
 *
 * The returned TileEntity owns the same independent hold as the public getter.
 * Acquisition source_chunk/descendants remain passive borrowed diagnostics;
 * keep their actual World/loader occurrences live. No new owning hold, source
 * class identity or failure-selection owner is created by this facade. */
mc_world_storage_status
mc_live_tile_entities_get_source_detailed(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                          mc_tile_entity **out, mc_live_tile_entity_error *error,
                                          mc_world_runtime_acquisition_error *acquisition,
                                          bool *provider_null);

#endif
