#include "mc/block/registry/runtime.h"
#include "mc/resources/translation/translation.h"
#include "mc/storage/region/region.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/presentation.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/platform/time/millis.h"
#include "../../../src/world/runtime/provider_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/storage/files.h"
#include "../../support/java_random.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        const bool passed = !!(condition);                                                         \
        if (!passed) {                                                                             \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Constructed native contracts from fd.h/dj.e, not an original observation.
 * Genuine GAME constructors, real class initialization and loaded populated
 * known-empty records supply every owner. Explicit controls temporarily change
 * only actual m references/elements, Block.bA, slab.b and one loaded block byte.
 * They never forge phases, cache counts, RNG/profile or collection state. */
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
    mc_registry_classes *registry;
    mc_world_runtime *world;
    mc_world_runtime_classes *classes;
    const mc_chunk_cache *cache;
    size_t opens, closes, guarded;
    bool fail_first, bad;
} resources;

typedef struct child {
    mc_world_store *store;
    mc_chunk_state *state;
} child;

typedef struct fixture {
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_world_regions *regions;
    mc_world_store *stores[6];
    mc_world_info_name *name;
    resources sources[4];
    mc_registry_classes *registry[4];
    mc_world_runtime_classes *classes[5];
    mc_world_runtime *worlds[7];
    mc_world_dimension *dimensions[6];
    mc_world_provider_fields providers[6];
    bool bound[6];
    child children[32];
    mc_chunk_state *center[2];
    mc_ws_file_bytes seed[2];
} fixture;

typedef struct fixture_record {
    mc_nbt_tag root, level, fields[11];
    const mc_nbt_tag *root_fields[1], *level_fields[11];
    uint16_t names[12][20];
    uint8_t arrays[5][MC_CHUNK_BLOCKS];
} fixture_record;

static size_t cell(void)
{
    return ((size_t)8 << 11u) | ((size_t)8 << 7u) | 64u;
}

static bool same_cause(const mc_world_storage_error *a, const mc_world_storage_error *b)
{
    return a->status == b->status && a->operation == b->operation &&
           a->system_error == b->system_error;
}

