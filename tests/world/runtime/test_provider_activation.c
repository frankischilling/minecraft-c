#include "mc/block/registry/falling.h"
#include "mc/block/registry/runtime.h"
#include "mc/resources/translation/translation.h"
#include "mc/world/biome/registry.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/lighting.h"
#include "mc/world/runtime/tiles/tiles.h"

#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/chunk/object_internal.h"
#include "../../../src/world/runtime/acquisition_internal.h"
#include "../../../src/world/runtime/activation_internal.h"
#include "../../../src/world/runtime/provider_internal.h"
#include "../../../src/world/storage/objects_internal.h"
#include "../../support/java_random.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Native owned-file integration contracts. No source collection/World/Chunk
 * owners are fabricated. Empty registration order is independently reviewed
 * from primary bodies; unchanged empty counts do not prove eight callbacks.
 * Complete original generation comparisons remain a separate qualification. */
static unsigned int checks;
static mc_math_trig trig;
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";
static const uint16_t root_title[] = {'R', 0, 0xdfff};
static const uint16_t unknown_text[] = {'U', 0, 0xd800};
static const uint8_t unknown_bytes[] = {0, 0x80, 0xff, 7};

typedef struct stream {
    const uint8_t *data;
    size_t length, offset;
    bool closed;
} stream;

typedef struct resources {
    stream streams[3];
    mc_registry_resources api;
    mc_registry_classes *registry;
    mc_world_runtime *world;
    mc_world_store *store;
    const mc_chunk_acquire_context *acquisition;
    mc_chunk_state **ordinary_out;
    mc_chunk_state *sentinel;
    size_t count_before, opens, closes, guarded;
    bool watch, bad;
} resources;

typedef struct child {
    mc_world_store *store;
    mc_chunk_state *state;
} child;

typedef struct fixture {
    mc_world_regions *regions;
    mc_world_store *stores[3];
    mc_world_info_name *name;
    mc_materials *materials;
    mc_java_random_profile *profile;
    resources resources[2];
    mc_registry_classes *registry[2];
    mc_world_runtime_classes *classes[2];
    mc_world_runtime *worlds[4];
    mc_world_dimension *dimensions[4];
    mc_world_provider_fields provider[4];
    bool bound[4];
    child children[24];
    bool bad;
} fixture;

typedef struct record {
    mc_nbt_tag root, level, fields[11], extra_root, extra_level, tail, opaque, opaque_id;
    const mc_nbt_tag *root_items[2], *level_items[13], *opaque_items[1], *opaque_fields[1];
    uint16_t names[16][24];
    uint8_t arrays[5][MC_CHUNK_BLOCKS];
} record;

static uint64_t profile_u(const fixture *f)
{
    return atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
}

static bool remember(fixture *f, mc_world_store *store, mc_chunk_state *state)
{
    if (state == NULL || !mc_world_store_owns_chunk(store, state))
        return false;
    for (size_t i = 0; i < COUNT(f->children); ++i) {
        if (f->children[i].state == state)
            return f->children[i].store == store;
        if (f->children[i].state == NULL) {
            f->children[i] = (child){store, state};
            return true;
        }
    }
    return false;
}

static bool observe_cache(fixture *f, size_t world)
{
    const mc_world_provider_fields *p = &f->provider[world];
    if (!mc_chunk_cache_validate(p->cache))
        return false;
    for (size_t i = 0; i < p->cache->count; ++i)
        if (!remember(f, p->store, p->cache->entries[i].chunk))
            return false;
    for (size_t i = 0; i < p->cache->insertion_count; ++i)
        if (!remember(f, p->store, p->cache->insertions[i]))
            return false;
    return true;
}

