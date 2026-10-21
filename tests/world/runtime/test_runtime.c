#include "mc/world/runtime/runtime.h"

#include "mc/entity/entity.h"

#include <emmintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Constructed owned-file and lifetime contracts, not original-game observations.
 * The driver supplies a fresh owned root as UTF-8 hex on both native platforms. */
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
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_store_options store_options;
    mc_world_runtime *runtime;
    mc_java_random_profile *random_profile;
    mc_chunk_cache cache;
    mc_chunk_cache_entry entries[8];
    mc_chunk_state *list[16];
    mc_chunk_pending pending;
    int32_t keys[16];
    mc_java_int_set *history;
    uint8_t map[256];
    mc_chunk_data data;
    uint8_t blocks[MC_CHUNK_BLOCKS];
    mc_chunk_state source;
    mc_world_storage_error error;
    unsigned auxiliary_calls, progress_calls, provider_calls, populate_calls;
    bool callback_error;
    mc_biome_memory memory;
    mc_biome_manager manager;
    mc_normal_generator generator;
    mc_normal_generator_materials materials;
    mc_math_trig trig;
    mc_light_region light_entries[4096];
    mc_light_queue queue;
    int32_t depth, opacity[256];
    mc_world_lighting lighting;
    mc_normal_generation_resources generation;
} fixture;

static fixture f;
static mc_chunk_data saved_data;
static uint8_t saved_blocks[MC_CHUNK_BLOCKS];
static unsigned char saved_generator[sizeof(mc_normal_generator)];
static mc_chunk_cache registered_caches[257];

static uint32_t region_clock(void *owner) {
    (void)owner;
    return UINT32_C(0xfedcba98);
}

static int64_t metadata_clock(void *owner) {
    (void)owner;
    return INT64_C(900000123);
}

static void check_busy(void) {
    if (f.runtime == NULL || mc_world_runtime_free(f.runtime, NULL) != MC_WORLD_STORAGE_BUSY)
        f.callback_error = true;
}

static mc_chunk_callback_result auxiliary(void *owner, const mc_world_save_context *context) {
    (void)owner;
    (void)context;
    ++f.auxiliary_calls;
    check_busy();
    return MC_CHUNK_CALLBACK_OK;
}

static mc_chunk_callback_result progress(void *owner, const mc_world_save_context *context,
                                         mc_world_save_stage stage) {
    (void)owner;
    (void)context;
    (void)stage;
    ++f.progress_calls;
    check_busy();
    return MC_CHUNK_CALLBACK_OK;
}

static mc_chunk_callback_result provider(void *owner, const mc_chunk_lifecycle_context *context,
                                         bool *result) {
    (void)owner;
    (void)context;
    ++f.provider_calls;
    check_busy();
    *result = false;
    return MC_CHUNK_CALLBACK_OK;
}

static bool populate(void *owner, const mc_chunk_acquire_context *context, int32_t x, int32_t z) {
    (void)owner;
    (void)context;
    (void)x;
    (void)z;
    ++f.populate_calls;
    check_busy();
    return true;
}

