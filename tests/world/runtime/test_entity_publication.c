#include "mc/block/registry/runtime.h"
#include "mc/entity/entity.h"
#include "mc/storage/region/region.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/saving/saving.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/entity/entity_internal.h"
#include "../../../src/platform/time/millis.h"
#include "../../../src/world/runtime/construction_internal.h"
#include "../../../src/world/runtime/provider_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/storage/files.h"
#include "../../../src/world/storage/objects_internal.h"
#include "../../support/java_random.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        const bool mc_check_success = !!(condition);                                               \
        if (!mc_check_success) {                                                                   \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Constructed native contracts, not original observed state. All eight loaded
 * records and GAME constructors are real. Native observer/nullable Stack controls are
 * temporary and restored before assertions. No Entity phase/position fields,
 * holds, World/cache counts, limits, Random or returned results are injected. */
enum { WORLD_COUNT = 8, ENTITY_COUNT = 32 };
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
} resources;
typedef struct fixture {
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
    mc_world_dimension *dimensions[WORLD_COUNT];
    mc_world_provider_fields providers[WORLD_COUNT];
    mc_chunk_state *states[WORLD_COUNT];
    mc_world_chunk *chunks[WORLD_COUNT];
    const mc_world_chunk_entity_list *slices[WORLD_COUNT][8];
    mc_world_reference_list *world_lists[WORLD_COUNT];
    size_t initial_chunk_bytes[WORLD_COUNT];
    size_t baseline_world_bytes[2];
    bool bound[WORLD_COUNT], leased[WORLD_COUNT], terminal[WORLD_COUNT];
    bool descriptors_retired[WORLD_COUNT], world_retired[WORLD_COUNT];
    mc_ws_file_bytes saved[WORLD_COUNT];
    mc_item_stack *stack;
    mc_entity *entities[ENTITY_COUNT];
    size_t entity_count;
} fixture;
typedef struct fixture_record {
    mc_nbt_tag root, level, fields[11];
    const mc_nbt_tag *root_fields[1], *level_fields[11];
    uint16_t names[12][20];
    uint8_t arrays[5][MC_CHUNK_BLOCKS];
} fixture_record;

/* Actual transport/codec helpers from the frozen real support/fluid fixture,
 * with no observer, allocator, clock or gameplay callback injected. */

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count)
{
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

static bool ready_resource(void *context, const mc_java_properties_input *input)
{
    resources *r = context;
    if (input == NULL || input->read != read_bytes)
        r->bad = true;
    return false;
}

static mc_nbt_string ascii(const char *text, uint16_t *units)
{
    const size_t n = strlen(text);
    for (size_t i = 0; i < n; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){units, n};
}

static const mc_nbt_tag *field(const mc_nbt_tag *tag, const char *name)
{
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

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24u) | ((uint32_t)p[1] << 16u) | ((uint32_t)p[2] << 8u) | p[3];
}

static bool decode_path(const char *hex, char *out, size_t capacity)
{
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
                                        mc_java_properties_input *out)
{
    resources *r = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    if (name == NULL || out == NULL || r->opens >= COUNT(names) ||
        strcmp(name, names[r->opens]) != 0) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    *out = (mc_java_properties_input){&r->streams[r->opens++], read_bytes};
    return MC_REGISTRY_OK;
}

static bool region_path(fixture *f, size_t slot, char **out)
{
    return mc_ws_path_join(mc_world_store_root(f->stores[slot]), "region/r.0.0.mcr", out, NULL) ==
           MC_WORLD_STORAGE_OK;
}

