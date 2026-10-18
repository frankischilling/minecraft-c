#include "mc/world/cache/acquire.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Constructed callback contracts, including deliberate recursion/failures.
 * Genuine ok/jb/he observations establish the ordering; these cases do not
 * claim that their injected callbacks occur in normal gameplay. */
static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

enum { CHUNKS = 16, MAP_CAPACITY = 32, LIST_CAPACITY = 64, PENDING_CAPACITY = 16 };
typedef enum generate_action { GENERATE_PLAIN, GENERATE_FILL, GENERATE_RECURSE } generate_action;
typedef enum population_action {
    POPULATE_PLAIN,
    POPULATE_REMOVE_LATER,
    POPULATE_REPLACE_LATER,
    POPULATE_REPLACE_SELF,
    POPULATE_RECURSE
} population_action;
typedef struct event {
    char kind;
    int32_t x, z;
    size_t slice, map_count, list_count;
    mc_chunk_state *chunk;
    bool active, populated, dirty;
} event;
typedef struct fixture {
    mc_chunk_data data[CHUNKS];
    uint8_t blocks[CHUNKS][MC_CHUNK_BLOCKS + 3u];
    mc_chunk_state chunks[CHUNKS];
    mc_chunk_cache_entry entries[MAP_CAPACITY];
    mc_chunk_state *insertions[LIST_CAPACITY];
    int32_t keys[PENDING_CAPACITY];
    mc_chunk_cache cache;
    mc_chunk_pending pending;
    mc_chunk_acquire_context context;
    int64_t time;
    int32_t spawn_x, spawn_z;
    bool world_force, provider_force;
    int world_owner, loader_owner, provider_owner;
    mc_chunk_load_result load_result;
    mc_chunk_state *loaded, *generated;
    generate_action generate_action;
    population_action population_action;
    bool bump_time, generate_ok, populate_ok, tiles_ok, callback_error, invalidate_cache;
    size_t fail_slice, generate_calls, population_calls, depth;
    int32_t requested_key;
    mc_chunk_acquire_status recursive_status;
    mc_chunk_state *recursive_result;
    event events[128];
    size_t event_count;
} fixture;

/* Large payload owners and snapshots stay off the MSVC default stack. */
static fixture f, saved;

static bool callback_check(bool condition)
{
    ++checks;
    if (!condition)
        f.callback_error = true;
    return condition;
}

static void log_event(char kind, int32_t x, int32_t z, mc_chunk_state *chunk, size_t slice)
{
    if (!callback_check(f.event_count < sizeof(f.events) / sizeof(f.events[0])))
        return;
    event *entry = &f.events[f.event_count++];
    *entry = (event){.kind = kind,
                     .x = x,
                     .z = z,
                     .slice = slice,
                     .map_count = f.cache.count,
                     .list_count = f.cache.insertion_count,
                     .chunk = chunk,
                     .active = chunk != NULL && chunk->active,
                     .populated = chunk != NULL && chunk->terrain_populated,
                     .dirty = chunk != NULL && chunk->dirty};
}

static mc_chunk_load_result load_chunk(void *owner, const mc_chunk_acquire_context *context,
                                       int32_t x, int32_t z, mc_chunk_state **loaded)
{
    if (!callback_check(owner == &f.loader_owner && context == &f.context && loaded != NULL))
        return MC_CHUNK_LOAD_FATAL;
    log_event('L', x, z, NULL, 0);
    f.requested_key = mc_chunk_cache_key(x, z);
    if (f.bump_time)
        f.time = 777;
    if (f.load_result == MC_CHUNK_LOAD_LOADED)
        *loaded = f.loaded;
    return f.load_result;
}

static bool generate_chunk(void *owner, const mc_chunk_acquire_context *context, int32_t x,
                           int32_t z, mc_chunk_state **generated)
{
    if (!callback_check(owner == &f.provider_owner && context == &f.context && generated != NULL))
        return false;
    ++f.generate_calls;
    log_event('G', x, z, NULL, 0);
    f.requested_key = mc_chunk_cache_key(x, z);
    if (f.generate_action == GENERATE_FILL) {
        f.chunks[2].dirty = true;
        if (!callback_check(mc_chunk_cache_publish(context->cache, 99, 99, &f.chunks[2])))
            return false;
    }
    if (f.generate_action == GENERATE_RECURSE && f.depth == 0) {
        ++f.depth;
        f.recursive_result = &f.chunks[14];
        /* Same packed key through a different requested coordinate. */
        f.recursive_status = mc_chunk_acquire(context, x + 32768, z, &f.recursive_result);
        --f.depth;
        if (!callback_check(f.recursive_status == MC_CHUNK_ACQUIRE_OK))
            return false;
    }
    if (f.invalidate_cache)
        context->cache->count = context->cache->capacity + 1u;
    *generated = f.depth != 0 ? &f.chunks[1] : f.generated;
    return f.generate_ok;
}

