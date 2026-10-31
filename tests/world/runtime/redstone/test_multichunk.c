#include "mc/block/registry/runtime.h"
#include "mc/entity/classes/classes.h"
#include "mc/server/runtime/construction.h"
#include "mc/storage/region/region.h"
#include "mc/world/runtime/blocks.h"

#include "../../../../src/block/registry/registry_internal.h"
#include "../../../../src/world/chunk/object_state_internal.h"
#include "../../../../src/world/runtime/chunk_provider_internal.h"
#include "../../../../src/world/runtime/construction_internal.h"
#include "../../../../src/world/runtime/mutation/redstone/redstone_internal.h"
#include "../../../../src/world/runtime/runtime_internal.h"
#include "../../../../src/world/storage/objects_internal.h"
#include "../../../support/java_random.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define WORLDS 4u
#define CHUNKS 3u
#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "FAIL redstone.multichunk:%d: %s\n", __LINE__, #expression);           \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Authored native inputs, not original observations or natural generation.
 * Four ordinary GAME owners use real Regions, Stores and populated NBT Chunks
 * at (-1,0), (0,0), (1,0). World0 is client; World1/3 are base server;
 * World2 is the actual dedicated subclass with a constructed Server owner.
 * All three server Worlds share one Registry, EntityClasses and WorldClasses.
 * Public notifying setters perform every lifecycle operation. One additional
 * actual strength-body call exposes its generated Set before the drain, so a
 * real snapshot can span another World's clear/refill. That is sequential
 * shared ownership, not callback-driven cross-World reentry. No World, Block,
 * generation, observer, power or queue service is substituted. */
static unsigned int checks, cases;
static mc_math_trig trig;
static const uint32_t timestamp = UINT32_C(0xfedcba98);
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct resource_stream {
    const uint8_t *bytes;
    size_t length, offset;
    bool closed;
} resource_stream;

typedef struct resource_owner {
    resource_stream streams[3];
    mc_registry_resources api;
    size_t opens, closes;
    bool bad;
} resource_owner;

typedef struct fixture {
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_world_regions *regions;
    mc_world_info_name *name;
    resource_owner resources[2];
    mc_registry_classes *registry[2];
    mc_entity_classes *entities[2];
    mc_world_runtime_classes *classes[2];
    mc_world_provider *dimensions[WORLDS];
    mc_world_store *stores[WORLDS];
    mc_world_runtime *worlds[WORLDS];
    mc_chunk_provider_fields providers[WORLDS];
    mc_chunk_state *states[WORLDS][CHUNKS];
    mc_world_chunk *chunks[WORLDS][CHUNKS];
    mc_server_runtime *server;
    mc_world_mutation_error error;
    bool bound[WORLDS];
} fixture;

typedef struct chunk_record {
    mc_nbt_tag root, level, fields[11];
    const mc_nbt_tag *root_fields[1], *level_fields[11];
    uint16_t names[12][20];
    uint8_t arrays[5][MC_CHUNK_BLOCKS];
} chunk_record;

typedef struct chunk_image {
    mc_chunk_data data;
    uint8_t blocks[MC_CHUNK_BLOCKS];
    bool active, dirty, terrain_populated;
    int64_t last_save_time;
} chunk_image;

static size_t side_for(size_t slot) {
    return slot == 0 ? 0u : 1u;
}