static bool same_registry(const mc_registry_error *a, const mc_registry_error *b)
{
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

static bool fresh_registry(const mc_registry_error *error, mc_registry_status status)
{
    const mc_registry_error expected = {.status = status,
                                        .message = error->message,
                                        .initializer_pc = -1,
                                        .block_id = -1,
                                        .constructor_pc = -1,
                                        .active_initializer_pc = -1,
                                        .item_argument = -1};
    return (status == MC_REGISTRY_OK ? error->message == NULL : error->message != NULL) &&
           same_registry(error, &expected);
}

static bool remember(fixture *f, size_t slot)
{
    if (!f->bound[slot])
        return true;
    const mc_chunk_cache *cache = f->providers[slot].cache;
    if (!mc_chunk_cache_validate(cache))
        return false;
    for (size_t j = 0; j < cache->insertion_count; ++j) {
        mc_chunk_state *state = cache->insertions[j];
        if (state == NULL || !mc_world_store_owns_chunk(f->stores[slot], state))
            return false;
        size_t i = 0;
        while (i < COUNT(f->children) && f->children[i].state != NULL &&
               f->children[i].state != state)
            ++i;
        if (i == COUNT(f->children))
            return false;
        if (f->children[i].state != NULL && f->children[i].store != f->stores[slot])
            return false;
        f->children[i] = (child){f->stores[slot], state};
    }
    return true;
}

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
    bool value = true;
    mc_world_solid_support_error nested;
    mc_world_mutation_error presentation;
    if (r->world == NULL || r->world->active_calls == 0 || r->cache == NULL ||
        r->cache->count != 0 || r->cache->insertion_count != 0 ||
        mc_world_runtime_retire_world(r->world, NULL) != MC_WORLD_STORAGE_BUSY ||
        mc_world_runtime_free(r->world, NULL) != MC_WORLD_STORAGE_BUSY ||
        mc_world_runtime_classes_free(r->classes, NULL) != MC_WORLD_STORAGE_BUSY ||
        mc_registry_classes_free(r->registry, NULL) != MC_REGISTRY_BUSY ||
        mc_world_runtime_sound(r->world, 0.0, -0.0, 1.0, NULL, -2.0f, 0.0f, &presentation) !=
            MC_WORLD_STORAGE_OK ||
        mc_world_runtime_particle(r->world, NULL, 0.0, 1.0, -1.0, 1.0e200, -1.0e200, 0.0,
                                  &presentation) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_solid_support_get(r->world, 8, -1, 8, &value, &nested) !=
            MC_WORLD_STORAGE_BUSY ||
        !value || nested.bytecode != 0 || nested.registry.status != MC_REGISTRY_BUSY ||
        r->world->active_calls != 1)
        r->bad = true;
    ++r->guarded;
    if (index == 0 && r->fail_first)
        return MC_REGISTRY_NULL_REFERENCE;
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

static bool open_store(fixture *f, size_t slot, const char *root)
{
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    uint16_t title[] = {'S', 'u', 'p'};
    const int n = snprintf(path, sizeof(path), "%s/world%zu", root, slot);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 127,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = slot < 4u ? 4u : 0u;
    CHECK(mc_world_store_open(path, f->regions, &options, &f->stores[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_level_info info = {.seed = 23,
                                .spawn_y = 64,
                                .time = 0,
                                .level_name = {title, COUNT(title)},
                                .version = 19132};
    /* Native fixture metadata, not a supplied GAME save clock. It makes both
     * ordinary client/server load-or-create entries take the real loaded path. */
    CHECK(mc_world_store_save_info(f->stores[slot], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool create_world(fixture *f, size_t slot)
{
    bool ok = false;
    const size_t class_index = slot < 4u ? slot : slot - 4u;
    const bool server = slot % 2u != 0;
    mc_world_runtime_construction state;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
    CHECK(mc_world_dimension_create(0, NULL, &f->dimensions[slot], NULL) == MC_WORLD_DIMENSION_OK);
    CHECK(mc_world_runtime_allocate_world(server ? MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE
                                                 : MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE,
                                          f->classes[class_index], &limits, &f->worlds[slot],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[slot], .name = f->name, .seed = -99, .dimension = f->dimensions[slot]};
    CHECK(mc_world_runtime_construct_world(f->worlds[slot], &arguments, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->worlds[slot], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY && state.contexts_bound && state.cache_registered);
    CHECK(mc_world_provider_bind_fields(f->worlds[slot]->fields.v, &f->providers[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[slot] = true;
    CHECK(f->providers[slot].cache->count == 0 && f->providers[slot].cache->insertion_count == 0 &&
          mc_world_runtime_registry(f->worlds[slot]) == f->registry[class_index]);
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
    r->arrays[0][cell()] = 1;
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
    r->root.value.compound.length = COUNT(r->root_fields);
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

static bool region_path(fixture *f, size_t slot, char **out)
{
    return mc_ws_path_join(mc_world_store_root(f->stores[slot]), "region/r.0.0.mcr", out, NULL) ==
           MC_WORLD_STORAGE_OK;
}

static bool retain_seed(fixture *f, const char *root, size_t slot)
{
    bool ok = false;
    char *path = NULL, *copy = NULL;
    char name[64];
    const size_t index = slot - 4u;
    const int n = snprintf(name, sizeof(name), "seed-world%zu.region.bin", slot);
    CHECK(index < COUNT(f->seed) && n > 0 && (size_t)n < sizeof(name));
    CHECK(region_path(f, slot, &path));
    CHECK(mc_ws_read_file(path, 1024u * 1024u, &f->seed[index], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_path_join(root, name, &copy, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_write_file(copy, f->seed[index].data, f->seed[index].length, true, NULL) ==
          MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(path);
    free(copy);
    return ok;
}

static bool cold_and_admission(fixture *f)
{
    bool ok = false;
    mc_world_solid_support_error error;
    bool value = true;
    CHECK(mc_world_runtime_solid_support_get(NULL, 0, 0, 0, &value, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value && error.bytecode == 0 && fresh_registry(&error.registry, MC_REGISTRY_OK));
    for (size_t i = 0; i < 4; ++i) {
        mc_registry_block_view view;
        mc_world_runtime *w = f->worlds[i];
        CHECK(mc_registry_classes_inspect_blocks(f->registry[i], &view, NULL) == MC_REGISTRY_OK &&
              view.phase == MC_REGISTRY_COLD && f->sources[i].opens == 0);
        const mc_java_random random = *w->fields.r;
        const uint64_t u = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
        mc_world_mutation_error presentation;
        CHECK(mc_world_runtime_sound(w, 0.0, -0.0, 1.0, NULL, -2.0f, 0.0f, &presentation) ==
                  MC_WORLD_STORAGE_OK &&
              presentation.status == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_particle(w, NULL, 0.0, 1.0, -1.0, 1.0e200, -1.0e200, 0.0, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_test_java_random_equal(w->fields.r, &random) &&
              atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed) == u &&
              mc_registry_classes_inspect_blocks(f->registry[i], &view, NULL) == MC_REGISTRY_OK &&
              view.phase == MC_REGISTRY_COLD && f->sources[i].opens == 0 && w->active_calls == 0);
        const mc_world_storage_error before = *mc_world_runtime_last_error(w);
        CHECK(mc_world_runtime_solid_support_get(w, 8, 64, 8, NULL, &error) ==
                  MC_WORLD_STORAGE_INVALID_ARGUMENT &&
              error.bytecode == 0 && fresh_registry(&error.registry, MC_REGISTRY_OK) &&
              same_cause(&before, mc_world_runtime_last_error(w)) && f->sources[i].opens == 0);
        f->sources[i].world = w;
        f->sources[i].cache = f->providers[i].cache;
        value = true;
        const mc_world_storage_status status =
            mc_world_runtime_solid_support_get(w, 8, i < 2 ? -1 : 64, 8, &value, &error);
        CHECK(mc_registry_classes_inspect_blocks(f->registry[i], &view, NULL) == MC_REGISTRY_OK);
        if (i < 2) {
            CHECK(status == MC_WORLD_STORAGE_OK && !value && error.status == MC_WORLD_STORAGE_OK &&
                  error.bytecode == 0 && view.phase == MC_REGISTRY_READY &&
                  f->sources[i].opens == 3 && f->sources[i].closes == 1 &&
                  f->sources[i].guarded == 3);
        } else {
            CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && error.bytecode == 0 &&
                  view.phase == MC_REGISTRY_FAILED && error.registry.status != MC_REGISTRY_OK &&
                  same_registry(&error.registry, mc_world_runtime_last_registry_error(w)) &&
                  same_cause(&error.cause, mc_world_runtime_last_error(w)) &&
                  f->sources[i].opens == 1 && f->sources[i].closes == 0 &&
                  f->sources[i].guarded == 1);
            value = false;
            CHECK(mc_world_runtime_solid_support_get(w, INT32_MAX, -1, INT32_MAX, &value, &error) ==
                      MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  !value && error.bytecode == 0 &&
                  same_registry(&error.registry, mc_world_runtime_last_registry_error(w)) &&
                  f->sources[i].opens == 1);
        }
        CHECK(!f->sources[i].bad && f->providers[i].cache->count == 0 &&
              f->providers[i].cache->insertion_count == 0 &&
              mc_world_store_chunk_count(f->stores[i]) == 0 && w->active_calls == 0);
    }
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[4],
                                          NULL, &f->worlds[6], NULL) == MC_WORLD_STORAGE_OK);
    value = true;
    CHECK(mc_world_runtime_solid_support_get(f->worlds[6], 0, -1, 0, &value, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value && error.bytecode == 0 &&
          fresh_registry(&error.registry, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY));
    ok = true;
cleanup:
    return ok;
}

static bool late_array(fixture *f, size_t side)
{
    bool ok = false, value = true;
    mc_registry_classes *r = f->registry[side];
    mc_world_runtime *w = f->worlds[side];
    mc_world_solid_support_error error;
    mc_block **const saved = r->blocks.m;
    r->blocks.m = NULL;
    const mc_world_storage_status status =
        mc_world_runtime_solid_support_get(w, 8, 64, 8, &value, &error);
    r->blocks.m = saved;
    CHECK(remember(f, side));
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value && error.bytecode == 10 &&
          fresh_registry(&error.registry, MC_REGISTRY_NULL_REFERENCE) &&
          same_registry(&error.registry, mc_world_runtime_last_registry_error(w)) &&
          same_cause(&error.cause, mc_world_runtime_last_error(w)));
    CHECK(f->providers[side].cache->count == 1 && f->providers[side].cache->insertion_count == 1 &&
          mc_world_store_chunk_count(f->stores[side]) == 1);
    f->center[side] = f->providers[side].cache->insertions[0];
    CHECK(f->center[side]->active && f->center[side]->terrain_populated &&
          !f->center[side]->dirty && f->center[side]->x == 0 && f->center[side]->z == 0 &&
          f->center[side]->data->blocks[cell()] == 1);

    value = false;
    r->blocks.m = NULL;
    const mc_world_storage_status outside =
        mc_world_runtime_solid_support_get(w, 8, -1, 8, &value, &error);
    r->blocks.m = saved;
    CHECK(outside == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !value && error.bytecode == 10 &&
          error.registry.status == MC_REGISTRY_NULL_REFERENCE);

    mc_world_runtime *limited = f->worlds[side + 4u];
    value = true;
    r->blocks.m = NULL;
    const mc_world_storage_status limit =
        mc_world_runtime_solid_support_get(limited, 8, 64, 8, &value, &error);
    r->blocks.m = saved;
    CHECK(remember(f, side + 4u));
    CHECK(limit == MC_WORLD_STORAGE_LIMIT && value && error.bytecode == 7 &&
          fresh_registry(&error.registry, MC_REGISTRY_OK) &&
          same_cause(&error.cause, mc_world_runtime_last_error(limited)) &&
          f->providers[side + 4u].cache->count == 0 &&
          f->providers[side + 4u].cache->insertion_count == 0 &&
          mc_world_store_chunk_count(f->stores[side + 4u]) == 0);
    ok = true;
cleanup:
    return ok;
}

static mc_block *find_kind(mc_registry_classes *registry, mc_block_kind kind)
{
    for (size_t i = 0; i < MC_REGISTRY_BLOCK_SLOTS; ++i)
        if (registry->blocks.m[i] != NULL && registry->blocks.m[i]->kind == kind)
            return registry->blocks.m[i];
    return NULL;
}

static bool selected_predicates(fixture *f, size_t side)
{
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    mc_chunk_state *chunk = f->center[side];
    const mc_java_random random = *w->fields.r;
    const uint64_t u = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
    mc_world_solid_support_error error;
    mc_block *const stone = r->blocks.m[1];
    mc_block *const slab = find_kind(r, MC_BLOCK_YS_PW);
    mc_block *const flower = find_kind(r, MC_BLOCK_WB_NW);
    mc_block *const glass = find_kind(r, MC_BLOCK_FK_DO);
    mc_block *const leaves = find_kind(r, MC_BLOCK_BK_AR);
    CHECK(stone != NULL && slab != NULL && flower != NULL && glass != NULL && leaves != NULL);
    bool value = true;
    mc_block *const saved_element = r->blocks.m[1];
    r->blocks.m[1] = NULL;
    const mc_world_storage_status null_block =
        mc_world_runtime_solid_support_get(w, 8, 64, 8, &value, &error);
    r->blocks.m[1] = saved_element;
    CHECK(null_block == MC_WORLD_STORAGE_OK && !value && error.status == MC_WORLD_STORAGE_OK);

    mc_material *const saved_material = stone->fields.bA;
    value = false;
    stone->fields.bA = NULL;
    const mc_world_storage_status null_material =
        mc_world_runtime_solid_support_get(w, 8, 64, 8, &value, &error);
    stone->fields.bA = saved_material;
    CHECK(null_material == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !value && error.bytecode == 25 &&
          fresh_registry(&error.registry, MC_REGISTRY_NULL_REFERENCE) &&
          same_cause(&error.cause, mc_world_runtime_last_error(w)));
    const mc_world_storage_error old_world = *mc_world_runtime_last_error(w);
    const mc_registry_error old_registry = *mc_world_runtime_last_registry_error(w);
    value = false;
    CHECK(mc_world_runtime_solid_support_get(w, 8, 64, 8, &value, &error) == MC_WORLD_STORAGE_OK &&
          value && error.status == MC_WORLD_STORAGE_OK && error.bytecode == 0 &&
          fresh_registry(&error.registry, MC_REGISTRY_OK) &&
          same_cause(&old_world, mc_world_runtime_last_error(w)) &&
          same_registry(&old_registry, mc_world_runtime_last_registry_error(w)));

    /* Expected bodies are paired primary d/b contracts, not opacity or a
     * final-ID table: flower false, glass/leaves inherit base true. */
    mc_block *const blocks[] = {flower, glass, leaves};
    const bool expected[] = {false, true, true};
    for (size_t i = 0; i < COUNT(blocks); ++i) {
        const int32_t id = blocks[i]->fields.bn;
        CHECK(id >= 0 && id < 256);
        mc_material *const material = blocks[i]->fields.bA;
        blocks[i]->fields.bA = saved_material;
        chunk->data->blocks[cell()] = (uint8_t)id;
        value = !expected[i];
        const mc_world_storage_status status =
            mc_world_runtime_solid_support_get(w, 8, 64, 8, &value, &error);
        chunk->data->blocks[cell()] = 1;
        blocks[i]->fields.bA = material;
        CHECK(status == MC_WORLD_STORAGE_OK && value == expected[i]);
    }
    const int32_t slab_id = slab->fields.bn;
    CHECK(slab_id >= 0 && slab_id < 256);
    const bool old_b = slab->subtype.ys.b;
    mc_material *const slab_material = slab->fields.bA;
    for (unsigned int b = 0; b < 2; ++b) {
        slab->subtype.ys.b = b != 0;
        slab->fields.bA = saved_material;
        chunk->data->blocks[cell()] = (uint8_t)slab_id;
        value = b == 0;
        const mc_world_storage_status status =
            mc_world_runtime_solid_support_get(w, 8, 64, 8, &value, &error);
        chunk->data->blocks[cell()] = 1;
        slab->fields.bA = slab_material;
        slab->subtype.ys.b = old_b;
        CHECK(status == MC_WORLD_STORAGE_OK && value == (b != 0));
        slab->subtype.ys.b = b != 0;
        slab->fields.bA = mc_materials_get(f->materials, MC_MATERIAL_a);
        chunk->data->blocks[cell()] = (uint8_t)slab_id;
        value = true;
        const mc_world_storage_status material_false =
            mc_world_runtime_solid_support_get(w, 8, 64, 8, &value, &error);
        chunk->data->blocks[cell()] = 1;
        slab->fields.bA = slab_material;
        slab->subtype.ys.b = old_b;
        CHECK(material_false == MC_WORLD_STORAGE_OK && !value);
    }
    value = true;
    CHECK(mc_world_runtime_solid_support_get(w, INT32_MAX, 64, 0, &value, NULL) ==
              MC_WORLD_STORAGE_OK &&
          !value);
    value = true;
    CHECK(mc_world_runtime_solid_support_get(w, 8, 128, 8, &value, NULL) == MC_WORLD_STORAGE_OK &&
          !value && mc_test_java_random_equal(w->fields.r, &random) &&
          atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed) == u &&
          w->active_calls == 0 && f->providers[side].cache->count == 1);
    ok = true;
cleanup:
    return ok;
}

static bool drain(fixture *f, size_t slot)
{
    if (!f->bound[slot])
        return true;
    mc_chunk_cache *cache = f->providers[slot].cache;
    mc_chunk_pending *pending = f->providers[slot].pending;
    if (!remember(f, slot) || !mc_chunk_pending_validate(pending))
        return false;
    for (size_t i = 0; i < cache->count; ++i) {
        bool changed = false;
        if (mc_chunk_pending_add(pending, cache->entries[i].key, &changed) != MC_CHUNK_ACQUIRE_OK)
            return false;
    }
    while (pending->count != 0) {
        const size_t before = pending->count;
        bool more = true;
        if (mc_world_runtime_maintain(f->worlds[slot], &more) != MC_CHUNK_LIFECYCLE_OK || more ||
            pending->count >= before)
            return false;
    }
    return cache->count == 0 && cache->insertion_count == 0;
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

static bool check_disk(fixture *f, size_t slot, int64_t time, uint32_t low, uint32_t high)
{
    bool ok = false;
    char *path = NULL;
    mc_ws_file_bytes bytes = {0};
    mc_world_regions *disk = NULL;
    mc_compression_buffer payload = {0};
    mc_nbt_document *document = NULL;
    CHECK(region_path(f, slot, &path));
    CHECK(mc_ws_read_file(path, 1024u * 1024u, &bytes, NULL) == MC_WORLD_STORAGE_OK &&
          bytes.length >= MC_REGION_HEADER_BYTES && be32(bytes.data) != 0);
    const uint32_t timestamp = be32(bytes.data + MC_REGION_SECTOR_BYTES);
    for (size_t i = 1; i < MC_REGION_ENTRY_COUNT; ++i)
        CHECK(be32(bytes.data + 4u * i) == 0 &&
              be32(bytes.data + MC_REGION_SECTOR_BYTES + 4u * i) == 0);
    if (slot < 2) {
        CHECK(timestamp >= low && timestamp <= high && timestamp != fixture_timestamp);
    } else {
        CHECK(timestamp == fixture_timestamp && slot >= 4 &&
              bytes.length == f->seed[slot - 4u].length &&
              memcmp(bytes.data, f->seed[slot - 4u].data, bytes.length) == 0);
    }
    /* A fresh independent region owner forces actual disk reload instead of
     * reading the writer's retained image. No shared cache is cleared. */
    CHECK(mc_world_regions_create(NULL, &disk, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_regions_read(disk, mc_world_store_root(f->stores[slot]), 0, 0, &payload, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_nbt_read_compound(payload.data, payload.length, NULL, &document, NULL, NULL) ==
          MC_NBT_OK);
    const mc_nbt_tag *level = field(mc_nbt_document_root(document), "Level");
    const mc_nbt_tag *last = field(level, "LastUpdate");
    const mc_nbt_tag *blocks = field(level, "Blocks");
    CHECK(last != NULL && last->type == MC_NBT_LONG && last->value.long_value == time &&
          blocks != NULL && blocks->type == MC_NBT_BYTE_ARRAY &&
          blocks->value.byte_array.length == MC_CHUNK_BLOCKS &&
          blocks->value.byte_array.data[cell()] == 1);
    ok = true;
cleanup:
    mc_nbt_document_free(document);
    mc_compression_buffer_free(&payload);
    mc_world_regions_free(disk);
    mc_ws_free_file_bytes(&bytes);
    free(path);
    return ok;
}

static bool maintained_and_retired(fixture *f)
{
    bool ok = false;
    for (size_t side = 0; side < 2; ++side) {
        mc_world_runtime *w = f->worlds[side];
        bool changed = false;
        mc_world_storage_error error;
        CHECK(mc_world_runtime_metadata_set_quiet(w, 8, 64, 8, 0, &changed, &error) ==
                  MC_WORLD_STORAGE_OK &&
              changed && f->center[side]->dirty);
        CHECK(mc_world_info_time_set(w->fields.x, 900 + (int64_t)side) == MC_WORLD_INFO_OK);
        int64_t before, after;
        CHECK(mc_platform_current_time_millis(&before) == MC_PLATFORM_MILLIS_OK && before >= 0);
        CHECK(drain(f, side));
        CHECK(mc_platform_current_time_millis(&after) == MC_PLATFORM_MILLIS_OK && after >= before &&
              after / INT64_C(1000) <= UINT32_MAX);
        CHECK(!f->center[side]->active && f->center[side]->dirty &&
              f->center[side]->last_save_time == 900 + (int64_t)side);
        CHECK(check_disk(f, side, 900 + (int64_t)side, (uint32_t)(before / INT64_C(1000)),
                         (uint32_t)(after / INT64_C(1000))));
        CHECK(check_disk(f, side + 4u, -123, 0, 0));
    }
    for (size_t slot = 2; slot < 4; ++slot) {
        char *path = NULL;
        bool exists = true;
        const bool named = region_path(f, slot, &path);
        const mc_world_storage_status status =
            named ? mc_ws_path_exists(path, &exists, NULL) : MC_WORLD_STORAGE_INVALID_ARGUMENT;
        free(path);
        CHECK(named && status == MC_WORLD_STORAGE_OK && !exists);
    }
    for (size_t i = 0; i < COUNT(f->children); ++i) {
        if (f->children[i].state == NULL)
            continue;
        CHECK(mc_world_store_retire_chunk(f->children[i].store, f->children[i].state, NULL) ==
              MC_WORLD_STORAGE_OK);
        f->children[i] = (child){0};
    }
    f->center[0] = NULL;
    f->center[1] = NULL;
    for (size_t i = 0; i < COUNT(f->worlds); ++i) {
        mc_world_runtime *w = f->worlds[i];
        CHECK(mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_OK);
        const mc_world_storage_error before = *mc_world_runtime_last_error(w);
        bool value = true;
        mc_world_solid_support_error error;
        CHECK(mc_world_runtime_solid_support_get(w, 8, 64, 8, &value, &error) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              value && error.bytecode == 0 && fresh_registry(&error.registry, MC_REGISTRY_OK) &&
              same_cause(&before, mc_world_runtime_last_error(w)));
        CHECK(mc_world_runtime_free(w, NULL) == MC_WORLD_STORAGE_OK);
        f->worlds[i] = NULL;
        if (i < COUNT(f->bound))
            f->bound[i] = false;
    }
    ok = true;
cleanup:
    return ok;
}

static bool cleanup_fixture(fixture *f)
{
    if (f == NULL)
        return true;
    for (size_t i = 0; i < COUNT(f->bound); ++i)
        if (f->worlds[i] != NULL && !drain(f, i))
            return false;
    for (size_t i = 0; i < COUNT(f->children); ++i)
        if (f->children[i].state != NULL) {
            if (mc_world_store_retire_chunk(f->children[i].store, f->children[i].state, NULL) !=
                MC_WORLD_STORAGE_OK)
                return false;
            f->children[i] = (child){0};
        }
    for (size_t i = 0; i < COUNT(f->worlds); ++i)
        if (f->worlds[i] != NULL &&
            (mc_world_runtime_retire_world(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK ||
             mc_world_runtime_free(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK))
            return false;
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
    for (size_t i = 0; i < COUNT(f->seed); ++i)
        mc_ws_free_file_bytes(&f->seed[i]);
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

int main(int argc, char **argv)
{
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'S', 'u', 'p'};
    if (argc != 3 || strcmp(argv[1], "world-solid-support") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t i = 0; i < COUNT(f->stores); ++i)
        CHECK(open_store(f, i, root));
    for (size_t i = 0; i < COUNT(f->registry); ++i) {
        resources *r = &f->sources[i];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = (stream){.bytes = language};
        r->streams[2] = (stream){.bytes = language};
        r->fail_first = i >= 2;
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_side side = i % 2u == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER;
        const mc_registry_options options = {side, f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[i], NULL) == MC_REGISTRY_OK);
        r->registry = f->registry[i];
        CHECK(mc_world_runtime_classes_create(
                  i % 2u == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[i], NULL) == MC_WORLD_STORAGE_OK);
        r->classes = f->classes[i];
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[i], f->registry[i], NULL) ==
              MC_REGISTRY_OK);
    }
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[4],
                                          NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < COUNT(f->stores); ++i)
        CHECK(create_world(f, i));
    for (size_t side = 0; side < 2; ++side) {
        CHECK(seed_record(f, side) && seed_record(f, side + 4u));
        CHECK(retain_seed(f, root, side + 4u));
    }
    CHECK(cold_and_admission(f));
    for (size_t side = 0; side < 2; ++side)
        CHECK(late_array(f, side) && selected_predicates(f, side));
    CHECK(maintained_and_retired(f));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("solid-support cleanup refused; live dependencies retained\n", stderr);
        return 1;
    }
    if (result == 0 &&
        printf("PASS: %u World solid-support checks (world-solid-support)\n", checks) < 0)
        result = 1;
    return result;
}
