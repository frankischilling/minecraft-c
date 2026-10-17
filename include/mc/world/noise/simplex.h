#ifndef MC_WORLD_NOISE_SIMPLEX_H
#define MC_WORLD_NOISE_SIMPLEX_H

#include "mc/core/java/java.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Original initialized algorithm constants, pinned to observed binary64 bits. */
extern const double mc_noise_simplex_skew;
extern const double mc_noise_simplex_unskew;
#define MC_NOISE_SIMPLEX_DEFAULT_DIVISOR_MULTIPLIER 0.5

/* Caller-owned seeded state. Constructor draws X, Z, then an unused third
 * offset before shuffling. Preserve that draw for following generators.
 * Treat fields as immutable after initialization; sampling permits readers. */
typedef struct mc_noise_simplex {
    double offset_x;
    double offset_z;
    double offset_unused;
    uint16_t permutation[512];
} mc_noise_simplex;

typedef struct mc_noise_simplex_grid {
    double origin_x;
    double origin_z;
    size_t size_x;
    size_t size_z;
    double scale_x;
    double scale_z;
} mc_noise_simplex_grid;

/* The caller retains the layer array. Keep layers/count unchanged after init. */
typedef struct mc_noise_simplex_octaves {
    mc_noise_simplex *layers;
    size_t count;
} mc_noise_simplex_octaves;

/* Null arguments fail without changing state or Random. Storage must be
 * disjoint. Otherwise consumes the original explicit-Random constructor. */
bool mc_noise_simplex_init(mc_noise_simplex *noise, mc_java_random *random);

/* Add original 2D samples times amplitude into the first size_x*size_z values,
 * X outermost, Z innermost. Retains existing values and any suffix. Coordinates
 * are (origin+index)*scale+offset. The original floor and wrapped cell-sum
 * behavior are preserved; this is not a 3D or scalar original method.
 * Both dimensions/product must fit INT32_MAX, capacity must cover the product,
 * and output_length must fit SIZE_MAX/sizeof(double). Zero-size grids are no-ops.
 * Null state/grid, invalid dimensions or insufficient capacity return false
 * without changing output. Null output is allowed only with zero length.
 * Output must not overlap state or grid; state must be initialized. */
bool mc_noise_simplex_add(const mc_noise_simplex *noise, const mc_noise_simplex_grid *grid,
                          double amplitude, double *output, size_t output_length);

/* Count must fit INT32_MAX and its layer span must be representable. Zero
 * layers may use null storage/Random. Invalid arguments preserve all state.
 * Result, layers and Random storage must be disjoint. */
bool mc_noise_simplex_octaves_init(mc_noise_simplex_octaves *noise, mc_noise_simplex *layers,
                                   size_t count, mc_java_random *random);

/* Clear the entire supplied output span (including its suffix), then fill
 * using each initial scale divided by 1.5. Frequency and amplitude divisor
 * begin at 1; each layer uses scaled frequency and amplitude 0.55/divisor,
 * then multiplies these by the respective caller factors. The original short
 * overload uses MC_NOISE_SIMPLEX_DEFAULT_DIVISOR_MULTIPLIER for the second factor.
 * Zero layers still clear. Same capacity/failure rules as simplex_add; output
 * must not overlap octave state, its layers or grid. No allocations.
 * Exact comparisons assume binary64 round-to-nearest on tested platforms.
 * Nonfinite inputs have no NaN payload identity guarantee. */
bool mc_noise_simplex_octaves_fill(const mc_noise_simplex_octaves *noise,
                                   const mc_noise_simplex_grid *grid, double frequency_multiplier,
                                   double amplitude_divisor_multiplier, double *output,
                                   size_t output_length);

#endif
