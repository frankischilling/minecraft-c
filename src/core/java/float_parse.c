#include "mc/core/java/float_parse.h"

#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8 && sizeof(uint16_t) == 2, "counted UTF-16 byte budget");
_Static_assert(sizeof(float) == sizeof(uint32_t) && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                   FLT_MAX_EXP == 128,
               "Float parsing requires IEEE binary32 storage");

enum { LIMBS = 64, PREFIX_DIGITS = 192, EXPONENT_CAP = 1000000000 };

typedef struct integer {
    uint32_t limb[LIMBS];
    size_t used;
} integer;

typedef struct magnitude {
    integer numerator, denominator;
    int32_t binary_exponent;
    bool sticky;
} magnitude;

static bool multiply(integer *value, uint32_t small) {
    uint64_t carry = 0;
    for (size_t i = 0; i < value->used; ++i) {
        const uint64_t product = (uint64_t)value->limb[i] * small + carry;
        value->limb[i] = (uint32_t)product;
        carry = product >> 32;
    }
    if (carry != 0) {
        if (value->used == LIMBS)
            return false;
        value->limb[value->used++] = (uint32_t)carry;
    }
    return true;
}

static bool append_digit(integer *value, unsigned int base, unsigned int digit) {
    if (!multiply(value, base))
        return false;
    uint64_t carry = digit;
    for (size_t i = 0; carry != 0 && i < value->used; ++i) {
        const uint64_t sum = (uint64_t)value->limb[i] + carry;
        value->limb[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
    if (carry != 0) {
        if (value->used == LIMBS)
            return false;
        value->limb[value->used++] = (uint32_t)carry;
    }
    return true;
}

static bool shift(integer *value, uint32_t bits) {
    if (value->used == 0 || bits == 0)
        return true;
    const size_t words = bits / 32u;
    const unsigned int remainder = bits % 32u;
    if (words >= LIMBS || value->used > LIMBS - words)
        return false;
    integer result = {0};
    result.used = value->used + words;
    for (size_t i = 0; i < value->used; ++i) {
        const uint64_t part = (uint64_t)value->limb[i] << remainder;
        const size_t index = i + words;
        result.limb[index] |= (uint32_t)part;
        if ((part >> 32) != 0) {
            if (index + 1u == LIMBS)
                return false;
            result.limb[index + 1u] |= (uint32_t)(part >> 32);
            if (result.used < index + 2u)
                result.used = index + 2u;
        }
    }
    *value = result;
    return true;
}

static int integer_compare(const integer *first, const integer *second) {
    if (first->used != second->used)
        return first->used < second->used ? -1 : 1;
    for (size_t i = first->used; i != 0; --i)
        if (first->limb[i - 1u] != second->limb[i - 1u])
            return first->limb[i - 1u] < second->limb[i - 1u] ? -1 : 1;
    return 0;
}

static unsigned int bit_length(const integer *value) {
    unsigned int result = (unsigned int)((value->used - 1u) * 32u);
    uint32_t high = value->limb[value->used - 1u];
    do {
        ++result;
        high >>= 1;
    } while (high != 0);
    return result;
}

/* Compare numerator/denominator * 2^e with mantissa * 2^q. A discarded
 * nonzero tail can change only equality: every float/midpoint aligns to the
 * retained prefix grid (decimal e<=-153 or hex e<=-637 when digits drop). */
static bool compare(const magnitude *value, uint32_t mantissa, int32_t exponent, int *out) {
    integer left = value->numerator, right = value->denominator;
    if (!multiply(&right, mantissa))
        return false;
    if (mantissa == 0)
        right = (integer){0};
    const int32_t difference = value->binary_exponent - exponent;
    if (difference >= 0) {
        if (!shift(&left, (uint32_t)difference))
            return false;
    } else if (!shift(&right, (uint32_t)-difference)) {
        return false;
    }
    const int order = integer_compare(&left, &right);
    *out = order == 0 && value->sticky ? 1 : order;
    return true;
}

static void decode(uint32_t bits, uint32_t *mantissa, int32_t *exponent) {
    const uint32_t field = bits >> 23;
    *mantissa = bits & UINT32_C(0x007fffff);
    *exponent = -149;
    if (field != 0) {
        *mantissa |= UINT32_C(0x00800000);
        *exponent = (int32_t)field - 150;
    }
}

static bool rounded(const magnitude *value, uint32_t *out) {
    uint32_t lower = 0, upper = UINT32_C(0x7f800000);
    while (upper - lower > 1u) {
        const uint32_t middle = lower + (upper - lower) / 2u;
        uint32_t mantissa;
        int32_t exponent;
        int order;
        decode(middle, &mantissa, &exponent);
        if (!compare(value, mantissa, exponent, &order))
            return false;
        if (order >= 0)
            lower = middle;
        else
            upper = middle;
    }
    uint32_t mantissa;
    int32_t exponent;
    int order;
    decode(lower, &mantissa, &exponent);
    if (!compare(value, mantissa * 2u + 1u, exponent - 1, &order))
        return false;
    *out = order > 0 || (order == 0 && (lower & 1u) != 0) ? lower + 1u : lower;
    return true;
}

static int digit(uint16_t unit) {
    if (unit >= '0' && unit <= '9')
        return unit - '0';
    if (unit >= 'a' && unit <= 'f')
        return unit - 'a' + 10;
    if (unit >= 'A' && unit <= 'F')
        return unit - 'A' + 10;
    return -1;
}

static bool literal(const uint16_t *units, size_t first, size_t end, const char *word,
                    size_t length) {
    if (end - first != length)
        return false;
    for (size_t i = 0; i < length; ++i)
        if (units[first + i] != (unsigned char)word[i])
            return false;
    return true;
}

static mc_java_float_parse_status numeric(const uint16_t *units, size_t first, size_t end,
                                          uint32_t *out) {
    const bool hex = end - first >= 2u && units[first] == '0' &&
                     (units[first + 1u] == 'x' || units[first + 1u] == 'X');
    if (hex)
        first += 2u;
    const unsigned int base = hex ? 16u : 10u;
    integer prefix = {0};
    size_t significant = 0, kept = 0, fractional = 0;
    bool any_digit = false, point = false, sticky = false;
    while (first < end) {
        const int next = digit(units[first]);
        if (next >= 0 && (unsigned int)next < base) {
            any_digit = true;
            if (point)
                ++fractional;
            if (next != 0 || significant != 0) {
                ++significant;
                if (kept < PREFIX_DIGITS) {
                    if (!append_digit(&prefix, base, (unsigned int)next))
                        return MC_JAVA_FLOAT_PARSE_LIMIT;
                    ++kept;
                } else if (next != 0) {
                    sticky = true;
                }
            }
            ++first;
        } else if (units[first] == '.' && !point) {
            point = true;
            ++first;
        } else {
            break;
        }
    }
    if (!any_digit)
        return MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT;
    int64_t explicit_exponent = 0;
    const uint16_t lower_exponent = (uint16_t)(hex ? 'p' : 'e');
    const uint16_t upper_exponent = (uint16_t)(hex ? 'P' : 'E');
    if (first < end && (units[first] == lower_exponent || units[first] == upper_exponent)) {
        ++first;
        bool negative = false;
        if (first < end && (units[first] == '+' || units[first] == '-'))
            negative = units[first++] == '-';
        const size_t exponent_start = first;
        uint32_t value = 0;
        while (first < end && units[first] >= '0' && units[first] <= '9') {
            const uint64_t next = (uint64_t)value * 10u + (uint64_t)(units[first] - '0');
            value = next > EXPONENT_CAP ? EXPONENT_CAP : (uint32_t)next;
            ++first;
        }
        if (first == exponent_start)
            return MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT;
        explicit_exponent = negative ? -(int64_t)value : (int64_t)value;
    } else if (hex) {
        return MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT;
    }
    if (first < end &&
        (units[first] == 'f' || units[first] == 'F' || units[first] == 'd' || units[first] == 'D'))
        ++first;
    if (first != end)
        return MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT;
    if (significant == 0) {
        *out = 0;
        return MC_JAVA_FLOAT_PARSE_OK;
    }
    const int64_t unit = hex ? 4 : 1;
    const int64_t exponent =
        explicit_exponent - unit * (int64_t)fractional + unit * (int64_t)(significant - kept);
    const int64_t order = exponent + (hex ? (int64_t)bit_length(&prefix) - 1 : (int64_t)kept);
    if (order > (hex ? 127 : 39)) {
        *out = UINT32_C(0x7f800000);
        return MC_JAVA_FLOAT_PARSE_OK;
    }
    if (order < (hex ? -150 : -46)) {
        *out = 0;
        return MC_JAVA_FLOAT_PARSE_OK;
    }
    /* Decimal: |e|<=238 and comparisons need <1024 bits. Hex: e>=-917,
     * <=127; its worst shifted midpoint needs1045 bits. Both fit64 limbs. */
    if (exponent < (hex ? -917 : -238) || exponent > (hex ? 127 : 38))
        return MC_JAVA_FLOAT_PARSE_LIMIT;
    magnitude value = {.numerator = prefix,
                       .denominator = {.limb = {1}, .used = 1},
                       .binary_exponent = (int32_t)exponent,
                       .sticky = sticky};
    if (!hex) {
        integer *power = exponent >= 0 ? &value.numerator : &value.denominator;
        const int64_t count = exponent >= 0 ? exponent : -exponent;
        for (int64_t i = 0; i < count; ++i)
            if (!multiply(power, 5))
                return MC_JAVA_FLOAT_PARSE_LIMIT;
    }
    return rounded(&value, out) ? MC_JAVA_FLOAT_PARSE_OK : MC_JAVA_FLOAT_PARSE_LIMIT;
}

mc_java_float_parse_status mc_java_float_parse(const mc_java_float_parse_text *text, float *out) {
    if (out == NULL)
        return MC_JAVA_FLOAT_PARSE_INVALID_ARGUMENT;
    if (text == NULL)
        return MC_JAVA_FLOAT_PARSE_NULL_TEXT;
    if (text->length > MC_JAVA_FLOAT_PARSE_MAX_UNITS)
        return MC_JAVA_FLOAT_PARSE_LIMIT;
    if (text->units == NULL && text->length != 0)
        return MC_JAVA_FLOAT_PARSE_INVALID_ARGUMENT;
    size_t first = 0, end = text->length;
    while (first < end && text->units[first] <= 32u)
        ++first;
    while (end > first && text->units[end - 1u] <= 32u)
        --end;
    if (first == end)
        return MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT;
    bool negative = false;
    if (text->units[first] == '+' || text->units[first] == '-')
        negative = text->units[first++] == '-';
    if (first == end)
        return MC_JAVA_FLOAT_PARSE_NUMBER_FORMAT;
    uint32_t bits;
    if (literal(text->units, first, end, "NaN", 3)) {
        bits = UINT32_C(0x7fc00000);
    } else {
        if (literal(text->units, first, end, "Infinity", 8)) {
            bits = UINT32_C(0x7f800000);
        } else {
            const mc_java_float_parse_status status = numeric(text->units, first, end, &bits);
            if (status != MC_JAVA_FLOAT_PARSE_OK)
                return status;
        }
        if (negative)
            bits |= UINT32_C(0x80000000);
    }
    memcpy(out, &bits, sizeof(bits));
    return MC_JAVA_FLOAT_PARSE_OK;
}
