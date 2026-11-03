/* Fixed-work CPU benchmark. No display, Source tick, raw cache export,
 * direct Source mutation, assertion, capture or disk work occurs inside a
 * timed sample. Native call holds and diagnostic counters remain ordinary. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "../world/runtime/saving/mob_nbt_source_support.inc"
#include "core/java/random_profile_internal.h"
#include "mc/world/runtime/queries.h"
#include <inttypes.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

enum { BENCH_CALLS = 65536, BENCH_SAMPLES = 7, BENCH_COORDINATES = 256, BENCH_MODES = 6 };

static void bench_chunk_coordinates(unsigned int count, unsigned int index, int32_t *x,
                                    int32_t *z) {
    if (count == 1) {
        *x = 0;
        *z = 0;
    } else if (count == 289) {
        *x = (int32_t)(index / 17u) - 8;
        *z = (int32_t)(index % 17u) - 8;
    } else {
        *x = (int32_t)(index / 16u) - 16;
        *z = (int32_t)(index % 16u) - 8;
    }
}

static int bench_open(source_save_fixture *s) {
    fixture *f = &s->game;
    mc_world_info_name *folder = ss_name("world");
    VERIFY(folder != NULL &&
           mc_world_store_source_allocate(s->handlers, &f->stores[0], &s->source) ==
               MC_WORLD_SOURCE_OK &&
           mc_world_store_source_construct(f->stores[0], s->directory, folder, false, &s->source) ==
               MC_WORLD_SOURCE_OK);
    mc_world_info_name_release(folder);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 512;
    limits.max_scheduled_entries = 0;
    VERIFY(mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK &&
           mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                           &limits, &f->worlds[0],
                                           &s->error) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[0], .name = f->name, .seed = -99, .dimension = f->dimensions[0]};
    VERIFY(mc_world_runtime_construct_world(f->worlds[0], &arguments, &s->error) ==
           MC_WORLD_STORAGE_OK);
    mc_chunk_provider_cache_observation cache;
    VERIFY(mc_chunk_provider_cache_observe(f->worlds[0]->fields.v, &cache) && cache.owned &&
           cache.map_count == 0 && cache.raw_exports == 0 && cache.mutation_depth == 0);
    return 0;
}

static int bench_stop(source_save_fixture *s) {
    fixture *f = &s->game;
    /* Only after all measurement and continuity checks, borrow ordinary raw
     * descriptors for this existing fixture's exact checked cleanup sequence. */
    mc_chunk_provider_fields raw = {0};
    VERIFY(mc_chunk_provider_bind_fields(f->worlds[0]->fields.v, &raw, &s->error) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_retire_entity_containers(f->worlds[0], &s->error) ==
               MC_WORLD_STORAGE_OK &&
           mc_chunk_pending_clear(raw.pending) == MC_CHUNK_ACQUIRE_OK &&
           mc_chunk_cache_init(raw.cache, raw.cache->entries, raw.cache->capacity,
                               raw.cache->insertions, raw.cache->insertion_capacity) &&
           mc_world_runtime_retire_world(f->worlds[0], &s->error) == MC_WORLD_STORAGE_OK);
    while (mc_world_store_chunk_count(f->stores[0]) != 0) {
        const size_t before = mc_world_store_chunk_count(f->stores[0]);
        mc_chunk_state *state = NULL;
        VERIFY(mc_world_store_chunk_at(f->stores[0], 0, &state, &s->error) == MC_WORLD_STORAGE_OK &&
               state != NULL &&
               mc_world_store_retire_chunk(f->stores[0], state, &s->error) == MC_WORLD_STORAGE_OK &&
               mc_world_store_chunk_count(f->stores[0]) == before - 1);
    }
    VERIFY(mc_world_runtime_free(f->worlds[0], &s->error) == MC_WORLD_STORAGE_OK);
    f->worlds[0] = NULL;
    mc_world_provider_release(f->dimensions[0]);
    f->dimensions[0] = NULL;
    VERIFY(mc_world_store_source_release(&f->stores[0], &s->source) == MC_WORLD_SOURCE_OK &&
           f->stores[0] == NULL &&
           mc_world_source_regions_flush(s->source_regions, &s->region) == MC_WORLD_SOURCE_OK);
    return 0;
}

