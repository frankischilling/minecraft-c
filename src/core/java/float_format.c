#include "mc/core/java/float_format.h"

#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8 && sizeof(double) == 8 && FLT_RADIX == 2 && DBL_MANT_DIG == 53 &&
                   DBL_MIN_EXP == -1021 && DBL_MAX_EXP == 1024,
               "Float formatting requires IEEE binary64");

_Static_assert(sizeof(float) == sizeof(uint32_t) && FLT_MANT_DIG == 24 && FLT_MIN_EXP == -125 &&
                   FLT_MAX_EXP == 128,
               "Float formatting requires IEEE binary32 storage");

enum { LIMBS = 16 };

typedef struct integer {
    uint32_t limb[LIMBS];
} integer;

typedef struct scale {
    integer grid, margin;
    unsigned int width;
} scale;

static integer small(uint64_t value) {
    integer result = {{0}};
    result.limb[0] = (uint32_t)value;
    result.limb[1] = (uint32_t)(value >> 32);
    return result;
}

static unsigned int bits64(uint64_t value) {
    unsigned int count = 0;
    while (value != 0) {
        ++count;
        value >>= 1;
    }
    return count;
}

static size_t bit_count(const integer *value) {
    for (size_t i = LIMBS; i != 0; --i)
        if (value->limb[i - 1] != 0)
            return (i - 1) * 32u + bits64(value->limb[i - 1]);
    return 0;
}

static int compare(const integer *left, const integer *right) {
    for (size_t i = LIMBS; i != 0; --i) {
        if (left->limb[i - 1] != right->limb[i - 1])
            return left->limb[i - 1] < right->limb[i - 1] ? -1 : 1;
    }
    return 0;
}

static bool shift(integer *value, unsigned int count) {
    integer result = {{0}};
    const size_t words = count / 32u;
    const unsigned int tail = count % 32u;
    for (size_t i = 0; i < LIMBS; ++i) {
        if (value->limb[i] == 0)
            continue;
        if (words >= LIMBS || i >= LIMBS - words)
            return false;
        const size_t destination = i + words;
        result.limb[destination] |= (uint32_t)((uint64_t)value->limb[i] << tail);
        if (tail != 0) {
            const uint32_t carry = value->limb[i] >> (32u - tail);
            if (carry != 0) {
                if (destination + 1u >= LIMBS)
                    return false;
                result.limb[destination + 1u] |= carry;
            }
        }
    }
    *value = result;
    return true;
}

static bool times_small(integer *value, uint32_t multiplier) {
    integer result = {{0}};
    uint64_t carry = 0;
    for (size_t i = 0; i < LIMBS; ++i) {
        const uint64_t next = (uint64_t)value->limb[i] * multiplier + carry;
        result.limb[i] = (uint32_t)next;
        carry = next >> 32;
    }
    if (carry != 0)
        return false;
    *value = result;
    return true;
}

static bool times_power(integer *value, uint32_t base, unsigned int exponent) {
    for (unsigned int i = 0; i < exponent; ++i)
        if (!times_small(value, base))
            return false;
    return true;
}

static bool multiply(const integer *left, const integer *right, integer *out) {
    uint32_t work[LIMBS * 2] = {0};
    for (size_t i = 0; i < LIMBS; ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < LIMBS; ++j) {
            const uint64_t next = (uint64_t)left->limb[i] * right->limb[j] + work[i + j] + carry;
            work[i + j] = (uint32_t)next;
            carry = next >> 32;
        }
        work[i + LIMBS] = (uint32_t)carry;
    }
    for (size_t i = LIMBS; i < LIMBS * 2u; ++i)
        if (work[i] != 0)
            return false;
    memcpy(out->limb, work, sizeof(out->limb));
    return true;
}

static bool add(const integer *left, const integer *right, integer *out) {
    integer result = {{0}};
    uint64_t carry = 0;
    for (size_t i = 0; i < LIMBS; ++i) {
        const uint64_t next = (uint64_t)left->limb[i] + right->limb[i] + carry;
        result.limb[i] = (uint32_t)next;
        carry = next >> 32;
    }
    if (carry != 0)
        return false;
    *out = result;
    return true;
}

static void subtract(integer *left, const integer *right) {
    uint64_t borrow = 0;
    for (size_t i = 0; i < LIMBS; ++i) {
        const uint64_t subtrahend = (uint64_t)right->limb[i] + borrow;
        const uint64_t original = left->limb[i];
        left->limb[i] = (uint32_t)(original - subtrahend);
        borrow = original < subtrahend ? 1u : 0u;
    }
}

