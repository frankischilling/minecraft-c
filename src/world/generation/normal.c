#include "normal_internal.h"

#include "../terrain/caves_internal.h"

#include <limits.h>
#include <string.h>

_Static_assert(MC_BIOME_SKY - MC_BIOME_RAINFOREST + 1 == MC_NORMAL_GENERATOR_BIOME_MATERIALS,
               "all original biome identities require explicit material pairs");
_Static_assert(MC_TERRAIN_NORMAL_BLOCKS == MC_CHUNK_BLOCKS,
               "normal terrain and chunk data share the fixed block prefix");

const char *mc_normal_generator_status_string(mc_normal_generator_status status) {
    switch (status) {
        case MC_NORMAL_GENERATOR_OK:
            return "ok";
        case MC_NORMAL_GENERATOR_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_NORMAL_GENERATOR_LIMIT:
            return "resource limit";
        case MC_NORMAL_GENERATOR_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_NORMAL_GENERATOR_STATE_ERROR:
            return "downstream state error";
        case MC_NORMAL_GENERATOR_PROFILE_FAILED:
            return "cave Random construction failed";
    }
    return "unknown normal generator status";
}

static bool manager_ready(const mc_biome_manager *manager) {
    if (manager == NULL || manager->initialized != 1 || manager->memory == NULL ||
        manager->memory->live_allocation_bytes > manager->memory->max_allocation_bytes) {
        return false;
    }
    for (size_t i = 0; i < MC_BIOME_TABLE_ENTRIES; ++i) {
        if (manager->classifier.table[i] < MC_BIOME_RAINFOREST ||
            manager->classifier.table[i] > MC_BIOME_SKY) {
            return false;
        }
    }
    return true;
}

static bool normal_raw(const mc_normal_generator *generator, const mc_biome_manager *manager,
                       const mc_math_trig *trig) {
    return generator != NULL && !generator->initialized &&
           generator->phase == MC_NORMAL_GENERATOR_PHASE_RAW && generator->biome_manager == NULL &&
           generator->trig == NULL && generator->biomes.data == NULL &&
           generator->biomes.length == 0 && manager_ready(manager) && trig != NULL;
}

/* Shared allocation-free seeded tail. In particular this must never clear,
 * copy or seed the already constructed provider-mode cave Random. */
static mc_normal_generator_status normal_seeded_initialize(mc_normal_generator *generator,
                                                           int64_t world_seed,
                                                           mc_biome_manager *manager,
                                                           const mc_math_trig *trig) {
    if (!mc_terrain_normal_init(&generator->terrain, world_seed)) {
        generator->phase = MC_NORMAL_GENERATOR_PHASE_FAILED;
        return MC_NORMAL_GENERATOR_STATE_ERROR;
    }
    generator->world_seed = world_seed;
    generator->biome_manager = manager;
    generator->trig = trig;
    generator->generator_random = generator->terrain.random_after_init;
    generator->phase = MC_NORMAL_GENERATOR_PHASE_READY;
    generator->initialized = true;
    return MC_NORMAL_GENERATOR_OK;
}

mc_normal_generator_status mc_normal_generator_init(mc_normal_generator *generator,
                                                    int64_t world_seed, mc_biome_manager *manager,
                                                    const mc_math_trig *trig) {
    if (!normal_raw(generator, manager, trig)) {
        return MC_NORMAL_GENERATOR_INVALID_ARGUMENT;
    }
    memset(generator, 0, sizeof(*generator));
    generator->phase = MC_NORMAL_GENERATOR_PHASE_INITIALIZING;
    generator->origin = MC_NORMAL_GENERATOR_NUMERICAL;
    generator->cave_range = (int32_t)MC_TERRAIN_NORMAL_CAVE_RANGE;
    return normal_seeded_initialize(generator, world_seed, manager, trig);
}

