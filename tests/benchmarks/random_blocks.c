/* Fixed-work genuine Grass/Leaves declaring-method benchmark. All setup,
 * snapshots, assertions, event serialization and retirement are outside timing. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
int random_blocks_fluid_fixture_main(int, char **);
#define main random_blocks_fluid_fixture_main
#include "../world/runtime/test_fluids.c"
#undef main

#include "mc/world/runtime/lighting.h"
#include "random_blocks_adapter.h"
#include <inttypes.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

typedef struct rb_event {
    uint32_t kind;
    int32_t coordinates[6];
} rb_event;

typedef struct rb_observer {
    mc_world_runtime *world;
    rb_event events[4096];
    size_t count;
    bool bad;
} rb_observer;

typedef struct rb_guard {
    mc_world_runtime *world;
    mc_chunk_state *chunk;
    mc_chunk_data *data;
    mc_world_chunk *object;
    mc_world_chunk_fields fields;
    mc_world_runtime_fields world_fields;
    uint8_t *planes[5];
    mc_world_runtime_classes *classes;
    mc_registry_classes *registry;
    mc_entity_classes *entities;
    mc_java_random *random;
    mc_chunk_provider *provider;
    size_t bindings, calls, leases;
    uint64_t uniquifier, math;
    unsigned int holder;
    mc_java_random generator_random, cave_random;
} rb_guard;

static bool rb_clock(int64_t *out) {
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

static bool rb_frequency(int64_t *out) {
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

static FILE *rb_open(const char *out, const char *mode, unsigned int ordinal, const char *suffix) {
    char path[8192];
    const int length =
        snprintf(path, sizeof(path), "%s/%s/sample%u.%s", out, mode, ordinal, suffix);
    if (length < 0 || (size_t)length >= sizeof(path))
        return NULL;
    FILE *file = NULL;
#ifdef _MSC_VER
    if (fopen_s(&file, path, "wb") != 0)
        return NULL;
#else
    file = fopen(path, "wb");
#endif
    return file;
}

static bool rb_word(FILE *file, uint32_t value) {
    const uint8_t bytes[4] = {(uint8_t)(value >> 24u), (uint8_t)(value >> 16u),
                              (uint8_t)(value >> 8u), (uint8_t)value};
    return fwrite(bytes, sizeof(bytes), 1, file) == 1;
}

static bool rb_long(FILE *file, uint64_t value) {
    return rb_word(file, (uint32_t)(value >> 32u)) && rb_word(file, (uint32_t)value);
}

static bool rb_bytes(FILE *file, const uint8_t *data, size_t length) {
    return length <= UINT32_MAX && rb_word(file, (uint32_t)length) &&
           (length == 0 || fwrite(data, length, 1, file) == 1);
}

static mc_world_storage_status rb_event_store(rb_observer *o, mc_world_runtime *world,
                                              uint32_t kind, int32_t x, int32_t y, int32_t z,
                                              int32_t a, int32_t b, int32_t c,
                                              mc_world_storage_error *error) {
    if (world != o->world || o->count == COUNT(o->events)) {
        o->bad = true;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, "benchmark event admission", 0};
        return error->status;
    }
    o->events[o->count++] = (rb_event){kind, {x, y, z, a, b, c}};
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status rb_cell(void *context, mc_world_runtime *world, int32_t x, int32_t y,
                                       int32_t z, mc_world_storage_error *error) {
    return rb_event_store(context, world, 1, x, y, z, 0, 0, 0, error);
}

static mc_world_storage_status rb_range(void *context, mc_world_runtime *world, int32_t x,
                                        int32_t y, int32_t z, int32_t a, int32_t b, int32_t c,
                                        mc_world_storage_error *error) {
    return rb_event_store(context, world, 2, x, y, z, a, b, c, error);
}

static mc_world_storage_status rb_added(void *context, mc_world_runtime *world, mc_entity *entity,
                                        mc_world_storage_error *error) {
    (void)world;
    (void)entity;
    rb_observer *o = context;
    o->bad = true;
    *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "unexpected benchmark Entity publication", 0};
    return error->status;
}

static bool rb_capture(fixture *f, mc_chunk_state *chunk, rb_guard *g) {
    mc_world_runtime *world = f->worlds[0];
    mc_world_chunk *object = mc_world_store_chunk_object(f->stores[0], chunk);
    mc_world_chunk_fields fields;
    if (object == NULL || mc_world_chunk_fields_get(object, &fields) != MC_WORLD_STORAGE_OK)
        return false;
    *g = (rb_guard){.world = world,
                    .chunk = chunk,
                    .data = chunk->data,
                    .object = object,
                    .fields = fields,
                    .world_fields = world->fields,
                    .planes = {chunk->data->blocks, chunk->data->metadata, chunk->data->skylight,
                               chunk->data->block_light, chunk->data->heightmap},
                    .classes = world->classes,
                    .registry = f->registry[0],
                    .entities = f->entities[0],
                    .random = world->fields.r,
                    .provider = world->fields.v,
                    .bindings = world->persistent_bindings,
                    .calls = world->active_calls,
                    .leases = world->local_leases,
                    .uniquifier = atomic_load(&f->profile->uniquifier),
                    .math = f->profile->math ? atomic_load(&f->profile->math->state) : 0,
                    .holder = atomic_load(&f->profile->holder_phase),
                    .generator_random = f->providers[0].generator->generator_random,
                    .cave_random = f->providers[0].generator->cave_random};
    return true;
}

static bool rb_same_owners(const rb_guard *a, const rb_guard *b) {
    return a->world == b->world && a->chunk == b->chunk && a->data == b->data &&
           a->object == b->object && a->classes == b->classes && a->registry == b->registry &&
           a->entities == b->entities && a->random == b->random && a->provider == b->provider &&
           a->fields.b == b->fields.b && a->fields.d == b->fields.d && a->fields.e == b->fields.e &&
           a->fields.f == b->fields.f && a->fields.g == b->fields.g && a->fields.h == b->fields.h &&
           a->fields.l == b->fields.l && a->fields.m == b->fields.m &&
           memcmp(&a->world_fields, &b->world_fields, sizeof(a->world_fields)) == 0 &&
           memcmp(a->planes, b->planes, sizeof(a->planes)) == 0;
}

static bool rb_tick_record(FILE *file, const mc_scheduled_tick *tick) {
    mc_scheduled_tick_fields fields;
    return mc_scheduled_tick_fields_get(tick, &fields) == MC_SCHEDULED_TICK_OK &&
           rb_word(file, (uint32_t)fields.x) && rb_word(file, (uint32_t)fields.y) &&
           rb_word(file, (uint32_t)fields.z) && rb_word(file, (uint32_t)fields.block) &&
           rb_long(file, (uint64_t)fields.due_time) && rb_long(file, (uint64_t)fields.sequence);
}

static bool rb_snapshot(fixture *f, const rb_guard *g, const char *out, const char *mode,
                        unsigned int ordinal, const char *suffix) {
    bool ok = false;
    FILE *file = rb_open(out, mode, ordinal, suffix);
    const mc_scheduled_tick **membership = NULL, **ordered = NULL;
    int32_t fc = 0, ec = 0;
    size_t written = 0;
    mc_world_light_list_view light;
    if (file == NULL)
        goto done;
    const mc_chunk_data *data = g->data;
    if (!rb_word(file, UINT32_C(0x52425331)) ||
        !rb_bytes(file, data->blocks, data->blocks_capacity) ||
        !rb_bytes(file, data->metadata, MC_CHUNK_NIBBLE_BYTES) ||
        !rb_bytes(file, data->skylight, MC_CHUNK_NIBBLE_BYTES) ||
        !rb_bytes(file, data->block_light, MC_CHUNK_NIBBLE_BYTES) ||
        !rb_bytes(file, data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES))
        goto done;
    const int32_t *scratch = f->registry[0]->blocks.m[18]->subtype.bk.a;
    if (!rb_word(file, scratch == NULL ? UINT32_MAX : 32768u))
        goto done;
    if (scratch != NULL)
        for (size_t i = 0; i < 32768; ++i)
            if (!rb_word(file, (uint32_t)scratch[i]))
                goto done;
    if (mc_world_runtime_light_list_get(g->world, &light, NULL) != MC_WORLD_STORAGE_OK ||
        light.count > UINT32_MAX || !rb_word(file, (uint32_t)light.count))
        goto done;
    for (size_t i = 0; i < light.count; ++i) {
        const mc_world_light_region *region = NULL;
        if (mc_world_runtime_light_at(g->world, i, &region, NULL) != MC_WORLD_STORAGE_OK)
            goto done;
        const mc_world_light_region_fields *fields =
            mc_world_runtime_light_region_fields_get(region);
        if (fields == NULL)
            goto done;
        const mc_world_light_type_fields *type = mc_world_runtime_light_type_fields_get(fields->a);
        const mc_light_bounds *bounds = &fields->bounds;
        /* Each list occurrence carries its exact same-identity join to earlier
         * occurrences, independently of object addresses in another process. */
        size_t identity = i;
        for (size_t j = 0; j < i; ++j) {
            const mc_world_light_region *earlier = NULL;
            if (mc_world_runtime_light_at(g->world, j, &earlier, NULL) != MC_WORLD_STORAGE_OK)
                goto done;
            if (earlier == region) {
                identity = j;
                break;
            }
        }
        if (!rb_word(file, (uint32_t)identity) ||
            !rb_word(file, type == NULL ? 2u : (uint32_t)type->kind) ||
            !rb_word(file, (uint32_t)bounds->min_x) || !rb_word(file, (uint32_t)bounds->min_y) ||
            !rb_word(file, (uint32_t)bounds->min_z) || !rb_word(file, (uint32_t)bounds->max_x) ||
            !rb_word(file, (uint32_t)bounds->max_y) || !rb_word(file, (uint32_t)bounds->max_z))
            goto done;
    }
    if (mc_scheduled_membership_size(g->world->fields.F, &fc) != MC_SCHEDULED_MEMBERSHIP_OK ||
        mc_scheduled_ordered_size(g->world->fields.E, &ec) != MC_SCHEDULED_ORDERED_OK || fc < 0 ||
        ec < 0 || !rb_word(file, (uint32_t)fc) || !rb_word(file, (uint32_t)ec))
        goto done;
    membership = fc ? calloc((size_t)fc, sizeof(*membership)) : NULL;
    ordered = ec ? calloc((size_t)ec, sizeof(*ordered)) : NULL;
    if ((fc && membership == NULL) || (ec && ordered == NULL) ||
        mc_scheduled_membership_snapshot(g->world->fields.F, membership, (size_t)fc, &written) !=
            MC_SCHEDULED_MEMBERSHIP_OK ||
        written != (size_t)fc ||
        mc_scheduled_ordered_snapshot(g->world->fields.E, ordered, (size_t)ec, &written) !=
            MC_SCHEDULED_ORDERED_OK ||
        written != (size_t)ec)
        goto done;
    for (int32_t i = 0; i < fc; ++i)
        if (!rb_tick_record(file, membership[i]))
            goto done;
    for (int32_t i = 0; i < ec; ++i) {
        int32_t identity = -1;
        for (int32_t j = 0; j < fc; ++j)
            if (ordered[i] == membership[j]) {
                identity = j;
                break;
            }
        if (!rb_tick_record(file, ordered[i]) || !rb_word(file, (uint32_t)identity))
            goto done;
    }
    ok = !ferror(file);
