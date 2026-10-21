#include "mc/world/cache/lifecycle.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHUNKS 132u
#define EVENTS 4096u

typedef struct event {
    char kind;
    size_t chunk, slice;
    int64_t time;
    bool dirty, active;
} event;

typedef struct fixture {
    mc_chunk_cache cache;
    mc_chunk_cache_entry entries[CHUNKS + 4];
    mc_chunk_state *list[CHUNKS + 4];
    mc_chunk_state chunks[CHUNKS];
    mc_chunk_data data[CHUNKS];
    mc_chunk_pending pending;
    int32_t keys[CHUNKS + 4];
    mc_java_int_set *history;
    mc_chunk_lifecycle_context context;
    mc_chunk_references references;
    size_t leases[CHUNKS], holds, releases;
    size_t maximum_lease;
    mc_chunk_lifecycle_status nested_status;
    event events[EVENTS];
    size_t event_count;
    int64_t time;
    bool saving_disabled, provider_result, fail_hold;
    bool fail_entity;
    size_t failed_slice;
    mc_chunk_callback_result entity_result;
    mc_chunk_callback_result main_result, extra_result, detach_result, maintenance_result;
    mc_chunk_callback_result session_result, metadata_result, auxiliary_result, progress_result;
    unsigned mutation;
    bool mutated;
    int callback_error;
} fixture;

static fixture f;
static unsigned checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static size_t identity(const mc_chunk_state *chunk) {
    for (size_t i = 0; i < CHUNKS; ++i)
        if (chunk == &f.chunks[i])
            return i;
    return CHUNKS;
}

static void record(char kind, mc_chunk_state *chunk, size_t slice) {
    size_t id = chunk == NULL ? CHUNKS : identity(chunk);
    if (f.event_count == EVENTS || (chunk != NULL && (id == CHUNKS || f.leases[id] == 0))) {
        f.callback_error = 1;
        return;
    }
    f.events[f.event_count++] = (event){
        kind, id, slice, f.time, chunk != NULL && chunk->dirty, chunk != NULL && chunk->active};
}

static bool hold(void *owner, mc_chunk_state *chunk) {
    (void)owner;
    size_t id = identity(chunk);
    if (id == CHUNKS || f.fail_hold)
        return false;
    ++f.leases[id];
    if (f.leases[id] > f.maximum_lease)
        f.maximum_lease = f.leases[id];
    ++f.holds;
    return true;
}

static void release(void *owner, mc_chunk_state *chunk) {
    (void)owner;
    size_t id = identity(chunk);
    if (id == CHUNKS || f.leases[id] == 0) {
        f.callback_error = 1;
        return;
    }
    --f.leases[id];
    ++f.releases;
}

static mc_chunk_callback_result main_save(void *owner, const mc_chunk_lifecycle_context *context,
                                          mc_chunk_state *chunk) {
    (void)owner;
    record('m', chunk, 0);
    if (!f.mutated) {
        f.mutated = true;
        if (f.mutation == 1) {
            f.time = 1700;
            if (!mc_chunk_cache_publish(context->cache, 2, 0, &f.chunks[2]))
                f.callback_error = 1;
        } else if (f.mutation == 2) {
            memmove(f.list, f.list + 1, (--f.cache.insertion_count) * sizeof(*f.list));
        } else if (f.mutation == 3) {
            if (!mc_chunk_cache_publish(context->cache, 0, 0, &f.chunks[1]))
                f.callback_error = 1;
        } else if (f.mutation == 6) {
            bool completed = false;
            f.nested_status = mc_chunk_lifecycle_save(context, false, &completed);
            if (f.main_result == MC_CHUNK_CALLBACK_OK &&
                (f.nested_status != MC_CHUNK_LIFECYCLE_OK || !completed))
                f.callback_error = 1;
        } else if (f.mutation == 7) {
            bool result = true;
            f.nested_status = mc_chunk_lifecycle_maintain(context, &result);
            if (f.main_result == MC_CHUNK_CALLBACK_OK &&
                (f.nested_status != MC_CHUNK_LIFECYCLE_OK || result))
                f.callback_error = 1;
        }
    }
    return f.main_result;
}

