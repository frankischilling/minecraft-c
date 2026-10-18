#include "mc/world/generation/normal.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Constructed ownership/resource contracts; original comparisons are separate. */
static unsigned checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_biome_memory memory;
    mc_biome_manager manager;
    mc_normal_generator generator;
    mc_normal_generator_materials materials;
    mc_normal_chunk results[3];
    uint8_t blocks[3][MC_CHUNK_BLOCKS + 3u];
    mc_chunk_cache_entry entries[8];
    mc_chunk_state *insertions[16];
    mc_chunk_cache cache;
    mc_light_region regions[4096];
    mc_light_queue queue;
    int32_t depth;
    mc_world_lighting lighting;
    int32_t opacity[257];
    mc_chunk_height_query queries[MC_CHUNK_FINISH_QUERIES + 2u];
    mc_chunk_finish_trace trace;
    size_t aborts;
    mc_normal_generation_resources resources;
} fixture;

static fixture f;
static unsigned char saved[sizeof(f)];
static mc_math_trig trig, saved_trig;
static mc_terrain_normal_state saved_terrain;

static int reset_fixture(size_t budget)
{
    mc_normal_generator_free(&f.generator);
    mc_biome_manager_free(&f.manager);
    CHECK(f.memory.live_allocation_bytes == 0);
    memset(&f, 0, sizeof(f));
    CHECK(mc_biome_memory_init(&f.memory, budget) == MC_BIOME_OK);
    CHECK(mc_biome_manager_init(&f.manager, 23, &f.memory) == MC_BIOME_OK);
    CHECK(mc_normal_generator_init(&f.generator, 23, &f.manager, &trig) == MC_NORMAL_GENERATOR_OK);
    CHECK(mc_chunk_cache_init(&f.cache, f.entries, 8u, f.insertions, 16u));
    CHECK(mc_light_queue_init(&f.queue, f.regions, 4096u));
    f.lighting = (mc_world_lighting){&f.cache, &f.queue, &f.depth, false};
    for (size_t i = 0; i < MC_NORMAL_GENERATOR_BIOME_MATERIALS; ++i)
        f.materials.by_biome[i] = (mc_terrain_surface_material){2, 3};
    f.materials.by_biome[MC_BIOME_DESERT - 1] = (mc_terrain_surface_material){12, 12};
    f.materials.by_biome[MC_BIOME_ICE_DESERT - 1] = (mc_terrain_surface_material){12, 12};
    /* Explicit constructed opacity, not an original registry observation. */
    for (size_t i = 1; i < 256u; ++i)
        f.opacity[i] = 255;
    f.opacity[256] = -1; /* Prefix-only input contract. */
    memset(f.blocks, 0xa7, sizeof(f.blocks));
    memset(f.results, 0x95, sizeof(f.results));
    memset(f.queries, 0x87, sizeof(f.queries));
    f.trace = (mc_chunk_finish_trace){f.queries, MC_CHUNK_FINISH_QUERIES + 2u, 19u};
    f.aborts = 123u;
    f.resources = (mc_normal_generation_resources){&f.lighting, &f.materials, f.opacity,
                                                   257u,        &f.trace,     &f.aborts};
    return 0;
}

static mc_normal_generator_status generate(size_t output, int32_t x, int32_t z)
{
    return mc_normal_generator_generate(&f.generator, x, z, &f.resources, &f.results[output],
                                        f.blocks[output], sizeof(f.blocks[output]));
}

static int test_init_and_allocation_failure(void)
{
    static const size_t budgets[] = {0u, 4096u, 7000u};
    for (size_t i = 0; i < sizeof(budgets) / sizeof(*budgets); ++i) {
        CHECK(reset_fixture(budgets[i]) == 0);
        memcpy(saved, &f, sizeof(f));
        CHECK(mc_normal_generator_init(&f.generator, 23, &f.manager, &trig) ==
              MC_NORMAL_GENERATOR_INVALID_ARGUMENT);
        CHECK(memcmp(saved, &f, sizeof(f)) == 0);
        CHECK(generate(0, 1, 0) == MC_NORMAL_GENERATOR_LIMIT);
        CHECK(memcmp(saved, &f, sizeof(f)) == 0);
        CHECK(f.memory.live_allocation_bytes == 0);
    }
    CHECK(strcmp(mc_normal_generator_status_string(MC_NORMAL_GENERATOR_LIMIT), "resource limit") ==
          0);
    CHECK(mc_normal_generator_status_string((mc_normal_generator_status)99) != NULL);
    mc_normal_generator_free(NULL);
    return 0;
}

