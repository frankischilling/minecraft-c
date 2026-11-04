/* Optional fixed-work genuine World random-Chunk CPU workload.
 * Loading, scalar oracle, inspection, outer retirement and output are untimed.
 * Portable clock/control entry projection; captured Windows samples retain
 * their original private executable and harness bindings. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#define main fixture_contract_main
#include "../world/runtime/test_world_random_tick.c"
#undef main
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

static bool bench_frequency(int64_t *out) {
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

static bool bench_sample(int64_t *out) {
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

static bool random_once(random_fixture *f, unsigned int ordinal, bool timed, FILE *out) {
    mc_world_runtime *w = f->world;
    const size_t bindings = w->persistent_bindings;
    const uint64_t random_before = w->fields.r->state;
    const int32_t g_before = w->fields.g;
    mc_java_random reference = *w->fields.r;
    int32_t g = g_before;
    const int32_t q = w->fields.Q > 0 ? w->fields.Q - 1 : w->fields.Q;
    size_t snow = 0;
    const size_t ambient = q == 0 ? 361u : 0u;
    for (size_t chunk = 0; chunk < 361; ++chunk) {
        if (q == 0)
            g = step(g);
        int32_t ignored, draw;
        CHECK(mc_java_random_next_bounded(&reference, 100000, &ignored));
        CHECK(mc_java_random_next_bounded(&reference, 16, &draw));
        if (draw == 0) {
            ++snow;
            g = step(g);
        }
        for (size_t i = 0; i < 80; ++i)
            g = step(g);
    }
    int64_t frequency, start, stop, retirement_start, retired;
    CHECK(bench_frequency(&frequency));
    mc_world_random_tick_error admission;
    const bool clock_started = bench_sample(&start);
    const mc_world_storage_status status =
        mc_world_runtime_random_tick_begin(w, f->math, &f->call, &admission);
    const bool clock_stopped = bench_sample(&stop);
    CHECK(clock_started && clock_stopped && stop >= start);
    mc_world_random_tick_state state;
    CHECK(mc_world_runtime_random_tick_inspect(f->call, &state));
    CHECK(status == MC_WORLD_STORAGE_OK && state.returned && !state.active &&
          state.source_pc == 994 && state.world == w && state.math == f->math &&
          state.chunk_count == 361 && state.chunks_started == 361 && state.chunks_returned == 361 &&
          state.samples_started == 28880 && state.samples_returned == 28880 &&
          state.block_ticks == 0 && state.random_calls == 722 &&
          state.lcg_updates == 28880 + ambient + snow && state.ambient_attempts == ambient &&
          state.snow_attempts == snow && state.ambient_sounds == 0 &&
          state.weather_publications == 0 && state.snow_writes == 0 && state.ice_writes == 0 &&
          !state.chunk.captured && !state.chunk.failure && state.chunk.reads == 80 + (q == 0) &&
          w->fields.g == g && w->fields.r->state == reference.state && w->fields.Q == q &&
          w->local_leases == 0 && w->active_calls == 0 && w->persistent_bindings == bindings + 1 &&
          w->player_membership.count == 1 && w->entity_membership.count == 0 &&
          f->provider.cache->count == 361 && !f->callback_bad && f->sounds == 0 && f->cells == 0 &&
          f->ranges == 0);
    CHECK(bench_sample(&retirement_start));
    CHECK(mc_world_runtime_random_tick_retire(&f->call, NULL) == MC_WORLD_STORAGE_OK && !f->call);
    CHECK(bench_sample(&retired));
    CHECK(w->local_leases == 0 && w->active_calls == 0 && w->persistent_bindings == bindings);
    fprintf(out,
            "%s %u %u %.9f %.9f %08" PRIx32 " %08" PRIx32 " %012" PRIx64 " %012" PRIx64
            " %d %zu %zu %zu %zu %zu %zu %zu\n",
            f->index == 2 ? "stone_q0" : "air_qhigh", ordinal, timed ? 1u : 0u,
            (double)(stop - start) * 1000.0 / (double)frequency,
            (double)(retired - retirement_start) * 1000.0 / (double)frequency, (uint32_t)g_before,
            (uint32_t)g, random_before, reference.state, q, state.chunks_returned,
            state.samples_returned, state.random_calls, state.lcg_updates, state.ambient_attempts,
            state.snow_attempts, state.block_ticks);
    return true;
}

static bool random_run(const char *resources, const char *directory, const char *output,
                       unsigned int index) {
    random_fixture *f = calloc(1, sizeof(*f));
    CHECK(f);
    f->index = index;
#ifdef _MSC_VER
    CHECK(tmpfile_s(&f->events) == 0);
#else
    f->events = tmpfile();
#endif
    FILE *out = open_file(output, "wb");
    CHECK(f->events && out && setup(f, resources, directory) && prepare_chunks(f) && player(f) &&
          prepare_access(f));
    mc_world_runtime *w = f->world;
    w->fields.q = 1;
    w->fields.f = w->fields.m = 0;
    w->fields.i = w->fields.j = w->fields.k = w->fields.l = 0.f;
    w->fields.g = 17;
    w->fields.Q = index == 2 ? 0 : 6001;
    mc_java_random_reseed(w->fields.r, 0);
    CHECK(mc_entity_bounds_pool_reset(f->entities, NULL) == MC_ENTITY_OK &&
          mc_entity_vector_pool_reset(f->entities, NULL) == MC_ENTITY_OK);
    const mc_java_random player_random = *f->player->random;
    const uint64_t unique = atomic_load(&f->profile->uniquifier);
    for (unsigned int i = 0; i < 16; ++i)
        CHECK(random_once(f, i, i >= 2, out));
    CHECK(f->player->random->state == player_random.state &&
          atomic_load(&f->profile->uniquifier) == unique && f->owned_count == 361);
    for (size_t i = 0; i < f->owned_count; ++i) {
        mc_chunk_state *found = NULL;
        CHECK(mc_chunk_cache_lookup(f->provider.cache, f->owned[i]->x, f->owned[i]->z, &found) &&
              found == f->owned[i] && !found->blank && found->terrain_populated);
        for (size_t j = 0; j < MC_CHUNK_BLOCKS; ++j)
            CHECK(found->data->blocks[j] == (index == 2 ? 1 : 0));
    }
    CHECK(teardown(f) && fclose(f->events) == 0 && fclose(out) == 0);
    free(f);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 5 && strcmp(argv[1], "controls") == 0)
        return fixture_contract_main(argc - 1, argv + 1);
    if (argc == 4)
        return fixture_contract_main(argc, argv);
    if (argc != 5 || (strcmp(argv[4], "1") && strcmp(argv[4], "2")))
        return 2;
    return random_run(argv[1], argv[2], argv[3], (unsigned int)strtoul(argv[4], NULL, 10)) ? 0 : 1;
}
