/* Optional fixed-work CPU benchmark. Genuine source World/Chunk/Light calls;
 * no display, ticks, field repair, hashing, assertions or output in timing. */
#define MC_WORLD_QUERIES_ENTRY mc_light_drain_unused_world_queries_program
#include "world_queries.c"
#undef MC_WORLD_QUERIES_ENTRY
#include "mc/world/runtime/propagation.h"
#include "mc/world/runtime/world_access.h"
#include "world/chunk/object_state_internal.h"

static uint64_t hash_bytes(uint64_t hash, const void *bytes, size_t count) {
    const unsigned char *data = bytes;
    for (size_t i = 0; i < count; ++i)
        hash = (hash ^ data[i]) * UINT64_C(1099511628211);
    return hash;
}

static uint64_t hash_int(uint64_t hash, int64_t value) {
    return hash_bytes(hash, &value, sizeof(value));
}

static uint64_t hash_text(uint64_t hash, const char *value) {
    hash = hash_int(hash, value != NULL);
    return value == NULL ? hash : hash_bytes(hash, value, strlen(value) + 1);
}

static uint64_t registry_hash(uint64_t h, const mc_registry_error *e) {
    h = hash_int(h, e->status);
    h = hash_text(h, e->message);
    h = hash_int(h, e->initializer_pc);
    h = hash_int(h, e->block_id);
    h = hash_text(h, e->constructor_class);
    h = hash_int(h, e->constructor_pc);
    h = hash_int(h, e->profile_status);
    h = hash_text(h, e->active_initializer_class);
    h = hash_int(h, e->active_initializer_pc);
    h = hash_int(h, e->item_argument);
    h = hash_int(h, e->format_error.status);
    h = hash_int(h, (int64_t)e->format_error.offset);
    h = hash_int(h, (int64_t)e->format_error.length);
    h = hash_int(h, e->format_error.value);
    h = hash_int(h, e->format_error.flags);
    h = hash_int(h, e->format_error.conversion);
    return hash_int(h, e->format_error.date_time);
}

static uint64_t storage_hash(uint64_t h, const mc_world_storage_error *e) {
    h = hash_int(h, e->status);
    h = hash_text(h, e->operation);
    return hash_int(h, e->system_error);
}

static uint64_t site_hash(uint64_t h, const mc_world_population_site *e) {
    h = hash_text(h, e->provider_class);
    h = hash_text(h, e->method);
    h = hash_int(h, e->bytecode);
    h = hash_int(h, e->world_bytecode);
    h = hash_text(h, e->selector_class);
    h = hash_text(h, e->selector_method);
    h = hash_int(h, e->selector_bytecode);
    h = hash_int(h, (int64_t)e->iteration);
    return hash_int(h, e->native_boundary);
}

/* Every query-receipt field, no addresses or padding. This failure's
 * source_chunk is checked NULL before serialization. */
static uint64_t query_hash(const mc_world_query_error *e) {
    uint64_t h = hash_int(UINT64_C(1469598103934665603), e->status);
    h = hash_text(h, e->operation);
    h = hash_int(h, e->bytecode);
    h = hash_int(h, e->provider_bytecode);
    h = hash_int(h, e->chunk_bytecode);
    h = hash_int(h, e->acquisition);
    h = storage_hash(h, &e->cause);
    h = registry_hash(h, &e->registry);
    h = hash_int(h, e->population.reached);
    h = site_hash(h, &e->population.current);
    h = site_hash(h, &e->population.origin);
    h = storage_hash(h, &e->population.cause);
    h = registry_hash(h, &e->population.registry);
    return hash_int(h, e->source_chunk != NULL);
}

static uint64_t chunks_hash(source_save_fixture *s) {
    uint64_t hash = UINT64_C(1469598103934665603);
    const size_t count = mc_world_store_chunk_count(s->game.stores[0]);
    for (size_t i = 0; i < count; ++i) {
        mc_chunk_state *chunk = NULL;
        if (mc_world_store_chunk_at(s->game.stores[0], i, &chunk, NULL) != MC_WORLD_STORAGE_OK ||
            chunk == NULL || chunk->blank || chunk->data == NULL)
            return 0;
        hash = hash_int(hash, chunk->x);
        hash = hash_int(hash, chunk->z);
        hash = hash_int(hash, chunk->dirty);
        hash = hash_int(hash, chunk->active);
        hash = hash_int(hash, chunk->terrain_populated);
        hash = hash_int(hash, chunk->never_save);
        hash = hash_int(hash, chunk->has_entities);
        hash = hash_int(hash, chunk->last_save_time);
        hash = hash_bytes(hash, chunk->data->blocks, MC_CHUNK_BLOCKS);
        hash = hash_bytes(hash, chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES);
        hash = hash_bytes(hash, chunk->data->skylight, MC_CHUNK_NIBBLE_BYTES);
        hash = hash_bytes(hash, chunk->data->block_light, MC_CHUNK_NIBBLE_BYTES);
        hash = hash_bytes(hash, chunk->data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES);
    }
    return hash;
}