static mc_chunk_callback_result extra_save(void *owner, const mc_chunk_lifecycle_context *context,
                                           mc_chunk_state *chunk) {
    (void)owner;
    (void)context;
    record('x', chunk, 0);
    if (f.mutation == 4)
        f.time = 777;
    return f.extra_result;
}

static mc_chunk_callback_result flush(void *owner, const mc_chunk_lifecycle_context *context) {
    (void)owner;
    (void)context;
    record('f', NULL, 0);
    return f.maintenance_result;
}

static mc_chunk_callback_result loader_maintenance(void *owner,
                                                   const mc_chunk_lifecycle_context *context) {
    (void)owner;
    (void)context;
    record('l', NULL, 0);
    return f.maintenance_result;
}

static mc_chunk_callback_result tiles(void *owner, const mc_chunk_lifecycle_context *context,
                                      mc_chunk_state *chunk) {
    (void)owner;
    (void)context;
    record('t', chunk, 0);
    if (f.mutation == 8 && !f.mutated) {
        f.mutated = true;
        f.nested_status = mc_chunk_lifecycle_detach(context, chunk);
        if (f.nested_status != MC_CHUNK_LIFECYCLE_OK)
            return MC_CHUNK_CALLBACK_FATAL;
    } else if (f.mutation == 9) {
        context->cache->count = context->cache->insertion_count = 0;
        if (mc_chunk_pending_clear(context->pending) != MC_CHUNK_ACQUIRE_OK)
            f.callback_error = 1;
    }
    return f.detach_result;
}

static mc_chunk_callback_result entities(void *owner, const mc_chunk_lifecycle_context *context,
                                         mc_chunk_state *chunk, size_t slice) {
    (void)owner;
    (void)context;
    record('e', chunk, slice);
    if (f.fail_entity && slice == f.failed_slice)
        return f.entity_result;
    return f.detach_result;
}

static mc_chunk_callback_result
provider_maintenance(void *owner, const mc_chunk_lifecycle_context *context, bool *result) {
    (void)owner;
    (void)context;
    record('p', NULL, 0);
    *result = f.provider_result;
    return f.maintenance_result;
}

static mc_chunk_callback_result session(void *owner, const mc_world_save_context *context) {
    (void)owner;
    (void)context;
    record('s', NULL, 0);
    return f.session_result;
}

static mc_chunk_callback_result metadata(void *owner, const mc_world_save_context *context) {
    (void)owner;
    (void)context;
    record('d', NULL, 0);
    if (f.mutation == 5)
        f.time = 1100;
    return f.metadata_result;
}

static mc_chunk_callback_result auxiliary(void *owner, const mc_world_save_context *context) {
    (void)owner;
    (void)context;
    record('a', NULL, 0);
    return f.auxiliary_result;
}

static mc_chunk_callback_result progress(void *owner, const mc_world_save_context *context,
                                         mc_world_save_stage stage) {
    (void)owner;
    (void)context;
    record(stage == MC_WORLD_SAVE_LEVEL ? 'L' : 'C', NULL, 0);
    return f.progress_result;
}

