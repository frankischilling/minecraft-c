#include "mc/world/runtime/lighting.h"

#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/runtime/provider_internal.h"
#include "../../support/java_random.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define MAX_ENTRIES 16u
static unsigned int checks;
static mc_math_trig trig;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Constructed native contracts. All disk/cache fixtures belong to the supplied
 * fresh root; no generated Chunk or original World observation is impersonated. */
typedef struct fixture {
    mc_world_regions *regions;
    mc_world_store *store;
    mc_java_random_profile *profile;
    mc_world_runtime_classes *classes;
    mc_world_runtime *raw, *world, *composition;
    mc_world_dimension *dimension;
    mc_world_info_name *name;
    mc_world_provider_fields provider;
    bool provider_live, world_binding;
    mc_chunk_state *chunks[7];
    mc_chunk_data scratch;
    uint8_t blocks[MC_CHUNK_BLOCKS];
    mc_chunk_cache composition_cache;
    mc_chunk_cache_entry composition_entries[1];
    mc_chunk_state *composition_list[1];
    mc_chunk_pending composition_pending;
    int32_t composition_keys[1];
    mc_java_int_set *composition_history;
    unsigned int forbidden_calls;
} fixture;

typedef struct passive_snapshot {
    size_t count, insertions, pending_count, history_count, chunks, reservations;
    mc_chunk_cache_entry entries[MAX_ENTRIES];
    mc_chunk_state *list[MAX_ENTRIES];
    int32_t pending[MAX_ENTRIES], history[MAX_ENTRIES];
    mc_world_regions_usage regions;
    mc_java_random world_random, generator_random, cave_random;
    uint64_t uniquifier;
    size_t profile_bytes;
    mc_java_random_profile_status random_status;
} passive_snapshot;

static bool same_storage_error(const mc_world_storage_error *a, const mc_world_storage_error *b)
{
    return a->status == b->status && a->operation == b->operation &&
           a->system_error == b->system_error;
}