static bool register_tiles(void *owner, const mc_chunk_acquire_context *context,
                           mc_chunk_state *chunk)
{
    bool published = false;
    for (size_t i = 0; i < f.cache.count; ++i) {
        if (f.entries[i].key == f.requested_key && f.entries[i].chunk == chunk)
            published = true;
    }
    if (!callback_check(owner == &f.world_owner && context == &f.context && chunk->active &&
                        published && f.cache.insertion_count > 0 &&
                        f.insertions[f.cache.insertion_count - 1u] == chunk))
        return false;
    if (f.context.load == load_chunk && f.load_result == MC_CHUNK_LOAD_LOADED &&
        !callback_check(chunk->last_save_time == f.time))
        return false;
    log_event('T', 0, 0, chunk, 0);
    return f.tiles_ok;
}

static bool register_entities(void *owner, const mc_chunk_acquire_context *context,
                              mc_chunk_state *chunk, size_t slice)
{
    if (!callback_check(owner == &f.world_owner && context == &f.context && chunk->active &&
                        slice < 8u))
        return false;
    log_event('E', 0, 0, chunk, slice);
    return slice != f.fail_slice;
}

static bool remove_mapping(mc_chunk_cache *cache, int32_t x, int32_t z)
{
    int32_t key = mc_chunk_cache_key(x, z);
    for (size_t i = 0; i < cache->count; ++i) {
        if (cache->entries[i].key == key) {
            memmove(cache->entries + i, cache->entries + i + 1u,
                    (cache->count - i - 1u) * sizeof(*cache->entries));
            --cache->count;
            return true;
        }
    }
    return false;
}

static bool populate_chunk(void *owner, const mc_chunk_acquire_context *context, int32_t x,
                           int32_t z)
{
    mc_chunk_state *chunk = NULL;
    if (!callback_check(owner == &f.provider_owner && context == &f.context &&
                        mc_chunk_cache_lookup(context->cache, x, z, &chunk) &&
                        chunk->terrain_populated))
        return false;
    ++f.population_calls;
    log_event('P', x, z, chunk, 0);
    if (f.population_calls == 1u) {
        if (f.population_action == POPULATE_REMOVE_LATER &&
            !callback_check(remove_mapping(context->cache, -1, 1)))
            return false;
        if (f.population_action == POPULATE_REPLACE_LATER) {
            f.chunks[9].terrain_populated = true;
            if (!callback_check(mc_chunk_cache_publish(context->cache, -1, 0, &f.chunks[9])))
                return false;
        }
        if (f.population_action == POPULATE_REPLACE_SELF &&
            !callback_check(mc_chunk_cache_publish(context->cache, x, z, &f.chunks[9])))
            return false;
        if (f.population_action == POPULATE_RECURSE) {
            f.recursive_status = mc_chunk_acquire_populate(context, x, z);
            if (!callback_check(f.recursive_status == MC_CHUNK_ACQUIRE_OK))
                return false;
        }
    }
    return f.populate_ok;
}