static int setup(size_t count) {
    mc_java_int_set_free(f.history);
    memset(&f, 0, sizeof(f));
    CHECK(mc_java_int_set_create(NULL, &f.history) == MC_JAVA_INT_SET_OK);
    CHECK(mc_chunk_cache_init(&f.cache, f.entries, CHUNKS + 4, f.list, CHUNKS + 4));
    CHECK(mc_chunk_pending_init_ordered(&f.pending, f.keys, CHUNKS + 4, f.history));
    f.time = 1000;
    for (size_t i = 0; i < CHUNKS; ++i) {
        f.chunks[i] =
            (mc_chunk_state){.data = &f.data[i], .x = (int32_t)i, .active = true, .dirty = true};
        if (i < count)
            CHECK(mc_chunk_cache_publish(&f.cache, (int32_t)i, 0, &f.chunks[i]));
    }
    f.references = (mc_chunk_references){NULL, hold, release};
    f.context = (mc_chunk_lifecycle_context){.cache = &f.cache,
                                             .pending = &f.pending,
                                             .world_time = &f.time,
                                             .saving_disabled = &f.saving_disabled,
                                             .policy = MC_CHUNK_ACQUIRE_BASE,
                                             .save_main = main_save,
                                             .save_extra = extra_save,
                                             .flush = flush,
                                             .loader_maintenance = loader_maintenance,
                                             .invalidate_tiles = tiles,
                                             .defer_entities = entities,
                                             .provider_maintenance = provider_maintenance,
                                             .references = &f.references};
    CHECK(mc_chunk_lifecycle_validate(&f.context));
    return 0;
}

static int balanced(void) {
    CHECK(!f.callback_error && f.holds == f.releases);
    for (size_t i = 0; i < CHUNKS; ++i)
        CHECK(f.leases[i] == 0);
    return 0;
}

static int eligibility(void) {
    mc_chunk_state chunk = {0};
    static const int64_t times[] = {0, 599, 600, 601, -1, INT64_MIN, INT64_MAX};
    for (unsigned flags = 0; flags < 128; ++flags) {
        chunk.blank = (flags & 1u) != 0;
        chunk.never_save = (flags & 2u) != 0;
        chunk.has_entities = (flags & 4u) != 0;
        chunk.dirty = (flags & 8u) != 0;
        chunk.active = (flags & 16u) != 0;
        chunk.terrain_populated = (flags & 32u) != 0;
        chunk.last_save_time = (flags & 64u) != 0 ? INT64_MAX : 0;
        for (size_t i = 0; i < sizeof(times) / sizeof(*times); ++i) {
            bool allowed = !chunk.blank && !chunk.never_save;
            bool due = chunk.last_save_time == 0 ? times[i] >= 600 : times[i] >= INT64_MIN + 599;
            CHECK(mc_chunk_save_eligible(&chunk, times[i], false) ==
                  (allowed && (chunk.dirty || (chunk.has_entities && due))));
            CHECK(mc_chunk_save_eligible(&chunk, times[i], true) ==
                  (allowed &&
                   (chunk.dirty || (chunk.has_entities && times[i] != chunk.last_save_time))));
        }
    }
    CHECK(!mc_chunk_save_eligible(NULL, 0, true));
    return 0;
}

