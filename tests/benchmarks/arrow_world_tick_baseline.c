/* Uninstrumented fixed FD.g baseline: one fresh cell, eight real World calls.
 * Setup, snapshots, checks, output and checked retirement are outside clock. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
int am_unused_natural_flight_main(int, char **);
#define MC_ARROW_NATURAL_FLIGHT_MAIN am_unused_natural_flight_main
#include "../world/runtime/test_arrow_natural_flight.c"
#undef MC_ARROW_NATURAL_FLIGHT_MAIN
#include "../../src/core/java/random_profile_internal.h"
#include "../../src/entity/bounds/bounds_internal.h"
#include "mc/world/runtime/movement.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <time.h>
#endif

#include <inttypes.h>

enum { AM_ARROWS = 8, AM_CHUNKS = 30, AM_TICKS = 8 };

typedef struct am_actor {
    mc_entity_snapshot entity;
    mc_arrow_state arrow;
    mc_entity_bounds bounds;
    mc_java_random random;
    size_t holds, chunk_occurrences, world_occurrences, nearby_occurrences;
    bool backlink_clear;
} am_actor;

typedef struct am_chunk {
    mc_chunk_state state;
    uintptr_t identity, object;
    uint64_t air_hash;
    size_t allocation_bytes, slices[8];
    bool containers_retired, source_disabled;
} am_chunk;

typedef struct am_observation {
    am_actor actors[AM_ARROWS];
    am_chunk chunks[AM_CHUNKS];
    mc_chunk_provider_cache_observation cache;
    mc_math_class_state math;
    mc_java_random world_random;
    uint64_t uniquifier, math_random_state;
    double math_gaussian;
    unsigned int holder_phase;
    bool math_present, math_cached, constructor_inputs_bound, constructor_records_bound;
    size_t world_bindings, active_calls, local_leases, code_borrows, entity_borrowers;
    size_t live_entities, entity_bytes, bounds_holds, vector_holds, hit_calls, hit_results;
    int32_t bounds_cursor, vector_cursor;
    size_t loaded_count, deferred_count, nearby_count, collision_count;
    unsigned char context[sizeof(mc_world_entity_tick_context)];
} am_observation;

typedef struct am_run {
    nf_fixture setup;
    mc_entity *actors[AM_ARROWS]; /* Each published pointer keeps its caller occurrence. */
    mc_world_entity_tick_error *errors;
    mc_arrow_error constructed[AM_ARROWS], heading[AM_ARROWS];
    am_observation *observations;
    mc_world_storage_status statuses[AM_TICKS];
    size_t count, attempted, returned;
    int64_t begin, end, frequency;
    bool setup_complete, before_captured, clock_started, clock_ended, after_captured, validated,
        cleanup;
} am_run;