static int reset_fixture(void)
{
    memset(&f, 0, sizeof(f));
    for (size_t i = 0; i < CHUNKS; ++i) {
        f.chunks[i].data = &f.data[i];
        f.chunks[i].x = (int32_t)i + 71;
        f.chunks[i].z = -(int32_t)i - 33;
        f.chunks[i].never_save = (i & 1u) != 0;
        f.chunks[i].has_entities = (i & 2u) != 0;
        f.chunks[i].last_save_time = (int64_t)i + 123;
        f.data[i].blocks = f.blocks[i];
        f.data[i].blocks_capacity = sizeof(f.blocks[i]);
        f.data[i].minimum_height = (int32_t)i;
        memset(f.blocks[i], (int)(i + 1u), sizeof(f.blocks[i]));
        memset(f.data[i].metadata, (int)(i + 1u), sizeof(f.data[i].metadata));
        memset(f.data[i].skylight, (int)(i + 1u), sizeof(f.data[i].skylight));
        memset(f.data[i].block_light, (int)(i + 1u), sizeof(f.data[i].block_light));
        memset(f.data[i].heightmap, (int)i, sizeof(f.data[i].heightmap));
    }
    f.chunks[15].data = NULL;
    f.chunks[15].blank = true;
    f.chunks[0].dirty = true; /* Constructed finished normal generator result. */
    CHECK(mc_chunk_cache_init(&f.cache, f.entries, MAP_CAPACITY, f.insertions, LIST_CAPACITY));
    CHECK(mc_chunk_pending_init(&f.pending, f.keys, PENDING_CAPACITY));
    f.time = 456;
    f.generated = &f.chunks[0];
    f.loaded = &f.chunks[1];
    f.load_result = MC_CHUNK_LOAD_MISS;
    f.generate_ok = f.populate_ok = f.tiles_ok = true;
    f.fail_slice = SIZE_MAX;
    f.context = (mc_chunk_acquire_context){.cache = &f.cache,
                                           .pending = &f.pending,
                                           .fallback = &f.chunks[15],
                                           .world_time = &f.time,
                                           .world_force_load = &f.world_force,
                                           .provider_force_load = &f.provider_force,
                                           .spawn_x = &f.spawn_x,
                                           .spawn_z = &f.spawn_z,
                                           .policy = MC_CHUNK_ACQUIRE_BASE,
                                           .world = &f.world_owner,
                                           .loader = &f.loader_owner,
                                           .provider = &f.provider_owner,
                                           .generate = generate_chunk,
                                           .register_tiles = register_tiles,
                                           .register_entities = register_entities,
                                           .populate = populate_chunk};
    CHECK(mc_chunk_acquire_validate(&f.context));
    return 0;
}

static void add_pending(int32_t x, int32_t z)
{
    f.keys[f.pending.count++] = mc_chunk_cache_key(x, z);
}

static int expect_activation(size_t first, mc_chunk_state *chunk, size_t count)
{
    CHECK(f.events[first].kind == 'T' && f.events[first].chunk == chunk);
    for (size_t i = 0; i < 8u; ++i) {
        const event *entry = &f.events[first + i + 1u];
        CHECK(entry->kind == 'E' && entry->slice == i && entry->chunk == chunk && entry->active);
        CHECK(entry->map_count == count && entry->list_count == count);
    }
    CHECK(!f.callback_error);
    return 0;
}

static int test_dedicated_gate_and_hit_pending(void)
{
    CHECK(reset_fixture() == 0);
    f.context.policy = MC_CHUNK_ACQUIRE_DEDICATED;
    add_pending(9, 0);
    memcpy(&saved, &f, sizeof(f));
    mc_chunk_state *out = &f.chunks[14];
    CHECK(mc_chunk_acquire_get(&f.context, 9, 0, &out) == MC_CHUNK_ACQUIRE_OK);
    CHECK(out == &f.chunks[15] && memcmp(&f, &saved, sizeof(f)) == 0);
    f.world_force = true;
    CHECK(mc_chunk_acquire_get(&f.context, 9, 0, &out) == MC_CHUNK_ACQUIRE_OK);
    CHECK(out == &f.chunks[0] && f.pending.count == 0u && f.cache.count == 1u);
    CHECK(f.event_count == 10u && f.events[0].kind == 'G');
    CHECK(expect_activation(1u, out, 1u) == 0);
    add_pending(9, 0);
    f.world_force = false;
    f.cache.insertion_capacity = f.cache.insertion_count;
    memcpy(&saved, &f, sizeof(f));
    CHECK(mc_chunk_acquire_get(&f.context, 32777, 0, &out) == MC_CHUNK_ACQUIRE_OK);
    CHECK(out == &f.chunks[0] && memcmp(&f, &saved, sizeof(f)) == 0);
    add_pending(44, 55);
    CHECK(mc_chunk_acquire(&f.context, 32777, 0, &out) == MC_CHUNK_ACQUIRE_OK);
    CHECK(f.pending.count == 1u && f.keys[0] == mc_chunk_cache_key(44, 55));
    CHECK(f.event_count == 10u && f.cache.insertion_count == 1u);

    CHECK(reset_fixture() == 0);
    f.context.policy = MC_CHUNK_ACQUIRE_DEDICATED;
    f.provider_force = true;
    CHECK(mc_chunk_acquire_get(&f.context, 1, 0, &out) == MC_CHUNK_ACQUIRE_OK);
    CHECK(out == &f.chunks[0] && f.generate_calls == 1u);
    CHECK(reset_fixture() == 0);
    f.context.world_force_load = f.context.provider_force_load = NULL;
    f.context.spawn_x = f.context.spawn_z = NULL;
    CHECK(mc_chunk_acquire_get(&f.context, 1, 0, &out) == MC_CHUNK_ACQUIRE_OK);
    CHECK(out == &f.chunks[0]); /* Base ignores dedicated gate resources. */
    return 0;
}

