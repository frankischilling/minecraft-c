#include "mc/resources/translation/translation.h"
#include "mc/world/runtime/chunks.h"

#include "../../../src/block/registry/runtime_internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/runtime/chunk_provider_internal.h"
#include "../../../src/world/storage/generated_internal.h"
#include "../../support/java_random.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define WORLD_BYTES (16u * 1024u * 1024u)
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static unsigned int checks;
static mc_math_trig trig;
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct byte_stream {
    const uint8_t *bytes;
    size_t length, offset;
    bool closed;
} byte_stream;

typedef struct resource_source {
    byte_stream streams[3];
    mc_registry_resources api;
    mc_registry_classes *registry;
    mc_world_store *watch_store;
    mc_world_runtime *watch_world;
    mc_chunk_state *watch_chunk;
    size_t opens, closes, guarded;
    bool fail_first, bad;
} resource_source;

typedef struct chunk_slot {
    mc_world_chunk *object;
    mc_chunk_state *state;
    mc_world_store *store;
} chunk_slot;

typedef struct fixture {
    mc_world_regions *regions;
    mc_world_store *stores[4];
    mc_world_info_name *name;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *registries[2];
    resource_source resources[2];
    mc_world_runtime_classes *classes[2];
    mc_world_runtime *worlds[12];
    mc_world_provider *dimensions[12];
    mc_world_store *world_stores[12];
    mc_chunk_provider_fields providers[12];
    bool provider_live[12];
    bool world_hold[12];
    chunk_slot chunks[24];
    mc_chunk_state *loaded;
    mc_chunk_state *finished_loaded;
    mc_registry_block_q_view q;
} fixture;

static bool bytes_are(const uint8_t *bytes, size_t length, uint8_t value) {
    for (size_t i = 0; i < length; ++i)
        if (bytes[i] != value)
            return false;
    return true;
}

static uint8_t pattern(size_t i) {
    return (uint8_t)((i * 37u + 11u) & 255u);
}

static bool patterned(const uint8_t *bytes) {
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i)
        if (bytes[i] != pattern(i))
            return false;
    return true;
}

static bool same_storage(const mc_world_storage_error *a, const mc_world_storage_error *b) {
    return a != NULL && b != NULL && a->status == b->status && a->operation == b->operation &&
           a->system_error == b->system_error;
}