static bool bench_clock(uint64_t *out, uint64_t *frequency) {
#ifdef _WIN32
    LARGE_INTEGER count, hz;
    if (!QueryPerformanceCounter(&count) || !QueryPerformanceFrequency(&hz) || count.QuadPart < 0 ||
        hz.QuadPart <= 0)
        return false;
    *out = (uint64_t)count.QuadPart;
    *frequency = (uint64_t)hz.QuadPart;
#else
    struct timespec count;
    if (clock_gettime(CLOCK_MONOTONIC, &count) != 0 || count.tv_sec < 0)
        return false;
    *out = (uint64_t)count.tv_sec * UINT64_C(1000000000) + (uint64_t)count.tv_nsec;
    *frequency = UINT64_C(1000000000);
#endif
    return true;
}

static int bench_value(mc_world_runtime *world, const mc_world_light_type *light, int mode,
                       unsigned int chunk_count, unsigned int index, int32_t *value,
                       mc_world_storage_error *storage, mc_world_query_error *query) {
    const int32_t x = (int32_t)(index & 15u), z = (int32_t)((index >> 4u) & 15u);
    const int32_t y = 8 + (int32_t)((index * 17u) % 104u);
    if (mode == 0)
        return (int)mc_world_runtime_block_get(world, x, y, z, value, storage);
    if (mode == 1)
        return (int)mc_world_runtime_metadata_get(world, x, y, z, value, storage);
    if (mode == 2)
        return (int)mc_world_runtime_saved_light(world, light, x, y, z, value, query);
    if (mode >= 4) {
        int32_t chunk_x, chunk_z;
        bench_chunk_coordinates(chunk_count, (index * 37u) % chunk_count, &chunk_x, &chunk_z);
        if (mode == 5) {
            chunk_x = 128 + (int32_t)index;
            chunk_z = -128 - (int32_t)index;
        }
        bool present = false;
        const mc_world_storage_status status =
            mc_world_runtime_chunk_provider_contains(world, chunk_x, chunk_z, &present, storage);
        *value = present ? 1 : 0;
        return (int)status;
    }
    mc_chunk_state *state = NULL;
    const mc_chunk_acquire_status status = mc_world_runtime_get_detailed(world, 0, 0, &state, NULL);
    *value = state == NULL ? -1 : state->x;
    return (int)status;
}

static uint64_t bench_mix(uint64_t hash, int32_t value, unsigned int index) {
    return (hash ^ ((uint64_t)(uint32_t)value + index)) * UINT64_C(1099511628211);
}