done:
    free(membership);
    free(ordered);
    if (file != NULL && fclose(file) != 0)
        ok = false;
    return ok;
}

static bool rb_events(const rb_observer *observer, const char *out, const char *mode,
                      unsigned int ordinal) {
    FILE *file = rb_open(out, mode, ordinal, "events.bin");
    if (file == NULL)
        return false;
    bool ok = rb_word(file, UINT32_C(0x52424531)) && rb_word(file, (uint32_t)observer->count);
    for (size_t i = 0; ok && i < observer->count; ++i) {
        ok = rb_word(file, observer->events[i].kind);
        for (size_t j = 0; ok && j < 6; ++j)
            ok = rb_word(file, (uint32_t)observer->events[i].coordinates[j]);
    }
    if (fclose(file) != 0)
        ok = false;
    return ok;
}

static bool rb_scalar(FILE *file, const rb_guard *g, const mc_java_random *supplied) {
    const mc_world_runtime *world = g->world;
    mc_world_chunk_fields fields;
    uint64_t supplied_gaussian, world_gaussian, generator_gaussian, cave_gaussian;
    memcpy(&supplied_gaussian, &supplied->next_next_gaussian, sizeof(supplied_gaussian));
    memcpy(&world_gaussian, &world->fields.r->next_next_gaussian, sizeof(world_gaussian));
    memcpy(&generator_gaussian, &g->generator_random.next_next_gaussian,
           sizeof(generator_gaussian));
    memcpy(&cave_gaussian, &g->cave_random.next_next_gaussian, sizeof(cave_gaussian));
    if (mc_world_chunk_fields_get(g->object, &fields) != MC_WORLD_STORAGE_OK)
        return false;
    return fprintf(
               file,
               "{\"supplied\":[\"%012" PRIx64 "\",%u,\"%016" PRIx64
               "\"],\"world_random\":[\"%012" PRIx64 "\",%u,\"%016" PRIx64 "\"],\"g\":%" PRId32
               ",\"Q\":%" PRId32 ",\"h\":%" PRId32 ",\"f\":%" PRId32 ",\"M\":%" PRId32
               ",\"chunk_flags\":[%u,%u,%u,%u,%u],\"last_save\":%" PRId64
               ",\"chunk_fields\":[%" PRId32 ",%" PRId32 ",%" PRId32 "],\"minimum_height\":%" PRId32
               ",\"chunk_a\":%u,\"profile\":[\"%016" PRIx64 "\",%u,\"%016" PRIx64
               "\"],\"holds\":[%zu,%zu,%zu],\"cache\":%zu,\"entities\":%zu,"
               "\"weather\":%zu,\"players\":%zu,\"generation_randoms\":[[\"%012" PRIx64
               "\",%u,\"%016" PRIx64 "\"],[\"%012" PRIx64 "\",%u,\"%016" PRIx64 "\"]]}",
               supplied->state, supplied->have_next_next_gaussian ? 1u : 0u, supplied_gaussian,
               world->fields.r->state, world->fields.r->have_next_next_gaussian ? 1u : 0u,
               world_gaussian, world->fields.g, world->fields.Q, world->fields.h, world->fields.f,
               world->fields.M, g->chunk->dirty ? 1u : 0u, g->chunk->active ? 1u : 0u,
               g->chunk->terrain_populated ? 1u : 0u, g->chunk->never_save ? 1u : 0u,
               g->chunk->has_entities ? 1u : 0u, g->chunk->last_save_time, fields.i, fields.j,
               fields.k, g->data->minimum_height, world->classes->chunk_a ? 1u : 0u,
               atomic_load(&world->classes->profile->uniquifier),
               atomic_load(&world->classes->profile->holder_phase),
               world->classes->profile->math ? atomic_load(&world->classes->profile->math->state)
                                             : 0,
               world->persistent_bindings, world->active_calls, world->local_leases,
               world->lifecycle.cache->count, world->entity_membership.count,
               world->weather_membership.count, world->player_membership.count,
               g->generator_random.state, g->generator_random.have_next_next_gaussian ? 1u : 0u,
               generator_gaussian, g->cave_random.state,
               g->cave_random.have_next_next_gaussian ? 1u : 0u, cave_gaussian) >= 0;
}

