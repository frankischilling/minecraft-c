#ifndef MC_WORLD_NOISE_H
#define MC_WORLD_NOISE_H

#include "mc/core/java/java.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Caller-owned seeded state. Treat fields as read-only after initialization.
 * A state may be shared by readers. No allocation, default seeding or JVM. */
typedef struct mc_noise_perlin {
    double offset_x;
    double offset_y;
    double offset_z;
    uint16_t permutation[512];
} mc_noise_perlin;

typedef struct mc_noise_grid {
    double origin_x;
    double origin_y;
    double origin_z;
    size_t size_x;
    size_t size_y;
    size_t size_z;
    double scale_x;
    double scale_y;
    double scale_z;
} mc_noise_grid;

/* The caller retains the layer array. Do not change layers/count after init. */
typedef struct mc_noise_octaves {
    mc_noise_perlin *layers;
    size_t count;
} mc_noise_octaves;

/* Null arguments fail without changing the state or generator. Otherwise
 * consumes the supplied Random in original constructor order. State/generator
 * storage must not overlap. */
bool mc_noise_perlin_init(mc_noise_perlin *noise, mc_java_random *random);

/* Requires nonnull initialized state. This is the original scalar 3D method;
 * its scalar 2D overload is sample(x, y, 0), distinct from a height-one grid.
 * Coordinates use Java narrowing and wrapping floor adjustment. Nonfinite
 * inputs have no NaN payload identity guarantee. */
double mc_noise_perlin_sample(const mc_noise_perlin *noise, double x, double y, double z);

/* Add samples/divisor to the first size_x*size_y*size_z values, in X/Z/Y
 * order. Existing output values and any suffix are retained. The height-one
 * path ignores origin_y/scale_y/offset_y; the general path retains original
 * Y-cell gradient caching. It is not repeated scalar sampling.
 * Dimensions and their product must fit INT32_MAX; the complete output span
 * must fit SIZE_MAX/sizeof(double). Zero-size grids are successful no-ops.
 * Null state/grid, invalid dimensions or insufficient capacity return false
 * without changing output. Output may be null only when its length is zero.
 * Output storage must not overlap state or grid. */
bool mc_noise_perlin_add(const mc_noise_perlin *noise, const mc_noise_grid *grid, double divisor,
                         double *output, size_t output_length);

/* Count must fit INT32_MAX. Zero layers may use null storage and generator.
 * Invalid arguments leave the result, layers and generator unchanged. All
 * three storage areas must be disjoint. Positive count consumes one sampler
 * constructor after another from the same Random. */
bool mc_noise_octaves_init(mc_noise_octaves *noise, mc_noise_perlin *layers, size_t count,
                           mc_java_random *random);

/* Requires initialized state. Original scalar octave sum: each layer samples
 * (x*frequency, y*frequency, 0), divides by frequency, then halves frequency. */
double mc_noise_octaves_sample2(const mc_noise_octaves *noise, double x, double y);

/* Same grid/capacity rules as perlin_add. Clears the entire supplied output
 * span, including any suffix, before accumulating layers in original order.
 * Zero octaves still clear output. Invalid arguments preserve all output.
 * Output must not overlap octave state, its layers or grid. */
bool mc_noise_octaves_fill(const mc_noise_octaves *noise, const mc_noise_grid *grid, double *output,
                           size_t output_length);

#endif