static int test_pending_and_drop_arithmetic(void)
{
    CHECK(reset_fixture() == 0);
    CHECK(!mc_chunk_pending_init(NULL, f.keys, PENDING_CAPACITY));
    memcpy(&saved, &f, sizeof(f));
    CHECK(!mc_chunk_pending_init(&f.pending, NULL, 1u));
    CHECK(memcmp(&saved, &f, sizeof(f)) == 0);
    CHECK(!mc_chunk_pending_contains(NULL, 0));
    f.context.policy = MC_CHUNK_ACQUIRE_DEDICATED;
    f.spawn_x = f.spawn_z = 8;
    CHECK(mc_chunk_acquire_mark_drop(&f.context, -8, 8) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_acquire_mark_drop(&f.context, 8, -8) == MC_CHUNK_ACQUIRE_OK);
    CHECK(f.pending.count == 0u); /* Both exact +/-128 boundaries are retained. */
    CHECK(mc_chunk_acquire_mark_drop(&f.context, 9, 0) == MC_CHUNK_ACQUIRE_OK);
    CHECK(f.pending.count == 1u && f.keys[0] == 589824);
    f.pending.capacity = 1u;
    memcpy(&saved, &f, sizeof(f));
    CHECK(mc_chunk_acquire_mark_drop(&f.context, 32777, 0) == MC_CHUNK_ACQUIRE_OK);
    CHECK(memcmp(&saved, &f, sizeof(f)) == 0); /* Alias already present at capacity. */
    CHECK(mc_chunk_acquire_mark_drop(&f.context, 0, 9) == MC_CHUNK_ACQUIRE_LIMIT);
    CHECK(memcmp(&saved, &f, sizeof(f)) == 0);
    CHECK(reset_fixture() == 0);
    f.context.policy = MC_CHUNK_ACQUIRE_DEDICATED;
    f.spawn_x = -8;
    f.spawn_z = 8;
    CHECK(mc_chunk_acquire_mark_drop(&f.context, INT32_MAX, 0) == MC_CHUNK_ACQUIRE_OK);
    f.spawn_x = 8;
    CHECK(mc_chunk_acquire_mark_drop(&f.context, INT32_MIN, 0) == MC_CHUNK_ACQUIRE_OK);
    CHECK(f.pending.count == 0u); /* Wrapped products, not widened midpoint arithmetic. */
    CHECK(mc_chunk_acquire_mark_drop(&f.context, 0, -9) == MC_CHUNK_ACQUIRE_OK);
    CHECK(f.keys[0] == mc_chunk_cache_key(0, -9));
    f.keys[1] = f.keys[0];
    f.pending.count = 2u;
    CHECK(!mc_chunk_pending_validate(&f.pending));
    memcpy(&saved, &f, sizeof(f));
    CHECK(mc_chunk_acquire_mark_drop(&f.context, 1, 1) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&saved, &f, sizeof(f)) == 0);
    return 0;
}

