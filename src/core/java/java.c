#include "mc/core/java/java.h"

#include "mc/core/math/strict.h"

#include <float.h>
#include <limits.h>
#include <stddef.h>

_Static_assert(CHAR_BIT == 8, "Java compatibility requires eight-bit bytes");
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128,
               "Java compatibility requires binary32 float");
_Static_assert(DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024,
               "Java compatibility requires binary64 double");

/* Converting an unsigned value outside the signed range is implementation
 * defined in C17. Split the range so every signed conversion is representable. */
static int32_t i32_from_bits(uint32_t value)
{
    if (value <= INT32_MAX) {
        return (int32_t)value;
    }
    return -1 - (int32_t)(UINT32_MAX - value);
}

static int64_t i64_from_bits(uint64_t value)
{
    if (value <= INT64_MAX) {
        return (int64_t)value;
    }
    return -1 - (int64_t)(UINT64_MAX - value);
}

int32_t mc_java_i32_add(int32_t a, int32_t b)
{
    return i32_from_bits((uint32_t)a + (uint32_t)b);
}

int32_t mc_java_i32_sub(int32_t a, int32_t b)
{
    return i32_from_bits((uint32_t)a - (uint32_t)b);
}

int32_t mc_java_i32_mul(int32_t a, int32_t b)
{
    return i32_from_bits((uint32_t)a * (uint32_t)b);
}

int32_t mc_java_i32_neg(int32_t value)
{
    return i32_from_bits(UINT32_C(0) - (uint32_t)value);
}

int64_t mc_java_i64_add(int64_t a, int64_t b)
{
    return i64_from_bits((uint64_t)a + (uint64_t)b);
}

int64_t mc_java_i64_sub(int64_t a, int64_t b)
{
    return i64_from_bits((uint64_t)a - (uint64_t)b);
}

int64_t mc_java_i64_mul(int64_t a, int64_t b)
{
    return i64_from_bits((uint64_t)a * (uint64_t)b);
}

int64_t mc_java_i64_neg(int64_t value)
{
    return i64_from_bits(UINT64_C(0) - (uint64_t)value);
}

int32_t mc_java_i32_shl(int32_t value, int32_t distance)
{
    unsigned int shift = (uint32_t)distance & 31u;
    return i32_from_bits((uint32_t)value << shift);
}

int32_t mc_java_i32_shr(int32_t value, int32_t distance)
{
    unsigned int shift = (uint32_t)distance & 31u;
    uint32_t bits = (uint32_t)value >> shift;
    if (value < 0 && shift != 0) {
        bits |= UINT32_MAX << (32u - shift);
    }
    return i32_from_bits(bits);
}

int32_t mc_java_i32_ushr(int32_t value, int32_t distance)
{
    return i32_from_bits((uint32_t)value >> ((uint32_t)distance & 31u));
}

int64_t mc_java_i64_shl(int64_t value, int32_t distance)
{
    unsigned int shift = (uint32_t)distance & 63u;
    return i64_from_bits((uint64_t)value << shift);
}

int64_t mc_java_i64_shr(int64_t value, int32_t distance)
{
    unsigned int shift = (uint32_t)distance & 63u;
    uint64_t bits = (uint64_t)value >> shift;
    if (value < 0 && shift != 0) {
        bits |= UINT64_MAX << (64u - shift);
    }
    return i64_from_bits(bits);
}

int64_t mc_java_i64_ushr(int64_t value, int32_t distance)
{
    return i64_from_bits((uint64_t)value >> ((uint32_t)distance & 63u));
}

int8_t mc_java_i8_narrow(int64_t value)
{
    uint8_t bits = (uint8_t)value;
    return bits <= INT8_MAX ? (int8_t)bits : (int8_t)(-1 - (int)(UINT8_MAX - bits));
}

int16_t mc_java_i16_narrow(int64_t value)
{
    uint16_t bits = (uint16_t)value;
    return bits <= INT16_MAX ? (int16_t)bits : (int16_t)(-1 - (int)(UINT16_MAX - bits));
}

uint16_t mc_java_u16_narrow(int64_t value)
{
    return (uint16_t)value;
}

int32_t mc_java_i32_narrow(int64_t value)
{
    return i32_from_bits((uint32_t)value);
}

bool mc_java_i32_div(int32_t a, int32_t b, int32_t *out)
{
    if (b == 0 || out == NULL) {
        return false;
    }
    *out = a == INT32_MIN && b == -1 ? INT32_MIN : a / b;
    return true;
}

bool mc_java_i32_rem(int32_t a, int32_t b, int32_t *out)
{
    if (b == 0 || out == NULL) {
        return false;
    }
    *out = a == INT32_MIN && b == -1 ? 0 : a % b;
    return true;
}

