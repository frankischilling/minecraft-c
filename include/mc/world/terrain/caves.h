#ifndef MC_WORLD_TERRAIN_CAVES_H
#define MC_WORLD_TERRAIN_CAVES_H

#include "mc/core/java/java.h"
#include "mc/core/math/math.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MC_TERRAIN_NORMAL_CAVE_RANGE 8u
#define MC_TERRAIN_CAVE_BLOCKS 32768u

/* Complete normal cave pass over a borrowed chunk prefix, with fixed range 8.
 * Requires an initialized, immutable table and valid disjoint table/Random/
 * block storage. Own cave_random separately from generator/surface Random.
 * Its prior contents are arbitrary: success seeds it from world_seed first.
 * Only the first MC_TERRAIN_CAVE_BLOCKS bytes are accessed; suffix is retained.
 *
 * Returns false before mutation for null table/Random/blocks, short capacity,
 * or either chunk coordinate outside INT32_MIN+8 .. INT32_MAX-9 inclusive.
 * Otherwise completes without allocation or a mid-pass checked failure.
 * Retains Java wrapping within this ordinary 289-neighbor scheduler domain.
 * Exact geometry requires the project's strict binary32/binary64 FP contract.
 */
bool mc_terrain_normal_caves(const mc_math_trig *table, int64_t world_seed, int32_t chunk_x,
                             int32_t chunk_z, mc_java_random *cave_random, uint8_t *blocks,
                             size_t blocks_length);

#endif
