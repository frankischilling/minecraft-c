/* Fixed genuine FD.g CPU work. Every interval has a fresh actual World and
 * canonical published Mobs. Only sixteen calls of the public World body are
 * timed; constructors, literal workload inputs, checks, hashes and retirement
 * remain outside. The four executables share THIS exact workload object. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#define MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS 64
#define main mc_mob_diagnostics_unused_statistics_main
#include "../client/statistics/test_player_dispatch.c"
#undef main
#undef MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS
#include "mob_tick_diagnostics_adapter.h"
#include "mob_tick_diagnostics_support.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

static uint64_t mb_dispatch_calls, mb_workspace_calls;

void mc_benchmark_mob_tick_observe_dispatch(bool reserved) {
    ++mb_dispatch_calls;
    mb_workspace_calls += reserved ? 1u : 0u;
}

static bool mb_clock(int64_t *out) {
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

static bool mb_frequency(int64_t *out) {
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

static bool mb_ground(const mb_fixture *f) {
    bool valid = true;
    for (size_t i = 0; i < MB_CHUNKS; ++i) {
        const mc_chunk_state *state = f->chunks[i];
        CHECK(state != NULL && state->data != NULL && state->data->blocks != NULL &&
              state->active && !state->blank && state->terrain_populated);
        const mc_chunk_data *data = state->data;
        for (size_t cell = 0; cell < MC_CHUNK_BLOCKS; ++cell) {
            const unsigned int expected = cell % 128u == 63u ? 1u : 0u;
            if (valid && data->blocks[cell] != expected)
                fprintf(stderr,
                        "Mob ground first difference: chunk=%" PRId32 ",%" PRId32
                        " field=Blocks index=%zu expected=%u actual=%u\n",
                        state->x, state->z, cell, expected, (unsigned int)data->blocks[cell]);
            valid = valid && data->blocks[cell] == expected;
        }
        for (size_t cell = 0; cell < MC_CHUNK_NIBBLE_BYTES; ++cell) {
            if (valid && (data->metadata[cell] != 0 || data->skylight[cell] != 0 ||
                          data->block_light[cell] != 0))
                fprintf(stderr,
                        "Mob ground first difference: chunk=%" PRId32 ",%" PRId32
                        " index=%zu Data=%u SkyLight=%u BlockLight=%u expected=0,0,0\n",
                        state->x, state->z, cell, (unsigned int)data->metadata[cell],
                        (unsigned int)data->skylight[cell], (unsigned int)data->block_light[cell]);
            valid = valid && data->metadata[cell] == 0 && data->skylight[cell] == 0 &&
                    data->block_light[cell] == 0;
        }
        for (size_t cell = 0; cell < MC_CHUNK_HEIGHTMAP_BYTES; ++cell) {
            if (valid && data->heightmap[cell] != 64)
                fprintf(stderr,
                        "Mob ground first difference: chunk=%" PRId32 ",%" PRId32
                        " field=HeightMap index=%zu expected=64 actual=%u\n",
                        state->x, state->z, cell, (unsigned int)data->heightmap[cell]);
            valid = valid && data->heightmap[cell] == 64;
        }
        /* Valid stored HeightMap/SkyLight bypass TO's rebuild at142..194.
         * LM's constructor leaves its nonserialized i at the primitive zero
         * default; copying the serialized h bytes64 does not derive i64.
         * The later LD272 -> LM.i only normalizes block IDs through VI.a. */
        if (valid && data->minimum_height != 0)
            fprintf(stderr,
                    "Mob ground first difference: chunk=%" PRId32 ",%" PRId32
                    " field=minimum_height expected=0 actual=%" PRId32 "\n",
                    state->x, state->z, data->minimum_height);
        valid = valid && data->minimum_height == 0;
    }
    CHECK(valid);
    return true;
}