static int test_load_generate_and_activation(void)
{
    mc_chunk_state *out = NULL;
    CHECK(reset_fixture() == 0);
    f.context.load = load_chunk;
    f.load_result = MC_CHUNK_LOAD_LOADED;
    f.bump_time = true;
    f.chunks[1].never_save = f.chunks[1].has_entities = true;
    memcpy(&saved, &f, sizeof(f));
    CHECK(mc_chunk_acquire(&f.context, -7, 3, &out) == MC_CHUNK_ACQUIRE_OK);
    CHECK(out == &f.chunks[1] && out->last_save_time == 777 && f.generate_calls == 0u);
    CHECK(f.event_count == 10u && f.events[0].kind == 'L');
    CHECK(expect_activation(1u, out, 1u) == 0);
    CHECK(out->active && !out->dirty && !out->terrain_populated && out->never_save &&
          out->has_entities);
    CHECK(out->x == saved.chunks[1].x && out->z == saved.chunks[1].z &&
          out->data == saved.chunks[1].data);
    CHECK(memcmp(f.data, saved.data, sizeof(f.data)) == 0);
    CHECK(memcmp(f.blocks, saved.blocks, sizeof(f.blocks)) == 0);
    CHECK(memcmp(&f.chunks[2], &saved.chunks[2], sizeof(f.chunks) - 2u * sizeof(f.chunks[0])) == 0);
    for (int route = 0; route < 2; ++route) {
        CHECK(reset_fixture() == 0);
        f.context.load = load_chunk;
        f.load_result = route == 0 ? MC_CHUNK_LOAD_MISS : MC_CHUNK_LOAD_CAUGHT_ERROR;
        f.bump_time = true;
        int64_t old_time = f.chunks[0].last_save_time;
        CHECK(mc_chunk_acquire(&f.context, 0, 0, &out) == MC_CHUNK_ACQUIRE_OK);
        CHECK(out == &f.chunks[0] && out->last_save_time == old_time && f.time == 777);
        CHECK(f.events[0].kind == 'L' && f.events[1].kind == 'G' && f.event_count == 11u);
        CHECK(expect_activation(2u, out, 1u) == 0);
    }
    CHECK(reset_fixture() == 0);
    f.context.generate = NULL;
    f.context.populate = NULL;
    memcpy(&saved, &f, sizeof(f));
    CHECK(mc_chunk_acquire(&f.context, 1234, -5678, &out) == MC_CHUNK_ACQUIRE_OK);
    CHECK(out == &f.chunks[15] && f.cache.count == 1u && f.insertions[0] == out &&
          f.event_count == 0u);
    CHECK(memcmp(f.chunks, saved.chunks, sizeof(f.chunks)) == 0);
    CHECK(memcmp(f.data, saved.data, sizeof(f.data)) ==
          0); /* Blank virtual activation is a no-op. */
    CHECK(memcmp(f.blocks, saved.blocks, sizeof(f.blocks)) == 0);
    return 0;
}

static int build_grid(void)
{
    size_t chunk = 1u;
    for (int32_t z = -1; z <= 1; ++z) {
        for (int32_t x = -1; x <= 1; ++x) {
            if (x == 0 && z == 0)
                continue;
            CHECK(mc_chunk_cache_publish(&f.cache, x, z, &f.chunks[chunk++]));
        }
    }
    return 0;
}

static int expect_population(const int32_t (*coordinates)[2], size_t count)
{
    size_t found = 0;
    for (size_t i = 0; i < f.event_count; ++i) {
        const event *entry = &f.events[i];
        if (entry->kind != 'P')
            continue;
        CHECK(found < count && entry->x == coordinates[found][0] &&
              entry->z == coordinates[found][1]);
        CHECK(entry->populated);
        ++found;
    }
    CHECK(found == count && !f.callback_error);
    return 0;
}