static mc_chunk_acquire_status acquire(fixture *f, size_t world, int32_t x, int32_t z, bool getter,
                                       mc_chunk_state **out)
{
    const mc_chunk_acquire_status status =
        getter ? mc_world_runtime_get(f->worlds[world], x, z, out)
               : mc_world_runtime_acquire(f->worlds[world], x, z, out);
    if (!observe_cache(f, world))
        f->bad = true;
    return status;
}

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count)
{
    stream *s = context;
    if (out == NULL || count == NULL || capacity == 0 || s->closed || s->offset > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > 17)
        take = 17;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->data + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out)
{
    resources *r = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    if (name == NULL || out == NULL || r->opens >= COUNT(names) ||
        strcmp(name, names[r->opens]) != 0) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    const size_t index = r->opens++;
    if (r->watch) {
        /* Genuine first-generation resource callback, before cache publication.
         * The entry already linked its actual unpublished store-owned child. */
        if (r->world == NULL || r->store == NULL || r->acquisition == NULL ||
            r->ordinary_out == NULL || *r->ordinary_out != r->sentinel ||
            r->acquisition->cache->count != 0 || r->acquisition->cache->insertion_count != 0 ||
            mc_world_store_chunk_count(r->store) != r->count_before + 1u ||
            !mc_world_store_has_chunks_for_world(r->store, r->world) ||
            mc_world_runtime_retire_world(r->world, NULL) != MC_WORLD_STORAGE_BUSY ||
            mc_world_runtime_free(r->world, NULL) != MC_WORLD_STORAGE_BUSY ||
            mc_registry_classes_free(r->registry, NULL) != MC_REGISTRY_BUSY)
            r->bad = true;
        ++r->guarded;
    }
    *out = (mc_java_properties_input){&r->streams[index], read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input)
{
    resources *r = context;
    if (input == NULL || input->context != &r->streams[2] || input->read != read_bytes) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    r->streams[2].closed = true;
    ++r->closes;
    return MC_REGISTRY_OK;
}

static bool resource_ready(void *context, const mc_java_properties_input *input)
{
    resources *r = context;
    if (input == NULL || input->read != read_bytes)
        r->bad = true;
    return false;
}

static bool open_store(fixture *f, size_t slot, const char *root, const char *suffix, size_t limit)
{
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
    options.limits.max_loaded_chunks = limit;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool create_world(fixture *f, size_t world, size_t registry, size_t store, size_t capacity)
{
    bool ok = false;
    mc_world_runtime_fields fields;
    mc_world_runtime_construction state;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = capacity;
    CHECK(mc_world_dimension_create(0, NULL, &f->dimensions[world], NULL) == MC_WORLD_DIMENSION_OK);
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                          f->classes[registry], &limits, &f->worlds[world],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->stores[store], .name = f->name, .seed = 23, .dimension = f->dimensions[world]};
    CHECK(mc_world_runtime_construct_world(f->worlds[world], &args, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_fields_get(f->worlds[world], &fields) == MC_WORLD_STORAGE_OK &&
          fields.v != NULL);
    CHECK(mc_world_provider_bind_fields(fields.v, &f->provider[world], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[world] = true;
    CHECK(mc_world_runtime_construction_get(f->worlds[world], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY && state.contexts_bound && state.cache_registered);
    CHECK(f->provider[world].world == f->worlds[world] &&
          f->provider[world].store == f->stores[store] && f->provider[world].generator != NULL &&
          f->provider[world].manager != NULL && f->provider[world].generation != NULL &&
          f->provider[world].pending != NULL && mc_chunk_cache_validate(f->provider[world].cache) &&
          f->provider[world].cache->count == 0 && f->provider[world].cache->insertion_count == 0 &&
          f->provider[world].pending->count == 0);
    ok = true;
cleanup:
    return ok;
}

static bool check_children(fixture *f, size_t world, mc_chunk_state *state)
{
    bool ok = false;
    mc_world_chunk_fields fields;
    mc_world_chunk_nibble_fields metadata, sky, light;
    const mc_world_chunk_entity_list *slices[8] = {NULL};
    mc_world_chunk *const object = mc_world_store_chunk_object(f->provider[world].store, state);
    CHECK(object != NULL && mc_world_chunk_state(object) == state &&
          mc_world_chunk_fields_get(object, &fields) == MC_WORLD_STORAGE_OK);
    CHECK(fields.d == f->worlds[world] && fields.b == state->data->blocks &&
          fields.h == state->data->heightmap && fields.j == state->x && fields.k == state->z &&
          fields.c == state->active && fields.n == state->terrain_populated &&
          fields.o == state->dirty);
    CHECK(fields.e != NULL && fields.f != NULL && fields.g != NULL && fields.e != fields.f &&
          fields.e != fields.g && fields.f != fields.g && fields.l != NULL && fields.m != NULL);
    CHECK(mc_world_chunk_nibble_fields_get(fields.e, &metadata) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_nibble_fields_get(fields.f, &sky) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_nibble_fields_get(fields.g, &light) == MC_WORLD_STORAGE_OK &&
          metadata.a == state->data->metadata && sky.a == state->data->skylight &&
          light.a == state->data->block_light && metadata.length == MC_CHUNK_NIBBLE_BYTES &&
          sky.length == MC_CHUNK_NIBBLE_BYTES && light.length == MC_CHUNK_NIBBLE_BYTES);
    size_t count = 777;
    CHECK(mc_world_chunk_tile_count(fields.l, &count) == MC_WORLD_STORAGE_OK && count == 0);
    for (size_t i = 0; i < COUNT(slices); ++i) {
        CHECK(mc_world_chunk_slice_at(fields.m, i, &slices[i]) == MC_WORLD_STORAGE_OK &&
              slices[i] != NULL);
        count = 777;
        CHECK(mc_world_chunk_entity_count(slices[i], &count) == MC_WORLD_STORAGE_OK && count == 0);
        for (size_t j = 0; j < i; ++j)
            CHECK(slices[i] != slices[j]);
    }
    ok = true;
cleanup:
    return ok;
}

static bool generated_and_cached(fixture *f)
{
    bool ok = false;
    mc_chunk_state *out = NULL, *first = NULL;
    mc_world_runtime_fields fields;
    bool changed = false;
    const mc_chunk_acquire_context *const acquisition = mc_world_runtime_acquisition(f->worlds[0]);
    resources *r = &f->resources[0];
    CHECK(acquisition != NULL && acquisition->cache == f->provider[0].cache);
    const int32_t key = mc_chunk_cache_key(8, 0);
    CHECK(mc_chunk_pending_add(f->provider[0].pending, key, &changed) == MC_CHUNK_ACQUIRE_OK &&
          changed);
    r->world = f->worlds[0];
    r->store = f->stores[0];
    r->acquisition = acquisition;
    r->ordinary_out = &out;
    r->sentinel = NULL;
    r->count_before = mc_world_store_chunk_count(f->stores[0]);
    r->watch = true;
    CHECK(acquire(f, 0, 8, 0, false, &out) == MC_CHUNK_ACQUIRE_OK);
    r->watch = false;
    r->ordinary_out = NULL;
    first = out;
    CHECK(first != NULL && first->active && first->dirty && !first->terrain_populated &&
          first->x == 8 && first->z == 0 && f->provider[0].pending->count == 0 &&
          f->provider[0].cache->count == 1 && f->provider[0].cache->insertion_count == 1 &&
          f->provider[0].cache->insertions[0] == first && r->opens == 3 && r->closes == 1 &&
          r->guarded == 3 && !r->bad && mc_world_store_chunk_count(f->stores[0]) == 1);
    CHECK(check_children(f, 0, first));
    CHECK(mc_world_runtime_fields_get(f->worlds[0], &fields) == MC_WORLD_STORAGE_OK &&
          fields.b != NULL && fields.c != NULL && fields.G != NULL && fields.b != fields.c &&
          fields.b != fields.G && fields.c != fields.G);
    const mc_java_random world_random = *fields.r;
    const mc_java_random generator = f->provider[0].generator->generator_random;
    const mc_java_random cave = f->provider[0].generator->cave_random;
    const mc_biome_reference_array *const v = f->provider[0].generation->v;
    const double *const climate = f->provider[0].manager->a.data;
    const uint64_t u = profile_u(f);
    CHECK(mc_chunk_pending_add(f->provider[0].pending, key, &changed) == MC_CHUNK_ACQUIRE_OK &&
          changed);
    out = NULL;
    CHECK(acquire(f, 0, 8, 0, true, &out) == MC_CHUNK_ACQUIRE_OK && out == first &&
          mc_chunk_pending_contains(f->provider[0].pending, key));
    out = NULL;
    CHECK(mc_chunk_cache_key(8, 0) == mc_chunk_cache_key(32776, 0));
    CHECK(acquire(f, 0, 32776, 0, true, &out) == MC_CHUNK_ACQUIRE_OK && out == first &&
          first->x == 8);
    CHECK(acquire(f, 0, 8, 0, false, &out) == MC_CHUNK_ACQUIRE_OK && out == first &&
          !mc_chunk_pending_contains(f->provider[0].pending, key));
    CHECK(mc_world_store_chunk_count(f->stores[0]) == 1 &&
          f->provider[0].cache->insertion_count == 1 &&
          mc_test_java_random_equal(fields.r, &world_random) &&
          mc_test_java_random_equal(&f->provider[0].generator->generator_random, &generator) &&
          mc_test_java_random_equal(&f->provider[0].generator->cave_random, &cave) &&
          profile_u(f) == u && f->provider[0].generation->v == v &&
          f->provider[0].manager->a.data == climate);
    CHECK(mc_world_store_retire_chunk(f->stores[0], first, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_runtime_retire_world(f->worlds[0], NULL) == MC_WORLD_STORAGE_BUSY);
    ok = true;
cleanup:
    r->watch = false;
    r->ordinary_out = NULL;
    return ok;
}

static bool population_completion(fixture *f)
{
    bool ok = false, completed = false;
    mc_world_info_fields info_before, info_after;
    mc_chunk_state *square[4] = {NULL};
    const int32_t x[] = {0, 1, 0}, z[] = {0, 0, 1};
    for (size_t i = 0; i < COUNT(x); ++i) {
        CHECK(acquire(f, 0, x[i], z[i], true, &square[i]) == MC_CHUNK_ACQUIRE_OK);
        CHECK(square[i]->active && !square[i]->terrain_populated &&
              check_children(f, 0, square[i]));
    }
    CHECK(mc_world_info_fields_get(mc_world_runtime_info(f->worlds[0]), &info_before) ==
              MC_WORLD_INFO_OK &&
          info_before.value.size_on_disk == 0 && mc_world_store_info(f->stores[0]) == NULL);
    CHECK(mc_world_runtime_save_chunks(f->worlds[0], false, &completed) == MC_CHUNK_LIFECYCLE_OK &&
          completed);
    CHECK(mc_world_info_fields_get(mc_world_runtime_info(f->worlds[0]), &info_after) ==
              MC_WORLD_INFO_OK &&
          info_after.value.size_on_disk > info_before.value.size_on_disk &&
          mc_world_store_info(f->stores[0]) == NULL);
    for (size_t i = 0; i < COUNT(x); ++i)
        CHECK(!square[i]->dirty && !square[i]->terrain_populated);
    mc_chunk_state *out = f->children[0].state;
    mc_world_runtime_acquisition_error population;
    CHECK(mc_world_runtime_get_detailed(f->worlds[0], 1, 1, &out, &population) ==
              MC_CHUNK_ACQUIRE_OK &&
          out != NULL && !population.population.reached &&
          population.cause.status == MC_WORLD_STORAGE_OK &&
          population.registry.status == MC_REGISTRY_OK);
    CHECK(observe_cache(f, 0));
    CHECK(mc_chunk_cache_lookup(f->provider[0].cache, 1, 1, &square[3]) && square[3] != NULL &&
          out == square[3] && square[3]->active && square[3]->dirty &&
          !square[3]->terrain_populated && square[0]->terrain_populated && square[0]->dirty &&
          !square[1]->terrain_populated && !square[2]->terrain_populated);
    CHECK(check_children(f, 0, square[3]));
    /* The actual generated-terrain controller now continues through notifying
     * snow placements and resets its shared falling flag on normal return.
     * A hit or explicit populate must not repeat the marked candidate. */
    bool falling = true;
    CHECK(mc_registry_falling_flag_get(f->registry[0], &falling, NULL) == MC_REGISTRY_OK &&
          !falling);
    const mc_block *snow = NULL;
    CHECK(mc_registry_block_static_get(f->registry[0], "aT", &snow, NULL) == MC_REGISTRY_OK &&
          snow != NULL);
    size_t snow_count = 0;
    for (size_t i = 0; i < COUNT(square); ++i)
        for (size_t j = 0; j < MC_CHUNK_BLOCKS; ++j)
            if (square[i]->data->blocks[j] == mc_block_fields_get(snow)->bn)
                ++snow_count;
    CHECK(snow_count != 0);
    const size_t count = mc_world_store_chunk_count(f->stores[0]);
    const mc_java_random generator = f->provider[0].generator->generator_random;
    const mc_java_random cave = f->provider[0].generator->cave_random;
    out = NULL;
    CHECK(acquire(f, 0, 1, 1, true, &out) == MC_CHUNK_ACQUIRE_OK && out == square[3]);
    CHECK(mc_world_runtime_populate(f->worlds[0], 0, 0) == MC_CHUNK_ACQUIRE_OK &&
          square[0]->terrain_populated && square[0]->dirty &&
          mc_world_store_chunk_count(f->stores[0]) == count &&
          mc_test_java_random_equal(&f->provider[0].generator->generator_random, &generator) &&
          mc_test_java_random_equal(&f->provider[0].generator->cave_random, &cave));
    ok = true;
cleanup:
    return ok;
}

static mc_nbt_string ascii(const char *text, uint16_t *out)
{
    const size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i)
        out[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){out, length};
}

static const mc_nbt_tag *field(const mc_nbt_tag *tag, const char *name)
{
    uint16_t units[32];
    const mc_nbt_string key = ascii(name, units);
    return mc_nbt_compound_get(tag, key.units, key.length);
}

static bool text_is(mc_nbt_string value, const uint16_t *text, size_t length)
{
    return value.length == length &&
           (length == 0 ||
            (value.units != NULL && memcmp(value.units, text, length * sizeof(*text)) == 0));
}

static uint8_t input_byte(size_t array, size_t i)
{
    return (uint8_t)((i * (2u * array + 1u) + array * 37u + 11u) & 255u);
}

static bool write_record_at(fixture *f, int32_t x, int32_t z, int32_t stored_x, int32_t stored_z,
                            bool entities, bool tiles)
{
    bool ok = false;
    record *r = NULL;
    uint8_t *encoded = NULL;
    size_t length = 0, written = 0;
    const char *const names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                 MC_NBT_LIST,       MC_NBT_LIST};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    r = calloc(1, sizeof(*r));
    CHECK(r != NULL);
    r->root.type = MC_NBT_COMPOUND;
    r->root.name = (mc_nbt_string){root_title, COUNT(root_title)};
    r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_items[i + 1u] = &r->fields[i];
    }
    r->fields[0].value.int_value = stored_x;
    r->fields[1].value.int_value = stored_z;
    r->fields[2].value.long_value = -123;
    r->fields[8].value.byte_value = 1;
    for (size_t a = 0; a < COUNT(lengths); ++a) {
        for (size_t i = 0; i < lengths[a]; ++i)
            r->arrays[a][i] = input_byte(a, i);
        r->fields[a + 3u].value.byte_array.data = r->arrays[a];
        r->fields[a + 3u].value.byte_array.length = lengths[a];
    }
    r->opaque.type = MC_NBT_COMPOUND;
    r->opaque_id.type = MC_NBT_STRING;
    r->opaque_id.name = ascii("id", r->names[14]);
    r->opaque_id.value.string = (mc_nbt_string){unknown_text, COUNT(unknown_text)};
    r->opaque_fields[0] = &r->opaque_id;
    r->opaque.value.compound.items = r->opaque_fields;
    r->opaque.value.compound.length = 1;
    r->opaque_items[0] = &r->opaque;
    for (size_t i = 9; i <= 10; ++i) {
        r->fields[i].value.list.element_type = MC_NBT_COMPOUND;
        r->fields[i].value.list.items = r->opaque_items;
        r->fields[i].value.list.length = (i == 9 ? entities : tiles) ? 1u : 0u;
    }
    r->extra_root.type = MC_NBT_STRING;
    r->extra_root.name = ascii("RootUnknown", r->names[12]);
    r->extra_root.value.string = (mc_nbt_string){unknown_text, COUNT(unknown_text)};
    r->extra_level.type = MC_NBT_BYTE_ARRAY;
    r->extra_level.name = ascii("LevelUnknown", r->names[13]);
    r->extra_level.value.byte_array.data = unknown_bytes;
    r->extra_level.value.byte_array.length = sizeof(unknown_bytes);
    r->tail.type = MC_NBT_FLOAT;
    r->tail.name = ascii("SignedZero", r->names[15]);
    r->tail.value.float_bits = UINT32_C(0x80000000);
    r->level_items[0] = &r->extra_level;
    r->level_items[12] = &r->tail;
    r->level.value.compound.items = r->level_items;
    r->level.value.compound.length = COUNT(r->level_items);
    r->root_items[0] = &r->extra_root;
    r->root_items[1] = &r->level;
    r->root.value.compound.items = r->root_items;
    r->root.value.compound.length = COUNT(r->root_items);
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK && length != 0);
    encoded = malloc(length);
    CHECK(encoded != NULL);
    CHECK(mc_nbt_write(&r->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = 0;
    /* Explicit native source fixture transport, not a GAME clock injection. */
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[1]), x, z, encoded,
                                 length, 1, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool write_record(fixture *f, int32_t x, int32_t z, bool entities, bool tiles)
{
    return write_record_at(f, x, z, x, z, entities, tiles);
}

static bool patterned(const mc_chunk_data *data)
{
    const uint8_t *const arrays[] = {data->blocks, data->metadata, data->skylight,
                                     data->block_light, data->heightmap};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    for (size_t a = 0; a < COUNT(arrays); ++a)
        for (size_t i = 0; i < lengths[a]; ++i)
            if (arrays[a][i] != input_byte(a, i))
                return false;
    return true;
}

static bool disk_unknowns(fixture *f, int32_t x, int32_t z, const mc_chunk_state *current)
{
    bool ok = false;
    mc_compression_buffer bytes = {0};
    mc_nbt_document *document = NULL;
    CHECK(mc_world_regions_read(f->regions, mc_world_store_root(f->stores[1]), x, z, &bytes,
                                NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_nbt_read_compound(bytes.data, bytes.length, NULL, &document, NULL, NULL) == MC_NBT_OK);
    const mc_nbt_tag *const root = mc_nbt_document_root(document);
    const mc_nbt_tag *const level = field(root, "Level");
    const mc_nbt_tag *const extra = field(root, "RootUnknown");
    const mc_nbt_tag *const payload = field(level, "LevelUnknown");
    const mc_nbt_tag *const zero = field(level, "SignedZero");
    CHECK(root != NULL && text_is(root->name, root_title, COUNT(root_title)) && level != NULL &&
          extra != NULL && extra->type == MC_NBT_STRING &&
          text_is(extra->value.string, unknown_text, COUNT(unknown_text)) && payload != NULL &&
          payload->type == MC_NBT_BYTE_ARRAY &&
          payload->value.byte_array.length == sizeof(unknown_bytes) &&
          memcmp(payload->value.byte_array.data, unknown_bytes, sizeof(unknown_bytes)) == 0 &&
          zero != NULL && zero->type == MC_NBT_FLOAT &&
          zero->value.float_bits == UINT32_C(0x80000000));
    const char *const names[] = {"Blocks", "Data", "SkyLight", "BlockLight", "HeightMap"};
    const uint8_t *const arrays[] = {current->data->blocks, current->data->metadata,
                                     current->data->skylight, current->data->block_light,
                                     current->data->heightmap};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    for (size_t i = 0; i < COUNT(names); ++i) {
        const mc_nbt_tag *const tag = field(level, names[i]);
        CHECK(tag != NULL && tag->type == MC_NBT_BYTE_ARRAY &&
              tag->value.byte_array.length == lengths[i] &&
              memcmp(tag->value.byte_array.data, arrays[i], lengths[i]) == 0);
    }
    const mc_nbt_tag *const entities = field(level, "Entities");
    const mc_nbt_tag *const tiles = field(level, "TileEntities");
    const mc_nbt_tag *const time = field(level, "LastUpdate");
    CHECK(entities != NULL && entities->type == MC_NBT_LIST && entities->value.list.length == 0 &&
          tiles != NULL && tiles->type == MC_NBT_LIST && tiles->value.list.length == 0 &&
          time != NULL && time->type == MC_NBT_LONG && time->value.long_value == 4567);
    ok = true;
cleanup:
    mc_nbt_document_free(document);
    mc_compression_buffer_free(&bytes);
    return ok;
}

static bool loaded_and_persisted(fixture *f)
{
    bool ok = false, completed = false;
    mc_chunk_state *raw = NULL, *loaded = NULL, *again = NULL;
    mc_world_runtime_fields world;
    mc_registry_block_view blocks;
    mc_biome_registry_view biomes;
    CHECK(write_record(f, -2, 3, false, false));
    CHECK(mc_world_runtime_fields_get(f->worlds[1], &world) == MC_WORLD_STORAGE_OK);
    const mc_java_random world_random = *world.r;
    const mc_java_random generator = f->provider[1].generator->generator_random;
    const mc_java_random cave = f->provider[1].generator->cave_random;
    const uint64_t u = profile_u(f);
    CHECK(mc_world_store_load_game_chunk(f->stores[1], f->worlds[1], 0, -2, 3, &raw, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(remember(f, f->stores[1], raw));
    CHECK(raw != NULL && raw->x == -2 && raw->z == 3 && !raw->active && !raw->dirty &&
          raw->terrain_populated && raw->last_save_time == 0 && raw->data->minimum_height == 0 &&
          patterned(raw->data) && check_children(f, 1, raw) &&
          mc_world_store_has_chunks_for_world(f->stores[1], f->worlds[1]));
    *mc_world_runtime_live(f->worlds[1])->time = 3456;
    CHECK(acquire(f, 1, -2, 3, true, &loaded) == MC_CHUNK_ACQUIRE_OK && loaded != raw &&
          loaded->data != raw->data && loaded->data->blocks != raw->data->blocks &&
          loaded->x == -2 && loaded->z == 3 && loaded->active && !loaded->dirty &&
          loaded->terrain_populated && loaded->last_save_time == 3456 &&
          loaded->data->minimum_height == 0 && patterned(loaded->data) &&
          check_children(f, 1, loaded));
    mc_world_chunk_fields before, after;
    mc_world_chunk *const object = mc_world_store_chunk_object(f->stores[1], loaded);
    CHECK(mc_world_chunk_fields_get(object, &before) == MC_WORLD_STORAGE_OK);
    loaded->data->blocks[17] ^= 0x80u;
    loaded->data->metadata[11] ^= 0x40u;
    loaded->data->skylight[13] ^= 0x20u;
    loaded->data->block_light[19] ^= 0x10u;
    loaded->data->heightmap[23] ^= 0x08u;
    loaded->dirty = true; /* Ordinary public mutable native Chunk state. */
    *mc_world_runtime_live(f->worlds[1])->time = 4567;
    CHECK(mc_world_runtime_save_chunks(f->worlds[1], false, &completed) == MC_CHUNK_LIFECYCLE_OK &&
          completed);
    CHECK(!loaded->dirty && loaded->active && loaded->last_save_time == 4567 &&
          mc_world_chunk_fields_get(object, &after) == MC_WORLD_STORAGE_OK && before.b == after.b &&
          before.d == after.d && before.e == after.e && before.f == after.f &&
          before.g == after.g && before.h == after.h && before.l == after.l &&
          before.m == after.m && patterned(raw->data));
    CHECK(disk_unknowns(f, -2, 3, loaded));
    CHECK(mc_world_store_load_game_chunk(f->stores[1], f->worlds[1], 0, -2, 3, &again, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(remember(f, f->stores[1], again));
    CHECK(again != loaded && again != raw && again->data != loaded->data && again->x == -2 &&
          again->z == 3 && !again->active && !again->dirty && again->last_save_time == 0 &&
          check_children(f, 1, again));
    CHECK(memcmp(again->data->blocks, loaded->data->blocks, MC_CHUNK_BLOCKS) == 0 &&
          memcmp(again->data->metadata, loaded->data->metadata, MC_CHUNK_NIBBLE_BYTES) == 0 &&
          memcmp(again->data->skylight, loaded->data->skylight, MC_CHUNK_NIBBLE_BYTES) == 0 &&
          memcmp(again->data->block_light, loaded->data->block_light, MC_CHUNK_NIBBLE_BYTES) == 0 &&
          memcmp(again->data->heightmap, loaded->data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES) == 0);
    CHECK(profile_u(f) == u && mc_test_java_random_equal(world.r, &world_random) &&
          mc_test_java_random_equal(&f->provider[1].generator->generator_random, &generator) &&
          mc_test_java_random_equal(&f->provider[1].generator->cave_random, &cave) &&
          f->provider[1].generation->v == NULL && f->resources[1].opens == 0 &&
          mc_registry_classes_inspect_blocks(f->registry[1], &blocks, NULL) == MC_REGISTRY_OK &&
          blocks.phase == MC_REGISTRY_COLD &&
          mc_biome_registry_inspect(f->registry[1], &biomes, NULL) == MC_REGISTRY_OK &&
          biomes.phase == MC_REGISTRY_COLD);
    ok = true;
cleanup:
    return ok;
}

static bool opaque_refusal(fixture *f)
{
    bool ok = false;
    mc_compression_buffer before = {0}, after = {0};
    mc_chunk_state *const sentinel = f->children[0].state;
    for (size_t i = 0; i < 2; ++i) {
        const int32_t x = (int32_t)(40u + i);
        CHECK(write_record(f, x, 4, i == 0, i == 1));
        CHECK(mc_world_regions_read(f->regions, mc_world_store_root(f->stores[1]), x, 4, &before,
                                    NULL) == MC_WORLD_STORAGE_OK);
        const size_t count = mc_world_store_chunk_count(f->stores[1]);
        const size_t stages = mc_world_store_game_stage_count(f->stores[1]);
        const size_t cached = f->provider[1].cache->count;
        mc_chunk_state *out = sentinel;
        CHECK(mc_world_store_load_game_chunk(f->stores[1], f->worlds[1], 0, x, 4, &out, NULL) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              out == sentinel && mc_world_store_chunk_count(f->stores[1]) == count);
        mc_world_runtime_acquisition_error detail;
        CHECK(mc_world_runtime_get_detailed(f->worlds[1], x, 4, &out, &detail) ==
                  MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
              out == sentinel && detail.cause.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              detail.load_bytecode == 143 &&
              detail.load_builder_bytecode == (i == 0 ? 269u : 338u) &&
              mc_world_store_chunk_count(f->stores[1]) == count + 1 &&
              mc_world_store_game_stage_count(f->stores[1]) == stages + 1 &&
              f->provider[1].cache->count == cached &&
              !mc_chunk_cache_contains(f->provider[1].cache, x, 4) && f->resources[1].opens == 0 &&
              f->provider[1].generation->v == NULL);
        mc_world_store_game_stage *stage = NULL;
        mc_world_store_game_stage_view view;
        CHECK(mc_world_store_game_stage_at(f->stores[1], stages, &stage, &view, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              stage != NULL && view.phase == MC_WORLD_STORE_GAME_FAILED && view.state != NULL &&
              view.chunk != NULL && !view.state->active && !view.state->has_entities &&
              !mc_world_chunk_source_enabled(view.chunk) &&
              mc_world_store_chunk_object(f->stores[1], view.state) == view.chunk &&
              remember(f, f->stores[1], view.state));
        for (size_t cell = 0; cell < MC_CHUNK_BLOCKS; ++cell)
            CHECK(view.state->data->blocks[cell] == input_byte(0, cell));
        CHECK(mc_world_store_retire_chunk(f->stores[1], view.state, NULL) == MC_WORLD_STORAGE_BUSY);
        if (i == 1) {
            mc_live_tile_error tile_error;
            CHECK(mc_live_tiles_remove(f->worlds[1], x * 16, 64, 4 * 16, &tile_error) ==
                      MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  tile_error.status == tile_error.cause.status && tile_error.bytecode == 338 &&
                  tile_error.load_bytecode == 143 && tile_error.load_builder_bytecode == 338 &&
                  tile_error.acquisition_parent.remove_bytecode == 4 &&
                  tile_error.acquisition_parent.bytecode ==
                      tile_error.acquisition_caller_bytecode &&
                  tile_error.acquisition_parent.bytecode != 0 &&
                  mc_world_store_game_stage_count(f->stores[1]) == stages + 2 &&
                  !mc_chunk_cache_contains(f->provider[1].cache, x, 4));
            mc_world_store_game_stage_view nested_view;
            CHECK(mc_world_store_game_stage_at(f->stores[1], stages + 1, &stage, &nested_view,
                                               NULL) == MC_WORLD_STORAGE_OK &&
                  nested_view.phase == MC_WORLD_STORE_GAME_FAILED &&
                  nested_view.state != view.state && nested_view.chunk != view.chunk &&
                  remember(f, f->stores[1], nested_view.state));
        }
        CHECK(mc_world_regions_read(f->regions, mc_world_store_root(f->stores[1]), x, 4, &after,
                                    NULL) == MC_WORLD_STORAGE_OK &&
              before.length == after.length && memcmp(before.data, after.data, before.length) == 0);
        mc_compression_buffer_free(&before);
        mc_compression_buffer_free(&after);
        before = (mc_compression_buffer){0};
        after = (mc_compression_buffer){0};
    }
    ok = true;
cleanup:
    mc_compression_buffer_free(&before);
    mc_compression_buffer_free(&after);
    return ok;
}

static bool relocation_frontier(fixture *f)
{
    bool ok = false, leased = false;
    mc_compression_buffer before = {0}, after = {0};
    mc_chunk_state *out = f->children[0].state;
    mc_world_store_game_stage *stage = NULL;
    mc_world_store_game_stage_view view = {0};
    const size_t count = mc_world_store_chunk_count(f->stores[1]);
    const size_t stages = mc_world_store_game_stage_count(f->stores[1]);
    CHECK(write_record_at(f, 42, 4, 43, 4, false, false));
    CHECK(mc_world_regions_read(f->regions, mc_world_store_root(f->stores[1]), 42, 4, &before,
                                NULL) == MC_WORLD_STORAGE_OK);
    mc_world_runtime_acquisition_error detail;
    CHECK(mc_world_runtime_get_detailed(f->worlds[1], 42, 4, &out, &detail) ==
              MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
          out == f->children[0].state &&
          detail.cause.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          detail.load_bytecode == 158 && detail.load_builder_bytecode == 0 &&
          mc_world_store_chunk_count(f->stores[1]) == count + 1 &&
          mc_world_store_game_stage_count(f->stores[1]) == stages + 1 &&
          !mc_chunk_cache_contains(f->provider[1].cache, 42, 4));
    CHECK(mc_world_store_game_stage_at(f->stores[1], stages, &stage, &view, NULL) ==
              MC_WORLD_STORAGE_OK &&
          view.phase == MC_WORLD_STORE_GAME_FAILED && view.state->x == 43 && view.state->z == 4 &&
          !view.state->has_entities && !view.state->active &&
          !mc_world_chunk_source_enabled(view.chunk) && remember(f, f->stores[1], view.state));
    for (size_t cell = 0; cell < MC_CHUNK_BLOCKS; ++cell)
        CHECK(view.state->data->blocks[cell] == input_byte(0, cell));
    CHECK(mc_world_store_lease_chunk(f->stores[1], view.state, NULL) == MC_WORLD_STORAGE_OK);
    leased = true;
    CHECK(mc_world_runtime_retire_entity_containers(f->worlds[1], NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_store_release_chunk(f->stores[1], view.state, NULL) == MC_WORLD_STORAGE_OK);
    leased = false;
    CHECK(mc_world_regions_read(f->regions, mc_world_store_root(f->stores[1]), 42, 4, &after,
                                NULL) == MC_WORLD_STORAGE_OK &&
          before.length == after.length && memcmp(before.data, after.data, before.length) == 0);
    ok = true;
cleanup:
    if (leased)
        (void)mc_world_store_release_chunk(f->stores[1], view.state, NULL);
    mc_compression_buffer_free(&before);
    mc_compression_buffer_free(&after);
    return ok;
}

static bool limits(fixture *f)
{
    bool ok = false, changed = false;
    mc_chunk_state *first = NULL, *out = f->children[0].state;
    CHECK(create_world(f, 2, 0, 0, 1));
    CHECK(acquire(f, 2, 20, 0, true, &first) == MC_CHUNK_ACQUIRE_OK);
    const mc_java_random before = f->provider[2].generator->generator_random;
    const size_t count = mc_world_store_chunk_count(f->stores[0]);
    const int32_t key = mc_chunk_cache_key(21, 0);
    CHECK(mc_chunk_pending_add(f->provider[2].pending, key, &changed) == MC_CHUNK_ACQUIRE_OK &&
          changed);
    CHECK(acquire(f, 2, 21, 0, true, &out) == MC_CHUNK_ACQUIRE_LIMIT &&
          out == f->children[0].state && mc_chunk_pending_contains(f->provider[2].pending, key) &&
          mc_world_store_chunk_count(f->stores[0]) == count &&
          mc_test_java_random_equal(&before, &f->provider[2].generator->generator_random));
    CHECK(mc_chunk_pending_remove(f->provider[2].pending, key, &changed) == MC_CHUNK_ACQUIRE_OK &&
          changed);
    CHECK(create_world(f, 3, 0, 2, 4));
    const int32_t failed = mc_chunk_cache_key(0, 0);
    CHECK(mc_chunk_pending_add(f->provider[3].pending, failed, &changed) == MC_CHUNK_ACQUIRE_OK &&
          changed);
    CHECK(acquire(f, 3, 0, 0, false, &out) == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
          out == f->children[0].state &&
          mc_world_runtime_last_error(f->worlds[3])->status == MC_WORLD_STORAGE_LIMIT &&
          !mc_chunk_pending_contains(f->provider[3].pending, failed) &&
          mc_world_store_chunk_count(f->stores[2]) == 0 && f->provider[3].cache->count == 0 &&
          f->provider[3].cache->insertion_count == 0);
    ok = true;
cleanup:
    return ok;
}

static bool drain_world(fixture *f, size_t world)
{
    if (!f->bound[world])
        return true;
    mc_chunk_cache *cache = f->provider[world].cache;
    mc_chunk_pending *pending = f->provider[world].pending;
    if (!mc_chunk_cache_validate(cache) || !mc_chunk_pending_validate(pending))
        return false;
    for (size_t i = 0; i < cache->count; ++i) {
        bool changed = false;
        if (mc_chunk_pending_add(pending, cache->entries[i].key, &changed) != MC_CHUNK_ACQUIRE_OK)
            return false;
    }
    while (pending->count != 0) {
        const size_t before = pending->count;
        bool result = true;
        if (mc_world_runtime_maintain(f->worlds[world], &result) != MC_CHUNK_LIFECYCLE_OK ||
            result || pending->count >= before)
            return false;
    }
    return cache->count == 0 && cache->insertion_count == 0;
}

static bool retirement(fixture *f)
{
    bool ok = false, leased = false;
    mc_chunk_state *const loaded = f->provider[1].cache->insertions[0];
    CHECK(mc_world_store_lease_chunk(f->stores[1], loaded, NULL) == MC_WORLD_STORAGE_OK);
    leased = true;
    CHECK(drain_world(f, 1));
    CHECK(!loaded->active &&
          mc_world_store_retire_chunk(f->stores[1], loaded, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_store_release_chunk(f->stores[1], loaded, NULL) == MC_WORLD_STORAGE_OK);
    leased = false;
    CHECK(mc_world_runtime_retire_world(f->worlds[1], NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_dimension_world(f->dimensions[1]) == NULL &&
          mc_world_store_has_chunks_for_world(f->stores[1], f->worlds[1]) &&
          mc_world_runtime_free(f->worlds[1], NULL) == MC_WORLD_STORAGE_BUSY);
    mc_world_chunk *const object = mc_world_store_chunk_object(f->stores[1], loaded);
    mc_world_activation_error error;
    CHECK(object != NULL);
    CHECK(mc_world_runtime_register_chunk_tiles(object, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.bytecode == 18 && error.world_bytecode == 0 && !loaded->active);
    CHECK(mc_world_runtime_register_chunk_entities(object, 0, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.bytecode == 42 && error.world_bytecode == 0 && !loaded->active);
    CHECK(mc_world_runtime_defer_chunk_entities(object, 0, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.bytecode == 67 && error.world_bytecode == 0 && !loaded->active);
    /* Empty invalidation reaches no World method, so retirement is no new
     * failure for that distinct source body. The live store still owns it. */
    CHECK(mc_world_runtime_invalidate_chunk_tiles(object, &error) == MC_WORLD_STORAGE_OK &&
          !loaded->active);
    ok = true;
cleanup:
    if (leased)
        (void)mc_world_store_release_chunk(f->stores[1], loaded, NULL);
    return ok;
}

static bool cleanup_fixture(fixture *f)
{
    if (f == NULL)
        return true;
    for (size_t i = 0; i < COUNT(f->worlds); ++i)
        if (f->worlds[i] != NULL && !drain_world(f, i))
            return false;
    for (size_t i = 0; i < COUNT(f->worlds); ++i)
        if (f->worlds[i] != NULL &&
            mc_world_runtime_retire_entity_containers(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
    for (size_t i = 0; i < COUNT(f->children); ++i) {
        if (f->children[i].state != NULL &&
            mc_world_store_retire_chunk(f->children[i].store, f->children[i].state, NULL) !=
                MC_WORLD_STORAGE_OK)
            return false;
        f->children[i] = (child){0};
    }
    for (size_t i = 0; i < COUNT(f->worlds); ++i) {
        if (f->worlds[i] != NULL &&
            (mc_world_runtime_retire_world(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK ||
             mc_world_runtime_free(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK))
            return false;
        f->worlds[i] = NULL;
    }
    for (size_t i = 0; i < COUNT(f->dimensions); ++i)
        mc_world_dimension_release(f->dimensions[i]);
    for (size_t i = 0; i < COUNT(f->classes); ++i)
        if (mc_world_runtime_classes_free(f->classes[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
    for (size_t i = 0; i < COUNT(f->registry); ++i)
        if (mc_registry_classes_free(f->registry[i], NULL) != MC_REGISTRY_OK)
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

static bool decode_path(const char *hex, char *out, size_t capacity)
{
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

int main(int argc, char **argv)
{
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'A', 'c', 't'};
    if (argc != 3 || strcmp(argv[1], "world-provider-activation") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(open_store(f, 0, root, "/generated", 16) && open_store(f, 1, root, "/loaded", 16) &&
          open_store(f, 2, root, "/count", 0));
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t i = 0; i < COUNT(f->registry); ++i) {
        resources *r = &f->resources[i];
        r->streams[0] = (stream){.data = language, .length = sizeof(language) - 1};
        r->streams[1] = (stream){.data = language};
        r->streams[2] = (stream){.data = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, resource_ready};
        const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                             &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[i], NULL) == MC_REGISTRY_OK);
        r->registry = f->registry[i];
        CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                              &f->classes[i], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[i], f->registry[i], NULL) ==
              MC_REGISTRY_OK);
    }
    CHECK(create_world(f, 0, 0, 0, 12) && create_world(f, 1, 1, 1, 12));
    CHECK(generated_and_cached(f));
    CHECK(population_completion(f));
    CHECK(loaded_and_persisted(f));
    CHECK(opaque_refusal(f));
    CHECK(relocation_frontier(f));
    CHECK(limits(f));
    CHECK(retirement(f));
    CHECK(!f->bad && !f->resources[0].bad && !f->resources[1].bad);
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("provider activation cleanup refused; live dependencies retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u constructed GAME provider activation checks (world-provider-activation)\n",
               checks) < 0)
        result = 1;
    return result;
}
