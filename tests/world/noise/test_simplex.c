#include "mc/world/noise/noise.h"
#include "mc/world/noise/simplex.h"

#include "../../support/java_random.h"

#include <limits.h>
#include <math.h>
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

static uint64_t bits_of(double value)
{
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 storage required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double from_bits(uint64_t bits)
{
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void prefill(double *values, size_t length)
{
    for (size_t i = 0; i < length; ++i)
        values[i] = (double)((int)(i % 11u) - 5) * 0.125;
}

static int test_constants_and_construction(void)
{
    /* Static f/g values observed independently in untouched client cc and
     * server bi on Temurin 17.0.17; observing them consumed no Random draws. */
    CHECK(bits_of(mc_noise_simplex_skew) == UINT64_C(0x3fd76cf5d0b09954));
    CHECK(bits_of(mc_noise_simplex_unskew) == UINT64_C(0x3fcb0cb174df99c8));
    CHECK(bits_of(MC_NOISE_SIMPLEX_DEFAULT_DIVISOR_MULTIPLIER) == UINT64_C(0x3fe0000000000000));
    static const int64_t seeds[] = {0, 1, -1, INT64_MIN, INT64_MAX, INT64_C(123456789)};
    for (size_t seed = 0; seed < sizeof(seeds) / sizeof(seeds[0]); ++seed) {
        mc_java_random random;
        mc_java_random_seed(&random, seeds[seed]);
        mc_noise_simplex noise;
        CHECK(mc_noise_simplex_init(&noise, &random));
        CHECK(isfinite(noise.offset_x) && noise.offset_x >= 0 && noise.offset_x < 256);
        CHECK(isfinite(noise.offset_z) && noise.offset_z >= 0 && noise.offset_z < 256);
        CHECK(isfinite(noise.offset_unused) && noise.offset_unused >= 0 &&
              noise.offset_unused < 256);
        unsigned int occurrences[256] = {0};
        for (size_t i = 0; i < 256; ++i) {
            CHECK(noise.permutation[i] < 256);
            CHECK(noise.permutation[i] == noise.permutation[i + 256u]);
            ++occurrences[noise.permutation[i]];
        }
        for (size_t i = 0; i < 256; ++i)
            CHECK(occurrences[i] == 1);
    }
    return 0;
}

static int test_constructor_order(void)
{
    mc_java_random random;
    mc_java_random manual;
    mc_java_random_seed(&random, 7);
    mc_java_random_seed(&manual, 7);
    mc_noise_simplex layers[4];
    mc_noise_simplex consecutive[4];
    mc_noise_simplex_octaves noise;
    CHECK(mc_noise_simplex_octaves_init(&noise, layers, 4, &random));
    CHECK(noise.layers == layers && noise.count == 4);
    for (size_t i = 0; i < 4; ++i)
        CHECK(mc_noise_simplex_init(&consecutive[i], &manual));
    CHECK(memcmp(layers, consecutive, sizeof(layers)) == 0);
    for (size_t i = 0; i < 4; ++i)
        CHECK(mc_java_random_next_i64(&random) == mc_java_random_next_i64(&manual));
    mc_noise_simplex_octaves empty;
    CHECK(mc_noise_simplex_octaves_init(&empty, NULL, 0, NULL));
    CHECK(empty.layers == NULL && empty.count == 0);
    mc_noise_simplex_grid grid = {-1, 3, 2, 2, 0.5, 0.25};
    double output[7];
    prefill(output, 7);
    CHECK(mc_noise_simplex_octaves_fill(&empty, &grid, 0.5, 0.5, output, 7));
    for (size_t i = 0; i < 7; ++i)
        CHECK(bits_of(output[i]) == 0);
    return 0;
}

/* Fixed original-only observations captured on both untouched sides, with
 * exact agreement recorded in the private simplex fixed-vector manifest. */
typedef struct constructor_fixture {
    const char *prefix;
    size_t count;
    uint64_t offsets[2][3];
    uint16_t permutation_prefixes[2][16];
    uint64_t next[4];
} constructor_fixture;

static const constructor_fixture constructor_fixtures[] = {
    {"-",
     1,
     {{UINT64_C(0x406764168ea6ca89), UINT64_C(0x404ec9e5b3672e14), UINT64_C(0x406465b93a78ef81)}},
     {{140, 157, 53, 179, 117, 253, 229, 104, 247, 35, 20, 221, 25, 51, 211, 18}},
     {UINT64_C(0xa15aa3684d0173ab), UINT64_C(0xd15dcc46f84f5c2a), UINT64_C(0xbdb9ee86519b4e29),
      UINT64_C(0xa1a566f569d0f53c)}},
    {"PS",
     1,
     {{UINT64_C(0x4060d7162d099092), UINT64_C(0x3ff070992fcd5b80), UINT64_C(0x4021b7ec5fda52b0)}},
     {{201, 164, 138, 44, 157, 15, 63, 192, 243, 78, 233, 227, 122, 229, 37, 248}},
     {UINT64_C(0x9388ab0cb17037ef), UINT64_C(0x38b27f4e79445af3), UINT64_C(0xebc6f76d1b97a69a),
      UINT64_C(0x4affba54c9f8685f)}},
    {"SP",
     1,
     {{UINT64_C(0x4060d7162d099092), UINT64_C(0x3ff070992fcd5b80), UINT64_C(0x4021b7ec5fda52b0)}},
     {{201, 164, 138, 44, 157, 15, 63, 192, 243, 78, 233, 227, 122, 229, 37, 248}},
     {UINT64_C(0x9388ab0cb17037ef), UINT64_C(0x38b27f4e79445af3), UINT64_C(0xebc6f76d1b97a69a),
      UINT64_C(0x4affba54c9f8685f)}},
    {"PSSP",
     2,
     {{UINT64_C(0x406cb9cdf8acd2cf), UINT64_C(0x4053b9c28b1601a8), UINT64_C(0x405743c17bd2f192)},
      {UINT64_C(0x400f3634b543b3c0), UINT64_C(0x406ef50ba55f1cbd), UINT64_C(0x406ed0cdcc326ce6)}},
     {{122, 72, 50, 94, 24, 81, 255, 218, 217, 104, 234, 71, 11, 158, 70, 84},
      {64, 240, 163, 166, 153, 28, 165, 16, 130, 126, 223, 154, 114, 217, 23, 202}},
     {UINT64_C(0xc95e8facf25af878), UINT64_C(0xc2f89f60487f6d92), UINT64_C(0x95123fe718c3e11b),
      UINT64_C(0xb835e983aae3a033)}},
};

static int test_observed_constructor_states(void)
{
    for (size_t fixture = 0;
         fixture < sizeof(constructor_fixtures) / sizeof(constructor_fixtures[0]); ++fixture) {
        const constructor_fixture *expected = &constructor_fixtures[fixture];
        mc_java_random random;
        mc_java_random_seed(&random, 0);
        mc_noise_perlin perlin;
        mc_noise_simplex discarded;
        if (strcmp(expected->prefix, "-") != 0) {
            for (const char *kind = expected->prefix; *kind != '\0'; ++kind) {
                if (*kind == 'P')
                    CHECK(mc_noise_perlin_init(&perlin, &random));
                else
                    CHECK(mc_noise_simplex_init(&discarded, &random));
            }
        }
        mc_noise_simplex layers[2];
        mc_noise_simplex_octaves noise;
        CHECK(mc_noise_simplex_octaves_init(&noise, layers, expected->count, &random));
        for (size_t i = 0; i < expected->count; ++i) {
            CHECK(bits_of(layers[i].offset_x) == expected->offsets[i][0]);
            CHECK(bits_of(layers[i].offset_z) == expected->offsets[i][1]);
            CHECK(bits_of(layers[i].offset_unused) == expected->offsets[i][2]);
            CHECK(memcmp(layers[i].permutation, expected->permutation_prefixes[i],
                         sizeof(expected->permutation_prefixes[i])) == 0);
        }
        for (size_t i = 0; i < 4; ++i)
            CHECK((uint64_t)mc_java_random_next_i64(&random) == expected->next[i]);
    }
    return 0;
}

typedef struct grid_fixture {
    int add;
    int use_default;
    size_t layers;
    uint64_t origin_x;
    uint64_t origin_z;
    size_t size_x;
    size_t size_z;
    uint64_t scale_x;
    uint64_t scale_z;
    uint64_t control1;
    uint64_t control2;
    uint64_t expected[9];
    size_t length;
    uint64_t next[4];
} grid_fixture;

static const grid_fixture grid_fixtures[] = {
    {1,
     0,
     1,
     UINT64_C(0x0000000000000000),
     UINT64_C(0x0000000000000000),
     3,
     2,
     UINT64_C(0x3fc0000000000000),
     UINT64_C(0x3fd0000000000000),
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3fe0000000000000),
     {UINT64_C(0xbfe993dc839d5d8c), UINT64_C(0xbff118b39e17b9fb), UINT64_C(0xbfd7f553eb96bbb8),
      UINT64_C(0xbfe17856709d72bc), UINT64_C(0x3fcf0ccb210398d4), UINT64_C(0x3fd0f9a4d0fee158),
      UINT64_C(0x3fc0000000000000), UINT64_C(0x3fd0000000000000), UINT64_C(0x3fd8000000000000)},
     9,
     {UINT64_C(0xa15aa3684d0173ab), UINT64_C(0xd15dcc46f84f5c2a), UINT64_C(0xbdb9ee86519b4e29),
      UINT64_C(0xa1a566f569d0f53c)}},
    {1,
     0,
     1,
     UINT64_C(0xc06764168ea6ca89),
     UINT64_C(0xc04ec9e5b3672e14),
     1,
     1,
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3fe0000000000000),
     {UINT64_C(0xbfe4000000000000), UINT64_C(0xbfe0000000000000), UINT64_C(0xbfd8000000000000),
      UINT64_C(0xbfd0000000000000)},
     4,
     {UINT64_C(0xa15aa3684d0173ab), UINT64_C(0xd15dcc46f84f5c2a), UINT64_C(0xbdb9ee86519b4e29),
      UINT64_C(0xa1a566f569d0f53c)}},
    {1,
     0,
     1,
     UINT64_C(0xc06784168ea6ca89),
     UINT64_C(0xc04e49e5b3672e14),
     1,
     1,
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3fe0000000000000),
     {UINT64_C(0xbfe4000000000000), UINT64_C(0xbfe0000000000000), UINT64_C(0xbfd8000000000000),
      UINT64_C(0xbfd0000000000000)},
     4,
     {UINT64_C(0xa15aa3684d0173ab), UINT64_C(0xd15dcc46f84f5c2a), UINT64_C(0xbdb9ee86519b4e29),
      UINT64_C(0xa1a566f569d0f53c)}},
    {1,
     0,
     1,
     UINT64_C(0xc0675c168ea6ca89),
     UINT64_C(0xc04ea9e5b3672e14),
     1,
     1,
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3fe0000000000000),
     {UINT64_C(0xbfb5e309c8941f60), UINT64_C(0xbfe0000000000000), UINT64_C(0xbfd8000000000000),
      UINT64_C(0xbfd0000000000000)},
     4,
     {UINT64_C(0xa15aa3684d0173ab), UINT64_C(0xd15dcc46f84f5c2a), UINT64_C(0xbdb9ee86519b4e29),
      UINT64_C(0xa1a566f569d0f53c)}},
    {1,
     0,
     1,
     UINT64_C(0xc06754168ea6ca89),
     UINT64_C(0xc04e89e5b3672e14),
     1,
     1,
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3fe0000000000000),
     {UINT64_C(0xbfedd439e599085f), UINT64_C(0xbfe0000000000000), UINT64_C(0xbfd8000000000000),
      UINT64_C(0xbfd0000000000000)},
     4,
     {UINT64_C(0xa15aa3684d0173ab), UINT64_C(0xd15dcc46f84f5c2a), UINT64_C(0xbdb9ee86519b4e29),
      UINT64_C(0xa1a566f569d0f53c)}},
    {1,
     0,
     1,
     UINT64_C(0x41cdcd6500000000),
     UINT64_C(0x41cdcd6500000000),
     1,
     1,
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3fe0000000000000),
     {UINT64_C(0xbfe4000000000000), UINT64_C(0xbfe0000000000000), UINT64_C(0xbfd8000000000000),
      UINT64_C(0xbfd0000000000000)},
     4,
     {UINT64_C(0xa15aa3684d0173ab), UINT64_C(0xd15dcc46f84f5c2a), UINT64_C(0xbdb9ee86519b4e29),
      UINT64_C(0xa1a566f569d0f53c)}},
    {0,
     0,
     4,
     UINT64_C(0xc031400000000000),
     UINT64_C(0x403fe00000000000),
     3,
     2,
     UINT64_C(0x3fc0000000000000),
     UINT64_C(0x3fd0000000000000),
     UINT64_C(0x3fd0000000000000),
     UINT64_C(0x3fe0000000000000),
     {UINT64_C(0x3ff2ce9949507ebb), UINT64_C(0x3ff4a37ade40a5ab), UINT64_C(0x3ff4e22555e4a052),
      UINT64_C(0x3ff8bc9cc2e381e9), UINT64_C(0x3ff7ee4f021607b8), UINT64_C(0x3ffd97a1b3ea44f8),
      UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000)},
     9,
     {UINT64_C(0xe5ce6fc6159a59ff), UINT64_C(0x4ee70a3eb1601a89), UINT64_C(0x5d0f05ebbd2f193f),
      UINT64_C(0x7a5712a42bf2462a)}},
    {0,
     1,
     4,
     UINT64_C(0xc031400000000000),
     UINT64_C(0x403fe00000000000),
     3,
     2,
     UINT64_C(0x3fc0000000000000),
     UINT64_C(0x3fd0000000000000),
     UINT64_C(0x3fd0000000000000),
     UINT64_C(0x3fe0000000000000),
     {UINT64_C(0x3ff2ce9949507ebb), UINT64_C(0x3ff4a37ade40a5ab), UINT64_C(0x3ff4e22555e4a052),
      UINT64_C(0x3ff8bc9cc2e381e9), UINT64_C(0x3ff7ee4f021607b8), UINT64_C(0x3ffd97a1b3ea44f8),
      UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000)},
     9,
     {UINT64_C(0xe5ce6fc6159a59ff), UINT64_C(0x4ee70a3eb1601a89), UINT64_C(0x5d0f05ebbd2f193f),
      UINT64_C(0x7a5712a42bf2462a)}},
    {0,
     0,
     4,
     UINT64_C(0xc031400000000000),
     UINT64_C(0x403fe00000000000),
     3,
     2,
     UINT64_C(0x3fc0000000000000),
     UINT64_C(0x3fd0000000000000),
     UINT64_C(0x3fe8000000000000),
     UINT64_C(0x4000000000000000),
     {UINT64_C(0xbfe068d594c78889), UINT64_C(0xbfe272968482a0e3), UINT64_C(0xbfd7252f22dc032a),
      UINT64_C(0xbfd4376dc567cf21), UINT64_C(0xbfc2471dd9208305), UINT64_C(0x3f8629c0673e91f0),
      UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000), UINT64_C(0x0000000000000000)},
     9,
     {UINT64_C(0xe5ce6fc6159a59ff), UINT64_C(0x4ee70a3eb1601a89), UINT64_C(0x5d0f05ebbd2f193f),
      UINT64_C(0x7a5712a42bf2462a)}},
};

