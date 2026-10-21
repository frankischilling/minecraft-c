#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/generation/normal_internal.h"
#include "../../../src/world/terrain/caves_internal.h"
#include "../../support/java_random.h"

#include <emmintrin.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "../terrain/caves_fixtures.h"

/* Constructed constructor/continuation contracts, not a full original provider
 * or World comparison. Actual profile internals are read only while quiescent. */
static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static mc_math_trig trig;
static mc_biome_memory memory;
static mc_biome_manager manager;
static mc_normal_generator provider, numerical;
static unsigned char saved_generator[sizeof(mc_normal_generator)];
static mc_terrain_normal_state explicit_terrain;
static uint8_t live_blocks[MC_CHUNK_BLOCKS + 3u], fresh_blocks[MC_CHUNK_BLOCKS + 3u];
static uint8_t saved_blocks[MC_CHUNK_BLOCKS + 3u];

static uint64_t bits(const double *value) {
    uint64_t result;
    _Static_assert(sizeof(result) == sizeof(*value), "binary64 required");
    memcpy(&result, value, sizeof(result));
    return result;
}

static uint64_t uniquifier(const mc_java_random_profile *profile) {
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static bool cold(const mc_java_random_profile *profile) {
    return profile->math == NULL &&
           atomic_load_explicit(&profile->holder_phase, memory_order_acquire) ==
               (unsigned int)MC_JAVA_RANDOM_HOLDER_COLD;
}

static bool fresh_random(const mc_java_random *random) {
    /* Zero LCG state is valid; no expected clock-derived state is synthesized. */
    return random->state <= UINT64_C(0xffffffffffff) && bits(&random->next_next_gaussian) == 0 &&
           !random->have_next_next_gaussian;
}

static int terrain_matches(const mc_terrain_normal_state *actual,
                           const mc_terrain_normal_state *expected) {
    int result = 1;
    const mc_noise_perlin *left[] = {actual->low_layers,
                                     actual->high_layers,
                                     actual->mix_layers,
                                     actual->surface_primary_layers,
                                     actual->surface_secondary_layers,
                                     actual->column_scale_layers,
                                     actual->column_bias_layers,
                                     actual->later_layers};
    const mc_noise_perlin *right[] = {expected->low_layers,
                                      expected->high_layers,
                                      expected->mix_layers,
                                      expected->surface_primary_layers,
                                      expected->surface_secondary_layers,
                                      expected->column_scale_layers,
                                      expected->column_bias_layers,
                                      expected->later_layers};
    const size_t counts[] = {16, 16, 8, 4, 4, 10, 16, 8};
    for (size_t group = 0; group < 8; ++group) {
        for (size_t i = 0; i < counts[group]; ++i) {
            CHECK(bits(&left[group][i].offset_x) == bits(&right[group][i].offset_x));
            CHECK(bits(&left[group][i].offset_y) == bits(&right[group][i].offset_y));
            CHECK(bits(&left[group][i].offset_z) == bits(&right[group][i].offset_z));
            CHECK(memcmp(left[group][i].permutation, right[group][i].permutation,
                         sizeof(left[group][i].permutation)) == 0);
        }
    }
    CHECK(mc_test_java_random_equal(&actual->random_after_init, &expected->random_after_init));
    result = 0;
cleanup:
    return result;
}

static int test_construction(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_java_random_profile_status cause = MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED;
    uint64_t before = 0;
    size_t allocated = 0;
    mc_java_random cave = {0};
    mc_java_random *const actual_cave = &provider.cave_random;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    before = uniquifier(profile);
    allocated = mc_java_random_profile_allocation_bytes(profile);
    CHECK(mc_terrain_normal_init(&explicit_terrain, 23));
    CHECK(mc_normal_generator_construct_provider(&provider, 23, &manager, &trig, profile, &cause) ==
          MC_NORMAL_GENERATOR_OK);
    CHECK(cause == MC_JAVA_RANDOM_PROFILE_OK && provider.initialized &&
          provider.phase == MC_NORMAL_GENERATOR_PHASE_READY &&
          provider.origin == MC_NORMAL_GENERATOR_PROVIDER && provider.cave_range == 8);
    CHECK(&provider.cave_random == actual_cave && fresh_random(actual_cave));
    CHECK(uniquifier(profile) == before * UINT64_C(1181783497276652981) && cold(profile));
    CHECK(mc_java_random_profile_allocation_bytes(profile) == allocated);
    CHECK(provider.world_seed == 23 && provider.biome_manager == &manager &&
          provider.trig == &trig);
    CHECK(provider.biomes.data == NULL && provider.biomes.length == 0 &&
          memory.live_allocation_bytes == 0);
    CHECK(terrain_matches(&provider.terrain, &explicit_terrain) == 0);
    CHECK(
        mc_test_java_random_equal(&provider.generator_random, &explicit_terrain.random_after_init));
    cave = provider.cave_random;
    before = uniquifier(profile);
    CHECK(mc_normal_generator_init(&numerical, 23, &manager, &trig) == MC_NORMAL_GENERATOR_OK);
    CHECK(numerical.phase == MC_NORMAL_GENERATOR_PHASE_READY &&
          numerical.origin == MC_NORMAL_GENERATOR_NUMERICAL && numerical.cave_range == 8);
    CHECK(numerical.cave_random.state == 0 && fresh_random(&numerical.cave_random));
    CHECK(uniquifier(profile) == before && cold(profile));
    CHECK(mc_test_java_random_equal(&provider.cave_random, &cave));
    CHECK(terrain_matches(&numerical.terrain, &explicit_terrain) == 0);
    memcpy(saved_generator, &provider, sizeof(provider));
    cause = MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED;
    CHECK(mc_normal_generator_construct_provider(&provider, 23, &manager, &trig, profile, &cause) ==
          MC_NORMAL_GENERATOR_INVALID_ARGUMENT);
    CHECK(cause == MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED && uniquifier(profile) == before);
    CHECK(memcmp(saved_generator, &provider, sizeof(provider)) == 0);
    CHECK(mc_normal_generator_init(&provider, 23, &manager, &trig) ==
          MC_NORMAL_GENERATOR_INVALID_ARGUMENT);
    CHECK(memcmp(saved_generator, &provider, sizeof(provider)) == 0);
    mc_normal_generator_free(&provider);
    CHECK(provider.phase == MC_NORMAL_GENERATOR_PHASE_RAW && !provider.initialized &&
          provider.biome_manager == NULL && provider.trig == NULL && provider.biomes.data == NULL);
    CHECK(uniquifier(profile) == before && cold(profile) && manager.initialized == 1);
    CHECK(strcmp(mc_normal_generator_status_string(MC_NORMAL_GENERATOR_PROFILE_FAILED),
                 "cave Random construction failed") == 0);
    result = 0;
cleanup:
    mc_normal_generator_free(&provider);
    mc_normal_generator_free(&numerical);
    mc_java_random_profile_free(profile);
    return result;
}

static mc_normal_generator_status construct_fp(mc_java_random_profile *profile,
                                               unsigned int control,
                                               mc_java_random_profile_status *cause) {
    const unsigned int saved = _mm_getcsr();
    _mm_setcsr(control);
    const mc_normal_generator_status status =
        mc_normal_generator_construct_provider(&provider, 23, &manager, &trig, profile, cause);
    _mm_setcsr(saved); /* Restore before any CHECK, formatting or cleanup. */
    return status;
}

static int test_failure_prefix(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_java_random_profile_status cause = MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED;
    uint64_t before = 0;
    const unsigned int saved_control = _mm_getcsr();
    const unsigned int controls[] = {saved_control | 0x8000u, saved_control | 0x0040u,
                                     (saved_control & ~0x6000u) | 0x2000u};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    before = uniquifier(profile);
    memcpy(saved_generator, &provider, sizeof(provider));
    CHECK(mc_normal_generator_construct_provider(&provider, 23, &manager, &trig, NULL, &cause) ==
          MC_NORMAL_GENERATOR_INVALID_ARGUMENT);
    CHECK(mc_normal_generator_construct_provider(&provider, 23, NULL, &trig, profile, &cause) ==
          MC_NORMAL_GENERATOR_INVALID_ARGUMENT);
    CHECK(mc_normal_generator_construct_provider(&provider, 23, &manager, NULL, profile, &cause) ==
          MC_NORMAL_GENERATOR_INVALID_ARGUMENT);
    CHECK(mc_normal_generator_construct_provider(&provider, 23, &manager, &trig, profile, NULL) ==
          MC_NORMAL_GENERATOR_INVALID_ARGUMENT);
    CHECK(mc_normal_generator_construct_provider(NULL, 23, &manager, &trig, profile, &cause) ==
          MC_NORMAL_GENERATOR_INVALID_ARGUMENT);
    CHECK(cause == MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED && uniquifier(profile) == before);
    CHECK(memcmp(saved_generator, &provider, sizeof(provider)) == 0);
    for (size_t i = 0; i < sizeof(controls) / sizeof(*controls); ++i) {
        CHECK(construct_fp(profile, controls[i], &cause) == MC_NORMAL_GENERATOR_PROFILE_FAILED);
        CHECK(_mm_getcsr() == saved_control && cause == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP);
        CHECK(provider.phase == MC_NORMAL_GENERATOR_PHASE_FAILED && !provider.initialized &&
              provider.origin == MC_NORMAL_GENERATOR_PROVIDER && provider.cave_range == 8);
        CHECK(provider.world_seed == 0 && provider.biome_manager == NULL && provider.trig == NULL &&
              provider.biomes.data == NULL && provider.biomes.length == 0);
        CHECK(provider.cave_random.state == 0 && fresh_random(&provider.cave_random) &&
              provider.generator_random.state == 0 && fresh_random(&provider.generator_random));
        CHECK(provider.terrain.low_layers[0].offset_x == 0.0 &&
              provider.terrain.low_layers[0].permutation[1] == 0);
        CHECK(uniquifier(profile) == before && cold(profile));
        memcpy(saved_generator, &provider, sizeof(provider));
        cause = MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED;
        CHECK(mc_normal_generator_construct_provider(&provider, 23, &manager, &trig, profile,
                                                     &cause) ==
              MC_NORMAL_GENERATOR_INVALID_ARGUMENT);
        CHECK(cause == MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED && uniquifier(profile) == before);
        CHECK(mc_normal_generator_init(&provider, 23, &manager, &trig) ==
              MC_NORMAL_GENERATOR_INVALID_ARGUMENT);
        CHECK(memcmp(saved_generator, &provider, sizeof(provider)) == 0);
        mc_normal_generator_free(&provider);
        CHECK(provider.phase == MC_NORMAL_GENERATOR_PHASE_RAW && !provider.initialized);
    }
    CHECK(mc_normal_generator_construct_provider(&provider, 23, &manager, &trig, profile, &cause) ==
          MC_NORMAL_GENERATOR_OK);
    CHECK(uniquifier(profile) == before * UINT64_C(1181783497276652981) && cold(profile));
    result = 0;
cleanup:
    _mm_setcsr(saved_control);
    mc_normal_generator_free(&provider);
    mc_java_random_profile_free(profile);
    return result;
}

static int test_cave_reseed(void) {
    int result = 1;
    const caves_case_fixture *fixture = &observed_caves_cases[0];
    const caves_block_fixture input = fixture->arrays[0];
    size_t length = 0;
    mc_java_random live = {0}, fresh = {0}, continuation = {0}, saved_random = {0};
    uint64_t cache = 0;
    CHECK(input.offset <= sizeof(observed_caves_runs) / sizeof(observed_caves_runs[0]));
    CHECK(input.count <=
          sizeof(observed_caves_runs) / sizeof(observed_caves_runs[0]) - input.offset);
    for (size_t i = 0; i < input.count; ++i) {
        const caves_byte_run run = observed_caves_runs[input.offset + i];
        CHECK(run.count <= sizeof(live_blocks) - length);
        memset(live_blocks + length, run.value, run.count);
        length += run.count;
    }
    CHECK(length == sizeof(live_blocks));
    memcpy(fresh_blocks, live_blocks, sizeof(live_blocks));
    mc_java_random_seed(&live, 7);
    (void)mc_java_random_next_gaussian(&live);
    CHECK(live.have_next_next_gaussian);
    cache = bits(&live.next_next_gaussian);
    CHECK(cache != 0);
    fresh = live;
    CHECK(mc_terrain_normal_caves_reseed(&trig, fixture->seed, fixture->chunk_x, fixture->chunk_z,
                                         &live, live_blocks, sizeof(live_blocks)));
    CHECK(!live.have_next_next_gaussian && bits(&live.next_next_gaussian) == cache);
    CHECK(mc_terrain_normal_caves(&trig, fixture->seed, fixture->chunk_x, fixture->chunk_z, &fresh,
                                  fresh_blocks, sizeof(fresh_blocks)));
    CHECK(!fresh.have_next_next_gaussian && bits(&fresh.next_next_gaussian) == 0);
    CHECK(live.state == fresh.state && memcmp(live_blocks, fresh_blocks, sizeof(live_blocks)) == 0);
    /* Literal continuation observations from the retained paired-original
     * fixture, not an expected LCG computed by this test. */
    continuation = live;
    for (size_t i = 0; i < 4; ++i)
        CHECK((uint64_t)mc_java_random_next_i64(&continuation) == fixture->following[0][i]);
    saved_random = live;
    memcpy(saved_blocks, live_blocks, sizeof(live_blocks));
    CHECK(
        !mc_terrain_normal_caves_reseed(&trig, 23, 0, 0, &live, live_blocks, MC_CHUNK_BLOCKS - 1u));
    CHECK(mc_test_java_random_equal(&live, &saved_random) &&
          memcmp(saved_blocks, live_blocks, sizeof(live_blocks)) == 0);
    CHECK(!mc_terrain_normal_caves_reseed(&trig, 23, INT32_MIN, 0, &live, live_blocks,
                                          sizeof(live_blocks)));
    CHECK(mc_test_java_random_equal(&live, &saved_random));
    CHECK(!mc_terrain_normal_caves_reseed(NULL, 23, 0, 0, &live, live_blocks, sizeof(live_blocks)));
    CHECK(!mc_terrain_normal_caves_reseed(&trig, 23, 0, 0, NULL, live_blocks, sizeof(live_blocks)));
    CHECK(memcmp(saved_blocks, live_blocks, sizeof(live_blocks)) == 0);
    result = 0;
cleanup:
    return result;
}

static int test_generation_dispatch(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_java_random_profile_status cause = MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED;
    mc_chunk_cache_entry entries[1];
    mc_chunk_state *insertions[1];
    mc_chunk_cache cache = {0};
    static mc_light_region regions[4096];
    mc_light_queue queue = {0};
    int32_t depth = 0, opacity[256] = {0};
    mc_world_lighting lighting = {&cache, &queue, &depth, false};
    mc_normal_generator_materials materials = {0};
    mc_normal_generation_resources resources = {&lighting, &materials, opacity, 256, NULL, NULL};
    static mc_normal_chunk live_result, fresh_result;
    uint64_t retained = 0, before = 0;
    CHECK(mc_chunk_cache_init(&cache, entries, 1, insertions, 1));
    CHECK(mc_light_queue_init(&queue, regions, 4096));
    for (size_t i = 1; i < 256; ++i)
        opacity[i] = 255;
    for (size_t i = 0; i < MC_NORMAL_GENERATOR_BIOME_MATERIALS; ++i)
        materials.by_biome[i] = (mc_terrain_surface_material){2, 3};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_normal_generator_construct_provider(&provider, 23, &manager, &trig, profile, &cause) ==
          MC_NORMAL_GENERATOR_OK);
    CHECK(mc_normal_generator_init(&numerical, 23, &manager, &trig) == MC_NORMAL_GENERATOR_OK);
    before = uniquifier(profile);
    /* Actual initialized-owner reseeding/Gaussian operations prepare a stale
     * payload. They do not supply the constructor's entropy or expected seed. */
    mc_java_random_reseed(&provider.cave_random, 7);
    (void)mc_java_random_next_gaussian(&provider.cave_random);
    numerical.cave_random = provider.cave_random;
    retained = bits(&provider.cave_random.next_next_gaussian);
    CHECK(provider.cave_random.have_next_next_gaussian && retained != 0);
    memset(live_blocks, 0xa7, sizeof(live_blocks));
    memset(fresh_blocks, 0xa7, sizeof(fresh_blocks));
    CHECK(mc_normal_generator_generate(&provider, 1, 0, &resources, &live_result, live_blocks,
                                       sizeof(live_blocks)) == MC_NORMAL_GENERATOR_OK);
    CHECK(mc_normal_generator_generate(&numerical, 1, 0, &resources, &fresh_result, fresh_blocks,
                                       sizeof(fresh_blocks)) == MC_NORMAL_GENERATOR_OK);
    CHECK(bits(&provider.cave_random.next_next_gaussian) == retained &&
          !provider.cave_random.have_next_next_gaussian);
    CHECK(bits(&numerical.cave_random.next_next_gaussian) == 0 &&
          !numerical.cave_random.have_next_next_gaussian);
    CHECK(provider.cave_random.state == numerical.cave_random.state);
    CHECK(mc_test_java_random_equal(&provider.generator_random, &numerical.generator_random));
    CHECK(memcmp(live_blocks, fresh_blocks, sizeof(live_blocks)) == 0);
    CHECK(memcmp(live_result.data.metadata, fresh_result.data.metadata,
                 sizeof(live_result.data.metadata)) == 0);
    CHECK(memcmp(live_result.data.skylight, fresh_result.data.skylight,
                 sizeof(live_result.data.skylight)) == 0);
    CHECK(memcmp(live_result.data.heightmap, fresh_result.data.heightmap,
                 sizeof(live_result.data.heightmap)) == 0);
    CHECK(uniquifier(profile) == before && cold(profile));
    CHECK(provider.biomes.data != NULL && numerical.biomes.data != NULL &&
          provider.biomes.data != numerical.biomes.data);
    result = 0;
cleanup:
    mc_normal_generator_free(&provider);
    mc_normal_generator_free(&numerical);
    mc_java_random_profile_free(profile);
    return result;
}

int main(void) {
    int result = 1;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u || !mc_math_trig_init(&trig))
        return 1;
    if (mc_biome_memory_init(&memory, MC_BIOME_DEFAULT_ALLOCATION_BYTES) != MC_BIOME_OK ||
        mc_biome_manager_init(&manager, 23, &memory) != MC_BIOME_OK)
        goto cleanup;
    if (test_construction() != 0 || test_failure_prefix() != 0 || test_cave_reseed() != 0 ||
        test_generation_dispatch() != 0)
        goto cleanup;
    result = 0;
cleanup:
    mc_normal_generator_free(&provider);
    mc_normal_generator_free(&numerical);
    mc_biome_manager_free(&manager);
    if (memory.live_allocation_bytes != 0) {
        fputs("provider construction test: biome owner leak\n", stderr);
        result = 1;
    }
    if (result == 0)
        printf("normal provider construction contracts: %u checks passed\n", checks);
    return result;
}