static int test_live_population_gates(void)
{
    static const int32_t all[][2] = {{0, 0}, {-1, 0}, {0, -1}, {-1, -1}};
    static const int32_t changed[][2] = {{0, 0}, {0, -1}, {-1, -1}};
    mc_chunk_state *out = NULL;
    CHECK(reset_fixture() == 0);
    CHECK(build_grid() == 0);
    memcpy(&saved, &f, sizeof(f));
    CHECK(mc_chunk_acquire(&f.context, 0, 0, &out) == MC_CHUNK_ACQUIRE_OK);
    CHECK(expect_population(all, 4u) == 0 && f.population_calls == 4u);
    CHECK(f.events[0].kind == 'G' && f.events[1].kind == 'T' && f.events[10].kind == 'P');
    CHECK(f.cache.count == 9u && f.cache.insertion_count == 9u);
    CHECK(memcmp(f.data, saved.data, sizeof(f.data)) == 0);
    CHECK(memcmp(f.blocks, saved.blocks, sizeof(f.blocks)) == 0);
    for (size_t i = 0; i < CHUNKS; ++i) {
        CHECK(f.chunks[i].never_save == saved.chunks[i].never_save &&
              f.chunks[i].has_entities == saved.chunks[i].has_entities &&
              f.chunks[i].last_save_time == saved.chunks[i].last_save_time &&
              f.chunks[i].data == saved.chunks[i].data);
    }
    for (int mutation = 0; mutation < 2; ++mutation) {
        CHECK(reset_fixture() == 0);
        CHECK(build_grid() == 0);
        f.population_action = mutation == 0 ? POPULATE_REMOVE_LATER : POPULATE_REPLACE_LATER;
        CHECK(mc_chunk_acquire(&f.context, 0, 0, &out) == MC_CHUNK_ACQUIRE_OK);
        CHECK(expect_population(changed, 3u) == 0);
        CHECK(mc_chunk_cache_validate(&f.cache));
        mc_chunk_state *west = NULL;
        CHECK(mc_chunk_cache_lookup(&f.cache, -1, 0, &west));
        CHECK(mutation == 0 ? !west->terrain_populated : west == &f.chunks[9]);
    }
    return 0;
}

static int test_population_flags_virtuals_and_reentry(void)
{
    CHECK(reset_fixture() == 0);
    f.chunks[0].dirty = false;
    CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, &f.chunks[0]));
    f.population_action = POPULATE_RECURSE;
    CHECK(mc_chunk_acquire_populate(&f.context, 0, 0) == MC_CHUNK_ACQUIRE_OK);
    CHECK(f.population_calls == 1u && f.recursive_status == MC_CHUNK_ACQUIRE_OK);
    CHECK(f.events[0].populated && !f.events[0].dirty && f.chunks[0].dirty);
    memcpy(&saved, &f, sizeof(f));
    CHECK(mc_chunk_acquire_populate(&f.context, 32768, 0) == MC_CHUNK_ACQUIRE_OK);
    CHECK(memcmp(&f, &saved, sizeof(f)) == 0); /* Populated aliases do not call provider again. */

    CHECK(reset_fixture() == 0);
    f.chunks[0].dirty = false;
    CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, &f.chunks[0]));
    f.population_action = POPULATE_REPLACE_SELF;
    CHECK(mc_chunk_acquire_populate(&f.context, 0, 0) == MC_CHUNK_ACQUIRE_OK);
    CHECK(f.entries[0].chunk == &f.chunks[9] && f.insertions[0] == &f.chunks[0] &&
          f.insertions[1] == &f.chunks[9]);
    CHECK(f.chunks[0].terrain_populated && f.chunks[0].dirty && !f.chunks[9].dirty &&
          !f.chunks[9].terrain_populated); /* Dirty mark retains the pre-callback target. */

    CHECK(reset_fixture() == 0);
    f.context.generate = NULL;
    f.context.populate = NULL;
    f.chunks[0].dirty = false;
    CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, &f.chunks[0]));
    CHECK(mc_chunk_acquire_populate(&f.context, 0, 0) == MC_CHUNK_ACQUIRE_OK);
    CHECK(f.chunks[0].terrain_populated && !f.chunks[0].dirty && f.event_count == 0u);
    CHECK(reset_fixture() == 0);
    CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, &f.chunks[15]));
    CHECK(mc_chunk_acquire_populate(&f.context, 0, 0) == MC_CHUNK_ACQUIRE_OK);
    CHECK(f.chunks[15].terrain_populated && !f.chunks[15].dirty && !f.chunks[15].active);
    CHECK(f.population_calls == 1u &&
          f.event_count == 1u); /* Blank provider call, virtual dirty no-op. */

    CHECK(reset_fixture() == 0);
    f.chunks[0].dirty = false;
    f.populate_ok = false;
    CHECK(mc_chunk_cache_publish(&f.cache, 0, 0, &f.chunks[0]));
    CHECK(mc_chunk_acquire_populate(&f.context, 0, 0) == MC_CHUNK_ACQUIRE_CALLBACK_FAILED);
    CHECK(f.chunks[0].terrain_populated && !f.chunks[0].dirty && f.population_calls == 1u);
    return 0;
}