static int test_observed_grids(void)
{
    for (size_t fixture = 0; fixture < sizeof(grid_fixtures) / sizeof(grid_fixtures[0]);
         ++fixture) {
        const grid_fixture *expected = &grid_fixtures[fixture];
        mc_java_random random;
        mc_java_random_seed(&random, 0);
        mc_noise_simplex layers[4] = {0};
        mc_noise_simplex_octaves noise;
        if (expected->add)
            CHECK(mc_noise_simplex_init(&layers[0], &random));
        else
            CHECK(mc_noise_simplex_octaves_init(&noise, layers, expected->layers, &random));
        mc_noise_simplex saved[4];
        memcpy(saved, layers, sizeof(saved));
        mc_noise_simplex_grid grid = {from_bits(expected->origin_x),
                                      from_bits(expected->origin_z),
                                      expected->size_x,
                                      expected->size_z,
                                      from_bits(expected->scale_x),
                                      from_bits(expected->scale_z)};
        mc_noise_simplex_grid grid_saved = grid;
        double output[9];
        prefill(output, expected->length);
        if (expected->add)
            CHECK(mc_noise_simplex_add(&layers[0], &grid, from_bits(expected->control1), output,
                                       expected->length));
        else
            CHECK(mc_noise_simplex_octaves_fill(&noise, &grid, from_bits(expected->control1),
                                                expected->use_default
                                                    ? MC_NOISE_SIMPLEX_DEFAULT_DIVISOR_MULTIPLIER
                                                    : from_bits(expected->control2),
                                                output, expected->length));
        for (size_t i = 0; i < expected->length; ++i)
            CHECK(bits_of(output[i]) == expected->expected[i]);
        CHECK(memcmp(layers, saved, sizeof(layers)) == 0);
        CHECK(memcmp(&grid, &grid_saved, sizeof(grid)) == 0);
        if (expected->add) {
            prefill(output, expected->length);
            double unchanged[9];
            memcpy(unchanged, output, expected->length * sizeof(*output));
            CHECK(mc_noise_simplex_add(&layers[0], &grid, 0, output, expected->length));
            CHECK(memcmp(output, unchanged, expected->length * sizeof(*output)) == 0);
        } else {
            for (size_t i = 0; i < expected->length; ++i)
                output[i] = 12345;
            CHECK(mc_noise_simplex_octaves_fill(&noise, &grid, from_bits(expected->control1),
                                                from_bits(expected->control2), output,
                                                expected->length));
            for (size_t i = 0; i < expected->length; ++i)
                CHECK(bits_of(output[i]) == expected->expected[i]);
        }
        for (size_t i = 0; i < 4; ++i)
            CHECK((uint64_t)mc_java_random_next_i64(&random) == expected->next[i]);
    }
    return 0;
}

