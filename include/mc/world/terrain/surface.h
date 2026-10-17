#ifndef MC_WORLD_TERRAIN_SURFACE_H
#define MC_WORLD_TERRAIN_SURFACE_H

#include "mc/world/terrain/terrain.h"

#define MC_TERRAIN_NORMAL_SURFACE_VALUES 256u

/* Snapshot of an original biome's current public byte fields p/q. These are
 * byte patterns, not immutable defaults or biome registry IDs. Materials use
 * x*16+z order, matching the normal biome and block column layouts. */
typedef struct mc_terrain_surface_material {
    uint8_t top;
    uint8_t filler;
} mc_terrain_surface_material;

/* Borrowed r/s/t outputs. Each needs at least 256 values. Noise sampling clears
 * each complete span, including its suffix, before filling the normal prefix.
 * Descriptors are never changed. Apply only reads the pointed-to arrays. */
typedef struct mc_terrain_normal_surface_workspace {
    mc_terrain_double_span sand;
    mc_terrain_double_span gravel;
    mc_terrain_double_span thickness;
} mc_terrain_normal_surface_workspace;

/* Original normal-entry seed: signed chunk coordinates promoted separately
 * to long, multiplied by 341873128712/132897987541, then added with Java long
 * wrapping. No world-seed XOR and no Random mutation. */
int64_t mc_terrain_normal_chunk_seed(int32_t chunk_x, int32_t chunk_z);

/* All operations require initialized, unmodified constructor state, an
 * initialized Random where used, and valid borrowed storage. Every complete
 * double length must fit its byte span;
 * materials_length must fit SIZE_MAX/sizeof(mc_terrain_surface_material).
 * Null pointers, short spans or byte-size overflow fail before any mutation.
 * Calls allocate nothing. Exact finite results assume binary32/binary64
 * round-to-nearest with no floating contraction, as for normal terrain.
 *
 * Mutable spans/Random must be mutually disjoint and must not overlap state,
 * descriptors or read-only inputs. These are caller storage preconditions,
 * rather than runtime pointer-range checks. In apply, read-only noise arrays
 * may overlap one another. The embedded state.random_after_init is read-only;
 * copy it to a separate Random for a direct post-constructor surface call. */

/* Sample the original r/s/t requests using existing four-layer n/o channels.
 * Noise origins independently wrap chunk_x*16/chunk_z*16 as Java int before
 * conversion. Sand/thickness retain their general 16x16x1 grid branch; gravel
 * retains its distinct 16x1x16 height-one branch. No Random draws; state remains
 * unchanged. Workspace descriptors remain unchanged; all three spans clear. */
bool mc_terrain_normal_surface_noise(const mc_terrain_normal_state *state,
                                     const mc_terrain_normal_surface_workspace *workspace,
                                     int32_t chunk_x, int32_t chunk_z);

/* Apply prepared fields and at least 256 material pairs to an existing block
 * prefix, advancing the explicit Random without reseeding. Visits Z then X,
 * descending Y; selectively changes the first 32768 bytes in x/z/y layout.
 * Preserves the block suffix, all fields/materials and descriptors. Arbitrary
 * double fields are accepted; thickness uses Java narrowing, including its
 * defined nonfinite behavior. The original comparison domain uses finite
 * generated fields. All non-stone bytes retain their patterns except where
 * the per-Y bedrock decision overrides them. */
bool mc_terrain_normal_surface_apply(const mc_terrain_normal_surface_workspace *fields,
                                     const mc_terrain_surface_material *materials,
                                     size_t materials_length, mc_java_random *random,
                                     uint8_t *blocks, size_t blocks_length);

/* Complete preflight, then surface_noise and surface_apply. Validates block,
 * material and Random arguments before clearing workspace. Checked failures
 * preserve workspace contents/descriptors, blocks, Random and constructor
 * state. No implicit reseeding; callers choose the direct or chunk-seeded
 * Random context explicitly. */
bool mc_terrain_normal_surface(const mc_terrain_normal_state *state,
                               const mc_terrain_normal_surface_workspace *workspace,
                               int32_t chunk_x, int32_t chunk_z,
                               const mc_terrain_surface_material *materials,
                               size_t materials_length, mc_java_random *random, uint8_t *blocks,
                               size_t blocks_length);

#endif
