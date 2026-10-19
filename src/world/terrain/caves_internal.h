#ifndef MC_WORLD_TERRAIN_CAVES_INTERNAL_H
#define MC_WORLD_TERRAIN_CAVES_INTERNAL_H

#include "mc/world/terrain/caves.h"

/* Shared finite path arithmetic, with no World/Registry/block dependency.
 * Use actual initialized shared Random state; begin owns a fresh explicit-
 * seeded local Random in the frame. Fixed range8 runtime/room/child forms and
 * the existing finite protected geometry are admitted. No coordinate whitelist
 * is imposed here. All storage is disjoint/exclusive and trig remains live.
 * room/slow_pitch are source locals, not numerical/GAME policy selectors. */
typedef struct mc_terrain_cave_path_args {
    int32_t chunk_x, chunk_z;
    double x, y, z, vertical_ratio;
    float width, yaw, pitch;
    int32_t step, end;
} mc_terrain_cave_path_args;

typedef struct mc_terrain_cave_path_frame {
    mc_terrain_cave_path_args current;
    int32_t origin_x, origin_z;
    double target_x, target_z;
    float yaw_velocity, pitch_velocity;
    mc_java_random local;
    int32_t fork_step;
    bool room, slow_pitch;
} mc_terrain_cave_path_frame;

typedef struct mc_terrain_cave_ellipsoid {
    double x, y, z, radius, vertical_radius;
    int32_t origin_x, origin_z;
    int32_t low_x, high_x, low_y, high_y, low_z, high_z;
} mc_terrain_cave_ellipsoid;

typedef struct mc_terrain_cave_column {
    double normalized_x, normalized_z;
    int32_t initial_index;
} mc_terrain_cave_column;

typedef enum mc_terrain_cave_step {
    MC_TERRAIN_CAVE_END,
    MC_TERRAIN_CAVE_SKIP,
    MC_TERRAIN_CAVE_FORK,
    MC_TERRAIN_CAVE_ELLIPSOID
} mc_terrain_cave_step;

void mc_terrain_cave_path_begin(mc_terrain_cave_path_frame *frame, mc_java_random *shared,
                                const mc_terrain_cave_path_args *args);
/* Does not advance current.step. Only ELLIPSOID publishes out; other ordinary
 * outcomes retain it. Caller advances at source1207 after completed/skipped
 * work; a failed concrete read/write has not reached that increment. */
mc_terrain_cave_step mc_terrain_cave_path_step(mc_terrain_cave_path_frame *frame,
                                               const mc_math_trig *trig,
                                               mc_terrain_cave_ellipsoid *out);
/* At FORK, call index0 once, complete that recursive child, then index1 once.
 * Each call consumes exactly one local width draw; never prepare both before
 * the first child returns. Only the two fixed indices are admitted. */
void mc_terrain_cave_path_child(mc_terrain_cave_path_frame *parent, unsigned int index,
                                mc_terrain_cave_path_args *out);
double mc_terrain_cave_ellipsoid_x(const mc_terrain_cave_ellipsoid *ellipse, int32_t local_x);
/* Always publishes normalized values/index, then returns strict horizontal
 * inclusion. initial_index uses high_y; geometric Y begins at high_y-1. */
bool mc_terrain_cave_ellipsoid_column(const mc_terrain_cave_ellipsoid *ellipse, double normalized_x,
                                      int32_t local_x, int32_t local_z,
                                      mc_terrain_cave_column *out);
bool mc_terrain_cave_ellipsoid_contains(const mc_terrain_cave_ellipsoid *ellipse,
                                        const mc_terrain_cave_column *column, int32_t geometric_y);

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

/* Existing-owner provider path through the same scheduler/geometry as public
 * normal caves. Random must be valid initialized state. Initial world-seed
 * and neighbor setSeed operations preserve stored Gaussian bits and clear
 * validity. The public arbitrary-prior-state numerical entry instead fresh-
 * seeds its output projection. Other bounds/failure/disjointness rules match
 * caves.h; there is no profile access or default construction in either pass. */
bool mc_terrain_normal_caves_reseed(const mc_math_trig *table, int64_t world_seed, int32_t chunk_x,
                                    int32_t chunk_z, mc_java_random *cave_random, uint8_t *blocks,
                                    size_t blocks_length);

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