static int test_invalid_constructors(void)
{
    mc_java_random random;
    mc_java_random_seed(&random, 123);
    mc_java_random random_saved = random;
    mc_noise_simplex layer;
    mc_noise_simplex layer_saved;
    memset(&layer, 0xa5, sizeof(layer));
    memcpy(&layer_saved, &layer, sizeof(layer));
    CHECK(!mc_noise_simplex_init(NULL, &random));
    CHECK(mc_test_java_random_equal(&random, &random_saved));
    CHECK(!mc_noise_simplex_init(&layer, NULL));
    CHECK(memcmp(&layer, &layer_saved, sizeof(layer)) == 0);
    mc_noise_simplex_octaves noise;
    mc_noise_simplex_octaves saved;
    memset(&noise, 0xa5, sizeof(noise));
    memcpy(&saved, &noise, sizeof(noise));
    CHECK(!mc_noise_simplex_octaves_init(NULL, &layer, 1, &random));
    CHECK(!mc_noise_simplex_octaves_init(&noise, NULL, 1, &random));
    CHECK(!mc_noise_simplex_octaves_init(&noise, &layer, 1, NULL));
    CHECK(!mc_noise_simplex_octaves_init(&noise, &layer, (size_t)INT32_MAX + 1u, &random));
    CHECK(!mc_noise_simplex_octaves_init(&noise, &layer, SIZE_MAX, &random));
    CHECK(memcmp(&noise, &saved, sizeof(noise)) == 0);
    CHECK(memcmp(&layer, &layer_saved, sizeof(layer)) == 0);
    CHECK(mc_test_java_random_equal(&random, &random_saved));
    CHECK(mc_noise_simplex_octaves_init(&noise, NULL, 0, &random));
    CHECK(mc_test_java_random_equal(&random, &random_saved));
    return 0;
}

