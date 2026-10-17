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
} mc_java_random;

void mc_java_random_seed(mc_java_random *random, int64_t seed);
int32_t mc_java_random_next_i32(mc_java_random *random);
int64_t mc_java_random_next_i64(mc_java_random *random);
float mc_java_random_next_float(mc_java_random *random);
double mc_java_random_next_double(mc_java_random *random);
/* Invalid bound (<= 0) or null output returns false without consuming randomness. */
bool mc_java_random_next_bounded(mc_java_random *random, int32_t bound, int32_t *out);

#endif