static int test_preflight_preservation(void)
{
    for (unsigned failure = 0; failure < 9u; ++failure) {
        CHECK(reset_fixture(MC_BIOME_DEFAULT_ALLOCATION_BYTES) == 0);
        int32_t x = 1, z = 0;
        size_t capacity = sizeof(f.blocks[0]);
        mc_normal_generator_status expected = MC_NORMAL_GENERATOR_INVALID_ARGUMENT;
        if (failure == 0u)
            capacity = MC_CHUNK_BLOCKS - 1u;
        else if (failure == 1u)
            x = INT32_MIN + 7;
        else if (failure == 2u)
            z = INT32_MAX - 8;
        else if (failure == 3u)
            f.resources.materials = NULL;
        else if (failure == 4u)
            f.resources.opacity_length = 255u;
        else if (failure == 5u)
            f.opacity[255] = -1;
        else if (failure == 6u) {
            f.trace.capacity = MC_CHUNK_FINISH_QUERIES - 1u;
            expected = MC_NORMAL_GENERATOR_LIMIT;
        } else if (failure == 7u) {
            f.queue.capacity = MC_CHUNK_FINISH_QUERIES - 1u;
            expected = MC_NORMAL_GENERATOR_LIMIT;
        } else
            f.manager.classifier.table[0] = MC_BIOME_NONE;
        memcpy(saved, &f, sizeof(f));
        CHECK(mc_normal_generator_generate(&f.generator, x, z, &f.resources, &f.results[0],
                                           f.blocks[0], capacity) == expected);
        CHECK(memcmp(saved, &f, sizeof(f)) == 0);
    }
    return 0;
}

static int test_existing_allocations_survive_failure(void)
{
    CHECK(reset_fixture(7000u) == 0);
    CHECK(mc_biome_cached_square(&f.manager, 5, 6, 3) == MC_BIOME_OK);
    double temperature[9], rainfall[9], auxiliary[9];
    mc_biome_id cached[9];
    memcpy(temperature, f.manager.a.data, sizeof(temperature));
    memcpy(rainfall, f.manager.b.data, sizeof(rainfall));
    memcpy(auxiliary, f.manager.c.data, sizeof(auxiliary));
    memcpy(cached, f.manager.d.data, sizeof(cached));
    memcpy(saved, &f, sizeof(f));
    CHECK(generate(0, 1, 0) == MC_NORMAL_GENERATOR_LIMIT);
    CHECK(memcmp(saved, &f, sizeof(f)) == 0);
    CHECK(memcmp(temperature, f.manager.a.data, sizeof(temperature)) == 0);
    CHECK(memcmp(rainfall, f.manager.b.data, sizeof(rainfall)) == 0);
    CHECK(memcmp(auxiliary, f.manager.c.data, sizeof(auxiliary)) == 0);
    CHECK(memcmp(cached, f.manager.d.data, sizeof(cached)) == 0);
    return 0;
}

static int test_unpublished_identity(void)
{
    for (unsigned location = 0; location < 3u; ++location) {
        CHECK(reset_fixture(MC_BIOME_DEFAULT_ALLOCATION_BYTES) == 0);
        CHECK(mc_chunk_data_init(&f.results[0].data, f.blocks[0], sizeof(f.blocks[0])));
        f.results[0].chunk = (mc_chunk_state){.data = &f.results[0].data};
        CHECK(mc_chunk_data_init(&f.results[1].data, f.blocks[1], sizeof(f.blocks[1])));
        f.results[1].chunk = (mc_chunk_state){.data = &f.results[1].data};
        if (location == 1u) {
            /* Different state, same data: a result cannot overwrite cached data. */
            f.results[1].chunk.data = &f.results[0].data;
            CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, &f.results[1].chunk));
        } else {
            CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, &f.results[0].chunk));
            if (location == 2u)
                CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, &f.results[1].chunk));
        }
        memcpy(saved, &f, sizeof(f));
        CHECK(generate(0, 1, 0) == MC_NORMAL_GENERATOR_INVALID_ARGUMENT);
        CHECK(memcmp(saved, &f, sizeof(f)) == 0);
    }
    return 0;
}