static int bench_run(source_save_fixture *s, bool mutation, unsigned int chunk_count) {
    static const char *names[] = {"block",     "metadata",     "saved-light",
                                  "owned-get", "presence-hit", "presence-miss"};
    mc_world_runtime *world = s->game.worlds[0];
    const mc_world_light_type *light = NULL;
    mc_chunk_state *chunk = NULL;
    for (unsigned int index = 0; index < chunk_count; ++index) {
        int32_t x, z;
        bench_chunk_coordinates(chunk_count, index, &x, &z);
        mc_chunk_state *loaded = NULL;
        VERIFY(mc_world_runtime_get_detailed(world, x, z, &loaded, NULL) == MC_CHUNK_ACQUIRE_OK &&
               loaded != NULL && loaded->active && !loaded->blank && loaded->x == x &&
               loaded->z == z);
    }
    VERIFY(mc_world_runtime_get_detailed(world, 0, 0, &chunk, NULL) == MC_CHUNK_ACQUIRE_OK &&
           chunk != NULL && chunk->active && !chunk->blank && chunk->data != NULL &&
           mc_world_runtime_light_type_get(world->classes, MC_LIGHT_SKY, &light, &s->error) ==
               MC_WORLD_STORAGE_OK);
    int32_t expected[BENCH_MODES][BENCH_COORDINATES];
    mc_world_storage_error storage;
    mc_world_query_error query;
    for (int mode = 0; mode < BENCH_MODES; ++mode)
        for (unsigned int i = 0; i < BENCH_COORDINATES; ++i) {
            const unsigned int x = i & 15u, z = (i >> 4u) & 15u;
            const unsigned int y = 8u + (i * 17u) % 104u;
            const size_t offset = ((size_t)x * 16u + z) * 128u + y;
            expected[mode][i] =
                mode == 0   ? (int32_t)chunk->data->blocks[offset]
                : mode == 1 ? (int32_t)(((unsigned int)chunk->data->metadata[offset / 2u] >>
                                         ((y & 1u) * 4u)) &
                                        15u)
                : mode == 2 ? (int32_t)(((unsigned int)chunk->data->skylight[offset / 2u] >>
                                         ((y & 1u) * 4u)) &
                                        15u)
                : mode == 3 ? chunk->x
                : mode == 4 ? 1
                            : 0;
            int32_t actual = -1;
            VERIFY(bench_value(world, light, mode, chunk_count, i, &actual, &storage, &query) ==
                       0 &&
                   actual == expected[mode][i]);
        }
    /* These are snapshots of actual read-only owners, never replacement state.
     * The byte snapshots prove no writes occurred, including original Random
     * state. Padding is compared only against the same unchanged allocation. */
    unsigned char fields[sizeof(world->fields)], random[sizeof(*world->fields.r)];
    memcpy(fields, &world->fields, sizeof(fields));
    memcpy(random, world->fields.r, sizeof(random));
    const uint64_t unique = atomic_load(&s->game.profile->uniquifier);
    const unsigned int holder_phase = atomic_load(&s->game.profile->holder_phase);
    mc_java_math_random_owner *const math = s->game.profile->math;
    const uint64_t math_state = math == NULL ? 0 : atomic_load(&math->state);
    uint64_t math_gaussian = 0;
    if (math != NULL)
        memcpy(&math_gaussian, &math->next_next_gaussian, sizeof(math_gaussian));
    const bool math_have_gaussian = math != NULL && math->have_next_next_gaussian;
    unsigned char chunk_state[sizeof(*chunk)], data[sizeof(*chunk->data)];
    memcpy(chunk_state, chunk, sizeof(chunk_state));
    memcpy(data, chunk->data, sizeof(data));
    uint8_t *blocks = malloc(MC_CHUNK_BLOCKS);
    VERIFY(blocks != NULL);
    memcpy(blocks, chunk->data->blocks, MC_CHUNK_BLOCKS);
    mc_chunk_cache *const cache = world->acquisition.cache;
    mc_chunk_pending *const pending = world->acquisition.pending;
    VERIFY(mc_chunk_cache_validate(cache) && mc_chunk_pending_validate(pending) &&
           cache->capacity <= 1024 && cache->insertion_capacity <= 1024 &&
           pending->capacity <= 1024);
    unsigned char cache_descriptor[sizeof(*cache)], pending_descriptor[sizeof(*pending)];
    memcpy(cache_descriptor, cache, sizeof(cache_descriptor));
    memcpy(pending_descriptor, pending, sizeof(pending_descriptor));
    const size_t entries_bytes = cache->capacity * sizeof(*cache->entries);
    const size_t insertions_bytes = cache->insertion_capacity * sizeof(*cache->insertions);
    const size_t pending_bytes = pending->capacity * sizeof(*pending->keys);
    unsigned char *backing = malloc(entries_bytes + insertions_bytes + pending_bytes + 1);
    VERIFY(backing != NULL);
    if (entries_bytes != 0)
        memcpy(backing, cache->entries, entries_bytes);
    if (insertions_bytes != 0)
        memcpy(backing + entries_bytes, cache->insertions, insertions_bytes);
    if (pending_bytes != 0)
        memcpy(backing + entries_bytes + insertions_bytes, pending->keys, pending_bytes);
    int32_t pending_order[1024], after_order[1024];
    size_t pending_count = 0, after_count = 0;
    if (pending->history != NULL)
        VERIFY(mc_java_int_set_copy_order(pending->history, pending_order, COUNT(pending_order),
                                          &pending_count) == MC_JAVA_INT_SET_OK);
    unsigned char storage_diagnostic[sizeof(world->last_error)];
    unsigned char registry_diagnostic[sizeof(world->last_registry_error)];
    memcpy(storage_diagnostic, &world->last_error, sizeof(storage_diagnostic));
    memcpy(registry_diagnostic, &world->last_registry_error, sizeof(registry_diagnostic));
    const mc_java_random_profile_status random_status = world->last_random_status;
    const size_t active_calls = world->active_calls, leases = world->local_leases;
    mc_world_runtime_acquisition_error *const outer = world->acquisition_error;
    mc_world_runtime_lifecycle_error *const lifecycle = world->lifecycle_error;
    if (mutation) {
        VERIFY(mc_world_runtime_call_enter(world, &storage) == MC_WORLD_STORAGE_OK);
        mc_chunk_provider_cache_mutation_begin(world->fields.v);
    }
    mc_chunk_provider_cache_observation before, after;
    VERIFY(mc_chunk_provider_cache_observe(world->fields.v, &before) && before.owned &&
           before.raw_exports == 0 && before.mutation_depth == (mutation ? 1u : 0u) &&
           before.map_count == chunk_count);
    uint64_t last_sink = 0;
    for (int mode = 0; mode < BENCH_MODES; ++mode) {
        uint64_t expected_hash = UINT64_C(1469598103934665603);
        for (unsigned int i = 0; i < BENCH_CALLS; ++i)
            expected_hash = bench_mix(expected_hash, expected[mode][i & 255u], i);
        for (unsigned int sample = 0; sample < BENCH_SAMPLES; ++sample) {
            uint64_t start, end, frequency, end_frequency;
            uint64_t hash = UINT64_C(1469598103934665603);
            unsigned int status_sum = 0;
            int32_t value = -1;
            VERIFY(bench_clock(&start, &frequency));
            /* Timed fixed public query workload. No assertion, disk, allocation,
             * instrumentation callback, direct mutation or RNG call here. */
            for (unsigned int i = 0; i < BENCH_CALLS; ++i) {
                status_sum |= (unsigned int)bench_value(world, light, mode, chunk_count, i & 255u,
                                                        &value, &storage, &query);
                hash = bench_mix(hash, value, i);
            }
            VERIFY(bench_clock(&end, &end_frequency) && frequency == end_frequency && end > start);
            VERIFY(status_sum == 0 && hash == expected_hash);
            if (mode == 2)
                VERIFY(query.status == MC_WORLD_STORAGE_OK && query.operation == NULL &&
                       query.registry.initializer_pc == -1 && query.registry.block_id == -1 &&
                       query.source_chunk == NULL && query.provider_bytecode == 0);
            else if (mode < 2 || mode >= 4)
                VERIFY(storage.status == MC_WORLD_STORAGE_OK && storage.operation == NULL);
            last_sink ^= hash;
            printf("CACHE_QUERY_SAMPLE mode=%s sample=%u calls=%u ticks=%" PRIu64
                   " frequency=%" PRIu64 " checksum=%" PRIu64 "\n",
                   names[mode], sample, BENCH_CALLS, end - start, frequency, hash);
        }
    }
    const uint64_t get_calls = (uint64_t)4u * BENCH_SAMPLES * BENCH_CALLS;
    VERIFY(mc_chunk_provider_cache_observe(world->fields.v, &after) && after.owned &&
           after.raw_exports == 0 && after.mutation_depth == (mutation ? 1u : 0u) &&
           after.map_count == before.map_count && after.insertion_count == before.insertion_count &&
           after.misses == before.misses &&
           after.generic_gets == before.generic_gets + (mutation ? get_calls : 0u) &&
           after.rebuilds == before.rebuilds &&
           after.hits == before.hits + (mutation ? 0u : get_calls) &&
           after.index_bytes == before.index_bytes && after.direct_bytes == before.direct_bytes);
    VERIFY(memcmp(&world->fields, fields, sizeof(fields)) == 0 &&
           memcmp(world->fields.r, random, sizeof(random)) == 0 &&
           atomic_load(&s->game.profile->uniquifier) == unique &&
           atomic_load(&s->game.profile->holder_phase) == holder_phase &&
           s->game.profile->math == math &&
           (math == NULL || atomic_load(&math->state) == math_state) &&
           (math != NULL && math->have_next_next_gaussian) == math_have_gaussian &&
           memcmp(chunk, chunk_state, sizeof(chunk_state)) == 0 &&
           memcmp(chunk->data, data, sizeof(data)) == 0 &&
           memcmp(chunk->data->blocks, blocks, MC_CHUNK_BLOCKS) == 0 &&
           world->active_calls == active_calls + (mutation ? 1u : 0u) &&
           world->local_leases == leases && world->acquisition_error == outer &&
           world->lifecycle_error == lifecycle);
    uint64_t after_gaussian = 0;
    if (math != NULL)
        memcpy(&after_gaussian, &math->next_next_gaussian, sizeof(after_gaussian));
    VERIFY(after_gaussian == math_gaussian && world->acquisition.cache == cache &&
           world->acquisition.pending == pending &&
           memcmp(cache, cache_descriptor, sizeof(cache_descriptor)) == 0 &&
           memcmp(pending, pending_descriptor, sizeof(pending_descriptor)) == 0 &&
           (entries_bytes == 0 || memcmp(cache->entries, backing, entries_bytes) == 0) &&
           (insertions_bytes == 0 ||
            memcmp(cache->insertions, backing + entries_bytes, insertions_bytes) == 0) &&
           (pending_bytes == 0 || memcmp(pending->keys, backing + entries_bytes + insertions_bytes,
                                         pending_bytes) == 0) &&
           memcmp(&world->last_error, storage_diagnostic, sizeof(storage_diagnostic)) == 0 &&
           memcmp(&world->last_registry_error, registry_diagnostic, sizeof(registry_diagnostic)) ==
               0 &&
           world->last_random_status == random_status && mc_chunk_cache_validate(cache) &&
           mc_chunk_pending_validate(pending));
    if (pending->history != NULL)
        VERIFY(mc_java_int_set_copy_order(pending->history, after_order, COUNT(after_order),
                                          &after_count) == MC_JAVA_INT_SET_OK &&
               after_count == pending_count &&
               memcmp(pending_order, after_order, pending_count * sizeof(*pending_order)) == 0);
    printf("CACHE_QUERY_INVARIANTS owned=1 raw-exports=0 same-chunk=1 same-arrays=1 "
           "same-world-fields=1 same-random=1 same-cache-layout=1 same-pending-membership=1 "
           "same-diagnostics=1 same-holds=1 calls=%u sink=%" PRIu64 "\n",
           (unsigned int)BENCH_MODES * BENCH_SAMPLES * BENCH_CALLS, last_sink);
    if (mutation) {
        mc_chunk_provider_cache_mutation_end(world->fields.v);
        VERIFY(mc_chunk_provider_cache_observe(world->fields.v, &after) && after.owned &&
               after.raw_exports == 0 && after.mutation_depth == 0 &&
               after.rebuilds == before.rebuilds + 1);
        mc_world_runtime_call_leave(world);
        VERIFY(world->active_calls == active_calls);
    }
    free(backing);
    free(blocks);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 4);
    const bool mutation = strcmp(argv[2], "mutation") == 0;
    const unsigned int chunk_count = strcmp(argv[3], "289") == 0   ? 289u
                                     : strcmp(argv[3], "410") == 0 ? 410u
                                     : strcmp(argv[3], "512") == 0 ? 512u
                                                                   : 1u;
    VERIFY((mutation || strcmp(argv[2], "indexed") == 0) &&
           (chunk_count != 1 || strcmp(argv[3], "1") == 0));
