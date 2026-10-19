#ifndef MC_WORLD_RUNTIME_BLOCKS_H
#define MC_WORLD_RUNTIME_BLOCKS_H

#include "mc/world/runtime/construction.h"

/* Actual GAME World block reads through its installed provider/cache. The
 * x domain is [-32000000,32000000), z is [-32000000,32000000], and y is
 * [0,127]. Outside returns air without acquisition. Inside, signed arithmetic
 * chunk shifts and local bit masks select the actual acquired chunk payload.
 * Missing generation/activation capabilities fail explicitly, retaining prior
 * acquisition effects; an absent cached chunk is never treated as air.
 * The owner may still be constructing or have a retained failed prefix, but
 * retirement rejects. Failure preserves out. Exclusive access and disjoint
 * output/error storage are required. No implicit save occurs. */
mc_world_storage_status mc_world_runtime_block_get(mc_world_runtime *runtime, int32_t x, int32_t y,
                                                   int32_t z, int32_t *out,
                                                   mc_world_storage_error *error);

/* Start at y=63, acquire/read y+1 until air, then read y at that same x/z.
 * This is the original top-block operation, not a height-map query or a
 * downward search. The bounded World getter supplies air above height127. */
mc_world_storage_status mc_world_runtime_top_block(mc_world_runtime *runtime, int32_t x, int32_t z,
                                                   int32_t *out, mc_world_storage_error *error);

#endif