static int test_repeated_generation_and_borrowed_owners(void)
{
    CHECK(reset_fixture(MC_BIOME_DEFAULT_ALLOCATION_BYTES) == 0);
    CHECK(mc_biome_cached_square(&f.manager, 700, -800, 3) == MC_BIOME_OK);
    mc_biome_buffer cached = f.manager.d;
    mc_biome_id cached_values[9];
    memcpy(cached_values, cached.data, sizeof(cached_values));
    memcpy(&saved_terrain, &f.generator.terrain, sizeof(saved_terrain));
    CHECK(generate(0, 1, 0) == MC_NORMAL_GENERATOR_OK);
    CHECK(f.results[0].chunk.data == &f.results[0].data && f.results[0].chunk.dirty &&
          !f.results[0].chunk.blank && f.results[0].chunk.x == 1 && f.results[0].chunk.z == 0);
    CHECK(!f.results[0].chunk.active && !f.results[0].chunk.terrain_populated &&
          !f.results[0].chunk.never_save && !f.results[0].chunk.has_entities &&
          f.results[0].chunk.last_save_time == 0);
    CHECK(f.results[0].data.blocks == f.blocks[0]);
    CHECK(f.results[0].data.blocks_capacity == sizeof(f.blocks[0]));
    CHECK(f.trace.count == MC_CHUNK_FINISH_QUERIES && f.aborts == 0 && f.queue.count == 0);
    CHECK(f.generator.generator_random.state == UINT64_C(161302643799564));
    CHECK(f.generator.cave_random.state == UINT64_C(23799015621029));
    mc_java_random generator_random = f.generator.generator_random;
    mc_java_random cave_random = f.generator.cave_random;
    double *temperature = f.manager.a.data, *rainfall = f.manager.b.data;
    double *auxiliary = f.manager.c.data;
    mc_biome_id *biomes = f.generator.biomes.data;
    static mc_normal_chunk first_result;
    static uint8_t first_blocks[MC_CHUNK_BLOCKS + 3u];
    memcpy(&first_result, &f.results[0], sizeof(first_result));
    memcpy(first_blocks, f.blocks[0], sizeof(first_blocks));
    CHECK(generate(1, -1, -1) == MC_NORMAL_GENERATOR_OK);
    CHECK(memcmp(&first_result, &f.results[0], sizeof(first_result)) == 0);
    CHECK(memcmp(first_blocks, f.blocks[0], sizeof(first_blocks)) == 0);
    CHECK(generate(2, 1, 0) == MC_NORMAL_GENERATOR_OK);
    CHECK(memcmp(&generator_random, &f.generator.generator_random, sizeof(generator_random)) == 0);
    CHECK(memcmp(&cave_random, &f.generator.cave_random, sizeof(cave_random)) == 0);
    CHECK(memcmp(f.blocks[0], f.blocks[2], sizeof(f.blocks[0])) == 0);
    CHECK(memcmp(f.results[0].data.metadata, f.results[2].data.metadata,
                 sizeof(f.results[0].data.metadata)) == 0);
    CHECK(memcmp(f.results[0].data.skylight, f.results[2].data.skylight,
                 sizeof(f.results[0].data.skylight)) == 0);
    CHECK(memcmp(f.results[0].data.heightmap, f.results[2].data.heightmap,
                 sizeof(f.results[0].data.heightmap)) == 0);
    CHECK(f.results[0].data.minimum_height == f.results[2].data.minimum_height);
    CHECK(f.manager.a.data == temperature && f.manager.b.data == rainfall &&
          f.manager.c.data == auxiliary && f.generator.biomes.data == biomes);
    CHECK(f.manager.d.data == cached.data && f.manager.d.length == cached.length);
    CHECK(memcmp(f.manager.d.data, cached_values, sizeof(cached_values)) == 0);
    CHECK(memcmp(&f.generator.terrain, &saved_terrain, sizeof(saved_terrain)) == 0);
    CHECK(memcmp(&trig, &saved_trig, sizeof(trig)) == 0);
    CHECK(f.cache.count == 0 && f.cache.insertion_count == 0 && f.depth == 0);
    for (size_t output = 0; output < 3u; ++output) {
        CHECK(f.blocks[output][MC_CHUNK_BLOCKS] == 0xa7 &&
              f.blocks[output][MC_CHUNK_BLOCKS + 1u] == 0xa7 &&
              f.blocks[output][MC_CHUNK_BLOCKS + 2u] == 0xa7);
        for (size_t i = 0; i < MC_CHUNK_NIBBLE_BYTES; ++i)
            CHECK(f.results[output].data.metadata[i] == 0 &&
                  f.results[output].data.block_light[i] == 0);
    }
    const unsigned char *tail = (const unsigned char *)(f.queries + MC_CHUNK_FINISH_QUERIES);
    for (size_t i = 0; i < 2u * sizeof(f.queries[0]); ++i)
        CHECK(tail[i] == 0x87);
    mc_biome_manager manager = f.manager; /* Read-only descriptor snapshot. */
    size_t before_free = f.memory.live_allocation_bytes;
    mc_normal_generator_free(&f.generator);
    CHECK(memcmp(&f.manager, &manager, sizeof(manager)) == 0);
    CHECK(f.memory.live_allocation_bytes < before_free && f.memory.live_allocation_bytes > 0);
    CHECK(f.manager.d.data == cached.data);
    CHECK(mc_normal_generator_init(&f.generator, 23, &f.manager, &trig) == MC_NORMAL_GENERATOR_OK);
    return 0;
}

