/* Fixed genuine GS ally-alert cleanup work. Only the actual checked retire
 * call is timed; constructors, insertion, query/body, controls and observations
 * remain outside the interval. No source method or object is substituted. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "player_ally_retire_adapter.h"
#define main mc_ally_retire_unused_statistics_main
#define MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS 16
#include "../client/statistics/test_player_dispatch.c"
#undef MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS
#undef main
#include "../../src/entity/mob/internal.h"
#include "../../src/world/runtime/runtime_internal.h"
#include "mc/entity/player/ally_alert.h"
#include "mc/world/runtime/entities.h"

#include <inttypes.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

enum { ALLY_MAX_WOLVES = 1024, ALLY_WARMUPS = 2, ALLY_SAMPLES = 21 };

static bool ally_clock(int64_t *out) {
#if defined(_WIN32)
    LARGE_INTEGER value;
    if (!QueryPerformanceCounter(&value) || value.QuadPart < 0)
        return false;
    *out = value.QuadPart;
#else
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0 || value.tv_sec < 0 || value.tv_nsec < 0 ||
        value.tv_nsec >= 1000000000L ||
        (uint64_t)value.tv_sec > (uint64_t)INT64_MAX / UINT64_C(1000000000) ||
        ((uint64_t)value.tv_sec == (uint64_t)INT64_MAX / UINT64_C(1000000000) &&
         (uint64_t)value.tv_nsec > (uint64_t)INT64_MAX % UINT64_C(1000000000)))
        return false;
    *out = (int64_t)value.tv_sec * INT64_C(1000000000) + (int64_t)value.tv_nsec;
#endif
    return true;
}

static bool ally_frequency(int64_t *out) {
#if defined(_WIN32)
    LARGE_INTEGER value;
    if (!QueryPerformanceFrequency(&value) || value.QuadPart <= 0)
        return false;
    *out = value.QuadPart;
#else
    *out = INT64_C(1000000000);
#endif
    return true;
}

static uint64_t ally_hash(uint64_t hash, const void *storage, size_t count) {
    const unsigned char *bytes = storage;
    for (size_t i = 0; i < count; ++i)
        hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    return hash;
}

static uint64_t ally_source(const fixture *f, mc_entity *const *wolves, size_t count) {
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = ally_hash(hash, &f->world->fields, sizeof(f->world->fields));
    hash = ally_hash(hash, f->world->fields.r, sizeof(*f->world->fields.r));
    hash = ally_hash(hash, &f->player->base, sizeof(f->player->base));
    hash = ally_hash(hash, f->player->random, sizeof(*f->player->random));
    for (size_t i = 0; i < count; ++i) {
        const mc_entity *entity = wolves[i];
        hash = ally_hash(hash, &entity->base, sizeof(entity->base));
        hash = ally_hash(hash, entity->random, sizeof(*entity->random));
        hash = ally_hash(hash, entity->bounds, sizeof(*entity->bounds));
        hash = ally_hash(hash, entity->mob, sizeof(*entity->mob));
        hash = ally_hash(hash, &entity->watcher->dirty, sizeof(entity->watcher->dirty));
        for (const mc_entity_watch_node *n = entity->watcher->map->head; n != NULL; n = n->next)
            hash = ally_hash(hash, n->record, sizeof(*n->record));
    }
    return hash;
}

static bool ally_once(fixture *f, mc_entity **wolves, size_t distinct, size_t occurrences,
                      bool duplicates, unsigned int sample, bool control, FILE *out) {
    mc_player_ally_alert_call *call = NULL;
    mc_player_ally_alert_state state;
    mc_player_ally_alert_cleanup cleanup;
    size_t holds[ALLY_MAX_WOLVES];
    const size_t player_holds = f->player->holds, leases = f->world->local_leases;
    const size_t bindings = f->world->persistent_bindings;
    const size_t bytes = f->world->construction.direct_allocation_bytes;
    const size_t bounds_holds = f->app.entities->bounds_holds;
    const int32_t next_id = f->app.entities->next_id;
    for (size_t i = 0; i < distinct; ++i)
        holds[i] = wolves[i]->holds;
    CHECK(mc_player_ally_alert_begin(f->player, wolves[0], false, &call) == MC_ENTITY_OK &&
          call != NULL && mc_player_ally_alert_inspect(call, &state) && state.returned &&
          state.source_pc == 214 && state.wolves_examined == occurrences &&
          state.wolves_alerted == 0 && state.target == wolves[0] && state.wolves != NULL);
    size_t count = 0;
    CHECK(mc_world_class_entity_list_count(state.wolves, &count) == MC_WORLD_STORAGE_OK &&
          count == occurrences);
    for (size_t i = 0; i < occurrences; ++i) {
        mc_entity *actual = NULL;
        CHECK(mc_world_class_entity_list_get(state.wolves, i, &actual) == MC_WORLD_STORAGE_OK &&
              actual == wolves[duplicates ? 0 : i]);
    }
    const uint64_t source = ally_source(f, wolves, distinct);
    const size_t allocation_before = mc_benchmark_ally_retire_failed_allocations();
    if (control) {
        const size_t saved = wolves[0]->holds;
        /* Actual list + target alias need N+1 holds (2 for unique). This is
         * deliberately corrupted native metadata, never a source input. */
        wolves[0]->holds = duplicates ? occurrences : 1;
        const mc_entity_status refused = mc_player_ally_alert_retire(&call, &cleanup);
        wolves[0]->holds = saved;
        CHECK(refused == MC_ENTITY_STATE_ERROR && call != NULL &&
              mc_player_ally_alert_inspect(call, &state) && state.returned &&
              state.error->status == MC_ENTITY_OK && state.source_pc == 214 &&
              ally_source(f, wolves, distinct) == source);
    }
    int64_t begin, end;
    if (!ally_clock(&begin))
        return false;
    const mc_entity_status retired = mc_player_ally_alert_retire(&call, &cleanup);
    const bool ended = ally_clock(&end);
    CHECK(ended);
    CHECK(retired == MC_ENTITY_OK && call == NULL && cleanup.status == MC_ENTITY_OK &&
          end >= begin && f->player->holds == player_holds && f->world->local_leases == leases &&
          f->world->persistent_bindings == bindings &&
          f->world->construction.direct_allocation_bytes == bytes &&
          f->app.entities->bounds_holds == bounds_holds && f->app.entities->next_id == next_id &&
          ally_source(f, wolves, distinct) == source);
    const size_t allocation_delta =
        mc_benchmark_ally_retire_failed_allocations() - allocation_before;
    const size_t expected_allocations =
        !strcmp(mc_benchmark_ally_retire_adapter(), "fallback") && occurrences > 8
            ? (control ? 2u : 1u)
            : 0u;
    CHECK(allocation_delta == expected_allocations);
    for (size_t i = 0; i < distinct; ++i)
        CHECK(wolves[i]->holds == holds[i]);
    if (out != NULL)
        CHECK(fprintf(out, "RETIRE\t%s\t%zu\t%u\t%d\t%" PRId64 "\t%zu\t%zu\t%zu\n",
                      duplicates ? "duplicate" : "unique", occurrences, sample,
                      !control && sample >= ALLY_WARMUPS, end - begin, distinct,
                      occurrences * (occurrences + 1), allocation_delta) > 0);
    return true;
}

