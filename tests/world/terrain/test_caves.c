#include "mc/world/terrain/caves.h"

#include "../../support/java_random.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "caves_fixtures.h"

#define ARRAY_LENGTH(values) (sizeof(values) / sizeof((values)[0]))

enum { BLOCKS = 32768, GOLDEN_LENGTH = 32771, GUARD_BYTES = 16, CAPACITY = 32799 };

_Static_assert(MC_TERRAIN_NORMAL_CAVE_RANGE == 8u, "normal cave range");
_Static_assert(MC_TERRAIN_CAVE_BLOCKS == 32768u, "normal cave prefix shape");
_Static_assert(ARRAY_LENGTH(observed_caves_cases) == 10, "paired public fixture count");

typedef struct guarded_blocks {
    uint8_t before[GUARD_BYTES];
    uint8_t blocks[CAPACITY];
    uint8_t after[GUARD_BYTES];
} guarded_blocks;

/* Keep complete tables and block snapshots off the native Windows stack. */
static mc_math_trig table;
static mc_math_trig table_before;
static uint8_t working[GOLDEN_LENGTH];
static uint8_t original[GOLDEN_LENGTH];
static uint8_t expected[GOLDEN_LENGTH];
static uint8_t pass_input[GOLDEN_LENGTH];
static uint8_t first_output[GOLDEN_LENGTH];
static uint8_t other[GOLDEN_LENGTH];
static uint8_t other_before[GOLDEN_LENGTH];
static guarded_blocks guarded;
static guarded_blocks guarded_before;
static guarded_blocks property_output;
static mc_java_random rejected_random;
static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int decode(caves_block_fixture fixture, uint8_t output[GOLDEN_LENGTH])
{
    CHECK(fixture.offset <= ARRAY_LENGTH(observed_caves_runs));
    CHECK(fixture.count <= ARRAY_LENGTH(observed_caves_runs) - fixture.offset);
    size_t position = 0;
    for (size_t i = 0; i < fixture.count; ++i) {
        const caves_byte_run *run = &observed_caves_runs[fixture.offset + i];
        CHECK(run->count > 0 && run->count <= GOLDEN_LENGTH - position);
        memset(output + position, run->value, run->count);
        position += run->count;
    }
    CHECK(position == GOLDEN_LENGTH);
    return 0;
}

