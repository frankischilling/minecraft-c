#include "../../../../src/block/registry/registry_internal.h"
#include "../../../../src/entity/entity_internal.h"
#include "../../../../src/world/chunk/object_internal.h"
#include "../../../../src/world/chunk/object_state_internal.h"
#include "../../../../src/world/runtime/acquisition_internal.h"
#include "../../../../src/world/runtime/chunk_provider_internal.h"
#include "../../../../src/world/runtime/construction_internal.h"
#include "../../../../src/world/runtime/loading/loading_internal.h"
#include "../../../../src/world/runtime/runtime_internal.h"
#include "../../../../src/world/storage/files.h"
#include "../../../../src/world/storage/objects_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/entity/entity.h"
#include "mc/storage/region/region.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

#include <limits.h>
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

/* Constructed native integration contracts. Real owners and GAME constructors;
 * no original/native or natural-population equivalence. Prepared source-field
 * inputs are restored before CHECK. Counts, holds, phases and leases are never
 * manufactured. No successful gameplay/resource backend is supplied. */
enum { WORLD_COUNT = 4 };

static unsigned int checks;
static mc_math_trig trig;
static const uint32_t fixture_timestamp = UINT32_C(0xfedcba98);
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct stream {
    const uint8_t *bytes;
    size_t length, offset;
    bool closed;
} stream;

typedef struct resources {
    stream streams[3];
    mc_registry_resources api;
    size_t opens, closes;
    bool bad;
    mc_world_runtime_classes *retirement_owner;
    size_t retirement_checks;
} resources;

typedef struct fixture {
    const char *root;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_world_regions *regions;
    mc_world_store *stores[WORLD_COUNT];
    mc_world_info_name *name;
    resources sources[2];
    mc_registry_classes *registry[2];
    mc_entity_classes *entity_classes[2];
    mc_world_runtime_classes *classes[2];
    mc_world_runtime *worlds[WORLD_COUNT];
    mc_world_provider *dimensions[WORLD_COUNT];
    mc_chunk_provider_fields providers[WORLD_COUNT];
    mc_chunk_state *states[WORLD_COUNT];
    mc_world_chunk *chunks[WORLD_COUNT];
    mc_tile_entity *aliases[WORLD_COUNT];
    mc_tile_entity_name *names[2];
    int32_t chest_id[2], spawner_id[2];
    bool bound[WORLD_COUNT], terminal[WORLD_COUNT], retired[WORLD_COUNT];
    bool descriptors_retired[WORLD_COUNT];
} fixture;

typedef struct fixture_record {
    mc_nbt_tag root, level, fields[11];
    const mc_nbt_tag *root_fields[1], *level_fields[11];
    uint16_t names[12][20];
    uint8_t arrays[5][MC_CHUNK_BLOCKS];
} fixture_record;

/* Native UTF-8 transport/properties helpers reused from the real publication
 * fixture. These open actual byte streams and never replace game operations. */
static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count) {
    stream *s = context;
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

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    resources *r = context;
    if (input == NULL || input->context != &r->streams[2] || input->read != read_bytes) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    r->streams[2].closed = true;
    ++r->closes;
    return MC_REGISTRY_OK;
}

static bool ready_resource(void *context, const mc_java_properties_input *input) {
    resources *r = context;
    if (input == NULL || input->read != read_bytes)
        r->bad = true;
    return false;
}

static mc_nbt_string ascii(const char *text, uint16_t *units) {
    const size_t n = strlen(text);
    for (size_t i = 0; i < n; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){units, n};
}

static const mc_nbt_tag *field(const mc_nbt_tag *tag, const char *name) {
    if (tag == NULL || tag->type != MC_NBT_COMPOUND)
        return NULL;
    for (size_t i = 0; i < tag->value.compound.length; ++i) {
        const mc_nbt_tag *child_tag = tag->value.compound.items[i];
        if (child_tag->name.length != strlen(name))
            continue;
        size_t j = 0;
        while (j < child_tag->name.length && child_tag->name.units[j] == (unsigned char)name[j])
            ++j;
        if (j == child_tag->name.length)
            return child_tag;
    }
    return NULL;
}