static bool open_store(fixture *f, size_t slot, const char *root)
{
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const uint16_t title[] = {'P', 'u', 'b', 'l', 'i', 's', 'h'};
    const int n = snprintf(path, sizeof(path), "%s/world%zu", root, slot);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 137,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = 1;
    if (slot >= 6)
        options.limits.nbt.max_allocation_bytes = 256u * 1024u;
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

static bool seed_record(fixture *f, size_t slot)
{
    bool ok = false;
    fixture_record *r = calloc(1, sizeof(*r));
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
    r->root.type = MC_NBT_COMPOUND;
    r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    r->fields[2].value.long_value = -123;
    r->fields[8].value.byte_value = 1;
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3u].value.byte_array.data = r->arrays[i];
        r->fields[i + 3u].value.byte_array.length = lengths[i];
    }
    r->fields[9].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
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
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool create_world(fixture *f, size_t slot)
{
    bool ok = false;
    mc_world_runtime_construction state = {0};
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 1;
    limits.max_scheduled_entries = 0;
    if (slot == 4 || slot == 5)
        limits.max_direct_allocation_bytes = f->baseline_world_bytes[slot % 2u];
    CHECK(mc_world_dimension_create(0, NULL, &f->dimensions[slot], NULL) == MC_WORLD_DIMENSION_OK);
    CHECK(mc_world_runtime_allocate_world(slot % 2u == 0 ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                                         : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                          f->classes[slot % 2u], &limits, &f->worlds[slot],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[slot], .name = f->name, .seed = -99, .dimension = f->dimensions[slot]};
    CHECK(mc_world_runtime_construct_world(f->worlds[slot], &arguments, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->worlds[slot], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY && state.contexts_bound && state.cache_registered);
    if (slot < 2)
        f->baseline_world_bytes[slot] = state.direct_allocation_bytes;
    if (slot == 4 || slot == 5)
        CHECK(state.direct_allocation_bytes == limits.max_direct_allocation_bytes);
    CHECK(mc_world_provider_bind_fields(f->worlds[slot]->fields.v, &f->providers[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[slot] = true;
    CHECK(seed_record(f, slot));
    mc_chunk_state *loaded = NULL;
    const mc_chunk_acquire_status acquired = mc_world_runtime_get(f->worlds[slot], 0, 0, &loaded);
    /* Record a genuine published owner even if a later activation failed. */
    CHECK(mc_chunk_cache_validate(f->providers[slot].cache));
    if (f->providers[slot].cache->insertion_count == 1)
        f->states[slot] = f->providers[slot].cache->insertions[0];
    CHECK(acquired == MC_CHUNK_ACQUIRE_OK && loaded != NULL && loaded == f->states[slot] &&
          loaded->active && loaded->terrain_populated && !loaded->dirty && loaded->x == 0 &&
          loaded->z == 0);
    f->chunks[slot] = mc_world_store_chunk_object(f->stores[slot], loaded);
    mc_world_chunk_fields fields = {0};
    CHECK(f->chunks[slot] != NULL &&
          mc_world_chunk_fields_get(f->chunks[slot], &fields) == MC_WORLD_STORAGE_OK &&
          fields.d == f->worlds[slot] && !fields.q && f->providers[slot].cache->count == 1 &&
          f->providers[slot].cache->insertion_count == 1 &&
          mc_world_store_chunk_count(f->stores[slot]) == 1);
    bool distinct = true;
    for (size_t i = 0; i < 8; ++i) {
        CHECK(mc_world_chunk_slice_at(fields.m, i, &f->slices[slot][i]) == MC_WORLD_STORAGE_OK &&
              f->slices[slot][i] != NULL);
        for (size_t j = 0; j < i; ++j)
            if (f->slices[slot][i] == f->slices[slot][j])
                distinct = false;
    }
    CHECK(distinct);
    f->world_lists[slot] = f->worlds[slot]->fields.b;
    CHECK(f->world_lists[slot] != NULL && f->world_lists[slot]->count == 0);
    f->initial_chunk_bytes[slot] = mc_world_chunk_allocation_bytes(f->chunks[slot]);
    ok = true;
cleanup:
    return ok;
}

static bool files_unchanged(fixture *f, size_t slot)
{
    bool ok = false;
    char *path = NULL;
    mc_ws_file_bytes current = {0};
    CHECK(f->saved[slot].data != NULL && region_path(f, slot, &path));
    CHECK(mc_ws_read_file(path, 1024u * 1024u, &current, NULL) == MC_WORLD_STORAGE_OK &&
          current.length == f->saved[slot].length &&
          memcmp(current.data, f->saved[slot].data, current.length) == 0);
    ok = true;
cleanup:
    mc_ws_free_file_bytes(&current);
    free(path);
    return ok;
}

static bool save_before_publication(fixture *f, size_t slot, const char *root)
{
    bool ok = false, changed = false, completed = false;
    char *path = NULL, *snapshot_path = NULL;
    char leaf[80];
    mc_world_regions *disk = NULL;
    mc_compression_buffer payload = {0};
    mc_nbt_document *document = NULL;
    CHECK(mc_world_runtime_metadata_set_quiet(f->worlds[slot], 1, 1, 1, 0, &changed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          changed);
    CHECK(mc_world_info_time_set(f->worlds[slot]->fields.x, 1000 + (int64_t)slot) ==
          MC_WORLD_INFO_OK);
    int64_t before = 0, after = 0;
    CHECK(mc_platform_current_time_millis(&before) == MC_PLATFORM_MILLIS_OK && before >= 0);
    CHECK(mc_world_runtime_save_chunks(f->worlds[slot], true, &completed) ==
              MC_CHUNK_LIFECYCLE_OK &&
          completed);
    CHECK(mc_platform_current_time_millis(&after) == MC_PLATFORM_MILLIS_OK && after >= before &&
          after / INT64_C(1000) <= UINT32_MAX);
    CHECK(region_path(f, slot, &path) &&
          mc_ws_read_file(path, 1024u * 1024u, &f->saved[slot], NULL) == MC_WORLD_STORAGE_OK &&
          f->saved[slot].length >= MC_REGION_HEADER_BYTES);
    const uint32_t timestamp = be32(f->saved[slot].data + MC_REGION_SECTOR_BYTES);
    CHECK(be32(f->saved[slot].data) != 0 && timestamp >= (uint32_t)(before / INT64_C(1000)) &&
          timestamp <= (uint32_t)(after / INT64_C(1000)) && timestamp != fixture_timestamp &&
          !f->states[slot]->dirty && f->states[slot]->last_save_time == 1000 + (int64_t)slot);
    bool empty_elsewhere = true;
    for (size_t i = 1; i < MC_REGION_ENTRY_COUNT; ++i)
        if (be32(f->saved[slot].data + 4u * i) != 0 ||
            be32(f->saved[slot].data + MC_REGION_SECTOR_BYTES + 4u * i) != 0)
            empty_elsewhere = false;
    CHECK(empty_elsewhere && mc_world_regions_create(NULL, &disk, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_regions_read(disk, mc_world_store_root(f->stores[slot]), 0, 0, &payload, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_nbt_read_compound(payload.data, payload.length, NULL, &document, NULL, NULL) ==
              MC_NBT_OK);
    const mc_nbt_tag *level = field(mc_nbt_document_root(document), "Level");
    const mc_nbt_tag *time = field(level, "LastUpdate");
    const mc_nbt_tag *entities = field(level, "Entities");
    CHECK(time != NULL && time->type == MC_NBT_LONG &&
          time->value.long_value == 1000 + (int64_t)slot && entities != NULL &&
          entities->type == MC_NBT_LIST && entities->value.list.length == 0);
    const int n = snprintf(leaf, sizeof(leaf), "saved-world%zu.region.bin", slot);
    CHECK(n > 0 && (size_t)n < sizeof(leaf) &&
          mc_ws_path_join(root, leaf, &snapshot_path, NULL) == MC_WORLD_STORAGE_OK &&
          mc_ws_write_file(snapshot_path, f->saved[slot].data, f->saved[slot].length, true, NULL) ==
              MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    mc_nbt_document_free(document);
    mc_compression_buffer_free(&payload);
    mc_world_regions_free(disk);
    free(path);
    free(snapshot_path);
    return ok;
}

static bool make_entity(fixture *f, size_t side, mc_world_runtime *world, double x, double y,
                        double z, mc_entity **out)
{
    bool ok = false;
    CHECK(f->entity_count < COUNT(f->entities));
    mc_entity *entity = NULL;
    CHECK(mc_item_entity_allocate(f->entity_classes[side], &entity, NULL) == MC_ENTITY_OK &&
          entity != NULL);
    f->entities[f->entity_count++] = entity;
    CHECK(mc_item_entity_construct(entity, world, x, y, z, f->stack, NULL) == MC_ENTITY_OK);
    /* Actual spawn payload store, not a forged constructor phase or snapshot. */
    CHECK(mc_entity_item_pickup_delay_set(entity, 10) == MC_ENTITY_OK);
    *out = entity;
    ok = true;
cleanup:
    return ok;
}

static bool slice_is(fixture *f, size_t slot, size_t slice, mc_entity *const *expected,
                     size_t count)
{
    mc_world_chunk_fields fields = {0};
    const mc_world_chunk_entity_list *list = NULL;
    size_t actual = SIZE_MAX;
    if (mc_world_chunk_fields_get(f->chunks[slot], &fields) != MC_WORLD_STORAGE_OK ||
        mc_world_chunk_slice_at(fields.m, slice, &list) != MC_WORLD_STORAGE_OK ||
        list != f->slices[slot][slice] ||
        mc_world_chunk_entity_count(list, &actual) != MC_WORLD_STORAGE_OK || actual != count)
        return false;
    for (size_t i = 0; i < count; ++i) {
        mc_entity *entry = NULL;
        if (mc_world_chunk_entity_at(list, i, &entry) != MC_WORLD_STORAGE_OK ||
            entry != expected[i])
            return false;
    }
    return true;
}

static bool world_is(fixture *f, size_t slot, mc_entity *const *expected, size_t count)
{
    mc_world_runtime *w = f->worlds[slot];
    const mc_world_reference_list *list = w->fields.b;
    if (list != f->world_lists[slot] || list->count != count || w->entity_membership.list != list ||
        w->entity_membership.count != count)
        return false;
    for (size_t i = 0; i < count; ++i)
        if (list->items[i] != expected[i] || w->entity_membership.entries[i] != expected[i])
            return false;
    return true;
}

static bool membership(mc_entity *entity, mc_world_runtime *origin, int32_t slice)
{
    mc_entity_snapshot snapshot = {0};
    return mc_entity_get_snapshot(entity, &snapshot) == MC_ENTITY_OK &&
           snapshot.phase == MC_ENTITY_READY && snapshot.base.world == origin && snapshot.base.bF &&
           snapshot.base.bG == 0 && snapshot.base.bH == slice && snapshot.base.bI == 0;
}

static bool payload_is(fixture *f, mc_entity *entity)
{
    mc_entity_snapshot snapshot = {0};
    mc_item_stack *stack = NULL;
    mc_item_stack_fields fields = {0};
    const mc_entity_status got = mc_item_entity_stack_get(entity, &stack);
    const bool same = got == MC_ENTITY_OK && stack == f->stack &&
                      mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
                      fields.id == 262 && fields.count == 5 && fields.damage == 7 &&
                      mc_entity_get_snapshot(entity, &snapshot) == MC_ENTITY_OK &&
                      snapshot.item.c == 10 && snapshot.stack == f->stack;
    mc_item_stack_release(stack);
    return same;
}

static bool publication(fixture *f, size_t side, mc_entity **primary)
{
    bool ok = false, value = true;
    const size_t slot = side, other = side + 2u;
    mc_world_runtime *w = f->worlds[slot];
    mc_world_entity_error error = {0};
    mc_world_chunk_fields fields = {0};
    mc_entity *a = NULL, *b = NULL, *without_world = NULL;
    CHECK(make_entity(f, side, w, 32.5, 16.0, 8.5, &a));
    CHECK(mc_world_runtime_chunk_add_entity(NULL, a, &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_world_runtime_insert_entity(NULL, a, &value, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value);
    CHECK(mc_world_runtime_insert_entity(w, a, &value, &error) == MC_WORLD_STORAGE_OK && !value &&
          a->holds == 1 && world_is(f, slot, NULL, 0) &&
          mc_world_store_chunk_count(f->stores[slot]) == 1 &&
          mc_world_chunk_fields_get(f->chunks[slot], &fields) == MC_WORLD_STORAGE_OK && !fields.q);
    value = true;
    CHECK(mc_world_runtime_insert_entity(w, NULL, &value, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value && error.bytecode == 1 && error.chunk_bytecode == 0 &&
          mc_world_chunk_fields_get(f->chunks[slot], &fields) == MC_WORLD_STORAGE_OK && !fields.q);
    CHECK(mc_world_runtime_chunk_add_entity(f->chunks[slot], NULL, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.chunk_bytecode == 6 &&
          mc_world_chunk_fields_get(f->chunks[slot], &fields) == MC_WORLD_STORAGE_OK && fields.q);
    mc_entity_snapshot before = {0}, after = {0};
    CHECK(mc_entity_get_snapshot(a, &before) == MC_ENTITY_OK && !before.base.bF);
    CHECK(mc_world_runtime_chunk_add_entity(f->chunks[slot], a, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.chunk_bytecode == 45 && a->holds == 1 &&
          mc_entity_get_snapshot(a, &after) == MC_ENTITY_OK && !after.base.bF &&
          after.base.bG == before.base.bG && after.base.bH == before.base.bH &&
          after.base.bI == before.base.bI);
    CHECK(mc_entity_set_position(a, 8.5, -0.5, 8.5) == MC_ENTITY_OK);
    const mc_java_random random = *w->fields.r;
    CHECK(mc_world_runtime_chunk_add_entity(f->chunks[slot], a, &error) == MC_WORLD_STORAGE_OK &&
          membership(a, w, 0));
    mc_entity *aa[] = {a, a};
    mc_entity *one_a[] = {a};
    bool pending_changed = false;
    CHECK(mc_chunk_pending_add(f->providers[slot].pending, mc_chunk_cache_key(0, 0),
                               &pending_changed) == MC_CHUNK_ACQUIRE_OK &&
          pending_changed);
    CHECK(mc_world_runtime_insert_entity(w, a, &value, &error) == MC_WORLD_STORAGE_OK && value &&
          slice_is(f, slot, 0, aa, COUNT(aa)) && world_is(f, slot, one_a, COUNT(one_a)) &&
          a->holds == 4);
    CHECK(mc_chunk_pending_contains(f->providers[slot].pending, mc_chunk_cache_key(0, 0)));
    CHECK(mc_chunk_pending_remove(f->providers[slot].pending, mc_chunk_cache_key(0, 0),
                                  &pending_changed) == MC_CHUNK_ACQUIRE_OK &&
          pending_changed);
    CHECK(make_entity(f, side, w, 8.5, 200.0, 8.5, &b));
    CHECK(mc_world_runtime_insert_entity(w, b, &value, &error) == MC_WORLD_STORAGE_OK && value &&
          membership(b, w, 7));
    mc_entity *one_b[] = {b};
    CHECK(slice_is(f, slot, 7, one_b, COUNT(one_b)) && b->holds == 3);
    CHECK(mc_entity_set_position(a, 8.5, 16.0, 8.5) == MC_ENTITY_OK &&
          mc_world_runtime_insert_entity(w, a, &value, &error) == MC_WORLD_STORAGE_OK && value &&
          membership(a, w, 1));
    CHECK(mc_world_runtime_chunk_add_entity(f->chunks[slot], a, &error) == MC_WORLD_STORAGE_OK &&
          slice_is(f, slot, 1, aa, COUNT(aa)) && slice_is(f, slot, 0, aa, COUNT(aa)) &&
          a->holds == 7);
    CHECK(make_entity(f, side, NULL, 8.5, -32.0, 8.5, &without_world));
    CHECK(mc_world_runtime_insert_entity(w, without_world, &value, &error) == MC_WORLD_STORAGE_OK &&
          value && membership(without_world, NULL, 0) && without_world->holds == 3);
    mc_entity *slice_zero[] = {a, a, without_world};
    mc_entity *world_order[] = {a, b, a, without_world};
    CHECK(slice_is(f, slot, 0, slice_zero, COUNT(slice_zero)) &&
          world_is(f, slot, world_order, COUNT(world_order)) && payload_is(f, a) &&
          payload_is(f, without_world) && mc_test_java_random_equal(w->fields.r, &random));
    CHECK(mc_world_runtime_insert_entity(f->worlds[other], a, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value && membership(a, w, 1) && slice_is(f, other, 1, one_a, COUNT(one_a)) &&
          world_is(f, other, one_a, COUNT(one_a)) && a->holds == 9);
    mc_entity *sentinel = b;
    CHECK(mc_world_chunk_entity_at(f->slices[slot][1], 2, &sentinel) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          sentinel == b);
    CHECK(w->active_calls == 0 && w->local_leases == 0 && f->worlds[other]->active_calls == 0 &&
          f->worlds[other]->local_leases == 0);
    *primary = a;
    ok = true;
cleanup:
    return ok;
}

static bool observer_prefix(fixture *f, size_t side, mc_entity *primary)
{
    bool ok = false;
    const size_t slot = side + 2u;
    mc_world_runtime *w = f->worlds[slot];
    mc_entity *entity = NULL;
    CHECK(make_entity(f, side, w, 8.5, 32.0, 8.5, &entity));
    mc_world_reference_list *const actual_u = w->fields.u;
    mc_world_entity_error error = {0};
    bool value = true;
    /* Explicit native input controls only. No fake Observer type/callback or
     * successful cast is supplied. Actual constructor-created u stays intact. */
    w->fields.u = NULL;
    const mc_world_storage_status null_status =
        mc_world_runtime_insert_entity(w, entity, &value, &error);
    w->fields.u = actual_u;
    CHECK(null_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && error.bytecode == 103 &&
          error.observer_bytecode == 7);
    int nonobserver = 1;
    void *items[] = {&nonobserver};
    mc_world_reference_list input = {.items = items, .count = 1, .capacity = 1};
    w->fields.u = &input;
    const mc_world_storage_status cast_status =
        mc_world_runtime_insert_entity(w, entity, &value, &error);
    w->fields.u = actual_u;
    CHECK(cast_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && error.bytecode == 103 &&
          error.observer_bytecode == 25);
    items[0] = NULL;
    w->fields.u = &input;
    const mc_world_storage_status invoke_status =
        mc_world_runtime_insert_entity(w, entity, &value, &error);
    w->fields.u = actual_u;
    CHECK(invoke_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && error.bytecode == 103 &&
          error.observer_bytecode == 29);
    mc_entity *occurrences[] = {entity, entity, entity};
    mc_entity *world_order[] = {primary, entity, entity, entity};
    CHECK(slice_is(f, slot, 2, occurrences, COUNT(occurrences)) &&
          world_is(f, slot, world_order, COUNT(world_order)) && entity->holds == 7 &&
          membership(entity, w, 2) && payload_is(f, entity) && w->active_calls == 0 &&
          w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool world_limit(fixture *f, size_t side)
{
    bool ok = false, value = true;
    const size_t slot = side + 4u;
    mc_world_runtime *w = f->worlds[slot];
    mc_entity *entity = NULL;
    CHECK(make_entity(f, side, w, 8.5, 16.0, 8.5, &entity));
    mc_world_entity_error error = {0};
    const size_t bytes = w->construction.direct_allocation_bytes;
    CHECK(bytes == w->construction_limits.max_direct_allocation_bytes);
    CHECK(mc_world_runtime_insert_entity(w, entity, &value, &error) == MC_WORLD_STORAGE_LIMIT &&
          value && error.bytecode == 95 && error.chunk_bytecode == 0 && membership(entity, w, 1) &&
          entity->holds == 2 && w->construction.direct_allocation_bytes == bytes &&
          world_is(f, slot, NULL, 0));
    mc_entity *one[] = {entity};
    CHECK(slice_is(f, slot, 1, one, COUNT(one)) &&
          mc_world_chunk_allocation_bytes(f->chunks[slot]) > f->initial_chunk_bytes[slot] &&
          w->active_calls == 0 && w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool chunk_limit(fixture *f, size_t side)
{
    bool ok = false;
    const size_t slot = side + 6u;
    mc_world_runtime *w = f->worlds[slot];
    mc_entity *entity = NULL;
    CHECK(make_entity(f, side, w, 8.5, 16.0, 8.5, &entity));
    size_t appended = 0;
    mc_world_entity_error error = {0};
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    /* Genuine duplicate appends exhaust the immutable actual Store-supplied
     * Chunk budget. Bound is test work admission, not a runtime/game quota.
     * No private limit, capacity, count, hold or allocation result is changed. */
    for (; appended < 65536u; ++appended) {
        status = mc_world_runtime_chunk_add_entity(f->chunks[slot], entity, &error);
        if (status != MC_WORLD_STORAGE_OK)
            break;
    }
    size_t count = SIZE_MAX;
    CHECK(status == MC_WORLD_STORAGE_LIMIT && error.chunk_bytecode == 148 && appended >= 4 &&
          appended < 65536u &&
          mc_world_chunk_entity_count(f->slices[slot][1], &count) == MC_WORLD_STORAGE_OK &&
          count == appended && membership(entity, w, 1) && entity->holds == appended + 1u &&
          world_is(f, slot, NULL, 0));
    bool same = true;
    for (size_t i = 0; i < count; ++i) {
        mc_entity *entry = NULL;
        if (mc_world_chunk_entity_at(f->slices[slot][1], i, &entry) != MC_WORLD_STORAGE_OK ||
            entry != entity)
            same = false;
    }
    const size_t bytes = mc_world_chunk_allocation_bytes(f->chunks[slot]);
    CHECK(same && bytes > f->initial_chunk_bytes[slot] && bytes <= 256u * 1024u &&
          mc_world_runtime_chunk_add_entity(f->chunks[slot], entity, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          error.chunk_bytecode == 148 && entity->holds == appended + 1u &&
          mc_world_chunk_allocation_bytes(f->chunks[slot]) == bytes &&
          mc_world_chunk_entity_count(f->slices[slot][1], &count) == MC_WORLD_STORAGE_OK &&
          count == appended && w->active_calls == 0 && w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool opaque_and_failed_saves(fixture *f, size_t slot)
{
    bool ok = false, completed = true;
    mc_world_storage_error error = {0};
    uint32_t growth = UINT32_MAX;
    CHECK(mc_world_store_write_runtime_chunk(f->stores[slot], f->states[slot], 2000, 12345, &growth,
                                             &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          growth == UINT32_MAX && f->states[slot]->has_entities && files_unchanged(f, slot));
    CHECK(mc_world_store_write_chunk(f->stores[slot], f->states[slot], 2000, 12345, &growth,
                                     &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          growth == UINT32_MAX && f->states[slot]->has_entities && files_unchanged(f, slot));
    /* Source force-save still skips a clean q Chunk at its last saved time.
     * Advance the real WorldInfo time to reach the actual writer callback. */
    CHECK(!mc_chunk_save_eligible(f->states[slot], 1000 + (int64_t)slot, true) &&
          mc_world_info_time_set(f->worlds[slot]->fields.x, 2000) == MC_WORLD_INFO_OK &&
          mc_chunk_save_eligible(f->states[slot], 2000, true));
    mc_entity *first = NULL;
    size_t selected_slice = SIZE_MAX;
    for (size_t slice = 0; slice < COUNT(f->slices[slot]); ++slice) {
        size_t count = 0;
        CHECK(mc_world_chunk_entity_count(f->slices[slot][slice], &count) == MC_WORLD_STORAGE_OK);
        if (count != 0) {
            CHECK(mc_world_chunk_entity_at(f->slices[slot][slice], 0, &first) ==
                  MC_WORLD_STORAGE_OK);
            selected_slice = slice;
            break;
        }
    }
    CHECK(first != NULL);
    /* Generic opaque entry points above keep their refusal. The live GAME
     * path now has a real serializer: select its first actual occurrence and
     * supply a late NULL Stack receiver. Restore before assertions/teardown;
     * the real owning Stack hold remains live throughout the control. */
    mc_item_stack *const stack = first->stack;
    first->stack = NULL;
    mc_world_runtime_lifecycle_error detail;
    const mc_chunk_lifecycle_status saved =
        mc_world_runtime_save_chunks_detailed(f->worlds[slot], true, &completed, &detail);
    first->stack = stack;
    CHECK(saved == MC_CHUNK_LIFECYCLE_CALLBACK_FAILED && detail.save.serializer_bytecode == 183 &&
          detail.save.entity_slice == selected_slice && detail.save.entity_index == 0 &&
          detail.save.entity.status == MC_ENTITY_NBT_NULL_REFERENCE &&
          detail.save.entity.wrapper_bytecode == 27 && detail.save.entity.item_bytecode == 37 &&
          completed && f->states[slot]->has_entities && files_unchanged(f, slot));
    ok = true;
cleanup:
    return ok;
}

static bool lease_blocks_retirement(fixture *f, size_t side, mc_entity *primary)
{
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_world_runtime_construction before = {0}, after = {0};
    size_t count = SIZE_MAX, later = SIZE_MAX;
    const size_t holds = primary->holds;
    const size_t world_count = w->fields.b->count;
    void **const world_items = w->fields.b->items;
    const size_t bytes = mc_world_chunk_allocation_bytes(f->chunks[side]);
    CHECK(mc_world_runtime_construction_get(w, &before) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_entity_count(f->slices[side][0], &count) == MC_WORLD_STORAGE_OK &&
          mc_world_store_lease_chunk(f->stores[side], f->states[side], NULL) ==
              MC_WORLD_STORAGE_OK);
    f->leased[side] = true;
    const mc_world_storage_status status = mc_world_runtime_retire_entity_containers(w, NULL);
    const mc_world_storage_status released =
        mc_world_store_release_chunk(f->stores[side], f->states[side], NULL);
    if (released == MC_WORLD_STORAGE_OK)
        f->leased[side] = false;
    CHECK(status == MC_WORLD_STORAGE_BUSY && released == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_construction_get(w, &after) == MC_WORLD_STORAGE_OK &&
          before.phase == MC_WORLD_RUNTIME_READY && after.phase == before.phase &&
          !w->entity_containers_retired && primary->holds == holds &&
          w->fields.b->count == world_count && w->fields.b->items == world_items &&
          mc_world_chunk_entity_count(f->slices[side][0], &later) == MC_WORLD_STORAGE_OK &&
          later == count && mc_world_chunk_allocation_bytes(f->chunks[side]) == bytes &&
          files_unchanged(f, side));
    ok = true;
cleanup:
    return ok;
}

/* Explicit native retirement of the real cache/pending descriptors. This is
 * legal only AFTER terminal container teardown, never source Chunk unload.
 * It invokes no save/detach/Entity callback and never clears state.active. */
static bool retire_descriptors(fixture *f, size_t slot)
{
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

static bool terminal_retirement(fixture *f, mc_entity *const primary[2])
{
    bool ok = false;
    mc_entity_snapshot before[ENTITY_COUNT] = {0};
    for (size_t i = 0; i < f->entity_count; ++i)
        CHECK(mc_entity_get_snapshot(f->entities[i], &before[i]) == MC_ENTITY_OK);
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot) {
        mc_world_runtime *w = f->worlds[slot];
        CHECK(mc_world_runtime_retire_entity_containers(w, NULL) == MC_WORLD_STORAGE_OK);
        f->terminal[slot] = true;
        mc_world_chunk_fields fields = {0};
        CHECK(w->entity_containers_retired &&
              mc_world_chunk_fields_get(f->chunks[slot], &fields) == MC_WORLD_STORAGE_OK &&
              fields.q && fields.c && w->fields.b == f->world_lists[slot] &&
              w->fields.b->count == 0 && w->entity_membership.count == 0 &&
              mc_world_chunk_allocation_bytes(f->chunks[slot]) == f->initial_chunk_bytes[slot]);
        bool empty = true;
        for (size_t slice = 0; slice < 8; ++slice)
            if (!slice_is(f, slot, slice, NULL, 0))
                empty = false;
        CHECK(empty);
        bool value = true, completed = true;
        int32_t id = 123;
        mc_world_entity_error error = {0};
        mc_entity *const entity = primary[slot % 2u];
        CHECK(mc_world_runtime_insert_entity(w, entity, &value, &error) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              value && error.bytecode == 0);
        CHECK(mc_world_runtime_chunk_add_entity(f->chunks[slot], NULL, &error) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              error.chunk_bytecode == 0);
        CHECK(mc_world_runtime_block_get(w, 1, 1, 1, &id, NULL) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              id == 123);
        CHECK(mc_world_runtime_save_chunks(w, true, &completed) != MC_CHUNK_LIFECYCLE_OK &&
              completed);
        uint32_t growth = UINT32_MAX;
        CHECK(mc_world_store_write_runtime_chunk(f->stores[slot], f->states[slot], 3000, 54321,
                                                 &growth,
                                                 NULL) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              growth == UINT32_MAX);
        CHECK(mc_world_store_write_chunk(f->stores[slot], f->states[slot], 3000, 54321, &growth,
                                         NULL) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              growth == UINT32_MAX && files_unchanged(f, slot));
        CHECK(retire_descriptors(f, slot) && f->states[slot]->active);
        CHECK(mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_OK);
        f->world_retired[slot] = true;
        mc_world_runtime_construction construction = {0};
        CHECK(mc_world_runtime_construction_get(w, &construction) == MC_WORLD_STORAGE_OK &&
              construction.phase == MC_WORLD_RUNTIME_RETIRED && f->states[slot]->active);
        /* Only the new terminal-marker exception may retire this still-active
         * owner, after all real cache references and leases are gone. */
        CHECK(mc_world_store_retire_chunk(f->stores[slot], f->states[slot], NULL) ==
              MC_WORLD_STORAGE_OK);
        f->states[slot] = NULL;
        f->chunks[slot] = NULL;
        CHECK(mc_world_store_chunk_count(f->stores[slot]) == 0 && files_unchanged(f, slot));
        CHECK(mc_world_runtime_free(w, NULL) == MC_WORLD_STORAGE_BUSY);
    }
    for (size_t i = 0; i < f->entity_count; ++i) {
        mc_entity_snapshot after = {0};
        CHECK(f->entities[i]->holds == 1 &&
              mc_entity_get_snapshot(f->entities[i], &after) == MC_ENTITY_OK &&
              after.phase == before[i].phase && after.base.world == before[i].base.world &&
              after.base.bF == before[i].base.bF && after.base.bG == before[i].base.bG &&
              after.base.bH == before[i].base.bH && after.base.bI == before[i].base.bI &&
              after.item.c == before[i].item.c && after.stack == before[i].stack);
    }
    ok = true;
cleanup:
    return ok;
}

static bool cleanup_fixture(fixture *f)
{
    if (f == NULL)
        return true;
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        if (f->leased[slot]) {
            if (mc_world_store_release_chunk(f->stores[slot], f->states[slot], NULL) !=
                MC_WORLD_STORAGE_OK)
                return false;
            f->leased[slot] = false;
        }
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
        if (!f->world_retired[slot]) {
            if (mc_world_runtime_retire_world(f->worlds[slot], NULL) != MC_WORLD_STORAGE_OK)
                return false;
            f->world_retired[slot] = true;
        }
        if (f->states[slot] != NULL) {
            if (mc_world_store_retire_chunk(f->stores[slot], f->states[slot], NULL) !=
                MC_WORLD_STORAGE_OK)
                return false;
            f->states[slot] = NULL;
            f->chunks[slot] = NULL;
        }
    }
    for (size_t i = 0; i < f->entity_count; ++i) {
        mc_entity_release(f->entities[i]);
        f->entities[i] = NULL;
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot) {
        if (f->worlds[slot] != NULL &&
            mc_world_runtime_free(f->worlds[slot], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        mc_world_dimension_release(f->dimensions[slot]);
    }
    for (size_t side = 0; side < 2; ++side) {
        if (mc_world_runtime_classes_free(f->classes[side], NULL) != MC_WORLD_STORAGE_OK ||
            mc_registry_classes_free(f->registry[side], NULL) != MC_REGISTRY_OK ||
            mc_entity_classes_free(f->entity_classes[side]) != MC_ENTITY_OK)
            return false;
    }
    mc_item_stack_release(f->stack);
    mc_world_info_name_release(f->name);
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot) {
        mc_ws_free_file_bytes(&f->saved[slot]);
        mc_world_store_free(f->stores[slot]);
    }
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    return true;
}

int main(int argc, char **argv)
{
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'P', 'u', 'b', 'l', 'i', 's', 'h'};
    if (argc != 3 || strcmp(argv[1], "world-entity-publication") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = (stream){.bytes = language};
        r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK);
        CHECK(mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
              MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              r->opens == 3 && r->closes == 1 && !r->bad);
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(open_store(f, slot, root) && create_world(f, slot));
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(save_before_publication(f, slot, root));
    mc_entity *primary[2] = {0};
    for (size_t side = 0; side < 2; ++side) {
        CHECK(publication(f, side, &primary[side]));
        CHECK(observer_prefix(f, side, primary[side]));
        CHECK(world_limit(f, side));
        CHECK(chunk_limit(f, side));
        CHECK(lease_blocks_retirement(f, side, primary[side]));
    }
    for (size_t slot = 0; slot < WORLD_COUNT; ++slot)
        CHECK(opaque_and_failed_saves(f, slot));
    CHECK(terminal_retirement(f, primary));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("publication cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u World Entity publication checks (world-entity-publication)\n", checks) < 0)
        result = 1;
    return result;
}
