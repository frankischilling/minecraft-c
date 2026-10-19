#include "caves_internal.h"

#include "../../support/java_random.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "caves_geometry_fixtures.h"

#define ARRAY_LENGTH(values) (sizeof(values) / sizeof((values)[0]))

enum { BLOCKS = 32768, LENGTH = 32771, GUARDS = 16 };

_Static_assert(MC_TERRAIN_CAVE_BLOCKS == 32768u, "original cave block prefix");
_Static_assert(MC_TERRAIN_NORMAL_CAVE_RANGE == 8u, "original normal cave range");
_Static_assert(ARRAY_LENGTH(observed_geometry_cases) == 44, "paired protected fixture count");

typedef struct geometry_buffer {
    uint8_t before[GUARDS];
    uint8_t blocks[LENGTH];
    uint8_t after[GUARDS];
} geometry_buffer;

/* Large table and block snapshots use caller-owned static storage. */
static mc_math_trig table;
static mc_math_trig table_before;
static geometry_buffer first;
static geometry_buffer second;
static geometry_buffer peer_before;
static uint8_t expected[LENGTH];
static uint8_t input_before[LENGTH];
static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static float float_from_bits(uint32_t bits)
{
    float value;
    _Static_assert(sizeof(bits) == sizeof(value), "binary32 storage required");
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t float_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static double double_from_bits(uint64_t bits)
{
    double value;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 storage required");
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint64_t double_bits(double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int decode(caves_geometry_blocks reference, uint8_t output[LENGTH])
{
    CHECK(reference.offset <= ARRAY_LENGTH(observed_geometry_runs));
    CHECK(reference.count <= ARRAY_LENGTH(observed_geometry_runs) - reference.offset);
    size_t position = 0;
    for (size_t i = 0; i < reference.count; ++i) {
        const caves_geometry_run *run = &observed_geometry_runs[reference.offset + i];
        CHECK(run->count > 0 && run->count <= LENGTH - position);
        memset(output + position, run->value, run->count);
        position += run->count;
    }
    CHECK(position == LENGTH);
    return 0;
}

static int check_bytes(const uint8_t *actual, const uint8_t *wanted, size_t length,
                       const char *label)
{
    for (size_t i = 0; i < length; ++i) {
        ++checks;
        if (actual[i] != wanted[i]) {
            fprintf(stderr, "%s byte %zu: got %u, expected %u\n", label, i, (unsigned int)actual[i],
                    (unsigned int)wanted[i]);
            return 1;
        }
    }
    return 0;
}

static int check_following(const mc_java_random *random, const uint64_t following[4])
{
    mc_java_random copy = *random;
    mc_java_random before = *random;
    for (size_t i = 0; i < 4; ++i) {
        CHECK((uint64_t)mc_java_random_next_i64(&copy) == following[i]);
    }
    CHECK(mc_test_java_random_equal(random, &before));
    return 0;
}

static int check_consumption(const caves_geometry_fixture *fixture)
{
    const caves_geometry_consumption *observed = &fixture->consumption;
    CHECK(strcmp(observed->basis, "continuation_match") == 0);
    CHECK(observed->candidates_matched == 1);
    CHECK(observed->next_float_calls == (uint32_t)fixture->room);
    CHECK(observed->next_long_calls == 1 || (!fixture->room && observed->next_long_calls == 3));
    CHECK(observed->fork_inferred == (!fixture->room && observed->next_long_calls == 3));

    /* The original observer inferred these counts by matching independent
     * actual JDK Random candidates. Here the accepted C primitive sequence
     * is compared to that captured continuation; it creates no expectations. */
    mc_java_random candidate;
    mc_java_random_seed(&candidate, fixture->seed);
    CHECK(check_following(&candidate, fixture->before) == 0);
    for (uint32_t i = 0; i < observed->next_float_calls; ++i) {
        (void)mc_java_random_next_float(&candidate);
    }
    for (uint32_t i = 0; i < observed->next_long_calls; ++i) {
        (void)mc_java_random_next_i64(&candidate);
    }
    CHECK(check_following(&candidate, fixture->after) == 0);
    return 0;
}

static int invoke_fixture(const caves_geometry_fixture *fixture, geometry_buffer *buffer,
                          mc_java_random *random, const geometry_buffer *peer,
                          const mc_java_random *peer_random)
{
    memset(buffer->before, 0x9c, sizeof(buffer->before));
    memset(buffer->after, 0xe6, sizeof(buffer->after));
    CHECK(decode(fixture->arrays[0], buffer->blocks) == 0);
    memcpy(input_before, buffer->blocks, sizeof(input_before));
    mc_java_random_seed(random, fixture->seed);
    CHECK(check_following(random, fixture->before) == 0);
    peer_before = *peer;
    mc_java_random saved_peer_random = *peer_random;
    mc_terrain_cave_context context = {&table, random, buffer->blocks, fixture->chunk_x,
                                       fixture->chunk_z};

    double x = double_from_bits(fixture->centers[0]);
    double y = double_from_bits(fixture->centers[1]);
    double z = double_from_bits(fixture->centers[2]);
    CHECK(double_bits(x) == fixture->centers[0]);
    CHECK(double_bits(y) == fixture->centers[1]);
    CHECK(double_bits(z) == fixture->centers[2]);
    CHECK(isfinite(x) && x >= 0.0 && x <= 15.0);
    CHECK(isfinite(y) && y >= 0.0 && y <= 127.0);
    CHECK(isfinite(z) && z >= 0.0 && z <= 15.0);
    CHECK(fixture->chunk_x == 0 && fixture->chunk_z == 0);
    if (fixture->room) {
        /* Actual wrapper owns the width draw and its private step/ratio rules. */
        mc_terrain_cave_room(&context, x, y, z);
    } else {
        float width = float_from_bits(fixture->angles[0]);
        float yaw = float_from_bits(fixture->angles[1]);
        float pitch = float_from_bits(fixture->angles[2]);
        CHECK(float_bits(width) == fixture->angles[0]);
        CHECK(float_bits(yaw) == fixture->angles[1]);
        CHECK(float_bits(pitch) == fixture->angles[2]);
        CHECK(isfinite(width) && width >= 0.0f && width <= 3.0f);
        CHECK(isfinite(yaw) && yaw >= 0.0f && yaw <= 0x1.921fb6p+2f);
        CHECK(isfinite(pitch) && pitch >= -0.125f && pitch <= 0.125f);
        CHECK(fixture->end >= 85 && fixture->end <= 112);
        CHECK(fixture->step >= 0 && fixture->step < fixture->end);
        mc_terrain_cave_path(&context, x, y, z, width, yaw, pitch, fixture->step, fixture->end,
                             1.0);
    }

    CHECK(context.table == &table);
    CHECK(context.random == random);
    CHECK(context.blocks == buffer->blocks);
    CHECK(context.chunk_x == fixture->chunk_x && context.chunk_z == fixture->chunk_z);
    CHECK(decode(fixture->arrays[1], expected) == 0);
    CHECK(check_bytes(buffer->blocks, expected, LENGTH, fixture->label) == 0);
    size_t changed = 0;
    for (size_t i = 0; i < BLOCKS; ++i) {
        if (input_before[i] != buffer->blocks[i]) {
            ++changed;
        }
    }
    CHECK(changed == fixture->changed);
    CHECK(check_bytes(buffer->blocks + BLOCKS, input_before + BLOCKS, LENGTH - BLOCKS,
                      "retained suffix") == 0);
    for (size_t i = 0; i < GUARDS; ++i) {
        CHECK(buffer->before[i] == 0x9c);
        CHECK(buffer->after[i] == 0xe6);
    }
    CHECK(check_following(random, fixture->after) == 0);
    CHECK(memcmp(&table, &table_before, sizeof(table)) == 0);
    CHECK(memcmp(peer, &peer_before, sizeof(peer_before)) == 0);
    CHECK(mc_test_java_random_equal(peer_random, &saved_peer_random));
    return 0;
}

int main(void)
{
    CHECK(mc_math_trig_init(&table));
    memcpy(&table_before, &table, sizeof(table));
    memset(&first, 0x39, sizeof(first));
    memset(&second, 0xb7, sizeof(second));
    mc_java_random first_random;
    mc_java_random second_random;
    mc_java_random_seed(&first_random, INT64_C(-991));
    mc_java_random_seed(&second_random, INT64_C(258));
    for (size_t i = 0; i < ARRAY_LENGTH(observed_geometry_cases); ++i) {
        const caves_geometry_fixture *fixture = &observed_geometry_cases[i];
        CHECK(check_consumption(fixture) == 0);
        CHECK(invoke_fixture(fixture, &first, &first_random, &second, &second_random) == 0);
        CHECK(invoke_fixture(fixture, &second, &second_random, &first, &first_random) == 0);
        CHECK(memcmp(&first, &second, sizeof(first)) == 0);
        CHECK(mc_test_java_random_equal(&first_random, &second_random));
    }
    printf("Cave geometry: %u checks passed\n", checks);
    return 0;
}
