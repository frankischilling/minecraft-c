#ifndef MC_JAVA_H
#define MC_JAVA_H

#include <stdbool.h>
#include <stdint.h>

/* Java integer operations, including overflow and masked shift distances. */
int32_t mc_java_i32_add(int32_t a, int32_t b);
int32_t mc_java_i32_sub(int32_t a, int32_t b);
int32_t mc_java_i32_mul(int32_t a, int32_t b);
int32_t mc_java_i32_neg(int32_t value);
int64_t mc_java_i64_add(int64_t a, int64_t b);
int64_t mc_java_i64_sub(int64_t a, int64_t b);
int64_t mc_java_i64_mul(int64_t a, int64_t b);
int64_t mc_java_i64_neg(int64_t value);
int32_t mc_java_i32_shl(int32_t value, int32_t distance);
int32_t mc_java_i32_shr(int32_t value, int32_t distance);
int32_t mc_java_i32_ushr(int32_t value, int32_t distance);
int64_t mc_java_i64_shl(int64_t value, int32_t distance);
int64_t mc_java_i64_shr(int64_t value, int32_t distance);
int64_t mc_java_i64_ushr(int64_t value, int32_t distance);

int8_t mc_java_i8_narrow(int64_t value);
int16_t mc_java_i16_narrow(int64_t value);
uint16_t mc_java_u16_narrow(int64_t value); /* Java char. */
int32_t mc_java_i32_narrow(int64_t value);

/* Return false on zero divisor or null output, leaving output unchanged.
 * Callers decide how the reference's ArithmeticException affects game state. */
bool mc_java_i32_div(int32_t a, int32_t b, int32_t *out);
bool mc_java_i32_rem(int32_t a, int32_t b, int32_t *out);
bool mc_java_i64_div(int64_t a, int64_t b, int64_t *out);
bool mc_java_i64_rem(int64_t a, int64_t b, int64_t *out);

/* Truncate toward zero; NaN becomes zero and out-of-range values saturate. */
int32_t mc_java_double_to_i32(double value);
int64_t mc_java_double_to_i64(double value);
int32_t mc_java_float_to_i32(float value);
int64_t mc_java_float_to_i64(float value);

/* Explicitly seeded java.util.Random sequence. Generator pointers must be nonnull.
 * Seed before drawing. One owner per instance;
 * concurrent access requires external synchronization. State is opaque to users. */
typedef struct mc_java_random {
    uint64_t state;
    double next_next_gaussian;
    bool have_next_next_gaussian;
} mc_java_random;

/* Fully initialize a fresh native seeded value, including +0.0/false cache
 * fields. Accepts uninitialized storage and consumes no constructor entropy. */
void mc_java_random_seed(mc_java_random *random, int64_t seed);
/* Java Random.setSeed on an initialized owner: reset the LCG and clear the
 * cache-valid flag while preserving the stored Gaussian double's bits. */
void mc_java_random_reseed(mc_java_random *random, int64_t seed);
int32_t mc_java_random_next_i32(mc_java_random *random);
int64_t mc_java_random_next_i64(mc_java_random *random);
float mc_java_random_next_float(mc_java_random *random);
double mc_java_random_next_double(mc_java_random *random);
/* Ordinary Random's cached polar algorithm. Requires an initialized owner and
 * the captured x86-64/SSE2 StrictMath backend environment: round-to-nearest,
 * ties-to-even binary64 with gradual underflow, no FTZ/DAZ or excess intermediate
 * precision, disabled floating traps, no reassociation and no FMA contraction.
 * Uses concrete log/sqrt on positive finite arguments. A cache hit clears its
 * valid flag and returns the stored double without drawing. Other draw methods
 * do not invalidate that cache. No rejection cap, callback, constructor entropy
 * or implicit synchronization is supplied. */
double mc_java_random_next_gaussian(mc_java_random *random);
/* Invalid bound (<= 0) or null output returns false without consuming randomness. */
bool mc_java_random_next_bounded(mc_java_random *random, int32_t bound, int32_t *out);

#endif
