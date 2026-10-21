#include "../../../../src/world/runtime/acquisition_internal.h"
#include "../../../../src/world/runtime/provider_internal.h"
#include "../../../../src/world/storage/objects_internal.h"
#include "../../../support/java_random.h"
#include "mc/block/registry/falling.h"
#include "mc/entity/entity.h"
#include "mc/item/registry/runtime.h"
#include "mc/resources/translation/translation.h"
#include "mc/world/biome/registry.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/population/normal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Native full-controller/cache-gate contracts. Saved all-air input is an
 * explicit prepared World, not natural terrain. No source phase, count,
 * cache/hold descriptor or RNG state is assigned. Successful source false
 * feature returns are exercised through the real controller. These checks
 * supply no original/native full-call or RNG-continuation oracle. */
enum { WORLD_COUNT = 4, ROOM_COUNT = 4 };

static unsigned int checks;
static mc_math_trig trig;
static const int32_t room_x[ROOM_COUNT] = {0, 1, 0, 1};
static const int32_t room_z[ROOM_COUNT] = {0, 0, 1, 1};
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct byte_stream {
    const uint8_t *bytes;
    size_t length, offset;
    bool closed;
} byte_stream;

typedef struct resources {
    byte_stream streams[3];
    mc_registry_resources api;
    size_t attempts, opens, closes;
    bool missing_language, bad;
} resources;

typedef struct world_slot {
    resources source;
    mc_registry_classes *registry;
    mc_entity_classes *entities;
    mc_world_runtime_classes *classes;
    mc_world_store *store;
    mc_world_dimension *dimension;
    mc_world_runtime *world;
    mc_world_provider *provider;
    mc_world_provider_fields view;
    mc_chunk_state *rooms[ROOM_COUNT];
    mc_temperature_array *temperature_alias;
    bool bound, terminal, retired;
} world_slot;

typedef struct fixture {
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_world_regions *regions;
    mc_world_info_name *name;
    world_slot slots[WORLD_COUNT];
} fixture;

typedef struct room_record {
    mc_nbt_tag root, level, fields[11];
    const mc_nbt_tag *root_fields[1], *level_fields[11];
    uint16_t names[12][20];
    uint8_t blocks[MC_CHUNK_BLOCKS];
    uint8_t metadata[MC_CHUNK_NIBBLE_BYTES];
    uint8_t sky[MC_CHUNK_NIBBLE_BYTES];
    uint8_t light[MC_CHUNK_NIBBLE_BYTES];
    uint8_t height[MC_CHUNK_HEIGHTMAP_BYTES];
} room_record;

static mc_java_properties_status read_bytes(void *owner, uint8_t *out, size_t capacity,
                                            size_t *count) {
    byte_stream *s = owner;
    if (s == NULL || out == NULL || capacity == 0 || count == NULL || s->closed ||
        s->offset > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > 17u)
        take = 17u;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->bytes + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_resource(void *owner, const char *name,
                                        mc_java_properties_input *out) {
    resources *r = owner;
    static const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang",
                                        "/achievement/map.txt"};
    ++r->attempts;
    if (name == NULL || out == NULL || r->opens >= COUNT(names) ||
        strcmp(name, names[r->opens]) != 0) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    /* An actual missing resource is the negative input; initialization still
     * executes its real preceding constructors and shared-profile effects. */
    if (r->missing_language)
        return MC_REGISTRY_NULL_REFERENCE;
    *out = (mc_java_properties_input){&r->streams[r->opens++], read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *owner, const mc_java_properties_input *input) {
    resources *r = owner;
    if (input == NULL || input->context != &r->streams[2] || input->read != read_bytes) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    r->streams[2].closed = true;
    ++r->closes;
    return MC_REGISTRY_OK;
}

static bool resource_ready(void *owner, const mc_java_properties_input *input) {
    resources *r = owner;
    if (input == NULL || input->read != read_bytes)
        r->bad = true;
    return false;
}

static mc_nbt_string ascii(const char *text, uint16_t *units) {
    const size_t length = strlen(text);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){units, length};
}