static int check_bytes(const uint8_t *actual, const uint8_t *wanted, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        ++checks;
        if (actual[i] != wanted[i]) {
            fprintf(stderr, "block byte %zu: got %u, expected %u\n", i, (unsigned int)actual[i],
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

static size_t changed_bytes(const uint8_t *before, const uint8_t *after)
{
    size_t changed = 0;
    for (size_t i = 0; i < BLOCKS; ++i) {
        if (before[i] != after[i]) {
            ++changed;
        }
    }
    return changed;
}

static int check_ineligible(const uint8_t *before, const uint8_t *after)
{
    for (size_t i = 0; i < BLOCKS; ++i) {
        if (before[i] != 1 && before[i] != 2 && before[i] != 3) {
            CHECK(before[i] == after[i]);
        }
    }
    return 0;
}

static int check_empty_gaussian_cache(const mc_java_random *random)
{
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(random->next_next_gaussian), "binary64 storage required");
    memcpy(&bits, &random->next_next_gaussian, sizeof(bits));
    CHECK(bits == UINT64_C(0));
    CHECK(!random->have_next_next_gaussian);
    return 0;
}

static int check_golden(const caves_case_fixture *fixture, size_t pass, const uint8_t *actual,
                        const mc_java_random *random)
{
    CHECK(decode(fixture->arrays[pass + 1], expected) == 0);
    CHECK(check_bytes(actual, expected, GOLDEN_LENGTH) == 0);
    CHECK(check_empty_gaussian_cache(random) == 0);
    CHECK(check_following(random, fixture->following[pass]) == 0);
    CHECK(memcmp(&table, &table_before, sizeof(table)) == 0);
    return 0;
}

static int test_original_public_cases(void)
{
    mc_java_random cave_random;
    mc_java_random surface_random;
    mc_java_random surface_before;
    mc_java_random_seed(&surface_random, INT64_C(-872341));
    (void)mc_java_random_next_i64(&surface_random);
    surface_before = surface_random;
    for (size_t i = 0; i < ARRAY_LENGTH(observed_caves_cases); ++i) {
        const caves_case_fixture *fixture = &observed_caves_cases[i];
        CHECK(decode(fixture->arrays[0], original) == 0);
        memcpy(working, original, sizeof(working));
        /* Public entry seeds the cave-owned Random before reading its state. */
        memset(&cave_random, 0xa5, sizeof(cave_random));
        for (size_t pass = 0; pass < 3; ++pass) {
            if (pass == 1) {
                memcpy(working, original, sizeof(working));
            }
            memcpy(pass_input, working, sizeof(pass_input));
            CHECK(mc_terrain_normal_caves(&table, fixture->seed, fixture->chunk_x, fixture->chunk_z,
                                          &cave_random, working, sizeof(working)));
            CHECK(check_golden(fixture, pass, working, &cave_random) == 0);
            CHECK(changed_bytes(pass_input, working) == fixture->changed[pass]);
            CHECK(memcmp(working + BLOCKS, pass_input + BLOCKS, GOLDEN_LENGTH - BLOCKS) == 0);
            CHECK(check_ineligible(pass_input, working) == 0);
            CHECK(mc_test_java_random_equal(&surface_random, &surface_before));
            if (pass == 0) {
                memcpy(first_output, working, sizeof(first_output));
            } else if (pass == 1) {
                CHECK((memcmp(working, first_output, sizeof(working)) == 0) ==
                      fixture->relationships[0]);
                CHECK((memcmp(working, original, sizeof(working)) != 0) ==
                      fixture->relationships[1]);
            } else {
                /* Equality here is observed only for the recorded corpus. */
                CHECK((memcmp(working, pass_input, sizeof(working)) == 0) ==
                      fixture->relationships[2]);
            }
        }

        /* The same recorded first result must survive an unrelated prior seed. */
        memcpy(working, original, sizeof(working));
        mc_java_random_seed(&cave_random, INT64_C(9248657));
        (void)mc_java_random_next_i64(&cave_random);
        /* This fresh-output projection also discards an initialized cache. */
        cave_random.next_next_gaussian = -0x1.23456789abcdep+1;
        cave_random.have_next_next_gaussian = true;
        CHECK(mc_terrain_normal_caves(&table, fixture->seed, fixture->chunk_x, fixture->chunk_z,
                                      &cave_random, working, sizeof(working)));
        CHECK(check_golden(fixture, 0, working, &cave_random) == 0);
    }
    return 0;
}

static int test_independent_owners(void)
{
    const caves_case_fixture *a = &observed_caves_cases[2];
    const caves_case_fixture *b = &observed_caves_cases[6];
    mc_java_random random_a;
    mc_java_random random_b;
    mc_java_random before_a;
    mc_java_random before_b;
    CHECK(decode(a->arrays[0], working) == 0);
    CHECK(decode(b->arrays[0], other) == 0);
    memset(&random_a, 0x3c, sizeof(random_a));
    memset(&random_b, 0x81, sizeof(random_b));
    CHECK(mc_terrain_normal_caves(&table, a->seed, a->chunk_x, a->chunk_z, &random_a, working,
                                  sizeof(working)));
    CHECK(check_golden(a, 0, working, &random_a) == 0);
    before_a = random_a;
    memcpy(other_before, working, sizeof(other_before));
    CHECK(mc_terrain_normal_caves(&table, b->seed, b->chunk_x, b->chunk_z, &random_b, other,
                                  sizeof(other)));
    CHECK(check_golden(b, 0, other, &random_b) == 0);
    CHECK(memcmp(working, other_before, sizeof(working)) == 0);
    CHECK(mc_test_java_random_equal(&random_a, &before_a));
    before_b = random_b;
    memcpy(other_before, other, sizeof(other_before));
    CHECK(decode(a->arrays[0], working) == 0);
    CHECK(mc_terrain_normal_caves(&table, a->seed, a->chunk_x, a->chunk_z, &random_a, working,
                                  sizeof(working)));
    CHECK(check_golden(a, 0, working, &random_a) == 0);
    CHECK(memcmp(other, other_before, sizeof(other)) == 0);
    CHECK(mc_test_java_random_equal(&random_b, &before_b));
    return 0;
}

/* Everything below uses explicitly constructed C inputs, not original-derived
 * numerical cave fixtures. None of these tests calls an invalid original. */
static void fill_guarded(int eligible)
{
    static const uint8_t ineligible[] = {0, 4, 5, 7, 10, 11, 12, 13, 24, 79, 127, 128, 254, 255};
    memset(guarded.before, 0x93, sizeof(guarded.before));
    memset(guarded.after, 0xe7, sizeof(guarded.after));
    for (size_t i = 0; i < CAPACITY; ++i) {
        guarded.blocks[i] = (uint8_t)(i * 37u + 11u);
    }
    for (size_t i = 0; i < BLOCKS; ++i) {
        guarded.blocks[i] = eligible ? 1 : ineligible[i % ARRAY_LENGTH(ineligible)];
    }
}

static int check_guards_and_suffix(void)
{
    CHECK(memcmp(guarded.before, guarded_before.before, sizeof(guarded.before)) == 0);
    CHECK(memcmp(guarded.after, guarded_before.after, sizeof(guarded.after)) == 0);
    CHECK(memcmp(guarded.blocks + BLOCKS, guarded_before.blocks + BLOCKS, CAPACITY - BLOCKS) == 0);
    CHECK(memcmp(&table, &table_before, sizeof(table)) == 0);
    return 0;
}

static int property_call(int32_t x, int32_t z, size_t capacity, int eligible)
{
    mc_java_random random;
    mc_java_random first_random;
    fill_guarded(eligible);
    guarded_before = guarded;
    memset(&random, 0xb6, sizeof(random));
    CHECK(mc_terrain_normal_caves(&table, INT64_C(-1), x, z, &random, guarded.blocks, capacity));
    CHECK(check_empty_gaussian_cache(&random) == 0);
    CHECK(check_guards_and_suffix() == 0);
    if (!eligible) {
        CHECK(memcmp(&guarded, &guarded_before, sizeof(guarded)) == 0);
    }
    property_output = guarded;
    first_random = random;
    guarded = guarded_before;
    CHECK(mc_terrain_normal_caves(&table, INT64_C(-1), x, z, &random, guarded.blocks, capacity));
    CHECK(memcmp(&guarded, &property_output, sizeof(guarded)) == 0);
    CHECK(mc_test_java_random_equal(&random, &first_random));
    CHECK(check_guards_and_suffix() == 0);
    return 0;
}

static int test_constructed_boundaries_and_bytes(void)
{
    static const int32_t endpoints[] = {INT32_MIN + 8, 0, INT32_MAX - 9};
    static const int32_t transitions[] = {INT32_C(134217727), INT32_C(134217728),
                                          -INT32_C(134217728), -INT32_C(134217729)};
    static const size_t capacities[] = {BLOCKS, GOLDEN_LENGTH, CAPACITY};
    for (size_t i = 0; i < ARRAY_LENGTH(endpoints); ++i) {
        for (size_t j = 0; j < ARRAY_LENGTH(endpoints); ++j) {
            CHECK(property_call(endpoints[i], endpoints[j], CAPACITY, 0) == 0);
        }
    }
    for (size_t i = 0; i < ARRAY_LENGTH(transitions); ++i) {
        CHECK(property_call(transitions[i], 0, CAPACITY, 0) == 0);
        CHECK(property_call(0, transitions[i], CAPACITY, 0) == 0);
    }
    /* Eligible endpoint/product inputs exercise bounded wrapped geometry. The
     * assertions are safety, preservation and repeatability, not golden bytes. */
    CHECK(property_call(endpoints[0], endpoints[2], CAPACITY, 1) == 0);
    CHECK(property_call(endpoints[2], endpoints[0], CAPACITY, 1) == 0);
    CHECK(property_call(transitions[1], transitions[3], CAPACITY, 1) == 0);
    for (size_t i = 0; i < ARRAY_LENGTH(capacities); ++i) {
        CHECK(property_call(0, 0, capacities[i], 0) == 0);
    }

    /* All byte patterns are valid input. Only the evidenced eligible IDs can
     * change; private paired fixtures separately cover water/grass geometry. */
    fill_guarded(0);
    for (size_t i = 0; i < BLOCKS; ++i) {
        guarded.blocks[i] = (uint8_t)i;
    }
    guarded_before = guarded;
    mc_java_random random;
    CHECK(mc_terrain_normal_caves(&table, INT64_C(0), 0, 0, &random, guarded.blocks, CAPACITY));
    CHECK(check_empty_gaussian_cache(&random) == 0);
    CHECK(check_ineligible(guarded_before.blocks, guarded.blocks) == 0);
    CHECK(check_guards_and_suffix() == 0);
    return 0;
}

static int rejected(const mc_math_trig *input_table, int32_t x, int32_t z, mc_java_random *random,
                    uint8_t *blocks, size_t length)
{
    /* Rejected calls preserve even deliberately poisoned prior storage. */
    unsigned char before[sizeof(rejected_random)];
    memcpy(before, &rejected_random, sizeof(before));
    guarded_before = guarded;
    CHECK(!mc_terrain_normal_caves(input_table, INT64_C(0), x, z, random, blocks, length));
    CHECK(memcmp(&guarded, &guarded_before, sizeof(guarded)) == 0);
    CHECK(memcmp(&rejected_random, before, sizeof(before)) == 0);
    CHECK(memcmp(&table, &table_before, sizeof(table)) == 0);
    return 0;
}

static int test_preflight_preservation(void)
{
    static const int32_t invalid[] = {INT32_MIN, INT32_MIN + 7, INT32_MAX - 8, INT32_MAX};
    static const int32_t valid[] = {INT32_MIN + 8, 0, INT32_MAX - 9};
    fill_guarded(1);
    mc_java_random_seed(&rejected_random, INT64_C(6389248));
    (void)mc_java_random_next_i64(&rejected_random);
    rejected_random.next_next_gaussian = 0x1.abcdef0123456p-2;
    rejected_random.have_next_next_gaussian = true;
    CHECK(rejected(NULL, 0, 0, &rejected_random, guarded.blocks, CAPACITY) == 0);
    CHECK(rejected(&table, 0, 0, NULL, guarded.blocks, CAPACITY) == 0);
    CHECK(rejected(&table, 0, 0, &rejected_random, NULL, CAPACITY) == 0);
    CHECK(rejected(&table, 0, 0, &rejected_random, guarded.blocks, 0) == 0);
    CHECK(rejected(&table, 0, 0, &rejected_random, guarded.blocks, BLOCKS - 1) == 0);
    for (size_t i = 0; i < ARRAY_LENGTH(invalid); ++i) {
        for (size_t j = 0; j < ARRAY_LENGTH(valid); ++j) {
            CHECK(rejected(&table, invalid[i], valid[j], &rejected_random, guarded.blocks,
                           CAPACITY) == 0);
            CHECK(rejected(&table, valid[j], invalid[i], &rejected_random, guarded.blocks,
                           CAPACITY) == 0);
        }
    }
    memset(&rejected_random, 0x8d, sizeof(rejected_random));
    CHECK(rejected(&table, 0, 0, &rejected_random, guarded.blocks, BLOCKS - 1) == 0);
    CHECK(rejected(NULL, 0, 0, &rejected_random, guarded.blocks, CAPACITY) == 0);
    return 0;
}

int main(void)
{
    CHECK(mc_math_trig_init(&table));
    memcpy(&table_before, &table, sizeof(table_before));
    if (test_original_public_cases() != 0 || test_independent_owners() != 0 ||
        test_constructed_boundaries_and_bytes() != 0 || test_preflight_preservation() != 0) {
        return 1;
    }
    printf("Caves: %u checks passed\n", checks);
    return 0;
}