static int test_recursive_outer_publication(void)
{
    CHECK(reset_fixture() == 0);
    f.generate_action = GENERATE_RECURSE;
    f.chunks[1].dirty = true;
    add_pending(0, 0);
    mc_chunk_state *out = &f.chunks[14];
    CHECK(mc_chunk_acquire(&f.context, 0, 0, &out) == MC_CHUNK_ACQUIRE_OK);
    CHECK(out == &f.chunks[0] && f.recursive_result == &f.chunks[1] && f.generate_calls == 2u);
    CHECK(f.cache.count == 1u && f.cache.insertion_count == 2u && f.pending.count == 0u);
    CHECK(f.entries[0].key == 0 && f.entries[0].chunk == out);
    CHECK(f.insertions[0] == &f.chunks[1] && f.insertions[1] == &f.chunks[0]);
    CHECK(f.events[0].kind == 'G' && f.events[1].kind == 'G' && f.events[2].chunk == &f.chunks[1] &&
          f.events[11].chunk == &f.chunks[0]);
    CHECK(f.chunks[0].active && f.chunks[1].active && !f.callback_error);
    return 0;
}

static int test_limits_and_callback_failures(void)
{
    mc_chunk_state *out = &f.chunks[14];
    for (int limit = 0; limit < 2; ++limit) {
        CHECK(reset_fixture() == 0);
        add_pending(0, 0);
        if (limit == 0)
            f.cache.capacity = 0u;
        else
            f.cache.insertion_capacity = 0u;
        memcpy(&saved, &f, sizeof(f));
        CHECK(mc_chunk_acquire(&f.context, 0, 0, &out) == MC_CHUNK_ACQUIRE_LIMIT);
        CHECK(out == &f.chunks[14] && memcmp(&saved, &f, sizeof(f)) == 0);

        CHECK(reset_fixture() == 0);
        add_pending(0, 0);
        f.generate_action = GENERATE_FILL;
        if (limit == 0)
            f.cache.capacity = 1u;
        else
            f.cache.insertion_capacity = 1u;
        CHECK(mc_chunk_acquire(&f.context, 0, 0, &out) == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT);
        CHECK(out == &f.chunks[14] && f.pending.count == 0u && f.generate_calls == 1u);
        CHECK(f.cache.count == 1u && f.cache.insertion_count == 1u &&
              f.entries[0].chunk == &f.chunks[2]);
        CHECK(f.chunks[2].dirty && !f.chunks[0].active && f.event_count == 1u);
    }
    for (int failure = 0; failure < 6; ++failure) {
        CHECK(reset_fixture() == 0);
        add_pending(0, 0);
        if (failure == 0) {
            f.context.load = load_chunk;
            f.load_result = MC_CHUNK_LOAD_FATAL;
            f.bump_time = true;
        } else if (failure == 1) {
            f.generate_ok = false;
            f.generate_action = GENERATE_FILL;
        } else if (failure == 2) {
            f.tiles_ok = false;
        } else if (failure == 3) {
            f.fail_slice = 3u;
        } else if (failure == 4) {
            f.generated = NULL;
        } else {
            f.invalidate_cache = true;
        }
        mc_chunk_acquire_status expected =
            failure >= 4 ? MC_CHUNK_ACQUIRE_STATE_ERROR : MC_CHUNK_ACQUIRE_CALLBACK_FAILED;
        CHECK(mc_chunk_acquire(&f.context, 0, 0, &out) == expected);
        CHECK(out == &f.chunks[14] && f.pending.count == 0u);
        if (failure == 0)
            CHECK(f.time == 777 && f.generate_calls == 0u && f.cache.count == 0u);
        else if (failure == 1)
            CHECK(f.cache.count == 1u && f.chunks[2].dirty && !f.chunks[0].active);
        else if (failure == 2 || failure == 3) {
            CHECK(f.cache.count == 1u && f.insertions[0] == &f.chunks[0] && f.chunks[0].active);
            CHECK(f.event_count == (failure == 2 ? 2u : 6u));
        } else if (failure == 4) {
            CHECK(f.cache.count == 0u && f.cache.insertion_count == 0u && f.event_count == 1u);
            CHECK(!f.chunks[15].active); /* Null generator success never silently uses fallback. */
        } else
            CHECK(f.cache.count == f.cache.capacity + 1u);
        CHECK(!f.callback_error);
    }
    CHECK(reset_fixture() == 0);
    f.context.load = load_chunk;
    f.load_result = MC_CHUNK_LOAD_LOADED;
    f.loaded = NULL;
    CHECK(mc_chunk_acquire(&f.context, 0, 0, &out) == MC_CHUNK_ACQUIRE_STATE_ERROR);
    CHECK(out == &f.chunks[14] && f.generate_calls == 0u && f.cache.count == 0u);
    CHECK(reset_fixture() == 0);
    f.generated = &f.chunks[1];
    f.chunks[1].data = NULL;
    CHECK(mc_chunk_acquire(&f.context, 0, 0, &out) == MC_CHUNK_ACQUIRE_STATE_ERROR);
    CHECK(out == &f.chunks[14] && f.cache.count == 0u);
    CHECK(reset_fixture() == 0);
    CHECK(build_grid() == 0);
    f.context.load = load_chunk;
    f.load_result = MC_CHUNK_LOAD_LOADED;
    f.loaded = &f.chunks[0];
    f.chunks[0].dirty = false;
    f.populate_ok = false;
    CHECK(mc_chunk_acquire(&f.context, 0, 0, &out) == MC_CHUNK_ACQUIRE_CALLBACK_FAILED);
    CHECK(out == &f.chunks[14] && f.cache.count == 9u && f.cache.insertion_count == 9u);
    CHECK(f.chunks[0].active && f.chunks[0].terrain_populated && !f.chunks[0].dirty);
    CHECK(f.population_calls == 1u && f.event_count == 11u);
    return 0;
}

