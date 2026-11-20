#include "mc/core/math/math.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "math_fixtures.h"

#define ARRAY_LENGTH(values) (sizeof(values) / sizeof((values)[0]))

_Static_assert(MC_MATH_TRIG_VALUES == 65536u, "original table shape");
_Static_assert(ARRAY_LENGTH(observed_float_calls) == 132, "paired float fixture count");
_Static_assert(ARRAY_LENGTH(observed_floor_calls) == 102, "paired floor fixture count");
_Static_assert(ARRAY_LENGTH(observed_table_entries) == 37, "selected table fixture count");

static mc_math_trig first;
static mc_math_trig second;
static mc_math_trig saved;
static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint32_t float_bits(float value) {
    uint32_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary32 storage required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static float float_from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static double double_from_bits(uint64_t bits) {
    double value;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 storage required");
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Byte order follows the independently captured big-endian raw float files,
 * regardless of the C host's object byte order. The optional comparison probe
 * also exposes every table entry for a direct reference comparison. */
static uint64_t table_fingerprint(const mc_math_trig *table) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < MC_MATH_TRIG_VALUES; ++i) {
        uint32_t bits = float_bits(table->values[i]);
        for (unsigned int byte = 0; byte < 4; ++byte) {
            unsigned int shift = 24u - byte * 8u;
            hash ^= (bits >> shift) & UINT32_C(255);
            hash *= UINT64_C(1099511628211);
        }
    }
    return hash;
}

static int test_floor_without_table(void) {
    for (size_t i = 0; i < ARRAY_LENGTH(observed_floor_calls); ++i) {
        const math_floor_fixture *fixture = &observed_floor_calls[i];
        int32_t result = mc_math_floor_double(double_from_bits(fixture->input));
        CHECK(result == fixture->integer);
        CHECK((uint32_t)result == fixture->bits);
    }
    return 0;
}

static int check_observed_table(const mc_math_trig *table) {
    CHECK(table_fingerprint(table) == observed_table_fnv1a);
    for (size_t i = 0; i < ARRAY_LENGTH(observed_table_entries); ++i) {
        const math_table_fixture *fixture = &observed_table_entries[i];
        CHECK(float_bits(table->values[fixture->index]) == fixture->bits);
    }
    return 0;
}

static int test_initialization_and_reads(void) {
    /* Arbitrary prior bytes are not a partially initialized table to read. Both
     * complete objects must be replaced by the explicit initializer first. */
    memset(&first, 0xa5, sizeof(first));
    memset(&second, 0x5a, sizeof(second));
    CHECK(!mc_math_trig_init(NULL));
    CHECK(mc_math_trig_init(&first));
    CHECK(check_observed_table(&first) == 0);
    memcpy(&saved, &first, sizeof(saved));

    CHECK(mc_math_trig_init(&second));
    CHECK(check_observed_table(&second) == 0);
    CHECK(memcmp(&first, &saved, sizeof(first)) == 0);
    CHECK(memcmp(&second, &saved, sizeof(second)) == 0);
    CHECK(!mc_math_trig_init(NULL));
    CHECK(memcmp(&first, &saved, sizeof(first)) == 0);
    CHECK(memcmp(&second, &saved, sizeof(second)) == 0);

    const mc_math_trig *first_reader = &first;
    const mc_math_trig *second_reader = &second;
    for (size_t i = 0; i < ARRAY_LENGTH(observed_float_calls); ++i) {
        const math_float_fixture *fixture = &observed_float_calls[i];
        float input = float_from_bits(fixture->input);
        CHECK(float_bits(mc_math_sin(first_reader, input)) == fixture->sine);
        CHECK(float_bits(mc_math_cos(first_reader, input)) == fixture->cosine);
        CHECK(float_bits(mc_math_sin(second_reader, input)) == fixture->sine);
        CHECK(float_bits(mc_math_cos(second_reader, input)) == fixture->cosine);
    }
    CHECK(memcmp(&first, &saved, sizeof(first)) == 0);
    CHECK(memcmp(&second, &saved, sizeof(second)) == 0);

    /* Reinitialization is exclusive and replaces the entire table again. The
     * independently initialized second object remains available for reads. */
    memset(&first, 0x3c, sizeof(first));
    CHECK(mc_math_trig_init(&first));
    CHECK(memcmp(&first, &saved, sizeof(first)) == 0);
    CHECK(memcmp(&second, &saved, sizeof(second)) == 0);
    CHECK(mc_math_trig_init(&first));
    CHECK(memcmp(&first, &saved, sizeof(first)) == 0);
    CHECK(check_observed_table(&first) == 0);
    CHECK(check_observed_table(&second) == 0);
    return 0;
}

int main(void) {
    if (test_floor_without_table() != 0 || test_initialization_and_reads() != 0) {
        return 1;
    }
    printf("Math: %u checks passed\n", checks);
    return 0;
}