static bool rb_call(mc_world_runtime *world, const mc_block *block, random_blocks_leaf function,
                    int32_t x, int32_t y, int32_t z, mc_java_random *random,
                    mc_world_mutation_error *error) {
    if (mc_world_runtime_call_enter(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    mc_world_mutation_error_init(error);
    const bool returned = function(world, block, x, y, z, random, error);
    mc_world_runtime_call_leave(world);
    return returned;
}

static bool rb_sample(fixture *f, const random_blocks_adapter *adapter, const char *root,
                      const char *out, unsigned int mode_index, unsigned int ordinal,
                      bool measure) {
    static const char *const names[] = {"grass_bright", "leaves_flag0", "leaves_connected"};
    const char *mode = names[mode_index];
    const size_t calls = mode_index == 2 ? 512u : 32768u;
    bool ok = false;
    char input[8192];
    rb_guard before = {0}, after = {0};
    mc_world_access *access = NULL;
    rb_observer *observer = calloc(1, sizeof(*observer));
    mc_world_mutation_error *error = calloc(1, sizeof(*error));
    FILE *state = NULL;
    mc_java_random supplied, expected, world_before;
    const int32_t *scratch_before = f->registry[0]->blocks.m[18]->subtype.bk.a;
    const int n = snprintf(input, sizeof(input), "%s/%s/sample%u", root, mode, ordinal);
    CHECK(observer != NULL && error != NULL && n > 0 && (size_t)n < sizeof(input) &&
          open_store(f, 0, input) && create_world(f, 0));
    /* Actual writes prefault every event byte before any measured call. */
    volatile unsigned char *event_bytes = (volatile unsigned char *)observer->events;
    for (size_t i = 0; i < sizeof(observer->events); ++i)
        event_bytes[i] = 0;
    mc_world_runtime *world = f->worlds[0];
    mc_chunk_state *chunk = NULL;
    CHECK(mc_world_runtime_get(world, 0, 0, &chunk) == MC_CHUNK_ACQUIRE_OK && chunk != NULL &&
          !chunk->blank && remember(f, 0) && f->providers[0].cache->count == 1);
    world->fields.q = 1;
    world->fields.f = world->fields.m = 0;
    world->fields.g = 17;
    world->fields.Q = 6001;
    world->fields.i = world->fields.j = world->fields.k = world->fields.l = 0.0f;
    world->fields.B = false;
    mc_java_random_reseed(world->fields.r, 89);
    mc_java_random_seed(&supplied, 0);
    expected = supplied;
    world_before = *world->fields.r;
    if (mode_index == 0)
        for (size_t i = 0; i < calls; ++i) {
            int32_t ignored;
            CHECK(mc_java_random_next_bounded(&expected, 3, &ignored) &&
                  mc_java_random_next_bounded(&expected, 5, &ignored) &&
                  mc_java_random_next_bounded(&expected, 3, &ignored));
        }
    observer->world = world;
    const mc_world_access_callbacks callbacks = {
        .cell = rb_cell, .range = rb_range, .entity_added = rb_added};
    CHECK(mc_world_access_create(&callbacks, observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK &&
          rb_capture(f, chunk, &before));
    const mc_block *block = f->registry[0]->blocks.m[mode_index == 0 ? 2 : 18];
    const random_blocks_leaf selected = mode_index == 0 ? adapter->grass : adapter->leaves;
    CHECK(block != NULL && selected != NULL &&
          rb_snapshot(f, &before, out, mode, ordinal, "before.bin"));
    state = rb_open(out, mode, ordinal, "state.json");
    CHECK(state != NULL && fputs("{\"before\":", state) >= 0 &&
          rb_scalar(state, &before, &supplied));
    int64_t frequency, start = 0, stop = 0;
    CHECK(rb_frequency(&frequency));
    const bool timed = measure && ordinal >= 2;
    const bool started = !timed || rb_clock(&start);
    size_t returned = 0;
    if (mode_index != 2) {
        for (size_t i = 0; i < calls; ++i) {
            if (!rb_call(world, block, selected, 8, 64, 8, &supplied, error))
                break;
            ++returned;
        }
    } else {
        static const int32_t xs[5] = {5, 6, 7, 9, 10};
        for (int32_t y = 5; y <= 22 && returned < calls; ++y)
            for (int32_t z = 5; z <= 10 && returned < calls; ++z)
                for (size_t x = 0; x < COUNT(xs) && returned < calls; ++x) {
                    if (!rb_call(world, block, selected, xs[x], y, z, &supplied, error))
                        goto stopped;
                    ++returned;
                }
    }
stopped:;
    const bool stopped = !timed || rb_clock(&stop);
    CHECK(started && stopped && stop >= start && returned == calls && !observer->bad &&
          rb_capture(f, chunk, &after) && rb_same_owners(&before, &after));
    CHECK(mc_test_java_random_equal(&supplied, &expected) &&
          mc_test_java_random_equal(world->fields.r, &world_before) &&
          before.bindings == after.bindings && before.calls == after.calls &&
          before.leases == after.leases && before.uniquifier == after.uniquifier &&
          before.holder == after.holder && before.math == after.math &&
          mc_test_java_random_equal(&before.generator_random, &after.generator_random) &&
          mc_test_java_random_equal(&before.cave_random, &after.cave_random) &&
          f->providers[0].cache->count == 1 && world->entity_membership.count == 0 &&
          world->weather_membership.count == 0 && world->player_membership.count == 0 &&
          error->status == MC_WORLD_STORAGE_OK && error->hook_class == NULL &&
          error->hook_method == NULL && error->hook_bytecode == 0);
    const int32_t *scratch = f->registry[0]->blocks.m[18]->subtype.bk.a;
    CHECK((scratch_before == NULL || scratch == scratch_before) &&
          (mode_index == 2 ? scratch != NULL : scratch == NULL) &&
          (mode_index == 0 ? observer->count > 0 : observer->count == 0) &&
          rb_snapshot(f, &after, out, mode, ordinal, "after.bin") &&
          rb_events(observer, out, mode, ordinal));
    CHECK(fputs(",\"after\":", state) >= 0 && rb_scalar(state, &after, &supplied) &&
          fprintf(state,
                  ",\"calls\":%zu,\"returned\":%zu,\"owners_same\":true,\"world_fields_same\":true,"
                  "\"holds_same\":true,\"scratch_same\":true,\"scratch_before\":%u,"
                  "\"scratch_after\":%u,\"events\":%zu,\"error\":[%d,%u,%u,%u,%u]}\n",
                  calls, returned, scratch_before != NULL ? 1u : 0u, scratch != NULL ? 1u : 0u,
                  observer->count, (int)error->status, error->hook_bytecode,
                  error->presence_bytecode, error->provider_bytecode, error->query.bytecode) >= 0 &&
          fclose(state) == 0);
    state = NULL;
    printf(
        "RANDOM_BLOCKS_CPU_SAMPLE mode=%s ordinal=%u timed=%u ticks=%" PRIu64 " frequency=%" PRIu64
        " calls=%zu returned=%zu owners-same=1 random-checked=1 "
        "scratch-same=1 holds-same=1 events=%zu cache=1 scratch=%u supplied-before=%012" PRIx64
        " supplied-after=%012" PRIx64 " world-before=%012" PRIx64 " world-after=%012" PRIx64 "\n",
        mode, ordinal, timed ? 1u : 0u, (uint64_t)(stop - start), (uint64_t)frequency, calls,
        returned, observer->count, scratch != NULL ? 1u : 0u, (UINT64_C(0) ^ UINT64_C(0x5deece66d)),
        supplied.state, world_before.state, world->fields.r->state);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    access = NULL;
    CHECK(remember(f, 0));
    ok = true;
cleanup:
    if (state != NULL)
        (void)fclose(state);
    if (access != NULL && f->worlds[0] != NULL) {
        bool cleanup_removed = false;
        (void)mc_world_runtime_remove_world_access(f->worlds[0], access, &cleanup_removed, NULL);
        (void)mc_world_access_free(access, NULL);
    }
    free(error);
    free(observer);
    /* The fixture owns terminal disposal; this is outside the measured call. */
    return ok;
}

static bool rb_close(fixture *f) {
    if (f->worlds[0] == NULL)
        return true;
    if (!remember(f, 0) ||
        mc_world_runtime_retire_entity_containers(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK ||
        mc_chunk_pending_clear(f->providers[0].pending) != MC_CHUNK_ACQUIRE_OK ||
        !mc_chunk_cache_init(f->providers[0].cache, f->providers[0].cache->entries,
                             f->providers[0].cache->capacity, f->providers[0].cache->insertions,
                             f->providers[0].cache->insertion_capacity) ||
        mc_world_runtime_retire_world(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
        return false;
    for (size_t i = 0; i < COUNT(f->children); ++i)
        if (f->children[i].state != NULL) {
            if (mc_world_store_retire_chunk(f->children[i].store, f->children[i].state, NULL) !=
                MC_WORLD_STORAGE_OK)
                return false;
            f->children[i] = (fluid_chunk){0};
        }
    if (mc_world_runtime_retire_world(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_free(f->worlds[0], NULL) != MC_WORLD_STORAGE_OK)
        return false;
    f->worlds[0] = NULL;
    f->bound[0] = false;
    mc_world_provider_release(f->dimensions[0]);
    f->dimensions[0] = NULL;
    if (mc_world_store_free_checked(&f->stores[0], NULL) != MC_WORLD_STORAGE_OK)
        return false;
    memset(&f->providers[0], 0, sizeof(f->providers[0]));
    return true;
}

int main(int argc, char **argv) {
    bool ok = false;
    fixture *f = NULL;
    if (argc != 4 || (strcmp(argv[3], "measure") != 0 && strcmp(argv[3], "controls") != 0) ||
        !mc_math_trig_init(&trig))
        return 2;
    const bool measure = strcmp(argv[3], "measure") == 0;
    const unsigned int samples = measure ? 9u : 1u;
    const random_blocks_adapter *adapter = random_blocks_adapter_get();
    const uint16_t title[] = {'L', 'e', 'a', 'f'};
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && adapter != NULL && adapter->name != NULL &&
          mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    resources *source = &f->sources[0];
    source->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    source->streams[1] = source->streams[2] = (stream){.bytes = language};
    source->api = (mc_registry_resources){source, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         &source->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entities[0]) == MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK);
    printf("RANDOM_BLOCKS_CPU_CONTEXT adapter=%s samples=%u measure=%u modes=3\n", adapter->name,
           samples, measure ? 1u : 0u);
    for (unsigned int mode = 0; mode < 3; ++mode)
        for (unsigned int ordinal = 0; ordinal < samples; ++ordinal)
            CHECK(rb_sample(f, adapter, argv[1], argv[2], mode, ordinal, measure) && rb_close(f));
    ok = true;
cleanup:
    if (!cleanup_fixture(f))
        ok = false;
    if (ok)
        printf("RANDOM_BLOCKS_CPU_FINISH checks=%u cleanup=1 modes=3\n", checks);
    return ok ? 0 : 1;
}
