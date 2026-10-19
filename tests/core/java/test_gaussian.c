#include "mc/core/java/java.h"
#include "mc/core/math/strict.h"

#include <emmintrin.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint64_t double_bits(double value)
{
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 storage required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t cached_bits(const mc_java_random *random)
{
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(random->next_next_gaussian),
                   "binary64 cache storage required");
    memcpy(&bits, &random->next_next_gaussian, sizeof(bits));
    return bits;
}

static double double_from_bits(uint64_t bits)
{
    double value;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 storage required");
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool random_matches(const mc_java_random *random, uint64_t state, uint64_t cached,
                           bool have_cached)
{
    return random->state == state && cached_bits(random) == cached &&
           random->have_next_next_gaussian == have_cached;
}

static int test_fresh_and_reseed(void)
{
    mc_java_random random;
    double value;

    /* Poison storage, but give _Bool a valid true representation before the
     * call so an omitted flag write is a defined failing assertion. */
    memset(&random, 0xa5, sizeof(random));
    random.have_next_next_gaussian = true;
    mc_java_random_seed(&random, 0);
    CHECK(random_matches(&random, UINT64_C(0x00000005deece66d), UINT64_C(0), false));

    /* Exact reset contract: stale bits survive; their valid flag does not. */
    random.next_next_gaussian = -0.0;
    random.have_next_next_gaussian = true;
    mc_java_random_reseed(&random, 0);
    CHECK(
        random_matches(&random, UINT64_C(0x00000005deece66d), UINT64_C(0x8000000000000000), false));
    value = mc_java_random_next_gaussian(&random);
    CHECK(double_bits(value) == UINT64_C(0x3fe9ae59d1d6f861));
    CHECK(
        random_matches(&random, UINT64_C(0x00009b3970be8d41), UINT64_C(0xbfecd9772eb2e0c8), true));

    random.next_next_gaussian = 1.25;
    random.have_next_next_gaussian = true;
    mc_java_random_reseed(&random, 1);
    CHECK(
        random_matches(&random, UINT64_C(0x00000005deece66c), UINT64_C(0x3ff4000000000000), false));
    mc_java_random_reseed(&random, 0);
    CHECK(
        random_matches(&random, UINT64_C(0x00000005deece66d), UINT64_C(0x3ff4000000000000), false));

    /* A valid cached signed zero is returned intact without a draw. */
    random.next_next_gaussian = -0.0;
    random.have_next_next_gaussian = true;
    value = mc_java_random_next_gaussian(&random);
    CHECK(double_bits(value) == UINT64_C(0x8000000000000000));
    CHECK(
        random_matches(&random, UINT64_C(0x00000005deece66d), UINT64_C(0x8000000000000000), false));

    /* Fresh initialization also overwrites a previously valid cache. */
    random.next_next_gaussian = 1.25;
    random.have_next_next_gaussian = true;
    mc_java_random_seed(&random, 1);
    CHECK(random_matches(&random, UINT64_C(0x00000005deece66c), UINT64_C(0), false));
    return 0;
}

static int test_captured_first_pairs(void)
{
    /* random_gaussian_fixtures.json events 6003, 6348, 9828, 9843, 9858.
     * Expected results AND continuation/cache fields are actual JDK output. */
    static const struct {
        int64_t seed;
        uint64_t first;
        uint64_t state;
        uint64_t cached;
    } cases[] = {{0, UINT64_C(0x3fe9ae59d1d6f861), UINT64_C(0x00009b3970be8d41),
                  UINT64_C(0xbfecd9772eb2e0c8)},
                 {23, UINT64_C(0xbff4edf9f46640a8), UINT64_C(0x000012a439ec6202),
                  UINT64_C(0xbfe20caac67b852b)},
                 {255, UINT64_C(0x3fe967a04af03aae), UINT64_C(0x0000117325734d1a),
                  UINT64_C(0x4002807cf39e5d52)},
                 {-1, UINT64_C(0x3ffc90b7b3790a3a), UINT64_C(0x00006637c50ce01a),
                  UINT64_C(0xbfed740e27d7f93c)},
                 {INT64_MIN, UINT64_C(0x3fe9ae59d1d6f861), UINT64_C(0x00009b3970be8d41),
                  UINT64_C(0xbfecd9772eb2e0c8)}};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        mc_java_random random;
        double value;
        mc_java_random_seed(&random, cases[i].seed);
        value = mc_java_random_next_gaussian(&random);
        CHECK(double_bits(value) == cases[i].first);
        CHECK(random_matches(&random, cases[i].state, cases[i].cached, true));
        /* Apply the exact no-draw cache contract to the captured state. */
        value = mc_java_random_next_gaussian(&random);
        CHECK(double_bits(value) == cases[i].cached);
        CHECK(random_matches(&random, cases[i].state, cases[i].cached, false));
    }
    return 0;
}