static bool decode_path(const char *hex, char *out, size_t capacity) {
    const size_t length = strlen(hex);
    if (length == 0 || length % 2u != 0 || length / 2u >= capacity)
        return false;
    for (size_t i = 0; i < length / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            const unsigned char c = (unsigned char)hex[2u * i + j];
            const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                       : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                       : c >= 'A' && c <= 'F' ? (unsigned int)(c - 'A') + 10u
                                                              : 16u;
            if (digit >= 16u)
                return false;
            value = value * 16u + digit;
        }
        if (value == 0)
            return false;
        out[i] = (char)(unsigned char)value;
    }
    out[length / 2u] = '\0';
    return true;
}

static bool registry_same(const mc_registry_error *a, const mc_registry_error *b) {
    return a->status == b->status && a->message == b->message &&
           a->initializer_pc == b->initializer_pc && a->block_id == b->block_id &&
           a->constructor_class == b->constructor_class && a->constructor_pc == b->constructor_pc &&
           a->profile_status == b->profile_status &&
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

static bool bytes_are(const uint8_t *bytes, size_t length, uint8_t value) {
    for (size_t i = 0; i < length; ++i)
        if (bytes[i] != value)
            return false;
    return true;
}

static bool refresh(world_slot *s) {
    return mc_world_provider_bind_fields(s->provider, &s->view, NULL) == MC_WORLD_STORAGE_OK;
}

static bool open_world(fixture *f, size_t index, const char *root) {
    bool ok = false;
    world_slot *s = &f->slots[index];
    const bool client = index % 2u == 0;
    const bool warm = index < 2u;
    char path[8192];
    uint8_t map[256];
    const uint16_t title[] = {'N', 'o', 'r', 'm', 'a', 'l'};
    const int length = snprintf(path, sizeof(path), "%s/world%zu", root, index);
    CHECK(length > 0 && (size_t)length < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 197,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = ROOM_COUNT;
    CHECK(mc_world_store_open(path, f->regions, &options, &s->store, NULL) == MC_WORLD_STORAGE_OK);
    const mc_level_info info = {
        .seed = 23, .spawn_y = 64, .level_name = {title, COUNT(title)}, .version = 19132};
    CHECK(mc_world_store_save_info(s->store, &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    s->source.streams[0] = (byte_stream){.bytes = language, .length = sizeof(language) - 1};
    s->source.streams[1] = s->source.streams[2] = (byte_stream){.bytes = language};
    s->source.missing_language = !warm;
    s->source.api =
        (mc_registry_resources){&s->source, open_resource, close_resource, resource_ready};
    const mc_registry_options registry_options = {client ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                                  f->profile, f->materials, 0, &s->source.api};
    CHECK(mc_registry_classes_allocate(&registry_options, &s->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(client ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile, NULL,
                                   &s->entities) == MC_ENTITY_OK);
    CHECK(mc_registry_classes_bind_entities(s->registry, s->entities, NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(client ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER,
                                          f->profile, &trig, &s->classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(s->classes, s->registry, NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_bind_entities(s->classes, s->entities) == MC_ENTITY_OK);
    if (warm) {
        CHECK(mc_registry_classes_initialize_blocks(s->registry, NULL) == MC_REGISTRY_OK &&
              mc_registry_classes_initialize_items(s->registry, NULL) == MC_REGISTRY_OK &&
              mc_registry_classes_initialize_biomes(s->registry, NULL) == MC_REGISTRY_OK);
        CHECK(s->source.attempts == 3 && s->source.opens == 3 && s->source.closes == 1 &&
              !s->source.bad);
    }
    CHECK(mc_world_dimension_create(0, NULL, &s->dimension, NULL) == MC_WORLD_DIMENSION_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = ROOM_COUNT;
    CHECK(mc_world_runtime_allocate_world(client ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                                 : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                          s->classes, &limits, &s->world,
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = s->store, .name = f->name, .seed = -99, .dimension = s->dimension};
    CHECK(mc_world_runtime_construct_world(s->world, &arguments, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_runtime_construction construction = {0};
    mc_world_runtime_fields fields = {0};
    CHECK(mc_world_runtime_construction_get(s->world, &construction) == MC_WORLD_STORAGE_OK &&
          construction.mode == MC_WORLD_RUNTIME_GAME &&
          construction.phase == MC_WORLD_RUNTIME_READY && construction.contexts_bound &&
          construction.cache_registered);
    CHECK(mc_world_runtime_fields_get(s->world, &fields) == MC_WORLD_STORAGE_OK &&
          fields.v != NULL && fields.r != NULL && fields.w == s->store &&
          fields.t == s->dimension && !fields.a && !fields.o);
    s->provider = fields.v;
    CHECK(refresh(s));
    s->bound = true;
    CHECK(s->view.world == s->world && s->view.store == s->store && s->view.manager != NULL &&
          s->view.generator != NULL && s->view.max_chunks == ROOM_COUNT &&
          mc_chunk_cache_validate(s->view.cache) && s->view.cache->count == 0 &&
          s->view.cache->insertion_count == 0 && mc_chunk_pending_validate(s->view.pending) &&
          s->view.pending->count == 0 && s->view.w == NULL &&
          s->view.temperature_allocations == NULL && mc_world_store_chunk_count(s->store) == 0);
    if (!warm) {
        mc_registry_block_view blocks = {0};
        CHECK(mc_registry_classes_inspect_blocks(s->registry, &blocks, NULL) == MC_REGISTRY_OK &&
              blocks.phase == MC_REGISTRY_COLD && blocks.m == NULL && s->source.attempts == 0);
    }
    ok = true;
cleanup:
    return ok;
}

/* Prepare complete ordinary NBT records BEFORE genuine GAME load. Sky 15 and
 * height zero are explicit serialized inputs, with all blocks/metadata/light
 * zero. Every TerrainPopulated byte is zero; no neighbor skips its callback by
 * being premarked populated. Empty source lists are real decoded Lists. */
static bool save_air_room(fixture *f, world_slot *s, size_t index) {
    bool ok = false;
    room_record *r = calloc(1, sizeof(*r));
    uint8_t *encoded = NULL;
    const char *const names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                 MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                 MC_NBT_LIST,       MC_NBT_LIST};
    CHECK(r != NULL);
    memset(r->sky, 0xff, sizeof(r->sky));
    r->root.type = r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    const uint8_t *const arrays[] = {r->blocks, r->metadata, r->sky, r->light, r->height};
    const size_t lengths[] = {sizeof(r->blocks), sizeof(r->metadata), sizeof(r->sky),
                              sizeof(r->light), sizeof(r->height)};
    for (size_t i = 0; i < COUNT(arrays); ++i) {
        r->fields[3u + i].value.byte_array.data = arrays[i];
        r->fields[3u + i].value.byte_array.length = lengths[i];
    }
    r->fields[0].value.int_value = room_x[index];
    r->fields[1].value.int_value = room_z[index];
    r->fields[9].value.list.element_type = r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(r->level_fields);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = COUNT(r->root_fields);
    size_t length = 0, written = 0;
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK);
    encoded = malloc(length);
    CHECK(encoded != NULL &&
          mc_nbt_write(&r->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = UINT32_MAX;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(s->store), room_x[index],
                                 room_z[index], encoded, length, UINT32_C(0xfedcba98), &growth,
                                 NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

/* Remember actual cache-owned identities even when a call fails after source
 * publication. This is cleanup bookkeeping, never publication or a cache edit. */
static bool remember_rooms(world_slot *s) {
    if (!s->bound)
        return true;
    if (!mc_chunk_cache_validate(s->view.cache))
        return false;
    for (size_t i = 0; i < s->view.cache->count; ++i) {
        mc_chunk_state *const state = s->view.cache->entries[i].chunk;
        bool found = false;
        for (size_t j = 0; j < ROOM_COUNT; ++j) {
            if (state->x == room_x[j] && state->z == room_z[j]) {
                if (s->rooms[j] != NULL && s->rooms[j] != state)
                    return false;
                s->rooms[j] = state;
                found = true;
                break;
            }
        }
        if (!found || !mc_world_store_owns_chunk(s->store, state))
            return false;
    }
    return true;
}

static bool air_image(world_slot *s, size_t index, bool populated) {
    bool ok = false;
    mc_chunk_state *state = s->rooms[index];
    mc_world_chunk_fields fields = {0};
    mc_world_chunk_nibble_fields metadata = {0}, sky = {0}, light = {0};
    const mc_world_chunk_entity_list *slices[8] = {NULL};
    size_t size = SIZE_MAX;
    CHECK(state != NULL && state->data != NULL && state->data->blocks != NULL &&
          state->x == room_x[index] && state->z == room_z[index] && !state->blank &&
          state->active && state->terrain_populated == populated && state->dirty == populated &&
          !state->never_save && !state->has_entities && state->last_save_time == 0);
    mc_world_chunk *const object = mc_world_store_chunk_object(s->store, state);
    CHECK(object != NULL && mc_world_chunk_state(object) == state &&
          mc_world_chunk_fields_get(object, &fields) == MC_WORLD_STORAGE_OK);
    CHECK(fields.d == s->world && fields.b == state->data->blocks &&
          fields.h == state->data->heightmap && fields.j == room_x[index] &&
          fields.k == room_z[index] && fields.c && fields.n == populated && fields.o == populated &&
          !fields.p && !fields.q && fields.r == 0 && fields.i == 0);
    CHECK(fields.e != NULL && fields.f != NULL && fields.g != NULL && fields.e != fields.f &&
          fields.e != fields.g && fields.f != fields.g && fields.l != NULL && fields.m != NULL);
    CHECK(mc_world_chunk_nibble_fields_get(fields.e, &metadata) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_nibble_fields_get(fields.f, &sky) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_nibble_fields_get(fields.g, &light) == MC_WORLD_STORAGE_OK &&
          metadata.a == state->data->metadata && sky.a == state->data->skylight &&
          light.a == state->data->block_light && metadata.length == MC_CHUNK_NIBBLE_BYTES &&
          sky.length == MC_CHUNK_NIBBLE_BYTES && light.length == MC_CHUNK_NIBBLE_BYTES);
    CHECK(state->data->blocks_capacity >= MC_CHUNK_BLOCKS &&
          bytes_are(fields.b, MC_CHUNK_BLOCKS, 0) && bytes_are(metadata.a, metadata.length, 0) &&
          bytes_are(sky.a, sky.length, 0xff) && bytes_are(light.a, light.length, 0) &&
          bytes_are(fields.h, MC_CHUNK_HEIGHTMAP_BYTES, 0));
    CHECK(mc_world_chunk_tile_count(fields.l, &size) == MC_WORLD_STORAGE_OK && size == 0);
    for (size_t i = 0; i < COUNT(slices); ++i) {
        CHECK(mc_world_chunk_slice_at(fields.m, i, &slices[i]) == MC_WORLD_STORAGE_OK &&
              slices[i] != NULL);
        CHECK(mc_world_chunk_entity_count(slices[i], &size) == MC_WORLD_STORAGE_OK && size == 0);
        for (size_t j = 0; j < i; ++j)
            CHECK(slices[i] != slices[j]);
    }
    ok = true;
cleanup:
    return ok;
}

static bool success_gate(fixture *f, size_t index) {
    bool ok = false;
    world_slot *s = &f->slots[index];
    mc_world_runtime_fields fields = {0};
    mc_temperature_array_view array = {0}, after = {0};
    mc_temperature_owner_state ledger = {0}, ledger_after = {0};
    double temperatures[256];
    mc_java_random world_random = {0}, generator = {0}, cave = {0};
    CHECK(mc_world_runtime_fields_get(s->world, &fields) == MC_WORLD_STORAGE_OK);
    world_random = *fields.r;
    for (size_t i = 0; i < ROOM_COUNT; ++i)
        CHECK(save_air_room(f, s, i));
    for (size_t i = 0; i < ROOM_COUNT; ++i) {
        mc_chunk_state *result = NULL;
        const mc_chunk_acquire_status status =
            mc_world_runtime_acquire(s->world, room_x[i], room_z[i], &result);
        CHECK(remember_rooms(s));
        if (status != MC_CHUNK_ACQUIRE_OK) {
            const mc_world_storage_error *cause = mc_world_runtime_last_error(s->world);
            fprintf(stderr, "normal population side%zu acquisition%zu: status=%d cause=%s\n", index,
                    i, (int)status,
                    cause != NULL && cause->operation != NULL ? cause->operation : "none");
        }
        CHECK(status == MC_CHUNK_ACQUIRE_OK && result == s->rooms[i] && result != NULL);
        CHECK(refresh(s) && s->view.cache->count == i + 1u &&
              s->view.cache->insertion_count == i + 1u && s->view.pending->count == 0 &&
              mc_world_store_chunk_count(s->store) == i + 1u &&
              mc_world_store_reservation_count(s->store) == 0);
        for (size_t j = 0; j <= i; ++j) {
            CHECK(s->view.cache->insertions[j] == s->rooms[j]);
            CHECK(air_image(s, j, i == ROOM_COUNT - 1u && j == 0));
        }
        if (i < ROOM_COUNT - 1u)
            CHECK(s->view.w == NULL && s->view.temperature_allocations == NULL);
    }
    /* Fourth actual publication reaches precisely the northwest candidate.
     * The source all-air guards allow ordinary false. Completed population and
     * full images verify this prepared outcome, without observing individual
     * helper Booleans, hidden call counts or an original RNG continuation. */
    CHECK(s->view.w != NULL && s->view.temperature_allocations != NULL &&
          mc_temperature_array_get(s->view.w, &array) == MC_BIOME_OK && array.length == 256 &&
          array.holds == 1 && array.owner == s->view.temperature_allocations && array.data != NULL);
    CHECK(mc_temperature_owner_get(s->view.temperature_allocations, &ledger) == MC_BIOME_OK &&
          ledger.holds == 2 && ledger.arrays == 1 && ledger.direct_allocation_bytes > 0 &&
          ledger.direct_allocation_bytes <= ledger.limits.max_direct_allocation_bytes &&
          ledger.buffer_allocation_bytes >= sizeof(temperatures) &&
          ledger.buffer_allocation_bytes <= ledger.limits.max_buffer_allocation_bytes);
    for (size_t i = 0; i < COUNT(temperatures); ++i)
        CHECK(isfinite(array.data[i]) && array.data[i] >= 0.0 && array.data[i] <= 1.0);
    memcpy(temperatures, array.data, sizeof(temperatures));
    bool falling = true;
    mc_registry_error registry = {0};
    CHECK(mc_registry_falling_flag_get(s->registry, &falling, &registry) == MC_REGISTRY_OK &&
          !falling && registry.status == MC_REGISTRY_OK && registry.message == NULL &&
          registry.initializer_pc == -1 && registry.constructor_pc == -1 &&
          registry.active_initializer_pc == -1);
    CHECK(mc_world_runtime_fields_get(s->world, &fields) == MC_WORLD_STORAGE_OK && !fields.a &&
          !fields.o && fields.r != NULL && mc_test_java_random_equal(fields.r, &world_random));
    int32_t scheduled = -1;
    CHECK(mc_scheduled_ordered_size(fields.E, &scheduled) == MC_SCHEDULED_ORDERED_OK &&
          scheduled == 0);
    CHECK(mc_scheduled_membership_size(fields.F, &scheduled) == MC_SCHEDULED_MEMBERSHIP_OK &&
          scheduled == 0);
    generator = s->view.generator->generator_random;
    cave = s->view.generator->cave_random;
    mc_temperature_array *const current = s->view.w;
    const size_t attempts = s->source.attempts;
    for (size_t i = 0; i < ROOM_COUNT; ++i) {
        mc_chunk_state *result = NULL;
        mc_world_runtime_acquisition_error error = {0};
        CHECK(mc_world_runtime_get_detailed(s->world, room_x[i], room_z[i], &result, &error) ==
                  MC_CHUNK_ACQUIRE_OK &&
              result == s->rooms[i] && error.status == MC_CHUNK_ACQUIRE_OK &&
              !error.population.reached);
        CHECK(air_image(s, i, i == 0));
    }
    CHECK(refresh(s) && s->view.w == current && s->view.cache->count == ROOM_COUNT &&
          s->view.cache->insertion_count == ROOM_COUNT && s->view.pending->count == 0 &&
          s->source.attempts == attempts && !s->source.bad &&
          mc_test_java_random_equal(&s->view.generator->generator_random, &generator) &&
          mc_test_java_random_equal(&s->view.generator->cave_random, &cave));
    CHECK(mc_temperature_array_get(current, &after) == MC_BIOME_OK && after.data == array.data &&
          after.length == array.length && after.holds == array.holds &&
          after.owner == array.owner &&
          memcmp(after.data, temperatures, sizeof(temperatures)) == 0);
    CHECK(mc_temperature_owner_get(s->view.temperature_allocations, &ledger_after) == MC_BIOME_OK &&
          ledger_after.holds == ledger.holds && ledger_after.arrays == ledger.arrays &&
          ledger_after.direct_allocation_bytes == ledger.direct_allocation_bytes &&
          ledger_after.buffer_allocation_bytes == ledger.buffer_allocation_bytes);
    CHECK(mc_temperature_array_retain(current) == MC_BIOME_OK);
    s->temperature_alias = current;
    CHECK(mc_temperature_array_get(current, &after) == MC_BIOME_OK && after.holds == 2 &&
          mc_temperature_owner_get(s->view.temperature_allocations, &ledger_after) == MC_BIOME_OK &&
          ledger_after.holds == 2 && ledger_after.arrays == 1);
    ok = true;
cleanup:
    return ok;
}

static bool cold_entry_failure(fixture *f, size_t index) {
    bool ok = false;
    world_slot *s = &f->slots[index];
    mc_world_normal_population_error first = {0}, retry = {0};
    mc_world_runtime_fields fields = {0};
    mc_registry_block_view blocks = {0};
    CHECK(mc_world_runtime_fields_get(s->world, &fields) == MC_WORLD_STORAGE_OK &&
          fields.r != NULL);
    const mc_java_random world_random = *fields.r;
    const mc_java_random generator = s->view.generator->generator_random;
    const mc_java_random cave = s->view.generator->cave_random;
    CHECK(mc_world_provider_populate_game(s->provider, 0, 0, &first) ==
          MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    CHECK(first.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && first.operation != NULL &&
          first.child == MC_NORMAL_CHILD_REGISTRY && first.bytecode == 1 &&
          first.world_bytecode == 0 && !first.native_boundary && first.iteration == SIZE_MAX &&
          first.selector_class == NULL && first.selector_method == NULL &&
          first.selector_bytecode == 0 && !first.nested.reached);
    CHECK(first.provider_class != NULL &&
          strcmp(first.provider_class, index % 2u == 0 ? "yf" : "pk") == 0 &&
          first.method != NULL &&
          strcmp(first.method, index % 2u == 0 ? "a(Lcl;II)V" : "a(Lbl;II)V") == 0 &&
          first.cause.status == first.status && first.cause.operation == first.operation &&
          first.cause.system_error == 0 && first.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          first.registry.message != NULL && first.registry.initializer_pc >= 0);
    CHECK(s->source.attempts == 1 && s->source.opens == 0 && s->source.closes == 0 &&
          !s->source.bad);
    CHECK(mc_registry_classes_inspect_blocks(s->registry, &blocks, NULL) == MC_REGISTRY_OK &&
          blocks.phase == MC_REGISTRY_FAILED && blocks.m != NULL);
    const size_t allocation = blocks.allocation_bytes;
    const mc_world_population_context context = mc_world_normal_population_context_get(&first);
    CHECK(context.reached && context.current.bytecode == 1 && context.origin.bytecode == 1 &&
          context.current.iteration == SIZE_MAX && !context.current.native_boundary &&
          context.current.provider_class == first.provider_class &&
          context.current.method == first.method && context.cause.status == first.cause.status &&
          context.cause.operation == first.cause.operation &&
          context.cause.system_error == first.cause.system_error &&
          registry_same(&context.registry, &first.registry));
    CHECK(mc_world_provider_populate_game(s->provider, 0, 0, &retry) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          retry.status == first.status && retry.bytecode == 1 &&
          retry.child == MC_NORMAL_CHILD_REGISTRY && retry.iteration == SIZE_MAX &&
          !retry.native_boundary && registry_same(&retry.registry, &first.registry));
    bool output = true;
    mc_registry_error cached = {0};
    CHECK(mc_registry_falling_flag_get(s->registry, &output, &cached) ==
              MC_REGISTRY_FAILED_INITIALIZATION &&
          output && registry_same(&cached, &first.registry));
    CHECK(mc_registry_classes_inspect_blocks(s->registry, &blocks, NULL) == MC_REGISTRY_OK &&
          blocks.phase == MC_REGISTRY_FAILED && blocks.allocation_bytes == allocation &&
          s->source.attempts == 1 && s->source.opens == 0 && s->source.closes == 0 &&
          !s->source.bad);
    CHECK(refresh(s) && s->view.w == NULL && s->view.temperature_allocations == NULL &&
          s->view.cache->count == 0 && s->view.cache->insertion_count == 0 &&
          s->view.pending->count == 0 && mc_world_store_chunk_count(s->store) == 0 &&
          mc_world_store_reservation_count(s->store) == 0 &&
          mc_test_java_random_equal(&s->view.generator->generator_random, &generator) &&
          mc_test_java_random_equal(&s->view.generator->cave_random, &cave));
    CHECK(mc_world_runtime_fields_get(s->world, &fields) == MC_WORLD_STORAGE_OK && !fields.a &&
          !fields.o && mc_test_java_random_equal(fields.r, &world_random));
    /* No profile reset/asserted no-consumption: real Block constructors can
     * consume default Random before the missing language resource is reached. */
    ok = true;
cleanup:
    return ok;
}

static bool drain(world_slot *s) {
    if (!s->bound || s->retired)
        return true;
    if (!remember_rooms(s) || !mc_chunk_pending_validate(s->view.pending))
        return false;
    for (size_t i = 0; i < s->view.cache->count; ++i) {
        bool changed = false;
        if (mc_chunk_pending_add(s->view.pending, s->view.cache->entries[i].key, &changed) !=
            MC_CHUNK_ACQUIRE_OK)
            return false;
    }
    while (s->view.pending->count != 0) {
        const size_t before = s->view.pending->count;
        bool result = true;
        if (mc_world_runtime_maintain(s->world, &result) != MC_CHUNK_LIFECYCLE_OK || result ||
            s->view.pending->count >= before)
            return false;
    }
    return s->view.cache->count == 0 && s->view.cache->insertion_count == 0;
}

static bool retirement(world_slot *s) {
    bool ok = false, leased = false;
    mc_chunk_state *const target = s->rooms[0];
    mc_temperature_array_view before = {0}, after = {0};
    mc_temperature_owner_state ledger = {0};
    double values[256];
    CHECK(s->temperature_alias != NULL && target != NULL &&
          mc_temperature_array_get(s->temperature_alias, &before) == MC_BIOME_OK &&
          before.length == COUNT(values));
    memcpy(values, before.data, sizeof(values));
    CHECK(mc_world_store_lease_chunk(s->store, target, NULL) == MC_WORLD_STORAGE_OK);
    leased = true;
    /* Actual unload saves the dirty target through GAME transport. No source
     * save result is used as the population oracle, and no descriptor is reset. */
    /* The unload main-save path retains dirty; the separate save-pass wrapper
     * clears it. Check the reached unload body without inventing that wrapper. */
    CHECK(drain(s) && !target->active && target->dirty);
    CHECK(mc_world_store_retire_chunk(s->store, target, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_store_release_chunk(s->store, target, NULL) == MC_WORLD_STORAGE_OK);
    leased = false;
    CHECK(mc_world_runtime_retire_entity_containers(s->world, NULL) == MC_WORLD_STORAGE_OK);
    s->terminal = true;
    CHECK(mc_world_runtime_retire_world(s->world, NULL) == MC_WORLD_STORAGE_OK);
    s->retired = true;
    CHECK(mc_world_runtime_free(s->world, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_store_has_chunks_for_world(s->store, s->world));
    for (size_t i = 0; i < ROOM_COUNT; ++i) {
        CHECK(mc_world_store_retire_chunk(s->store, s->rooms[i], NULL) == MC_WORLD_STORAGE_OK);
        s->rooms[i] = NULL;
    }
    CHECK(mc_world_store_chunk_count(s->store) == 0 &&
          mc_world_runtime_free(s->world, NULL) == MC_WORLD_STORAGE_OK);
    s->world = NULL;
    s->provider = NULL;
    s->bound = false;
    s->view = (mc_world_provider_fields){0};
    CHECK(mc_temperature_array_get(s->temperature_alias, &after) == MC_BIOME_OK &&
          after.data == before.data && after.owner == before.owner &&
          after.length == before.length && after.holds == 1 &&
          memcmp(after.data, values, sizeof(values)) == 0);
    CHECK(mc_temperature_owner_get(after.owner, &ledger) == MC_BIOME_OK && ledger.holds == 1 &&
          ledger.arrays == 1 && ledger.buffer_allocation_bytes >= sizeof(values));
    mc_temperature_array_release(s->temperature_alias);
    s->temperature_alias = NULL;
    ok = true;
cleanup:
    if (leased)
        (void)mc_world_store_release_chunk(s->store, target, NULL);
    return ok;
}

/* Checked teardown never forges collection counts or drops dependencies after
 * a refused owner retirement. A failed run leaves its real files/evidence for
 * the controlling harness; no cleanup here deletes files. */
static bool cleanup_fixture(fixture *f) {
    if (f == NULL)
        return true;
    for (size_t i = 0; i < WORLD_COUNT; ++i) {
        world_slot *s = &f->slots[i];
        if (s->world != NULL) {
            if (!drain(s))
                return false;
            if (!s->terminal &&
                mc_world_runtime_retire_entity_containers(s->world, NULL) != MC_WORLD_STORAGE_OK)
                return false;
            s->terminal = true;
            if (!s->retired && mc_world_runtime_retire_world(s->world, NULL) != MC_WORLD_STORAGE_OK)
                return false;
            s->retired = true;
            for (size_t j = 0; j < ROOM_COUNT; ++j) {
                if (s->rooms[j] != NULL &&
                    mc_world_store_retire_chunk(s->store, s->rooms[j], NULL) != MC_WORLD_STORAGE_OK)
                    return false;
                s->rooms[j] = NULL;
            }
            if (mc_world_runtime_free(s->world, NULL) != MC_WORLD_STORAGE_OK)
                return false;
            s->world = NULL;
        }
        mc_temperature_array_release(s->temperature_alias);
        s->temperature_alias = NULL;
        mc_world_dimension_release(s->dimension);
        s->dimension = NULL;
        if (mc_world_runtime_classes_free(s->classes, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        s->classes = NULL;
        if (mc_registry_classes_free(s->registry, NULL) != MC_REGISTRY_OK)
            return false;
        s->registry = NULL;
        if (mc_entity_classes_free(s->entities) != MC_ENTITY_OK)
            return false;
        s->entities = NULL;
        if (mc_world_store_free_checked(&s->store, NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
    mc_world_info_name_release(f->name);
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "normal-population") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    const uint16_t title[] = {'N', 'o', 'r', 'm', 'a', 'l'};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    for (size_t i = 0; i < WORLD_COUNT; ++i)
        CHECK(open_world(f, i, root));
    CHECK(success_gate(f, 0) && success_gate(f, 1));
    CHECK(cold_entry_failure(f, 2) && cold_entry_failure(f, 3));
    CHECK(retirement(&f->slots[0]) && retirement(&f->slots[1]));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fprintf(stderr, "normal population checked owner cleanup refused\n");
        result = 1;
    }
    if (result == 0)
        printf("PASS: %u native controller/cache-gate checks (normal-population)\n", checks);
    return result;
}
