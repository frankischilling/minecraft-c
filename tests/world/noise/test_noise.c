#include "mc/world/noise/noise.h"

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

static uint64_t bits_of(double value) {
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 storage required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double from_bits(uint64_t bits) {
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void prefill(double *values, size_t length) {
    for (size_t i = 0; i < length; ++i)
        values[i] = (double)((int)(i % 11u) - 5) * 0.125;
}

/* These fixed numerical observations came from the untouched client/server
 * classes am/ab and uf/mp on Temurin 17, not from a second C noise algorithm. */
static const uint64_t seed_zero_next[] = {
    UINT64_C(0xa15aa3684d0173ab), UINT64_C(0xd15dcc46f84f5c2a), UINT64_C(0xbdb9ee86519b4e29),
    UINT64_C(0xa1a566f569d0f53c)};

static int test_constructor_state(void) {
    static const uint16_t prefix[] = {140, 157, 53, 179, 117, 253, 229, 104,
                                      247, 35,  20, 221, 25,  51,  211, 18};
    mc_java_random random;
    mc_java_random_seed(&random, 0);
    mc_noise_perlin noise;
    CHECK(mc_noise_perlin_init(&noise, &random));
    CHECK(bits_of(noise.offset_x) == UINT64_C(0x406764168ea6ca89));
    CHECK(bits_of(noise.offset_y) == UINT64_C(0x404ec9e5b3672e14));
    CHECK(bits_of(noise.offset_z) == UINT64_C(0x406465b93a78ef81));
    CHECK(memcmp(noise.permutation, prefix, sizeof(prefix)) == 0);
    unsigned int occurrences[256] = {0};
    for (size_t i = 0; i < 256; ++i) {
        CHECK(noise.permutation[i] < 256);
        CHECK(noise.permutation[i] == noise.permutation[i + 256u]);
        ++occurrences[noise.permutation[i]];
    }
    for (size_t i = 0; i < 256; ++i)
        CHECK(occurrences[i] == 1);
    for (size_t i = 0; i < 4; ++i)
        CHECK((uint64_t)mc_java_random_next_i64(&random) == seed_zero_next[i]);
    static const int64_t seeds[] = {INT64_MIN, -1, 1, INT64_MAX, INT64_C(123456789)};
    for (size_t seed = 0; seed < sizeof(seeds) / sizeof(seeds[0]); ++seed) {
        mc_java_random_seed(&random, seeds[seed]);
        CHECK(mc_noise_perlin_init(&noise, &random));
        CHECK(isfinite(noise.offset_x) && noise.offset_x >= 0 && noise.offset_x < 256);
        CHECK(isfinite(noise.offset_y) && noise.offset_y >= 0 && noise.offset_y < 256);
        CHECK(isfinite(noise.offset_z) && noise.offset_z >= 0 && noise.offset_z < 256);
        memset(occurrences, 0, sizeof(occurrences));
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

static int test_scalar_observations(void) {
    static const struct {
        double x;
        double y;
        double z;
        uint64_t expected;
    } points[] = {
        {0, 0, 0, UINT64_C(0xbfb87d67ea2e10e4)},      {0.25, -0.5, 3, UINT64_C(0xbfcbc67fadfed28b)},
        {0, 0.0625, 0, UINT64_C(0xbfc506c0d62e376f)}, {0, 0.125, 0, UINT64_C(0xbfcb7a0ace42bb80)},
        {0, 0.1875, 0, UINT64_C(0xbfcedece6708ef75)}, {0, 10, 0, UINT64_C(0xbfaec7d155a123c1)}};

    mc_java_random random;
    mc_java_random_seed(&random, 0);
    mc_noise_perlin noise;
    CHECK(mc_noise_perlin_init(&noise, &random));
    mc_noise_perlin saved = noise;
    for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); ++i) {
        CHECK(bits_of(mc_noise_perlin_sample(&noise, points[i].x, points[i].y, points[i].z)) ==
              points[i].expected);
        CHECK(bits_of(mc_noise_perlin_sample(&noise, points[i].x, points[i].y, points[i].z)) ==
              points[i].expected);
    }
    static const uint64_t boundaries[][4] = {
        {UINT64_C(0xc06764168ea6ca89), UINT64_C(0xc04ec9e5b3672e14), UINT64_C(0xc06465b93a78ef81),
         UINT64_C(0)},
        {UINT64_C(0xc06764168ea6ca8a), UINT64_C(0xc04ec9e5b3672e15), UINT64_C(0xc06465b93a78ef82),
         UINT64_C(0)},
        {UINT64_C(0xc06764168ea6ca88), UINT64_C(0xc04ec9e5b3672e13), UINT64_C(0xc06465b93a78ef80),
         UINT64_C(0x37b3fffffffffd0d)},
    };
    for (size_t i = 0; i < sizeof(boundaries) / sizeof(boundaries[0]); ++i)
        CHECK(bits_of(mc_noise_perlin_sample(&noise, from_bits(boundaries[i][0]),
                                             from_bits(boundaries[i][1]),
                                             from_bits(boundaries[i][2]))) == boundaries[i][3]);
    CHECK(memcmp(&noise, &saved, sizeof(noise)) == 0);
    for (size_t i = 0; i < 4; ++i)
        CHECK((uint64_t)mc_java_random_next_i64(&random) == seed_zero_next[i]);
    return 0;
}

static int test_bulk_observations(void) {
    static const uint64_t height_one[] = {
        UINT64_C(0xbfe19b172975fc00), UINT64_C(0xbfe70613a0c31139), UINT64_C(0xbfd6187760625228),
        UINT64_C(0xbf99e48a8e3d7e98), UINT64_C(0xbfc0000000000000), UINT64_C(0),
        UINT64_C(0x3fc0000000000000)};
    static const uint64_t cached_y[] = {UINT64_C(0xbfe70facfd45c21c), UINT64_C(0xbfe730802f30b52a),
                                        UINT64_C(0xbfe6bc342ae33c39), UINT64_C(0xbfe5845269836592),
                                        UINT64_C(0xbfc0000000000000), UINT64_C(0),
                                        UINT64_C(0x3fc0000000000000)};
    mc_java_random random;
    mc_java_random_seed(&random, 0);
    mc_noise_perlin noise;
    CHECK(mc_noise_perlin_init(&noise, &random));
    mc_noise_perlin saved = noise;
    mc_noise_grid grid = {0, 10, 0, 2, 1, 2, 0.5, 1, 0.5};
    double output[7];
    prefill(output, 7);
    CHECK(mc_noise_perlin_add(&noise, &grid, 1, output, 7));
    for (size_t i = 0; i < 7; ++i)
        CHECK(bits_of(output[i]) == height_one[i]);
    CHECK(bits_of(output[0]) != bits_of(-0.625 + mc_noise_perlin_sample(&noise, 0, 10, 0)));
    double independent[7];
    prefill(independent, 7);
    grid.origin_y = -12345;
    grid.scale_y = 0.125;
    CHECK(mc_noise_perlin_add(&noise, &grid, 1, independent, 7));
    CHECK(memcmp(independent, output, sizeof(output)) == 0);
    grid = (mc_noise_grid){0, 0, 0, 1, 4, 1, 1, 0.0625, 1};
    prefill(output, 7);
    CHECK(mc_noise_perlin_add(&noise, &grid, 1, output, 7));
    for (size_t i = 0; i < 7; ++i)
        CHECK(bits_of(output[i]) == cached_y[i]);
    CHECK(bits_of(output[1]) != bits_of(-0.5 + mc_noise_perlin_sample(&noise, 0, 0.0625, 0)));
    CHECK(memcmp(&noise, &saved, sizeof(noise)) == 0);
    for (size_t i = 0; i < 4; ++i)
        CHECK((uint64_t)mc_java_random_next_i64(&random) == seed_zero_next[i]);
    return 0;
}

static int test_octave_observations_and_order(void) {
    static const uint64_t expected[] = {UINT64_C(0xc0010366a8e07e76),
                                        UINT64_C(0xbffd290e3bcb77ae),
                                        UINT64_C(0xc00025eb5750107a),
                                        UINT64_C(0xbfff0b33fb4db4bd),
                                        UINT64_C(0),
                                        UINT64_C(0),
                                        UINT64_C(0)};
    static const uint64_t next[] = {UINT64_C(0xe5ce6fc6159a59ff), UINT64_C(0x4ee70a3eb1601a89),
                                    UINT64_C(0x5d0f05ebbd2f193f), UINT64_C(0x7a5712a42bf2462a)};
    mc_java_random random;
    mc_java_random_seed(&random, 0);
    mc_noise_perlin layers[4];
    mc_noise_octaves noise;
    CHECK(mc_noise_octaves_init(&noise, layers, 4, &random));
    CHECK(noise.layers == layers && noise.count == 4);
    CHECK(bits_of(mc_noise_octaves_sample2(&noise, 0.25, -0.5)) == UINT64_C(0x3f71b6d5a1046500));
    mc_noise_grid grid = {-1, 0, 3, 2, 2, 1, 0.5, 0.25, 0.5};
    double output[7];
    prefill(output, 7);
    CHECK(mc_noise_octaves_fill(&noise, &grid, output, 7));
    for (size_t i = 0; i < 7; ++i)
        CHECK(bits_of(output[i]) == expected[i]);
    for (size_t i = 0; i < 4; ++i)
        CHECK((uint64_t)mc_java_random_next_i64(&random) == next[i]);
    /* A caller may construct consecutive objects from one shared generator. */
    mc_java_random manual;
    mc_java_random_seed(&manual, 0);
    mc_noise_perlin consecutive[4];
    for (size_t i = 0; i < 4; ++i)
        CHECK(mc_noise_perlin_init(&consecutive[i], &manual));
    CHECK(memcmp(consecutive, layers, sizeof(layers)) == 0);
    for (size_t i = 0; i < 4; ++i)
        CHECK((uint64_t)mc_java_random_next_i64(&manual) == next[i]);
    mc_noise_octaves empty;
    CHECK(mc_noise_octaves_init(&empty, NULL, 0, NULL));
    CHECK(empty.layers == NULL && empty.count == 0);
    CHECK(bits_of(mc_noise_octaves_sample2(&empty, 1, -2)) == 0);
    prefill(output, 7);
    CHECK(mc_noise_octaves_fill(&empty, &grid, output, 7));
    for (size_t i = 0; i < 7; ++i)
        CHECK(bits_of(output[i]) == 0);
    return 0;
}

static int test_invalid_constructors(void) {
    mc_java_random random;
    mc_java_random_seed(&random, 123);
    mc_java_random random_saved = random;
    mc_noise_perlin layer;
    mc_noise_perlin layer_saved;
    memset(&layer, 0xa5, sizeof(layer));
    memcpy(&layer_saved, &layer, sizeof(layer));
    CHECK(!mc_noise_perlin_init(NULL, &random));
    CHECK(mc_test_java_random_equal(&random, &random_saved));
    CHECK(!mc_noise_perlin_init(&layer, NULL));
    CHECK(memcmp(&layer, &layer_saved, sizeof(layer)) == 0);
    mc_noise_octaves noise;
    mc_noise_octaves saved;
    memset(&noise, 0xa5, sizeof(noise));
    memcpy(&saved, &noise, sizeof(noise));
    CHECK(!mc_noise_octaves_init(NULL, &layer, 1, &random));
    CHECK(!mc_noise_octaves_init(&noise, NULL, 1, &random));
    CHECK(!mc_noise_octaves_init(&noise, &layer, 1, NULL));
    CHECK(!mc_noise_octaves_init(&noise, &layer, (size_t)INT32_MAX + 1u, &random));
    CHECK(!mc_noise_octaves_init(&noise, &layer, SIZE_MAX, &random));
    CHECK(memcmp(&noise, &saved, sizeof(noise)) == 0);
    CHECK(memcmp(&layer, &layer_saved, sizeof(layer)) == 0);
    CHECK(mc_test_java_random_equal(&random, &random_saved));
    CHECK(mc_noise_octaves_init(&noise, NULL, 0, &random));
    CHECK(mc_test_java_random_equal(&random, &random_saved));
    return 0;
}

static int expect_grid_failure(const mc_noise_perlin *layer, const mc_noise_octaves *noise,
                               const mc_noise_grid *grid, size_t capacity) {
    double output[16];
    double saved[16];
    prefill(output, 16);
    memcpy(saved, output, sizeof(saved));
    CHECK(!mc_noise_perlin_add(layer, grid, 1, output, capacity));
    CHECK(memcmp(saved, output, sizeof(output)) == 0);
    CHECK(!mc_noise_octaves_fill(noise, grid, output, capacity));
    CHECK(memcmp(saved, output, sizeof(output)) == 0);
    return 0;
}

static int test_grid_validation_and_zero_sizes(void) {
    mc_java_random random;
    mc_java_random_seed(&random, 0);
    mc_noise_perlin layer;
    mc_noise_octaves noise;
    CHECK(mc_noise_octaves_init(&noise, &layer, 1, &random));
    mc_noise_grid grid = {0, 0, 0, 2, 2, 2, 1, 1, 1};
    CHECK(expect_grid_failure(&layer, &noise, &grid, 7) == 0);
    CHECK(expect_grid_failure(&layer, &noise, NULL, 16) == 0);
    grid.size_x = (size_t)INT32_MAX + 1u;
    CHECK(expect_grid_failure(&layer, &noise, &grid, 16) == 0);
    grid.size_x = 46341;
    grid.size_y = 46341;
    grid.size_z = 1;
    CHECK(expect_grid_failure(&layer, &noise, &grid, 16) == 0);
    grid.size_x = INT32_MAX;
    grid.size_y = INT32_MAX;
    grid.size_z = INT32_MAX;
    CHECK(expect_grid_failure(&layer, &noise, &grid, 16) == 0);
    grid.size_x = SIZE_MAX;
    grid.size_y = 2;
    grid.size_z = 2;
    CHECK(expect_grid_failure(&layer, &noise, &grid, 16) == 0);
    double output[16];
    double saved[16];
    prefill(output, 16);
    memcpy(saved, output, sizeof(saved));
    grid = (mc_noise_grid){0, 0, 0, 2, 2, 2, 1, 1, 1};
    CHECK(!mc_noise_perlin_add(NULL, &grid, 1, output, 16));
    CHECK(!mc_noise_octaves_fill(NULL, &grid, output, 16));
    CHECK(memcmp(output, saved, sizeof(output)) == 0);
    CHECK(!mc_noise_perlin_add(&layer, &grid, 1, NULL, 8));
    CHECK(!mc_noise_octaves_fill(&noise, &grid, NULL, 8));
    CHECK(!mc_noise_perlin_add(&layer, &grid, 1, NULL, 0));
    CHECK(!mc_noise_octaves_fill(&noise, &grid, NULL, 0));
    for (size_t dimension = 0; dimension < 3; ++dimension) {
        grid = (mc_noise_grid){0, 0, 0, 2, 2, 2, 1, 1, 1};
        if (dimension == 0)
            grid.size_x = 0;
        else if (dimension == 1)
            grid.size_y = 0;
        else
            grid.size_z = 0;
        prefill(output, 16);
        memcpy(saved, output, sizeof(saved));
        CHECK(mc_noise_perlin_add(&layer, &grid, 1, output, 16));
        CHECK(memcmp(output, saved, sizeof(output)) == 0);
        CHECK(mc_noise_perlin_add(&layer, &grid, 1, NULL, 0));
        CHECK(mc_noise_octaves_fill(&noise, &grid, NULL, 0));
        CHECK(!mc_noise_perlin_add(&layer, &grid, 1, NULL, 1));
        CHECK(!mc_noise_octaves_fill(&noise, &grid, NULL, 1));
        CHECK(mc_noise_octaves_fill(&noise, &grid, output, 16));
        for (size_t i = 0; i < 16; ++i)
            CHECK(bits_of(output[i]) == 0);
    }
    grid = (mc_noise_grid){0};
    CHECK(expect_grid_failure(&layer, &noise, &grid, SIZE_MAX / sizeof(double) + 1u) == 0);
    grid.size_x = (size_t)INT32_MAX + 1u;
    CHECK(expect_grid_failure(&layer, &noise, &grid, 16) == 0);
    return 0;
}

int main(void) {
    if (test_constructor_state() || test_scalar_observations() || test_bulk_observations() ||
        test_octave_observations_and_order() || test_invalid_constructors() ||
        test_grid_validation_and_zero_sizes())
        return 1;
    printf("Noise: %u checks passed\n", checks);
    return 0;
}