static uint64_t uniquifier(const mc_java_random_profile *profile)
{
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static bool snapshot(fixture *f, passive_snapshot *out)
{
    const mc_chunk_cache *cache = f->provider.cache;
    const mc_chunk_pending *pending = f->provider.pending;
    mc_world_runtime_fields fields;
    if (!mc_chunk_cache_validate(cache) || !mc_chunk_pending_validate(pending) ||
        cache->count > MAX_ENTRIES || cache->insertion_count > MAX_ENTRIES ||
        pending->count > MAX_ENTRIES ||
        mc_world_runtime_fields_get(f->world, &fields) != MC_WORLD_STORAGE_OK || fields.r == NULL)
        return false;
    out->count = cache->count;
    out->insertions = cache->insertion_count;
    out->pending_count = pending->count;
    for (size_t i = 0; i < cache->count; ++i)
        out->entries[i] = cache->entries[i];
    for (size_t i = 0; i < cache->insertion_count; ++i)
        out->list[i] = cache->insertions[i];
    for (size_t i = 0; i < pending->count; ++i)
        out->pending[i] = pending->keys[i];
    if (mc_java_int_set_copy_order(pending->history, out->history, MAX_ENTRIES,
                                   &out->history_count) != MC_JAVA_INT_SET_OK)
        return false;
    out->chunks = mc_world_store_chunk_count(f->store);
    out->reservations = mc_world_store_reservation_count(f->store);
    out->regions = mc_world_regions_get_usage(f->regions);
    out->world_random = *fields.r;
    out->generator_random = f->provider.generator->generator_random;
    out->cave_random = f->provider.generator->cave_random;
    out->uniquifier = uniquifier(f->profile);
    out->profile_bytes = mc_java_random_profile_allocation_bytes(f->profile);
    out->random_status = mc_world_runtime_last_random_status(f->world);
    return true;
}

static bool snapshot_same(fixture *f, const passive_snapshot *before)
{
    passive_snapshot after;
    if (!snapshot(f, &after) || before->count != after.count ||
        before->insertions != after.insertions || before->pending_count != after.pending_count ||
        before->history_count != after.history_count || before->chunks != after.chunks ||
        before->reservations != after.reservations ||
        before->regions.count != after.regions.count ||
        before->regions.bytes != after.regions.bytes || before->uniquifier != after.uniquifier ||
        before->profile_bytes != after.profile_bytes ||
        before->random_status != after.random_status ||
        !mc_test_java_random_equal(&before->world_random, &after.world_random) ||
        !mc_test_java_random_equal(&before->generator_random, &after.generator_random) ||
        !mc_test_java_random_equal(&before->cave_random, &after.cave_random))
        return false;
    for (size_t i = 0; i < before->count; ++i)
        if (before->entries[i].key != after.entries[i].key ||
            before->entries[i].chunk != after.entries[i].chunk)
            return false;
    for (size_t i = 0; i < before->insertions; ++i)
        if (before->list[i] != after.list[i])
            return false;
    for (size_t i = 0; i < before->pending_count; ++i)
        if (before->pending[i] != after.pending[i])
            return false;
    for (size_t i = 0; i < before->history_count; ++i)
        if (before->history[i] != after.history[i])
            return false;
    return true;
}

static mc_chunk_callback_result
forbidden_maintenance(void *owner, const mc_chunk_lifecycle_context *context, bool *out)
{
    fixture *f = owner;
    (void)context;
    (void)out;
    ++f->forbidden_calls;
    return MC_CHUNK_CALLBACK_FATAL;
}

static mc_chunk_callback_result forbidden_auxiliary(void *owner,
                                                    const mc_world_save_context *context)
{
    fixture *f = owner;
    (void)context;
    ++f->forbidden_calls;
    return MC_CHUNK_CALLBACK_FATAL;
}

static uint32_t forbidden_timestamp(void *owner)
{
    fixture *f = owner;
    ++f->forbidden_calls;
    return 0; /* Admission must reject before reaching this native callback. */
}

static bool test_raw(fixture *f)
{
    bool ok = false;
    mc_world_light_error error;
    const int32_t outside[][2] = {{-32000001, 0}, {32000000, 0},  {0, -32000001}, {0, 32000001},
                                  {INT32_MIN, 0}, {INT32_MAX, 0}, {0, INT32_MIN}, {0, INT32_MAX}};
    const int32_t inside[][2] = {
        {-32000000, 0}, {31999999, 0}, {0, -32000000}, {0, 32000000}, {0, 0}};
    int32_t out = -731;
    CHECK(mc_world_runtime_height(NULL, 0, 0, &out, &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(out == -731 && error.status == MC_WORLD_STORAGE_INVALID_ARGUMENT && error.bytecode == 0 &&
          error.provider_bytecode == 0);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes, NULL,
                                          &f->raw, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_runtime_fields fields;
    CHECK(mc_world_runtime_fields_get(f->raw, &fields) == MC_WORLD_STORAGE_OK && fields.v == NULL);
    uint64_t u = uniquifier(f->profile);
    for (size_t i = 0; i < COUNT(outside); ++i) {
        out = -731;
        CHECK(mc_world_runtime_height(f->raw, outside[i][0], outside[i][1], &out, &error) ==
                  MC_WORLD_STORAGE_OK &&
              out == 0);
    }
    for (size_t i = 0; i < COUNT(inside); ++i) {
        out = -731;
        CHECK(mc_world_runtime_height(f->raw, inside[i][0], inside[i][1], &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE);
        CHECK(out == -731 && error.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              error.bytecode == 33 && error.provider_bytecode == 6 &&
              error.acquisition == MC_CHUNK_ACQUIRE_OK &&
              error.cause.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              error.cause.operation != NULL && error.cause.system_error == 0);
        CHECK(same_storage_error(mc_world_runtime_last_error(f->raw), &error.cause));
    }
    const mc_world_storage_error saved = *mc_world_runtime_last_error(f->raw);
    out = -731;
    CHECK(mc_world_runtime_height(f->raw, 32000000, 0, &out, &error) == MC_WORLD_STORAGE_OK &&
          out == 0);
    CHECK(same_storage_error(mc_world_runtime_last_error(f->raw), &saved));
    CHECK(mc_world_runtime_height(f->raw, 0, 0, NULL, &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(error.bytecode == 0 && same_storage_error(mc_world_runtime_last_error(f->raw), &saved));
    CHECK(uniquifier(f->profile) == u);
    CHECK(mc_world_runtime_fields_get(f->raw, &fields) == MC_WORLD_STORAGE_OK && fields.v == NULL);
    ok = true;
cleanup:
    return ok;
}

static bool create_world(fixture *f)
{
    bool ok = false;
    mc_world_runtime_fields fields;
    mc_world_runtime_construction construction;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = MAX_ENTRIES;
    CHECK(mc_world_dimension_create(0, NULL, &f->dimension, NULL) == MC_WORLD_DIMENSION_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes,
                                          &limits, &f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->store, .name = f->name, .seed = 23, .dimension = f->dimension};
    CHECK(mc_world_runtime_construct_world(f->world, &arguments, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->world, &construction) == MC_WORLD_STORAGE_OK &&
          construction.phase == MC_WORLD_RUNTIME_READY && construction.contexts_bound);
    CHECK(mc_world_runtime_fields_get(f->world, &fields) == MC_WORLD_STORAGE_OK &&
          fields.v != NULL);
    CHECK(mc_world_provider_bind_fields(fields.v, &f->provider, NULL) == MC_WORLD_STORAGE_OK);
    f->provider_live = true;
    CHECK(f->provider.world == f->world && f->provider.store == f->store &&
          f->provider.cache->count == 0 && f->provider.cache->insertion_count == 0 &&
          f->provider.pending->count == 0);
    CHECK(f->provider.fallback != NULL && f->provider.fallback->blank);
    ok = true;
cleanup:
    return ok;
}

static bool test_composition(fixture *f)
{
    bool ok = false;
    mc_world_light_error error;
    CHECK(mc_chunk_cache_init(&f->composition_cache, f->composition_entries, 1, f->composition_list,
                              1));
    CHECK(mc_java_int_set_create(NULL, &f->composition_history) == MC_JAVA_INT_SET_OK);
    CHECK(mc_chunk_pending_init_ordered(&f->composition_pending, f->composition_keys, 1,
                                        f->composition_history));
    const mc_world_runtime_options options = {.store = f->store,
                                              .cache = &f->composition_cache,
                                              .pending = &f->composition_pending,
                                              .dimension = 0,
                                              .policy = MC_CHUNK_ACQUIRE_BASE,
                                              .provider = f,
                                              .provider_maintenance = forbidden_maintenance,
                                              .auxiliary = f,
                                              .save_auxiliary = forbidden_auxiliary,
                                              .clock = f,
                                              .region_timestamp = forbidden_timestamp,
                                              .world_info = mc_world_runtime_info(f->world)};
    CHECK(mc_world_runtime_create(&options, &f->composition, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_storage_error saved = *mc_world_runtime_last_error(f->composition);
    const int32_t points[][2] = {{0, 0}, {32000000, 0}};
    for (size_t i = 0; i < COUNT(points); ++i) {
        int32_t out = -731;
        CHECK(mc_world_runtime_height(f->composition, points[i][0], points[i][1], &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE);
        CHECK(out == -731 && error.bytecode == 0 && error.provider_bytecode == 0 &&
              same_storage_error(mc_world_runtime_last_error(f->composition), &saved));
    }
    CHECK(f->forbidden_calls == 0 && f->composition_cache.count == 0 &&
          f->composition_pending.count == 0);
    CHECK(mc_world_runtime_free(f->composition, NULL) == MC_WORLD_STORAGE_OK);
    f->composition = NULL;
    ok = true;
cleanup:
    return ok;
}

static bool retain_chunk(fixture *f, size_t index, int32_t x, int32_t z, uint8_t height)
{
    if (index >= COUNT(f->chunks) || !mc_chunk_data_init(&f->scratch, f->blocks, sizeof(f->blocks)))
        return false;
    memset(f->scratch.heightmap, height, sizeof(f->scratch.heightmap));
    if (index == 0) {
        f->scratch.heightmap[0] = 201;
        f->scratch.heightmap[15] = 202;
        f->scratch.heightmap[15u * 16u] = 203;
        f->scratch.heightmap[255] = 255;
    } else if (index == 1) {
        f->scratch.heightmap[0] = 88;
    }
    const mc_chunk_state source = {.data = &f->scratch,
                                   .x = x,
                                   .z = z,
                                   .dirty = true,
                                   .terrain_populated = true,
                                   .last_save_time = 12345};
    return mc_world_store_retain_chunk(f->store, 0, &source, &f->chunks[index], NULL) ==
           MC_WORLD_STORAGE_OK;
}

static bool create_chunks(fixture *f)
{
    bool ok = false;
    const int32_t coordinates[][3] = {{0, 0, 73},        {-1, -1, 77},     {-2000000, 0, 129},
                                      {1999999, 0, 222}, {0, -2000000, 7}, {0, 2000000, 250},
                                      {2, 2, 123}};
    for (size_t i = 0; i < COUNT(coordinates); ++i)
        CHECK(retain_chunk(f, i, coordinates[i][0], coordinates[i][1], (uint8_t)coordinates[i][2]));
    uint32_t growth = UINT32_MAX;
    CHECK(mc_world_store_write_runtime_chunk_for_info(f->store, f->chunks[6],
                                                      mc_world_runtime_info(f->world), 0, 1,
                                                      &growth, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(growth != UINT32_MAX && mc_world_store_chunk_count(f->store) == COUNT(f->chunks));
    CHECK(f->provider.cache->count == 0 && !mc_chunk_cache_contains(f->provider.cache, 2, 2));
    bool changed = false;
    CHECK(mc_chunk_pending_add(f->provider.pending, mc_chunk_cache_key(0, 0), &changed) ==
              MC_CHUNK_ACQUIRE_OK &&
          changed);
    changed = false;
    CHECK(mc_chunk_pending_add(f->provider.pending, mc_chunk_cache_key(2, 2), &changed) ==
              MC_CHUNK_ACQUIRE_OK &&
          changed);
    ok = true;
cleanup:
    return ok;
}

static bool test_empty(fixture *f)
{
    bool ok = false;
    passive_snapshot before;
    mc_world_light_error error;
    /* Produce a real unrelated diagnostic without entropy or a fake clock. */
    CHECK(mc_world_runtime_construct_default_random(f->world, NULL) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    const mc_world_storage_error saved = *mc_world_runtime_last_error(f->world);
    CHECK(saved.status == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(snapshot(f, &before));
    const int32_t points[][2] = {{0, 0},        {32, 32},       {-1, -1},
                                 {0, 32000000}, {-32000000, 0}, {31999999, 0}};
    for (size_t i = 0; i < COUNT(points); ++i) {
        int32_t out = -731;
        CHECK(mc_world_runtime_height(f->world, points[i][0], points[i][1], &out, &error) ==
                  MC_WORLD_STORAGE_OK &&
              out == 0);
        CHECK(snapshot_same(f, &before));
        CHECK(same_storage_error(mc_world_runtime_last_error(f->world), &saved));
    }
    CHECK(f->provider.pending->count == 2 && f->provider.cache->count == 0);
    ok = true;
cleanup:
    return ok;
}

static bool test_present(fixture *f)
{
    bool ok = false;
    passive_snapshot before;
    mc_world_light_error error;
    const int32_t coordinates[][2] = {{0, 0},       {-1, -1},      {-2000000, 0},
                                      {1999999, 0}, {0, -2000000}, {0, 2000000}};
    /* Explicit validated restored baseline, not generated/activated chunks.
     * The real cache publication API keeps map/list ownership and aliases. */
    for (size_t i = 0; i < COUNT(coordinates); ++i)
        CHECK(mc_chunk_cache_publish(f->provider.cache, coordinates[i][0], coordinates[i][1],
                                     f->chunks[i]));
    CHECK(mc_chunk_cache_publish(f->provider.cache, 1, 0, f->provider.fallback));
    CHECK(mc_chunk_cache_validate(f->provider.cache) && f->provider.cache->count == 7);
    CHECK(snapshot(f, &before));
    const int32_t queries[][3] = {{0, 0, 201},
                                  {15, 0, 202},
                                  {0, 15, 203},
                                  {15, 15, 255},
                                  {-1, -1, 77},
                                  {-16, -16, 88},
                                  {16, 0, 0},
                                  {32, 32, 0},
                                  {524288, 0, 201},
                                  {-524288, 0, 0},
                                  {0, 524288, 201},
                                  {524303, 15, 255},
                                  {-524304, -524304, 88},
                                  {-32000000, 0, 129},
                                  {31999999, 0, 222},
                                  {0, -32000000, 7},
                                  {0, 32000000, 250},
                                  {-32000001, 0, 0},
                                  {32000000, 0, 0},
                                  {0, -32000001, 0},
                                  {0, 32000001, 0}};
    for (size_t i = 0; i < COUNT(queries); ++i) {
        int32_t out = -731;
        CHECK(mc_world_runtime_height(f->world, queries[i][0], queries[i][1], &out, &error) ==
                  MC_WORLD_STORAGE_OK &&
              out == queries[i][2]);
        CHECK(snapshot_same(f, &before));
    }
    CHECK(mc_chunk_cache_key(0, 0) == mc_chunk_cache_key(32768, 0));
    CHECK(mc_chunk_cache_key(0, 0) != mc_chunk_cache_key(-32768, 0));
    CHECK(mc_chunk_cache_key(-1, -1) == mc_chunk_cache_key(-32769, -32769));
    CHECK(mc_chunk_cache_publish(f->provider.cache, 32768, 0, f->chunks[1]));
    CHECK(f->provider.cache->count == 7 && f->provider.cache->insertion_count == 8 &&
          f->provider.cache->insertions[0] == f->chunks[0] &&
          f->provider.cache->insertions[7] == f->chunks[1]);
    CHECK(snapshot(f, &before));
    int32_t out = -731;
    CHECK(mc_world_runtime_height(f->world, 0, 0, &out, &error) == MC_WORLD_STORAGE_OK &&
          out == 88);
    CHECK(mc_world_runtime_height(f->world, 524288, 0, &out, &error) == MC_WORLD_STORAGE_OK &&
          out == 88);
    CHECK(snapshot_same(f, &before));
    CHECK(mc_world_store_retire_chunk(f->store, f->chunks[0], NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_store_retire_chunk(f->store, f->chunks[1], NULL) == MC_WORLD_STORAGE_BUSY);
    for (size_t i = 0; i < COUNT(f->chunks); ++i)
        CHECK(!f->chunks[i]->active && f->chunks[i]->dirty && f->chunks[i]->terrain_populated &&
              !f->chunks[i]->has_entities && f->chunks[i]->last_save_time == 12345);
    ok = true;
cleanup:
    return ok;
}

static bool clear_restored(fixture *f)
{
    if (!f->provider_live)
        return true;
    mc_chunk_cache *cache = f->provider.cache;
    if (!mc_chunk_cache_validate(cache))
        return false;
    for (size_t i = 0; i < cache->count + cache->insertion_count; ++i) {
        mc_chunk_state *chunk =
            i < cache->count ? cache->entries[i].chunk : cache->insertions[i - cache->count];
        bool known = chunk == f->provider.fallback;
        for (size_t j = 0; j < COUNT(f->chunks); ++j)
            if (chunk == f->chunks[j])
                known = true;
        if (!known)
            return false;
    }
    cache->count = 0;
    cache->insertion_count = 0;
    return mc_chunk_pending_clear(f->provider.pending) == MC_CHUNK_ACQUIRE_OK;
}

static bool test_retirement(fixture *f)
{
    bool ok = false;
    CHECK(mc_world_runtime_binding_retain(f->world));
    f->world_binding = true;
    CHECK(clear_restored(f));
    mc_world_dimension_unbind(f->dimension);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_BUSY);
    int32_t out = -731;
    mc_world_light_error error;
    CHECK(mc_world_runtime_height(f->world, 32, 32, &out, &error) == MC_WORLD_STORAGE_OK &&
          out == 0);
    CHECK(f->provider.cache->count == 0 && f->provider.pending->count == 0);
    CHECK(mc_world_runtime_retire_world(f->world, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_storage_error saved = *mc_world_runtime_last_error(f->world);
    out = -731;
    CHECK(mc_world_runtime_height(f->world, 0, 0, &out, &error) ==
          MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    CHECK(out == -731 && error.bytecode == 0 && error.provider_bytecode == 0 &&
          same_storage_error(mc_world_runtime_last_error(f->world), &saved));
    CHECK(mc_world_runtime_height(f->world, 32000000, 0, &out, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          out == -731 && error.bytecode == 0);
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_BUSY);
    mc_world_runtime_binding_release(f->world);
    f->world_binding = false;
    CHECK(mc_world_runtime_free(f->world, NULL) == MC_WORLD_STORAGE_OK);
    f->world = NULL;
    f->provider_live = false;
    ok = true;
cleanup:
    return ok;
}

static bool close_fixture(fixture *f)
{
    if (f == NULL)
        return true;
    if (f->composition != NULL &&
        mc_world_runtime_free(f->composition, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    f->composition = NULL;
    if (!clear_restored(f))
        return false;
    mc_world_dimension_unbind(f->dimension);
    if (f->world_binding) {
        mc_world_runtime_binding_release(f->world);
        f->world_binding = false;
    }
    mc_world_runtime **worlds[] = {&f->raw, &f->world};
    for (size_t i = 0; i < COUNT(worlds); ++i) {
        if (*worlds[i] == NULL)
            continue;
        if (mc_world_runtime_retire_world(*worlds[i], NULL) != MC_WORLD_STORAGE_OK ||
            mc_world_runtime_free(*worlds[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        *worlds[i] = NULL;
    }
    f->provider_live = false;
    for (size_t i = 0; i < COUNT(f->chunks); ++i) {
        if (f->chunks[i] != NULL &&
            mc_world_store_retire_chunk(f->store, f->chunks[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->chunks[i] = NULL;
    }
    mc_world_dimension_release(f->dimension);
    if (mc_world_runtime_classes_free(f->classes, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    mc_java_int_set_free(f->composition_history);
    mc_world_info_name_release(f->name);
    mc_world_store_free(f->store);
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->profile);
    free(f);
    return true;
}

static bool decode_path(const char *hex, char *out, size_t capacity)
{
    size_t n = strlen(hex);
    if (n == 0 || n % 2u != 0 || n / 2u >= capacity)
        return false;
    for (size_t i = 0; i < n / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            unsigned char c = (unsigned char)hex[2 * i + j];
            unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                 : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                        : 16u;
            if (digit > 15u)
                return false;
            value = value * 16u + digit;
        }
        if (value == 0)
            return false;
        out[i] = (char)(unsigned char)value;
    }
    out[n / 2u] = '\0';
    return true;
}

int main(int argc, char **argv)
{
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "world-lighting-runtime") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    uint8_t map[256];
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 126,
                                            .block_map = map,
                                            .block_map_length = sizeof(map),
                                            .limits = mc_world_store_default_limits()};
    const uint16_t title[] = {'H', 'e', 'i', 'g', 'h', 't'};
    const size_t profile_budget =
        sizeof(mc_java_random_profile) + sizeof(mc_java_math_random_owner);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_store_open(root, f->regions, &options, &f->store, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_java_random_profile_create(profile_budget, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(test_raw(f));
    CHECK(create_world(f));
    CHECK(test_composition(f));
    CHECK(create_chunks(f));
    CHECK(test_empty(f));
    CHECK(test_present(f));
    CHECK(test_retirement(f));
    result = 0;
cleanup:
    if (!close_fixture(f)) {
        fputs("World lighting fixture retirement refused; borrowers and dependencies retained\n",
              stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u World height checks (world-lighting-runtime)\n", checks) < 0)
        result = 1;
    return result;
}