static int expect_grid_failure(const mc_noise_simplex *layer, const mc_noise_simplex_octaves *noise,
                               const mc_noise_simplex_grid *grid, size_t capacity)
{
    double output[16];
    double saved[16];
    mc_noise_simplex saved_layer = *layer;
    prefill(output, 16);
    memcpy(saved, output, sizeof(saved));
    CHECK(!mc_noise_simplex_add(layer, grid, 1, output, capacity));
    CHECK(memcmp(saved, output, sizeof(output)) == 0);
    CHECK(memcmp(layer, &saved_layer, sizeof(saved_layer)) == 0);
    CHECK(!mc_noise_simplex_octaves_fill(noise, grid, 0.5, 0.5, output, capacity));
    CHECK(memcmp(saved, output, sizeof(output)) == 0);
    CHECK(memcmp(layer, &saved_layer, sizeof(saved_layer)) == 0);
    return 0;
}

static int test_grid_validation_and_zero_sizes(void)
{
    mc_java_random random;
    mc_java_random_seed(&random, 0);
    mc_noise_simplex layer;
    mc_noise_simplex_octaves noise;
    CHECK(mc_noise_simplex_octaves_init(&noise, &layer, 1, &random));
    mc_noise_simplex_grid grid = {0, 0, 2, 2, 1, 1};
    CHECK(expect_grid_failure(&layer, &noise, &grid, 3) == 0);
    CHECK(expect_grid_failure(&layer, &noise, NULL, 16) == 0);
    grid.size_x = (size_t)INT32_MAX + 1u;
    CHECK(expect_grid_failure(&layer, &noise, &grid, 16) == 0);
    grid.size_x = 2;
    grid.size_z = (size_t)INT32_MAX + 1u;
    CHECK(expect_grid_failure(&layer, &noise, &grid, 16) == 0);
    grid.size_x = 46341;
    grid.size_z = 46341;
    CHECK(expect_grid_failure(&layer, &noise, &grid, 16) == 0);
    grid.size_x = INT32_MAX;
    grid.size_z = INT32_MAX;
    CHECK(expect_grid_failure(&layer, &noise, &grid, 16) == 0);
    grid.size_x = SIZE_MAX;
    grid.size_z = 2;
    CHECK(expect_grid_failure(&layer, &noise, &grid, 16) == 0);
    double output[16];
    double saved[16];
    prefill(output, 16);
    memcpy(saved, output, sizeof(saved));
    grid = (mc_noise_simplex_grid){0, 0, 2, 2, 1, 1};
    CHECK(!mc_noise_simplex_add(NULL, &grid, 1, output, 16));
    CHECK(!mc_noise_simplex_octaves_fill(NULL, &grid, 0.5, 0.5, output, 16));
    CHECK(memcmp(output, saved, sizeof(output)) == 0);
    CHECK(!mc_noise_simplex_add(&layer, &grid, 1, NULL, 4));
    CHECK(!mc_noise_simplex_octaves_fill(&noise, &grid, 0.5, 0.5, NULL, 4));
    CHECK(!mc_noise_simplex_add(&layer, &grid, 1, NULL, 0));
    CHECK(!mc_noise_simplex_octaves_fill(&noise, &grid, 0.5, 0.5, NULL, 0));
    for (size_t dimension = 0; dimension < 2; ++dimension) {
        grid = (mc_noise_simplex_grid){0, 0, 2, 2, 1, 1};
        if (dimension == 0)
            grid.size_x = 0;
        else
            grid.size_z = 0;
        prefill(output, 16);
        memcpy(saved, output, sizeof(saved));
        CHECK(mc_noise_simplex_add(&layer, &grid, 1, output, 16));
        CHECK(memcmp(output, saved, sizeof(output)) == 0);
        CHECK(mc_noise_simplex_add(&layer, &grid, 1, NULL, 0));
        CHECK(mc_noise_simplex_octaves_fill(&noise, &grid, 0.5, 0.5, NULL, 0));
        CHECK(!mc_noise_simplex_add(&layer, &grid, 1, NULL, 1));
        CHECK(!mc_noise_simplex_octaves_fill(&noise, &grid, 0.5, 0.5, NULL, 1));
        CHECK(mc_noise_simplex_octaves_fill(&noise, &grid, 0.5, 0.5, output, 16));
        for (size_t i = 0; i < 16; ++i)
            CHECK(bits_of(output[i]) == 0);
    }
    grid = (mc_noise_simplex_grid){0};
    CHECK(expect_grid_failure(&layer, &noise, &grid, SIZE_MAX / sizeof(double) + 1u) == 0);
    grid.size_x = (size_t)INT32_MAX + 1u;
    CHECK(expect_grid_failure(&layer, &noise, &grid, 16) == 0);
    return 0;
}

int main(void)
{
    if (test_constants_and_construction() || test_constructor_order() ||
        test_observed_constructor_states() || test_observed_grids() ||
        test_invalid_constructors() || test_grid_validation_and_zero_sizes())
        return 1;
    printf("Simplex: %u checks passed\n", checks);
    return 0;
}