static int test_invalid_resources(void)
{
    CHECK(reset_fixture() == 0);
    mc_chunk_state *out = &f.chunks[14];
    memcpy(&saved, &f, sizeof(f));
    CHECK(mc_chunk_acquire(NULL, 0, 0, &out) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT);
    CHECK(mc_chunk_acquire(&f.context, 0, 0, NULL) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT);
    CHECK(memcmp(&saved, &f, sizeof(f)) == 0);
    for (int invalid = 0; invalid < 10; ++invalid) {
        CHECK(reset_fixture() == 0);
        if (invalid == 0)
            f.context.world_time = NULL;
        else if (invalid == 1)
            f.context.register_tiles = NULL;
        else if (invalid == 2)
            f.context.register_entities = NULL;
        else if (invalid == 3)
            f.context.populate = NULL;
        else if (invalid == 4)
            f.context.fallback = NULL;
        else if (invalid == 5)
            f.context.policy = (mc_chunk_acquire_policy)99;
        else if (invalid == 6) {
            f.context.policy = MC_CHUNK_ACQUIRE_DEDICATED;
            f.context.spawn_z = NULL;
        } else if (invalid == 7)
            f.pending.count = f.pending.capacity + 1u;
        else if (invalid == 8)
            f.cache.capacity = SIZE_MAX;
        else {
            f.keys[0] = f.keys[1] = 12;
            f.pending.count = 2u;
        }
        memcpy(&saved, &f, sizeof(f));
        CHECK(!mc_chunk_acquire_validate(&f.context));
        CHECK(mc_chunk_acquire(&f.context, 0, 0, &out) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT);
        CHECK(mc_chunk_acquire_get(&f.context, 0, 0, &out) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT);
        CHECK(mc_chunk_acquire_populate(&f.context, 0, 0) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT);
        CHECK(memcmp(&saved, &f, sizeof(f)) == 0 && out == &f.chunks[14]);
    }
    CHECK(reset_fixture() == 0);
    CHECK(mc_chunk_acquire_mark_drop(&f.context, 99, 99) == MC_CHUNK_ACQUIRE_INVALID_ARGUMENT);
    CHECK(strcmp(mc_chunk_acquire_status_string(MC_CHUNK_ACQUIRE_PARTIAL_LIMIT),
                 "resource limit after partial effects") == 0);
    return 0;
}

int main(void)
{
    if (test_dedicated_gate_and_hit_pending() || test_pending_and_drop_arithmetic() ||
        test_load_generate_and_activation() || test_live_population_gates() ||
        test_population_flags_virtuals_and_reentry() || test_recursive_outer_publication() ||
        test_limits_and_callback_failures() || test_invalid_resources())
        return 1;
    printf("chunk acquisition contracts: %u checks\n", checks);
    return 0;
}