static bool decode_path(const char *hex, char *out, size_t capacity) {
    const size_t n = strlen(hex);
    if (n == 0 || n % 2u != 0 || n / 2u >= capacity)
        return false;
    for (size_t i = 0; i < n / 2u; ++i) {
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
    out[n / 2u] = '\0';
    return true;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out) {
    resources *r = context;
    if (r->retirement_owner != NULL) {
        mc_world_runtime_classes *owner = r->retirement_owner;
        mc_tile_entity_classes *tile_entities = owner->tile_entities;
        mc_registry_classes *registry = owner->registry;
        mc_entity_classes *entities = owner->entities;
        const size_t registry_borrows = registry->runtime_borrows;
        const size_t entity_borrows = entities->borrowers;
        /* This is a real Block initialization callback. No entered/phase or
         * borrow value is assigned to make class retirement report BUSY. */
        if (mc_world_runtime_classes_free(owner, NULL) != MC_WORLD_STORAGE_BUSY ||
            owner->tile_entities != tile_entities || owner->registry != registry ||
            owner->entities != entities || registry->runtime_borrows != registry_borrows ||
            entities->borrowers != entity_borrows) {
            r->bad = true;
            return MC_REGISTRY_INVALID_ARGUMENT;
        }
        ++r->retirement_checks;
    }
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    if (name == NULL || out == NULL || r->opens >= COUNT(names) ||
        strcmp(name, names[r->opens]) != 0) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    *out = (mc_java_properties_input){&r->streams[r->opens++], read_bytes};
    return MC_REGISTRY_OK;
}

static bool class_retirement_callback(fixture *f, size_t side) {
    bool ok = false;
    resources source = {0};
    mc_registry_classes *registry = NULL;
    mc_entity_classes *entities = NULL;
    mc_world_runtime_classes *classes = NULL;
    mc_world_runtime *world = NULL;
    mc_tile_entity_name *literal = NULL;
    source.streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
    source.streams[1] = source.streams[2] = (stream){.bytes = language};
    source.api = (mc_registry_resources){&source, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                         f->profile, f->materials, 0, &source.api};
    CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK &&
          mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                   NULL, &entities) == MC_ENTITY_OK &&
          mc_world_runtime_classes_create(side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER,
                                          f->profile, &trig, &classes,
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(classes, registry, NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_bind_entities(classes, entities) == MC_ENTITY_OK);
    /* Actual raw GAME allocation is sufficient for the code-literal owner.
     * No source constructor, Chunk or missing Store operation is fabricated. */
    CHECK(mc_world_runtime_allocate_world(side == 0 ? MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA
                                                    : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                          classes, NULL, &world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_live_tile_entities_literal(world, "Pig", &literal, NULL) == MC_WORLD_STORAGE_OK);
    mc_tile_entity_classes *const tile_entities = classes->tile_entities;
    mc_tile_entity_name_release(literal);
    literal = NULL;
    CHECK(tile_entities != NULL && mc_world_runtime_free(world, NULL) == MC_WORLD_STORAGE_OK);
    world = NULL;
    source.retirement_owner = classes;
    CHECK(mc_registry_classes_initialize_blocks(registry, NULL) == MC_REGISTRY_OK &&
          source.retirement_checks == 3 && source.opens == 3 && source.closes == 1 && !source.bad &&
          classes->tile_entities == tile_entities && classes->registry == registry &&
          classes->entities == entities);
    source.retirement_owner = NULL;
    CHECK(mc_world_runtime_classes_free(classes, NULL) == MC_WORLD_STORAGE_OK);
    classes = NULL;
    CHECK(mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
    registry = NULL;
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    entities = NULL;
    ok = true;
cleanup:
    source.retirement_owner = NULL;
    mc_tile_entity_name_release(literal);
    if (world != NULL)
        (void)mc_world_runtime_free(world, NULL);
    if (classes != NULL)
        (void)mc_world_runtime_classes_free(classes, NULL);
    if (registry != NULL)
        (void)mc_registry_classes_free(registry, NULL);
    if (entities != NULL)
        (void)mc_entity_classes_free(entities);
    return ok;
}

static bool open_store(fixture *f, size_t slot, const char *root) {
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const uint16_t title[] = {'P', 'u', 'b', 'l', 'i', 's', 'h'};
    const int n = snprintf(path, sizeof(path), "%s/world%zu", root, slot);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    if (slot >= 2) {
        /* Explicit native normalization-map input. Successful imports must see
         * the stored TileEntity blocks before these bytes become air at finish. */
        CHECK(f->chest_id[slot % 2] >= 0 && f->chest_id[slot % 2] < 256 &&
              f->spawner_id[slot % 2] >= 0 && f->spawner_id[slot % 2] < 256);
        map[f->chest_id[slot % 2]] = 0;
        map[f->spawner_id[slot % 2]] = 0;
    }
    mc_world_store_options options = {.session_token = 137,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = 1;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_level_info info = {.seed = 23,
                                .spawn_y = 64,
                                .time = 0,
                                .level_name = {title, COUNT(title)},
                                .version = 19132};
    CHECK(mc_world_store_save_info(f->stores[slot], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

typedef struct tile_entity_record {
    mc_nbt_tag tile_entities[2], chest[6], spawner[7], item[5];
    const mc_nbt_tag *records[2], *chest_fields[6], *spawner_fields[7], *item_fields[5];
    uint16_t names[18][24], strings[3][16];
} tile_entity_record;

static void prepare_tiles(tile_entity_record *r) {
    const char *const chest_names[] = {"id", "x", "y", "z", "Items", "TileExtra"};
    const char *const spawner_names[] = {"id", "x", "y", "z", "EntityId", "Delay", "TileExtra"};
    const char *const item_names[] = {"Slot", "id", "Count", "Damage", "ItemExtra"};
    const mc_nbt_type chest_types[] = {MC_NBT_STRING, MC_NBT_INT,  MC_NBT_INT,
                                       MC_NBT_INT,    MC_NBT_LIST, MC_NBT_INT};
    const mc_nbt_type spawner_types[] = {MC_NBT_STRING, MC_NBT_INT,   MC_NBT_INT, MC_NBT_INT,
                                         MC_NBT_STRING, MC_NBT_SHORT, MC_NBT_INT};
    const mc_nbt_type item_types[] = {MC_NBT_BYTE, MC_NBT_SHORT, MC_NBT_BYTE, MC_NBT_SHORT,
                                      MC_NBT_LONG};
    for (size_t i = 0; i < COUNT(chest_names); ++i) {
        r->chest[i].type = chest_types[i];
        r->chest[i].name = ascii(chest_names[i], r->names[i]);
        r->chest_fields[i] = &r->chest[i];
    }
    for (size_t i = 0; i < COUNT(spawner_names); ++i) {
        r->spawner[i].type = spawner_types[i];
        r->spawner[i].name = ascii(spawner_names[i], r->names[6 + i]);
        r->spawner_fields[i] = &r->spawner[i];
    }
    for (size_t i = 0; i < COUNT(item_names); ++i) {
        r->item[i].type = item_types[i];
        r->item[i].name = ascii(item_names[i], r->names[13 + i]);
        r->item_fields[i] = &r->item[i];
    }
    r->chest[0].value.string = ascii("Chest", r->strings[0]);
    r->chest[1].value.int_value = 8;
    r->chest[2].value.int_value = 64;
    r->chest[3].value.int_value = 8;
    r->chest[4].value.list.element_type = MC_NBT_COMPOUND;
    r->chest[4].value.list.items = &r->records[0];
    r->chest[4].value.list.length = 1;
    r->chest[5].value.int_value = 77;
    r->item[1].value.short_value = 262;
    r->item[2].value.byte_value = 7;
    r->item[3].value.short_value = 3;
    r->item[4].value.long_value = 123456;
    /* Separate stable record pointer: the top-level records array is filled
     * after these TileEntity compounds are assembled. */
    r->tile_entities[0].type = MC_NBT_COMPOUND;
    r->tile_entities[0].value.compound.items = r->chest_fields;
    r->tile_entities[0].value.compound.length = COUNT(r->chest_fields);
    r->tile_entities[1].type = MC_NBT_COMPOUND;
    r->tile_entities[1].value.compound.items = r->spawner_fields;
    r->tile_entities[1].value.compound.length = COUNT(r->spawner_fields);
    r->spawner[0].value.string = ascii("MobSpawner", r->strings[1]);
    r->spawner[1].value.int_value = 9;
    r->spawner[2].value.int_value = 64;
    r->spawner[3].value.int_value = 8;
    r->spawner[4].value.string = ascii("Zombie", r->strings[2]);
    r->spawner[5].value.short_value = 39;
    r->spawner[6].value.int_value = 88;
}

static size_t cell(int32_t x, int32_t y, int32_t z) {
    return ((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y;
}

static bool name_is(const mc_tile_entity_name *name, const char *text) {
    mc_tile_entity_name_view view;
    if (mc_tile_entity_name_get_view(name, &view) != MC_TILE_ENTITY_OK || view.is_null ||
        view.text.length != strlen(text))
        return false;
    for (size_t i = 0; i < view.text.length; ++i)
        if (view.text.units[i] != (uint16_t)(unsigned char)text[i])
            return false;
    return true;
}

static bool fresh_registry(const mc_registry_error *e, mc_registry_status status) {
    return e->status == status && e->initializer_pc == -1 && e->block_id == -1 &&
           e->constructor_class == NULL && e->constructor_pc == -1 &&
           e->profile_status == MC_JAVA_RANDOM_PROFILE_OK && e->active_initializer_class == NULL &&
           e->active_initializer_pc == -1 && e->item_argument == -1 &&
           e->format_error.status == MC_JAVA_FORMAT_OK && e->format_error.offset == 0 &&
           e->format_error.length == 0 && e->format_error.value == 0 &&
           e->format_error.flags == 0 && e->format_error.conversion == 0 &&
           !e->format_error.date_time &&
           (status == MC_REGISTRY_OK ? e->message == NULL : e->message != NULL);
}

static bool empty_entity_error(const mc_entity_error *e) {
    return e->status == MC_ENTITY_OK && e->operation == NULL && e->bytecode == 0 &&
           e->profile == MC_JAVA_RANDOM_PROFILE_OK && e->item == MC_ITEM_STACK_OK &&
           e->world == MC_WORLD_STORAGE_OK;
}

/* Every independent child/caller field is checked; no padding comparison and
 * no reconstruction from ambient World last-error state. Outer failure PCs and
 * cause are checked separately at the actual acquisition call site. */
static bool empty_tile_children(const mc_live_tile_entity_error *e) {
    const mc_tile_entity_error *t = &e->tile_entity;
    const mc_world_entity_error *p = &e->publication;
    return fresh_registry(&e->registry, MC_REGISTRY_OK) && t->status == MC_TILE_ENTITY_OK &&
           t->field == NULL && t->item_index == SIZE_MAX && t->item.status == MC_ITEM_STACK_OK &&
           t->item.field == NULL && t->item.nbt.status == MC_NBT_OK && t->item.nbt.offset == 0 &&
           t->nbt.status == MC_NBT_OK && t->nbt.offset == 0 && t->source_bytecode == 0 &&
           fresh_registry(&t->registry, MC_REGISTRY_OK) && t->constructor_bytecode == 0 &&
           t->initializer_bytecode == 0 && empty_entity_error(&e->entity) &&
           p->status == MC_WORLD_STORAGE_OK && p->operation == NULL && p->bytecode == 0 &&
           p->presence_bytecode == 0 && p->provider_bytecode == 0 && p->chunk_bytecode == 0 &&
           p->world_access_bytecode == 0 && p->acquisition == MC_CHUNK_ACQUIRE_OK &&
           p->cause.status == MC_WORLD_STORAGE_OK && p->cause.operation == NULL &&
           p->cause.system_error == 0 && empty_entity_error(&p->entity) &&
           e->world_access_bytecode == 0 && e->initializer_bytecode == 0 &&
           e->caller_bytecode == 0 && e->hook_bytecode == 0 && e->remove_bytecode == 0 &&
           e->tile_entity_bytecode == 0 && e->inventory_bytecode == 0 &&
           e->acquisition_caller_bytecode == 0 && e->activation_bytecode == 0 &&
           e->activation_world_bytecode == 0 && e->load_bytecode == 0 &&
           e->load_builder_bytecode == 0 && e->provider_bytecode == 0 &&
           e->acquisition_parent.bytecode == 0 && e->acquisition_parent.chunk_bytecode == 0 &&
           e->acquisition_parent.world_access_bytecode == 0 &&
           e->acquisition_parent.initializer_bytecode == 0 &&
           e->acquisition_parent.caller_bytecode == 0 && e->acquisition_parent.hook_bytecode == 0 &&
           e->acquisition_parent.remove_bytecode == 0 &&
           e->acquisition_parent.tile_entity_bytecode == 0 &&
           e->acquisition_parent.inventory_bytecode == 0 &&
           e->acquisition_parent.provider_bytecode == 0;
}

static bool baseline(fixture *f, size_t slot) {
    bool ok = false;
    char *region = NULL, *saved = NULL;
    char leaf[64];
    mc_ws_file_bytes bytes = {0};
    const int n = snprintf(leaf, sizeof(leaf), "seed-world%zu.region.bin", slot);
    CHECK(n > 0 && (size_t)n < sizeof(leaf));
    CHECK(mc_ws_path_join(mc_world_store_root(f->stores[slot]), "region/r.0.0.mcr", &region,
                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_ws_path_join(f->root, leaf, &saved, NULL) == MC_WORLD_STORAGE_OK &&
          mc_ws_read_file(region, 1024u * 1024u, &bytes, NULL) == MC_WORLD_STORAGE_OK &&
          mc_ws_write_file(saved, bytes.data, bytes.length, true, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    mc_ws_free_file_bytes(&bytes);
    free(region);
    free(saved);
    return ok;
}

static bool seed_record(fixture *f, size_t slot) {
    bool ok = false;
    fixture_record *r = calloc(1, sizeof(*r));
    tile_entity_record tile_entities = {0};
    mc_nbt_tag item = {.type = MC_NBT_COMPOUND};
    const mc_nbt_tag *item_pointer = &item;
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
    prepare_tiles(&tile_entities);
    item.value.compound.items = tile_entities.item_fields;
    item.value.compound.length = COUNT(tile_entities.item_fields);
    tile_entities.chest[4].value.list.items = &item_pointer;
    tile_entities.records[0] = &tile_entities.tile_entities[0];
    tile_entities.records[1] = &tile_entities.tile_entities[1];
    r->root.type = r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3].value.byte_array.data = r->arrays[i];
        r->fields[i + 3].value.byte_array.length = lengths[i];
    }
    if (slot >= 2) {
        r->arrays[0][cell(8, 64, 8)] = (uint8_t)f->chest_id[slot % 2];
        r->arrays[0][cell(9, 64, 8)] = (uint8_t)f->spawner_id[slot % 2];
    }
    r->fields[2].value.long_value = -123;
    r->fields[8].value.byte_value = 1;
    r->fields[9].value.list.element_type = r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[10].value.list.items = tile_entities.records;
    r->fields[10].value.list.length = slot < 2 ? 0 : 2;
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(r->level_fields);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = 1;
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK && length != 0);
    encoded = malloc(length);
    CHECK(encoded != NULL &&
          mc_nbt_write(&r->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = 0;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[slot]), 0, 0, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(baseline(f, slot));
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool create_world(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_runtime_construction state = {0};
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 1;
    limits.max_scheduled_entries = 0;
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[slot], NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_runtime_allocate_world(slot % 2 == 0 ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                                        : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                          f->classes[slot % 2], &limits, &f->worlds[slot],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[slot], .name = f->name, .seed = -99, .dimension = f->dimensions[slot]};
    CHECK(mc_world_runtime_construct_world(f->worlds[slot], &arguments, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->worlds[slot], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY && state.contexts_bound && state.cache_registered);
    CHECK(mc_chunk_provider_bind_fields(f->worlds[slot]->fields.v, &f->providers[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[slot] = true;
    ok = true;
cleanup:
    return ok;
}

static bool shared_and_fresh(fixture *f, size_t side) {
    bool ok = false;
    mc_tile_entity_name *same = NULL, *name = NULL;
    mc_tile_entity *chest = NULL, *spawner = NULL;
    mc_item_stack *stack = NULL, *held = NULL, *split = NULL;
    mc_live_tile_entity_error error;
    CHECK(f->classes[side]->tile_entities == NULL);
    CHECK(mc_live_tile_entities_literal(f->worlds[side], "Pig", &f->names[side], &error) ==
          MC_WORLD_STORAGE_OK);
    mc_tile_entity_classes *const shared = f->classes[side]->tile_entities;
    CHECK(shared != NULL &&
          mc_live_tile_entities_literal(f->worlds[side + 2], "Pig", &same, &error) ==
              MC_WORLD_STORAGE_OK &&
          same == f->names[side] &&
          mc_live_tile_entities_name_identity(same) ==
              mc_live_tile_entities_name_identity(f->names[side]));
    CHECK(mc_live_tile_entities_new(f->worlds[side], MC_TILE_ENTITY_CHEST, &chest, &error) ==
          MC_WORLD_STORAGE_OK);
    int32_t logical = 0, limit = 0;
    size_t physical = 0;
    CHECK(mc_tile_entity_chest_size(chest, &logical) == MC_TILE_ENTITY_OK && logical == 27 &&
          mc_tile_entity_chest_limit(chest, &limit) == MC_TILE_ENTITY_OK && limit == 64 &&
          mc_tile_entity_chest_capacity(chest, &physical) == MC_TILE_ENTITY_OK && physical == 36);
    for (int32_t i = 0; i < 36; ++i) {
        CHECK(mc_tile_entity_chest_get(chest, i, &held) == MC_TILE_ENTITY_OK && held == NULL);
    }
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 262, 85, 3) == MC_ITEM_STACK_OK &&
          mc_tile_entity_chest_set(chest, 0, stack) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_chest_set(chest, 35, stack) == MC_TILE_ENTITY_OK);
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.count == 64);
    CHECK(mc_tile_entity_chest_get(chest, 0, &held) == MC_TILE_ENTITY_OK && held == stack);
    CHECK(mc_tile_entity_chest_set(chest, 0, NULL) == MC_TILE_ENTITY_OK &&
          mc_item_stack_fields_get(held, &fields) == MC_ITEM_STACK_OK && fields.count == 64);
    CHECK(mc_tile_entity_chest_remove(chest, 35, 3, &split) == MC_TILE_ENTITY_OK &&
          split != stack && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
          fields.count == 61 && mc_item_stack_fields_get(split, &fields) == MC_ITEM_STACK_OK &&
          fields.count == 3);
    CHECK(mc_live_tile_entities_new(f->worlds[side + 2], MC_TILE_ENTITY_SPAWNER, &spawner,
                                    &error) == MC_WORLD_STORAGE_OK &&
          f->classes[side]->tile_entities == shared);
    mc_tile_entity_snapshot snapshot;
    CHECK(mc_tile_entity_get_snapshot(spawner, &snapshot) == MC_TILE_ENTITY_OK &&
          snapshot.spawner_delay == 20 && snapshot.spawner_rotation_bits == 0 &&
          snapshot.spawner_previous_rotation_bits == 0 && snapshot.world == NULL &&
          !snapshot.invalid);
    CHECK(mc_tile_entity_spawner_get_name(spawner, &name) == MC_TILE_ENTITY_OK && name == same &&
          name_is(name, "Pig"));
    CHECK(mc_tile_entity_spawner_set_name(spawner, NULL) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_get_snapshot(spawner, &snapshot) == MC_TILE_ENTITY_OK &&
          snapshot.spawner_name == NULL);
    CHECK(mc_tile_entity_spawner_set_name(spawner, name) == MC_TILE_ENTITY_OK);
    ok = true;
cleanup:
    mc_tile_entity_name_release(name);
    mc_tile_entity_name_release(same);
    mc_item_stack_release(held);
    mc_item_stack_release(split);
    mc_item_stack_release(stack);
    mc_tile_entity_release(chest);
    mc_tile_entity_release(spawner);
    return ok;
}

static bool load_world(fixture *f, size_t slot) {
    bool ok = false;
    if (slot < 2) {
        mc_chunk_state *loaded = NULL;
        mc_world_runtime *w = f->worlds[slot];
        mc_world_runtime_acquisition_error detail;
        mc_world_reference_list *const source_c = w->fields.c;
        w->fields.c = NULL; /* Declared actual source destination input. */
        const mc_chunk_acquire_status failed =
            mc_world_runtime_get_detailed(w, 0, 0, &loaded, &detail);
        w->fields.c = source_c;
        if (f->providers[slot].cache->insertion_count != 0)
            f->states[slot] = f->providers[slot].cache->insertions[0];
        CHECK(failed == MC_CHUNK_ACQUIRE_CALLBACK_FAILED && loaded == NULL &&
              f->states[slot] != NULL && f->states[slot]->active &&
              f->providers[slot].cache->count == 1 &&
              f->providers[slot].cache->insertion_count == 1 && detail.status == failed &&
              detail.activation_bytecode == 18 && detail.activation_world_bytecode == 26 &&
              detail.cause.status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              fresh_registry(&detail.registry, MC_REGISTRY_OK) &&
              detail.tile_entity.status == detail.cause.status &&
              detail.tile_entity.operation != NULL &&
              detail.tile_entity.operation == detail.cause.operation &&
              detail.tile_entity.cause.status == detail.cause.status &&
              detail.tile_entity.cause.operation == detail.cause.operation &&
              detail.tile_entity.cause.system_error == 0 && detail.tile_entity.bytecode == 18 &&
              detail.tile_entity.chunk_bytecode == 26 && empty_tile_children(&detail.tile_entity));
        /* A genuine cached hit does not rerun activation. Reuse the prior
         * failed detail and require complete per-attempt freshness. */
        CHECK(mc_world_runtime_get_detailed(w, 0, 0, &loaded, &detail) == MC_CHUNK_ACQUIRE_OK &&
              loaded == f->states[slot] && detail.status == MC_CHUNK_ACQUIRE_OK &&
              detail.activation_bytecode == 0 && detail.activation_world_bytecode == 0 &&
              detail.cause.status == MC_WORLD_STORAGE_OK && detail.cause.operation == NULL &&
              detail.cause.system_error == 0 && fresh_registry(&detail.registry, MC_REGISTRY_OK) &&
              detail.tile_entity.status == MC_WORLD_STORAGE_OK &&
              detail.tile_entity.operation == NULL &&
              detail.tile_entity.cause.status == MC_WORLD_STORAGE_OK &&
              detail.tile_entity.cause.operation == NULL &&
              detail.tile_entity.cause.system_error == 0 && detail.tile_entity.bytecode == 0 &&
              detail.tile_entity.chunk_bytecode == 0 && empty_tile_children(&detail.tile_entity) &&
              f->providers[slot].cache->count == 1 && source_c->count == 0 &&
              mc_live_tile_entities_lists_valid(w));
    } else {
        CHECK(mc_world_runtime_load_game(f->worlds[slot], 0, 0, &f->states[slot], NULL) ==
              MC_WORLD_STORAGE_OK);
    }
    f->chunks[slot] = mc_world_store_chunk_object(f->stores[slot], f->states[slot]);
    CHECK(f->chunks[slot] != NULL && f->states[slot]->active == (slot < 2) &&
          f->states[slot]->terrain_populated && mc_live_tile_entities_map_valid(f->chunks[slot]) &&
          mc_live_tile_entities_lists_valid(f->worlds[slot]));
    size_t count = 0;
    CHECK(mc_world_chunk_tile_entity_count(f->chunks[slot]->l, &count) == MC_WORLD_STORAGE_OK &&
          count == (slot < 2 ? 0u : 2u) && f->worlds[slot]->fields.c->count == 0);
    if (slot < 2) {
        ok = true;
        goto cleanup;
    }
    CHECK(f->states[slot]->data->blocks[cell(8, 64, 8)] == 0 &&
          f->states[slot]->data->blocks[cell(9, 64, 8)] == 0);
    for (size_t i = 0; i < count; ++i) {
        mc_tile_entity *value = NULL;
        mc_tile_entity_snapshot snapshot;
        CHECK(mc_live_tile_entities_map_at(f->chunks[slot], i, &value) == MC_WORLD_STORAGE_OK &&
              mc_tile_entity_get_snapshot(value, &snapshot) == MC_TILE_ENTITY_OK &&
              snapshot.world == f->worlds[slot] && !snapshot.invalid);
        if (snapshot.kind == MC_TILE_ENTITY_CHEST) {
            CHECK(snapshot.chest_capacity == 27 &&
                  mc_tile_entity_retain(value) == MC_TILE_ENTITY_OK);
            f->aliases[slot] = value;
        } else {
            CHECK(snapshot.kind == MC_TILE_ENTITY_SPAWNER && snapshot.spawner_delay == 39 &&
                  name_is(snapshot.spawner_name, "Zombie"));
        }
    }
    CHECK(f->aliases[slot] != NULL);
    mc_nbt_tag empty = {.type = MC_NBT_LIST, .value.list.element_type = MC_NBT_COMPOUND};
    mc_live_tile_entity_error error;
    CHECK(mc_live_tile_entities_load_records(f->chunks[slot], &empty, NULL, &error) !=
              MC_WORLD_STORAGE_OK &&
          mc_world_chunk_tile_entity_count(f->chunks[slot]->l, &count) == MC_WORLD_STORAGE_OK &&
          count == 2);
    ok = true;
cleanup:
    return ok;
}

static bool install_primary(fixture *f, size_t side) {
    bool ok = false, changed = false;
    mc_world_mutation_error mutation;
    mc_live_tile_entity_error error;
    CHECK(mc_world_runtime_block_set_quiet(f->worlds[side], 8, 64, 8, f->chest_id[side], &changed,
                                           &mutation) == MC_WORLD_STORAGE_OK &&
          changed);
    CHECK(mc_world_runtime_block_set_quiet(f->worlds[side], 9, 64, 8, f->spawner_id[side], &changed,
                                           &mutation) == MC_WORLD_STORAGE_OK &&
          changed);
    CHECK(mc_live_tile_entities_get(f->worlds[side], 8, 64, 8, &f->aliases[side], &error) ==
              MC_WORLD_STORAGE_OK &&
          f->aliases[side] != NULL && f->chunks[side]->l->count == 2 &&
          f->worlds[side]->fields.c->count == 2);
    ok = true;
cleanup:
    return ok;
}

static bool nbt_copy(fixture *f, size_t side) {
    bool ok = false;
    tile_entity_record r = {0};
    mc_nbt_tag item = {.type = MC_NBT_COMPOUND};
    const mc_nbt_tag *item_pointer = &item;
    mc_nbt_tree *input = NULL;
    mc_nbt_document *output = NULL;
    mc_tile_entity *tile_entity = NULL;
    mc_item_stack *stack = NULL;
    mc_live_tile_entity_error error;
    mc_tile_entity_error detail;
    prepare_tiles(&r);
    item.value.compound.items = r.item_fields;
    item.value.compound.length = COUNT(r.item_fields);
    r.chest[4].value.list.items = &item_pointer;
    CHECK(mc_nbt_tree_copy_compound(&r.tile_entities[0], NULL, &input, NULL) == MC_NBT_OK);
    CHECK(mc_live_tile_entities_factory(f->worlds[side], mc_nbt_tree_root(input), NULL, true,
                                        &tile_entity, &error) == MC_WORLD_STORAGE_OK &&
          tile_entity != NULL);
    mc_nbt_tree_free(input);
    input = NULL;
    CHECK(mc_tile_entity_chest_get(tile_entity, 0, &stack) == MC_TILE_ENTITY_OK && stack != NULL &&
          mc_tile_entity_chest_set(tile_entity, 1, stack) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_chest_set(tile_entity, 0, NULL) == MC_TILE_ENTITY_OK);
    size_t graph = 0;
    CHECK(mc_tile_entity_storage_bytes(tile_entity, &graph) == MC_TILE_ENTITY_OK && graph != 0);
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = graph;
    CHECK(mc_tile_entity_build(tile_entity, &limits, &output, &detail) == MC_TILE_ENTITY_LIMIT &&
          output == NULL);
    CHECK(mc_tile_entity_build(tile_entity, NULL, &output, &detail) == MC_TILE_ENTITY_OK);
    mc_tile_entity_release(tile_entity);
    tile_entity = NULL;
    mc_item_stack_release(stack);
    stack = NULL;
    const mc_nbt_tag *root = mc_nbt_document_root(output);
    const mc_nbt_tag *extra = field(root, "TileExtra"), *items = field(root, "Items");
    CHECK(extra != NULL && extra->type == MC_NBT_INT && extra->value.int_value == 77 &&
          items != NULL && items->type == MC_NBT_LIST && items->value.list.length == 1);
    const mc_nbt_tag *record = items->value.list.items[0];
    const mc_nbt_tag *slot = field(record, "Slot"), *unknown = field(record, "ItemExtra");
    CHECK(slot != NULL && slot->type == MC_NBT_BYTE && slot->value.byte_value == 1 &&
          unknown != NULL && unknown->type == MC_NBT_LONG && unknown->value.long_value == 123456);
    ok = true;
cleanup:
    mc_nbt_tree_free(input);
    mc_nbt_document_free(output);
    mc_item_stack_release(stack);
    mc_tile_entity_release(tile_entity);
    return ok;
}

static bool lookup_and_hooks(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *registry = f->registry[side];
    mc_world_chunk *chunk = f->chunks[side];
    mc_live_tile_entity_error error;
    mc_tile_entity *out = f->aliases[side];
    const size_t target = cell(12, 64, 8);
    const uint8_t saved = chunk->data->blocks[target];
    bool *saved_p = registry->blocks.p;
    mc_block **saved_m = registry->blocks.m;
    mc_block *chest_block = saved_m[f->chest_id[side]];
    mc_block *stone = registry->blocks.named[0];
    const size_t maps = chunk->l->count, active = w->fields.c->count;
    mc_chunk_provider *const saved_v = w->fields.v;
    w->fields.v = NULL; /* Actual source receiver input, restored before CHECK. */
    const mc_world_storage_status provider_status =
        mc_live_tile_entities_get(w, 12, 64, 8, &out, &error);
    mc_world_mutation_error provider_mutation;
    bool ordinary = true;
    const mc_world_storage_status mutation_status =
        mc_world_runtime_block_set_quiet(w, 12, 64, 8, 0, &ordinary, &provider_mutation);
    w->fields.v = saved_v;
    CHECK(provider_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && error.provider_bytecode == 6 &&
          out == f->aliases[side] && mutation_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          ordinary && provider_mutation.bytecode == 48 &&
          provider_mutation.provider_bytecode == 6 && chunk->data->blocks[target] == saved &&
          chunk->l->count == maps && w->fields.c->count == active);
    /* Labeled source inputs; every field is restored before assertions. */
    chunk->data->blocks[target] = (uint8_t)f->chest_id[side];
    registry->blocks.p = NULL;
    mc_world_storage_status status = mc_live_tile_entities_get(w, 12, 64, 8, &out, &error);
    registry->blocks.p = saved_p;
    chunk->data->blocks[target] = saved;
    CHECK(status != MC_WORLD_STORAGE_OK && out == f->aliases[side] && error.chunk_bytecode == 47 &&
          fresh_registry(&error.registry, MC_REGISTRY_NULL_REFERENCE));
    chunk->data->blocks[target] = (uint8_t)f->chest_id[side];
    registry->blocks.m = NULL;
    status = mc_live_tile_entities_get(w, 12, 64, 8, &out, &error);
    registry->blocks.m = saved_m;
    chunk->data->blocks[target] = saved;
    CHECK(status != MC_WORLD_STORAGE_OK && out == f->aliases[side] && error.chunk_bytecode == 58 &&
          fresh_registry(&error.registry, MC_REGISTRY_NULL_REFERENCE));
    chunk->data->blocks[target] = (uint8_t)f->chest_id[side];
    saved_m[f->chest_id[side]] = stone;
    status = mc_live_tile_entities_get(w, 12, 64, 8, &out, &error);
    saved_m[f->chest_id[side]] = chest_block;
    chunk->data->blocks[target] = saved;
    CHECK(status != MC_WORLD_STORAGE_OK && error.chunk_bytecode == 59 && out == f->aliases[side] &&
          fresh_registry(&error.registry, MC_REGISTRY_OK));
    chunk->data->blocks[target] = (uint8_t)f->chest_id[side];
    saved_m[f->chest_id[side]] = NULL;
    status = mc_live_tile_entities_get(w, 12, 64, 8, &out, &error);
    saved_m[f->chest_id[side]] = chest_block;
    chunk->data->blocks[target] = saved;
    CHECK(status != MC_WORLD_STORAGE_OK && error.chunk_bytecode == 89 && out == f->aliases[side] &&
          fresh_registry(&error.registry, MC_REGISTRY_OK) && chunk->l->count == maps &&
          w->fields.c->count == active);
    out = NULL;
    CHECK(mc_live_tile_entities_get(w, 12, 64, 8, &out, &error) == MC_WORLD_STORAGE_OK &&
          out == NULL);
    bool changed = false;
    mc_world_mutation_error mutation;
    CHECK(mc_world_runtime_block_set_quiet(w, 10, 64, 8, f->chest_id[side], &changed, &mutation) ==
              MC_WORLD_STORAGE_OK &&
          changed);
    CHECK(mc_world_runtime_block_set_quiet(w, 11, 64, 8, f->spawner_id[side], &changed,
                                           &mutation) == MC_WORLD_STORAGE_OK &&
          changed);
    CHECK(mc_live_tile_entities_get(w, 11, 64, 8, &out, &error) == MC_WORLD_STORAGE_OK &&
          out != NULL);
    mc_tile_entity_snapshot snapshot;
    const mc_tile_entity_status inspected = mc_tile_entity_get_snapshot(out, &snapshot);
    mc_tile_entity_release(out);
    out = NULL;
    CHECK(inspected == MC_TILE_ENTITY_OK && snapshot.kind == MC_TILE_ENTITY_SPAWNER &&
          snapshot.world == w && snapshot.x == 11 && snapshot.y == 64 && snapshot.z == 8);
    /* Real same-owner Map alias/replacement, without editing node/hold counts. */
    CHECK(mc_world_runtime_block_set_quiet(w, 14, 64, 8, f->chest_id[side], &changed, &mutation) ==
              MC_WORLD_STORAGE_OK &&
          changed &&
          mc_live_tile_entities_set(w, 14, 64, 8, f->aliases[side], &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_live_tile_entities_get(w, 8, 64, 8, &out, &error) == MC_WORLD_STORAGE_OK &&
          out == f->aliases[side]);
    mc_tile_entity_release(out);
    out = NULL;
    CHECK(mc_live_tile_entities_map_valid(chunk) && mc_live_tile_entities_lists_valid(w));
    ok = true;
cleanup:
    /* out is a borrowed sentinel before the successful get cases. */
    if (out != NULL && out != f->aliases[side])
        mc_tile_entity_release(out);
    return ok;
}

static bool containers_and_records(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[slot];
    mc_world_chunk *chunk = f->chunks[slot];
    mc_live_tile_entity_error error;
    mc_nbt_tree *records = NULL;
    const size_t count = chunk->l->count, before = w->fields.c->count;
    CHECK(mc_live_tile_entities_chunk_register(chunk, &error) == MC_WORLD_STORAGE_OK &&
          mc_live_tile_entities_chunk_register(chunk, &error) == MC_WORLD_STORAGE_OK &&
          w->fields.c->count == before + 2 * count);
    for (size_t i = 0; i < 2 * count; ++i) {
        mc_tile_entity *value = NULL;
        CHECK(mc_live_tile_entities_map_at(chunk, i % count, &value) == MC_WORLD_STORAGE_OK &&
              w->fields.c->items[before + i] == value);
    }
    if (slot < 2) {
        const bool saved_l = w->fields.L;
        w->fields.L = true; /* Declared source boolean input; no ledger mutation. */
        const mc_world_storage_status first = mc_live_tile_entities_chunk_register(chunk, &error);
        const mc_world_storage_status second = mc_live_tile_entities_chunk_register(chunk, &error);
        w->fields.L = saved_l;
        CHECK(first == MC_WORLD_STORAGE_OK && second == MC_WORLD_STORAGE_OK &&
              w->fields.G->count == 2 * count && mc_live_tile_entities_lists_valid(w));
        /* Direct bounded deferred-phase contract; this is not a World tick. */
        CHECK(!w->fields.L &&
              mc_live_tile_entities_drain_deferred(w, &error) == MC_WORLD_STORAGE_OK &&
              w->fields.G->count == 0 && mc_live_tile_entities_lists_valid(w));
    }
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = mc_world_chunk_allocation_bytes(chunk);
    CHECK(mc_live_tile_entities_build_records(chunk, &limits, &records, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          records == NULL);
    CHECK(mc_live_tile_entities_build_records(chunk, NULL, &records, &error) ==
          MC_WORLD_STORAGE_OK);
    const mc_nbt_tag *list = field(mc_nbt_tree_root(records), "TileEntities");
    CHECK(list != NULL && list->type == MC_NBT_LIST && list->value.list.length == chunk->l->count);
    for (size_t i = 0; i < list->value.list.length; ++i) {
        mc_tile_entity *value = NULL;
        mc_tile_entity_snapshot snapshot;
        const mc_nbt_tag *record = list->value.list.items[i];
        CHECK(mc_live_tile_entities_map_at(chunk, i, &value) == MC_WORLD_STORAGE_OK &&
              mc_tile_entity_get_snapshot(value, &snapshot) == MC_TILE_ENTITY_OK &&
              field(record, "x") != NULL && field(record, "x")->value.int_value == snapshot.x &&
              field(record, "y")->value.int_value == snapshot.y &&
              field(record, "z")->value.int_value == snapshot.z);
    }
    CHECK(mc_live_tile_entities_chunk_invalidate(chunk, &error) == MC_WORLD_STORAGE_OK &&
          f->states[slot]->active == (slot < 2) && chunk->l->count == count);
    for (size_t i = 0; i < count; ++i) {
        mc_tile_entity *value = NULL;
        mc_tile_entity_snapshot snapshot;
        CHECK(mc_live_tile_entities_map_at(chunk, i, &value) == MC_WORLD_STORAGE_OK &&
              mc_tile_entity_get_snapshot(value, &snapshot) == MC_TILE_ENTITY_OK &&
              snapshot.invalid && snapshot.world == w);
    }
    for (size_t i = 0; i < count; ++i) {
        mc_tile_entity *value = NULL;
        CHECK(mc_live_tile_entities_map_at(chunk, i, &value) == MC_WORLD_STORAGE_OK);
        /* Actual validation API restores this declared input for subsequent
         * source removal/drop cases; no occurrence/hold is reconstructed.
         * Inspect every occurrence first: duplicate keys can hold the same TileEntity. */
        CHECK(mc_tile_entity_set_invalid(value, false) == MC_TILE_ENTITY_OK);
    }
    CHECK(mc_live_tile_entities_map_valid(chunk) && mc_live_tile_entities_lists_valid(w));
    ok = true;
cleanup:
    mc_nbt_tree_free(records);
    return ok;
}

static bool drops_and_empty(fixture *f, size_t side) {
    bool ok = false;
    mc_tile_entity *chest = NULL;
    mc_item_stack *stack = NULL;
    mc_nbt_tree *records = NULL;
    mc_world_runtime *w = f->worlds[side];
    mc_live_tile_entity_error error;
    CHECK(mc_live_tile_entities_get(w, 10, 64, 8, &chest, &error) == MC_WORLD_STORAGE_OK &&
          chest != NULL);
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 262, 1, 3) == MC_ITEM_STACK_OK);
    CHECK(mc_tile_entity_chest_set_detailed(chest, 0, stack, &error) == MC_TILE_ENTITY_OK &&
          f->states[side]->dirty);
    const size_t before = w->fields.b->count;
    bool changed = false;
    mc_world_mutation_error mutation;
    CHECK(mc_world_runtime_block_set_quiet(w, 10, 64, 8, 0, &changed, &mutation) ==
              MC_WORLD_STORAGE_OK &&
          changed && w->fields.b->count == before + 1);
    mc_entity *entity = w->fields.b->items[before];
    mc_entity_snapshot snapshot;
    mc_item_stack_fields fields;
    CHECK(mc_entity_get_snapshot(entity, &snapshot) == MC_ENTITY_OK && snapshot.base.world == w &&
          snapshot.stack != NULL &&
          mc_item_stack_fields_get(snapshot.stack, &fields) == MC_ITEM_STACK_OK &&
          fields.id == 262 && fields.count == 1 && fields.damage == 3);
    CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.count == 0);
    /* Real World removal leaves header/provenance ownership even at size0. */
    const int32_t xs[] = {8, 9, 11, 14};
    for (size_t i = 0; i < COUNT(xs); ++i)
        CHECK(mc_live_tile_entities_remove(w, xs[i], 64, 8, &error) == MC_WORLD_STORAGE_OK);
    CHECK(f->chunks[side]->l->count == 0 && f->chunks[side]->l->backing != NULL &&
          mc_live_tile_entities_map_valid(f->chunks[side]) &&
          !mc_world_chunk_storage_empty(f->chunks[side]));
    CHECK(mc_live_tile_entities_build_records(f->chunks[side], NULL, &records, &error) ==
          MC_WORLD_STORAGE_OK);
    const mc_nbt_tag *list = field(mc_nbt_tree_root(records), "TileEntities");
    CHECK(list != NULL && list->value.list.length == 0);
    ok = true;
cleanup:
    mc_tile_entity_release(chest);
    mc_item_stack_release(stack);
    mc_nbt_tree_free(records);
    return ok;
}

static bool retire_descriptors(fixture *f, size_t slot) {
    if (!f->bound[slot] || f->descriptors_retired[slot])
        return true;
    if (!f->terminal[slot])
        return false;
    mc_chunk_cache *cache = f->providers[slot].cache;
    if (!mc_chunk_cache_validate(cache) || !mc_chunk_pending_validate(f->providers[slot].pending) ||
        mc_chunk_pending_clear(f->providers[slot].pending) != MC_CHUNK_ACQUIRE_OK ||
        !mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                             cache->insertion_capacity))
        return false;
    f->descriptors_retired[slot] = true;
    return true;
}

static bool retirement(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[slot];
    mc_item_stack *stack = NULL;
    mc_nbt_tree *records = NULL;
    const size_t active = w->fields.c->count, deferred = w->fields.G->count;
    const size_t map_count = f->chunks[slot]->l->count;
    CHECK(mc_world_store_lease_chunk(f->stores[slot], f->states[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_world_storage_status attempted = mc_world_runtime_retire_entity_containers(w, NULL);
    const mc_world_storage_status released =
        mc_world_store_release_chunk(f->stores[slot], f->states[slot], NULL);
    CHECK(attempted == MC_WORLD_STORAGE_BUSY && released == MC_WORLD_STORAGE_OK &&
          w->fields.c->count == active && w->fields.G->count == deferred &&
          f->chunks[slot]->l->count == map_count && mc_live_tile_entities_lists_valid(w));
    CHECK(mc_world_runtime_retire_entity_containers(w, NULL) == MC_WORLD_STORAGE_OK);
    f->terminal[slot] = true;
    CHECK(w->fields.c->count == 0 && w->fields.G->count == 0 && w->fields.b->count == 0 &&
          f->chunks[slot]->l->count == 0);
    mc_live_tile_entity_error error;
    CHECK(mc_live_tile_entities_build_records(f->chunks[slot], NULL, &records, &error) !=
              MC_WORLD_STORAGE_OK &&
          records == NULL);
    /* An actual external alias keeps its retired World binding. The real
     * setter installs/clamps before the reached dirty admission failure. */
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 262, 99, 0) == MC_ITEM_STACK_OK);
    CHECK(mc_tile_entity_chest_set_detailed(f->aliases[slot], 2, stack, &error) ==
              MC_TILE_ENTITY_WORLD_FAILED &&
          error.tile_entity_bytecode == 24 && error.inventory_bytecode == 31);
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.count == 64);
    CHECK(retire_descriptors(f, slot) &&
          mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_OK);
    f->retired[slot] = true;
    CHECK(mc_world_store_retire_chunk(f->stores[slot], f->states[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->states[slot] = NULL;
    f->chunks[slot] = NULL;
    CHECK(mc_world_runtime_free(w, NULL) == MC_WORLD_STORAGE_BUSY);
    ok = true;
cleanup:
    mc_nbt_tree_free(records);
    mc_item_stack_release(stack);
    return ok;
}

static bool cleanup_fixture(fixture *f) {
    if (f == NULL)
        return true;
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot) {
        if (f->worlds[slot] == NULL)
            continue;
        if (f->bound[slot] && !f->terminal[slot]) {
            if (mc_world_runtime_retire_entity_containers(f->worlds[slot], NULL) !=
                MC_WORLD_STORAGE_OK)
                return false;
            f->terminal[slot] = true;
        }
        if (!retire_descriptors(f, slot))
            return false;
        if (!f->retired[slot]) {
            if (mc_world_runtime_retire_world(f->worlds[slot], NULL) != MC_WORLD_STORAGE_OK)
                return false;
            f->retired[slot] = true;
        }
        if (f->states[slot] != NULL) {
            if (mc_world_store_retire_chunk(f->stores[slot], f->states[slot], NULL) !=
                MC_WORLD_STORAGE_OK)
                return false;
            f->states[slot] = NULL;
        }
        mc_tile_entity_release(f->aliases[slot]);
        f->aliases[slot] = NULL;
        if (mc_world_runtime_free(f->worlds[slot], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        mc_world_provider_release(f->dimensions[slot]);
    }
    for (size_t side = 0; side < 2; ++side) {
        if (f->names[side] != NULL &&
            mc_world_runtime_classes_free(f->classes[side], NULL) != MC_WORLD_STORAGE_BUSY)
            return false;
        mc_tile_entity_name_release(f->names[side]);
        if (mc_world_runtime_classes_free(f->classes[side], NULL) != MC_WORLD_STORAGE_OK ||
            mc_registry_classes_free(f->registry[side], NULL) != MC_REGISTRY_OK ||
            mc_entity_classes_free(f->entity_classes[side]) != MC_ENTITY_OK)
            return false;
    }
    mc_world_info_name_release(f->name);
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        if (mc_world_store_free_checked(&f->stores[slot], NULL) != MC_WORLD_STORAGE_OK)
            return false;
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
    const uint16_t title[] = {'T', 'i', 'l', 'e', 's'};
    const bool core = argc == 3 && strcmp(argv[1], "world-tile-entities-core") == 0;
    if (argc != 3 || (!core && strcmp(argv[1], "world-tile-entities") != 0) ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    f->root = root;
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    for (size_t side = 0; side < 2; ++side)
        CHECK(class_retirement_callback(f, side));
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK &&
              mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
                  MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
                  MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              r->opens == 3 && r->closes == 1 && !r->bad);
        const mc_block *chest = NULL, *spawner = NULL;
        CHECK(mc_registry_block_static_get(f->registry[side], "av", &chest, NULL) ==
                  MC_REGISTRY_OK &&
              mc_registry_block_static_get(f->registry[side], "at", &spawner, NULL) ==
                  MC_REGISTRY_OK &&
              chest != NULL && spawner != NULL);
        f->chest_id[side] = mc_block_fields_get(chest)->bn;
        f->spawner_id[side] = mc_block_fields_get(spawner)->bn;
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(open_store(f, slot, root) && create_world(f, slot));
    for (size_t side = 0; side < 2; ++side)
        CHECK(shared_and_fresh(f, side) && nbt_copy(f, side));
    /* Core uses real empty loaded records. No live Store import is needed to
     * run constructors, source-added owners and the ownership regressions. */
    for (size_t slot = 0; slot < 2; ++slot)
        CHECK(seed_record(f, slot) && load_world(f, slot) && install_primary(f, slot));
    for (size_t side = 0; side < 2; ++side)
        CHECK(lookup_and_hooks(f, side));
    for (size_t slot = 0; slot < 2; ++slot)
        CHECK(containers_and_records(f, slot));
    for (size_t side = 0; side < 2; ++side)
        CHECK(drops_and_empty(f, side));
    for (size_t slot = 0; slot < 2; ++slot)
        CHECK(retirement(f, slot));
    /* Full mode genuinely requires the owned nonempty GAME import route.
     * No detached header, reset journal or fabricated successful load is used. */
    if (!core)
        for (size_t slot = 2; slot < WORLD_COUNT; ++slot)
            CHECK(seed_record(f, slot) && load_world(f, slot) && containers_and_records(f, slot) &&
                  retirement(f, slot));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("TileEntity cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u live TileEntity integration checks (%s)\n", checks,
                              core ? "world-tile-entities-core" : "world-tile-entities") < 0)
        result = 1;
    return result;
}