bool mc_java_i64_div(int64_t a, int64_t b, int64_t *out)
{
    if (b == 0 || out == NULL) {
        return false;
    }
    *out = a == INT64_MIN && b == -1 ? INT64_MIN : a / b;
    return true;
}

bool mc_java_i64_rem(int64_t a, int64_t b, int64_t *out)
{
    if (b == 0 || out == NULL) {
        return false;
    }
    *out = a == INT64_MIN && b == -1 ? 0 : a % b;
    return true;
}

int32_t mc_java_double_to_i32(double value)
{
    if (value != value) {
        return 0;
    }
    if (value >= 0x1p31) {
        return INT32_MAX;
    }
    if (value <= -0x1p31) {
        return INT32_MIN;
    }
    return (int32_t)value;
}

int64_t mc_java_double_to_i64(double value)
{
    if (value != value) {
        return 0;
    }
    if (value >= 0x1p63) {
        return INT64_MAX;
    }
    if (value <= -0x1p63) {
        return INT64_MIN;
    }
    return (int64_t)value;
}

int32_t mc_java_float_to_i32(float value)
{
    return mc_java_double_to_i32((double)value);
}

int64_t mc_java_float_to_i64(float value)
{
    return mc_java_double_to_i64((double)value);
}

#define RANDOM_MULTIPLIER UINT64_C(0x5deece66d)
#define RANDOM_MASK UINT64_C(0xffffffffffff)

void mc_java_random_seed(mc_java_random *random, int64_t seed)
{
    random->state = ((uint64_t)seed ^ RANDOM_MULTIPLIER) & RANDOM_MASK;
    random->next_next_gaussian = 0.0;
    random->have_next_next_gaussian = false;
}

void mc_java_random_reseed(mc_java_random *random, int64_t seed)
{
    random->state = ((uint64_t)seed ^ RANDOM_MULTIPLIER) & RANDOM_MASK;
    random->have_next_next_gaussian = false;
}

static uint32_t random_next_bits(mc_java_random *random, unsigned int bits)
{
    random->state = (random->state * RANDOM_MULTIPLIER + 11u) & RANDOM_MASK;
    return (uint32_t)(random->state >> (48u - bits));
}

int32_t mc_java_random_next_i32(mc_java_random *random)
{
    return i32_from_bits(random_next_bits(random, 32));
}

bool mc_java_random_next_bounded(mc_java_random *random, int32_t bound, int32_t *out)
{
    uint32_t bits;
    uint32_t value;
    if (bound <= 0 || out == NULL) {
        return false;
    }
    bits = random_next_bits(random, 31);
    if (((uint32_t)bound & ((uint32_t)bound - 1u)) == 0) {
        *out = (int32_t)(((uint64_t)(uint32_t)bound * bits) >> 31);
        return true;
    }
    for (;;) {
        value = bits % (uint32_t)bound;
        /* This is the sign test of Java's wrapping int expression
         * bits - value + (bound - 1), not an unbounded integer comparison. */
        if (bits - value + ((uint32_t)bound - 1u) <= INT32_MAX) {
            *out = (int32_t)value;
            return true;
        }
        bits = random_next_bits(random, 31);
    }
}

int64_t mc_java_random_next_i64(mc_java_random *random)
{
    int32_t high = mc_java_random_next_i32(random);
    int32_t low = mc_java_random_next_i32(random);
    /* nextLong adds a sign-extended second int, rather than concatenating it. */
    return mc_java_i64_add(mc_java_i64_shl((int64_t)high, 32), (int64_t)low);
}

float mc_java_random_next_float(mc_java_random *random)
{
    return (float)random_next_bits(random, 24) * 0x1p-24f;
}

double mc_java_random_next_double(mc_java_random *random)
{
    uint64_t high = random_next_bits(random, 26);
    uint64_t low = random_next_bits(random, 27);
    return (double)((high << 27) + low) * 0x1p-53;
}

double mc_java_random_next_gaussian(mc_java_random *random)
{
    if (random->have_next_next_gaussian) {
        random->have_next_next_gaussian = false;
        return random->next_next_gaussian;
    }
    double v1, v2, s;
    do {
        const double first_draw = mc_java_random_next_double(random);
        const double first_scaled = 2.0 * first_draw;
        v1 = first_scaled - 1.0;
        const double second_draw = mc_java_random_next_double(random);
        const double second_scaled = 2.0 * second_draw;
        v2 = second_scaled - 1.0;
        const double first_square = v1 * v1;
        const double second_square = v2 * v2;
        s = first_square + second_square;
    } while (s >= 1.0 || s == 0.0);
    const double logarithm = mc_math_strict_log(s);
    const double scaled_logarithm = -2.0 * logarithm;
    const double ratio = scaled_logarithm / s;
    const double multiplier = mc_math_strict_sqrt(ratio);
    random->next_next_gaussian = v2 * multiplier;
    random->have_next_next_gaussian = true;
    return v1 * multiplier;
}