#if defined(__clang__)
    printf("CACHE_QUERY_CONTEXT compiler=clang major=%d minor=%d patch=%d ", __clang_major__,
           __clang_minor__, __clang_patchlevel__);
#elif defined(_MSC_VER)
    printf("CACHE_QUERY_CONTEXT compiler=msvc version=%d ", _MSC_FULL_VER);
#elif defined(__GNUC__)
    printf("CACHE_QUERY_CONTEXT compiler=gcc major=%d minor=%d patch=%d ", __GNUC__, __GNUC_MINOR__,
           __GNUC_PATCHLEVEL__);
#else
    printf("CACHE_QUERY_CONTEXT compiler=unknown ");
#endif
#ifdef NDEBUG
    printf("ndebug=1 ");
#else
    printf("ndebug=0 ");
#endif
    printf("pointer-bits=%zu scope=%s chunks=%u\n", sizeof(void *) * CHAR_BIT, argv[2],
           chunk_count);
    source_save_fixture *source = calloc(1, sizeof(*source));
    VERIFY(source != NULL && ss_setup(source, argv[1]) == 0 && bench_open(source) == 0);
    VERIFY(bench_run(source, mutation, chunk_count) == 0 && bench_stop(source) == 0 &&
           ss_finish(source) == 0);
    printf("CACHE_QUERY_FINISH checks=%u cleanup=1 genuine-source-world=1\n", checks);
    free(source);
    return 0;
}
