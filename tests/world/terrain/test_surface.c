#include "mc/world/terrain/surface.h"

#include "../../support/java_random.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "surface_controlled_fixtures.h"
#include "surface_fixtures.h"

_Static_assert(MC_TERRAIN_NORMAL_SURFACE_VALUES == 256, "normal surface shape");
_Static_assert(MC_TERRAIN_NORMAL_BLOCKS == 32768, "normal block shape");

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct surface_storage {
    double fields[3][259];
    mc_terrain_surface_material materials[259];
    uint8_t blocks[32771];
} surface_storage;

static surface_storage storage;
static surface_storage saved_storage;
static uint8_t expected_blocks[32771];
static mc_terrain_normal_state state;
static mc_terrain_normal_state saved_state;

static uint64_t bits_of(double value)
{
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 storage required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static mc_terrain_normal_surface_workspace workspace(void)
{
    return (mc_terrain_normal_surface_workspace){
        {storage.fields[0], 259}, {storage.fields[1], 259}, {storage.fields[2], 259}};
}

static void prepare(void)
{
    memset(&storage, 0, sizeof(storage));
    for (size_t i = 0; i < 259; ++i) {
        for (size_t field = 0; field < 3; ++field)
            storage.fields[field][i] = (double)((int)(i % 11u) - 5) * 0.125;
        storage.materials[i] = (mc_terrain_surface_material){2, 3};
    }
    memset(storage.blocks, 0xa5, sizeof(storage.blocks));
}

static void fields(double sand, double gravel, double thickness)
{
    for (size_t i = 0; i < 256; ++i) {
        storage.fields[0][i] = sand;
        storage.fields[1][i] = gravel;
        storage.fields[2][i] = thickness;
    }
}

static int check_continuation(mc_java_random random, const uint64_t expected[4])
{
    for (size_t i = 0; i < 4; ++i)
        CHECK((uint64_t)mc_java_random_next_i64(&random) == expected[i]);
    return 0;
}

static int decode_runs(const surface_block_run *runs, size_t count, uint8_t output[32771])
{
    size_t position = 0;
    for (size_t i = 0; i < count; ++i) {
        CHECK(runs[i].length != 0 && runs[i].length <= 32771u - position);
        memset(output + position, runs[i].value, runs[i].length);
        position += runs[i].length;
    }
    CHECK(position == 32771);
    return 0;
}

static int check_observed(const surface_fixture *fixture)
{
    for (size_t channel = 0; channel < 3; ++channel) {
        for (size_t i = 0; i < 259; ++i)
            CHECK(bits_of(storage.fields[channel][i]) == fixture->scratch[channel][i]);
    }
    CHECK(decode_runs(fixture->surface, fixture->surface_count, expected_blocks) == 0);
    for (size_t i = 0; i < sizeof(storage.blocks); ++i)
        CHECK(storage.blocks[i] == expected_blocks[i]);
    CHECK(memcmp(storage.materials, saved_storage.materials, sizeof(storage.materials)) == 0);
    CHECK(memcmp(&state, &saved_state, sizeof(state)) == 0);
    return 0;
}

static int test_original_composed_and_split(void)
{
    for (size_t f = 0; f < sizeof(observed_surface) / sizeof(observed_surface[0]); ++f) {
        const surface_fixture *fixture = &observed_surface[f];
        CHECK(mc_terrain_normal_init(&state, fixture->seed));
        memcpy(&saved_state, &state, sizeof(state));
        prepare();
        memcpy(storage.materials, fixture->materials, sizeof(fixture->materials));
        CHECK(decode_runs(fixture->base, fixture->base_count, storage.blocks) == 0);
        memcpy(&saved_storage, &storage, sizeof(storage));
        mc_terrain_normal_surface_workspace work = workspace();
        mc_terrain_normal_surface_workspace saved_work = work;
        int64_t seed = mc_terrain_normal_chunk_seed(fixture->chunk_x, fixture->chunk_z);
        CHECK((uint64_t)seed == fixture->chunk_seed_bits);
        mc_java_random random = state.random_after_init;
        if (fixture->chunk_reseed)
            mc_java_random_reseed(&random, seed);
        mc_java_random before = random;
        CHECK(check_continuation(random, fixture->before) == 0);
        CHECK(mc_terrain_normal_surface(&state, &work, fixture->chunk_x, fixture->chunk_z,
                                        storage.materials, 259, &random, storage.blocks,
                                        sizeof(storage.blocks)));
        CHECK(check_observed(fixture) == 0);
        CHECK(check_continuation(random, fixture->after) == 0);
        CHECK(memcmp(&work, &saved_work, sizeof(work)) == 0);

        /* Same original observations must also match the two public stages.
         * Restoring only caller-owned inputs makes this a repeatability check. */
        memcpy(&storage, &saved_storage, sizeof(storage));
        random = before;
        CHECK(mc_terrain_normal_surface_noise(&state, &work, fixture->chunk_x, fixture->chunk_z));
        CHECK(mc_test_java_random_equal(&random, &before));
        CHECK(memcmp(storage.blocks, saved_storage.blocks, sizeof(storage.blocks)) == 0);
        CHECK(mc_terrain_normal_surface_apply(&work, storage.materials, 259, &random,
                                              storage.blocks, sizeof(storage.blocks)));
        CHECK(check_observed(fixture) == 0);
        CHECK(check_continuation(random, fixture->after) == 0);

        CHECK(mc_terrain_normal_surface_noise(&state, &work, mc_java_i32_add(fixture->chunk_x, 1),
                                              mc_java_i32_sub(fixture->chunk_z, 1)));
        CHECK(mc_terrain_normal_surface_noise(&state, &work, fixture->chunk_x, fixture->chunk_z));
        CHECK(check_observed(fixture) == 0);
    }
    return 0;
}

static int test_chunk_seed_boundaries(void)
{
    for (size_t i = 0; i < sizeof(controlled_chunk_seeds) / sizeof(controlled_chunk_seeds[0]);
         ++i) {
        const chunk_seed_fixture *fixture = &controlled_chunk_seeds[i];
        CHECK((uint64_t)mc_terrain_normal_chunk_seed(fixture->x, fixture->z) == fixture->bits);
    }
    return 0;
}

static int test_minimum_spans_and_wrapped_origins(void)
{
    const surface_fixture *fixture = &observed_surface[0];
    CHECK(mc_terrain_normal_init(&state, fixture->seed));
    memcpy(&saved_state, &state, sizeof(state));
    prepare();
    memcpy(storage.materials, fixture->materials, sizeof(fixture->materials));
    CHECK(decode_runs(fixture->base, fixture->base_count, storage.blocks) == 0);
    memcpy(&saved_storage, &storage, sizeof(storage));
    mc_terrain_normal_surface_workspace work = workspace();
    work.sand.length = 256;
    work.gravel.length = 256;
    work.thickness.length = 256;
    mc_java_random random = state.random_after_init;
    if (fixture->chunk_reseed)
        mc_java_random_reseed(&random,
                              mc_terrain_normal_chunk_seed(fixture->chunk_x, fixture->chunk_z));
    CHECK(mc_terrain_normal_surface(&state, &work, fixture->chunk_x, fixture->chunk_z,
                                    storage.materials, 256, &random, storage.blocks, 32768));
    for (size_t field = 0; field < 3; ++field) {
        for (size_t i = 0; i < 256; ++i)
            CHECK(bits_of(storage.fields[field][i]) == fixture->scratch[field][i]);
        for (size_t i = 256; i < 259; ++i)
            CHECK(bits_of(storage.fields[field][i]) == bits_of(saved_storage.fields[field][i]));
    }
    CHECK(decode_runs(fixture->surface, fixture->surface_count, expected_blocks) == 0);
    CHECK(memcmp(storage.blocks, expected_blocks, sizeof(expected_blocks)) == 0);
    CHECK(check_continuation(random, fixture->after) == 0);
    CHECK(memcmp(storage.materials, saved_storage.materials, sizeof(storage.materials)) == 0);
    CHECK(memcmp(&state, &saved_state, sizeof(state)) == 0);
    memcpy(&saved_storage, &storage, sizeof(storage));
    /* Adding 2^28 chunks leaves the separately wrapped int*16 origins equal.
     * Noise must repeat even though the chunk Random seed need not repeat. */
    CHECK(mc_terrain_normal_surface_noise(
        &state, &work, mc_java_i32_add(fixture->chunk_x, INT32_C(268435456)), fixture->chunk_z));
    CHECK(memcmp(storage.fields, saved_storage.fields, sizeof(storage.fields)) == 0);
    CHECK(mc_terrain_normal_surface_noise(&state, &work, fixture->chunk_x,
                                          mc_java_i32_add(fixture->chunk_z, INT32_C(268435456))));
    CHECK(memcmp(storage.fields, saved_storage.fields, sizeof(storage.fields)) == 0);
    CHECK(memcmp(&state, &saved_state, sizeof(state)) == 0);
    return 0;
}

/* The following tests use constructed C inputs. Their field constants and
 * block arrangements are separate from the original-only fixtures above.
 * Fixed Random continuations come from java.util.Random-only observations. */
static int test_bedrock_and_nonstone_draws(void)
{
    static const uint8_t values[] = {0, 49, 128, 255};
    for (size_t value = 0; value < sizeof(values) / sizeof(values[0]); ++value) {
        prepare();
        fields(1, 4, 0);
        memset(storage.blocks, values[value], 32768);
        memcpy(&saved_storage, &storage, sizeof(storage));
        memcpy(expected_blocks, storage.blocks, sizeof(expected_blocks));
        mc_java_random random;
        mc_java_random expected;
        mc_java_random_seed(&random, 0);
        mc_java_random_seed(&expected, 0);
        /* Independent schedule contract: three column doubles, then a bound-5
         * draw at every descending Y, including high air/nonstone positions. */
        for (size_t z = 0; z < 16; ++z) {
            for (size_t x = 0; x < 16; ++x) {
                for (size_t draw = 0; draw < 3; ++draw)
                    (void)mc_java_random_next_double(&expected);
                for (int32_t y = 127; y >= 0; --y) {
                    int32_t threshold;
                    CHECK(mc_java_random_next_bounded(&expected, 5, &threshold));
                    if (y <= threshold)
                        expected_blocks[(x * 16u + z) * 128u + (size_t)y] = 7;
                }
            }
        }
        mc_terrain_normal_surface_workspace work = workspace();
        CHECK(mc_terrain_normal_surface_apply(&work, storage.materials, 259, &random,
                                              storage.blocks, sizeof(storage.blocks)));
        CHECK(mc_test_java_random_equal(&random, &expected));
        CHECK(check_continuation(random, controlled_air_after[0]) == 0);
        for (size_t i = 0; i < sizeof(storage.blocks); ++i)
            CHECK(storage.blocks[i] == expected_blocks[i]);
        CHECK(memcmp(storage.fields, saved_storage.fields, sizeof(storage.fields)) == 0);
        CHECK(memcmp(storage.materials, saved_storage.materials, sizeof(storage.materials)) == 0);
    }
    return 0;
}

static int test_sea_band_and_sand_precedence(void)
{
    static const size_t caps[] = {59, 60, 62, 63, 64, 65, 66};
    for (size_t cap_index = 0; cap_index < sizeof(caps) / sizeof(caps[0]); ++cap_index) {
        size_t cap = caps[cap_index];
        mc_java_random sand_random = {0};
        for (size_t combination = 0; combination < 4; ++combination) {
            prepare();
            fields((combination & 1u) != 0 ? 1 : -1, (combination & 2u) != 0 ? 4 : 2, 0);
            for (size_t column = 0; column < 256; ++column) {
                memset(storage.blocks + column * 128u, 1, cap + 1u);
                memset(storage.blocks + column * 128u + cap + 1u, 0, 127u - cap);
            }
            mc_java_random random;
            mc_java_random_seed(&random, 0);
            mc_terrain_normal_surface_workspace work = workspace();
            CHECK(mc_terrain_normal_surface_apply(&work, storage.materials, 259, &random,
                                                  storage.blocks, sizeof(storage.blocks)));
            int band = cap >= 60 && cap <= 65;
            int sand = band && (combination & 1u) != 0;
            int gravel = band && !sand && (combination & 2u) != 0;
            uint8_t top = sand ? 12 : gravel ? cap < 64 ? 9 : 0 : 2;
            uint8_t filler = sand ? 12 : gravel ? 13 : 3;
            for (size_t column = 0; column < 256; ++column) {
                CHECK(storage.blocks[column * 128u + cap] == (cap >= 63 ? top : filler));
                for (size_t depth = 1; depth <= 3; ++depth)
                    CHECK(storage.blocks[column * 128u + cap - depth] == filler);
            }
            for (size_t i = 32768; i < sizeof(storage.blocks); ++i)
                CHECK(storage.blocks[i] == 0xa5);
            if (combination == 1) {
                memcpy(expected_blocks, storage.blocks, sizeof(expected_blocks));
                memcpy(&saved_storage, &storage, sizeof(storage));
                sand_random = random;
            } else if (combination == 3) {
                /* Sand after gravel has exactly the same effect and draw
                 * schedule as sand alone, including sandstone continuations. */
                CHECK(memcmp(storage.blocks, expected_blocks, sizeof(expected_blocks)) == 0);
                CHECK(mc_test_java_random_equal(&random, &sand_random));
            }
        }
    }
    return 0;
}

static int test_zero_and_negative_thickness(void)
{
    static const size_t caps[] = {63, 127};
    for (size_t i = 0; i < sizeof(caps) / sizeof(caps[0]); ++i) {
        prepare();
        fields(-1, 2, -9);
        size_t cap = caps[i];
        for (size_t column = 0; column < 256; ++column) {
            memset(storage.blocks + column * 128u, 1, cap + 1u);
            memset(storage.blocks + column * 128u + cap + 1u, 0, 127u - cap);
        }
        mc_java_random random;
        mc_java_random_seed(&random, 0);
        mc_terrain_normal_surface_workspace work = workspace();
        CHECK(mc_terrain_normal_surface_apply(&work, storage.materials, 259, &random,
                                              storage.blocks, sizeof(storage.blocks)));
        for (size_t column = 0; column < 256; ++column) {
            CHECK(storage.blocks[column * 128u + cap] == (cap == 63 ? 9 : 0));
            for (size_t y = 5; y < cap; ++y)
                CHECK(storage.blocks[column * 128u + y] == 1);
        }
        CHECK(check_continuation(random, controlled_air_after[0]) == 0);
    }
    prepare();
    fields(-1, 2, -15);
    memset(storage.blocks, 1, 32768);
    mc_java_random random;
    mc_java_random_seed(&random, 0);
    mc_terrain_normal_surface_workspace work = workspace();
    CHECK(mc_terrain_normal_surface_apply(&work, storage.materials, 259, &random, storage.blocks,
                                          sizeof(storage.blocks)));
    /* The seed-zero third draw produces -1 after Java truncation. Remaining
     * therefore stays -1 and each following stone repeats first placement. */
    CHECK(storage.blocks[127] == 0 && storage.blocks[126] == 0);
    CHECK(storage.blocks[64] == 0 && storage.blocks[63] == 9 && storage.blocks[62] == 1);
    CHECK(check_continuation(random, controlled_air_after[0]) == 0);
    return 0;
}

static int test_sandstone_and_persistent_materials(void)
{
    for (int seed = 0; seed < 2; ++seed) {
        for (int gap = 0; gap < 2; ++gap) {
            prepare();
            fields(-1, 2, 0);
            for (size_t i = 0; i < 256; ++i)
                storage.materials[i] = (mc_terrain_surface_material){12, 12};
            memset(storage.blocks, 1, 32768);
            if (gap)
                storage.blocks[120] = 0;
            mc_java_random random;
            mc_java_random_seed(&random, seed);
            mc_terrain_normal_surface_workspace work = workspace();
            CHECK(mc_terrain_normal_surface_apply(&work, storage.materials, 259, &random,
                                                  storage.blocks, sizeof(storage.blocks)));
            for (size_t y = 124; y <= 127; ++y)
                CHECK(storage.blocks[y] == 12);
            int32_t length = controlled_first_sandstone[seed];
            for (int32_t depth = 0; depth < 3; ++depth)
                CHECK(storage.blocks[123u - (size_t)depth] == (depth < length ? 24 : 1));
            if (gap) {
                CHECK(storage.blocks[120] == 0);
                CHECK(storage.blocks[119] == 12);
                CHECK(storage.blocks[118] == 24 && storage.blocks[117] == 24 &&
                      storage.blocks[116] == 24);
                CHECK(storage.blocks[115] == 1);
            }
            /* Changing filler to sandstone happens even when the extra draw
             * returns zero; after an air gap that retained material is visible. */
            CHECK(check_continuation(random, controlled_sand_after[seed]) == 0);
        }
    }
    return 0;
}

static int test_material_byte_patterns_and_read_alias(void)
{
    prepare();
    fields(-1, 2, 0);
    memset(storage.blocks, 255, 32768);
    for (size_t y = 5; y < 128; ++y)
        storage.blocks[y] = 1;
    storage.materials[0] = (mc_terrain_surface_material){254, 128};
    memcpy(&saved_storage, &storage, sizeof(storage));
    mc_java_random random;
    mc_java_random_seed(&random, 0);
    mc_terrain_normal_surface_workspace work = workspace();
    CHECK(mc_terrain_normal_surface_apply(&work, storage.materials, 259, &random, storage.blocks,
                                          sizeof(storage.blocks)));
    CHECK(storage.blocks[127] == 254);
    CHECK(storage.blocks[126] == 128 && storage.blocks[125] == 128 && storage.blocks[124] == 128);
    CHECK(storage.blocks[123] == 1);
    for (size_t column = 1; column < 256; ++column) {
        for (size_t y = 5; y < 128; ++y)
            CHECK(storage.blocks[column * 128u + y] == 255);
    }
    CHECK(memcmp(storage.materials, saved_storage.materials, sizeof(storage.materials)) == 0);
    CHECK(memcmp(storage.fields, saved_storage.fields, sizeof(storage.fields)) == 0);

    prepare();
    fields(0, 0, 0);
    memset(storage.blocks, 1, 32768);
    memcpy(&saved_storage, &storage, sizeof(storage));
    mc_java_random_seed(&random, 0);
    CHECK(mc_terrain_normal_surface_apply(&work, storage.materials, 259, &random, storage.blocks,
                                          sizeof(storage.blocks)));
    memcpy(expected_blocks, storage.blocks, sizeof(expected_blocks));
    mc_java_random expected = random;
    memcpy(&storage, &saved_storage, sizeof(storage));
    mc_java_random_seed(&random, 0);
    work.gravel = work.sand;
    work.thickness = work.sand;
    mc_terrain_normal_surface_workspace saved_work = work;
    CHECK(mc_terrain_normal_surface_apply(&work, storage.materials, 259, &random, storage.blocks,
                                          sizeof(storage.blocks)));
    CHECK(mc_test_java_random_equal(&random, &expected));
    CHECK(memcmp(storage.blocks, expected_blocks, sizeof(expected_blocks)) == 0);
    CHECK(memcmp(storage.fields, saved_storage.fields, sizeof(storage.fields)) == 0);
    CHECK(memcmp(&work, &saved_work, sizeof(work)) == 0);
    return 0;
}

static int check_preserved(mc_java_random random, mc_java_random saved_random)
{
    CHECK(memcmp(&storage, &saved_storage, sizeof(storage)) == 0);
    CHECK(memcmp(&state, &saved_state, sizeof(state)) == 0);
    CHECK(mc_test_java_random_equal(&random, &saved_random));
    return 0;
}

static int expect_bad_fields(const mc_terrain_normal_surface_workspace *bad)
{
    mc_java_random random;
    mc_java_random_seed(&random, 0);
    mc_java_random saved_random = random;
    CHECK(!mc_terrain_normal_surface_noise(&state, bad, 0, 0));
    CHECK(check_preserved(random, saved_random) == 0);
    CHECK(!mc_terrain_normal_surface_apply(bad, storage.materials, 259, &random, storage.blocks,
                                           sizeof(storage.blocks)));
    CHECK(check_preserved(random, saved_random) == 0);
    CHECK(!mc_terrain_normal_surface(&state, bad, 0, 0, storage.materials, 259, &random,
                                     storage.blocks, sizeof(storage.blocks)));
    CHECK(check_preserved(random, saved_random) == 0);
    return 0;
}

static int test_preflight_preservation(void)
{
    CHECK(mc_terrain_normal_init(&state, 0));
    prepare();
    memcpy(&saved_storage, &storage, sizeof(storage));
    memcpy(&saved_state, &state, sizeof(state));
    CHECK(expect_bad_fields(NULL) == 0);
    mc_terrain_normal_surface_workspace work = workspace();
    mc_terrain_double_span *spans[] = {&work.sand, &work.gravel, &work.thickness};
    for (size_t i = 0; i < 3; ++i) {
        mc_terrain_double_span saved = *spans[i];
        spans[i]->data = NULL;
        CHECK(expect_bad_fields(&work) == 0);
        *spans[i] = saved;
        spans[i]->length = 255;
        CHECK(expect_bad_fields(&work) == 0);
        spans[i]->length = SIZE_MAX / sizeof(double) + 1u;
        CHECK(expect_bad_fields(&work) == 0);
        *spans[i] = saved;
    }
    mc_java_random random;
    mc_java_random_seed(&random, 0);
    mc_java_random saved_random = random;
    CHECK(!mc_terrain_normal_surface_noise(NULL, &work, 0, 0));
    CHECK(!mc_terrain_normal_surface(NULL, &work, 0, 0, storage.materials, 259, &random,
                                     storage.blocks, sizeof(storage.blocks)));
    CHECK(check_preserved(random, saved_random) == 0);
    for (int invalid = 0; invalid < 6; ++invalid) {
        const mc_terrain_surface_material *materials = invalid == 0 ? NULL : storage.materials;
        size_t length = invalid == 1   ? 255
                        : invalid == 2 ? SIZE_MAX / sizeof(*materials) + 1u
                                       : 259;
        mc_java_random *input_random = invalid == 3 ? NULL : &random;
        uint8_t *blocks = invalid == 4 ? NULL : storage.blocks;
        size_t block_length = invalid == 5 ? 32767 : sizeof(storage.blocks);
        CHECK(!mc_terrain_normal_surface_apply(&work, materials, length, input_random, blocks,
                                               block_length));
        CHECK(check_preserved(random, saved_random) == 0);
        CHECK(!mc_terrain_normal_surface(&state, &work, INT32_MAX, INT32_MIN, materials, length,
                                         input_random, blocks, block_length));
        CHECK(check_preserved(random, saved_random) == 0);
    }
    return 0;
}

int main(void)
{
    if (test_original_composed_and_split() || test_chunk_seed_boundaries() ||
        test_minimum_spans_and_wrapped_origins() || test_bedrock_and_nonstone_draws() ||
        test_sea_band_and_sand_precedence() || test_zero_and_negative_thickness() ||
        test_sandstone_and_persistent_materials() || test_material_byte_patterns_and_read_alias() ||
        test_preflight_preservation())
        return 1;
    printf("Surface: %u checks passed\n", checks);
    return 0;
}