static bool mb_setup(mb_fixture *f, const char *resources, const char *cell, size_t count,
                     mc_entity_class_kind kind, unsigned int sample) {
    char directory[80];
    CHECK(snprintf(directory, sizeof(directory), "scenes/%s/sample-%u", cell, sample) > 0 &&
          setup(&f->base, resources, directory, false));
    f->count = count;
    /* Passive access to the already-bound actual owner does not export its
     * provider descriptors or discard its private source-query index. */
    f->cache = f->base.world->options.cache;
    f->pending = f->base.world->options.pending;
    CHECK(f->cache != NULL && f->pending != NULL);
    for (int32_t x = -2; x <= 2; ++x)
        for (int32_t z = -2; z <= 2; ++z) {
            const size_t index = (size_t)(x + 2) * 5u + (size_t)(z + 2);
            CHECK(mc_world_runtime_get(f->base.world, x, z, &f->chunks[index]) ==
                      MC_CHUNK_ACQUIRE_OK &&
                  f->chunks[index] != NULL && !f->chunks[index]->blank && f->chunks[index]->active);
        }
    CHECK(mb_ground(f) && mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(f->math, NULL) == MC_MATH_CLASS_OK &&
          mc_world_info_time_set(f->base.world->fields.x, 13000) == MC_WORLD_INFO_OK &&
          mc_entity_source_position_rotation(f->base.player, 8.5, 64, 8.5, 0, 0, NULL) ==
              MC_ENTITY_OK);
    mc_world_runtime *world = f->base.world;
    world->fields.f = 4;
    world->fields.q = 2;
    world->fields.g = 17;
    world->fields.Q = 6001;
    /* Explicit workload timestamp, independent of the constructor's genuine
     * System.currentTimeMillis return. Keep I in the exact paired projection;
     * no clock-derived field is silently excluded from comparison. */
    world->fields.I = INT64_C(1234567890000) + sample;
    CHECK(!world->fields.B && world->fields.d->count == 0);
    const mc_entity_class_literal *literal = NULL;
    if (count != 0)
        CHECK(mc_entity_class_literal_get(f->base.app.entities, kind, &literal) == MC_ENTITY_OK);
    for (size_t i = 0; i < count; ++i) {
        mc_mob_error error;
        mc_entity *p = NULL;
        CHECK(mc_mob_allocate(f->base.app.entities, f->base.app.registry, literal, &p, &error) ==
                  MC_MOB_OK &&
              mc_mob_construct(p, world, &error) == MC_MOB_OK && p != NULL && p->mob != NULL);
        f->actors[i] = p;
        const float F = p->mob->living.F, G = p->mob->living.G, aj = p->mob->living.aj;
        /* Independent declared workload values; never inferred from a source
         * observation, constructor nanotime, desired branch or later seed. */
        const double x = 5.0 + (double)(i % 8u), z = 5.0 + (double)(i / 8u);
        CHECK(mc_entity_source_position_rotation(p, x, 64, z, 0, 0, NULL) == MC_ENTITY_OK);
        const int64_t seed = INT64_C(0x1234abc000) + (int64_t)sample * 65537 +
                             (int64_t)count * 257 + (int64_t)kind * 17 + (int64_t)i;
        mc_java_random_reseed(p->random, seed);
        CHECK(mb_float(F) == mb_float(p->mob->living.F) &&
              mb_float(G) == mb_float(p->mob->living.G) &&
              mb_float(aj) == mb_float(p->mob->living.aj));
        bool inserted = false;
        CHECK(mc_world_runtime_insert_entity(world, p, &inserted, NULL) == MC_WORLD_STORAGE_OK &&
              inserted && p->base.bF && p->mob->living.Y > 0);
    }
    mc_java_random_reseed(world->fields.r, INT64_C(0x334455000) + sample);
    mc_java_random_reseed(f->base.player->random, INT64_C(0x434455000) + sample);
    CHECK(f->base.app.profile->math != NULL && !f->base.app.profile->constructor_inputs_bound);
    mc_java_random shared;
    mc_java_random_seed(&shared, INT64_C(0x534455000) + sample);
    /* A declared private workload state, after all genuine constructors.
     * The real shared Math owner is used by every reached source call. */
    atomic_store(&f->base.app.profile->math->state, shared.state);
    f->base.app.profile->math->next_next_gaussian = 0.0;
    f->base.app.profile->math->have_next_next_gaussian = false;
    f->context.player.travel.code_registry = f->base.app.registry;
    f->context.player.travel.trig = mc_math_classes_table(f->math);
    f->context.player.travel.math = f->math;
    f->context.math = f->math;
    CHECK(world->fields.b->count == count && world->fields.d->count == 0 &&
          world->player_membership.count == 0 && f->base.player->holds == 1 &&
          !f->base.player->base.bF);
    return true;
}