static bool same_registry(const mc_registry_error *a, const mc_registry_error *b) {
    return a->message == b->message && a->initializer_pc == b->initializer_pc &&
           a->block_id == b->block_id && a->constructor_class == b->constructor_class &&
           a->constructor_pc == b->constructor_pc && a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

static uint64_t profile_u(const fixture *f) {
    return atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
}

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count) {
    byte_stream *s = context;
    if (out == NULL || count == NULL || capacity == 0 || s->closed || s->offset > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > 17)
        take = 17;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->bytes + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out) {
    resource_source *source = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    if (name == NULL || out == NULL || source->opens >= COUNT(names) ||
        strcmp(name, names[source->opens]) != 0) {
        source->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    const size_t index = source->opens++;
    if (source->fail_first) {
        if (index != 0 || source->watch_chunk == NULL || source->watch_store == NULL ||
            source->watch_world == NULL) {
            source->bad = true;
            return MC_REGISTRY_INVALID_ARGUMENT;
        }
        /* A real reached resource callback tests the finisher's actual lease.
         * Nothing is retired or replaced: every operation must refuse BUSY. */
        if (mc_world_store_retire_chunk(source->watch_store, source->watch_chunk, NULL) !=
                MC_WORLD_STORAGE_BUSY ||
            mc_world_runtime_retire_world(source->watch_world, NULL) != MC_WORLD_STORAGE_BUSY ||
            mc_world_runtime_free(source->watch_world, NULL) != MC_WORLD_STORAGE_BUSY ||
            mc_registry_classes_free(source->registry, NULL) != MC_REGISTRY_BUSY)
            source->bad = true;
        ++source->guarded;
        return MC_REGISTRY_IO_FAILED;
    }
    *out = (mc_java_properties_input){&source->streams[index], read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    resource_source *source = context;
    if (input == NULL || input->context != &source->streams[2] || input->read != read_bytes) {
        source->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++source->closes;
    source->streams[2].closed = true;
    return MC_REGISTRY_OK;
}

static bool resource_ready(void *context, const mc_java_properties_input *input) {
    resource_source *source = context;
    if (input == NULL || input->read != read_bytes)
        source->bad = true;
    return false;
}

static bool open_store(fixture *f, size_t slot, const char *root, const char *suffix, size_t chunks,
                       size_t bytes) {
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const int n = snprintf(path, sizeof(path), "%s%s", root, suffix);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 127,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = chunks;
    options.limits.nbt.max_allocation_bytes = bytes;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool create_world(fixture *f, size_t slot, size_t classes, size_t store, int32_t dimension,
                         size_t direct_bytes, mc_world_storage_status expected) {
    bool ok = false;
    mc_world_runtime_fields fields;
    mc_world_runtime_construction state;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_direct_allocation_bytes = direct_bytes;
    limits.max_provider_chunks = 32;
    CHECK(mc_world_provider_create(dimension, NULL, &f->dimensions[slot], NULL) ==
          MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                          f->classes[classes], &limits, &f->worlds[slot],
                                          NULL) == MC_WORLD_STORAGE_OK);
    f->world_stores[slot] = f->stores[store];
    const mc_world_runtime_world_arguments args = {
        .store = f->stores[store], .name = f->name, .seed = 23, .dimension = f->dimensions[slot]};
    CHECK(mc_world_runtime_construct_world(f->worlds[slot], &args, NULL) == expected);
    CHECK(mc_world_runtime_construction_get(f->worlds[slot], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == (expected == MC_WORLD_STORAGE_OK ? MC_WORLD_RUNTIME_READY
                                                          : MC_WORLD_RUNTIME_FAILED_CONSTRUCTION));
    CHECK(mc_world_runtime_fields_get(f->worlds[slot], &fields) == MC_WORLD_STORAGE_OK &&
          fields.w == f->stores[store] && fields.t == f->dimensions[slot]);
    if (expected == MC_WORLD_STORAGE_OK) {
        CHECK(state.contexts_bound && state.cache_registered && fields.v != NULL);
        CHECK(mc_chunk_provider_bind_fields(fields.v, &f->providers[slot], NULL) ==
              MC_WORLD_STORAGE_OK);
        f->provider_live[slot] = true;
        CHECK(f->providers[slot].world == f->worlds[slot] &&
              f->providers[slot].store == f->stores[store] &&
              mc_chunk_cache_validate(f->providers[slot].cache));
    } else {
        CHECK(fields.v == NULL);
    }
    ok = true;
cleanup:
    return ok;
}

static bool construct_chunk(fixture *f, size_t slot, size_t world, int32_t x, int32_t z,
                            bool input_pattern) {
    bool ok = false;
    uint8_t *blocks = malloc(MC_CHUNK_BLOCKS);
    const size_t before = mc_world_store_chunk_count(f->world_stores[world]);
    CHECK(blocks != NULL);
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i)
        blocks[i] = (uint8_t)(input_pattern ? pattern(i) : 0u);
    uint8_t *const supplied = blocks;
    mc_world_chunk_error error;
    CHECK(mc_world_runtime_chunk_construct(f->worlds[world], x, z, &blocks, MC_CHUNK_BLOCKS,
                                           &f->chunks[slot].object, &error) == MC_WORLD_STORAGE_OK);
    f->chunks[slot].state = mc_world_chunk_state(f->chunks[slot].object);
    f->chunks[slot].store = f->world_stores[world];
    CHECK(blocks == NULL && f->chunks[slot].state != NULL &&
          f->chunks[slot].state->data->blocks == supplied &&
          f->chunks[slot].state->data->blocks_capacity == MC_CHUNK_BLOCKS &&
          f->chunks[slot].state->x == x && f->chunks[slot].state->z == z &&
          !f->chunks[slot].state->blank &&
          mc_world_store_owns_chunk(f->chunks[slot].store, f->chunks[slot].state) &&
          mc_world_store_chunk_count(f->chunks[slot].store) == before + 1 &&
          mc_world_store_generated_chunk(f->chunks[slot].store, f->chunks[slot].state) ==
              f->chunks[slot].object);
    CHECK(input_pattern ? patterned(supplied) : bytes_are(supplied, MC_CHUNK_BLOCKS, 0));
    ok = true;
cleanup:
    free(blocks);
    return ok;
}

static bool test_constructor(fixture *f) {
    bool ok = false;
    mc_world_chunk_fields a, b;
    mc_world_chunk_nibble_fields e, sky, light;
    mc_world_runtime_fields world;
    CHECK(mc_world_runtime_fields_get(f->worlds[0], &world) == MC_WORLD_STORAGE_OK);
    const mc_java_random old_random = *world.r;
    const mc_java_random old_generator = f->providers[0].generator->generator_random;
    const mc_java_random old_cave = f->providers[0].generator->cave_random;
    const uint64_t old_u = profile_u(f);
    const size_t old_profile = mc_java_random_profile_allocation_bytes(f->profile);
    CHECK(construct_chunk(f, 0, 0, -2, 3, true));
    CHECK(construct_chunk(f, 1, 0, 0, 0, false));
    CHECK(mc_world_chunk_fields_get(f->chunks[0].object, &a) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_fields_get(f->chunks[1].object, &b) == MC_WORLD_STORAGE_OK);
    mc_chunk_state *state = f->chunks[0].state;
    CHECK(a.b == state->data->blocks && a.d == f->worlds[0] && a.j == -2 && a.k == 3 &&
          a.h == state->data->heightmap && a.i == 0 && !a.c && !a.n && !a.o && !a.p && !a.q &&
          a.r == 0);
    CHECK(a.e != NULL && a.f != NULL && a.g != NULL && a.e != a.f && a.e != a.g && a.f != a.g &&
          a.l != NULL && a.m != NULL && a.b != b.b && a.e != b.e && a.f != b.f && a.g != b.g &&
          a.h != b.h && a.l != b.l && a.m != b.m);
    CHECK(mc_world_chunk_nibble_fields_get(a.e, &e) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_nibble_fields_get(a.f, &sky) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_nibble_fields_get(a.g, &light) == MC_WORLD_STORAGE_OK);
    CHECK(e.a == state->data->metadata && sky.a == state->data->skylight &&
          light.a == state->data->block_light && e.length == MC_CHUNK_NIBBLE_BYTES &&
          sky.length == MC_CHUNK_NIBBLE_BYTES && light.length == MC_CHUNK_NIBBLE_BYTES &&
          e.a != sky.a && e.a != light.a && sky.a != light.a);
    CHECK(bytes_are(e.a, e.length, 0) && bytes_are(sky.a, sky.length, 0) &&
          bytes_are(light.a, light.length, 0) && bytes_are(a.h, MC_CHUNK_HEIGHTMAP_BYTES, 0));
    size_t count = 731;
    CHECK(mc_world_chunk_tile_entity_count(a.l, &count) == MC_WORLD_STORAGE_OK && count == 0);
    const mc_world_chunk_entity_list *lists[8] = {NULL};
    const mc_world_chunk_entity_list *others[8] = {NULL};
    for (size_t i = 0; i < COUNT(lists); ++i) {
        CHECK(mc_world_chunk_slice_at(a.m, i, &lists[i]) == MC_WORLD_STORAGE_OK &&
              lists[i] != NULL);
        count = 731;
        CHECK(mc_world_chunk_entity_count(lists[i], &count) == MC_WORLD_STORAGE_OK && count == 0);
        for (size_t j = 0; j < i; ++j)
            CHECK(lists[i] != lists[j]);
        CHECK(mc_world_chunk_slice_at(b.m, i, &others[i]) == MC_WORLD_STORAGE_OK &&
              others[i] != NULL);
    }
    for (size_t i = 0; i < COUNT(lists); ++i)
        for (size_t j = 0; j < COUNT(others); ++j)
            CHECK(lists[i] != others[j]);
    const mc_world_chunk_entity_list *guard = lists[0];
    CHECK(mc_world_chunk_slice_at(a.m, 8, &guard) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          guard == lists[0]);
    CHECK(mc_world_chunk_slice_at(NULL, 0, &guard) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          guard == lists[0]);
    count = 731;
    CHECK(mc_world_chunk_tile_entity_count(NULL, &count) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          count == 731);
    CHECK(mc_world_chunk_entity_count(NULL, &count) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          count == 731);
    CHECK(mc_world_chunk_state(NULL) == NULL && mc_world_chunk_allocation_bytes(NULL) == 0 &&
          mc_world_chunk_allocation_bytes(f->chunks[0].object) > MC_CHUNK_BLOCKS);
    state->active = true;
    state->terrain_populated = true;
    state->dirty = true;
    state->never_save = true;
    state->has_entities = true;
    state->last_save_time = INT64_MIN;
    state->data->minimum_height = -73;
    CHECK(mc_world_chunk_fields_get(f->chunks[0].object, &b) == MC_WORLD_STORAGE_OK && b.c && b.n &&
          b.o && b.p && b.q && b.r == INT64_MIN && b.i == -73 && b.b == a.b && b.e == a.e &&
          b.f == a.f && b.g == a.g && b.h == a.h && b.l == a.l && b.m == a.m && b.d == a.d &&
          b.j == a.j && b.k == a.k);
    state->active = false;
    state->terrain_populated = false;
    state->dirty = false;
    state->never_save = false;
    state->has_entities = false;
    state->last_save_time = 0;
    state->data->minimum_height = 0;
    CHECK(mc_test_java_random_equal(&old_random, world.r) &&
          mc_test_java_random_equal(&old_generator, &f->providers[0].generator->generator_random) &&
          mc_test_java_random_equal(&old_cave, &f->providers[0].generator->cave_random) &&
          profile_u(f) == old_u &&
          mc_java_random_profile_allocation_bytes(f->profile) == old_profile &&
          f->resources[0].opens == 0 && f->providers[0].cache->count == 0 &&
          f->providers[0].cache->insertion_count == 0 && f->providers[0].pending->count == 0);
    ok = true;
cleanup:
    return ok;
}

static bool native_q_error(const mc_registry_error *error) {
    return error->active_initializer_class == NULL && error->active_initializer_pc == -1 &&
           error->constructor_class == NULL && error->constructor_pc == -1 &&
           error->initializer_pc == -1;
}

static bool test_q(fixture *f) {
    bool ok = false;
    mc_registry_error error;
    mc_registry_block_view before, after;
    CHECK(mc_registry_classes_inspect_blocks(f->registries[0], &before, NULL) == MC_REGISTRY_OK &&
          before.phase == MC_REGISTRY_COLD && before.q == NULL);
    /* These are explicitly synthetic NONOWNING array guards. They are not a
     * substitute class owner or observations of malformed original q storage. */
    const int32_t values[2] = {INT32_MIN, INT32_MAX};
    mc_registry_block_q_view synthetic = {f->registries[0], NULL, 2};
    int32_t value = 731;
    const uint64_t u = profile_u(f);
    CHECK(mc_registry_block_q_at(&synthetic, -1, &value, &error) == MC_REGISTRY_NULL_REFERENCE &&
          value == 731 && native_q_error(&error));
    synthetic.identity = values;
    CHECK(mc_registry_block_q_at(&synthetic, -1, &value, &error) ==
              MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          value == 731 && native_q_error(&error));
    CHECK(mc_registry_block_q_at(&synthetic, 2, &value, &error) == MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          value == 731 && native_q_error(&error));
    CHECK(mc_registry_block_q_at(&synthetic, 0, &value, &error) == MC_REGISTRY_OK &&
          value == INT32_MIN);
    CHECK(mc_registry_block_q_at(&synthetic, 1, &value, &error) == MC_REGISTRY_OK &&
          value == INT32_MAX);
    CHECK(profile_u(f) == u && f->resources[0].opens == 0);
    CHECK(mc_registry_classes_inspect_blocks(f->registries[0], &after, NULL) == MC_REGISTRY_OK &&
          after.phase == MC_REGISTRY_COLD && after.q == NULL);
    CHECK(mc_registry_block_q_get(f->registries[0], &f->q, &error) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_inspect_blocks(f->registries[0], &after, NULL) == MC_REGISTRY_OK &&
          after.phase == MC_REGISTRY_READY && f->q.classes == f->registries[0] &&
          f->q.identity == after.q && f->q.identity != NULL && f->q.length == 256 &&
          f->resources[0].opens == 3 && f->resources[0].closes == 1 && !f->resources[0].bad);
    mc_registry_block_q_view again = {0};
    CHECK(mc_registry_block_q_get(f->registries[0], &again, &error) == MC_REGISTRY_OK &&
          again.classes == f->q.classes && again.identity == f->q.identity &&
          again.length == f->q.length);
    value = 731;
    CHECK(mc_registry_block_q_at(&f->q, 0, &value, &error) == MC_REGISTRY_OK && value == 0);
    CHECK(mc_registry_block_q_at(&f->q, 1, &value, &error) == MC_REGISTRY_OK &&
          value == f->q.identity[1]);
    ok = true;
cleanup:
    return ok;
}

static uint8_t sky_at(const mc_chunk_data *data, size_t x, size_t y, size_t z) {
    /* Public X/Z/Y byte-layout projection, not a second finalizer. */
    const size_t index = x * 2048u + z * 128u + y;
    return (uint8_t)((data->skylight[index / 2u] >> (4u * (index % 2u))) & 15u);
}

static bool test_finish_source(fixture *f) {
    bool ok = false, changed = false;
    int32_t *live = NULL;
    int32_t saved[4] = {0};
    uint8_t *old_sky = NULL;
    mc_world_chunk_fields before, after;
    mc_world_chunk_error error;
    mc_chunk_data *data = f->chunks[1].state->data;
    CHECK(f->q.identity != NULL && f->q.length == 256);
    live = (int32_t *)f->q.identity;
    for (size_t i = 0; i < COUNT(saved); ++i)
        saved[i] = live[i];
    changed = true;
    /* Authorized exclusive native controls of the REAL mutable q array.
     * These values are not claimed to occur in an observed original registry. */
    live[0] = 0;
    live[1] = 5;
    live[2] = INT32_MIN;
    live[3] = -1;
    memset(data->blocks, 0, MC_CHUNK_BLOCKS);
    data->blocks[126] = 1;  /* (0,126,0): height127. */
    data->blocks[2175] = 1; /* (1,127,0): ignored by height, sky starts here. */
    data->blocks[4096] = 1; /* (2,0,0): height1, sky must not write Y0. */
    data->blocks[6271] = 3; /* (3,127,0): 15-(-1)=16, nibble0 but loop continues. */
    data->blocks[8319] = 2; /* (4,127,0): wrapped 15-INT_MIN is negative. */
    memset(data->metadata, 0xa5, sizeof(data->metadata));
    memset(data->block_light, 0x3c, sizeof(data->block_light));
    memset(data->skylight, 0x66, sizeof(data->skylight));
    memset(data->heightmap, 0xee, sizeof(data->heightmap));
    data->minimum_height = -73;
    CHECK(mc_world_chunk_fields_get(f->chunks[1].object, &before) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construct_default_random(f->worlds[0], NULL) ==
          MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT);
    const mc_world_storage_error previous = *mc_world_runtime_last_error(f->worlds[0]);
    const uint64_t u = profile_u(f);
    CHECK(mc_world_runtime_chunk_finish(f->chunks[1].object, &error) == MC_WORLD_STORAGE_OK);
    CHECK(data->minimum_height == 0 && f->chunks[1].state->dirty &&
          bytes_are(data->metadata, sizeof(data->metadata), 0xa5) &&
          bytes_are(data->block_light, sizeof(data->block_light), 0x3c));
    for (size_t i = 0; i < MC_CHUNK_HEIGHTMAP_BYTES; ++i)
        CHECK(data->heightmap[i] == (i == 0 ? 127 : i == 2 ? 1 : 0));
    CHECK(sky_at(data, 0, 127, 0) == 15 && sky_at(data, 0, 126, 0) == 10 &&
          sky_at(data, 0, 1, 0) == 10 && sky_at(data, 0, 0, 0) == 6);
    CHECK(sky_at(data, 1, 127, 0) == 10 && sky_at(data, 1, 1, 0) == 10 &&
          sky_at(data, 1, 0, 0) == 6 && sky_at(data, 2, 1, 0) == 15 && sky_at(data, 2, 0, 0) == 6);
    CHECK(sky_at(data, 3, 127, 0) == 0 && sky_at(data, 3, 126, 0) == 0 &&
          sky_at(data, 3, 1, 0) == 0 && sky_at(data, 3, 0, 0) == 6);
    CHECK(sky_at(data, 4, 127, 0) == 6 && sky_at(data, 4, 126, 0) == 6 &&
          sky_at(data, 4, 1, 0) == 6 && sky_at(data, 4, 0, 0) == 6);
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i) {
        const uint8_t expected = (uint8_t)(i == 126 || i == 2175 || i == 4096 ? 1
                                           : i == 6271                        ? 3
                                           : i == 8319                        ? 2
                                                                              : 0);
        CHECK(data->blocks[i] == expected);
    }
    CHECK(mc_world_chunk_fields_get(f->chunks[1].object, &after) == MC_WORLD_STORAGE_OK &&
          after.b == before.b && after.e == before.e && after.f == before.f &&
          after.g == before.g && after.h == before.h && after.l == before.l &&
          after.m == before.m && after.d == before.d && after.j == before.j &&
          after.k == before.k && after.i == 0 && after.o);
    CHECK(profile_u(f) == u && same_storage(&previous, mc_world_runtime_last_error(f->worlds[0])));
    mc_world_light_list_view list;
    CHECK(mc_world_runtime_light_list_get(f->worlds[0], &list, NULL) == MC_WORLD_STORAGE_OK &&
          list.count == 0 && f->providers[0].cache->count == 0);
    old_sky = malloc(sizeof(data->skylight));
    CHECK(old_sky != NULL);
    memcpy(old_sky, data->skylight, sizeof(data->skylight));
    /* Ordinary byte edits before a repeat call: every direct-sky write now
     * skips, so the first pass's nonzero/zero packed history must survive. */
    memset(data->blocks, 0, MC_CHUNK_BLOCKS);
    live[0] = 15;
    f->chunks[1].state->dirty = false;
    CHECK(mc_world_runtime_chunk_finish(f->chunks[1].object, &error) == MC_WORLD_STORAGE_OK);
    CHECK(data->minimum_height == 127 && bytes_are(data->heightmap, sizeof(data->heightmap), 127) &&
          memcmp(data->skylight, old_sky, sizeof(data->skylight)) == 0 &&
          bytes_are(data->blocks, MC_CHUNK_BLOCKS, 0) && f->chunks[1].state->dirty);
    ok = true;
cleanup:
    if (changed)
        for (size_t i = 0; i < COUNT(saved); ++i)
            live[i] = saved[i];
    free(old_sky);
    return ok;
}

static bool test_no_sky_prefix(fixture *f) {
    bool ok = false;
    mc_world_chunk_error error;
    mc_world_chunk_fields fields;
    CHECK(create_world(f, 1, 0, 0, -1, WORLD_BYTES, MC_WORLD_STORAGE_UNSUPPORTED_STATE));
    CHECK(construct_chunk(f, 2, 1, 2000001, 2000001, false));
    CHECK(construct_chunk(f, 3, 1, 0, 0, false));
    CHECK(construct_chunk(f, 4, 1, INT32_MAX, INT32_MAX, false));
    for (size_t slot = 2; slot <= 4; ++slot) {
        mc_chunk_data *data = f->chunks[slot].state->data;
        memset(data->skylight, 0x77, sizeof(data->skylight));
        memset(data->heightmap, 0xee, sizeof(data->heightmap));
        data->minimum_height = 73;
        f->chunks[slot].state->dirty = false;
        const mc_world_storage_status status =
            mc_world_runtime_chunk_finish(f->chunks[slot].object, &error);
        CHECK(status == (slot == 2 ? MC_WORLD_STORAGE_OK : MC_WORLD_STORAGE_UNSUPPORTED_STATE));
        CHECK(bytes_are(data->heightmap, sizeof(data->heightmap), 0) && data->minimum_height == 0 &&
              bytes_are(data->skylight, sizeof(data->skylight), 0x77) &&
              bytes_are(data->blocks, MC_CHUNK_BLOCKS, 0));
        CHECK(mc_world_chunk_fields_get(f->chunks[slot].object, &fields) == MC_WORLD_STORAGE_OK &&
              fields.d == f->worlds[1] && fields.o == (slot == 2));
        if (slot != 2) {
            CHECK(error.bytecode == 201 && error.column_bytecode == 37 &&
                  error.interval_bytecode == 6 && error.light.bytecode == 33 &&
                  error.light.provider_bytecode == 6 &&
                  error.light.cause.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  same_storage(&error.light.cause, mc_world_runtime_last_error(f->worlds[1])));
        }
    }
    /* INT_MAX*16 wraps to -16. A widened/clamped outside-border substitute
     * would miss the real in-domain provider failure seen by slot4. */
    ok = true;
cleanup:
    return ok;
}

static bool publish(fixture *f, size_t world, size_t chunk, int32_t x, int32_t z) {
    return mc_chunk_cache_publish(f->providers[world].cache, x, z, f->chunks[chunk].state) &&
           mc_chunk_cache_validate(f->providers[world].cache);
}

static bool bounds_are(const mc_world_light_region *region, const mc_world_light_type *type,
                       int32_t x, int32_t y0, int32_t z, int32_t y1) {
    const mc_world_light_region_fields *fields = mc_world_runtime_light_region_fields_get(region);
    return fields != NULL && fields->a == type && fields->bounds.min_x == x &&
           fields->bounds.max_x == x && fields->bounds.min_y == y0 && fields->bounds.max_y == y1 &&
           fields->bounds.min_z == z && fields->bounds.max_z == z;
}

static bool test_neighbor_prefixes(fixture *f) {
    bool ok = false;
    mc_world_runtime_construction base, used, current;
    mc_world_light_error light;
    mc_world_chunk_error error;
    mc_light_submit_result result;
    mc_world_light_list_view view;
    const mc_world_light_type *sky = NULL;
    const mc_light_bounds first = {-1, 0, 0, -1, 5, 0};
    CHECK(construct_chunk(f, 5, 0, -1, 0, false));
    CHECK(construct_chunk(f, 6, 0, 0, 0, false));
    memset(f->chunks[5].state->data->heightmap, 5, MC_CHUNK_HEIGHTMAP_BYTES);
    memset(f->chunks[6].state->data->heightmap, 5, MC_CHUNK_HEIGHTMAP_BYTES);
    /* Native restored cache inputs: these are actual generated/store-owned
     * objects, not an assertion that the outer chunk has been published. */
    CHECK(publish(f, 0, 5, -1, 0) && publish(f, 0, 6, 0, 0));
    CHECK(mc_world_runtime_light_type_get(f->classes[0], MC_LIGHT_SKY, &sky, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->worlds[0], &base) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_light_submit(f->worlds[0], sky, &first, false, &result, &light) ==
              MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED);
    CHECK(mc_world_runtime_construction_get(f->worlds[0], &used) == MC_WORLD_STORAGE_OK &&
          used.direct_allocation_bytes > base.direct_allocation_bytes);
    CHECK(create_world(f, 2, 0, 0, 0, used.direct_allocation_bytes, MC_WORLD_STORAGE_OK));
    CHECK(publish(f, 2, 5, -1, 0) && publish(f, 2, 6, 0, 0));
    CHECK(mc_world_runtime_construction_get(f->worlds[2], &current) == MC_WORLD_STORAGE_OK &&
          current.direct_allocation_bytes == base.direct_allocation_bytes);
    CHECK(mc_world_runtime_light_submit(f->worlds[2], sky, &first, false, &result, &light) ==
              MC_WORLD_STORAGE_OK &&
          result == MC_LIGHT_SUBMIT_APPENDED);
    CHECK(construct_chunk(f, 7, 2, 0, 0, false));
    f->chunks[7].state->data->minimum_height = 73;
    const mc_world_light_region *retained = NULL;
    CHECK(mc_world_runtime_light_at(f->worlds[2], 0, &retained, NULL) == MC_WORLD_STORAGE_OK &&
          retained != NULL);
    CHECK(mc_world_runtime_chunk_finish(f->chunks[7].object, &error) == MC_WORLD_STORAGE_LIMIT);
    CHECK(error.bytecode == 201 && error.column_bytecode == 48 && error.interval_bytecode == 31 &&
          error.light.bytecode == 219 && error.light.status == MC_WORLD_STORAGE_LIMIT &&
          f->chunks[7].state->dirty && f->chunks[7].state->data->minimum_height == 0 &&
          bytes_are(f->chunks[7].state->data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES, 0) &&
          sky_at(f->chunks[7].state->data, 15, 1, 15) == 15);
    CHECK(mc_world_runtime_light_list_get(f->worlds[2], &view, NULL) == MC_WORLD_STORAGE_OK &&
          view.count == 1);
    const mc_world_light_region *again = NULL;
    CHECK(mc_world_runtime_light_at(f->worlds[2], 0, &again, NULL) == MC_WORLD_STORAGE_OK &&
          again == retained && bounds_are(again, sky, -1, 0, 0, 5));
    CHECK(same_storage(&error.light.cause, mc_world_runtime_last_error(f->worlds[2])));
    mc_world_runtime_class_state class_state;
    CHECK(mc_world_runtime_classes_get(f->classes[0], &class_state) == MC_WORLD_STORAGE_OK &&
          class_state.enqueue_depth == 0);

    /* With no pre-existing record, the same first -X interval fails before its
     * dirty36 store. The complete local height/minimum phase still survives. */
    CHECK(create_world(f, 9, 0, 0, 0, base.direct_allocation_bytes, MC_WORLD_STORAGE_OK));
    CHECK(publish(f, 9, 5, -1, 0) && publish(f, 9, 6, 0, 0));
    CHECK(construct_chunk(f, 8, 9, 0, 0, false));
    f->chunks[8].state->data->minimum_height = 73;
    CHECK(mc_world_runtime_chunk_finish(f->chunks[8].object, &error) == MC_WORLD_STORAGE_LIMIT);
    CHECK(error.bytecode == 201 && error.column_bytecode == 37 && error.interval_bytecode == 31 &&
          error.light.bytecode == 219 && !f->chunks[8].state->dirty &&
          f->chunks[8].state->data->minimum_height == 0 &&
          bytes_are(f->chunks[8].state->data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES, 0) &&
          sky_at(f->chunks[8].state->data, 15, 1, 15) == 15);
    CHECK(mc_world_runtime_light_list_get(f->worlds[9], &view, NULL) == MC_WORLD_STORAGE_OK &&
          view.count == 0);
    CHECK(mc_world_runtime_classes_get(f->classes[0], &class_state) == MC_WORLD_STORAGE_OK &&
          class_state.enqueue_depth == 0);
    ok = true;
cleanup:
    return ok;
}

static bool test_registry_failure(fixture *f) {
    bool ok = false;
    mc_world_chunk_error first, retry;
    mc_registry_error cached;
    mc_registry_block_view view;
    CHECK(create_world(f, 6, 1, 0, 0, WORLD_BYTES, MC_WORLD_STORAGE_OK));
    CHECK(construct_chunk(f, 9, 6, 0, 0, true));
    mc_chunk_state *state = f->chunks[9].state;
    memset(state->data->metadata, 0xa5, sizeof(state->data->metadata));
    memset(state->data->skylight, 0xb6, sizeof(state->data->skylight));
    memset(state->data->block_light, 0xc7, sizeof(state->data->block_light));
    memset(state->data->heightmap, 0xd8, sizeof(state->data->heightmap));
    state->data->minimum_height = 73;
    resource_source *source = &f->resources[1];
    source->watch_store = f->stores[0];
    source->watch_world = f->worlds[6];
    source->watch_chunk = state;
    CHECK(source->opens == 0);
    CHECK(mc_world_runtime_chunk_finish(f->chunks[9].object, &first) ==
          MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    CHECK(first.bytecode == 39 && first.column_bytecode == 0 && first.interval_bytecode == 0 &&
          first.registry.status == MC_REGISTRY_IO_FAILED && first.registry.constructor_pc == 23 &&
          first.registry.active_initializer_pc == 4 &&
          first.registry.active_initializer_class != NULL &&
          strcmp(first.registry.active_initializer_class, "nh") == 0 &&
          first.cause.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          same_storage(&first.cause, mc_world_runtime_last_error(f->worlds[6])));
    CHECK(source->opens == 1 && source->guarded == 1 && !source->bad && !state->dirty &&
          state->data->minimum_height == 73 && patterned(state->data->blocks) &&
          bytes_are(state->data->metadata, sizeof(state->data->metadata), 0xa5) &&
          bytes_are(state->data->skylight, sizeof(state->data->skylight), 0xb6) &&
          bytes_are(state->data->block_light, sizeof(state->data->block_light), 0xc7) &&
          bytes_are(state->data->heightmap, sizeof(state->data->heightmap), 0xd8));
    CHECK(mc_registry_classes_inspect_blocks(f->registries[1], &view, NULL) == MC_REGISTRY_OK &&
          view.phase == MC_REGISTRY_FAILED && view.q != NULL);
    mc_registry_block_q_view out = f->q;
    const uint64_t u = profile_u(f);
    CHECK(mc_registry_block_q_get(f->registries[1], &out, &cached) ==
              MC_REGISTRY_FAILED_INITIALIZATION &&
          out.classes == f->q.classes && out.identity == f->q.identity &&
          out.length == f->q.length && cached.status == MC_REGISTRY_FAILED_INITIALIZATION &&
          same_registry(&first.registry, &cached));
    CHECK(mc_world_runtime_chunk_finish(f->chunks[9].object, &retry) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          retry.bytecode == 39 && retry.registry.status == MC_REGISTRY_FAILED_INITIALIZATION &&
          same_registry(&first.registry, &retry.registry) && source->opens == 1 &&
          source->guarded == 1 && profile_u(f) == u && !state->dirty &&
          state->data->minimum_height == 73 && patterned(state->data->blocks));
    /* at uses its captured reference without reentering even a failed class.
     * Again this is a labeled synthetic nonowning view, not a replacement q. */
    const int32_t values[] = {-17};
    const mc_registry_block_q_view synthetic = {f->registries[1], values, COUNT(values)};
    int32_t value = 731;
    CHECK(mc_registry_block_q_at(&synthetic, 0, &value, &cached) == MC_REGISTRY_OK &&
          value == -17 && source->opens == 1 && profile_u(f) == u);
    ok = true;
cleanup:
    return ok;
}

static bool test_serialization(fixture *f) {
    bool ok = false, leased = false;
    mc_world_chunk_fields before, after;
    mc_chunk_state *state = f->chunks[0].state;
    mc_chunk_data *data = state->data;
    CHECK(mc_world_chunk_fields_get(f->chunks[0].object, &before) == MC_WORLD_STORAGE_OK &&
          patterned(data->blocks));
    memset(data->metadata, 0xa5, sizeof(data->metadata));
    memset(data->skylight, 0xb6, sizeof(data->skylight));
    memset(data->block_light, 0xc7, sizeof(data->block_light));
    memset(data->heightmap, 0xd8, sizeof(data->heightmap));
    data->minimum_height = -73;
    state->terrain_populated = true;
    state->dirty = true;
    state->never_save = true;
    state->has_entities = true;
    state->last_save_time = INT64_MIN + 17;
    uint32_t growth = UINT32_MAX;
    const size_t count = mc_world_store_chunk_count(f->stores[0]);
    /* Timestamp1 and world time922 are explicit native fixture scalars. No
     * serializer clock, source field identity or constructor entropy is injected. */
    CHECK(mc_world_store_write_runtime_chunk_for_info(f->stores[0], state,
                                                      mc_world_runtime_info(f->worlds[0]), 922, 1,
                                                      &growth, NULL) == MC_WORLD_STORAGE_OK &&
          growth != UINT32_MAX);
    CHECK(mc_world_store_chunk_count(f->stores[0]) == count && !state->has_entities &&
          state->dirty && state->never_save && state->last_save_time == INT64_MIN + 17);
    CHECK(mc_world_chunk_fields_get(f->chunks[0].object, &after) == MC_WORLD_STORAGE_OK &&
          after.b == before.b && after.d == before.d && after.e == before.e &&
          after.f == before.f && after.g == before.g && after.h == before.h &&
          after.l == before.l && after.m == before.m && after.j == -2 && after.k == 3 && after.n &&
          after.o && after.p && !after.q && after.r == INT64_MIN + 17 && after.i == -73 &&
          patterned(after.b));
    CHECK(mc_world_store_load_chunk(f->stores[0], 0, -2, 3, &f->loaded, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(f->loaded != state && f->loaded->data != data &&
          f->loaded->data->blocks != data->blocks &&
          mc_world_store_generated_chunk(f->stores[0], f->loaded) == NULL &&
          !mc_world_store_has_generated_for_world(f->stores[0], f->worlds[7]) &&
          mc_world_store_chunk_count(f->stores[0]) == count + 1);
    CHECK(f->loaded->x == -2 && f->loaded->z == 3 && f->loaded->terrain_populated &&
          !f->loaded->active && !f->loaded->dirty && !f->loaded->never_save &&
          !f->loaded->has_entities && f->loaded->last_save_time == 0 &&
          f->loaded->data->minimum_height == 0);
    CHECK(memcmp(f->loaded->data->blocks, data->blocks, MC_CHUNK_BLOCKS) == 0 &&
          memcmp(f->loaded->data->metadata, data->metadata, MC_CHUNK_NIBBLE_BYTES) == 0 &&
          memcmp(f->loaded->data->skylight, data->skylight, MC_CHUNK_NIBBLE_BYTES) == 0 &&
          memcmp(f->loaded->data->block_light, data->block_light, MC_CHUNK_NIBBLE_BYTES) == 0 &&
          memcmp(f->loaded->data->heightmap, data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES) == 0);
    CHECK(mc_world_store_release_chunk(f->stores[0], state, NULL) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_world_store_lease_chunk(f->stores[0], state, NULL) == MC_WORLD_STORAGE_OK);
    leased = true;
    CHECK(mc_world_store_retire_chunk(f->stores[0], state, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_store_release_chunk(f->stores[0], state, NULL) == MC_WORLD_STORAGE_OK);
    leased = false;
    state->active = true;
    CHECK(mc_world_store_retire_chunk(f->stores[0], state, NULL) == MC_WORLD_STORAGE_BUSY);
    state->active = false;
    CHECK(mc_world_store_has_generated_for_world(f->stores[0], f->worlds[0]) &&
          mc_world_runtime_free(f->worlds[0], NULL) == MC_WORLD_STORAGE_BUSY);
    /* Also serialize the SAME object actually finished earlier. No new copy
     * is installed as its state, data, bytes or child identities. */
    mc_chunk_state *finished = f->chunks[1].state;
    CHECK(mc_world_chunk_fields_get(f->chunks[1].object, &before) == MC_WORLD_STORAGE_OK &&
          before.i == 127 && bytes_are(finished->data->blocks, MC_CHUNK_BLOCKS, 0));
    growth = UINT32_MAX;
    CHECK(mc_world_store_write_runtime_chunk_for_info(f->stores[0], finished,
                                                      mc_world_runtime_info(f->worlds[0]), 923, 1,
                                                      &growth, NULL) == MC_WORLD_STORAGE_OK &&
          growth != UINT32_MAX && mc_world_chunk_state(f->chunks[1].object) == finished);
    CHECK(mc_world_chunk_fields_get(f->chunks[1].object, &after) == MC_WORLD_STORAGE_OK &&
          after.b == before.b && after.e == before.e && after.f == before.f &&
          after.g == before.g && after.h == before.h && after.l == before.l &&
          after.m == before.m && after.i == 127 && after.o && finished->last_save_time == 0);
    CHECK(mc_world_store_load_chunk(f->stores[0], 0, 0, 0, &f->finished_loaded, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(f->finished_loaded != finished && f->finished_loaded->data != finished->data &&
          mc_world_store_generated_chunk(f->stores[0], f->finished_loaded) == NULL &&
          f->finished_loaded->data->minimum_height == 0 && !f->finished_loaded->dirty &&
          bytes_are(f->finished_loaded->data->blocks, MC_CHUNK_BLOCKS, 0) &&
          memcmp(f->finished_loaded->data->heightmap, finished->data->heightmap,
                 MC_CHUNK_HEIGHTMAP_BYTES) == 0 &&
          memcmp(f->finished_loaded->data->skylight, finished->data->skylight,
                 MC_CHUNK_NIBBLE_BYTES) == 0);
    ok = true;
cleanup:
    if (leased)
        (void)mc_world_store_release_chunk(f->stores[0], state, NULL);
    return ok;
}

static bool test_admission_and_limits(fixture *f, const char *root) {
    bool ok = false;
    uint8_t *blocks = NULL;
    mc_world_store_reservation *reservation = NULL;
    mc_world_chunk_error error;
    mc_world_chunk *out = f->chunks[0].object;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, f->classes[0],
                                          NULL, &f->worlds[7], NULL) == MC_WORLD_STORAGE_OK);
    blocks = malloc(MC_CHUNK_BLOCKS);
    CHECK(blocks != NULL);
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i)
        blocks[i] = pattern(i);
    uint8_t *const supplied = blocks;
    const size_t before = mc_world_store_chunk_count(f->stores[0]);
    const uint64_t u = profile_u(f);
    CHECK(mc_world_runtime_chunk_construct(NULL, 0, 0, &blocks, MC_CHUNK_BLOCKS, &out, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          out == f->chunks[0].object && blocks == supplied);
    CHECK(mc_world_runtime_chunk_construct(f->worlds[0], 0, 0, &blocks, MC_CHUNK_BLOCKS - 1u, &out,
                                           &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          out == f->chunks[0].object && blocks == supplied && error.bytecode == 0);
    CHECK(mc_world_runtime_chunk_construct(f->worlds[0], 0, 0, NULL, MC_CHUNK_BLOCKS, &out,
                                           &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          out == f->chunks[0].object);
    CHECK(mc_world_runtime_chunk_construct(f->worlds[0], 0, 0, &blocks, MC_CHUNK_BLOCKS, NULL,
                                           &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          blocks == supplied);
    CHECK(mc_world_runtime_chunk_construct(f->worlds[7], 0, 0, &blocks, MC_CHUNK_BLOCKS, &out,
                                           &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          out == f->chunks[0].object && blocks == supplied && error.bytecode == 0);
    CHECK(patterned(blocks) && profile_u(f) == u &&
          mc_world_store_chunk_count(f->stores[0]) == before);
    free(blocks);
    blocks = NULL;

    const size_t bytes = mc_world_chunk_allocation_bytes(f->chunks[0].object);
    CHECK(bytes > MC_CHUNK_BLOCKS);
    CHECK(open_store(f, 1, root, "/count", 1, mc_nbt_default_limits().max_allocation_bytes));
    CHECK(open_store(f, 2, root, "/short", 2, bytes - 1));
    CHECK(open_store(f, 3, root, "/exact", 2, bytes));
    CHECK(create_world(f, 3, 0, 1, 0, WORLD_BYTES, MC_WORLD_STORAGE_OK));
    CHECK(create_world(f, 4, 0, 2, 0, WORLD_BYTES, MC_WORLD_STORAGE_OK));
    CHECK(create_world(f, 5, 0, 3, 0, WORLD_BYTES, MC_WORLD_STORAGE_OK));
    CHECK(construct_chunk(f, 11, 3, 0, 0, false));
    blocks = malloc(MC_CHUNK_BLOCKS);
    CHECK(blocks != NULL);
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i)
        blocks[i] = pattern(i);
    uint8_t *const limited_input = blocks;
    out = f->chunks[0].object;
    CHECK(mc_world_runtime_chunk_construct(f->worlds[3], 1, 0, &blocks, MC_CHUNK_BLOCKS, &out,
                                           &error) == MC_WORLD_STORAGE_LIMIT &&
          blocks == limited_input && out == f->chunks[0].object && error.bytecode == 0 &&
          patterned(blocks) && mc_world_store_chunk_count(f->stores[1]) == 1);
    CHECK(mc_world_runtime_chunk_construct(f->worlds[4], 0, 0, &blocks, MC_CHUNK_BLOCKS, &out,
                                           &error) == MC_WORLD_STORAGE_LIMIT &&
          blocks == limited_input && out == f->chunks[0].object && error.bytecode == 0 &&
          patterned(blocks) && mc_world_store_chunk_count(f->stores[2]) == 0);
    CHECK(mc_world_store_retire_chunk(f->stores[1], f->chunks[11].state, NULL) ==
          MC_WORLD_STORAGE_OK);
    f->chunks[11] = (chunk_slot){0};
    CHECK(mc_world_store_reserve_chunk(f->stores[1], 0, &reservation, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_store_chunk_count(f->stores[1]) == 0 &&
          mc_world_store_reservation_count(f->stores[1]) == 1);
    CHECK(mc_world_runtime_chunk_construct(f->worlds[3], 1, 0, &blocks, MC_CHUNK_BLOCKS, &out,
                                           &error) == MC_WORLD_STORAGE_LIMIT &&
          blocks == limited_input && out == f->chunks[0].object && error.bytecode == 0 &&
          patterned(blocks) && mc_world_store_chunk_count(f->stores[1]) == 0 &&
          mc_world_store_reservation_count(f->stores[1]) == 1);
    CHECK(mc_world_store_cancel_reservation(f->stores[1], reservation, NULL) ==
          MC_WORLD_STORAGE_OK);
    reservation = NULL;
    CHECK(mc_world_store_reservation_count(f->stores[1]) == 0);
    CHECK(mc_world_runtime_chunk_construct(f->worlds[3], 1, 0, &blocks, MC_CHUNK_BLOCKS,
                                           &f->chunks[12].object, &error) == MC_WORLD_STORAGE_OK);
    f->chunks[12].state = mc_world_chunk_state(f->chunks[12].object);
    f->chunks[12].store = f->stores[1];
    CHECK(blocks == NULL && f->chunks[12].state->data->blocks == limited_input &&
          patterned(limited_input) && mc_world_store_chunk_count(f->stores[1]) == 1);
    CHECK(construct_chunk(f, 13, 5, 0, 0, true));
    CHECK(mc_world_chunk_allocation_bytes(f->chunks[13].object) == bytes);
    ok = true;
cleanup:
    if (reservation != NULL)
        (void)mc_world_store_cancel_reservation(f->stores[1], reservation, NULL);
    free(blocks);
    return ok;
}

static bool clear_cache(fixture *f, size_t world) {
    if (!f->provider_live[world])
        return true;
    mc_chunk_cache *cache = f->providers[world].cache;
    if (!mc_chunk_cache_validate(cache))
        return false;
    for (size_t i = 0; i < cache->count + cache->insertion_count; ++i) {
        const mc_chunk_state *state =
            i < cache->count ? cache->entries[i].chunk : cache->insertions[i - cache->count];
        bool known = state == f->providers[world].fallback;
        for (size_t j = 0; j < COUNT(f->chunks); ++j)
            if (f->chunks[j].state != NULL && state == f->chunks[j].state)
                known = true;
        if (!known)
            return false;
    }
    /* Explicit retirement of the validated native fixture publication. This
     * is not gameplay unloading and does not touch the actual light C list. */
    cache->count = 0;
    cache->insertion_count = 0;
    return mc_chunk_cache_validate(cache);
}

static bool test_retirement(fixture *f) {
    bool ok = false, leased = false;
    uint8_t *blocks = NULL;
    mc_world_chunk_fields fields;
    mc_world_chunk_error error;
    mc_world_runtime_construction state;
    CHECK(create_world(f, 8, 0, 0, 0, WORLD_BYTES, MC_WORLD_STORAGE_OK));
    CHECK(construct_chunk(f, 10, 8, 3, -4, true));
    CHECK(mc_world_store_lease_chunk(f->stores[0], f->chunks[10].state, NULL) ==
          MC_WORLD_STORAGE_OK);
    leased = true;
    CHECK(mc_world_store_retire_chunk(f->stores[0], f->chunks[10].state, NULL) ==
          MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_store_release_chunk(f->stores[0], f->chunks[10].state, NULL) ==
          MC_WORLD_STORAGE_OK);
    leased = false;
    f->chunks[10].state->active = true;
    CHECK(mc_world_store_retire_chunk(f->stores[0], f->chunks[10].state, NULL) ==
          MC_WORLD_STORAGE_BUSY);
    f->chunks[10].state->active = false;
    CHECK(publish(f, 8, 10, 3, -4));
    CHECK(mc_world_store_retire_chunk(f->stores[0], f->chunks[10].state, NULL) ==
          MC_WORLD_STORAGE_BUSY);
    CHECK(mc_chunk_cache_key(3, -4) == mc_chunk_cache_key(32771, -4));
    CHECK(mc_chunk_cache_publish(f->providers[8].cache, 32771, -4, f->providers[8].fallback) &&
          mc_chunk_cache_validate(f->providers[8].cache) && f->providers[8].cache->count == 1 &&
          f->providers[8].cache->insertion_count == 2);
    CHECK(mc_world_store_retire_chunk(f->stores[0], f->chunks[10].state, NULL) ==
          MC_WORLD_STORAGE_BUSY);
    CHECK(clear_cache(f, 8));
    CHECK(mc_world_store_has_generated_for_world(f->stores[0], f->worlds[8]) &&
          mc_world_runtime_free(f->worlds[8], NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_runtime_retire_world(f->worlds[8], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->worlds[8], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_RETIRED);
    CHECK(mc_world_chunk_fields_get(f->chunks[10].object, &fields) == MC_WORLD_STORAGE_OK &&
          fields.d == f->worlds[8] && fields.b == f->chunks[10].state->data->blocks &&
          patterned(fields.b));
    CHECK(mc_world_runtime_chunk_finish(f->chunks[10].object, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.bytecode == 0 && patterned(fields.b));
    blocks = malloc(MC_CHUNK_BLOCKS);
    CHECK(blocks != NULL);
    memset(blocks, 0x53, MC_CHUNK_BLOCKS);
    uint8_t *const supplied = blocks;
    mc_world_chunk *out = f->chunks[10].object;
    CHECK(mc_world_runtime_chunk_construct(f->worlds[8], 0, 0, &blocks, MC_CHUNK_BLOCKS, &out,
                                           &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.bytecode == 0 && blocks == supplied && out == f->chunks[10].object &&
          bytes_are(blocks, MC_CHUNK_BLOCKS, 0x53));
    CHECK(mc_world_runtime_free(f->worlds[8], NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(!mc_world_runtime_binding_retain(f->worlds[8]));
    CHECK(mc_world_store_retire_chunk(f->stores[0], f->chunks[10].state, NULL) ==
          MC_WORLD_STORAGE_OK);
    f->chunks[10] = (chunk_slot){0};
    CHECK(!mc_world_store_has_generated_for_world(f->stores[0], f->worlds[8]) &&
          mc_world_runtime_free(f->worlds[8], NULL) == MC_WORLD_STORAGE_OK);
    f->worlds[8] = NULL;
    f->provider_live[8] = false;
    ok = true;
cleanup:
    if (leased)
        (void)mc_world_store_release_chunk(f->stores[0], f->chunks[10].state, NULL);
    free(blocks);
    return ok;
}

static bool cleanup_fixture(fixture *f) {
    if (f == NULL)
        return true;
    for (size_t i = 0; i < COUNT(f->worlds); ++i)
        if (!clear_cache(f, i))
            return false;
    if (f->loaded != NULL) {
        f->loaded->active = false;
        if (mc_world_store_retire_chunk(f->stores[0], f->loaded, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->loaded = NULL;
    }
    if (f->finished_loaded != NULL) {
        f->finished_loaded->active = false;
        if (mc_world_store_retire_chunk(f->stores[0], f->finished_loaded, NULL) !=
            MC_WORLD_STORAGE_OK)
            return false;
        f->finished_loaded = NULL;
    }
    for (size_t i = 0; i < COUNT(f->chunks); ++i) {
        if (f->chunks[i].state == NULL)
            continue;
        f->chunks[i].state->active = false;
        if (mc_world_store_retire_chunk(f->chunks[i].store, f->chunks[i].state, NULL) !=
            MC_WORLD_STORAGE_OK)
            return false;
        f->chunks[i] = (chunk_slot){0};
    }
    for (size_t i = 0; i < COUNT(f->worlds); ++i) {
        if (f->worlds[i] == NULL)
            continue;
        if (f->world_hold[i]) {
            mc_world_runtime_binding_release(f->worlds[i]);
            f->world_hold[i] = false;
        }
        if (mc_world_runtime_retire_world(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK ||
            mc_world_runtime_free(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->worlds[i] = NULL;
        f->provider_live[i] = false;
    }
    for (size_t i = 0; i < COUNT(f->dimensions); ++i)
        mc_world_provider_release(f->dimensions[i]);
    for (size_t i = 0; i < COUNT(f->classes); ++i)
        if (mc_world_runtime_classes_free(f->classes[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
    for (size_t i = 0; i < COUNT(f->registries); ++i)
        if (mc_registry_classes_free(f->registries[i], NULL) != MC_REGISTRY_OK)
            return false;
    mc_world_info_name_release(f->name);
    for (size_t i = 0; i < COUNT(f->stores); ++i)
        mc_world_store_free(f->stores[i]);
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    return true;
}

static bool decode_path(const char *hex, char *out, size_t capacity) {
    const size_t n = strlen(hex);
    if (n == 0 || n % 2u != 0 || n / 2u >= capacity)
        return false;
    for (size_t i = 0; i < n / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            const unsigned char c = (unsigned char)hex[2 * i + j];
            const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
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

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'C', 'h', 'u', 'n', 'k'};
    if (argc != 3 || strcmp(argv[1], "world-chunk-objects") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(open_store(f, 0, root, "", 32, mc_nbt_default_limits().max_allocation_bytes));
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t i = 0; i < COUNT(f->classes); ++i) {
        resource_source *source = &f->resources[i];
        source->streams[0] = (byte_stream){.bytes = language, .length = sizeof(language) - 1};
        source->streams[1] = (byte_stream){.bytes = language};
        source->streams[2] = (byte_stream){.bytes = language};
        source->fail_first = i == 1;
        source->api =
            (mc_registry_resources){source, open_resource, close_resource, resource_ready};
        const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                             &source->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registries[i], NULL) == MC_REGISTRY_OK);
        source->registry = f->registries[i];
        CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                              &f->classes[i], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[i], f->registries[i], NULL) ==
              MC_REGISTRY_OK);
    }
    CHECK(create_world(f, 0, 0, 0, 0, WORLD_BYTES, MC_WORLD_STORAGE_OK));
    CHECK(test_constructor(f));
    CHECK(test_q(f));
    CHECK(test_finish_source(f));
    CHECK(test_no_sky_prefix(f));
    CHECK(test_neighbor_prefixes(f));
    CHECK(test_registry_failure(f));
    CHECK(test_admission_and_limits(f, root));
    CHECK(test_serialization(f));
    CHECK(test_retirement(f));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("chunk fixture retirement refused; live dependencies retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u constructed Chunk object checks (world-chunk-objects)\n", checks) < 0)
        result = 1;
    return result;
}