static uint64_t queue_hash(mc_world_runtime *world, const mc_world_light_type *sky, size_t *count) {
    mc_world_light_list_view list;
    if (mc_world_runtime_light_list_get(world, &list, NULL) != MC_WORLD_STORAGE_OK)
        return 0;
    *count = list.count;
    uint64_t hash = hash_int(UINT64_C(1469598103934665603), (int64_t)list.count);
    for (size_t i = 0; i < list.count; ++i) {
        const mc_world_light_region *region = NULL;
        if (mc_world_runtime_light_at(world, i, &region, NULL) != MC_WORLD_STORAGE_OK)
            return 0;
        const mc_world_light_region_fields *fields =
            mc_world_runtime_light_region_fields_get(region);
        if (fields == NULL)
            return 0;
        hash = hash_int(hash, fields->a == sky ? 1 : fields->a == NULL ? 0 : 2);
        hash = hash_int(hash, fields->bounds.min_x);
        hash = hash_int(hash, fields->bounds.min_y);
        hash = hash_int(hash, fields->bounds.min_z);
        hash = hash_int(hash, fields->bounds.max_x);
        hash = hash_int(hash, fields->bounds.max_y);
        hash = hash_int(hash, fields->bounds.max_z);
    }
    return hash;
}

typedef struct light_observer {
    const mc_world_light_type *sky;
    mc_world_chunk *chunk;
    mc_world_chunk_nibble *original_sky;
    uint64_t trace;
    unsigned int calls, mode;
    bool bad, observed_prefix;
} light_observer;

static mc_world_storage_status observe_cell(void *context, mc_world_runtime *world, int32_t x,
                                            int32_t y, int32_t z, mc_world_storage_error *error) {
    light_observer *o = context;
    ++o->calls;
    int32_t value = -1;
    mc_world_query_error query;
    const mc_world_storage_status status =
        mc_world_runtime_saved_light(world, o->sky, x, y, z, &value, &query);
    o->bad = o->bad || status != MC_WORLD_STORAGE_OK || world->active_calls == 0 ||
             world->fields.M != (o->mode == 0 ? 0 : 1);
    o->trace = hash_int(hash_int(hash_int(hash_int(o->trace, x), y), z), value);
    if (status != MC_WORLD_STORAGE_OK) {
        *error = query.cause;
        return status;
    }
    if (o->mode == 1) {
        o->observed_prefix = value == 15;
        o->chunk->f = NULL; /* Authored callback mutation of the real nullable Source field. */
    } else if (o->mode == 2) {
        mc_chunk_provider_fields raw;
        if (mc_chunk_provider_bind_fields(world->fields.v, &raw, error) != MC_WORLD_STORAGE_OK)
            return error->status;
        o->observed_prefix = value == 15;
    }
    return MC_WORLD_STORAGE_OK;
}

static int drain_to_empty(mc_world_runtime *world, mc_world_light_operation_error *e) {
    bool more = true;
    for (unsigned int n = 0; more && n < 64; ++n)
        VERIFY(mc_world_runtime_light_drain(world, SIZE_MAX, &more, e) == MC_WORLD_STORAGE_OK);
    VERIFY(!more);
    return 0;
}