static int open_fixture(const char *root, size_t max_chunks, size_t nbt_budget) {
    memset(&f, 0, sizeof(f));
    for (size_t i = 0; i < 256; ++i)
        f.map[i] = (uint8_t)i;
    f.store_options = (mc_world_store_options){.create_players = false,
                                               .session_token = INT64_C(0x123456789abcdef),
                                               .block_map = f.map,
                                               .block_map_length = 256,
                                               .limits = mc_world_store_default_limits()};
    f.store_options.limits.max_loaded_chunks = max_chunks;
    f.store_options.limits.nbt.max_allocation_bytes = nbt_budget;
    CHECK(mc_world_regions_create(NULL, &f.regions, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_open(root, f.regions, &f.store_options, &f.store, &f.error) ==
          MC_WORLD_STORAGE_OK);
    static const uint16_t name[] = {'L', 'i', 'f', 'e', 0xd800, 0};
    mc_level_info info = {.seed = 23,
                          .time = 1000,
                          .last_played = -15,
                          .level_name = {name, sizeof(name) / sizeof(*name)},
                          .version = 19132};
    CHECK(mc_world_store_save_info(f.store, &info, 100, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_chunk_data_init(&f.data, f.blocks, sizeof(f.blocks)));
    f.source = (mc_chunk_state){.data = &f.data, .x = 16, .dirty = true, .terrain_populated = true};
    CHECK(mc_chunk_cache_init(&f.cache, f.entries, 8, f.list, 16));
    CHECK(mc_java_int_set_create(NULL, &f.history) == MC_JAVA_INT_SET_OK);
    CHECK(mc_chunk_pending_init_ordered(&f.pending, f.keys, 16, f.history));
    return 0;
}

static void close_fixture(void) {
    mc_normal_generator_free(&f.generator);
    mc_biome_manager_free(&f.manager);
    mc_java_int_set_free(f.history);
    mc_world_store_free(f.store);
    mc_world_regions_free(f.regions);
}

static int create_runtime(bool generation) {
    mc_world_runtime_options options = {.store = f.store,
                                        .cache = &f.cache,
                                        .pending = &f.pending,
                                        .dimension = 0,
                                        .policy = MC_CHUNK_ACQUIRE_DEDICATED,
                                        .provider_maintenance = provider,
                                        .save_auxiliary = auxiliary,
                                        .report_progress = progress,
                                        .region_timestamp = region_clock,
                                        .metadata_timestamp = metadata_clock,
                                        .random_profile = f.random_profile};
    if (generation) {
        options.generator = &f.generator;
        options.generation = &f.generation;
        options.populate = populate;
    }
    CHECK(mc_world_runtime_create(&options, &f.runtime, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK((*mc_world_runtime_live(f.runtime)->time) == 1000);
    CHECK(mc_world_runtime_acquisition(f.runtime)->world_time ==
          mc_world_runtime_live(f.runtime)->time);
    return 0;
}

static int ownership(void) {
    mc_chunk_state *a = NULL, *b = NULL, *c = NULL;
    CHECK(mc_world_store_retain_chunk(f.store, 0, &f.source, &a, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_retain_chunk(f.store, 0, &f.source, &b, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_retain_chunk(f.store, 0, &f.source, &c, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(a != b && b != c && a != c && mc_world_store_chunk_count(f.store) == 3);
    mc_world_store_reservation *reservation = NULL;
    CHECK(mc_world_store_reserve_chunk(f.store, 0, &reservation, &f.error) ==
              MC_WORLD_STORAGE_LIMIT &&
          reservation == NULL);
    CHECK(mc_world_store_retire_chunk(f.store, b, &f.error) == MC_WORLD_STORAGE_OK);
    b = NULL;
    CHECK(mc_world_store_reserve_chunk(f.store, -1, &reservation, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_reservation_count(f.store) == 1 &&
          mc_world_store_chunk_count(f.store) == 2);
    CHECK(mc_world_store_retain_chunk(f.store, 0, &f.source, &b, &f.error) ==
              MC_WORLD_STORAGE_LIMIT &&
          b == NULL);
    f.source.has_entities = true;
    CHECK(mc_world_store_retain_reserved(f.store, reservation, &f.source, &b, &f.error) ==
          MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    CHECK(b == NULL && mc_world_store_reservation_count(f.store) == 1);
    f.source.has_entities = false;
    CHECK(mc_world_store_retain_reserved(f.store, reservation, &f.source, &b, &f.error) ==
          MC_WORLD_STORAGE_OK);
    reservation = NULL;
    CHECK(mc_world_store_reservation_count(f.store) == 0 &&
          mc_world_store_chunk_count(f.store) == 3);
    CHECK(mc_world_store_owns_chunk(f.store, a) && !mc_world_store_owns_chunk(f.store, &f.source));
    CHECK(mc_world_store_release_chunk(f.store, a, &f.error) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_world_store_lease_chunk(f.store, a, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_lease_chunk(f.store, a, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_retire_chunk(f.store, a, &f.error) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_store_release_chunk(f.store, a, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_retire_chunk(f.store, a, &f.error) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_store_release_chunk(f.store, a, &f.error) == MC_WORLD_STORAGE_OK);
    mc_chunk_state fallback = {.blank = true}, *fallback_slot = &fallback;
    CHECK(mc_world_store_register_cache(f.store, &f.cache, &fallback_slot, &f.error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_register_cache(f.store, &f.cache, &fallback_slot, &f.error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_register_cache(f.store, &f.cache, NULL, &f.error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, a));
    CHECK(mc_chunk_cache_publish(&f.cache, 32768, 0, c));
    CHECK(f.cache.count == 1 && f.cache.insertion_count == 2);
    CHECK(mc_world_store_retire_chunk(f.store, a, &f.error) == MC_WORLD_STORAGE_BUSY);
    f.cache.insertion_count = 0;
    fallback_slot = a;
    CHECK(mc_world_store_retire_chunk(f.store, a, &f.error) == MC_WORLD_STORAGE_BUSY);
    fallback_slot = &fallback;
    f.cache.count = 0;
    a->active = true;
    CHECK(mc_world_store_retire_chunk(f.store, a, &f.error) == MC_WORLD_STORAGE_BUSY);
    a->active = false;
    f.cache.count = f.cache.capacity + 1;
    CHECK(mc_world_store_retire_chunk(f.store, a, &f.error) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    f.cache.count = 0;
    CHECK(mc_world_store_retire_chunk(f.store, a, &f.error) == MC_WORLD_STORAGE_OK); /* Head. */
    CHECK(mc_world_store_retire_chunk(f.store, b, &f.error) == MC_WORLD_STORAGE_OK); /* Tail. */
    CHECK(mc_world_store_retire_chunk(f.store, c, &f.error) == MC_WORLD_STORAGE_OK); /* Last. */
    CHECK(mc_world_store_chunk_count(f.store) == 0);
    CHECK(mc_world_store_unregister_cache(f.store, &f.cache, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_unregister_cache(f.store, &f.cache, &f.error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_world_store_reserve_chunk(f.store, 0, &reservation, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_cancel_reservation(f.store, reservation, &f.error) == MC_WORLD_STORAGE_OK);
    reservation = NULL;
    CHECK(mc_world_store_reservation_count(f.store) == 0);
    for (size_t i = 0; i < 257; ++i)
        CHECK(mc_chunk_cache_init(&registered_caches[i], NULL, 0, NULL, 0));
    for (size_t i = 0; i < 256; ++i)
        CHECK(mc_world_store_register_cache(f.store, &registered_caches[i], NULL, &f.error) ==
              MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_register_cache(f.store, &registered_caches[256], NULL, &f.error) ==
          MC_WORLD_STORAGE_LIMIT);
    for (size_t i = 0; i < 256; ++i)
        CHECK(mc_world_store_unregister_cache(f.store, &registered_caches[i], &f.error) ==
              MC_WORLD_STORAGE_OK);
    return 0;
}

static int load_save_unload(const char *root) {
    mc_chunk_state *baseline = NULL;
    CHECK(mc_world_store_retain_chunk(f.store, 0, &f.source, &baseline, &f.error) ==
          MC_WORLD_STORAGE_OK);
    uint32_t growth = 0;
    CHECK(mc_world_store_write_chunk(f.store, baseline, 1000, 3, &growth, &f.error) ==
              MC_WORLD_STORAGE_OK &&
          growth != 0);
    CHECK(create_runtime(false) == 0);
    const mc_world_runtime_state *live = mc_world_runtime_live(f.runtime);
    *live->world_force_load = true;
    mc_chunk_state *chunk = NULL;
    CHECK(mc_world_runtime_get(f.runtime, 16, 0, &chunk) == MC_CHUNK_ACQUIRE_OK);
    CHECK(chunk != baseline && chunk->active && !chunk->dirty && chunk->last_save_time == 1000);
    CHECK(mc_world_store_retire_chunk(f.store, baseline, &f.error) == MC_WORLD_STORAGE_OK);
    baseline = NULL;
    CHECK(mc_world_runtime_free(f.runtime, &f.error) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_store_retire_chunk(f.store, chunk, &f.error) == MC_WORLD_STORAGE_BUSY);
    chunk->has_entities = true;
    growth = 123;
    CHECK(mc_world_store_write_chunk(f.store, chunk, 1000, 4, &growth, &f.error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          growth == 123);
    uint32_t random = 1;
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i) {
        random = random * UINT32_C(1664525) + UINT32_C(1013904223);
        chunk->data->blocks[i] = (uint8_t)(random >> 24u);
    }
    chunk->dirty = true;
    *live->time = 2000;
    mc_level_info current_info;
    CHECK(mc_world_runtime_read_info(f.runtime, &current_info, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(current_info.time == 2000 && current_info.last_played == -15);
    CHECK(mc_world_store_info(f.store)->time == 1000);
    CHECK(mc_world_runtime_read_info(NULL, &current_info, &f.error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(current_info.time == 2000 && current_info.last_played == -15);
    int64_t previous_size = mc_world_store_info(f.store)->size_on_disk;
    CHECK(mc_world_runtime_save(f.runtime, false) == MC_CHUNK_LIFECYCLE_OK);
    CHECK(!chunk->dirty && !chunk->has_entities && chunk->last_save_time == 2000);
    CHECK(*live->time == 2000 && mc_world_store_info(f.store)->size_on_disk > previous_size);
    int64_t grown_size = mc_world_store_info(f.store)->size_on_disk;
    CHECK(mc_world_store_info(f.store)->last_played == -15);
    mc_world_store *observer = NULL;
    CHECK(mc_world_store_open(root, f.regions, &f.store_options, &observer, &f.error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_load_info(observer, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_info(observer)->size_on_disk == previous_size);
    CHECK(mc_world_store_info(observer)->time == 2000 &&
          mc_world_store_info(observer)->last_played == metadata_clock(NULL));
    CHECK(mc_world_runtime_save(f.runtime, true) == MC_CHUNK_LIFECYCLE_OK);
    CHECK(mc_world_store_load_info(observer, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_info(observer)->size_on_disk == grown_size);
    CHECK(mc_world_store_info(f.store)->last_played == -15 && *live->time == 2000);
    mc_world_store_free(observer);
    mc_world_store_options conflict_options = f.store_options;
    ++conflict_options.session_token;
    CHECK(mc_world_store_open(root, f.regions, &conflict_options, &observer, &f.error) ==
          MC_WORLD_STORAGE_OK);
    chunk->dirty = true;
    chunk->has_entities = true;
    *live->time = 3000;
    bool completed = false;
    CHECK(mc_world_runtime_save_chunks(f.runtime, false, &completed) ==
              MC_CHUNK_LIFECYCLE_CALLBACK_FAILED &&
          !completed);
    CHECK(chunk->dirty && chunk->has_entities && chunk->last_save_time == 3000);
    CHECK(mc_world_runtime_last_error(f.runtime)->status == MC_WORLD_STORAGE_SESSION_MISMATCH);
    CHECK(mc_world_runtime_read_info(f.runtime, &current_info, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(current_info.time == 3000 &&
          mc_world_runtime_last_error(f.runtime)->status == MC_WORLD_STORAGE_SESSION_MISMATCH);
    mc_world_store_free(observer);
    CHECK(mc_world_store_open(root, f.regions, &f.store_options, &observer, &f.error) ==
          MC_WORLD_STORAGE_OK);
    mc_world_store_free(observer);
    CHECK(mc_world_runtime_mark_drop(f.runtime, 16, 0) == MC_CHUNK_ACQUIRE_OK &&
          f.pending.count == 1);
    *live->saving_disabled = true;
    bool provider_result = true;
    CHECK(mc_world_runtime_maintain(f.runtime, &provider_result) == MC_CHUNK_LIFECYCLE_OK &&
          !provider_result);
    CHECK(f.pending.count == 1 && chunk->active && chunk->has_entities);
    *live->saving_disabled = false;
    CHECK(mc_world_store_lease_chunk(f.store, chunk, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_maintain(f.runtime, &provider_result) == MC_CHUNK_LIFECYCLE_OK &&
          !provider_result);
    CHECK(f.pending.count == 0 && f.cache.count == 0 && f.cache.insertion_count == 0);
    CHECK(!chunk->active && !chunk->has_entities && chunk->dirty);
    CHECK(mc_world_store_retire_chunk(f.store, chunk, &f.error) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_store_release_chunk(f.store, chunk, &f.error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_retire_chunk(f.store, chunk, &f.error) == MC_WORLD_STORAGE_OK);
    chunk = NULL;
    CHECK(mc_world_store_chunk_count(f.store) == 0);
    CHECK(mc_world_runtime_acquire(f.runtime, 16, 0, &chunk) == MC_CHUNK_ACQUIRE_OK);
    CHECK(chunk->active && !chunk->dirty && chunk->data->blocks[0] == (uint8_t)60 &&
          chunk->last_save_time == 3000);
    CHECK(mc_world_runtime_mark_drop(f.runtime, 16, 0) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_world_runtime_maintain(f.runtime, &provider_result) == MC_CHUNK_LIFECYCLE_OK);
    CHECK(mc_world_store_retire_chunk(f.store, chunk, &f.error) == MC_WORLD_STORAGE_OK);
    chunk = NULL;
    CHECK(!f.callback_error && f.auxiliary_calls == 2 && f.progress_calls == 4 &&
          f.provider_calls == 3);
    CHECK(mc_world_runtime_save(f.runtime, false) == MC_CHUNK_LIFECYCLE_OK);
    CHECK(!f.callback_error && f.auxiliary_calls == 3 && f.progress_calls == 6);
    CHECK(mc_world_runtime_free(f.runtime, &f.error) == MC_WORLD_STORAGE_OK);
    f.runtime = NULL;
    mc_world_regions_limits limited_options = mc_world_regions_default_limits();
    limited_options.image.max_image_bytes = 8192;
    mc_world_regions *limited_regions = NULL;
    mc_world_store *limited_store = NULL;
    CHECK(mc_world_regions_create(&limited_options, &limited_regions, &f.error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_open(root, limited_regions, &f.store_options, &limited_store, &f.error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_load_info(limited_store, &f.error) == MC_WORLD_STORAGE_OK);
    mc_chunk_state *limited_chunk = NULL;
    CHECK(mc_world_store_retain_chunk(limited_store, 0, &f.source, &limited_chunk, &f.error) ==
          MC_WORLD_STORAGE_OK);
    limited_chunk->has_entities = true;
    int64_t limited_size = mc_world_store_info(limited_store)->size_on_disk;
    growth = 456;
    CHECK(mc_world_store_write_runtime_chunk(limited_store, limited_chunk, 4000, region_clock(NULL),
                                             &growth, &f.error) == MC_WORLD_STORAGE_LIMIT);
    CHECK(!limited_chunk->has_entities && limited_chunk->dirty && growth == 456);
    CHECK(mc_world_store_info(limited_store)->size_on_disk == limited_size);
    CHECK(mc_world_store_retire_chunk(limited_store, limited_chunk, &f.error) ==
          MC_WORLD_STORAGE_OK);
    mc_world_store_free(limited_store);
    mc_world_regions_free(limited_regions);
    return 0;
}

static int init_generator(void) {
    CHECK(mc_math_trig_init(&f.trig));
    CHECK(mc_biome_memory_init(&f.memory, MC_BIOME_DEFAULT_ALLOCATION_BYTES) == MC_BIOME_OK);
    CHECK(mc_biome_manager_init(&f.manager, 23, &f.memory) == MC_BIOME_OK);
    CHECK(mc_normal_generator_init(&f.generator, 23, &f.manager, &f.trig) ==
          MC_NORMAL_GENERATOR_OK);
    CHECK(mc_light_queue_init(&f.queue, f.light_entries, 4096));
    f.lighting = (mc_world_lighting){&f.cache, &f.queue, &f.depth, false};
    for (size_t i = 0; i < MC_NORMAL_GENERATOR_BIOME_MATERIALS; ++i)
        f.materials.by_biome[i] = (mc_terrain_surface_material){2, 3};
    for (size_t i = 1; i < 256; ++i)
        f.opacity[i] = 255; /* Constructed opacity, not an original registry. */
    f.generation = (mc_normal_generation_resources){.lighting = &f.lighting,
                                                    .materials = &f.materials,
                                                    .opacity = f.opacity,
                                                    .opacity_length = 256};
    return 0;
}

static int generation(bool copy_fails) {
    CHECK(init_generator() == 0 && create_runtime(true) == 0);
    memcpy(saved_generator, &f.generator, sizeof(f.generator));
    mc_chunk_state *first = NULL;
    mc_chunk_acquire_status status = mc_world_runtime_acquire(f.runtime, 20, 0, &first);
    if (copy_fails) {
        CHECK(status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED && first == NULL);
        CHECK(mc_world_runtime_last_error(f.runtime)->status == MC_WORLD_STORAGE_LIMIT);
        CHECK(memcmp(saved_generator, &f.generator, sizeof(f.generator)) != 0);
        CHECK(mc_world_store_reservation_count(f.store) == 0 &&
              mc_world_store_chunk_count(f.store) == 0);
        CHECK(f.cache.count == 0 && f.cache.insertion_count == 0 && f.pending.count == 0);
    } else {
        CHECK(status == MC_CHUNK_ACQUIRE_OK && first != NULL);
        CHECK(mc_world_store_owns_chunk(f.store, first) && first->active && first->dirty);
        memcpy(&saved_data, first->data, sizeof(saved_data));
        memcpy(saved_blocks, first->data->blocks, sizeof(saved_blocks));
        mc_chunk_state *second = NULL;
        CHECK(mc_world_runtime_acquire(f.runtime, 22, 0, &second) == MC_CHUNK_ACQUIRE_OK);
        CHECK(first != second && first->data != second->data &&
              first->data->blocks != second->data->blocks);
        CHECK(memcmp(&saved_data, first->data, sizeof(saved_data)) == 0);
        CHECK(memcmp(saved_blocks, first->data->blocks, sizeof(saved_blocks)) == 0);
        mc_world_store_reservation *reserved = NULL;
        CHECK(mc_world_store_reserve_chunk(f.store, 0, &reserved, &f.error) == MC_WORLD_STORAGE_OK);
        memcpy(saved_generator, &f.generator, sizeof(f.generator));
        mc_chunk_state *third = NULL;
        CHECK(mc_world_runtime_acquire(f.runtime, 24, 0, &third) ==
                  MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
              third == NULL);
        CHECK(memcmp(saved_generator, &f.generator, sizeof(f.generator)) == 0);
        CHECK(mc_world_store_cancel_reservation(f.store, reserved, &f.error) ==
              MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_mark_drop(f.runtime, 20, 0) == MC_CHUNK_ACQUIRE_OK);
        CHECK(mc_world_runtime_mark_drop(f.runtime, 22, 0) == MC_CHUNK_ACQUIRE_OK);
        bool result = true;
        CHECK(mc_world_runtime_maintain(f.runtime, &result) == MC_CHUNK_LIFECYCLE_OK && !result);
        CHECK(mc_world_store_retire_chunk(f.store, first, &f.error) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_store_retire_chunk(f.store, second, &f.error) == MC_WORLD_STORAGE_OK);
    }
    CHECK(!f.callback_error && f.populate_calls == 0);
    CHECK(mc_world_runtime_free(f.runtime, &f.error) == MC_WORLD_STORAGE_OK);
    f.runtime = NULL;
    return 0;
}

#define BINDING_CHECK(condition)                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static uint64_t binding_double_bits(const double *value) {
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(*value), "binary64 storage required");
    memcpy(&bits, value, sizeof(bits));
    return bits;
}

static void binding_poison_random(mc_java_random *random) {
    memset(random, 0xa5, sizeof(*random));
    random->next_next_gaussian = -0.0;
    random->have_next_next_gaussian = true;
}

static bool binding_same_error(const mc_world_storage_error *left,
                               const mc_world_storage_error *right) {
    return left->status == right->status && left->operation == right->operation &&
           left->system_error == right->system_error;
}

static mc_java_random_profile_status binding_random_with_control(unsigned int control,
                                                                 mc_java_random *out) {
    const unsigned int saved = _mm_getcsr();
    _mm_setcsr(control);
    const mc_java_random_profile_status status =
        mc_world_runtime_construct_default_random(f.runtime, out);
    /* Restore the complete word before any CHECK, diagnostic, cleanup or return. */
    _mm_setcsr(saved);
    return status;
}

static mc_world_storage_status binding_free_runtime(void) {
    const mc_world_storage_status status = mc_world_runtime_free(f.runtime, &f.error);
    if (status == MC_WORLD_STORAGE_OK)
        f.runtime = NULL;
    return status;
}

static bool binding_runtime_is_busy(void) {
    const mc_world_storage_status status = binding_free_runtime();
    if (status == MC_WORLD_STORAGE_OK) {
        /* A broken lifetime guard has already freed a world borrowed by an
         * entity. Releasing that entity would dereference retired storage. */
        fprintf(stderr, "runtime freed despite a live persistent binding\n");
        abort();
    }
    return status == MC_WORLD_STORAGE_BUSY;
}

static int profile_and_entity_bindings(void) {
    int result = 1;
    size_t manual_bindings = 0;
    mc_java_random_profile *profile = NULL, *limited_profile = NULL;
    mc_entity_classes *classes = NULL, *limited_classes = NULL;
    mc_entity *ready = NULL, *limited = NULL;
    mc_entity_error entity_error = {0};
    mc_entity_snapshot before_entity = {0}, after_entity = {0};
    mc_entity_class_snapshot before_class = {0}, after_class = {0};
    mc_java_random random;
    unsigned char random_before[sizeof(random)];
    mc_level_info info;
    mc_world_storage_error prior_error;
    size_t profile_bytes = 0, raw_bytes = 0;
    unsigned int saved_control = 0;
    mc_java_random_profile_status status;
    double failed_math = -7.25;
    const uint64_t failed_math_before = binding_double_bits(&failed_math);

    BINDING_CHECK((_mm_getcsr() & 0xffc0u) == 0x1f80u);
    BINDING_CHECK(mc_world_runtime_random_profile(NULL) == NULL);
    BINDING_CHECK(mc_world_runtime_last_random_status(NULL) ==
                  MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    BINDING_CHECK(!mc_world_runtime_binding_retain(NULL));
    mc_world_runtime_binding_release(NULL);
    binding_poison_random(&random);
    memcpy(random_before, &random, sizeof(random_before));
    BINDING_CHECK(mc_world_runtime_construct_default_random(NULL, &random) ==
                  MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    BINDING_CHECK(memcmp(random_before, &random, sizeof(random_before)) == 0);

    /* The existing foundation remains valid with no profile and no generator. */
    BINDING_CHECK(create_runtime(false) == 0);
    BINDING_CHECK(mc_world_runtime_random_profile(f.runtime) == NULL);
    BINDING_CHECK(mc_world_runtime_last_random_status(f.runtime) == MC_JAVA_RANDOM_PROFILE_OK);
    BINDING_CHECK(mc_world_runtime_construct_default_random(f.runtime, &random) ==
                  MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    BINDING_CHECK(memcmp(random_before, &random, sizeof(random_before)) == 0);
    BINDING_CHECK(mc_world_runtime_construct_default_random(f.runtime, NULL) ==
                  MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    BINDING_CHECK(mc_world_runtime_last_random_status(f.runtime) ==
                  MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    BINDING_CHECK(mc_world_runtime_last_error(f.runtime)->status ==
                      MC_WORLD_STORAGE_INVALID_ARGUMENT &&
                  mc_world_runtime_last_error(f.runtime)->system_error == 0 &&
                  mc_world_runtime_last_error(f.runtime)->operation != NULL);
    BINDING_CHECK(mc_world_runtime_binding_retain(f.runtime));
    ++manual_bindings;
    BINDING_CHECK(mc_world_runtime_binding_retain(f.runtime));
    ++manual_bindings;
    BINDING_CHECK(binding_runtime_is_busy());
    mc_world_runtime_binding_release(f.runtime);
    --manual_bindings;
    BINDING_CHECK(binding_runtime_is_busy());
    mc_world_runtime_binding_release(f.runtime);
    --manual_bindings;
    mc_world_runtime_binding_release(f.runtime); /* Zero-count misuse, not underflow. */
    BINDING_CHECK(mc_world_runtime_last_error(f.runtime)->status ==
                      MC_WORLD_STORAGE_INVALID_ARGUMENT &&
                  mc_world_runtime_last_error(f.runtime)->system_error == 0 &&
                  mc_world_runtime_last_error(f.runtime)->operation != NULL);
    prior_error = *mc_world_runtime_last_error(f.runtime);
    BINDING_CHECK(mc_world_runtime_read_info(f.runtime, &info, &f.error) == MC_WORLD_STORAGE_OK);
    BINDING_CHECK(mc_world_runtime_last_random_status(f.runtime) ==
                  MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    BINDING_CHECK(binding_same_error(mc_world_runtime_last_error(f.runtime), &prior_error));
    BINDING_CHECK(binding_free_runtime() == MC_WORLD_STORAGE_OK);

    BINDING_CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    profile_bytes = mc_java_random_profile_allocation_bytes(profile); /* Quiescent and cold. */
    BINDING_CHECK(profile_bytes != 0);
    BINDING_CHECK(mc_java_random_profile_create(profile_bytes, &limited_profile) ==
                  MC_JAVA_RANDOM_PROFILE_OK);
    BINDING_CHECK(mc_java_random_profile_allocation_bytes(limited_profile) == profile_bytes);
    BINDING_CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) ==
                  MC_ENTITY_OK);
    BINDING_CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, limited_profile, NULL,
                                           &limited_classes) == MC_ENTITY_OK);
    f.random_profile = profile;
    BINDING_CHECK(create_runtime(false) == 0);
    BINDING_CHECK(mc_world_runtime_random_profile(f.runtime) == profile);
    BINDING_CHECK(mc_world_runtime_last_random_status(f.runtime) == MC_JAVA_RANDOM_PROFILE_OK);

    /* An unrelated diagnostic survives a successful actual core constructor. */
    BINDING_CHECK(mc_world_runtime_last_error(f.runtime)->status == MC_WORLD_STORAGE_OK);
    mc_world_runtime_binding_release(f.runtime);
    prior_error = *mc_world_runtime_last_error(f.runtime);
    BINDING_CHECK(prior_error.status == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
                  prior_error.system_error == 0 && prior_error.operation != NULL);
    BINDING_CHECK(mc_world_runtime_construct_default_random(f.runtime, &random) ==
                  MC_JAVA_RANDOM_PROFILE_OK);
    BINDING_CHECK(random.state <= UINT64_C(0xffffffffffff) &&
                  binding_double_bits(&random.next_next_gaussian) == 0 &&
                  !random.have_next_next_gaussian);
    BINDING_CHECK(mc_world_runtime_last_random_status(f.runtime) == MC_JAVA_RANDOM_PROFILE_OK);
    BINDING_CHECK(binding_same_error(mc_world_runtime_last_error(f.runtime), &prior_error));
    BINDING_CHECK(mc_world_runtime_read_info(f.runtime, &info, &f.error) == MC_WORLD_STORAGE_OK);
    BINDING_CHECK(mc_world_runtime_last_random_status(f.runtime) == MC_JAVA_RANDOM_PROFILE_OK);
    BINDING_CHECK(mc_world_runtime_construct_default_random(f.runtime, NULL) ==
                  MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    BINDING_CHECK(mc_world_runtime_read_info(f.runtime, &info, &f.error) == MC_WORLD_STORAGE_OK);
    BINDING_CHECK(mc_world_runtime_last_random_status(f.runtime) ==
                  MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);

    binding_poison_random(&random);
    memcpy(random_before, &random, sizeof(random_before));
    saved_control = _mm_getcsr();
    status = binding_random_with_control(saved_control | 0x8000u, &random);
    BINDING_CHECK(_mm_getcsr() == saved_control);
    BINDING_CHECK(status == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP);
    BINDING_CHECK(memcmp(random_before, &random, sizeof(random_before)) == 0);
    BINDING_CHECK(mc_world_runtime_last_random_status(f.runtime) ==
                  MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP);
    BINDING_CHECK(mc_world_runtime_last_error(f.runtime)->status ==
                      MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  mc_world_runtime_last_error(f.runtime)->system_error == 0 &&
                  mc_world_runtime_last_error(f.runtime)->operation != NULL);
    prior_error = *mc_world_runtime_last_error(f.runtime);
    BINDING_CHECK(mc_world_runtime_read_info(f.runtime, &info, &f.error) == MC_WORLD_STORAGE_OK);
    BINDING_CHECK(mc_world_runtime_last_random_status(f.runtime) ==
                  MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP);
    BINDING_CHECK(binding_same_error(mc_world_runtime_last_error(f.runtime), &prior_error));
    BINDING_CHECK(mc_world_runtime_construct_default_random(f.runtime, &random) ==
                  MC_JAVA_RANDOM_PROFILE_OK);
    BINDING_CHECK(binding_same_error(mc_world_runtime_last_error(f.runtime), &prior_error));
    BINDING_CHECK(mc_java_random_profile_allocation_bytes(profile) == profile_bytes);

    /* Different nonnull profiles reject while this actual allocated entity is RAW. */
    BINDING_CHECK(mc_item_entity_allocate(limited_classes, &limited, &entity_error) ==
                  MC_ENTITY_OK);
    BINDING_CHECK(mc_entity_get_snapshot(limited, &before_entity) == MC_ENTITY_OK);
    BINDING_CHECK(before_entity.phase == MC_ENTITY_RAW && before_entity.base.world == NULL);
    BINDING_CHECK(mc_entity_classes_get_snapshot(limited_classes, &before_class) == MC_ENTITY_OK);
    raw_bytes = mc_entity_native_bytes(limited);
    BINDING_CHECK(raw_bytes != 0);
    BINDING_CHECK(mc_item_entity_construct(limited, f.runtime, 2.0, 4.0, 8.0, NULL,
                                           &entity_error) == MC_ENTITY_INVALID_ARGUMENT);
    BINDING_CHECK(entity_error.status == MC_ENTITY_INVALID_ARGUMENT);
    BINDING_CHECK(mc_entity_get_snapshot(limited, &after_entity) == MC_ENTITY_OK);
    BINDING_CHECK(after_entity.phase == MC_ENTITY_RAW &&
                  after_entity.base_bytecode == before_entity.base_bytecode &&
                  after_entity.item_bytecode == before_entity.item_bytecode &&
                  after_entity.base.id == before_entity.base.id &&
                  after_entity.base.world == before_entity.base.world);
    BINDING_CHECK(
        binding_double_bits(&after_entity.base.x) == binding_double_bits(&before_entity.base.x) &&
        binding_double_bits(&after_entity.base.y) == binding_double_bits(&before_entity.base.y) &&
        binding_double_bits(&after_entity.base.z) == binding_double_bits(&before_entity.base.z));
    BINDING_CHECK(after_entity.bounds == before_entity.bounds &&
                  after_entity.random == before_entity.random &&
                  after_entity.watcher == before_entity.watcher &&
                  after_entity.stack == before_entity.stack &&
                  mc_entity_native_bytes(limited) == raw_bytes);
    BINDING_CHECK(mc_entity_classes_get_snapshot(limited_classes, &after_class) == MC_ENTITY_OK);
    BINDING_CHECK(after_class.next_id == before_class.next_id &&
                  after_class.live_entities == before_class.live_entities &&
                  after_class.allocation_bytes == before_class.allocation_bytes &&
                  after_class.entity_class == before_class.entity_class &&
                  after_class.bounds_class == before_class.bounds_class &&
                  after_class.watcher_class == before_class.watcher_class &&
                  after_class.bounds_pool_identity == before_class.bounds_pool_identity &&
                  after_class.watcher_types_identity == before_class.watcher_types_identity);

    BINDING_CHECK(mc_item_entity_allocate(classes, &ready, &entity_error) == MC_ENTITY_OK);
    BINDING_CHECK(mc_item_entity_construct(ready, f.runtime, 1.0, 2.0, 3.0, NULL, &entity_error) ==
                  MC_ENTITY_OK);
    BINDING_CHECK(mc_entity_get_snapshot(ready, &after_entity) == MC_ENTITY_OK);
    BINDING_CHECK(after_entity.phase == MC_ENTITY_READY && after_entity.base.world == f.runtime &&
                  after_entity.stack == NULL && after_entity.random != NULL &&
                  after_entity.bounds != NULL && after_entity.watcher != NULL);
    BINDING_CHECK(binding_runtime_is_busy());
    BINDING_CHECK(mc_world_runtime_read_info(f.runtime, &info, &f.error) == MC_WORLD_STORAGE_OK);
    mc_entity_release(ready);
    ready = NULL;
    BINDING_CHECK(binding_free_runtime() == MC_WORLD_STORAGE_OK);

    /* Reuse the unchanged RAW object with its matching world. Math's separate
     * allocation now fails against an actually measured profile-only budget. */
    f.random_profile = limited_profile;
    BINDING_CHECK(create_runtime(false) == 0);
    BINDING_CHECK(mc_world_runtime_random_profile(f.runtime) == limited_profile);
    BINDING_CHECK(mc_item_entity_construct(limited, f.runtime, 2.0, 4.0, 8.0, NULL,
                                           &entity_error) == MC_ENTITY_PROFILE_FAILED);
    BINDING_CHECK(entity_error.status == MC_ENTITY_PROFILE_FAILED &&
                  entity_error.profile == MC_JAVA_RANDOM_PROFILE_LIMIT);
    BINDING_CHECK(mc_entity_get_snapshot(limited, &after_entity) == MC_ENTITY_OK);
    BINDING_CHECK(after_entity.phase == MC_ENTITY_FAILED_CONSTRUCTION &&
                  after_entity.base.world == f.runtime && after_entity.random != NULL &&
                  after_entity.bounds != NULL && after_entity.watcher != NULL);
    BINDING_CHECK(mc_entity_classes_get_snapshot(limited_classes, &after_class) == MC_ENTITY_OK);
    BINDING_CHECK(after_class.next_id == mc_java_i32_add(before_class.next_id, 1));
    BINDING_CHECK(mc_java_random_profile_allocation_bytes(limited_profile) == profile_bytes);
    BINDING_CHECK(mc_java_math_random(limited_profile, &failed_math) ==
                  MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED);
    BINDING_CHECK(binding_double_bits(&failed_math) == failed_math_before);
    BINDING_CHECK(binding_runtime_is_busy());
    mc_entity_release(limited);
    limited = NULL;
    BINDING_CHECK(binding_free_runtime() == MC_WORLD_STORAGE_OK);
    BINDING_CHECK(!f.callback_error && f.auxiliary_calls == 0 && f.progress_calls == 0 &&
                  f.provider_calls == 0 && f.populate_calls == 0);
    result = 0;
cleanup:
    mc_entity_release(ready);
    mc_entity_release(limited);
    while (manual_bindings != 0 && f.runtime != NULL) {
        mc_world_runtime_binding_release(f.runtime);
        --manual_bindings;
    }
    if (f.runtime != NULL && binding_free_runtime() != MC_WORLD_STORAGE_OK) {
        fprintf(stderr, "runtime teardown failed after releasing owned bindings\n");
        abort(); /* Do not retire its still-borrowed store/profile underneath it. */
    }
    if (mc_entity_classes_free(limited_classes) != MC_ENTITY_OK ||
        mc_entity_classes_free(classes) != MC_ENTITY_OK) {
        fprintf(stderr, "entity class teardown failed after releasing owned entities\n");
        abort();
    }
    f.random_profile = NULL;
    mc_java_random_profile_free(limited_profile);
    mc_java_random_profile_free(profile);
    return result;
}

static bool decode_path(const char *hex, char *path, size_t capacity) {
    size_t length = strlen(hex);
    if (length == 0 || length % 2 != 0 || length / 2 >= capacity)
        return false;
    for (size_t i = 0; i < length / 2; ++i) {
        unsigned value = 0;
        for (size_t j = 0; j < 2; ++j) {
            char c = hex[i * 2 + j];
            unsigned digit = c >= '0' && c <= '9'   ? (unsigned)(c - '0')
                             : c >= 'a' && c <= 'f' ? (unsigned)(c - 'a') + 10u
                                                    : 16u;
            if (digit == 16)
                return false;
            value = value * 16u + digit;
        }
        if (value == 0)
            return false;
        path[i] = (char)(unsigned char)value;
    }
    path[length / 2] = '\0';
    return true;
}

int main(int argc, char **argv) {
    char root[8192];
    if (argc != 3 || !decode_path(argv[2], root, sizeof(root)))
        return 1;
    bool copy_fails = strcmp(argv[1], "generation-copy-limit") == 0;
    bool profile_bindings = strcmp(argv[1], "profile-and-entity-bindings") == 0;
    size_t budget = copy_fails ? 32768u : mc_world_store_default_limits().nbt.max_allocation_bytes;
    if (open_fixture(root, 3, budget) != 0) {
        if (profile_bindings)
            close_fixture();
        return 1;
    }
    int result;
    if (strcmp(argv[1], "ownership-and-runtime") == 0)
        result = ownership() != 0 || load_save_unload(root) != 0;
    else if (strcmp(argv[1], "generation") == 0 || copy_fails)
        result = generation(copy_fails);
    else if (profile_bindings)
        result = profile_and_entity_bindings();
    else
        result = 1;
    close_fixture();
    if (!result)
        printf("PASS: %u constructed runtime/owner checks (%s)\n", checks, argv[1]);
    return result;
}