mc_normal_generator_status
mc_normal_generator_construct_provider(mc_normal_generator *generator, int64_t world_seed,
                                       mc_biome_manager *manager, const mc_math_trig *trig,
                                       mc_java_random_profile *profile,
                                       mc_java_random_profile_status *profile_cause) {
    if (profile == NULL || profile_cause == NULL || !normal_raw(generator, manager, trig)) {
        return MC_NORMAL_GENERATOR_INVALID_ARGUMENT;
    }
    /* The sole whole-object clear precedes fv/dy's range/default-Random prefix. */
    memset(generator, 0, sizeof(*generator));
    generator->phase = MC_NORMAL_GENERATOR_PHASE_INITIALIZING;
    generator->origin = MC_NORMAL_GENERATOR_PROVIDER;
    generator->cave_range = (int32_t)MC_TERRAIN_NORMAL_CAVE_RANGE;
    const mc_java_random_profile_status status =
        mc_java_random_construct_default(profile, &generator->cave_random);
    *profile_cause = status;
    if (status != MC_JAVA_RANDOM_PROFILE_OK) {
        generator->phase = MC_NORMAL_GENERATOR_PHASE_FAILED;
        return MC_NORMAL_GENERATOR_PROFILE_FAILED;
    }
    return normal_seeded_initialize(generator, world_seed, manager, trig);
}

void mc_normal_generator_free(mc_normal_generator *generator) {
    if (generator != NULL) {
        mc_biome_buffer_free(&generator->biomes);
        memset(generator, 0, sizeof(*generator));
    }
}

static mc_normal_generator_status resources_ready(const mc_normal_generation_resources *resources) {
    if (resources == NULL || resources->lighting == NULL || resources->materials == NULL ||
        resources->opacity == NULL || resources->opacity_length < 256u ||
        resources->opacity_length > SIZE_MAX / sizeof(*resources->opacity)) {
        return MC_NORMAL_GENERATOR_INVALID_ARGUMENT;
    }
    const mc_chunk_cache *cache = resources->lighting->cache;
    const mc_light_queue *queue = resources->lighting->queue;
    if (cache == NULL || cache->capacity > SIZE_MAX / sizeof(*cache->entries) ||
        cache->insertion_capacity > SIZE_MAX / sizeof(*cache->insertions) || queue == NULL ||
        queue->capacity > SIZE_MAX / sizeof(*queue->entries) ||
        !mc_world_lighting_validate(resources->lighting)) {
        return MC_NORMAL_GENERATOR_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < 256u; ++i) {
        if (resources->opacity[i] < 0) {
            return MC_NORMAL_GENERATOR_INVALID_ARGUMENT;
        }
    }
    if (resources->trace != NULL) {
        if (resources->trace->entries == NULL ||
            resources->trace->capacity > SIZE_MAX / sizeof(*resources->trace->entries)) {
            return MC_NORMAL_GENERATOR_INVALID_ARGUMENT;
        }
        if (resources->trace->capacity < MC_CHUNK_FINISH_QUERIES) {
            return MC_NORMAL_GENERATOR_LIMIT;
        }
    }
    if (queue->capacity != MC_LIGHT_QUEUE_LIMIT &&
        queue->capacity - queue->count < MC_CHUNK_FINISH_QUERIES) {
        return MC_NORMAL_GENERATOR_LIMIT;
    }
    return MC_NORMAL_GENERATOR_OK;
}

static bool fresh_identity(const mc_chunk_state *cached, const mc_normal_chunk *result) {
    return cached != &result->chunk && cached->data != &result->data;
}

static bool result_unpublished(const mc_chunk_cache *cache, const mc_normal_chunk *result) {
    for (size_t i = 0; i < cache->count; ++i) {
        if (!fresh_identity(cache->entries[i].chunk, result)) {
            return false;
        }
    }
    for (size_t i = 0; i < cache->insertion_count; ++i) {
        if (!fresh_identity(cache->insertions[i], result)) {
            return false;
        }
    }
    return true;
}

static mc_normal_generator_status biome_status(mc_biome_status status) {
    switch (status) {
        case MC_BIOME_OK:
            return MC_NORMAL_GENERATOR_OK;
        case MC_BIOME_INVALID_ARGUMENT:
            return MC_NORMAL_GENERATOR_INVALID_ARGUMENT;
        case MC_BIOME_LIMIT:
            return MC_NORMAL_GENERATOR_LIMIT;
        case MC_BIOME_ALLOCATION_FAILED:
            return MC_NORMAL_GENERATOR_ALLOCATION_FAILED;
    }
    return MC_NORMAL_GENERATOR_STATE_ERROR;
}

