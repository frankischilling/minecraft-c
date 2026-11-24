/* Fixed-work FD.q CPU observer. The same object drives both implementations.
 * Construction, scalar replay, snapshots, assertions, output and outer call
 * retirement are outside the timed begin operation. No source entropy is set. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include "random_display_adapter.h"

static const rd_cpu_adapter *rd_selected;
int rd_fixture_contract_main(int, char **);
#define MC_RANDOM_DISPLAY_ENTRY rd_fixture_contract_main
#define mc_world_runtime_random_display_begin (rd_selected->begin)
#define mc_world_runtime_random_display_inspect (rd_selected->inspect)
#define mc_world_runtime_random_display_retire (rd_selected->retire)
#include "../world/runtime/test_random_display.c"
#undef mc_world_runtime_random_display_begin
#undef mc_world_runtime_random_display_inspect
#undef mc_world_runtime_random_display_retire
#undef MC_RANDOM_DISPLAY_ENTRY

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

static bool rd_clock(int64_t *out) {
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

static bool rd_frequency(int64_t *out) {
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

static uint64_t rd_hash(uint64_t value, const void *storage, size_t count) {
    const unsigned char *bytes = storage;
    for (size_t i = 0; i < count; ++i)
        value = (value ^ bytes[i]) * UINT64_C(1099511628211);
    return value;
}

static uint64_t rd_planes(const display_fixture *f) {
    uint64_t value = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < f->base.owned_count; ++i) {
        const mc_chunk_state *chunk = f->base.owned[i];
        const mc_chunk_data *data = chunk->data;
        value = rd_hash(value, chunk, sizeof(*chunk));
        value = rd_hash(value, data, sizeof(*data));
        value = rd_hash(value, data->blocks, MC_CHUNK_BLOCKS);
        value = rd_hash(value, data->metadata, MC_CHUNK_NIBBLE_BYTES);
        value = rd_hash(value, data->skylight, MC_CHUNK_NIBBLE_BYTES);
        value = rd_hash(value, data->block_light, MC_CHUNK_NIBBLE_BYTES);
        value = rd_hash(value, data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES);
    }
    return value;
}

static bool rd_once(display_fixture *f, unsigned int ordinal, bool timed, FILE *out) {
    mc_world_runtime *w = f->base.world;
    const size_t bindings = w->persistent_bindings;
    mc_world_runtime_fields fields;
    memcpy(&fields, &w->fields, sizeof(fields));
    const uint64_t planes = rd_planes(f);
    const uint64_t unique = atomic_load(&f->base.profile->uniquifier);
    const unsigned int holder = atomic_load(&f->base.profile->holder_phase);
    const uint64_t math = f->base.profile->math ? atomic_load(&f->base.profile->math->state) : 0;
    mc_chunk_provider_cache_observation before, after;
    CHECK(mc_chunk_provider_cache_observe(w->fields.v, &before) && !before.owned);
    mc_java_random expected = *w->fields.r;
    const uint64_t random_before = expected.state;
    int32_t last_draws[6] = {0};
    for (size_t sample = 0; sample < 1000; ++sample)
        for (size_t draw_index = 0; draw_index < 6; ++draw_index)
            CHECK(mc_java_random_next_bounded(&expected, 16, &last_draws[draw_index]));
    int64_t frequency, start, stop;
    CHECK(rd_frequency(&frequency));
    mc_world_random_display_error admission;
    const bool started = rd_clock(&start);
    const mc_world_storage_status status =
        rd_selected->begin(w, f->input.x, f->input.y, f->input.z, &f->sampler, &admission);
    const bool stopped = rd_clock(&stop);
    CHECK(started && stopped && stop >= start);
    mc_world_random_display_state state;
    CHECK(rd_selected->inspect(f->sampler, &state));
    CHECK(status == MC_WORLD_STORAGE_OK && admission.status == MC_WORLD_STORAGE_OK &&
          state.returned && !state.active && state.source_pc == 134 && state.sample == 1000 &&
          state.world == w && state.samples_started == 1000 && state.samples_returned == 1000 &&
          state.world_random_calls == 6000 && state.block_read &&
          state.block_id == f->input.block && state.block_selected == (f->input.block != 0) &&
          state.display_calls_started == (f->input.block ? 1000u : 0u) &&
          state.display_calls_returned == state.display_calls_started &&
          state.x == f->input.x + last_draws[0] - last_draws[1] &&
          state.y == f->input.y + last_draws[2] - last_draws[3] &&
          state.z == f->input.z + last_draws[4] - last_draws[5] &&
          !memcmp(state.draws, last_draws, sizeof(last_draws)) && state.coordinates_stored[0] &&
          state.coordinates_stored[1] && state.coordinates_stored[2] &&
          state.local_random_allocated && state.local_random_constructed && state.source_random &&
          state.source_random != w->fields.r &&
          mc_test_java_random_equal(&state.local_random_initial, &state.local_random) &&
          mc_test_java_random_equal(w->fields.r, &expected) && !state.error->status &&
          !state.error->operation && !state.error->acquisition && !state.error->registry &&
          !state.error->block && !state.error->world_access.receipt);
    CHECK(!memcmp(&fields, &w->fields, sizeof(fields)) && planes == rd_planes(f) &&
          w->active_calls == 0 && w->local_leases == 0 && w->persistent_bindings == bindings + 1 &&
          atomic_load(&f->base.profile->uniquifier) == unique * UINT64_C(1181783497276652981) &&
          atomic_load(&f->base.profile->holder_phase) == holder &&
          (!f->base.profile->math || atomic_load(&f->base.profile->math->state) == math) &&
          !f->base.profile->constructor_inputs_bound && !f->callback_bad && !f->particles &&
          !f->sounds && !f->events && controls_intact(f));
    CHECK(mc_chunk_provider_cache_observe(w->fields.v, &after) && !after.owned &&
          after.generic_gets == before.generic_gets + 1000 && after.hits == before.hits &&
          after.misses == before.misses && after.raw_exports == before.raw_exports &&
          after.rebuilds == before.rebuilds && after.map_count == before.map_count &&
          after.insertion_count == before.insertion_count &&
          after.direct_bytes == before.direct_bytes && after.index_bytes == before.index_bytes &&
          after.index_capacity == before.index_capacity &&
          after.mutation_depth == before.mutation_depth);
    CHECK(rd_selected->retire(&f->sampler, NULL) == MC_WORLD_STORAGE_OK && !f->sampler &&
          w->persistent_bindings == bindings && w->active_calls == 0 && w->local_leases == 0 &&
          controls_intact(f));
    fprintf(out,
            "%s %u %u %" PRId64 " %" PRId64 " %012" PRIx64 " %012" PRIx64
            " %zu %zu %zu %zu %zu %d %d %d\n",
            f->input.block ? "stone" : "air", ordinal, timed ? 1u : 0u, stop - start, frequency,
            random_before, expected.state, state.samples_started, state.samples_returned,
            state.world_random_calls, state.display_calls_started, state.display_calls_returned,
            state.x, state.y, state.z);
    return true;
}

static bool rd_run(const char *resources, const char *worlds, const char *output) {
    FILE *out = open_file(output, "wb");
    CHECK(out != NULL);
    for (int32_t block = 0; block <= 1; ++block) {
        display_fixture *f = calloc(1, sizeof(*f));
        CHECK(f != NULL);
        f->sampler_mode = true;
        f->input = (display_input){.block = block, .x = 0, .y = 64, .z = 0};
        memcpy(f->input.scene, "normal", sizeof("normal"));
        memcpy(f->input.callback, "none", sizeof("none"));
        f->out = out;
        char directory[8192];
        const int length =
            snprintf(directory, sizeof(directory), "%s/%s", worlds, block ? "stone" : "air");
        CHECK(length > 0 && (size_t)length < sizeof(directory) &&
              prepare_world(f, resources, directory) && attach_observer(f) && observe_data(f));
        CHECK(f->base.owned_count == 9 && f->base.provider.cache->count == 9);
        for (unsigned int ordinal = 0; ordinal < 23; ++ordinal)
            CHECK(rd_once(f, ordinal, ordinal >= 2, out));
        CHECK(data_unchanged(f) && display_teardown(f));
        free(f);
    }
    CHECK(fclose(out) == 0);
    printf("Random display CPU %s: %u checks;42 measured calls;42000 samples;252000 World draws\n",
           rd_selected->name, checks);
    return true;
}

int main(int argc, char **argv) {
    rd_selected = rd_cpu_adapter_get();
    if (argc == 6)
        return rd_fixture_contract_main(argc, argv);
    if (argc == 7 && !strcmp(argv[1], "controls"))
        return rd_fixture_contract_main(argc - 1, argv + 1);
    if (argc == 5 && !strcmp(argv[1], "timing"))
        return rd_run(argv[2], argv[3], argv[4]) ? 0 : 1;
    return 2;
}