static int save_passes(void) {
    static const size_t counts[] = {0, 23, 24, 25};
    for (size_t i = 0; i < sizeof(counts) / sizeof(*counts); ++i) {
        size_t count = counts[i];
        CHECK(setup(count) == 0);
        bool completed = true;
        CHECK(mc_chunk_lifecycle_save(&f.context, false, &completed) == MC_CHUNK_LIFECYCLE_OK);
        CHECK(completed == (count < 24) && f.event_count == (count < 24 ? count : 24));
        for (size_t j = 0; j < count; ++j) {
            CHECK(f.chunks[j].dirty == (j >= 24));
            CHECK(f.chunks[j].last_save_time == (j >= 24 ? 0 : 1000));
        }
        CHECK(balanced() == 0);
        CHECK(mc_chunk_lifecycle_save(&f.context, false, &completed) == MC_CHUNK_LIFECYCLE_OK &&
              completed);
    }
    CHECK(setup(4) == 0);
    f.chunks[0].dirty = false;
    f.chunks[1].never_save = true;
    f.chunks[2].blank = true;
    f.chunks[2].never_save = true;
    bool completed = false;
    CHECK(mc_chunk_lifecycle_save(&f.context, true, &completed) == MC_CHUNK_LIFECYCLE_OK &&
          completed);
    CHECK(f.event_count == 4 && f.events[0].kind == 'x' && f.events[0].chunk == 0);
    CHECK(f.events[1].kind == 'x' && f.events[1].chunk == 3);
    CHECK(f.events[2].kind == 'm' && f.events[2].chunk == 3 && f.events[3].kind == 'f');
    CHECK(f.chunks[1].dirty && f.chunks[2].dirty && balanced() == 0);
    CHECK(setup(1) == 0);
    f.main_result = MC_CHUNK_CALLBACK_IO_ERROR;
    CHECK(mc_chunk_lifecycle_save(&f.context, false, &completed) == MC_CHUNK_LIFECYCLE_OK);
    CHECK(!f.chunks[0].dirty && f.chunks[0].last_save_time == 1000 && balanced() == 0);
    CHECK(setup(1) == 0);
    f.main_result = MC_CHUNK_CALLBACK_FATAL;
    completed = false;
    CHECK(mc_chunk_lifecycle_save(&f.context, false, &completed) ==
          MC_CHUNK_LIFECYCLE_CALLBACK_FAILED);
    CHECK(!completed && f.chunks[0].dirty && f.chunks[0].last_save_time == 1000 && balanced() == 0);
    CHECK(setup(1) == 0);
    f.context.save_main = NULL;
    f.context.save_extra = NULL;
    f.context.flush = NULL;
    f.context.loader_maintenance = NULL;
    CHECK(mc_chunk_lifecycle_save(&f.context, false, &completed) == MC_CHUNK_LIFECYCLE_OK &&
          completed);
    CHECK(!f.chunks[0].dirty && f.chunks[0].last_save_time == 0 && f.event_count == 0 &&
          balanced() == 0);
    CHECK(setup(2) == 0);
    f.mutation = 1;
    CHECK(mc_chunk_lifecycle_save(&f.context, false, &completed) == MC_CHUNK_LIFECYCLE_OK &&
          completed);
    CHECK(f.event_count == 3 && f.chunks[0].last_save_time == 1000 &&
          f.chunks[1].last_save_time == 1700);
    CHECK(f.events[2].chunk == 2 && !f.chunks[2].dirty && balanced() == 0);
    CHECK(setup(3) == 0);
    f.mutation = 2;
    CHECK(mc_chunk_lifecycle_save(&f.context, false, &completed) == MC_CHUNK_LIFECYCLE_OK &&
          completed);
    CHECK(f.event_count == 2 && f.events[0].chunk == 0 && f.events[1].chunk == 2 &&
          f.chunks[1].dirty);
    CHECK(balanced() == 0);
    CHECK(setup(1) == 0);
    f.mutation = 4;
    f.extra_result = MC_CHUNK_CALLBACK_EXCEPTION;
    CHECK(mc_chunk_lifecycle_save(&f.context, true, &completed) == MC_CHUNK_LIFECYCLE_OK);
    CHECK(f.chunks[0].last_save_time == 777 && f.event_count == 3 && balanced() == 0);
    return 0;
}

static int add_pending(int32_t x) {
    bool changed = false;
    CHECK(mc_chunk_pending_add(&f.pending, mc_chunk_cache_key(x, 0), &changed) ==
              MC_CHUNK_ACQUIRE_OK &&
          changed);
    return 0;
}