static mc_java_properties_status resource_read(void *context, uint8_t *out, size_t capacity,
                                               size_t *count) {
    resource_stream *s = context;
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

static mc_registry_status resource_open(void *context, const char *name,
                                        mc_java_properties_input *out) {
    resource_owner *r = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    if (name == NULL || out == NULL || r->opens >= COUNT(names) ||
        strcmp(name, names[r->opens]) != 0) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    *out = (mc_java_properties_input){&r->streams[r->opens++], resource_read};
    return MC_REGISTRY_OK;
}

static mc_registry_status resource_close(void *context, const mc_java_properties_input *input) {
    resource_owner *r = context;
    if (input == NULL || input->context != &r->streams[2] || input->read != resource_read) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    r->streams[2].closed = true;
    ++r->closes;
    return MC_REGISTRY_OK;
}

static bool resource_ready(void *context, const mc_java_properties_input *input) {
    resource_owner *r = context;
    if (input == NULL || input->read != resource_read)
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
                                                              : 16u;
            if (digit > 15u)
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

static void authored_cell(chunk_record *r, int32_t chunk_x, int32_t x, int32_t y, uint8_t id) {
    if (mc_java_i32_shr(x, 4) != chunk_x)
        return;
    const size_t index = ((size_t)((uint32_t)x & 15u) << 11u) | (8u << 7u) | (size_t)y;
    r->arrays[0][index] = id;
    r->arrays[4][8u * 16u + ((uint32_t)x & 15u)] = 64;
}

static bool seed_chunk(fixture *f, size_t slot, int32_t x) {
    bool ok = false;
    chunk_record *r = calloc(1, sizeof(*r));
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
    CHECK(r != NULL);
    r->root.type = r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    r->fields[0].value.int_value = x;
    r->fields[8].value.byte_value = 1;
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3u].value.byte_array.data = r->arrays[i];
        r->fields[i + 3u].value.byte_array.length = lengths[i];
    }
    r->fields[9].value.list.element_type = r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(r->level_fields);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = 1;
    for (int32_t cell = -5; cell <= 7; ++cell) {
        authored_cell(r, x, cell, 63, 1);
        authored_cell(r, x, cell, 64, cell == -5 ? 70 : 55);
    }
    for (int32_t cell = 13; cell <= 30; ++cell) {
        authored_cell(r, x, cell, 63, 1);
        authored_cell(r, x, cell, 64, cell == 13 ? 70 : 55);
    }
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK && length != 0);
    encoded = malloc(length);
    CHECK(encoded != NULL &&
          mc_nbt_write(&r->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = 0;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[slot]), x, 0, encoded,
                                 length, timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool create_world(fixture *f, size_t slot, const char *root) {
    bool ok = false;
    char path[8192];
    uint8_t block_map[256];
    const uint16_t title[] = {'W', 'i', 'r', 'e'};
    const size_t side = side_for(slot);
    const int length = snprintf(path, sizeof(path), "%s/world%zu", root, slot);
    CHECK(length > 0 && (size_t)length < sizeof(path));
    for (size_t i = 0; i < COUNT(block_map); ++i)
        block_map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 137,
                                      .block_map = block_map,
                                      .block_map_length = sizeof(block_map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = CHUNKS;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_level_info info = {
        .seed = 23, .spawn_y = 64, .level_name = {title, COUNT(title)}, .version = 19132};
    CHECK(mc_world_store_save_info(f->stores[slot], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    for (int32_t x = -1; x <= 1; ++x)
        CHECK(seed_chunk(f, slot, x));
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = CHUNKS;
    limits.max_scheduled_entries = 8;
    if (slot != 2)
        CHECK(mc_world_provider_create(0, NULL, &f->dimensions[slot], NULL) ==
              MC_WORLD_PROVIDER_OK);
    const mc_world_runtime_entry constructor = slot == 0   ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                               : slot == 2 ? MC_WORLD_RUNTIME_SERVER_DEDICATED
                                                           : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE;
    CHECK(mc_world_runtime_allocate_world(constructor, f->classes[side], &limits, &f->worlds[slot],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {.store = f->stores[slot],
                                                        .name = f->name,
                                                        .seed = 23,
                                                        .dimension = f->dimensions[slot],
                                                        .server = slot == 2 ? f->server : NULL,
                                                        .dimension_id = 0};
    CHECK(mc_world_runtime_construct_world(f->worlds[slot], &arguments, NULL) ==
          MC_WORLD_STORAGE_OK);
    mc_world_runtime_construction state = {0};
    CHECK(mc_world_runtime_construction_get(f->worlds[slot], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY && state.contexts_bound && state.cache_registered);
    CHECK(mc_chunk_provider_bind_fields(f->worlds[slot]->fields.v, &f->providers[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[slot] = true;
    CHECK(f->providers[slot].world == f->worlds[slot] && f->providers[slot].cache->count == 0 &&
          f->providers[slot].cache->insertion_count == 0 && !f->worlds[slot]->fields.o &&
          !f->worlds[slot]->fields.B && !f->worlds[slot]->fields.y &&
          !*f->providers[slot].force_load);
    if (slot == 2) {
        mc_world_runtime_dedicated_fields fields;
        mc_world_runtime_dedicated_construction derived;
        mc_server_construction server;
        CHECK(mc_world_runtime_dedicated_get(f->worlds[slot], &fields, &derived) ==
                  MC_WORLD_STORAGE_OK &&
              fields.C == f->worlds[slot]->fields.v && fields.F == f->server && fields.G != NULL &&
              derived.last_store_pc == 31 && derived.failed_call_pc == 0 &&
              f->providers[slot].policy == MC_CHUNK_ACQUIRE_DEDICATED &&
              mc_server_runtime_inspect(f->server, NULL, &server) == MC_SERVER_OK &&
              server.phase == MC_SERVER_READY && server.references == 2);
    }
    ok = true;
cleanup:
    return ok;
}

/* Capture only genuine provider publications. This also runs before cleanup,
 * so an unsuccessful attempt cannot strand a real published Chunk owner. */
static bool capture_chunks(fixture *f, size_t slot) {
    mc_chunk_cache *cache = f->providers[slot].cache;
    if (!mc_chunk_cache_validate(cache))
        return false;
    for (size_t i = 0; i < cache->insertion_count; ++i) {
        mc_chunk_state *state = cache->insertions[i];
        if (state == NULL || state->blank || state->x < -1 || state->x > 1 || state->z != 0)
            return false;
        const size_t index = (size_t)(state->x + 1);
        if (f->states[slot][index] != NULL && f->states[slot][index] != state)
            return false;
        f->states[slot][index] = state;
        f->chunks[slot][index] = mc_world_store_chunk_object(f->stores[slot], state);
        mc_world_chunk_fields fields;
        if (!state->active || !state->terrain_populated || f->chunks[slot][index] == NULL ||
            mc_world_chunk_fields_get(f->chunks[slot][index], &fields) != MC_WORLD_STORAGE_OK ||
            fields.d != f->worlds[slot] || !fields.c || fields.q || fields.m == NULL)
            return false;
    }
    return cache->count == cache->insertion_count &&
           mc_world_store_chunk_count(f->stores[slot]) == cache->count;
}

/* y is an explicit native source-state input. The actual Server constructor
 * does not run startup or establish this test flag. Restore before assertions. */
static mc_chunk_acquire_status allowed_get(fixture *f, size_t slot, int32_t x,
                                           mc_chunk_state **out) {
    mc_world_runtime *world = f->worlds[slot];
    const bool original = world->fields.y;
    if (slot == 2)
        world->fields.y = true;
    const mc_chunk_acquire_status status = mc_world_runtime_get(world, x, 0, out);
    world->fields.y = original;
    return status;
}

static mc_world_storage_status source(fixture *f, size_t slot, int32_t x, int32_t metadata) {
    mc_world_runtime *world = f->worlds[slot];
    const bool original = world->fields.y;
    if (slot == 2)
        world->fields.y = true;
    const mc_world_storage_status status =
        mc_world_runtime_metadata_set(world, x, 64, 8, metadata, &f->error);
    world->fields.y = original;
    return status;
}

static bool value(fixture *f, size_t slot, int32_t x, int32_t id, int32_t metadata) {
    int32_t actual_id = -1, actual_metadata = -1;
    return mc_world_runtime_block_get(f->worlds[slot], x, 64, 8, &actual_id, NULL) ==
               MC_WORLD_STORAGE_OK &&
           actual_id == id &&
           mc_world_runtime_metadata_get(f->worlds[slot], x, 64, 8, &actual_metadata, NULL) ==
               MC_WORLD_STORAGE_OK &&
           actual_metadata == metadata;
}

static bool line(fixture *f, size_t slot, int32_t first, int32_t last, bool powered) {
    for (int32_t x = first; x <= last; ++x) {
        const int32_t strength = powered && x - first < 15 ? 15 - (x - first) : 0;
        if (!value(f, slot, x, 55, strength))
            return false;
    }
    return true;
}

static bool balanced(fixture *f, size_t slot, size_t snapshots) {
    mc_world_runtime *world = f->worlds[slot];
    mc_registry_classes *registry = f->registry[side_for(slot)];
    mc_registry_redstone_set_view view;
    return world->active_calls == 0 && world->local_leases == 0 && !world->fields.o &&
           !world->fields.B && !world->fields.y && !*f->providers[slot].force_load &&
           world->fields.b->count == 0 && registry->blocks.m[55]->subtype.sm.a &&
           registry->redstone_snapshot_count == snapshots &&
           mc_registry_redstone_set_inspect(registry->blocks.m[55]->subtype.sm.b, &view) ==
               MC_REGISTRY_OK &&
           view.size == 0;
}

static bool same_image(const chunk_image *image, const mc_chunk_state *state) {
    return image->data.blocks == state->data->blocks &&
           image->data.blocks_capacity == state->data->blocks_capacity &&
           memcmp(image->blocks, state->data->blocks, sizeof(image->blocks)) == 0 &&
           memcmp(image->data.metadata, state->data->metadata, MC_CHUNK_NIBBLE_BYTES) == 0 &&
           memcmp(image->data.skylight, state->data->skylight, MC_CHUNK_NIBBLE_BYTES) == 0 &&
           memcmp(image->data.block_light, state->data->block_light, MC_CHUNK_NIBBLE_BYTES) == 0 &&
           memcmp(image->data.heightmap, state->data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES) == 0 &&
           image->data.minimum_height == state->data->minimum_height &&
           image->active == state->active && image->dirty == state->dirty &&
           image->terrain_populated == state->terrain_populated &&
           image->last_save_time == state->last_save_time;
}

static bool repeated_source(fixture *f, size_t slot) {
    bool ok = false;
    chunk_image *images = calloc(CHUNKS, sizeof(*images));
    CHECK(images != NULL);
    for (size_t i = 0; i < CHUNKS; ++i) {
        const mc_chunk_state *state = f->states[slot][i];
        images[i].data = *state->data;
        memcpy(images[i].blocks, state->data->blocks, MC_CHUNK_BLOCKS);
        images[i].active = state->active;
        images[i].dirty = state->dirty;
        images[i].terrain_populated = state->terrain_populated;
        images[i].last_save_time = state->last_save_time;
    }
    mc_registry_redstone_set *set = f->registry[side_for(slot)]->blocks.m[55]->subtype.sm.b;
    mc_registry_redstone_set_view before, after;
    CHECK(mc_registry_redstone_set_inspect(set, &before) == MC_REGISTRY_OK);
    CHECK(source(f, slot, 13, 1) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < CHUNKS; ++i)
        CHECK(same_image(&images[i], f->states[slot][i]));
    CHECK(mc_registry_redstone_set_inspect(set, &after) == MC_REGISTRY_OK &&
          after.constructed_keys == before.constructed_keys && after.table == before.table &&
          after.capacity == before.capacity && balanced(f, slot, 0));
    ok = true;
cleanup:
    free(images);
    return ok;
}

static bool cross_chunks(fixture *f, size_t slot) {
    bool ok = false, changed = false;
    mc_world_runtime *world = f->worlds[slot];
    const mc_java_random before = *world->fields.r;
    mc_chunk_cache *cache = f->providers[slot].cache;
    mc_chunk_state *loaded = NULL, *hit = NULL;
    ++cases;
    if (slot == 2) {
        CHECK(mc_world_runtime_get(world, -1, 0, &loaded) == MC_CHUNK_ACQUIRE_OK &&
              loaded == f->providers[slot].fallback && loaded->blank && cache->count == 0 &&
              cache->insertion_count == 0 && mc_world_store_chunk_count(f->stores[slot]) == 0);
    }
    CHECK(allowed_get(f, slot, -1, &loaded) == MC_CHUNK_ACQUIRE_OK && loaded != NULL &&
          !loaded->blank && capture_chunks(f, slot) && cache->count == 1 &&
          loaded == f->states[slot][0] && !loaded->dirty);
    CHECK(mc_world_runtime_get(world, -1, 0, &hit) == MC_CHUNK_ACQUIRE_OK && hit == loaded &&
          cache->count == 1 && cache->insertion_count == 1);

    ++cases;
    /* Public metadata mutation still writes the source while either source
     * gate suppresses neighbor delivery. Both temporary inputs are restored. */
    for (unsigned int gate = 0; gate < 2; ++gate) {
        bool *flag = gate == 0 ? &world->fields.o : &world->fields.B;
        const bool original = *flag;
        *flag = true;
        const mc_world_storage_status status = source(f, slot, -5, 1);
        *flag = original;
        CHECK(status == MC_WORLD_STORAGE_OK && value(f, slot, -5, 70, 1) &&
              value(f, slot, -4, 55, 0) && cache->count == 1 && balanced(f, slot, 0));
        CHECK(source(f, slot, -5, 0) == MC_WORLD_STORAGE_OK && value(f, slot, -4, 55, 0));
    }

    ++cases;
    CHECK(source(f, slot, -5, 1) == MC_WORLD_STORAGE_OK && capture_chunks(f, slot) &&
          cache->count == 2 && cache->insertion_count == 2 && line(f, slot, -4, 7, true) &&
          balanced(f, slot, 0));
    CHECK(mc_world_runtime_get(world, 0, 0, &hit) == MC_CHUNK_ACQUIRE_OK &&
          hit == f->states[slot][1] && cache->insertion_count == 2);
    CHECK(source(f, slot, -5, 0) == MC_WORLD_STORAGE_OK && line(f, slot, -4, 7, false));

    ++cases;
    CHECK(source(f, slot, 13, 1) == MC_WORLD_STORAGE_OK && capture_chunks(f, slot) &&
          cache->count == CHUNKS && cache->insertion_count == CHUNKS &&
          line(f, slot, 14, 30, true) && balanced(f, slot, 0));
    CHECK(repeated_source(f, slot));
    CHECK(source(f, slot, 13, 0) == MC_WORLD_STORAGE_OK && line(f, slot, 14, 30, false));

    ++cases;
    CHECK(source(f, slot, -5, 1) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_block_set(world, 0, 64, 8, 0, &changed, &f->error) ==
              MC_WORLD_STORAGE_OK &&
          changed && value(f, slot, 0, 0, 0) && line(f, slot, -4, -1, true));
    for (int32_t x = 1; x <= 7; ++x)
        CHECK(value(f, slot, x, 55, 0));
    CHECK(mc_world_runtime_block_metadata_set(world, 0, 64, 8, 55, 0, &changed, &f->error) ==
              MC_WORLD_STORAGE_OK &&
          changed && line(f, slot, -4, 7, true));
    CHECK(mc_world_runtime_block_set(world, -5, 64, 8, 0, &changed, &f->error) ==
              MC_WORLD_STORAGE_OK &&
          changed && value(f, slot, -5, 0, 0) && line(f, slot, -4, 7, false));
    CHECK(mc_world_runtime_block_metadata_set(world, -5, 64, 8, 70, 0, &changed, &f->error) ==
              MC_WORLD_STORAGE_OK &&
          changed && value(f, slot, -5, 70, 0));

    ++cases;
    CHECK(source(f, slot, 13, 1) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_block_set(world, 16, 64, 8, 0, &changed, &f->error) ==
              MC_WORLD_STORAGE_OK &&
          changed && value(f, slot, 14, 55, 15) && value(f, slot, 15, 55, 14));
    for (int32_t x = 17; x <= 30; ++x)
        CHECK(value(f, slot, x, 55, 0));
    CHECK(mc_world_runtime_block_set(world, 16, 64, 8, 55, &changed, &f->error) ==
              MC_WORLD_STORAGE_OK &&
          changed && line(f, slot, 14, 30, true));
    CHECK(source(f, slot, 13, 0) == MC_WORLD_STORAGE_OK && line(f, slot, 14, 30, false));
    if (slot == 2) {
        loaded = NULL;
        CHECK(mc_world_runtime_get(world, 2, 0, &loaded) == MC_CHUNK_ACQUIRE_OK &&
              loaded == f->providers[slot].fallback && loaded->blank && cache->count == CHUNKS &&
              cache->insertion_count == CHUNKS &&
              mc_world_store_chunk_count(f->stores[slot]) == CHUNKS);
    }
    CHECK(mc_test_java_random_equal(world->fields.r, &before) && balanced(f, slot, 0) &&
          cache->count == CHUNKS && cache->insertion_count == CHUNKS && capture_chunks(f, slot));
    ok = true;
cleanup:
    if (!ok)
        fprintf(stderr, "redstone World%zu: status=%u operation=%s update=%u drain=%u\n", slot,
                (unsigned int)f->error.status,
                f->error.operation != NULL ? f->error.operation : "none", f->error.redstone.update,
                f->error.redstone.drain);
    return ok;
}

static bool shared_snapshot(fixture *f) {
    bool ok = false;
    mc_registry_redstone_snapshot *snapshot = NULL;
    mc_registry_redstone_position *positions = NULL;
    mc_registry_classes *registry = f->registry[1];
    mc_block *redstone = registry->blocks.m[55];
    mc_registry_redstone_set_view before, generated, after;
    mc_world_runtime *first = f->worlds[1], *second = f->worlds[3];
    const mc_java_random first_random = *first->fields.r, second_random = *second->fields.r;
    const size_t managed_before = registry->managed_current_bytes;
    ++cases;
    CHECK(first != second && first->fields.w != second->fields.w &&
          first->classes == second->classes && first->classes->registry == registry &&
          second->classes->registry->blocks.m[55] == redstone &&
          f->states[1][1] != f->states[3][1] &&
          mc_registry_redstone_set_inspect(redstone->subtype.sm.b, &before) == MC_REGISTRY_OK &&
          before.size == 0 && balanced(f, 1, 0) && balanced(f, 3, 0));
    const bool original = first->fields.o;
    first->fields.o = true;
    const mc_world_storage_status prepared = source(f, 1, -5, 1);
    first->fields.o = original;
    CHECK(prepared == MC_WORLD_STORAGE_OK && value(f, 1, -5, 70, 1) && line(f, 1, -4, 7, false));
    CHECK(mc_world_runtime_call_enter(first, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_mutation_error_init(&f->error);
    const mc_redstone_frame frame =
        mc_redstone_frame_make(first, redstone, &f->error, MC_REDSTONE_DRAIN);
    const bool updated = mc_redstone_update(&frame, 10, -4, 64, 8, -4, 64, 8);
    mc_world_runtime_call_leave(first);
    /* Strength recursion excludes the incoming position. At this real
     * intermediate boundary only the first redstone has risen; the pending
     * notification drain is what completes the line's propagation. */
    CHECK(updated && value(f, 1, -4, 55, 15) && line(f, 1, -3, 7, false) &&
          line(f, 3, -4, 7, false) &&
          mc_registry_redstone_set_inspect(redstone->subtype.sm.b, &generated) == MC_REGISTRY_OK &&
          generated.size == 7 && generated.constructed_keys == before.constructed_keys + 7u &&
          generated.map == before.map && generated.capacity == before.capacity &&
          registry->redstone_snapshot_count == 0);
    CHECK(mc_registry_redstone_snapshot_copy(redstone->subtype.sm.b, &snapshot) == MC_REGISTRY_OK &&
          mc_registry_redstone_snapshot_size(snapshot) == generated.size &&
          registry->redstone_snapshot_count == 1);
    positions = calloc(generated.size, sizeof(*positions));
    CHECK(positions != NULL);
    for (size_t i = 0; i < generated.size; ++i)
        CHECK(mc_registry_redstone_snapshot_at(snapshot, i, &positions[i]) == MC_REGISTRY_OK &&
              positions[i].identity != 0);

    CHECK(mc_world_runtime_call_enter(first, NULL) == MC_WORLD_STORAGE_OK);
    const bool drained = mc_redstone_drain(&frame, 23, -4, 64, 8);
    mc_world_runtime_call_leave(first);
    CHECK(drained && line(f, 1, -4, 7, true) && line(f, 3, -4, 7, false) && balanced(f, 1, 1) &&
          balanced(f, 3, 1));

    /* The retained snapshot now spans World3's actual refill and drain of
     * the same Set. Immutable keys survive; Chunk/World owners stay distinct. */
    CHECK(source(f, 3, -5, 1) == MC_WORLD_STORAGE_OK && line(f, 3, -4, 7, true) &&
          line(f, 1, -4, 7, true) && balanced(f, 1, 1) && balanced(f, 3, 1));
    CHECK(source(f, 1, -5, 0) == MC_WORLD_STORAGE_OK && line(f, 1, -4, 7, false) &&
          line(f, 3, -4, 7, true));
    CHECK(source(f, 3, -5, 0) == MC_WORLD_STORAGE_OK && line(f, 3, -4, 7, false));
    for (size_t i = 0; i < generated.size; ++i) {
        mc_registry_redstone_position current;
        CHECK(mc_registry_redstone_snapshot_at(snapshot, i, &current) == MC_REGISTRY_OK &&
              current.x == positions[i].x && current.y == positions[i].y &&
              current.z == positions[i].z && current.identity == positions[i].identity &&
              current.identity_hash_ready == positions[i].identity_hash_ready &&
              current.identity_hash == positions[i].identity_hash);
    }
    mc_registry_redstone_snapshot_release(snapshot);
    snapshot = NULL;
    CHECK(mc_registry_redstone_set_inspect(redstone->subtype.sm.b, &after) == MC_REGISTRY_OK &&
          after.map == before.map && after.table == before.table &&
          after.capacity == before.capacity &&
          after.constructed_keys > generated.constructed_keys &&
          registry->managed_current_bytes == managed_before && balanced(f, 1, 0) &&
          balanced(f, 3, 0) && mc_test_java_random_equal(first->fields.r, &first_random) &&
          mc_test_java_random_equal(second->fields.r, &second_random));
    ok = true;
cleanup:
    mc_registry_redstone_snapshot_release(snapshot);
    free(positions);
    return ok;
}

static bool cleanup_fixture(fixture *f) {
    if (f == NULL)
        return true;
    for (size_t slot = 0; slot < WORLDS; ++slot) {
        mc_world_runtime *world = f->worlds[slot];
        if (world == NULL)
            continue;
        if (f->bound[slot]) {
            if (!capture_chunks(f, slot) ||
                mc_world_runtime_retire_entity_containers(world, NULL) != MC_WORLD_STORAGE_OK)
                return false;
            mc_chunk_cache *cache = f->providers[slot].cache;
            if (!mc_chunk_pending_validate(f->providers[slot].pending) ||
                mc_chunk_pending_clear(f->providers[slot].pending) != MC_CHUNK_ACQUIRE_OK ||
                !mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                                     cache->insertion_capacity))
                return false;
        }
        if (mc_world_runtime_retire_world(world, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        for (size_t i = 0; i < CHUNKS; ++i)
            if (f->states[slot][i] != NULL &&
                mc_world_store_retire_chunk(f->stores[slot], f->states[slot][i], NULL) !=
                    MC_WORLD_STORAGE_OK)
                return false;
        if (mc_world_runtime_free(world, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->worlds[slot] = NULL;
        mc_world_provider_release(f->dimensions[slot]);
        mc_world_store_free(f->stores[slot]);
    }
    if (f->server != NULL) {
        mc_server_construction server;
        if (mc_server_runtime_inspect(f->server, NULL, &server) != MC_SERVER_OK ||
            server.references != 1 || mc_server_runtime_release(f->server) != MC_SERVER_OK)
            return false;
    }
    for (size_t side = 0; side < 2; ++side)
        if (mc_world_runtime_classes_free(f->classes[side], NULL) != MC_WORLD_STORAGE_OK ||
            mc_registry_classes_free(f->registry[side], NULL) != MC_REGISTRY_OK ||
            mc_entity_classes_free(f->entities[side]) != MC_ENTITY_OK)
            return false;
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
    const uint16_t title[] = {'W', 'i', 'r', 'e'};
    if (argc != 3 || strcmp(argv[1], "world-redstone-multichunk") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_server_runtime_allocate(SIZE_MAX, &f->server, NULL) == MC_SERVER_OK &&
          mc_server_runtime_construct(f->server, NULL) == MC_SERVER_OK);
    for (size_t side = 0; side < 2; ++side) {
        resource_owner *r = &f->resources[side];
        r->streams[0] = (resource_stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = (resource_stream){.bytes = language};
        r->streams[2] = (resource_stream){.bytes = language};
        r->api = (mc_registry_resources){r, resource_open, resource_close, resource_ready};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entities[side]) == MC_ENTITY_OK);
        CHECK(mc_world_runtime_classes_bind_entities(f->classes[side], f->entities[side]) ==
              MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              r->opens == 3 && r->closes == 1 && !r->bad);
    }
    for (size_t slot = 0; slot < WORLDS; ++slot)
        CHECK(create_world(f, slot, root));
    for (size_t slot = 0; slot < WORLDS; ++slot)
        CHECK(cross_chunks(f, slot));
    CHECK(shared_snapshot(f));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("redstone multichunk cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u redstone multichunk checks (%u literal cases, "
                              "world-redstone-multichunk)\n",
                              checks, cases) < 0)
        result = 1;
    return result;
}