/* Same native integer-clock convention as mob_tick_diagnostics.c. */
static bool am_clock(int64_t *out) {
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

static bool am_frequency(int64_t *out) {
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

static bool am_setup(am_run *r, const char *root) {
    bool ok = false;
    nf_fixture *s = &r->setup;
    s->world = calloc(1, sizeof(*s->world));
    tick_error = &r->errors[0];
    s->owns_error = false;
    CHECK(s->world != NULL &&
          mc_math_classes_create(SIZE_MAX, &s->math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(s->math, NULL) == MC_MATH_CLASS_OK);
    const mc_math_trig *table = mc_math_classes_table(s->math);
    /* Unchanged NF2 lower helper bootstraps/loads the genuine origin. Native
     * budgets are constructor inputs64, never raised after an admission. */
    CHECK(table != NULL && tick_setup_configured(s->world, root, 64, table));
    tick_context.player.travel.code_registry = s->world->registry[0];
    tick_context.player.travel.trig = table;
    tick_context.player.travel.math = s->math;
    tick_context.math = s->math;
    tick_context.arrow_code_classes = s->world->classes[0];
    /* Author all29 additional real records before any of their GETs. The
     * actual inclusive +/-32 presence union requires5x6, not NF2's5x5. */
    for (int32_t x = -2; x <= 2; ++x)
        for (int32_t z = -2; z <= 3; ++z)
            if (x != 0 || z != 0)
                CHECK(tick_seed(s->world, 0, x, z));
    for (int32_t x = -2; x <= 2; ++x)
        for (int32_t z = -2; z <= 3; ++z)
            if (x != 0 || z != 0) {
                mc_chunk_state *state = NULL;
                CHECK(mc_world_runtime_get(s->world->worlds[0], x, z, &state) ==
                          MC_CHUNK_ACQUIRE_OK &&
                      state != NULL && state->active && !state->blank);
            }
    CHECK(mc_world_store_chunk_count(s->world->stores[0]) == AM_CHUNKS &&
          s->world->providers[0].cache->count == AM_CHUNKS);
    for (size_t i = 0; i < r->count; ++i) {
        bool inserted = false;
        mc_entity_error allocation = {0};
        mc_world_entity_error publication = {0};
        CHECK(mc_arrow_allocate(s->world->entity_classes[0], &r->actors[i], &allocation) ==
                  MC_ENTITY_OK &&
              mc_arrow_construct_position(r->actors[i], s->world->worlds[0], 8.5, 66.0,
                                          0.5 + 4.0 * (double)i,
                                          &r->constructed[i]) == MC_ENTITY_OK &&
              mc_arrow_heading(r->actors[i], s->math, 1.0, 0.0, 0.0, 0.6f, 0.0f, &r->heading[i]) ==
                  MC_ENTITY_OK &&
              r->heading[i].heading_returned && r->heading[i].gaussian_calls == 3 &&
              r->heading[i].random_captured &&
              mc_world_runtime_insert_entity(s->world->worlds[0], r->actors[i], &inserted,
                                             &publication) == MC_WORLD_STORAGE_OK &&
              inserted);
    }
    /* Genuine public fd.a(IIIIII) presence for ALL8 planned lanes in ALL cells.
     * No held-only private helper or Source tick/parent is invoked here. */
    for (int32_t i = 0; i < AM_ARROWS; ++i) {
        bool present = false;
        mc_world_movement_error error = {0};
        CHECK(mc_world_runtime_area_loaded(s->world->worlds[0], 8 - 32, 0, 4 * i - 32, 8 + 32, 128,
                                           4 * i + 32, &present, &error) == MC_WORLD_STORAGE_OK &&
              present);
    }
    r->setup_complete = true;
    ok = true;
cleanup:
    return ok;
}

static uint64_t am_bytes(uint64_t h, const uint8_t *p, size_t n, bool *air) {
    for (size_t i = 0; i < n; ++i) {
        *air = *air && p[i] == 0;
        h = (h ^ p[i]) * UINT64_C(1099511628211);
    }
    return h;
}

static bool am_snapshot(am_run *r, am_observation *o) {
    bool ok = false;
    fixture *f = r->setup.world;
    mc_world_runtime *w = f->worlds[0];
    CHECK(mc_world_nearby_owned_valid(w) && w->fields.b == w->entity_membership.list &&
          w->fields.R == w->nearby_entities.list && w->fields.b->count == r->count &&
          w->entity_membership.count == r->count &&
          mc_chunk_cache_validate(f->providers[0].cache) &&
          mc_chunk_provider_cache_observe(w->fields.v, &o->cache) &&
          mc_math_classes_state(r->setup.math, &o->math, NULL) == MC_MATH_CLASS_OK);
    for (size_t i = 0; i < r->count; ++i)
        CHECK(w->entity_membership.entries[i] == w->fields.b->items[i]);
    o->world_random = *w->fields.r;
    o->uniquifier = atomic_load(&f->profile->uniquifier);
    o->holder_phase = atomic_load(&f->profile->holder_phase);
    o->math_present = f->profile->math != NULL;
    if (o->math_present) {
        o->math_random_state = atomic_load(&f->profile->math->state);
        o->math_gaussian = f->profile->math->next_next_gaussian;
        o->math_cached = f->profile->math->have_next_next_gaussian;
    }
    o->constructor_inputs_bound = f->profile->constructor_inputs_bound;
    o->constructor_records_bound = f->profile->constructor_records_bound;
    CHECK(!o->constructor_inputs_bound && !o->constructor_records_bound);
    o->world_bindings = w->persistent_bindings;
    o->active_calls = w->active_calls;
    o->local_leases = w->local_leases;
    o->code_borrows = f->classes[0]->native_borrows;
    mc_entity_classes *classes = f->entity_classes[0];
    o->entity_borrowers = classes->borrowers;
    o->live_entities = classes->live_entities;
    o->entity_bytes = classes->allocated_bytes;
    o->bounds_holds = classes->bounds_holds;
    o->vector_holds = classes->vector_holds;
    o->bounds_cursor = classes->bounds_cursor;
    o->vector_cursor = classes->vector_cursor;
    o->hit_calls = classes->live_hit_calls;
    o->hit_results = classes->live_hit_results;
    o->loaded_count = w->fields.b->count;
    o->deferred_count = w->fields.D->count;
    o->nearby_count = w->nearby_entities.count;
    o->collision_count = w->collision_boxes.count;
    memcpy(o->context, &tick_context, sizeof(tick_context));
    CHECK(mc_world_store_chunk_count(f->stores[0]) == AM_CHUNKS &&
          o->cache.map_count == AM_CHUNKS && o->cache.insertion_count == AM_CHUNKS);
    for (size_t c = 0; c < AM_CHUNKS; ++c) {
        mc_chunk_state *state = NULL;
        CHECK(mc_world_store_chunk_at(f->stores[0], c, &state, NULL) == MC_WORLD_STORAGE_OK &&
              state != NULL && state->active && state->terrain_populated && !state->blank &&
              state->data != NULL && state->x >= -2 && state->x <= 2 && state->z >= -2 &&
              state->z <= 3);
        mc_world_chunk *chunk = mc_world_store_chunk_object(f->stores[0], state);
        CHECK(chunk != NULL && chunk->d == w && !chunk->entity_containers_retired &&
              !chunk->source_disabled);
        for (size_t earlier = 0; earlier < c; ++earlier)
            CHECK(
                o->chunks[earlier].identity != (uintptr_t)state &&
                (o->chunks[earlier].state.x != state->x || o->chunks[earlier].state.z != state->z));
        am_chunk *out = &o->chunks[c];
        out->state = *state;
        out->identity = (uintptr_t)state;
        out->object = (uintptr_t)chunk;
        out->allocation_bytes = chunk->allocation_bytes;
        out->containers_retired = chunk->entity_containers_retired;
        out->source_disabled = chunk->source_disabled;
        bool air = true;
        uint64_t h = UINT64_C(1469598103934665603);
        h = am_bytes(h, state->data->blocks, MC_CHUNK_BLOCKS, &air);
        h = am_bytes(h, state->data->metadata, MC_CHUNK_NIBBLE_BYTES, &air);
        h = am_bytes(h, state->data->skylight, MC_CHUNK_NIBBLE_BYTES, &air);
        h = am_bytes(h, state->data->block_light, MC_CHUNK_NIBBLE_BYTES, &air);
        h = am_bytes(h, state->data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES, &air);
        CHECK(air);
        out->air_hash = h;
        for (size_t slice = 0; slice < 8; ++slice) {
            out->slices[slice] = chunk->entities[slice].count;
            for (size_t j = 0; j < chunk->entities[slice].count; ++j) {
                mc_entity *entity = chunk->entities[slice].items[j];
                size_t index = 0;
                while (index < r->count && r->actors[index] != entity)
                    ++index;
                CHECK(index < r->count && entity->base.bF && entity->base.bG == state->x &&
                      entity->base.bI == state->z && entity->base.bH == (int32_t)slice);
                ++o->actors[index].chunk_occurrences;
            }
        }
    }
    for (size_t i = 0; i < r->count; ++i) {
        mc_entity *p = r->actors[i];
        am_actor *a = &o->actors[i];
        CHECK(p != NULL && mc_entity_get_snapshot(p, &a->entity) == MC_ENTITY_OK &&
              mc_arrow_inspect(p, &a->arrow) && p->bounds != NULL && p->random != NULL &&
              p->base.world == w && p->base.aG == NULL && p->base.aH == NULL);
        a->bounds = *p->bounds;
        a->random = *p->random;
        a->holds = p->holds;
        a->backlink_clear = p->arrow->tick_call == NULL;
        for (size_t j = 0; j < w->fields.b->count; ++j)
            a->world_occurrences += w->fields.b->items[j] == p ? 1u : 0u;
        for (size_t j = 0; j < w->nearby_entities.count; ++j)
            a->nearby_occurrences += w->nearby_entities.entries[j] == p ? 1u : 0u;
        CHECK(a->world_occurrences == 1 && a->chunk_occurrences == 1 &&
              a->holds >= 1 + a->world_occurrences + a->chunk_occurrences + a->nearby_occurrences &&
              a->entity.phase == MC_ENTITY_READY && p->base.bF && a->backlink_clear &&
              !p->arrow->constructor_active && !p->arrow->heading_active);
    }
    ok = true;
cleanup:
    return ok;
}

static bool am_random_equal(const mc_java_random *a, const mc_java_random *b) {
    return a->state == b->state && a->have_next_next_gaussian == b->have_next_next_gaussian &&
           ad_bits(a->next_next_gaussian) == ad_bits(b->next_next_gaussian);
}

static bool am_validate(am_run *r) {
    bool ok = false;
    const am_observation *a = &r->observations[0], *b = &r->observations[1];
    CHECK(r->before_captured && r->after_captured);
    CHECK(r->attempted == AM_TICKS && r->returned == AM_TICKS && r->clock_started &&
          r->clock_ended && r->end >= r->begin && b->world_bindings == a->world_bindings &&
          b->code_borrows == a->code_borrows && b->active_calls == a->active_calls &&
          b->local_leases == a->local_leases && b->entity_borrowers == a->entity_borrowers &&
          b->live_entities == a->live_entities && b->bounds_holds == a->bounds_holds &&
          b->vector_holds == a->vector_holds && b->hit_calls == a->hit_calls &&
          b->hit_results == a->hit_results && b->math.holds == a->math.holds &&
          b->math.phase == a->math.phase && b->math.source_a == a->math.source_a &&
          b->uniquifier == a->uniquifier && b->holder_phase == a->holder_phase &&
          b->math_present == a->math_present && b->math_random_state == a->math_random_state &&
          b->math_cached == a->math_cached &&
          ad_bits(b->math_gaussian) == ad_bits(a->math_gaussian) &&
          am_random_equal(&a->world_random, &b->world_random) &&
          memcmp(a->context, b->context, sizeof(a->context)) == 0);
    for (size_t tick = 0; tick < AM_TICKS; ++tick)
        CHECK(r->statuses[tick] == MC_WORLD_STORAGE_OK &&
              r->errors[tick].status == MC_WORLD_STORAGE_OK &&
              r->errors[tick].child == MC_WORLD_ENTITY_TICK_CHILD_NONE);
    for (size_t c = 0; c < AM_CHUNKS; ++c) {
        CHECK(a->chunks[c].identity == b->chunks[c].identity &&
              a->chunks[c].object == b->chunks[c].object &&
              a->chunks[c].air_hash == b->chunks[c].air_hash);
        for (size_t j = 0; j < 8; ++j)
            CHECK(a->chunks[c].slices[j] == b->chunks[c].slices[j]);
    }
    for (size_t i = 0; i < r->count; ++i) {
        const am_actor *before = &a->actors[i], *after = &b->actors[i];
        CHECK(before->entity.base.bt == 0 && before->arrow.fields.k == 0 &&
              ad_bits(before->entity.base.x) == ad_bits(8.5) &&
              ad_bits(before->entity.base.y) == ad_bits(66.0) &&
              ad_bits(before->entity.base.z) == ad_bits(0.5 + 4.0 * (double)i) &&
              before->arrow.fields.c == NULL && after->arrow.fields.c == NULL &&
              after->entity.base.bt == mc_java_i32_add(before->entity.base.bt, AM_TICKS) &&
              after->arrow.fields.k == mc_java_i32_add(before->arrow.fields.k, AM_TICKS) &&
              after->entity.base.x > before->entity.base.x && !after->arrow.fields.i &&
              !after->entity.base.be && after->entity.base.bF && after->backlink_clear &&
              am_random_equal(&before->random, &after->random) &&
              before->holds >= before->nearby_occurrences &&
              after->holds >= after->nearby_occurrences &&
              before->holds - before->nearby_occurrences ==
                  after->holds - after->nearby_occurrences);
    }
    r->validated = true;
    ok = true;
cleanup:
    return ok;
}

static bool am_cleanup(am_run *r) {
    if (!r->setup_complete)
        return false; /* Keep an unfinished setup, never guess Source repair. */
    for (size_t tick = 0; tick < AM_TICKS; ++tick)
        if (!mc_world_entity_tick_error_retire(&r->errors[tick]))
            return false;
    for (size_t i = 0; i < AM_ARROWS; ++i) {
        mc_entity_release(r->actors[i]);
        r->actors[i] = NULL; /* Consume only the caller occurrence, not Source membership. */
    }
    r->cleanup = nf_cleanup(&r->setup); /* Real terminal containers; never World.R source clear. */
    return r->cleanup;
}

static bool am_hex(FILE *out, const unsigned char *p, size_t n) {
    if (fputc('"', out) == EOF)
        return false;
    for (size_t i = 0; i < n; ++i)
        if (fprintf(out, "%02x", (unsigned int)p[i]) != 2)
            return false;
    return fputc('"', out) != EOF;
}

static bool am_random_json(FILE *out, const mc_java_random *p) {
    return fprintf(out,
                   "{\"state\":\"%016" PRIx64 "\",\"cached\":%s,\"gaussian\":\"%016" PRIx64 "\"}",
                   p->state, p->have_next_next_gaussian ? "true" : "false",
                   ad_bits(p->next_next_gaussian)) > 0;
}

static bool am_actor_json(FILE *out, const am_actor *a) {
    const mc_entity_base_fields *b = &a->entity.base;
    const double d[] = {b->aE, b->aJ, b->aK,         b->aL,         b->x,
                        b->y,  b->z,  b->velocity_x, b->velocity_y, b->velocity_z,
                        b->bl, b->bm, b->bn,         b->private_d,  b->private_e};
    const float f[] = {b->yaw, b->aT, b->aU, b->aV,     b->offset, b->width, b->height,
                       b->bi,  b->bj, b->bk, b->y_size, b->bp,     b->br,    b->bE};
    const int32_t ints[] = {b->id,
                            b->private_b,
                            b->bt,
                            b->bu,
                            b->bv,
                            b->bw,
                            b->by,
                            b->bz,
                            b->bG,
                            b->bH,
                            b->bI,
                            b->client_only.bJ,
                            b->client_only.bK,
                            b->client_only.bL};
    const unsigned int flags[] = {b->aF, b->aX, b->aY, b->aZ,        b->ba, b->bb, b->bc, b->bd,
                                  b->be, b->bq, b->bx, b->private_c, b->bC, b->bF, b->bM};
    if (fprintf(out,
                "{\"id\":%" PRId32 ",\"bt\":%" PRId32 ",\"k\":%" PRId32
                ",\"phase\":%u,\"holds\":%zu,\"World\":%zu,\"Chunk\":%zu,\"R\":%zu,"
                "\"backlink_clear\":%s,\"bF\":%s,\"dead\":%s,\"chunk\":[%" PRId32 ",%" PRId32
                ",%" PRId32 "],\"doubles\":[",
                b->id, b->bt, a->arrow.fields.k, (unsigned int)a->entity.phase, a->holds,
                a->world_occurrences, a->chunk_occurrences, a->nearby_occurrences,
                a->backlink_clear ? "true" : "false", b->bF ? "true" : "false",
                b->be ? "true" : "false", b->bG, b->bH, b->bI) <= 0)
        return false;
    for (size_t i = 0; i < COUNT(d); ++i)
        if (fprintf(out, "%s\"%016" PRIx64 "\"", i ? "," : "", ad_bits(d[i])) <= 0)
            return false;
    if (fputs("],\"floats\":[", out) == EOF)
        return false;
    for (size_t i = 0; i < COUNT(f); ++i) {
        uint32_t bits;
        memcpy(&bits, &f[i], sizeof(bits));
        if (fprintf(out, "%s\"%08" PRIx32 "\"", i ? "," : "", bits) <= 0)
            return false;
    }
    if (fprintf(out,
                "],\"bounds\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],\"SL\":[%u,%" PRId32
                ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%u,%" PRId32
                ",%" PRId32 "],\"rng\":",
                ad_bits(a->bounds.min_x), ad_bits(a->bounds.min_y), ad_bits(a->bounds.min_z),
                ad_bits(a->bounds.max_x), ad_bits(a->bounds.max_y), ad_bits(a->bounds.max_z),
                (unsigned int)a->arrow.fields.a, a->arrow.fields.b, a->arrow.fields.d,
                a->arrow.fields.e, a->arrow.fields.f, a->arrow.fields.g, a->arrow.fields.h,
                (unsigned int)a->arrow.fields.i, a->arrow.fields.j, a->arrow.fields.k) <= 0)
        return false;
    if (!am_random_json(out, &a->random) ||
        fprintf(out,
                ",\"source\":%u,\"constructor_pc\":%u,\"heading_pc\":%u,"
                "\"shooter_null\":%s,\"base_ints\":[",
                (unsigned int)a->entity.source, a->arrow.constructor_pc, a->arrow.heading_pc,
                a->arrow.fields.c == NULL ? "true" : "false") <= 0)
        return false;
    for (size_t i = 0; i < COUNT(ints); ++i)
        if (fprintf(out, "%s%" PRId32, i ? "," : "", ints[i]) <= 0)
            return false;
    if (fputs("],\"base_flags\":[", out) == EOF)
        return false;
    for (size_t i = 0; i < COUNT(flags); ++i)
        if (fprintf(out, "%s%u", i ? "," : "", flags[i]) <= 0)
            return false;
    return fputs("]}", out) != EOF;
}

static bool am_observation_json(FILE *out, const am_run *r, size_t which) {
    const am_observation *o = &r->observations[which];
    if (fprintf(out,
                "{\"event\":\"observation\",\"which\":%zu,\"World_binding\":%zu,"
                "\"active\":%zu,\"leases\":%zu,\"code_borrows\":%zu,\"entity_borrowers\":%zu,"
                "\"live_entities\":%zu,\"entity_bytes\":%zu,\"bounds_holds\":%zu,"
                "\"vector_holds\":%zu,\"bounds_cursor\":%" PRId32 ",\"vector_cursor\":%" PRId32
                ",\"hit_calls\":%zu,\"hit_results\":%zu,\"lists\":[%zu,%zu,%zu,%zu],"
                "\"math\":[%u,%zu,%u],\"profile\":[\"%016" PRIx64 "\",%u,%s,\"%016" PRIx64
                "\",%s,\"%016" PRIx64 "\"],\"World_rng\":",
                which, o->world_bindings, o->active_calls, o->local_leases, o->code_borrows,
                o->entity_borrowers, o->live_entities, o->entity_bytes, o->bounds_holds,
                o->vector_holds, o->bounds_cursor, o->vector_cursor, o->hit_calls, o->hit_results,
                o->loaded_count, o->deferred_count, o->nearby_count, o->collision_count,
                (unsigned int)o->math.phase, o->math.holds, o->math.last_store_pc, o->uniquifier,
                o->holder_phase, o->math_present ? "true" : "false", o->math_random_state,
                o->math_cached ? "true" : "false", ad_bits(o->math_gaussian)) <= 0 ||
        !am_random_json(out, &o->world_random) || fputs(",\"context_raw\":", out) == EOF ||
        !am_hex(out, o->context, sizeof(o->context)) ||
        fprintf(out,
                ",\"cache\":{\"owned\":%s,\"map\":%zu,\"insertions\":%zu,"
                "\"index_capacity\":%zu,\"index_bytes\":%zu,\"mutation_depth\":%zu,"
                "\"direct_bytes\":%zu,\"hits\":%" PRIu64 ",\"misses\":%" PRIu64
                ",\"generic_gets\":%" PRIu64 ",\"raw_exports\":%" PRIu64 ",\"rebuilds\":%" PRIu64
                "}",
                o->cache.owned ? "true" : "false", o->cache.map_count, o->cache.insertion_count,
                o->cache.index_capacity, o->cache.index_bytes, o->cache.mutation_depth,
                o->cache.direct_bytes, o->cache.hits, o->cache.misses, o->cache.generic_gets,
                o->cache.raw_exports, o->cache.rebuilds) <= 0 ||
        fputs(",\"actors\":[", out) == EOF)
        return false;
    for (size_t i = 0; i < r->count; ++i)
        if ((i && fputc(',', out) == EOF) || !am_actor_json(out, &o->actors[i]))
            return false;
    if (fputs("],\"chunks\":[", out) == EOF)
        return false;
    for (size_t i = 0; i < AM_CHUNKS; ++i) {
        const am_chunk *c = &o->chunks[i];
        if (fprintf(out,
                    "%s{\"xz\":[%" PRId32 ",%" PRId32 "],\"identity\":\"%" PRIxPTR
                    "\",\"object\":\"%" PRIxPTR "\",\"air\":\"%016" PRIx64
                    "\",\"bytes\":%zu,\"flags\":[%u,%u,%u,%u,%u],\"last_save\":%" PRId64
                    ",\"slices\":[",
                    i ? "," : "", c->state.x, c->state.z, c->identity, c->object, c->air_hash,
                    c->allocation_bytes, (unsigned int)c->state.dirty,
                    (unsigned int)c->state.active, (unsigned int)c->state.terrain_populated,
                    (unsigned int)c->state.never_save, (unsigned int)c->state.has_entities,
                    c->state.last_save_time) <= 0)
            return false;
        for (size_t j = 0; j < 8; ++j)
            if (fprintf(out, "%s%zu", j ? "," : "", c->slices[j]) <= 0)
                return false;
        if (fputs("]}", out) == EOF)
            return false;
    }
    return fputs("]}\n", out) != EOF;
}

static bool am_error_json(FILE *out, const am_run *r, size_t tick) {
    const mc_world_entity_tick_error *e = &r->errors[tick];
    if (fprintf(out,
                "{\"event\":\"World_return\",\"tick\":%zu,\"status\":%u,\"error_status\":%u,"
                "\"child\":%u,\"pc\":[%u,%u,%u,%u,%u,%u],\"operation_hex\":",
                tick, (unsigned int)r->statuses[tick], (unsigned int)e->status,
                (unsigned int)e->child, e->bytecode, e->loop_bytecode, e->wrapper_bytecode,
                e->update_bytecode, e->presence_bytecode, e->provider_bytecode) <= 0)
        return false;
    if (e->operation != NULL) {
        if (!am_hex(out, (const unsigned char *)e->operation, strlen(e->operation)))
            return false;
    } else if (fputs("null", out) == EOF)
        return false;
    if (fprintf(out, ",\"cause\":%u,\"loop_work\":%zu,\"temporary_bytes\":%zu",
                (unsigned int)e->cause.status, e->loop_work, e->temporary_bytes) <= 0)
        return false;
    if (e->child == MC_WORLD_ENTITY_TICK_CHILD_ARROW &&
        fprintf(out, ",\"Arrow_status\":%u,\"Arrow_cleanup\":%u,\"Arrow_call_retained\":%s",
                (unsigned int)e->child_error.arrow.status,
                (unsigned int)e->child_error.arrow.cleanup_status,
                e->child_error.arrow.call != NULL ? "true" : "false") <= 0)
        return false;
    return fputs("}\n", out) != EOF;
}

static bool am_constructor_json(FILE *out, const am_run *r, size_t i) {
    const mc_arrow_error *h = &r->heading[i];
    if (fprintf(
            out,
            "{\"event\":\"constructor\",\"actor\":%zu,\"construct_status\":%u,"
            "\"heading_status\":%u,\"gaussian_calls\":%u,\"heading_returned\":%s,"
            "\"draws\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],\"rng_before\":",
            i, (unsigned int)r->constructed[i].status, (unsigned int)h->status, h->gaussian_calls,
            h->heading_returned ? "true" : "false", ad_bits(h->gaussian_values[0]),
            ad_bits(h->gaussian_values[1]), ad_bits(h->gaussian_values[2])) <= 0 ||
        !am_random_json(out, &h->random_before) || fputs(",\"rng_after\":", out) == EOF ||
        !am_random_json(out, &h->random_after))
        return false;
    return fputs("}\n", out) != EOF;
}

int main(int argc, char **argv) {
    char root[8192];
    size_t count = 0;
    if (argc != 4 || !decode_path(argv[2], root, sizeof(root)))
        return 2;
    if (strcmp(argv[1], "arrow1") == 0)
        count = 1;
    else if (strcmp(argv[1], "arrow8") == 0)
        count = 8;
    else if (strcmp(argv[1], "empty") != 0)
        return 2;
#if defined(_WIN32)
    const char *clock_name = "qpc";
#else
    const char *clock_name = "monotonic";
#endif
    FILE *out = fopen(argv[3], "wx");
    am_run *r = calloc(1, sizeof(*r));
    if (out == NULL || r == NULL)
        return 1;
    r->count = count;
    r->errors = calloc(AM_TICKS, sizeof(*r->errors));
    r->observations = calloc(2, sizeof(*r->observations));
    if (r->errors == NULL || r->observations == NULL)
        return 1;
    for (size_t tick = 0; tick < AM_TICKS; ++tick)
        mc_world_entity_tick_error_init(&r->errors[tick]);
    bool ok = am_frequency(&r->frequency) && am_setup(r, root);
    if (ok) {
        r->before_captured = am_snapshot(r, &r->observations[0]);
        ok = r->before_captured;
    }
    if (ok) {
        r->clock_started = am_clock(&r->begin);
        if (r->clock_started)
            for (size_t tick = 0; tick < AM_TICKS; ++tick) {
                ++r->attempted;
                r->statuses[tick] = mc_world_runtime_tick_entities(
                    r->setup.world->worlds[0], &tick_context, NULL, &r->errors[tick]);
                if (r->statuses[tick] != MC_WORLD_STORAGE_OK)
                    break;
                ++r->returned;
            }
        r->clock_ended = am_clock(&r->end);
        r->after_captured = am_snapshot(r, &r->observations[1]);
        ok = r->after_captured && am_validate(r);
    }
    /* All formatting/observations/checks and final retirement remain outside
     * the two clock calls. Every actual status and failed opportunity is kept. */
    char proofs[32] = "null";
    if (r->validated)
        (void)snprintf(proofs, sizeof(proofs), "%zu", count * AM_TICKS);
    bool written =
        fputs("{\"event\":\"header\",\"schema\":1,\"variant\":\"uninstrumented\","
              "\"rectangle\":[-2,2,-2,3],\"lane\":\"x8.5,y66,z0.5+4*i\","
              "\"double_fields\":[\"aE\",\"aJ\",\"aK\",\"aL\",\"x\",\"y\",\"z\","
              "\"vx\",\"vy\",\"vz\",\"bl\",\"bm\",\"bn\",\"private_d\",\"private_e\"],"
              "\"float_fields\":[\"yaw\",\"aT\",\"aU\",\"aV\",\"offset\",\"width\","
              "\"height\",\"bi\",\"bj\",\"bk\",\"y_size\",\"bp\",\"br\",\"bE\"],"
              "\"base_int_fields\":[\"id\",\"private_b\",\"bt\",\"bu\",\"bv\",\"bw\",\"by\",\"bz\","
              "\"bG\",\"bH\",\"bI\",\"bJ\",\"bK\",\"bL\"],"
              "\"base_flag_fields\":[\"aF\",\"aX\",\"aY\",\"aZ\",\"ba\",\"bb\",\"bc\","
              "\"bd\",\"be\",\"bq\",\"bx\",\"private_c\",\"bC\",\"bF\",\"bM\"],"
              "\"SL_fields\":[\"a\",\"b\",\"d\",\"e\",\"f\",\"g\",\"h\",\"i\",\"j\",\"k\"],"
              "\"float_encoding\":\"raw-IEEE-hex\",\"context_raw\":\"process-local-only\"}\n",
              out) != EOF;
    written =
        written &&
        fprintf(out,
                "{\"event\":\"interval\",\"schema\":1,\"cell\":\"%s\","
                "\"arrows\":%zu,\"chunks\":30,\"requested_calls\":8,\"attempted\":%zu,"
                "\"returned\":%zu,\"start\":%" PRId64 ",\"end\":%" PRId64 ",\"frequency\":%" PRId64
                ",\"clock\":\"%s\",\"clock_ok\":%s,\"validated\":%s,"
                "\"dispatch_path_proofs\":%s,\"allocator_observed\":false,"
                "\"attribution\":false,\"statuses\":[",
                argv[1], count, r->attempted, r->returned, r->begin, r->end, r->frequency,
                clock_name,
                r->clock_started && r->clock_ended && r->end >= r->begin ? "true" : "false",
                r->validated ? "true" : "false", proofs) > 0;
    for (size_t tick = 0; written && tick < r->attempted; ++tick)
        written = fprintf(out, "%s%u", tick ? "," : "", (unsigned int)r->statuses[tick]) > 0;
    written = written && fputs("]}\n", out) != EOF;
    if (r->before_captured)
        written = written && am_observation_json(out, r, 0);
    if (r->after_captured)
        written = written && am_observation_json(out, r, 1);
    for (size_t tick = 0; written && tick < r->attempted; ++tick)
        written = am_error_json(out, r, tick);
    if (r->setup_complete)
        for (size_t i = 0; written && i < r->count; ++i)
            written = am_constructor_json(out, r, i);
    bool cleaned = am_cleanup(r);
    written =
        written &&
        fprintf(out,
                "{\"event\":\"footer\",\"complete\":%s,\"cleanup\":%s,"
                "\"checks\":%u,\"scope\":\"uninstrumented-single-interval-no-attribution-FPS\"}\n",
                ok && cleaned ? "true" : "false", cleaned ? "true" : "false", checks) > 0;
    written = fclose(out) == 0 && written;
    const bool passed = ok && cleaned && written;
    if (cleaned) {
        free(r->observations);
        free(r->errors);
        free(r);
    }
    if (!passed) {
        fputs("Arrow baseline failed; retain inputs/raw row/actual unfinished owners\n", stderr);
        return 1;
    }
    printf("Arrow World baseline: cell=%s eight actual FD.g returns; checked cleanup\n", argv[1]);
    return 0;
}