static int unloads(void) {
    CHECK(setup(101) == 0);
    for (int32_t i = 0; i < 101; ++i)
        CHECK(add_pending(i) == 0);
    bool result = true;
    CHECK(mc_chunk_lifecycle_maintain(&f.context, &result) == MC_CHUNK_LIFECYCLE_OK && !result);
    CHECK(f.pending.count == 1 && f.cache.count == 1 && f.cache.insertion_count == 1);
    CHECK(f.event_count == 1102 && f.events[1100].kind == 'l' && f.events[1101].kind == 'p');
    for (size_t i = 0; i < 100; ++i) {
        size_t base = i * 11;
        CHECK(f.events[base].kind == 't' && !f.events[base].active);
        for (size_t slice = 0; slice < 8; ++slice)
            CHECK(f.events[base + slice + 1].kind == 'e' &&
                  f.events[base + slice + 1].slice == slice);
        CHECK(f.events[base + 9].kind == 'm' && f.events[base + 10].kind == 'x');
        CHECK(f.events[base + 9].dirty && f.events[base + 10].dirty);
    }
    CHECK(balanced() == 0);
    f.provider_result = true;
    CHECK(mc_chunk_lifecycle_maintain(&f.context, &result) == MC_CHUNK_LIFECYCLE_OK && result);
    CHECK(f.pending.count == 0 && f.cache.count == 0 && f.cache.insertion_count == 0 &&
          balanced() == 0);
    CHECK(setup(1) == 0 && add_pending(0) == 0);
    CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, &f.chunks[0]));
    CHECK(mc_chunk_cache_publish(&f.cache, 2, 0, &f.chunks[0]));
    f.mutation = 3;
    CHECK(mc_chunk_lifecycle_maintain(&f.context, &result) == MC_CHUNK_LIFECYCLE_OK);
    CHECK(f.cache.count == 1 && f.entries[0].key == mc_chunk_cache_key(2, 0) &&
          f.entries[0].chunk == &f.chunks[0]);
    CHECK(f.cache.insertion_count == 3 && f.list[0] == &f.chunks[0] && f.list[1] == &f.chunks[0] &&
          f.list[2] == &f.chunks[1]);
    CHECK(f.chunks[0].dirty && !f.chunks[0].active && balanced() == 0);
    CHECK(setup(1) == 0 && add_pending(0) == 0);
    f.context.policy = MC_CHUNK_ACQUIRE_DEDICATED;
    f.saving_disabled = true;
    f.provider_result = true;
    CHECK(!mc_chunk_lifecycle_saving_enabled(&f.context));
    CHECK(mc_chunk_lifecycle_maintain(&f.context, &result) == MC_CHUNK_LIFECYCLE_OK && result);
    CHECK(f.event_count == 1 && f.events[0].kind == 'p' && f.pending.count == 1 &&
          f.chunks[0].active);
    bool completed = false;
    CHECK(mc_chunk_lifecycle_save(&f.context, false, &completed) == MC_CHUNK_LIFECYCLE_OK &&
          completed);
    CHECK(!f.chunks[0].dirty && f.pending.count == 1 && balanced() == 0);
    CHECK(setup(1) == 0 && add_pending(0) == 0);
    f.chunks[0].blank = true;
    f.chunks[0].never_save = true;
    CHECK(mc_chunk_lifecycle_maintain(&f.context, &result) == MC_CHUNK_LIFECYCLE_OK);
    CHECK(f.event_count == 4 && f.events[0].kind == 'm' && f.events[1].kind == 'x');
    CHECK(f.chunks[0].active && f.chunks[0].dirty && balanced() == 0);
    CHECK(setup(1) == 0 && add_pending(0) == 0);
    f.detach_result = MC_CHUNK_CALLBACK_EXCEPTION;
    result = true;
    CHECK(mc_chunk_lifecycle_maintain(&f.context, &result) == MC_CHUNK_LIFECYCLE_CALLBACK_FAILED &&
          result);
    CHECK(!f.chunks[0].active && f.pending.count == 1 && f.cache.count == 1 && f.event_count == 1 &&
          balanced() == 0);
    CHECK(setup(0) == 0 && add_pending(5) == 0);
    CHECK(mc_chunk_lifecycle_maintain(&f.context, &result) == MC_CHUNK_LIFECYCLE_STATE_ERROR);
    CHECK(f.pending.count == 1 && f.event_count == 0 && balanced() == 0);
    CHECK(setup(1) == 0 && add_pending(0) == 0);
    f.fail_hold = true;
    CHECK(mc_chunk_lifecycle_maintain(&f.context, &result) == MC_CHUNK_LIFECYCLE_LIMIT);
    CHECK(f.pending.count == 1 && f.chunks[0].active && f.event_count == 0 && balanced() == 0);
    return 0;
}