static bool mb_cleanup(mb_fixture *f) {
    for (size_t i = 0; i < f->count; ++i) {
        CHECK(f->actors[i] != NULL && f->actors[i]->holds > 1);
        mc_entity_release(f->actors[i]);
        f->actors[i] = NULL;
    }
    mc_chunk_state *retiring[64];
    const size_t count = f->cache->insertion_count;
    CHECK(count <= sizeof(retiring) / sizeof(*retiring));
    if (count != 0)
        memcpy(retiring, f->cache->insertions, count * sizeof(*retiring));
    mc_world_storage_error error = {0};
    const mc_world_storage_status status =
        mc_world_runtime_retire_entity_containers(f->base.world, &error);
    if (status != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "Mob diagnostics terminal: status=%d operation=%s\n", (int)status,
                error.operation == NULL ? "none" : error.operation);
    CHECK(status == MC_WORLD_STORAGE_OK &&
          mc_chunk_pending_clear(f->pending) == MC_CHUNK_ACQUIRE_OK &&
          mc_chunk_cache_init(f->cache, f->cache->entries, f->cache->capacity, f->cache->insertions,
                              f->cache->insertion_capacity) &&
          mc_world_runtime_retire_world(f->base.world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < count; ++i) {
        bool duplicate = false;
        for (size_t j = 0; j < i; ++j)
            duplicate = duplicate || retiring[j] == retiring[i];
        if (!duplicate)
            CHECK(mc_world_store_retire_chunk(f->base.store, retiring[i], NULL) ==
                  MC_WORLD_STORAGE_OK);
    }
    CHECK(mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK && teardown(&f->base));
    return true;
}

static bool mb_tick_array(FILE *out, const int32_t *values, size_t count) {
    CHECK(fputc('[', out) != EOF);
    for (size_t i = 0; i < count; ++i)
        CHECK(fprintf(out, "%s%" PRId32, i == 0 ? "" : ",", values[i]) > 0);
    CHECK(fputc(']', out) != EOF);
    return true;
}

static bool mb_once(const char *resources, const char *cell, const char *kind_name, size_t count,
                    mc_entity_class_kind kind, unsigned int sample, FILE *out) {
    mb_fixture *f = calloc(1, sizeof(*f));
    mc_world_entity_tick_error *errors = calloc(MB_TICKS, sizeof(*errors));
    mb_hasher h = {.capacity = 262144};
    h.ranks = calloc(h.capacity, sizeof(*h.ranks));
    CHECK(f != NULL && errors != NULL && h.ranks != NULL &&
          mb_setup(f, resources, cell, count, kind, sample));
    for (size_t i = 0; i < MB_TICKS; ++i)
        mc_world_entity_tick_error_init(&errors[i]);
    mb_guard before, after;
    CHECK(mb_snapshot(f, &h, &before));
    mb_dispatch_calls = 0;
    mb_workspace_calls = 0;
    int64_t begin, end;
    CHECK(mb_clock(&begin));
    unsigned int returned = 0;
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    for (size_t tick = 0; tick < MB_TICKS; ++tick) {
        status = mc_world_runtime_tick_entities(f->base.world, &f->context, NULL, &errors[tick]);
        if (status != MC_WORLD_STORAGE_OK)
            break;
        ++returned;
    }
    const bool ended = mb_clock(&end);
    /* No digest, source read, output or fixture operation is timed. */
    CHECK(ended && end >= begin);
    if (status != MC_WORLD_STORAGE_OK) {
        const mc_world_entity_tick_error *error = &errors[returned];
        fprintf(stderr, "Mob diagnostics FD.g: cell=%s sample=%u return=%u status=%d pc=%u op=%s\n",
                cell, sample, returned, (int)status, error->bytecode,
                error->operation == NULL ? "none" : error->operation);
    }
    CHECK(status == MC_WORLD_STORAGE_OK && returned == MB_TICKS);
    const uint64_t expected_dispatch = (uint64_t)count * MB_TICKS;
    const uint64_t expected_workspace =
        mc_benchmark_mob_tick_legacy_dispatch() ? 0 : expected_dispatch;
    CHECK(mb_dispatch_calls == expected_dispatch && mb_workspace_calls == expected_workspace);
    uint64_t hurt_reached = 0, hurt_returned = 0, hurt_true = 0;
    for (size_t tick = 0; tick < MB_TICKS; ++tick) {
        const mc_world_entity_tick_error *error = &errors[tick];
        CHECK(error->status == MC_WORLD_STORAGE_OK &&
              error->child == MC_WORLD_ENTITY_TICK_CHILD_NONE && error->operation == NULL &&
              !error->mob_hurt.saturated && !error->mob_hurt.last.present);
        hurt_reached += error->mob_hurt.reached_calls;
        hurt_returned += error->mob_hurt.returned_calls;
        hurt_true += error->mob_hurt.true_returns;
    }
    CHECK(hurt_reached == 0 && hurt_returned == 0 && hurt_true == 0 && mb_snapshot(f, &h, &after) &&
          mb_ground(f));
    const bool unique = before.uniquifier == after.uniquifier;
    const bool world_random =
        before.world_random.state == after.world_random.state &&
        mb_double(before.world_random.next_next_gaussian) ==
            mb_double(after.world_random.next_next_gaussian) &&
        before.world_random.have_next_next_gaussian == after.world_random.have_next_next_gaussian;
    const bool player = before.player == after.player;
    CHECK(unique && world_random && player);
    for (size_t i = 0; i < count; ++i)
        CHECK(after.ticks[i] == mc_java_i32_add(before.ticks[i], MB_TICKS));
    CHECK(fprintf(out,
                  "{\"event\":\"interval\",\"cell\":\"%s\",\"kind\":\"%s\",\"count\":%zu,"
                  "\"sample\":%u,\"warmup\":%s,\"ticks\":16,\"start\":%" PRId64 ",\"end\":%" PRId64
                  ",\"status\":\"OK\",\"returns\":16,\"no_hurt\":true,"
                  "\"hurt_reached\":0,\"hurt_returned\":0,\"hurt_true\":0,"
                  "\"dispatch_calls\":%" PRIu64 ",\"workspace_calls\":%" PRIu64
                  ",\"actor_ticks_before\":",
                  cell, kind_name, count, sample, sample < MB_WARMUPS ? "true" : "false", begin,
                  end, mb_dispatch_calls, mb_workspace_calls) > 0 &&
          mb_tick_array(out, before.ticks, count) && fputs(",\"actor_ticks_after\":", out) >= 0 &&
          mb_tick_array(out, after.ticks, count));
    CHECK(fprintf(out,
                  ",\"semantic_before\":\"%016" PRIx64 "\",\"semantic_after\":\"%016" PRIx64
                  "\",\"raw_before\":\"%016" PRIx64 "\",\"raw_after\":\"%016" PRIx64
                  "\",\"rng_before\":\"%016" PRIx64 "\",\"rng_after\":\"%016" PRIx64
                  "\",\"chunks_before\":\"%016" PRIx64 "\",\"chunks_after\":\"%016" PRIx64
                  "\",\"ownership_before\":\"%016" PRIx64 "\",\"ownership_after\":\"%016" PRIx64
                  "\",\"constructor_before\":\"%016" PRIx64 "\",\"constructor_after\":\"%016" PRIx64
                  "\",\"constructor_unchanged\":%s,\"uniquifier_unchanged\":true,"
                  "\"world_rng_unchanged\":true,\"player_unchanged\":true,\"checks\":%zu}\n",
                  before.semantic, after.semantic, before.raw, after.raw, before.rng, after.rng,
                  before.chunks, after.chunks, before.ownership, after.ownership,
                  before.constructor, after.constructor,
                  before.constructor == after.constructor ? "true" : "false", checks) > 0 &&
          fflush(out) == 0);
    for (size_t tick = 0; tick < MB_TICKS; ++tick)
        CHECK(mc_world_entity_tick_error_retire(&errors[tick]));
    CHECK(mb_cleanup(f));
    free(h.ranks);
    free(errors);
    free(f);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    FILE *out = fopen(argv[3], "wx");
    int64_t frequency;
    if (out == NULL || !mb_frequency(&frequency))
        return 1;
    const bool old_tail = mc_benchmark_mob_tick_legacy_tail();
    const bool old_dispatch = mc_benchmark_mob_tick_legacy_dispatch();
    const char *variant =
        old_tail ? (old_dispatch ? "legacy" : "workspace") : (old_dispatch ? "tail" : "both");
#if defined(_WIN32)
    const char *clock_name = "qpc";
#else
    const char *clock_name = "monotonic";
#endif
    bool ok =
        fprintf(out,
                "{\"event\":\"header\",\"schema\":1,\"variant\":\"%s\",\"clock\":\"%s\","
                "\"frequency\":%" PRId64 ",\"ticks_per_interval\":16,\"warmups\":2,"
                "\"samples\":21,\"fixture_chunks\":25,\"digest_policy\":\"typed-fnv1a64-le-v1\","
                "\"entropy_exclusions\":[\"living.F\",\"living.G\",\"living.aj\"],"
                "\"world_finalization\":\"%s\",\"mob_dispatch\":\"%s\",\"sizeof\":{"
                "\"world_error\":%zu,\"mob_error\":%zu,\"effect_error\":%zu,"
                "\"environment_error\":%zu,\"action_error\":%zu,\"world_scope\":%zu,"
                "\"mob_scope\":%zu,\"mob_child_workspace\":%zu}}\n",
                variant, clock_name, frequency, old_tail ? "legacy" : "candidate",
                old_dispatch ? "legacy" : "candidate", sizeof(mc_world_entity_tick_error),
                sizeof(mc_mob_tick_error), sizeof(mc_entity_effect_error),
                sizeof(mc_entity_tick_error), sizeof(mc_player_action_error), sizeof(wt_scope),
                sizeof(mc_mob_tick_scope), sizeof(mc_mob_tick_child_workspace)) > 0 &&
        fflush(out) == 0;
    const char *const cells[] = {"empty",   "pig1",    "pig8",    "pig64",
                                 "zombie1", "zombie8", "zombie64"};
    const size_t counts[] = {0, 1, 8, 64, 1, 8, 64};
    size_t rows = 0;
    for (size_t cell = 0; ok && cell < sizeof(cells) / sizeof(*cells); ++cell) {
        const char *kind = cell == 0 ? "empty" : cell < 4 ? "pig" : "zombie";
        const mc_entity_class_kind literal =
            cell < 4 ? MC_ENTITY_CLASS_KIND_PIG : MC_ENTITY_CLASS_KIND_ZOMBIE;
        for (unsigned int sample = 0; ok && sample < MB_WARMUPS + MB_SAMPLES; ++sample) {
            ok = mb_once(argv[1], cells[cell], kind, counts[cell], literal, sample, out);
            if (ok)
                ++rows;
        }
    }
    if (ok)
        ok = rows == 161 && fprintf(out,
                                    "{\"event\":\"footer\",\"complete\":true,\"rows\":%zu,"
                                    "\"cleanup\":true,\"checks\":%zu}\n",
                                    rows, checks) > 0;
    ok = fclose(out) == 0 && ok;
    if (!ok)
        return 1;
    printf("Mob tick diagnostics: %zu checks; 161 intervals; 2576 FD.g returns\n", checks);
    return 0;
}
