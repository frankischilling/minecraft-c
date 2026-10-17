#ifndef MC_WORLD_TERRAIN_TERRAIN_H
#define MC_WORLD_TERRAIN_TERRAIN_H

#include "mc/world/noise/noise.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MC_TERRAIN_NORMAL_LAYERS 82u
#define MC_TERRAIN_NORMAL_DENSITY_VALUES 425u
#define MC_TERRAIN_NORMAL_COLUMN_VALUES 25u
#define MC_TERRAIN_NORMAL_CLIMATE_VALUES 256u
#define MC_TERRAIN_NORMAL_BLOCKS 32768u

/* Caller-owned, allocation-free constructor state. The eight channels consume
 * one shared Random in this declaration order, including channels not sampled
 * by the density pass. Treat all fields as read-only after initialization.
 * The continuation is the generator state immediately after construction;
 * sampling neither consumes nor reseeds it. State contains no internal views
 * or owning pointers and may be copied or shared by independent readers. */
typedef struct mc_terrain_normal_state {
    mc_noise_perlin low_layers[16];
    mc_noise_perlin high_layers[16];
    mc_noise_perlin mix_layers[8];
    mc_noise_perlin surface_primary_layers[4];
    mc_noise_perlin surface_secondary_layers[4];
    mc_noise_perlin column_scale_layers[10];
    mc_noise_perlin column_bias_layers[16];
    mc_noise_perlin later_layers[8];
    mc_java_random random_after_init;
} mc_terrain_normal_state;

/* Null state fails without writes. Otherwise initializes every channel from
 * the explicit seed. No default seeding, allocation, world object or I/O. */
bool mc_terrain_normal_init(mc_terrain_normal_state *state, int64_t seed);

typedef struct mc_terrain_double_span {
    double *data;
    size_t length;
} mc_terrain_double_span;

/* Borrowed octave outputs corresponding to original d/e/f/g/h. mix/low/high
 * need at least 425 values; column_scale/column_bias need at least 25. A valid
 * density call clears each complete span before sampling its requested prefix,
 * including any suffix beyond the normal shape. Descriptors are not changed. */
typedef struct mc_terrain_normal_workspace {
    mc_terrain_double_span mix;
    mc_terrain_double_span low;
    mc_terrain_double_span high;
    mc_terrain_double_span column_scale;
    mc_terrain_double_span column_bias;
} mc_terrain_normal_workspace;

/* Processed 16x16 climate in x*16+z order, not raw channel outputs. Each array
 * needs at least 256 values; longer suffixes are ignored. The actual normal
 * consumer samples this grid at separately wrapped Java int origins chunk_x*16
 * and chunk_z*16. For that path, call mc_biome_sample_square with size 16 and
 * pass the resulting manager a/b arrays. Density does not regenerate climate.
 * Read-only temperature/rainfall arrays may overlap each other. */
typedef struct mc_terrain_normal_climate {
    const double *temperature;
    size_t temperature_length;
    const double *rainfall;
    size_t rainfall_length;
} mc_terrain_normal_climate;

/* All sampling requires initialized, unmodified state and valid borrowed
 * storage. Every double span, including complete input lengths, must fit
 * SIZE_MAX/sizeof(double). Null pointers, short spans and byte-size overflow
 * fail before any mutation. Mutable spans must be mutually disjoint and must
 * not overlap state, descriptors or read-only inputs; these storage conditions
 * are caller preconditions rather than pointer-range checks. Calls allocate
 * nothing and do not alter constructor state. Arbitrary double inputs are
 * accepted; exact NaN payload identity is not promised. Exact finite results
 * assume binary32/binary64 round-to-nearest with no floating contraction. */

/* Samples the five original octave requests, then composes the fixed 5x17x5
 * normal density prefix in (x*5+z)*17+y order. Density origins independently
 * wrap chunk_x*4 and chunk_z*4 as Java int before conversion. Density capacity
 * must be at least 425; its suffix is retained, unlike workspace suffixes. */
bool mc_terrain_normal_density(const mc_terrain_normal_state *state,
                               const mc_terrain_normal_workspace *workspace, int32_t chunk_x,
                               int32_t chunk_z, const mc_terrain_normal_climate *climate,
                               mc_terrain_double_span density);

/* Interpolates an existing normal density prefix with the original repeated
 * additions. Temperature needs 256 processed values; density needs 425. Writes
 * all 32768 base blocks in (x*16+z)*128+y order, retaining the output suffix.
 * Outputs contain literal air 0, stationary water 9, ice 79 or stone 1. The
 * read-only density and temperature arrays may overlap each other. */
bool mc_terrain_normal_base(const double *density, size_t density_length, const double *temperature,
                            size_t temperature_length, uint8_t *blocks, size_t blocks_length);

/* Density followed by base interpolation using the same explicit temperature
 * grid. Validates every argument before changing workspace, density or blocks.
 * The supplied density remains available for inspection/future terrain work. */
bool mc_terrain_normal_generate(const mc_terrain_normal_state *state,
                                const mc_terrain_normal_workspace *workspace, int32_t chunk_x,
                                int32_t chunk_z, const mc_terrain_normal_climate *climate,
                                mc_terrain_double_span density, uint8_t *blocks,
                                size_t blocks_length);

#endif