static bool divide(const integer *numerator, const integer *denominator, integer *quotient,
                   integer *remainder) {
    if (bit_count(denominator) == 0)
        return false;
    integer q = {{0}}, r = {{0}};
    for (size_t position = bit_count(numerator); position != 0; --position) {
        const size_t bit = position - 1;
        if (!shift(&r, 1))
            return false;
        r.limb[0] |= (numerator->limb[bit / 32u] >> (bit % 32u)) & 1u;
        if (compare(&r, denominator) >= 0) {
            subtract(&r, denominator);
            q.limb[bit / 32u] |= UINT32_C(1) << (bit % 32u);
        }
    }
    *quotient = q;
    *remainder = r;
    return true;
}

static bool as64(const integer *value, uint64_t *out) {
    for (size_t i = 2; i < LIMBS; ++i)
        if (value->limb[i] != 0)
            return false;
    *out = value->limb[0] | (uint64_t)value->limb[1] << 32;
    return true;
}

static double binary64(uint64_t bits) {
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool estimate(uint32_t m, unsigned int precision, int exponent, int *out) {
    volatile double normalized = (double)m / (double)(UINT32_C(1) << (precision - 1u));
    volatile double centered = normalized - 1.5;
    volatile double slope = centered * binary64(UINT64_C(0x3fd287a762c9bd93));
    volatile double intercept = slope + binary64(UINT64_C(0x3fc68a288b421b8c));
    volatile double exponent_term = (double)exponent * binary64(UINT64_C(0x3fd34413509f79fb));
    volatile double combined = intercept + exponent_term;
    if (!(combined >= -46.0 && combined < 40.0))
        return false;
    int order = (int)combined;
    if (combined < (double)order)
        --order;
    *out = order;
    return order >= -45 && order <= 38;
}

static int five_bound(unsigned int exponent) {
    if (exponent >= 27u)
        return (int)(3u * exponent);
    uint64_t value = 1;
    for (unsigned int i = 0; i < exponent; ++i)
        value *= 5u;
    return (int)bits64(value - 1u);
}

static bool make_scale(uint32_t odd, int exponent, unsigned int precision, unsigned int t,
                       int order, scale *out) {
    const int tiny = (int)t - exponent - 1 > 0 ? (int)t - exponent - 1 : 0;
    const unsigned int a = order < 0 ? (unsigned int)-order : 0u;
    const unsigned int d = order > 0 ? (unsigned int)order : 0u;
    int u = (int)a + tiny + exponent - (int)t + 1;
    int v = (int)d + tiny;
    int z = (int)a + tiny + exponent - (int)precision - (t == 1u ? 1 : 0);
    const int common = u < v ? (u < z ? u : z) : (v < z ? v : z);
    u -= common;
    v -= common;
    z -= common;
    if (u < 0 || v < 0 || z < 0)
        return false;
    integer numerator = small(odd), denominator = small(1), margin = small(1);
    if (!times_power(&numerator, 5, a) || !shift(&numerator, (unsigned int)u) ||
        !times_power(&denominator, 5, d) || !shift(&denominator, (unsigned int)v) ||
        !times_power(&margin, 5, a) || !shift(&margin, (unsigned int)z))
        return false;
    const int numerator_bound = (int)t + u + five_bound(a);
    const int grid_bound = v + 1 + five_bound(d + 1u);
    const int maximum = numerator_bound > grid_bound ? numerator_bound : grid_bound;
    const unsigned int width = maximum < 32 ? 32u : maximum < 64 ? 64u : 0u;
    if (!times_small(&denominator, 10))
        return false;
    if (width != 0) {
        uint64_t b, g, m;
        const uint64_t limit = UINT64_C(1) << (width - 1u);
        if (!as64(&numerator, &b) || !as64(&denominator, &g) || !as64(&margin, &m) || b == 0 ||
            g == 0 || m == 0 || b >= limit || g >= limit || m >= limit)
            return false;
    }
    *out = (scale){.grid = denominator, .margin = margin, .width = width};
    return true;
}

static bool ratio(uint32_t m, int binary_exponent, int decimal_power, integer *numerator,
                  integer *denominator) {
    integer n = small(m), d = small(1);
    int binary;
    if (decimal_power >= 0) {
        binary = binary_exponent - decimal_power;
        if (!times_power(&d, 5, (unsigned int)decimal_power))
            return false;
    } else {
        const unsigned int a = (unsigned int)-decimal_power;
        binary = binary_exponent + (int)a;
        if (!times_power(&n, 5, a))
            return false;
    }
    if (binary >= 0) {
        if (!shift(&n, (unsigned int)binary))
            return false;
    } else if (!shift(&d, (unsigned int)-binary)) {
        return false;
    }
    *numerator = n;
    *denominator = d;
    return true;
}

static size_t decimal(uint64_t value, size_t minimum, char out[24]) {
    char reversed[24];
    size_t count = 0;
    do {
        reversed[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0);
    while (count < minimum)
        reversed[count++] = '0';
    for (size_t i = 0; i < count; ++i)
        out[i] = reversed[count - i - 1u];
    return count;
}

static bool emit(mc_java_float_string *result, char unit) {
    if (result->length + 1u >= sizeof(result->text))
        return false;
    result->text[result->length++] = unit;
    result->text[result->length] = '\0';
    return true;
}

static bool layout(const char *digits, size_t count, int order, bool negative,
                   mc_java_float_string *result) {
    if (count == 0 || count > 20u || order < -45 || order > 39)
        return false;
    if (negative && !emit(result, '-'))
        return false;
    if (order >= -3 && order <= 6) {
        const int point = order + 1;
        if (point <= 0) {
            if (!emit(result, '0') || !emit(result, '.'))
                return false;
            for (int i = 0; i < -point; ++i)
                if (!emit(result, '0'))
                    return false;
            for (size_t i = 0; i < count; ++i)
                if (!emit(result, digits[i]))
                    return false;
        } else {
            for (size_t i = 0; i < (size_t)point; ++i)
                if (!emit(result, i < count ? digits[i] : '0'))
                    return false;
            if (!emit(result, '.'))
                return false;
            if (count <= (size_t)point) {
                if (!emit(result, '0'))
                    return false;
            } else {
                for (size_t i = (size_t)point; i < count; ++i)
                    if (!emit(result, digits[i]))
                        return false;
            }
        }
    } else {
        if (!emit(result, digits[0]) || !emit(result, '.'))
            return false;
        if (count == 1) {
            if (!emit(result, '0'))
                return false;
        } else {
            for (size_t i = 1; i < count; ++i)
                if (!emit(result, digits[i]))
                    return false;
        }
        if (!emit(result, 'E') || (order < 0 && !emit(result, '-')))
            return false;
        char exponent[24];
        const size_t length = decimal((uint64_t)(order < 0 ? -order : order), 1, exponent);
        for (size_t i = 0; i < length; ++i)
            if (!emit(result, exponent[i]))
                return false;
    }
    return true;
}

static bool integer_result(uint32_t odd, unsigned int t, int exponent, unsigned int precision,
                           bool negative, mc_java_float_string *result) {
    const int shift_count = exponent - (int)t + 1;
    if (shift_count < 0 || shift_count >= 64)
        return false;
    const uint64_t x = (uint64_t)odd << (unsigned int)shift_count;
    const int gap = exponent - (int)precision - 1;
    uint64_t power = UINT64_C(1) << (gap > 0 ? (unsigned int)gap : 0u);
    unsigned int places = 0;
    while (power >= 10u) {
        power /= 10u;
        ++places;
    }
    uint64_t quantum = 1;
    for (unsigned int i = 0; i < places; ++i)
        quantum *= 10u;
    uint64_t coefficient = x / quantum;
    if (2u * (x % quantum) >= quantum)
        ++coefficient;
    if (coefficient > UINT64_MAX / quantum)
        return false;
    char digits[24];
    size_t count = decimal(coefficient * quantum, 1, digits);
    const int order = (int)count - 1;
    while (count > 1u && digits[count - 1u] == '0')
        --count;
    return layout(digits, count, order, negative, result);
}

static bool predicates(const scale *profile, const integer *residual, unsigned int attempt,
                       bool *lower, bool *upper, int *difference) {
    if (profile->width != 0) {
        uint64_t r, g, margin;
        if (!as64(residual, &r) || !as64(&profile->grid, &g) || !as64(&profile->margin, &margin))
            return false;
        const uint64_t mask = profile->width == 32u ? UINT32_MAX : UINT64_MAX;
        const uint64_t sign = UINT64_C(1) << (profile->width - 1u);
        for (unsigned int i = 0; i < attempt; ++i)
            margin = (margin * 10u) & mask;
        const bool positive = margin != 0 && (margin & sign) == 0;
        if (attempt >= 2u && !positive) {
            *lower = *upper = true;
        } else {
            const uint64_t sum = (r + margin) & mask;
            *lower = positive && r < margin;
            *upper = (sum & sign) == 0 && sum > g;
        }
        const uint64_t delta = (r + r - g) & mask;
        *difference = delta == 0 ? 0 : (delta & sign) != 0 ? -1 : 1;
        return true;
    }
    integer margin = profile->margin, sum, doubled = *residual;
    if (!times_power(&margin, 10, attempt) || !add(residual, &margin, &sum) || !shift(&doubled, 1))
        return false;
    *lower = compare(residual, &margin) < 0;
    *upper = compare(&sum, &profile->grid) >= 0;
    *difference = compare(&doubled, &profile->grid);
    return true;
}

mc_java_float_format_status mc_java_float_to_string(float value, mc_java_float_string *out) {
    if (out == NULL)
        return MC_JAVA_FLOAT_FORMAT_INVALID_ARGUMENT;
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    mc_java_float_string result;
    memset(&result, 0, sizeof(result));
    const uint32_t magnitude = bits & UINT32_C(0x7fffffff);
    const bool negative = (bits >> 31) != 0;
    if (magnitude == 0 || magnitude >= UINT32_C(0x7f800000)) {
        const char *text = magnitude > UINT32_C(0x7f800000) ? "NaN"
                           : magnitude == 0                 ? "0.0"
                                                            : "Infinity";
        if (negative && magnitude <= UINT32_C(0x7f800000) && !emit(&result, '-'))
            return MC_JAVA_FLOAT_FORMAT_INTERNAL_LIMIT;
        for (size_t i = 0; text[i] != '\0'; ++i)
            if (!emit(&result, text[i]))
                return MC_JAVA_FLOAT_FORMAT_INTERNAL_LIMIT;
        *out = result;
        return MC_JAVA_FLOAT_FORMAT_OK;
    }
    const uint32_t field = magnitude >> 23;
    const uint32_t m = (magnitude & UINT32_C(0x7fffff)) | (field == 0 ? 0 : UINT32_C(0x800000));
    const unsigned int precision = field == 0 ? bits64(m) : 24u;
    const int exponent = field == 0 ? (int)precision - 150 : (int)field - 127;
    uint32_t odd = m;
    while ((odd & 1u) == 0)
        odd >>= 1;
    const unsigned int t = bits64(odd);
    if (exponent <= 62 && (int)t <= exponent + 1) {
        if (!integer_result(odd, t, exponent, precision, negative, &result))
            return MC_JAVA_FLOAT_FORMAT_INTERNAL_LIMIT;
        *out = result;
        return MC_JAVA_FLOAT_FORMAT_OK;
    }
    int order;
    scale profile;
    if (!estimate(m, precision, exponent, &order) ||
        !make_scale(odd, exponent, precision, t, order, &profile))
        return MC_JAVA_FLOAT_FORMAT_INTERNAL_LIMIT;
    unsigned int discarded = 0;
    for (unsigned int attempt = 1; attempt <= 10u; ++attempt) {
        integer numerator, denominator, quotient, remainder, product, residual, exact_remainder;
        const int decimal_power = order - (int)attempt + 1;
        if (!ratio(m, exponent - (int)precision + 1, decimal_power, &numerator, &denominator) ||
            !divide(&numerator, &denominator, &quotient, &remainder) ||
            !multiply(&profile.grid, &remainder, &product) ||
            !divide(&product, &denominator, &residual, &exact_remainder) ||
            bit_count(&exact_remainder) != 0)
            return MC_JAVA_FLOAT_FORMAT_INTERNAL_LIMIT;
        uint64_t coefficient;
        bool lower, upper;
        int difference;
        if (!as64(&quotient, &coefficient) ||
            !predicates(&profile, &residual, attempt, &lower, &upper, &difference))
            return MC_JAVA_FLOAT_FORMAT_INTERNAL_LIMIT;
        if (attempt == 1u) {
            if (coefficient > 9u)
                return MC_JAVA_FLOAT_FORMAT_INTERNAL_LIMIT;
            discarded = coefficient == 0 && !upper ? 1u : 0u;
            const int displayed = order - (int)discarded;
            if (displayed < -3 || displayed >= 8)
                lower = upper = false;
        }
        if (!lower && !upper)
            continue;
        const unsigned int count = attempt - discarded;
        if (count == 0 || count > 10u)
            return MC_JAVA_FLOAT_FORMAT_INTERNAL_LIMIT;
        int displayed = order - (int)discarded;
        if (upper && (!lower || difference > 0 || (difference == 0 && (coefficient & 1u) != 0)))
            ++coefficient;
        uint64_t limit = 1;
        for (unsigned int i = 0; i < count; ++i)
            limit *= 10u;
        if (coefficient == limit) {
            coefficient /= 10u;
            ++displayed;
        }
        char digits[24];
        const size_t length = decimal(coefficient, count, digits);
        if (length != count || !layout(digits, length, displayed, negative, &result))
            return MC_JAVA_FLOAT_FORMAT_INTERNAL_LIMIT;
        *out = result;
        return MC_JAVA_FLOAT_FORMAT_OK;
    }
    return MC_JAVA_FLOAT_FORMAT_INTERNAL_LIMIT;
}