mc_normal_generator_status
mc_normal_generator_generate(mc_normal_generator *generator, int32_t chunk_x, int32_t chunk_z,
                             const mc_normal_generation_resources *resources,
                             mc_normal_chunk *result, uint8_t *blocks, size_t blocks_capacity) {
    if (generator == NULL || !generator->initialized ||
        generator->phase != MC_NORMAL_GENERATOR_PHASE_READY ||
        (generator->origin != MC_NORMAL_GENERATOR_NUMERICAL &&
         generator->origin != MC_NORMAL_GENERATOR_PROVIDER) ||
        generator->cave_range != (int32_t)MC_TERRAIN_NORMAL_CAVE_RANGE || generator->trig == NULL ||
        !manager_ready(generator->biome_manager) || result == NULL || blocks == NULL ||
        blocks_capacity < MC_CHUNK_BLOCKS || chunk_x < INT32_MIN + 8 || chunk_x > INT32_MAX - 9 ||
        chunk_z < INT32_MIN + 8 || chunk_z > INT32_MAX - 9 ||
        !((generator->biomes.data == NULL && generator->biomes.length == 0) ||
          (generator->biomes.data != NULL && generator->biomes.length == 256u))) {
        return MC_NORMAL_GENERATOR_INVALID_ARGUMENT;
    }
    mc_normal_generator_status status = resources_ready(resources);
    if (status != MC_NORMAL_GENERATOR_OK) {
        return status;
    }
    if (!result_unpublished(resources->lighting->cache, result)) {
        return MC_NORMAL_GENERATOR_INVALID_ARGUMENT;
    }
    mc_chunk_data staged_data;
    if (!mc_chunk_data_init(&staged_data, blocks, blocks_capacity)) {
        return MC_NORMAL_GENERATOR_STATE_ERROR;
    }
    mc_java_random generator_random = generator->generator_random;
    mc_java_random_reseed(&generator_random, mc_terrain_normal_chunk_seed(chunk_x, chunk_z));
    mc_java_random cave_random = generator->cave_random;
    /* This is the only allocating stage. It atomically stages every needed
     * manager/output replacement and preserves all state on checked failure. */
    status =
        biome_status(mc_biome_sample_square(generator->biome_manager, mc_java_i32_mul(chunk_x, 16),
                                            mc_java_i32_mul(chunk_z, 16), 16, &generator->biomes));
    if (status != MC_NORMAL_GENERATOR_OK) {
        return status;
    }
    for (size_t i = 0; i < 256u; ++i) {
        mc_biome_id id = generator->biomes.data[i];
        if (id < MC_BIOME_RAINFOREST || id > MC_BIOME_SKY) {
            return MC_NORMAL_GENERATOR_STATE_ERROR;
        }
        generator->materials[i] = resources->materials->by_biome[(size_t)id - 1u];
    }
    result->data = staged_data;
    result->chunk = (mc_chunk_state){.data = &result->data, .x = chunk_x, .z = chunk_z};
    memset(blocks, 0, MC_CHUNK_BLOCKS);
    mc_terrain_normal_workspace work = {{generator->mix, 425},
                                        {generator->low, 425},
                                        {generator->high, 425},
                                        {generator->column_scale, 25},
                                        {generator->column_bias, 25}};
    mc_terrain_normal_climate climate = {
        generator->biome_manager->a.data, generator->biome_manager->a.length,
        generator->biome_manager->b.data, generator->biome_manager->b.length};
    mc_terrain_normal_surface_workspace surface = {
        {generator->sand, 256}, {generator->gravel, 256}, {generator->thickness, 256}};
    /* Fixed embedded spans, ordinary cave coordinates and finishing resources
     * were validated before sampling. Keep unexpected contract failures visible. */
    if (!mc_terrain_normal_generate(&generator->terrain, &work, chunk_x, chunk_z, &climate,
                                    (mc_terrain_double_span){generator->density, 425}, blocks,
                                    blocks_capacity) ||
        !mc_terrain_normal_surface(&generator->terrain, &surface, chunk_x, chunk_z,
                                   generator->materials, 256, &generator_random, blocks,
                                   blocks_capacity) ||
        !(generator->origin == MC_NORMAL_GENERATOR_PROVIDER
              ? mc_terrain_normal_caves_reseed(generator->trig, generator->world_seed, chunk_x,
                                               chunk_z, &cave_random, blocks, blocks_capacity)
              : mc_terrain_normal_caves(generator->trig, generator->world_seed, chunk_x, chunk_z,
                                        &cave_random, blocks, blocks_capacity)) ||
        !mc_chunk_finish_normal(&result->chunk, resources->lighting, resources->opacity,
                                resources->opacity_length, resources->trace,
                                resources->queue_aborts)) {
        return MC_NORMAL_GENERATOR_STATE_ERROR;
    }
    generator->generator_random = generator_random;
    generator->cave_random = cave_random;
    return MC_NORMAL_GENERATOR_OK;
}