static int light_benchmark(source_save_fixture *s) {
    mc_world_runtime *world = s->game.worlds[0];
    mc_world_light_operation_error *e = calloc(1, sizeof(*e));
    VERIFY(e != NULL);
    for (int32_t x = -1; x <= 1; ++x)
        for (int32_t z = -1; z <= 1; ++z) {
            mc_chunk_state *loaded = NULL;
            VERIFY(mc_world_runtime_get_detailed(world, x, z, &loaded, NULL) ==
                       MC_CHUNK_ACQUIRE_OK &&
                   loaded != NULL && loaded->active && !loaded->blank);
        }
    const mc_world_light_type *sky = NULL;
    VERIFY(mc_world_runtime_light_type_get(world->classes, MC_LIGHT_SKY, &sky, NULL) ==
               MC_WORLD_STORAGE_OK &&
           drain_to_empty(world, e) == 0);
    mc_chunk_state *center = NULL;
    VERIFY(mc_world_runtime_get_detailed(world, 0, 0, &center, NULL) == MC_CHUNK_ACQUIRE_OK);
    light_observer o = {.sky = sky,
                        .chunk = mc_world_store_chunk_object(s->game.stores[0], center),
                        .trace = UINT64_C(1469598103934665603)};
    VERIFY(o.chunk != NULL);
    o.original_sky = o.chunk->f;
    mc_world_access *access = NULL;
    const mc_world_access_callbacks callbacks = {.cell = observe_cell};
    VERIFY(mc_world_access_create(&callbacks, &o, &access, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    printf("LIGHT_CPU_CONTEXT query-error-bytes=%zu light-error-bytes=%zu regions=500 "
           "voxel-volume=8 original-region-budget=499 samples=7\n",
           sizeof(mc_world_query_error), sizeof(*e));
    for (unsigned int sample = 0; sample < 7; ++sample) {
        for (int32_t i = 0; i < 500; ++i) {
            const int32_t x = 2 + (i / 80) % 6, y = 20 + i % 80, z = 2 + i / 480;
            const mc_light_bounds bounds = {x, y, z, x + 1, y + 1, z + 1};
            mc_light_submit_result submitted;
            VERIFY(mc_world_runtime_light_submit(world, sky, &bounds, false, &submitted, NULL) ==
                   MC_WORLD_STORAGE_OK);
        }
        size_t before_count, after_count;
        const uint64_t before_queue = queue_hash(world, sky, &before_count);
        const uint64_t before_chunks = chunks_hash(s);
        mc_chunk_provider_cache_observation before, after;
        VERIFY(before_count == 500 && before_queue != 0 && before_chunks != 0 &&
               mc_chunk_provider_cache_observe(world->fields.v, &before) && before.owned &&
               before.raw_exports == 0);
        unsigned char random[sizeof(*world->fields.r)];
        memcpy(random, world->fields.r, sizeof(random));
        const uint64_t unique = atomic_load(&s->game.profile->uniquifier);
        const size_t bindings = world->persistent_bindings;
        const mc_world_storage_error prior = world->last_error;
        const size_t direct = world->construction.direct_allocation_bytes;
        mc_world_reference_list *queue_identity = world->fields.C;
        bool more = false;
        uint64_t start, end, hz, end_hz;
        const bool started = bench_clock(&start, &hz);
        const mc_world_storage_status status =
            mc_world_runtime_light_drain(world, SIZE_MAX, &more, e);
        const bool ended = bench_clock(&end, &end_hz);
        const uint64_t after_queue = queue_hash(world, sky, &after_count);
        const uint64_t after_chunks = chunks_hash(s);
        VERIFY(started && ended && status == MC_WORLD_STORAGE_OK && more && after_count == 1 &&
               end > start && hz == end_hz && after_chunks == before_chunks && o.calls == 0 &&
               !o.bad && world->fields.C == queue_identity && world->fields.M == 0 &&
               world->active_calls == 0 && world->local_leases == 0 &&
               world->persistent_bindings == bindings &&
               memcmp(world->fields.r, random, sizeof(random)) == 0 &&
               atomic_load(&s->game.profile->uniquifier) == unique &&
               world->last_error.status == prior.status &&
               world->last_error.operation == prior.operation &&
               world->construction.direct_allocation_bytes < direct &&
               e->status == MC_WORLD_STORAGE_OK && e->query.status == MC_WORLD_STORAGE_OK &&
               e->query.bytecode == 0 && e->query.provider_bytecode == 0 &&
               e->query.chunk_bytecode == 0 && e->query.registry.initializer_pc == -1 &&
               mc_chunk_provider_cache_observe(world->fields.v, &after) && after.owned &&
               after.raw_exports == 0 && after.hits - before.hits == 37924 &&
               after.generic_gets == before.generic_gets && after.misses == before.misses &&
               after.rebuilds == before.rebuilds && after.map_count == before.map_count &&
               after.insertion_count == before.insertion_count);
        printf("LIGHT_CPU_SAMPLE sample=%u ticks=%" PRIu64 " frequency=%" PRIu64
               " before-queue=%" PRIu64 " after-queue=%" PRIu64 " chunks=%" PRIu64 " gets=%" PRIu64
               " remaining=%zu more=1 listeners=0 same-random=1 same-holds=1\n",
               sample, end - start, hz, before_queue, after_queue, after_chunks,
               after.hits - before.hits, after_count);
        VERIFY(drain_to_empty(world, e) == 0);
    }
    /* Later saved-light failure after seven successful reads in the SAME
     * outer frame: the actual listener observes the completed store, then
     * changes the real Source Nibble reference before the next query. */
    const mc_light_bounds point = {8, 80, 8, 8, 80, 8};
    mc_light_submit_result submitted;
    VERIFY(mc_world_runtime_saved_light_set(world, sky, 8, 80, 8, 0, e) == MC_WORLD_STORAGE_OK);
    o.calls = 0;
    o.mode = 1;
    o.trace = UINT64_C(1469598103934665603);
    VERIFY(mc_world_runtime_light_submit(world, sky, &point, false, &submitted, NULL) ==
           MC_WORLD_STORAGE_OK);
    bool more = true;
    const mc_world_storage_status failed = mc_world_runtime_light_drain(world, SIZE_MAX, &more, e);
    const bool source_prefix = o.calls == 1 && !o.bad && o.observed_prefix && o.chunk->f == NULL;
    o.chunk->f = o.original_sky; /* End the authored input scope; never rerun failed Source work. */
    VERIFY(failed == MC_WORLD_STORAGE_UNSUPPORTED_STATE && more && source_prefix &&
           e->drain_bytecode == 82 && e->region_bytecode == 607 && e->point_bytecode == 97 &&
           e->query.bytecode == 105 && e->query.chunk_bytecode == 15 &&
           e->query.registry.initializer_pc == -1 && e->query.source_chunk == NULL &&
           world->fields.M == 0 && world->active_calls == 0 && world->local_leases == 0);
    printf("LIGHT_CPU_FAILURE status=%d drain=%u region=%u point=%u query=%u chunk=%u "
           "acquisition=%d trace=%" PRIu64 " query-receipt=%" PRIu64
           " more=%d callbacks=%u operation=%s\n",
           (int)failed, e->drain_bytecode, e->region_bytecode, e->point_bytecode, e->query.bytecode,
           e->query.chunk_bytecode, (int)e->query.acquisition, o.trace, query_hash(&e->query),
           (int)more, o.calls, e->operation);
    /* Export occurs in a genuine setter callback. Later reads in the same
     * propagation invocation must use the ordinary generic getter fallback. */
    o.mode = 0;
    VERIFY(mc_world_runtime_saved_light_set(world, sky, 8, 80, 8, 0, e) == MC_WORLD_STORAGE_OK);
    o.calls = 0;
    o.mode = 2;
    o.observed_prefix = false;
    o.trace = UINT64_C(1469598103934665603);
    VERIFY(mc_world_runtime_light_submit(world, sky, &point, false, &submitted, NULL) ==
           MC_WORLD_STORAGE_OK);
    mc_chunk_provider_cache_observation before_export, after_export;
    VERIFY(mc_chunk_provider_cache_observe(world->fields.v, &before_export) && before_export.owned);
    VERIFY(mc_world_runtime_light_drain(world, SIZE_MAX, &more, e) == MC_WORLD_STORAGE_OK &&
           !more && o.calls == 1 && o.observed_prefix && !o.bad &&
           mc_chunk_provider_cache_observe(world->fields.v, &after_export) && !after_export.owned &&
           after_export.raw_exports == before_export.raw_exports + 1 &&
           after_export.generic_gets > before_export.generic_gets);
    printf("LIGHT_CPU_EXPORT callbacks=%u trace=%" PRIu64 " generic=%" PRIu64 " owned=0\n", o.calls,
           o.trace, after_export.generic_gets - before_export.generic_gets);
    o.mode = 0;
    const mc_light_bounds region = {8, 80, 8, 9, 81, 9};
    VERIFY(mc_world_runtime_light_submit(world, sky, &region, false, &submitted, NULL) ==
           MC_WORLD_STORAGE_OK);
    more = true;
    VERIFY(mc_world_runtime_light_drain(world, 5, &more, e) == MC_WORLD_STORAGE_LIMIT && more &&
           e->region_bytecode == 257 && e->query.status == MC_WORLD_STORAGE_OK &&
           e->query.bytecode == 0 && e->query.provider_bytecode == 0 &&
           e->query.chunk_bytecode == 0 && e->query.registry.initializer_pc == -1 &&
           world->fields.M == 0 && world->active_calls == 0 && world->local_leases == 0);
    printf("LIGHT_CPU_BUDGET status=%d region=%u query=%u query-chunk=%u canonical=1 more=%d\n",
           (int)e->status, e->region_bytecode, e->query.bytecode, e->query.chunk_bytecode,
           (int)more);
    bool removed = false;
    VERIFY(mc_world_runtime_remove_world_access(world, access, &removed, NULL) ==
               MC_WORLD_STORAGE_OK &&
           removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    free(e);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 2);
    source_save_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL && ss_setup(s, argv[1]) == 0 && bench_open(s) == 0);
    VERIFY(light_benchmark(s) == 0 && bench_stop(s) == 0 && ss_finish(s) == 0);
    printf("LIGHT_CPU_FINISH checks=%u cleanup=1 genuine-source-world=1\n", checks);
    free(s);
    return 0;
}