static int test_captured_mixed_history(void)
{
    /* Exact captured events 6002..6016, excluding only no-op snapshot 6004. */
    mc_java_random random;
    mc_java_random other;
    double value;
    int32_t integer;
    int64_t long_value;

    mc_java_random_seed(&random, 0);
    value = mc_java_random_next_gaussian(&random);
    CHECK(double_bits(value) == UINT64_C(0x3fe9ae59d1d6f861));
    CHECK(
        random_matches(&random, UINT64_C(0x00009b3970be8d41), UINT64_C(0xbfecd9772eb2e0c8), true));
    integer = mc_java_random_next_i32(&random);
    CHECK((uint32_t)integer == UINT32_C(0xa32dc9f6));
    CHECK(
        random_matches(&random, UINT64_C(0x0000a32dc9f68ab8), UINT64_C(0xbfecd9772eb2e0c8), true));
    value = mc_java_random_next_double(&random);
    CHECK(double_bits(value) == UINT64_C(0x3fd3c77c08ce970a));
    CHECK(
        random_matches(&random, UINT64_C(0x00008ce970b7fc32), UINT64_C(0xbfecd9772eb2e0c8), true));
    value = mc_java_random_next_gaussian(&random);
    CHECK(double_bits(value) == UINT64_C(0xbfecd9772eb2e0c8));
    CHECK(
        random_matches(&random, UINT64_C(0x00008ce970b7fc32), UINT64_C(0xbfecd9772eb2e0c8), false));
    value = mc_java_random_next_gaussian(&random);
    CHECK(double_bits(value) == UINT64_C(0xbfd732068a7ed544));
    CHECK(
        random_matches(&random, UINT64_C(0x0000554dcd2b1a56), UINT64_C(0x3fd10cfbc79228fe), true));

    mc_java_random_reseed(&random, 0);
    CHECK(
        random_matches(&random, UINT64_C(0x00000005deece66d), UINT64_C(0x3fd10cfbc79228fe), false));
    value = mc_java_random_next_gaussian(&random);
    CHECK(double_bits(value) == UINT64_C(0x3fe9ae59d1d6f861));
    CHECK(
        random_matches(&random, UINT64_C(0x00009b3970be8d41), UINT64_C(0xbfecd9772eb2e0c8), true));
    mc_java_random_reseed(&random, -INT64_C(3335678366873096957));
    CHECK(
        random_matches(&random, UINT64_C(0x00004a370f7e0b6e), UINT64_C(0xbfecd9772eb2e0c8), false));
    value = mc_java_random_next_gaussian(&random);
    CHECK(double_bits(value) == UINT64_C(0x3fd6fa53f7833317));
    CHECK(
        random_matches(&random, UINT64_C(0x0000c4945a496152), UINT64_C(0xbfd191b64c94d6cd), true));

    mc_java_random_seed(&other, -INT64_C(3335678366873096957));
    CHECK(random_matches(&other, UINT64_C(0x00004a370f7e0b6e), UINT64_C(0), false));
    value = mc_java_random_next_gaussian(&other);
    CHECK(double_bits(value) == UINT64_C(0x3fd6fa53f7833317));
    CHECK(random_matches(&other, UINT64_C(0x0000c4945a496152), UINT64_C(0xbfd191b64c94d6cd), true));
    long_value = mc_java_random_next_i64(&random);
    CHECK((uint64_t)long_value == UINT64_C(0xc0ed3245f0d2a0d4));
    CHECK(
        random_matches(&random, UINT64_C(0x0000f0d2a0d4055c), UINT64_C(0xbfd191b64c94d6cd), true));
    CHECK(random_matches(&other, UINT64_C(0x0000c4945a496152), UINT64_C(0xbfd191b64c94d6cd), true));
    value = mc_java_random_next_gaussian(&other);
    CHECK(double_bits(value) == UINT64_C(0xbfd191b64c94d6cd));
    CHECK(
        random_matches(&other, UINT64_C(0x0000c4945a496152), UINT64_C(0xbfd191b64c94d6cd), false));
    CHECK(
        random_matches(&random, UINT64_C(0x0000f0d2a0d4055c), UINT64_C(0xbfd191b64c94d6cd), true));
    return 0;
}