static bool ally_workload(const char *resources, size_t count, bool duplicates, FILE *out) {
    fixture *f = calloc(1, sizeof(*f));
    mc_entity **wolves = calloc(ALLY_MAX_WOLVES, sizeof(*wolves));
    CHECK(f != NULL && wolves != NULL);
    char directory[64];
    CHECK(snprintf(directory, sizeof(directory), "%s-%zu", duplicates ? "duplicate" : "unique",
                   count) > 0 &&
          setup(f, resources, directory, true));
    CHECK(mc_entity_set_position(f->player, 8.5, 64.5, 8.5) == MC_ENTITY_OK);
    mc_chunk_state *chunk = NULL;
    CHECK(mc_world_runtime_get(f->world, 0, 0, &chunk) == MC_CHUNK_ACQUIRE_OK && chunk != NULL);
    const mc_entity_class_literal *wolf_class = NULL;
    CHECK(mc_entity_class_literal_get(f->app.entities, MC_ENTITY_CLASS_KIND_WOLF, &wolf_class) ==
          MC_ENTITY_OK);
    const size_t distinct = duplicates ? 1 : count;
    for (size_t i = 0; i < distinct; ++i) {
        mc_mob_error error;
        CHECK(mc_mob_allocate(f->app.entities, f->app.registry, wolf_class, &wolves[i], &error) ==
                  MC_MOB_OK &&
              mc_mob_construct(wolves[i], f->world, &error) == MC_MOB_OK &&
              mc_entity_set_position(wolves[i], 8.5, 64.5, 8.5) == MC_ENTITY_OK);
    }
    for (size_t i = 0; i < count; ++i) {
        bool inserted = false;
        CHECK(mc_world_runtime_insert_entity(f->world, wolves[duplicates ? 0 : i], &inserted,
                                             NULL) == MC_WORLD_STORAGE_OK &&
              inserted);
    }
    CHECK(ally_once(f, wolves, distinct, count, duplicates, 0, true, NULL));
    for (unsigned int i = 0; i < ALLY_WARMUPS + ALLY_SAMPLES; ++i)
        CHECK(ally_once(f, wolves, distinct, count, duplicates, i, false, out));
    for (size_t i = 0; i < distinct; ++i)
        mc_entity_release(wolves[i]);
    /* The imported statistics fixture has no loaded Chunk. Our genuine query
     * adds one, so end native containers/cache/Chunk owners before entering
     * its existing remaining teardown. No source unload/save is replayed. */
    mc_chunk_cache *cache = f->world->options.cache;
    mc_chunk_state *states[16];
    const size_t state_count = cache->insertion_count;
    CHECK(state_count <= sizeof(states) / sizeof(*states));
    memcpy(states, cache->insertions, state_count * sizeof(*states));
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_pending_clear(f->world->options.pending) == MC_CHUNK_ACQUIRE_OK &&
          mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                              cache->insertion_capacity) &&
          mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < state_count; ++i) {
        bool prior = false;
        for (size_t j = 0; j < i; ++j)
            prior = prior || states[j] == states[i];
        if (!prior)
            CHECK(mc_world_store_retire_chunk(f->store, states[i], NULL) == MC_WORLD_STORAGE_OK);
    }
    CHECK(teardown(f));
    free(wolves);
    free(f);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    FILE *out = fopen(argv[3], "wx");
    int64_t frequency;
    if (out == NULL || !ally_frequency(&frequency))
        return 1;
    bool ok =
        fprintf(out, "ALLY_RETIRE_V2\t%s\t%" PRId64 "\t%d\t%d\n",
                mc_benchmark_ally_retire_adapter(), frequency, ALLY_WARMUPS, ALLY_SAMPLES) > 0;
    const size_t counts[] = {1, 8, 64, 256, 1024};
    for (size_t mode = 0; ok && mode < 2; ++mode)
        for (size_t i = 0; ok && i < sizeof(counts) / sizeof(*counts); ++i)
            ok = ally_workload(argv[1], counts[i], mode != 0, out);
    ok = fclose(out) == 0 && ok;
    if (!ok)
        return 1;
    printf("Ally retire: %zu checks; 10 actual Wolf workloads; 210 measured retirements\n", checks);
    return 0;
}