static int recursive_lifetimes(void) {
    bool completed = false;
    CHECK(setup(1) == 0);
    f.mutation = 6;
    CHECK(mc_chunk_lifecycle_save(&f.context, false, &completed) == MC_CHUNK_LIFECYCLE_OK &&
          completed);
    CHECK(f.event_count == 2 && f.maximum_lease == 2 && !f.chunks[0].dirty && balanced() == 0);
    CHECK(setup(1) == 0);
    f.mutation = 6;
    f.main_result = MC_CHUNK_CALLBACK_FATAL;
    CHECK(mc_chunk_lifecycle_save(&f.context, false, &completed) ==
          MC_CHUNK_LIFECYCLE_CALLBACK_FAILED);
    CHECK(f.nested_status == MC_CHUNK_LIFECYCLE_CALLBACK_FAILED && f.event_count == 2);
    CHECK(f.maximum_lease == 2 && f.chunks[0].dirty && balanced() == 0);
    CHECK(setup(1) == 0 && add_pending(0) == 0);
    f.mutation = 7;
    bool result = true;
    CHECK(mc_chunk_lifecycle_maintain(&f.context, &result) == MC_CHUNK_LIFECYCLE_OK && !result);
    CHECK(f.maximum_lease == 2 && f.event_count == 26 && f.pending.count == 0);
    CHECK(f.cache.count == 0 && f.cache.insertion_count == 0 && f.chunks[0].dirty &&
          balanced() == 0);
    CHECK(setup(1) == 0 && add_pending(0) == 0);
    f.mutation = 7;
    f.main_result = MC_CHUNK_CALLBACK_FATAL;
    CHECK(mc_chunk_lifecycle_maintain(&f.context, &result) == MC_CHUNK_LIFECYCLE_CALLBACK_FAILED);
    CHECK(f.nested_status == MC_CHUNK_LIFECYCLE_CALLBACK_FAILED && f.maximum_lease == 2 &&
          f.event_count == 20);
    CHECK(f.pending.count == 1 && f.cache.count == 1 && !f.chunks[0].active && f.chunks[0].dirty &&
          balanced() == 0);
    return 0;
}

