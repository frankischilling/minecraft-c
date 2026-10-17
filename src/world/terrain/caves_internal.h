#ifndef MC_WORLD_TERRAIN_CAVES_INTERNAL_H
#define MC_WORLD_TERRAIN_CAVES_INTERNAL_H

#include "mc/world/terrain/caves.h"

/* Private runtime/reference seam; not installed API. The normal pass validates
 * its public inputs. Protected reference adapters validate their own bounded
 * finite geometry before calling these same helpers. No duplicate geometry.
 * All pointers and the chunk domain obey caves.h preconditions; random is
 * already explicitly seeded and blocks contains a complete borrowed prefix.
 */
typedef struct mc_terrain_cave_context {
    const mc_math_trig *table;
    mc_java_random *random;
    uint8_t *blocks;
    int32_t chunk_x;
    int32_t chunk_z;
} mc_terrain_cave_context;

/* Actual room wrapper: consumes shared width draw then calls the path helper
 * with yaw/pitch zero, start/end -1 and vertical ratio 0.5. */
void mc_terrain_cave_room(mc_terrain_cave_context *context, double x, double y, double z);

/* Actual path entry: consumes one shared nextLong and owns its local Random.
 * Runtime supplies normal 0/0 or room -1/-1 step/end signals; child paths
 * inherit valid positive end/current step. The initial protected adapter
 * supplies finite centers, width 0..3, ordinary angles, end 85..112,
 * 0<=step<end and ratio 1. No arbitrary/nonfinite geometry is supported.
 * At most two geometry frames for these domains; children have width<=1.
 */
void mc_terrain_cave_path(mc_terrain_cave_context *context, double x, double y, double z,
                          float width, float yaw, float pitch, int32_t step, int32_t end,
                          double vertical_ratio);

#endif