static int test_copied_owner_independence(void)
{
    mc_java_random original;
    mc_java_random copy;
    double value;
    int32_t integer;

    mc_java_random_seed(&original, 0);
    value = mc_java_random_next_gaussian(&original);
    CHECK(double_bits(value) == UINT64_C(0x3fe9ae59d1d6f861));
    CHECK(random_matches(&original, UINT64_C(0x00009b3970be8d41), UINT64_C(0xbfecd9772eb2e0c8),
                         true));
    copy = original;
    integer = mc_java_random_next_i32(&original);
    CHECK((uint32_t)integer == UINT32_C(0xa32dc9f6));
    CHECK(random_matches(&original, UINT64_C(0x0000a32dc9f68ab8), UINT64_C(0xbfecd9772eb2e0c8),
                         true));
    CHECK(random_matches(&copy, UINT64_C(0x00009b3970be8d41), UINT64_C(0xbfecd9772eb2e0c8), true));
    value = mc_java_random_next_gaussian(&copy);
    CHECK(double_bits(value) == UINT64_C(0xbfecd9772eb2e0c8));
    CHECK(random_matches(&copy, UINT64_C(0x00009b3970be8d41), UINT64_C(0xbfecd9772eb2e0c8), false));
    CHECK(random_matches(&original, UINT64_C(0x0000a32dc9f68ab8), UINT64_C(0xbfecd9772eb2e0c8),
                         true));
    value = mc_java_random_next_double(&original);
    CHECK(double_bits(value) == UINT64_C(0x3fd3c77c08ce970a));
    value = mc_java_random_next_gaussian(&original);
    CHECK(double_bits(value) == UINT64_C(0xbfecd9772eb2e0c8));
    CHECK(random_matches(&original, UINT64_C(0x00008ce970b7fc32), UINT64_C(0xbfecd9772eb2e0c8),
                         false));
    integer = mc_java_random_next_i32(&copy);
    CHECK((uint32_t)integer == UINT32_C(0xa32dc9f6));
    CHECK(random_matches(&copy, UINT64_C(0x0000a32dc9f68ab8), UINT64_C(0xbfecd9772eb2e0c8), false));
    CHECK(random_matches(&original, UINT64_C(0x00008ce970b7fc32), UINT64_C(0xbfecd9772eb2e0c8),
                         false));
    return 0;
}

static int test_captured_strict_boundaries(void)
{
    /* Actual StrictMath observations: positive subnormals, the normal boundary,
     * one and its neighbors, and maximum finite. No host-math expectations. */
    static const struct {
        uint64_t input;
        uint64_t logarithm;
        uint64_t square_root;
    } cases[] = {
        {UINT64_C(0x0000000000000001), UINT64_C(0xc0874385446d71c3), UINT64_C(0x1e60000000000000)},
        {UINT64_C(0x0000000080000001), UINT64_C(0xc086979ebd354e82), UINT64_C(0x1f56a09e6695dc6b)},
        {UINT64_C(0x000fffffffffffff), UINT64_C(0xc086232bdd7abcd2), UINT64_C(0x1fffffffffffffff)},
        {UINT64_C(0x0010000000000000), UINT64_C(0xc086232bdd7abcd2), UINT64_C(0x2000000000000000)},
        {UINT64_C(0x3fefffffffffffff), UINT64_C(0xbca0000000000000), UINT64_C(0x3fefffffffffffff)},
        {UINT64_C(0x3ff0000000000000), UINT64_C(0x0000000000000000), UINT64_C(0x3ff0000000000000)},
        {UINT64_C(0x3ff0000000000001), UINT64_C(0x3cafffffffffffff), UINT64_C(0x3ff0000000000000)},
        {UINT64_C(0x7fefffffffffffff), UINT64_C(0x40862e42fefa39ef), UINT64_C(0x5fefffffffffffff)}};

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const double input = double_from_bits(cases[i].input);
        const double logarithm = mc_math_strict_log(input);
        const double square_root = mc_math_strict_sqrt(input);
        CHECK(double_bits(logarithm) == cases[i].logarithm);
        CHECK(double_bits(square_root) == cases[i].square_root);
    }
    return 0;
}

int main(void)
{
    /* Require the public backend's control environment without changing it.
     * Sticky status bits are deliberately ignored. */
    CHECK((_mm_getcsr() & 0xffc0u) == 0x1f80u);
    if (test_fresh_and_reseed() != 0 || test_captured_first_pairs() != 0 ||
        test_captured_mixed_history() != 0 || test_copied_owner_independence() != 0 ||
        test_captured_strict_boundaries() != 0) {
        return 1;
    }
    printf("PASS: %u Random Gaussian checks\n", checks);
    return 0;
}