static int test_current_materials_and_no_sky(void)
{
    CHECK(reset_fixture(MC_BIOME_DEFAULT_ALLOCATION_BYTES) == 0);
    CHECK(generate(0, 1, 0) == MC_NORMAL_GENERATOR_OK);
    size_t original_soil = 0;
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i)
        original_soil += f.blocks[0][i] == 2u || f.blocks[0][i] == 3u ? 1u : 0u;
    CHECK(original_soil > 0);
    for (size_t i = 0; i < MC_NORMAL_GENERATOR_BIOME_MATERIALS; ++i)
        f.materials.by_biome[i] = (mc_terrain_surface_material){0, 0};
    f.lighting.no_sky = true;
    CHECK(generate(1, 1, 0) == MC_NORMAL_GENERATOR_OK);
    CHECK(memcmp(f.blocks[0], f.blocks[1], MC_CHUNK_BLOCKS) != 0);
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i)
        CHECK(f.blocks[1][i] != 2u && f.blocks[1][i] != 3u);
    for (size_t i = 0; i < MC_CHUNK_NIBBLE_BYTES; ++i)
        CHECK(f.results[1].data.skylight[i] == 0);
    CHECK(f.results[1].chunk.dirty && f.results[1].data.minimum_height > 0);
    CHECK(f.queue.count == 0 && f.depth == 0 && f.aborts == 0);
    for (size_t i = 0; i < MC_NORMAL_GENERATOR_BIOME_MATERIALS; ++i)
        f.materials.by_biome[i] =
            (mc_terrain_surface_material){(uint8_t)(40u + i), (uint8_t)(80u + i)};
    CHECK(generate(2, -1, -1) == MC_NORMAL_GENERATOR_OK);
    CHECK(f.generator.biomes.length == MC_TERRAIN_NORMAL_CLIMATE_VALUES);
    for (size_t i = 0; i < MC_TERRAIN_NORMAL_CLIMATE_VALUES; ++i) {
        const size_t slot = (size_t)f.generator.biomes.data[i] - 1u;
        CHECK(f.generator.materials[i].top == f.materials.by_biome[slot].top);
        CHECK(f.generator.materials[i].filler == f.materials.by_biome[slot].filler);
    }
    return 0;
}

int main(void)
{
    if (!mc_math_trig_init(&trig))
        return 1;
    memcpy(&saved_trig, &trig, sizeof(trig));
    if (test_init_and_allocation_failure() || test_preflight_preservation() ||
        test_existing_allocations_survive_failure() || test_unpublished_identity() ||
        test_repeated_generation_and_borrowed_owners() || test_current_materials_and_no_sky())
        return 1;
    mc_normal_generator_free(&f.generator);
    mc_biome_manager_free(&f.manager);
    CHECK(f.memory.live_allocation_bytes == 0);
    printf("normal generator contracts: %u checks passed\n", checks);
    return 0;
}