static int direct_detachment(void) {
    CHECK(setup(1) == 0 && add_pending(0) == 0);
    f.chunks[0].never_save = f.chunks[0].has_entities = true;
    f.chunks[0].last_save_time = -77;
    f.data[0].heightmap[7] = 143;
    mc_chunk_data saved;
    memcpy(&saved, &f.data[0], sizeof(saved));
    f.context.policy = MC_CHUNK_ACQUIRE_DEDICATED;
    f.saving_disabled = true;
    CHECK(mc_chunk_lifecycle_detach(&f.context, &f.chunks[0]) == MC_CHUNK_LIFECYCLE_OK);
    CHECK(!f.chunks[0].active && f.chunks[0].dirty && f.chunks[0].never_save &&
          f.chunks[0].has_entities && f.chunks[0].last_save_time == -77);
    CHECK(f.cache.count == 1 && f.cache.insertion_count == 1 && f.pending.count == 1 &&
          f.entries[0].chunk == &f.chunks[0] && f.list[0] == &f.chunks[0]);
    CHECK(memcmp(&saved, &f.data[0], sizeof(saved)) == 0 && f.event_count == 9 &&
          f.events[0].kind == 't' && !f.events[0].active && f.holds == 1 && f.maximum_lease == 1);
    for (size_t slice = 0; slice < 8; ++slice)
        CHECK(f.events[slice + 1].kind == 'e' && f.events[slice + 1].slice == slice &&
              !f.events[slice + 1].active && f.events[slice + 1].dirty);
    CHECK(balanced() == 0);

    CHECK(setup(0) == 0);
    CHECK(mc_chunk_lifecycle_detach(&f.context, &f.chunks[0]) == MC_CHUNK_LIFECYCLE_OK &&
          f.cache.count == 0 && f.event_count == 9 && balanced() == 0);
    CHECK(setup(1) == 0);
    f.chunks[0].blank = true;
    f.chunks[0].data = NULL;
    mc_chunk_state blank;
    memcpy(&blank, &f.chunks[0], sizeof(blank));
    CHECK(mc_chunk_lifecycle_detach(&f.context, &f.chunks[0]) == MC_CHUNK_LIFECYCLE_OK &&
          memcmp(&blank, &f.chunks[0], sizeof(blank)) == 0 && f.event_count == 0 &&
          balanced() == 0);

    CHECK(setup(1) == 0);
    CHECK(mc_chunk_lifecycle_detach(NULL, &f.chunks[0]) == MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT);
    CHECK(mc_chunk_lifecycle_detach(&f.context, NULL) == MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT);
    f.context.invalidate_tiles = NULL;
    CHECK(mc_chunk_lifecycle_detach(&f.context, &f.chunks[0]) ==
          MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT);
    f.context.invalidate_tiles = tiles;
    f.context.defer_entities = NULL;
    CHECK(mc_chunk_lifecycle_detach(&f.context, &f.chunks[0]) ==
          MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT);
    f.context.defer_entities = entities;
    f.chunks[0].data = NULL;
    CHECK(mc_chunk_lifecycle_detach(&f.context, &f.chunks[0]) ==
          MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT);
    CHECK(f.chunks[0].active && !f.holds && !f.releases && !f.event_count);
    CHECK(setup(1) == 0);
    f.fail_hold = true;
    CHECK(mc_chunk_lifecycle_detach(&f.context, &f.chunks[0]) == MC_CHUNK_LIFECYCLE_LIMIT &&
          f.chunks[0].active && !f.event_count && balanced() == 0);

    for (unsigned result = MC_CHUNK_CALLBACK_IO_ERROR; result <= MC_CHUNK_CALLBACK_FATAL;
         ++result) {
        CHECK(setup(1) == 0);
        f.detach_result = (mc_chunk_callback_result)result;
        CHECK(mc_chunk_lifecycle_detach(&f.context, &f.chunks[0]) ==
                  MC_CHUNK_LIFECYCLE_CALLBACK_FAILED &&
              !f.chunks[0].active && f.chunks[0].dirty && f.event_count == 1 &&
              f.cache.count == 1 && balanced() == 0);
        for (size_t slice = 0; slice < 8; ++slice) {
            CHECK(setup(1) == 0);
            f.fail_entity = true;
            f.failed_slice = slice;
            f.entity_result = (mc_chunk_callback_result)result;
            CHECK(mc_chunk_lifecycle_detach(&f.context, &f.chunks[0]) ==
                      MC_CHUNK_LIFECYCLE_CALLBACK_FAILED &&
                  !f.chunks[0].active && f.chunks[0].dirty && f.event_count == slice + 2 &&
                  f.events[slice + 1].slice == slice && f.cache.insertion_count == 1 &&
                  balanced() == 0);
        }
    }
    CHECK(setup(1) == 0);
    f.mutation = 8;
    CHECK(mc_chunk_lifecycle_detach(&f.context, &f.chunks[0]) == MC_CHUNK_LIFECYCLE_OK &&
          f.nested_status == MC_CHUNK_LIFECYCLE_OK && f.event_count == 18 && f.maximum_lease == 2 &&
          balanced() == 0);
    CHECK(setup(1) == 0);
    f.mutation = 8;
    f.fail_entity = true;
    f.failed_slice = 3;
    f.entity_result = MC_CHUNK_CALLBACK_FATAL;
    CHECK(mc_chunk_lifecycle_detach(&f.context, &f.chunks[0]) ==
              MC_CHUNK_LIFECYCLE_CALLBACK_FAILED &&
          f.nested_status == MC_CHUNK_LIFECYCLE_CALLBACK_FAILED && f.event_count == 6 &&
          f.maximum_lease == 2 && balanced() == 0);
    CHECK(setup(1) == 0 && add_pending(0) == 0);
    f.mutation = 9;
    CHECK(mc_chunk_lifecycle_detach(&f.context, &f.chunks[0]) == MC_CHUNK_LIFECYCLE_OK &&
          f.cache.count == 0 && f.cache.insertion_count == 0 && f.pending.count == 0 &&
          f.event_count == 9 && f.maximum_lease == 1 && balanced() == 0);
    return 0;
}

static int world_saves(void) {
    CHECK(setup(25) == 0);
    mc_world_save_context context = {.chunks = &f.context,
                                     .check_session = session,
                                     .save_metadata = metadata,
                                     .save_auxiliary = auxiliary,
                                     .report_progress = progress};
    f.mutation = 5;
    CHECK(mc_world_save_validate(&context));
    CHECK(mc_world_lifecycle_save(&context, false) == MC_CHUNK_LIFECYCLE_OK);
    CHECK(f.event_count == 29 && f.events[0].kind == 'L' && f.events[1].kind == 's');
    CHECK(f.events[2].kind == 'd' && f.events[2].time == 1000);
    CHECK(f.events[3].kind == 'a' && f.events[3].time == 1100 && f.events[4].kind == 'C');
    CHECK(f.chunks[24].dirty && f.chunks[23].last_save_time == 1100 && balanced() == 0);
    CHECK(setup(25) == 0);
    bool completed = true;
    CHECK(mc_world_lifecycle_save_client(&context, 0, &completed) == MC_CHUNK_LIFECYCLE_OK &&
          !completed);
    CHECK(f.event_count == 27 && f.events[0].kind == 's' && f.events[1].kind == 'd' &&
          f.events[2].kind == 'a');
    CHECK(mc_world_lifecycle_save_client(&context, -1, &completed) == MC_CHUNK_LIFECYCLE_OK &&
          completed);
    CHECK(f.event_count == 28 && f.events[27].kind == 'm' && f.events[27].chunk == 24 &&
          balanced() == 0);
    CHECK(setup(1) == 0);
    f.context.policy = MC_CHUNK_ACQUIRE_DEDICATED;
    f.saving_disabled = true;
    CHECK(mc_world_lifecycle_save(&context, true) == MC_CHUNK_LIFECYCLE_OK);
    CHECK(f.event_count == 0 && f.chunks[0].dirty && balanced() == 0);
    CHECK(mc_world_lifecycle_save_client(&context, 0, &completed) ==
          MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT);
    CHECK(setup(1) == 0);
    f.metadata_result = MC_CHUNK_CALLBACK_IO_ERROR;
    CHECK(mc_world_lifecycle_save(&context, true) == MC_CHUNK_LIFECYCLE_CALLBACK_FAILED);
    CHECK(f.event_count == 3 && f.events[2].kind == 'd' && f.chunks[0].dirty && balanced() == 0);
    CHECK(setup(1) == 0);
    f.session_result = MC_CHUNK_CALLBACK_EXCEPTION;
    completed = false;
    CHECK(mc_world_lifecycle_save_client(&context, 0, &completed) ==
              MC_CHUNK_LIFECYCLE_CALLBACK_FAILED &&
          !completed);
    CHECK(f.event_count == 1 && f.events[0].kind == 's' && f.chunks[0].dirty && balanced() == 0);
    CHECK(setup(1) == 0);
    f.progress_result = MC_CHUNK_CALLBACK_FATAL;
    CHECK(mc_world_lifecycle_save(&context, false) == MC_CHUNK_LIFECYCLE_CALLBACK_FAILED);
    CHECK(f.event_count == 1 && f.events[0].kind == 'L' && f.chunks[0].dirty && balanced() == 0);
    context.save_auxiliary = NULL;
    CHECK(!mc_world_save_validate(&context));
    CHECK(mc_world_lifecycle_save(&context, true) == MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT);
    return 0;
}

int main(void) {
    int result = eligibility() != 0 || save_passes() != 0 || unloads() != 0 ||
                 recursive_lifetimes() != 0 || direct_detachment() != 0 || world_saves() != 0;
    mc_java_int_set_free(f.history);
    if (!result)
        printf("PASS: %u constructed lifecycle contract checks\n", checks);
    return result;
}
