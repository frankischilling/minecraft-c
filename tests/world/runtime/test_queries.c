#include "mc/block/registry/runtime.h"
#include "mc/resources/translation/translation.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/plants.h"
#include "mc/world/runtime/queries.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/block/registry/runtime_internal.h"
#include "../../../src/world/chunk/object_state_internal.h"
#include "../../../src/world/runtime/chunk_provider_internal.h"
#include "../../../src/world/runtime/construction_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/storage/files.h"
#include "../../../src/world/storage/objects_internal.h"

#include <limits.h>
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

/* Source-derived native contracts, not an original observation or replay.
 * Normal owners come from genuine GAME loading of populated known-empty NBT.
 * Explicit input controls change actual block/height bytes, Nibble references,
 * Registry references, Light statics, and valid cache publications. No phase,
 * lease, or collection count is forged. The denied-get policy control below
 * does not construct a dedicated-server World. All controls restore before an
 * assertion can leave their scope. */
static unsigned int checks;
static mc_math_trig trig;
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct stream {
    const uint8_t *bytes;
    size_t length, offset;
    bool closed;
} stream;

typedef struct resource {
    stream streams[3];
    mc_registry_resources api;
    mc_registry_classes *registry;
    mc_world_runtime_classes *classes;
    mc_world_runtime *world;
    mc_chunk_provider_fields *provider;
    mc_chunk_state *captured, *replacement;
    size_t opens, closes, guarded;
    bool bad, replaced;
} resource;

typedef struct child {
    mc_world_store *store;
    mc_chunk_state *state;
} child;

typedef struct fixture {
    mc_world_regions *regions;
    mc_world_store *stores[4];
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_world_info_name *name;
    resource resources[2];
    mc_registry_classes *registry[2];
    mc_world_runtime_classes *classes[2];
    mc_world_runtime *worlds[8];
    mc_world_provider *dimensions[4];
    mc_chunk_provider_fields providers[4];
    bool bound[4];
    mc_chunk_state *normal[4][2];
    child children[16];
    mc_ws_file_bytes seed[4];
    mc_ws_file_bytes negative_seed[2];
    const mc_world_light_type *types[2][2];
} fixture;

typedef struct record {
    mc_nbt_tag root, level, fields[11];
    const mc_nbt_tag *root_fields[1], *level_fields[11];
    uint16_t names[12][20];
    uint8_t arrays[5][MC_CHUNK_BLOCKS];
} record;

static size_t block_index(int32_t y) {
    return ((size_t)8u << 11u) | ((size_t)8u << 7u) | (size_t)y;
}

static bool same_cause(const mc_world_storage_error *a, const mc_world_storage_error *b) {
    return a->status == b->status && a->operation == b->operation &&
           a->system_error == b->system_error;
}

static bool same_registry(const mc_registry_error *a, const mc_registry_error *b) {
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

static bool cleared(const mc_world_query_error *e) {
    const mc_registry_error registry = {.initializer_pc = -1,
                                        .block_id = -1,
                                        .constructor_pc = -1,
                                        .active_initializer_pc = -1,
                                        .item_argument = -1};
    return e->status == MC_WORLD_STORAGE_OK && e->operation == NULL && e->bytecode == 0 &&
           e->provider_bytecode == 0 && e->chunk_bytecode == 0 &&
           e->acquisition == MC_CHUNK_ACQUIRE_OK && e->cause.status == MC_WORLD_STORAGE_OK &&
           e->cause.operation == NULL && e->cause.system_error == 0 &&
           same_registry(&e->registry, &registry);
}

static bool failure_is(mc_world_runtime *w, const mc_world_query_error *e, unsigned int pc,
                       unsigned int chunk_pc, mc_registry_status registry) {
    return e->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && e->bytecode == pc &&
           e->provider_bytecode == 0 && e->chunk_bytecode == chunk_pc &&
           e->registry.status == registry &&
           same_cause(&e->cause, mc_world_runtime_last_error(w)) &&
           (registry == MC_REGISTRY_OK ||
            same_registry(&e->registry, mc_world_runtime_last_registry_error(w)));
}

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count) {
    stream *s = context;
    if (s == NULL || out == NULL || count == NULL || capacity == 0 || s->closed ||
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
                                        mc_java_properties_input *out) {
    resource *r = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    if (name == NULL || out == NULL || r->opens >= COUNT(names) ||
        strcmp(name, names[r->opens]) != 0) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    const size_t index = r->opens++;
    mc_world_storage_error lease;
    mc_chunk_state *current = NULL;
    bool visible = false;
    mc_world_query_error nested;
    /* This read-only admission reaches the real Store lease count. Only the
     * outer top query holds a lease when each observation runs. */
    if (r->world == NULL || r->provider == NULL || r->captured == NULL || r->replacement == NULL ||
        r->world->active_calls != 1 ||
        mc_world_store_entity_containers_validate(r->provider->store, r->world, &lease) !=
            MC_WORLD_STORAGE_BUSY ||
        lease.operation == NULL ||
        strcmp(lease.operation, "Store Chunk lease during entity retirement") != 0 ||
        mc_world_runtime_retire_world(r->world, NULL) != MC_WORLD_STORAGE_BUSY ||
        mc_world_runtime_free(r->world, NULL) != MC_WORLD_STORAGE_BUSY ||
        mc_world_runtime_classes_free(r->classes, NULL) != MC_WORLD_STORAGE_BUSY ||
        mc_registry_classes_free(r->registry, NULL) != MC_REGISTRY_BUSY ||
        mc_world_runtime_sky_visible(r->world, 24, INT32_MAX, 8, &visible, &nested) !=
            MC_WORLD_STORAGE_OK ||
        !visible || !cleared(&nested) || r->world->active_calls != 1 ||
        mc_world_store_entity_containers_validate(r->provider->store, r->world, &lease) !=
            MC_WORLD_STORAGE_BUSY)
        r->bad = true;
    if (!r->bad && index == 0) {
        if (!mc_chunk_cache_lookup(r->provider->cache, 0, 0, &current) || current != r->captured ||
            !mc_chunk_cache_publish(r->provider->cache, 0, 0, r->replacement))
            r->bad = true;
        else
            r->replaced = true;
    }
    if (r->provider == NULL || !mc_chunk_cache_lookup(r->provider->cache, 0, 0, &current) ||
        current != r->replacement || !mc_chunk_cache_validate(r->provider->cache))
        r->bad = true;
    ++r->guarded;
    *out = (mc_java_properties_input){&r->streams[index], read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    resource *r = context;
    if (input == NULL || input->context != &r->streams[2] || input->read != read_bytes) {
        r->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++r->closes;
    r->streams[2].closed = true;
    return MC_REGISTRY_OK;
}

static bool ready_resource(void *context, const mc_java_properties_input *input) {
    resource *r = context;
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

static bool open_store(fixture *f, size_t slot, const char *root) {
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const uint16_t title[] = {'Q', 'u', 'e', 'r', 'y'};
    const int n = snprintf(path, sizeof(path), "%s/world%zu", root, slot);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    const mc_world_store_options options = {.session_token = 127,
                                            .block_map = map,
                                            .block_map_length = sizeof(map),
                                            .limits = mc_world_store_default_limits()};
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

static bool seed_record(fixture *f, size_t slot, int32_t chunk_x) {
    bool ok = false;
    record *r = calloc(1, sizeof(*r));
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
    r->fields[0].value.int_value = chunk_x;
    r->fields[2].value.long_value = -123;
    r->fields[8].value.byte_value = 1;
    r->arrays[0][block_index(chunk_x == 0 ? 64 : 127)] = 1;
    /* Prepared flower above stone forces the scan to continue on the captured
     * Chunk after Registry initialization replaces the cache's requested key. */
    if (chunk_x == 0)
        r->arrays[0][block_index(65)] = 37;
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
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[slot]), chunk_x, 0,
                                 encoded, length, UINT32_C(0xfedcba98), &growth,
                                 NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool image(fixture *f, const char *root, size_t slot, bool negative, bool retain) {
    bool ok = false;
    char name[64], relative[64];
    char *path = NULL, *copy = NULL;
    mc_ws_file_bytes bytes = {0};
    CHECK(slot < COUNT(f->seed) && (!negative || slot < COUNT(f->negative_seed)));
    mc_ws_file_bytes *seed = negative ? &f->negative_seed[slot] : &f->seed[slot];
    const int relative_length =
        snprintf(relative, sizeof(relative), "region/r.%d.0.mcr", negative ? -1 : 0);
    CHECK(relative_length > 0 && (size_t)relative_length < sizeof(relative));
    CHECK(mc_ws_path_join(mc_world_store_root(f->stores[slot]), relative, &path, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_read_file(path, 2u * 1024u * 1024u, &bytes, NULL) == MC_WORLD_STORAGE_OK);
    if (retain) {
        const int n = snprintf(name, sizeof(name), "seed-%sworld%zu.region.bin",
                               negative ? "negative-" : "", slot);
        CHECK(n > 0 && (size_t)n < sizeof(name));
        CHECK(mc_ws_path_join(root, name, &copy, NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_ws_write_file(copy, bytes.data, bytes.length, true, NULL) == MC_WORLD_STORAGE_OK);
        *seed = bytes;
        bytes = (mc_ws_file_bytes){0};
    } else
        CHECK(bytes.length == seed->length && memcmp(bytes.data, seed->data, bytes.length) == 0);
    ok = true;
cleanup:
    mc_ws_free_file_bytes(&bytes);
    free(path);
    free(copy);
    return ok;
}

static bool create_world(fixture *f, size_t slot) {
    bool ok = false;
    mc_world_runtime_construction state;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    /* Two additional real publications support the spring radius controls.
     * Later capacity trials still fill this owner and assert its exact limit. */
    limits.max_provider_chunks = 10;
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[slot], NULL) == MC_WORLD_PROVIDER_OK);
    CHECK(mc_world_runtime_allocate_world(slot % 2u == 0 ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                                         : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                          f->classes[slot % 2u], &limits, &f->worlds[slot],
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = f->stores[slot], .name = f->name, .seed = -99, .dimension = f->dimensions[slot]};
    CHECK(mc_world_runtime_construct_world(f->worlds[slot], &args, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(f->worlds[slot], &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_READY && state.contexts_bound && state.cache_registered);
    CHECK(mc_chunk_provider_bind_fields(f->worlds[slot]->fields.v, &f->providers[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[slot] = true;
    CHECK(f->providers[slot].cache->count == 0 && f->providers[slot].cache->insertion_count == 0 &&
          f->worlds[slot]->acquisition.policy == MC_CHUNK_ACQUIRE_BASE);
    ok = true;
cleanup:
    return ok;
}

static bool remember(fixture *f, size_t slot) {
    if (!f->bound[slot])
        return true;
    const mc_chunk_cache *cache = f->providers[slot].cache;
    if (!mc_chunk_cache_validate(cache))
        return false;
    for (size_t j = 0; j < cache->insertion_count; ++j) {
        mc_chunk_state *s = cache->insertions[j];
        if (s == f->providers[slot].fallback)
            continue;
        if (s == NULL || !mc_world_store_owns_chunk(f->stores[slot], s))
            return false;
        size_t i = 0;
        while (i < COUNT(f->children) && f->children[i].state != NULL && f->children[i].state != s)
            ++i;
        if (i == COUNT(f->children))
            return false;
        f->children[i] = (child){f->stores[slot], s};
    }
    return true;
}

static bool load_normals(fixture *f, size_t slot, size_t count) {
    bool ok = false;
    for (size_t i = 0; i < count; ++i) {
        CHECK(mc_world_runtime_acquire(f->worlds[slot], (int32_t)i, 0, &f->normal[slot][i]) ==
              MC_CHUNK_ACQUIRE_OK);
        CHECK(remember(f, slot));
        CHECK(f->normal[slot][i] != NULL && !f->normal[slot][i]->blank &&
              f->normal[slot][i]->terrain_populated && f->normal[slot][i]->active &&
              !f->normal[slot][i]->dirty &&
              f->normal[slot][i]->data->blocks[block_index(i == 0 ? 64 : 127)] == 1);
    }
    ok = true;
cleanup:
    return ok;
}

static bool cold_paths(fixture *f, size_t side) {
    bool ok = false, visible = true;
    int32_t value = 777;
    mc_world_query_error e;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_block_view view;
    CHECK(!f->classes[side]->light_types_initialized &&
          !mc_world_runtime_chunk_light_seen(f->classes[side]));
    CHECK(mc_world_runtime_saved_light(w, NULL, 8, INT32_MIN, 8, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          value == 0 && cleared(&e));
    CHECK(mc_world_runtime_plant_light(w, INT32_MAX, -1, INT32_MIN, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          value == 0 && cleared(&e));
    CHECK(mc_world_runtime_block_light(w, 32000000, INT32_MIN, 8, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          value == 15 && cleared(&e));
    CHECK(mc_world_runtime_block_light(w, 8, INT32_MAX, -32000001, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          value == 15 && cleared(&e));
    value = 777;
    CHECK(mc_world_runtime_saved_light(w, NULL, 32000000, INT32_MAX, 0, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value == 777 && e.bytecode == 54 && e.provider_bytecode == 0 && e.chunk_bytecode == 0);
    const mc_world_storage_error previous = *mc_world_runtime_last_error(w);
    CHECK(mc_world_runtime_saved_light(w, NULL, 8, 64, 8, &value, &e) == MC_WORLD_STORAGE_OK &&
          value == 0 && cleared(&e) && same_cause(&previous, mc_world_runtime_last_error(w)));
    CHECK(mc_chunk_cache_publish(f->providers[side].cache, 2, 0, f->providers[side].fallback));
    CHECK(mc_world_runtime_top_solid_liquid(w, 40, 8, &value, &e) == MC_WORLD_STORAGE_OK &&
          value == -1 && cleared(&e));
    CHECK(mc_world_runtime_saved_light(w, NULL, 40, 128, 8, &value, &e) == MC_WORLD_STORAGE_OK &&
          value == 0 && cleared(&e));
    CHECK(mc_world_runtime_plant_light(w, 40, INT32_MAX, 8, &value, &e) == MC_WORLD_STORAGE_OK &&
          value == 0 && cleared(&e));
    CHECK(mc_world_runtime_sky_visible(w, 40, INT32_MAX, 8, &visible, &e) == MC_WORLD_STORAGE_OK &&
          !visible && cleared(&e));
    CHECK(!f->classes[side]->light_types_initialized &&
          !mc_world_runtime_chunk_light_seen(f->classes[side]) &&
          mc_registry_classes_inspect_blocks(f->registry[side], &view, NULL) == MC_REGISTRY_OK &&
          view.phase == MC_REGISTRY_COLD && f->resources[side].opens == 0 &&
          mc_world_store_chunk_count(f->stores[side]) == 0 &&
          f->providers[side].cache->count == 1 && f->providers[side].cache->insertion_count == 1);
    ok = true;
cleanup:
    return ok;
}

static bool capture_and_pending(fixture *f, size_t side) {
    bool ok = false, changed = false;
    mc_world_runtime *w = f->worlds[side];
    resource *r = &f->resources[side];
    CHECK(load_normals(f, side, 2));
    CHECK(mc_chunk_pending_add(f->providers[side].pending, mc_chunk_cache_key(0, 0), &changed) ==
              MC_CHUNK_ACQUIRE_OK &&
          changed);
    r->world = w;
    r->provider = &f->providers[side];
    r->captured = f->normal[side][0];
    r->replacement = f->normal[side][1];
    int32_t height = 777;
    mc_world_query_error e;
    CHECK(mc_world_runtime_top_solid_liquid(w, 8, 8, &height, &e) == MC_WORLD_STORAGE_OK &&
          height == 65 && cleared(&e));
    CHECK(!r->bad && r->replaced && r->opens == 3 && r->closes == 1 && r->guarded == 3 &&
          mc_chunk_pending_contains(f->providers[side].pending, mc_chunk_cache_key(0, 0)) &&
          !f->classes[side]->light_types_initialized);
    mc_world_storage_error lease;
    CHECK(mc_world_store_entity_containers_validate(f->stores[side], w, &lease) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_top_solid_liquid(w, 8, 8, &height, &e) == MC_WORLD_STORAGE_OK &&
          height == 128 && cleared(&e));
    CHECK(mc_chunk_cache_publish(f->providers[side].cache, 0, 0, f->normal[side][0]));
    CHECK(remember(f, side));
    CHECK(mc_world_runtime_top_solid_liquid(w, 33554440, 8, &height, &e) == MC_WORLD_STORAGE_OK &&
          height == 65 && cleared(&e));
    CHECK(mc_chunk_pending_contains(f->providers[side].pending, mc_chunk_cache_key(0, 0)) &&
          f->providers[side].cache->count == 3 && f->providers[side].cache->insertion_count == 5);
    ok = true;
cleanup:
    return ok;
}

static bool column_case(fixture *f, size_t side, const uint8_t *column, int32_t expected) {
    bool ok = false;
    uint8_t saved[128];
    uint8_t *blocks = f->normal[side][0]->data->blocks;
    for (int32_t y = 0; y < 128; ++y) {
        saved[y] = blocks[block_index(y)];
        blocks[block_index(y)] = column[y];
    }
    int32_t value = 777;
    mc_world_query_error e;
    const mc_world_storage_status status =
        mc_world_runtime_top_solid_liquid(f->worlds[side], 8, 8, &value, &e);
    for (int32_t y = 0; y < 128; ++y)
        blocks[block_index(y)] = saved[y];
    CHECK(status == MC_WORLD_STORAGE_OK && value == expected && cleared(&e));
    ok = true;
cleanup:
    return ok;
}

static bool top_contracts(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    uint8_t column[128] = {0};
    column[0] = 1;
    CHECK(column_case(f, side, column, -1));
    column[1] = 1;
    CHECK(column_case(f, side, column, 2));
    column[127] = 1;
    CHECK(column_case(f, side, column, 128));
    const mc_block *water = NULL, *flower = NULL;
    CHECK(mc_registry_block_static_get(r, "C", &water, NULL) == MC_REGISTRY_OK && water != NULL);
    CHECK(mc_registry_block_static_get(r, "ae", &flower, NULL) == MC_REGISTRY_OK && flower != NULL);
    const mc_block_fields *wf = mc_block_fields_get(water), *ff = mc_block_fields_get(flower);
    CHECK(wf != NULL && ff != NULL && wf->bn > 0 && wf->bn <= UINT8_MAX && ff->bn > 0 &&
          ff->bn <= UINT8_MAX && !mc_material_c(wf->bA) && mc_material_d(wf->bA) &&
          !mc_material_c(ff->bA) && !mc_material_d(ff->bA));
    memset(column, 0, sizeof(column));
    column[127] = (uint8_t)ff->bn;
    column[126] = (uint8_t)wf->bn;
    CHECK(column_case(f, side, column, 127));
    column[126] = 0;
    CHECK(column_case(f, side, column, -1));
    mc_world_query_error e;
    int32_t value = 777;
    mc_block **saved_array = r->blocks.m;
    r->blocks.m = NULL;
    mc_world_storage_status status = mc_world_runtime_top_solid_liquid(w, 8, 8, &value, &e);
    r->blocks.m = saved_array;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value == 777 &&
          failure_is(w, &e, 52, 0, MC_REGISTRY_NULL_REFERENCE));
    mc_block *saved_block = r->blocks.m[1];
    r->blocks.m[1] = NULL;
    status = mc_world_runtime_top_solid_liquid(w, 8, 8, &value, &e);
    r->blocks.m[1] = saved_block;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value == 777 &&
          failure_is(w, &e, 53, 0, MC_REGISTRY_NULL_REFERENCE));
    mc_material *saved_material = saved_block->fields.bA;
    saved_block->fields.bA = NULL;
    status = mc_world_runtime_top_solid_liquid(w, 8, 8, &value, &e);
    saved_block->fields.bA = saved_material;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value == 777 &&
          failure_is(w, &e, 60, 0, MC_REGISTRY_NULL_REFERENCE));
    CHECK(mc_chunk_pending_contains(f->providers[side].pending, mc_chunk_cache_key(0, 0)));
    ok = true;
cleanup:
    return ok;
}

static bool light_and_height(fixture *f, size_t side) {
    bool ok = false, visible = true;
    mc_world_runtime *w = f->worlds[side];
    mc_world_chunk *chunk = mc_world_store_chunk_object(f->stores[side], f->normal[side][0]);
    CHECK(chunk != NULL && chunk->e != NULL && chunk->f != NULL && chunk->g != NULL &&
          chunk->h != NULL);
    int32_t cold_value = 777;
    mc_world_query_error cold_error;
    CHECK(!f->classes[side]->light_types_initialized &&
          mc_world_runtime_saved_light(w, NULL, 8, 64, 8, &cold_value, &cold_error) ==
              MC_WORLD_STORAGE_OK &&
          cold_value == 0 && cleared(&cold_error) && f->classes[side]->light_types_initialized &&
          !mc_world_runtime_chunk_light_seen(f->classes[side]));
    for (int kind = MC_LIGHT_SKY; kind <= MC_LIGHT_BLOCK; ++kind)
        CHECK(mc_world_runtime_light_type_get(f->classes[side], (mc_light_kind)kind,
                                              &f->types[side][kind], NULL) == MC_WORLD_STORAGE_OK);
    const size_t index = block_index(64) / 2u;
    const uint8_t old_sky = chunk->f->a[index], old_block = chunk->g->a[index];
    chunk->f->a[index] = UINT8_C(0xc3);
    chunk->g->a[index] = UINT8_C(0x4e);
    int32_t value = 777;
    mc_world_query_error e;
    mc_world_storage_status status[4];
    int32_t values[4] = {777, 777, 777, 777};
    bool frames = true;
    for (size_t i = 0; i < COUNT(status); ++i) {
        status[i] = mc_world_runtime_saved_light(w, f->types[side][i / 2u], 8,
                                                 64 + (int32_t)(i % 2u), 8, &values[i], &e);
        frames = frames && cleared(&e);
    }
    chunk->f->a[index] = old_sky;
    chunk->g->a[index] = old_block;
    CHECK(frames && status[0] == MC_WORLD_STORAGE_OK && status[1] == MC_WORLD_STORAGE_OK &&
          status[2] == MC_WORLD_STORAGE_OK && status[3] == MC_WORLD_STORAGE_OK && values[0] == 3 &&
          values[1] == 12 && values[2] == 14 && values[3] == 4 &&
          !mc_world_runtime_chunk_light_seen(f->classes[side]));
    const size_t low = block_index(0) / 2u, high = block_index(127) / 2u;
    const uint8_t old_low = chunk->f->a[low], old_high = chunk->f->a[high];
    chunk->f->a[low] = 6;
    chunk->f->a[high] = UINT8_C(0xb0);
    status[0] = mc_world_runtime_saved_light(w, f->types[side][0], 8, INT32_MIN, 8, &values[0], &e);
    frames = cleared(&e);
    status[1] = mc_world_runtime_saved_light(w, f->types[side][0], 8, INT32_MAX, 8, &values[1], &e);
    frames = frames && cleared(&e);
    chunk->f->a[low] = old_low;
    chunk->f->a[high] = old_high;
    CHECK(frames && status[0] == MC_WORLD_STORAGE_OK && status[1] == MC_WORLD_STORAGE_OK &&
          values[0] == 6 && values[1] == 11);
    CHECK(mc_world_runtime_saved_light(w, f->types[side][0], 32000000, 64, 8, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          value == 15 && cleared(&e));
    CHECK(mc_world_runtime_saved_light(w, f->types[side][1], 8, 64, 32000001, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          value == 0 && cleared(&e));
    /* NULL ordinarily matches neither Light identity and must not inspect
     * malformed Nibbles. Explicit aliases prove live comparison ordering. */
    mc_world_chunk_nibble *saved_f = chunk->f, *saved_g = chunk->g;
    chunk->f = NULL;
    chunk->g = NULL;
    status[0] = mc_world_runtime_saved_light(w, NULL, 8, 64, 8, &value, &e);
    chunk->f = saved_f;
    chunk->g = saved_g;
    CHECK(status[0] == MC_WORLD_STORAGE_OK && value == 0 && cleared(&e));
    const mc_world_light_type *saved_static = f->classes[side]->light_statics[0];
    chunk->f->a[index] = UINT8_C(0xc3);
    f->classes[side]->light_statics[0] = NULL;
    status[0] = mc_world_runtime_saved_light(w, NULL, 8, 64, 8, &value, &e);
    f->classes[side]->light_statics[0] = saved_static;
    chunk->f->a[index] = old_sky;
    CHECK(status[0] == MC_WORLD_STORAGE_OK && value == 3 && cleared(&e));
    chunk->f->a[index] = UINT8_C(0xc3);
    f->classes[side]->light_statics[0] = f->types[side][1];
    status[0] = mc_world_runtime_saved_light(w, f->types[side][1], 8, 64, 8, &value, &e);
    f->classes[side]->light_statics[0] = saved_static;
    chunk->f->a[index] = old_sky;
    CHECK(status[0] == MC_WORLD_STORAGE_OK && value == 3 && cleared(&e));
    const size_t hi = ((size_t)8u << 4u) | 8u;
    const uint8_t old_height = chunk->h[hi];
    chunk->h[hi] = UINT8_MAX;
    bool observed[4] = {true, true, false, false};
    const int32_t heights[] = {INT32_MIN, 254, 255, INT32_MAX};
    frames = true;
    for (size_t i = 0; i < COUNT(heights); ++i) {
        status[i] = mc_world_runtime_sky_visible(w, 33554440, heights[i], 8, &observed[i], &e);
        frames = frames && cleared(&e);
    }
    chunk->h[hi] = old_height;
    CHECK(frames && status[0] == MC_WORLD_STORAGE_OK && status[1] == MC_WORLD_STORAGE_OK &&
          status[2] == MC_WORLD_STORAGE_OK && status[3] == MC_WORLD_STORAGE_OK && !observed[0] &&
          !observed[1] && observed[2] && observed[3]);
    uint8_t *saved_h = chunk->h;
    chunk->h = NULL;
    const mc_world_storage_status bad_height =
        mc_world_runtime_sky_visible(w, 8, 0, 8, &visible, &e);
    chunk->h = saved_h;
    CHECK(bad_height == MC_WORLD_STORAGE_UNSUPPORTED_STATE && visible &&
          failure_is(w, &e, 19, 10, MC_REGISTRY_OK));
    CHECK(mc_chunk_pending_contains(f->providers[side].pending, mc_chunk_cache_key(0, 0)));
    ok = true;
cleanup:
    return ok;
}

static bool plant_flag(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_world_chunk *chunk = mc_world_store_chunk_object(f->stores[side], f->normal[side][0]);
    const size_t index = block_index(64) / 2u;
    const uint8_t old_sky = chunk->f->a[index], old_block = chunk->g->a[index];
    CHECK(!mc_world_runtime_chunk_light_seen(f->classes[side]));
    chunk->f->a[index] = 0;
    chunk->g->a[index] = 14;
    int32_t value = 777;
    mc_world_query_error e;
    mc_world_storage_status status = mc_world_runtime_plant_light(w, 8, 64, 8, &value, &e);
    chunk->f->a[index] = old_sky;
    chunk->g->a[index] = old_block;
    CHECK(status == MC_WORLD_STORAGE_OK && value == 14 && cleared(&e) &&
          !mc_world_runtime_chunk_light_seen(f->classes[side]));
    mc_world_chunk_nibble *saved = chunk->f;
    chunk->f = NULL;
    value = 777;
    status = mc_world_runtime_plant_light(w, 8, 64, 8, &value, &e);
    chunk->f = saved;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value == 777 &&
          failure_is(w, &e, 36, 7, MC_REGISTRY_OK) &&
          !mc_world_runtime_chunk_light_seen(f->classes[side]));
    /* The class write precedes BlockLight access and survives this failure. */
    chunk->f->a[index] = 3;
    saved = chunk->g;
    chunk->g = NULL;
    status = mc_world_runtime_plant_light(w, 8, 64, 8, &value, &e);
    chunk->g = saved;
    chunk->f->a[index] = old_sky;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value == 777 &&
          failure_is(w, &e, 36, 35, MC_REGISTRY_OK) &&
          mc_world_runtime_chunk_light_seen(f->classes[side]));
    CHECK(create_world(f, side + 2u) && load_normals(f, side + 2u, 1));
    CHECK(mc_world_runtime_chunk_light_seen(f->classes[side]) &&
          mc_world_runtime_plant_light(f->worlds[side + 2u], 8, 64, 8, &value, &e) ==
              MC_WORLD_STORAGE_OK &&
          value == 0 && cleared(&e) && mc_world_runtime_chunk_light_seen(f->classes[side]));
    chunk->f->a[index] = UINT8_C(0xc3);
    chunk->g->a[index] = UINT8_C(0x4e);
    int32_t values[2] = {777, 777};
    mc_world_storage_status results[2];
    results[0] = mc_world_runtime_plant_light(w, 33554440, 64, 8, &values[0], &e);
    bool frames = cleared(&e);
    results[1] = mc_world_runtime_plant_light(w, 8, 65, 8, &values[1], &e);
    frames = frames && cleared(&e);
    chunk->f->a[index] = old_sky;
    chunk->g->a[index] = old_block;
    CHECK(frames && results[0] == MC_WORLD_STORAGE_OK && results[1] == MC_WORLD_STORAGE_OK &&
          values[0] == 14 && values[1] == 12);
    const size_t high = block_index(127) / 2u;
    const uint8_t old_high = chunk->g->a[high];
    chunk->g->a[high] = UINT8_C(0xd0);
    status = mc_world_runtime_plant_light(w, 8, INT32_MAX, 8, &value, &e);
    chunk->g->a[high] = old_high;
    CHECK(status == MC_WORLD_STORAGE_OK && value == 13 && cleared(&e) &&
          mc_chunk_pending_contains(f->providers[side].pending, mc_chunk_cache_key(0, 0)));
    ok = true;
cleanup:
    return ok;
}

static bool denied_native_policy(fixture *f, size_t side) {
    bool ok = false, visible = true;
    mc_world_runtime *w = f->worlds[side];
    CHECK(w->acquisition.policy == MC_CHUNK_ACQUIRE_BASE && !*w->acquisition.world_force_load &&
          !*w->acquisition.provider_force_load);
    const size_t count = f->providers[side].cache->count;
    const size_t inserted = f->providers[side].cache->insertion_count;
    const size_t owned = mc_world_store_chunk_count(f->stores[side]);
    const mc_chunk_acquire_policy original = w->acquisition.policy;
    mc_world_storage_status results[4];
    int32_t values[3] = {777, 777, 777};
    mc_world_query_error e;
    bool frames = true;
    /* Quiescent native dependency input only. Actual base World construction
     * and every entered callback/binding remain unchanged. */
    w->acquisition.policy = MC_CHUNK_ACQUIRE_DEDICATED;
    results[0] = mc_world_runtime_top_solid_liquid(w, 56, 8, &values[0], &e);
    frames = frames && cleared(&e);
    results[1] = mc_world_runtime_plant_light(w, 56, 64, 8, &values[1], &e);
    frames = frames && cleared(&e);
    results[2] = mc_world_runtime_sky_visible(w, 56, INT32_MAX, 8, &visible, &e);
    frames = frames && cleared(&e);
    results[3] = mc_world_runtime_saved_light(w, NULL, 56, 64, 8, &values[2], &e);
    frames = frames && cleared(&e);
    w->acquisition.policy = original;
    CHECK(frames && results[0] == MC_WORLD_STORAGE_OK && results[1] == MC_WORLD_STORAGE_OK &&
          results[2] == MC_WORLD_STORAGE_OK && results[3] == MC_WORLD_STORAGE_OK &&
          values[0] == -1 && values[1] == 0 && values[2] == 0 && !visible &&
          f->providers[side].cache->count == count &&
          f->providers[side].cache->insertion_count == inserted &&
          mc_world_store_chunk_count(f->stores[side]) == owned &&
          !mc_chunk_cache_contains(f->providers[side].cache, 3, 0));
    ok = true;
cleanup:
    return ok;
}

static bool prefixes_and_admission(fixture *f, size_t side) {
    bool ok = false, visible = true;
    mc_world_query_error e;
    int32_t value = 777;
    mc_world_runtime *ready = f->worlds[side];
    const mc_world_storage_error previous = *mc_world_runtime_last_error(ready);
    CHECK(mc_world_runtime_top_solid_liquid(NULL, 0, 0, &value, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value == 777 && e.bytecode == 0);
    CHECK(mc_world_runtime_saved_light(NULL, NULL, 0, 0, 0, &value, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value == 777 && e.bytecode == 0);
    CHECK(mc_world_runtime_plant_light(NULL, 0, -1, 0, &value, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value == 777 && e.bytecode == 0);
    CHECK(mc_world_runtime_block_light(NULL, 0, -1, 0, &value, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value == 777 && e.bytecode == 0);
    CHECK(mc_world_runtime_sky_visible(NULL, 0, 0, 0, &visible, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          visible && e.bytecode == 0);
    CHECK(mc_world_runtime_top_solid_liquid(ready, 0, 0, NULL, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          e.bytecode == 0);
    CHECK(mc_world_runtime_saved_light(ready, NULL, 0, 0, 0, NULL, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          e.bytecode == 0);
    CHECK(mc_world_runtime_plant_light(ready, 0, 0, 0, NULL, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          e.bytecode == 0);
    CHECK(mc_world_runtime_block_light(ready, 0, 0, 0, NULL, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          e.bytecode == 0);
    CHECK(mc_world_runtime_sky_visible(ready, 0, 0, 0, NULL, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          e.bytecode == 0 && same_cause(&previous, mc_world_runtime_last_error(ready)));
    CHECK(mc_world_runtime_saved_light(ready, f->types[1u - side][0], INT32_MAX, -1, 0, &value,
                                       &e) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value == 777 && e.bytecode == 0 &&
          same_cause(&previous, mc_world_runtime_last_error(ready)));
    for (size_t phase = 0; phase < 2; ++phase) {
        const size_t slot = 4u + phase * 2u + side;
        const mc_world_runtime_entry entry = side == 0 ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                                       : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE;
        CHECK(mc_world_runtime_allocate_world(entry, f->classes[side], NULL, &f->worlds[slot],
                                              NULL) == MC_WORLD_STORAGE_OK);
        mc_world_runtime *w = f->worlds[slot];
        if (phase != 0) {
            const mc_world_runtime_world_arguments args = {.name = f->name, .seed = 23};
            CHECK(mc_world_runtime_construct_world(w, &args, NULL) ==
                  MC_WORLD_STORAGE_INVALID_ARGUMENT);
        }
        mc_world_runtime_construction state;
        CHECK(mc_world_runtime_construction_get(w, &state) == MC_WORLD_STORAGE_OK &&
              state.phase ==
                  (phase == 0 ? MC_WORLD_RUNTIME_RAW : MC_WORLD_RUNTIME_FAILED_CONSTRUCTION));
        const mc_world_storage_error before = *mc_world_runtime_last_error(w);
        CHECK(mc_world_runtime_plant_light(w, INT32_MAX, -1, INT32_MIN, &value, &e) ==
                  MC_WORLD_STORAGE_OK &&
              value == 0 && cleared(&e) && same_cause(&before, mc_world_runtime_last_error(w)));
        CHECK(mc_world_runtime_saved_light(w, f->types[side][0], 32000000, INT32_MIN, 0, &value,
                                           &e) == MC_WORLD_STORAGE_OK &&
              value == 15 && cleared(&e) && same_cause(&before, mc_world_runtime_last_error(w)));
        CHECK(mc_world_runtime_block_light(w, 8, INT32_MIN, 8, &value, &e) == MC_WORLD_STORAGE_OK &&
              value == 0 && cleared(&e) && same_cause(&before, mc_world_runtime_last_error(w)));
        value = 777;
        CHECK(mc_world_runtime_top_solid_liquid(w, 8, 8, &value, &e) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              value == 777 && e.bytecode == 3 && e.provider_bytecode == 6);
        CHECK(mc_world_runtime_saved_light(w, NULL, 8, 64, 8, &value, &e) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              value == 777 && e.bytecode == 74 && e.provider_bytecode == 6);
        CHECK(mc_world_runtime_plant_light(w, 8, 0, 8, &value, &e) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              value == 777 && e.bytecode == 23 && e.provider_bytecode == 6);
        CHECK(mc_world_runtime_sky_visible(w, INT32_MAX, INT32_MIN, INT32_MIN, &visible, &e) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              visible && e.bytecode == 7 && e.provider_bytecode == 6 && w->active_calls == 0);
    }
    ok = true;
cleanup:
    return ok;
}

static bool retired_contract(mc_world_runtime *w) {
    bool ok = false, visible = true;
    int32_t value = 777;
    mc_world_query_error e;
    const mc_world_storage_error previous = *mc_world_runtime_last_error(w);
    CHECK(mc_world_runtime_top_solid_liquid(w, 0, 0, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value == 777 && e.bytecode == 0);
    CHECK(mc_world_runtime_saved_light(w, NULL, 0, 0, 0, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value == 777 && e.bytecode == 0);
    CHECK(mc_world_runtime_plant_light(w, 0, -1, 0, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value == 777 && e.bytecode == 0);
    CHECK(mc_world_runtime_block_light(w, 0, -1, 0, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value == 777 && e.bytecode == 0);
    CHECK(mc_world_runtime_sky_visible(w, 0, 0, 0, &visible, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          visible && e.bytecode == 0 && same_cause(&previous, mc_world_runtime_last_error(w)));
    ok = true;
cleanup:
    return ok;
}

static bool cleanup_fixture(fixture *f, bool verify_retired) {
    if (f == NULL)
        return true;
    for (size_t slot = 0; slot < COUNT(f->bound); ++slot) {
        if (!f->bound[slot])
            continue;
        if (!remember(f, slot) ||
            mc_world_runtime_retire_entity_containers(f->worlds[slot], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        mc_chunk_cache *cache = f->providers[slot].cache;
        if (!mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                                 cache->insertion_capacity) ||
            mc_chunk_pending_clear(f->providers[slot].pending) != MC_CHUNK_ACQUIRE_OK ||
            mc_world_runtime_retire_world(f->worlds[slot], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->bound[slot] = false;
    }
    for (size_t i = 0; i < COUNT(f->children); ++i) {
        if (f->children[i].state == NULL)
            continue;
        if (mc_world_store_retire_chunk(f->children[i].store, f->children[i].state, NULL) !=
            MC_WORLD_STORAGE_OK)
            return false;
        f->children[i] = (child){0};
    }
    for (size_t i = 0; i < COUNT(f->worlds); ++i) {
        if (f->worlds[i] == NULL)
            continue;
        if (mc_world_runtime_retire_world(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK ||
            (verify_retired && !retired_contract(f->worlds[i])) ||
            mc_world_runtime_free(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        f->worlds[i] = NULL;
    }
    for (size_t i = 0; i < COUNT(f->dimensions); ++i)
        mc_world_provider_release(f->dimensions[i]);
    for (size_t i = 0; i < COUNT(f->classes); ++i)
        if (mc_world_runtime_classes_free(f->classes[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
    for (size_t i = 0; i < COUNT(f->registry); ++i)
        if (mc_registry_classes_free(f->registry[i], NULL) != MC_REGISTRY_OK)
            return false;
    mc_world_info_name_release(f->name);
    for (size_t i = 0; i < COUNT(f->stores); ++i) {
        if (mc_world_store_chunk_count(f->stores[i]) != 0)
            return false;
        mc_world_store_free(f->stores[i]);
        mc_ws_free_file_bytes(&f->seed[i]);
    }
    for (size_t i = 0; i < COUNT(f->negative_seed); ++i)
        mc_ws_free_file_bytes(&f->negative_seed[i]);
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

static bool query_registry_fresh(const mc_registry_error *error, mc_registry_status status) {
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

static bool detailed_material_contracts(fixture *f, size_t side) {
    bool ok = false, supported = true;
    mc_world_runtime *ready = f->worlds[side];
    mc_world_runtime *raw = f->worlds[4u + side];
    mc_registry_classes *registry = f->registry[side];
    CHECK(ready != NULL && raw != NULL && raw->construction.phase == MC_WORLD_RUNTIME_RAW &&
          registry->blocks.m != NULL && registry->blocks.m[1] != NULL);
    mc_block *block = registry->blocks.m[1];
    mc_material *sentinel = block->fields.bA;
    CHECK(sentinel != NULL && f->normal[side][0]->data->blocks[block_index(64)] == 1);

    /* Produce a real prior Registry exception on this same RAW World. Its
     * bounded y=-1 block read returns air before provider use, then the retained
     * NULL m reference fails at the reached array access. This does not forge
     * World.last_registry_error to manufacture a stale diagnostic. */
    mc_block **saved_array = registry->blocks.m;
    registry->blocks.m = NULL;
    mc_world_solid_support_error support;
    const mc_world_storage_status support_status =
        mc_world_runtime_solid_support_get(raw, 8, -1, 8, &supported, &support);
    registry->blocks.m = saved_array;
    CHECK(support_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && supported &&
          support.bytecode == 10 &&
          query_registry_fresh(&support.registry, MC_REGISTRY_NULL_REFERENCE) &&
          same_registry(&support.registry, mc_world_runtime_last_registry_error(raw)));
    const mc_registry_error prior = *mc_world_runtime_last_registry_error(raw);

    /* The real lower block query now fails on the unconstructed provider. The
     * detailed wrapper must forward that exact lower cause, preserve its own
     * result, and leave its fresh Registry frame empty. Earlier World Registry
     * diagnostics remain real prior state, not the cause of this new frame. */
    int32_t id = 777;
    mc_world_storage_error getter_cause;
    const mc_world_storage_status getter_status =
        mc_world_runtime_block_get(raw, 8, 64, 8, &id, &getter_cause);
    mc_material *material = sentinel;
    mc_world_material_error error;
    CHECK(getter_status != MC_WORLD_STORAGE_OK && id == 777 &&
          mc_world_runtime_material_get_detailed(raw, 8, 64, 8, &material, &error) ==
              getter_status &&
          material == sentinel && error.status == getter_status && error.bytecode == 4 &&
          same_cause(&error.cause, &getter_cause) &&
          query_registry_fresh(&error.registry, MC_REGISTRY_OK) &&
          same_registry(&prior, mc_world_runtime_last_registry_error(raw)));

    /* A live null Block is an actual bA receiver failure at24, rather than
     * successful nullable Material output. Check both public surfaces execute
     * the same body without adopting the nullable result early. */
    registry->blocks.m[1] = NULL;
    const mc_world_storage_status null_block_status =
        mc_world_runtime_material_get_detailed(ready, 8, 64, 8, &material, &error);
    mc_material *legacy_material = sentinel;
    mc_world_storage_error legacy_error;
    const mc_world_storage_status legacy_null_status =
        mc_world_runtime_material_get(ready, 8, 64, 8, &legacy_material, &legacy_error);
    registry->blocks.m[1] = block;
    CHECK(null_block_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          legacy_null_status == null_block_status && material == sentinel &&
          legacy_material == sentinel && error.bytecode == 24 &&
          error.status == null_block_status && same_cause(&error.cause, &legacy_error) &&
          query_registry_fresh(&error.registry, MC_REGISTRY_NULL_REFERENCE) &&
          same_registry(&error.registry, mc_world_runtime_last_registry_error(ready)));

    /* The Block exists but its real bA field is NULL: both calls succeed and
     * publish a nullable result. A stale first cause cannot turn this into a
     * failure, and no material predicate is invoked by this method. */
    block->fields.bA = NULL;
    const mc_world_storage_status null_material_status =
        mc_world_runtime_material_get_detailed(ready, 8, 64, 8, &material, &error);
    const mc_world_storage_status legacy_material_status =
        mc_world_runtime_material_get(ready, 8, 64, 8, &legacy_material, &legacy_error);
    block->fields.bA = sentinel;
    CHECK(null_material_status == MC_WORLD_STORAGE_OK &&
          legacy_material_status == MC_WORLD_STORAGE_OK && material == NULL &&
          legacy_material == NULL && error.status == MC_WORLD_STORAGE_OK &&
          error.operation == NULL && error.bytecode == 0 &&
          error.cause.status == MC_WORLD_STORAGE_OK && error.cause.operation == NULL &&
          error.cause.system_error == 0 && query_registry_fresh(&error.registry, MC_REGISTRY_OK) &&
          legacy_error.status == MC_WORLD_STORAGE_OK && legacy_error.operation == NULL &&
          query_registry_fresh(mc_world_runtime_last_registry_error(ready), MC_REGISTRY_OK) &&
          mc_chunk_pending_contains(f->providers[side].pending, mc_chunk_cache_key(0, 0)));
    ok = true;
cleanup:
    return ok;
}

static bool captured_o_contracts(fixture *f, size_t side) {
    bool ok = false;
    mc_registry_classes *registry = f->registry[side];
    CHECK(registry != NULL && registry->blocks.o != NULL && f->resources[side].opens == 3 &&
          !f->resources[side].bad);
    const size_t opens = f->resources[side].opens;
    mc_registry_error error;
    mc_registry_block_o_view captured;
    CHECK(mc_registry_block_o_get(registry, &captured, &error) == MC_REGISTRY_OK &&
          captured.classes == registry && captured.identity == registry->blocks.o &&
          captured.length == MC_REGISTRY_BLOCK_SLOTS &&
          query_registry_fresh(&error, MC_REGISTRY_OK));

    bool alternate[MC_REGISTRY_BLOCK_SLOTS];
    bool *saved_array = registry->blocks.o;
    for (size_t i = 0; i < COUNT(alternate); ++i)
        alternate[i] = !saved_array[i];
    const bool old_value = saved_array[1];
    bool old_result = !old_value, current_result = old_value, changed_result = old_value;
    mc_registry_block_o_view current;

    /* A captured reference keeps its original allocation. A fresh get sees the
     * replacement field. Reading the old allocation's subsequently changed
     * element separately proves reference capture did not copy array contents. */
    registry->blocks.o = alternate;
    const mc_registry_status old_status = mc_registry_block_o_at(&captured, 1, &old_result, &error);
    const bool old_frame = query_registry_fresh(&error, MC_REGISTRY_OK);
    const mc_registry_status current_status = mc_registry_block_o_get(registry, &current, &error);
    const bool current_frame = query_registry_fresh(&error, MC_REGISTRY_OK);
    const mc_registry_status current_at =
        current_status == MC_REGISTRY_OK
            ? mc_registry_block_o_at(&current, 1, &current_result, &error)
            : current_status;
    const bool current_at_frame = query_registry_fresh(&error, MC_REGISTRY_OK);
    saved_array[1] = !old_value;
    const mc_registry_status changed_status =
        mc_registry_block_o_at(&captured, 1, &changed_result, &error);
    const bool changed_frame = query_registry_fresh(&error, MC_REGISTRY_OK);
    saved_array[1] = old_value;
    registry->blocks.o = saved_array;
    CHECK(old_status == MC_REGISTRY_OK && old_frame && old_result == old_value &&
          current_status == MC_REGISTRY_OK && current_frame && current.identity == alternate &&
          current.length == COUNT(alternate) && current_at == MC_REGISTRY_OK && current_at_frame &&
          current_result == !old_value && changed_status == MC_REGISTRY_OK && changed_frame &&
          changed_result == !old_value && captured.identity == saved_array &&
          registry->blocks.o == saved_array && saved_array[1] == old_value &&
          f->resources[side].opens == opens);

    /* NULL is a successful captured source reference. Restoring current o does
     * not heal that reference. Null dereference precedes signed bounds, even
     * for the most negative int; result and fresh cause remain exact. */
    mc_registry_block_o_view null_array;
    registry->blocks.o = NULL;
    const mc_registry_status null_capture = mc_registry_block_o_get(registry, &null_array, &error);
    const bool null_get_frame = query_registry_fresh(&error, MC_REGISTRY_OK);
    registry->blocks.o = saved_array;
    CHECK(null_capture == MC_REGISTRY_OK && null_get_frame && null_array.classes == registry &&
          null_array.identity == NULL && null_array.length == 0);
    bool value = true;
    CHECK(mc_registry_block_o_at(&null_array, INT32_MIN, &value, &error) ==
              MC_REGISTRY_NULL_REFERENCE &&
          value && query_registry_fresh(&error, MC_REGISTRY_NULL_REFERENCE) &&
          same_registry(&error, &registry->last_error));

    const int32_t invalid_indices[] = {-1, 256, INT32_MIN, INT32_MAX};
    for (size_t i = 0; i < COUNT(invalid_indices); ++i) {
        value = true;
        CHECK(mc_registry_block_o_at(&captured, invalid_indices[i], &value, &error) ==
                  MC_REGISTRY_INDEX_OUT_OF_RANGE &&
              value && query_registry_fresh(&error, MC_REGISTRY_INDEX_OUT_OF_RANGE) &&
              same_registry(&error, &registry->last_error));
    }

    /* Native admission has no source array access and cannot overwrite the
     * Registry's earlier real element failure or the caller's value/view. */
    const mc_registry_error before = registry->last_error;
    mc_registry_block_o_view unchanged = captured;
    CHECK(mc_registry_block_o_get(NULL, &unchanged, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          unchanged.classes == captured.classes && unchanged.identity == captured.identity &&
          unchanged.length == captured.length &&
          query_registry_fresh(&error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_block_o_get(registry, NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          query_registry_fresh(&error, MC_REGISTRY_INVALID_ARGUMENT));
    value = true;
    CHECK(mc_registry_block_o_at(NULL, 1, &value, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          value && query_registry_fresh(&error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_block_o_at(&captured, 1, NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          query_registry_fresh(&error, MC_REGISTRY_INVALID_ARGUMENT) &&
          same_registry(&before, &registry->last_error) && registry->blocks.o == saved_array &&
          f->resources[side].opens == opens && registry->entered == 0);
    ok = true;
cleanup:
    return ok;
}

static bool captured_p_contracts(fixture *f, size_t side) {
    bool ok = false;
    mc_registry_classes *registry = f->registry[side];
    CHECK(registry != NULL && registry->blocks.p != NULL && f->resources[side].opens == 3 &&
          !f->resources[side].bad);
    const size_t opens = f->resources[side].opens;
    const size_t before_bytes = registry->allocation_bytes;
    const size_t before_borrows = registry->runtime_borrows;
    const size_t before_profile = mc_java_random_profile_allocation_bytes(f->profile);
    mc_registry_error error;
    mc_registry_block_p_view captured;
    CHECK(mc_registry_block_p_get(registry, &captured, &error) == MC_REGISTRY_OK &&
          captured.classes == registry && captured.identity == registry->blocks.p &&
          captured.length == MC_REGISTRY_BLOCK_SLOTS &&
          query_registry_fresh(&error, MC_REGISTRY_OK));

    const mc_block *chest = NULL, *spawner = NULL, *stone = NULL;
    CHECK(mc_registry_block_static_get(registry, "av", &chest, &error) == MC_REGISTRY_OK &&
          mc_registry_block_static_get(registry, "at", &spawner, &error) == MC_REGISTRY_OK &&
          mc_registry_block_static_get(registry, "u", &stone, &error) == MC_REGISTRY_OK &&
          chest != NULL && spawner != NULL && stone != NULL);
    const mc_block *const receivers[] = {chest, spawner, stone};
    for (size_t i = 0; i < COUNT(receivers); ++i) {
        const mc_block_fields *fields = mc_block_fields_get(receivers[i]);
        bool actual = i == 2;
        CHECK(fields != NULL &&
              mc_registry_block_p_at(&captured, fields->bn, &actual, &error) == MC_REGISTRY_OK &&
              actual == (i != 2) && query_registry_fresh(&error, MC_REGISTRY_OK));
    }

    bool alternate[MC_REGISTRY_BLOCK_SLOTS];
    bool *saved_array = registry->blocks.p;
    for (size_t i = 0; i < COUNT(alternate); ++i)
        alternate[i] = !saved_array[i];
    const bool old_value = saved_array[1];
    bool old_result = !old_value, current_result = old_value, changed_result = old_value;
    mc_registry_block_p_view current;

    /* A captured reference keeps its original allocation. A fresh get sees the
     * replacement field. Reading the old allocation's subsequently changed
     * element separately proves reference capture did not copy array contents. */
    registry->blocks.p = alternate;
    const mc_registry_status old_status = mc_registry_block_p_at(&captured, 1, &old_result, &error);
    const bool old_frame = query_registry_fresh(&error, MC_REGISTRY_OK);
    const mc_registry_status current_status = mc_registry_block_p_get(registry, &current, &error);
    const bool current_frame = query_registry_fresh(&error, MC_REGISTRY_OK);
    const mc_registry_status current_at =
        current_status == MC_REGISTRY_OK
            ? mc_registry_block_p_at(&current, 1, &current_result, &error)
            : current_status;
    const bool current_at_frame = query_registry_fresh(&error, MC_REGISTRY_OK);
    saved_array[1] = !old_value;
    const mc_registry_status changed_status =
        mc_registry_block_p_at(&captured, 1, &changed_result, &error);
    const bool changed_frame = query_registry_fresh(&error, MC_REGISTRY_OK);
    saved_array[1] = old_value;
    registry->blocks.p = saved_array;
    CHECK(old_status == MC_REGISTRY_OK && old_frame && old_result == old_value &&
          current_status == MC_REGISTRY_OK && current_frame && current.identity == alternate &&
          current.length == COUNT(alternate) && current_at == MC_REGISTRY_OK && current_at_frame &&
          current_result == !old_value && changed_status == MC_REGISTRY_OK && changed_frame &&
          changed_result == !old_value && captured.identity == saved_array &&
          registry->blocks.p == saved_array && saved_array[1] == old_value &&
          f->resources[side].opens == opens);

    /* NULL is a successful captured source reference. Restoring current p does
     * not heal that reference. Null dereference precedes signed bounds, even
     * for the most negative int; result and fresh cause remain exact. */
    mc_registry_block_p_view null_array;
    registry->blocks.p = NULL;
    const mc_registry_status null_capture = mc_registry_block_p_get(registry, &null_array, &error);
    const bool null_get_frame = query_registry_fresh(&error, MC_REGISTRY_OK);
    registry->blocks.p = saved_array;
    CHECK(null_capture == MC_REGISTRY_OK && null_get_frame && null_array.classes == registry &&
          null_array.identity == NULL && null_array.length == 0);
    bool value = true;
    CHECK(mc_registry_block_p_at(&null_array, INT32_MIN, &value, &error) ==
              MC_REGISTRY_NULL_REFERENCE &&
          value && query_registry_fresh(&error, MC_REGISTRY_NULL_REFERENCE) &&
          same_registry(&error, &registry->last_error));

    const int32_t invalid_indices[] = {-1, 256, INT32_MIN, INT32_MAX};
    for (size_t i = 0; i < COUNT(invalid_indices); ++i) {
        value = true;
        CHECK(mc_registry_block_p_at(&captured, invalid_indices[i], &value, &error) ==
                  MC_REGISTRY_INDEX_OUT_OF_RANGE &&
              value && query_registry_fresh(&error, MC_REGISTRY_INDEX_OUT_OF_RANGE) &&
              same_registry(&error, &registry->last_error));
    }

    /* Native admission has no source array access and cannot overwrite the
     * Registry's earlier real element failure or the caller's value/view. */
    const mc_registry_error before = registry->last_error;
    mc_registry_block_p_view unchanged = captured;
    CHECK(mc_registry_block_p_get(NULL, &unchanged, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          unchanged.classes == captured.classes && unchanged.identity == captured.identity &&
          unchanged.length == captured.length &&
          query_registry_fresh(&error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_block_p_get(registry, NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          query_registry_fresh(&error, MC_REGISTRY_INVALID_ARGUMENT));
    value = true;
    CHECK(mc_registry_block_p_at(NULL, 1, &value, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          value && query_registry_fresh(&error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_block_p_at(&captured, 1, NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          query_registry_fresh(&error, MC_REGISTRY_INVALID_ARGUMENT) &&
          same_registry(&before, &registry->last_error) && registry->blocks.p == saved_array &&
          f->resources[side].opens == opens && registry->entered == 0);
    mc_registry_block_p_view malformed = captured;
    malformed.length = SIZE_MAX;
    CHECK(mc_registry_block_p_at(&malformed, INT32_MIN, &value, &error) ==
              MC_REGISTRY_INVALID_ARGUMENT &&
          value && query_registry_fresh(&error, MC_REGISTRY_INVALID_ARGUMENT) &&
          same_registry(&before, &registry->last_error));
    malformed.identity = NULL;
    CHECK(mc_registry_block_p_at(&malformed, INT32_MIN, &value, &error) ==
              MC_REGISTRY_NULL_REFERENCE &&
          value && query_registry_fresh(&error, MC_REGISTRY_NULL_REFERENCE));
    value = true;
    CHECK(mc_registry_block_p_at(&captured, 0, &value, NULL) == MC_REGISTRY_OK &&
          value == captured.identity[0]);
    CHECK(mc_registry_block_p_at(&captured, 255, &value, NULL) == MC_REGISTRY_OK &&
          value == captured.identity[255]);
    value = true;
    CHECK(mc_registry_block_p_at(&captured, -1, &value, NULL) == MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          value && query_registry_fresh(&registry->last_error, MC_REGISTRY_INDEX_OUT_OF_RANGE));
    malformed = captured;
    malformed.classes = NULL;
    CHECK(mc_registry_block_p_at(&malformed, 0, &value, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          value && query_registry_fresh(&error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(registry->allocation_bytes == before_bytes &&
          registry->runtime_borrows == before_borrows &&
          mc_java_random_profile_allocation_bytes(f->profile) == before_profile &&
          f->resources[side].opens == opens);
    ok = true;
cleanup:
    return ok;
}

typedef enum plant_control {
    PLANT_INPUTS,
    PLANT_NULL_O,
    PLANT_NULL_M,
    PLANT_NULL_DIRT,
    PLANT_NULL_STONE_MATERIAL,
    PLANT_NULL_HEIGHT
} plant_control;

typedef struct plant_case {
    const mc_block *receiver;
    uint8_t below, current, water, solid, light;
    bool placement, lower_neighbors, visible, want;
    plant_control control;
    mc_world_storage_status status;
    const char *declaration, *method;
    unsigned int pc, caller_pc;
} plant_case;

static bool plant_cleared(const mc_world_plant_error *e) {
    const mc_registry_error empty = {.initializer_pc = -1,
                                     .block_id = -1,
                                     .constructor_pc = -1,
                                     .active_initializer_pc = -1,
                                     .item_argument = -1};
    return e->status == MC_WORLD_STORAGE_OK && e->operation == NULL && e->declaring_class == NULL &&
           e->declaring_method == NULL && e->bytecode == 0 && e->caller_class == NULL &&
           e->caller_method == NULL && e->caller_bytecode == 0 &&
           e->cause.status == MC_WORLD_STORAGE_OK && e->cause.operation == NULL &&
           e->cause.system_error == 0 && same_registry(&e->registry, &empty) &&
           cleared(&e->query) && e->material.status == MC_WORLD_STORAGE_OK &&
           e->material.bytecode == 0 && e->material.operation == NULL &&
           same_registry(&e->material.registry, &empty) &&
           e->support.status == MC_WORLD_STORAGE_OK && e->support.bytecode == 0 &&
           e->support.operation == NULL && same_registry(&e->support.registry, &empty);
}

static bool plant_trial(fixture *f, size_t side, const plant_case *t, uint8_t water_id,
                        uint8_t stone_id) {
    bool ok = false, value = true;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    mc_world_chunk *c = mc_world_store_chunk_object(f->stores[side], f->normal[side][0]);
    CHECK(c != NULL && c->f != NULL && c->g != NULL && c->h != NULL && c->f->a != NULL &&
          c->g->a != NULL && c->data->blocks != NULL && r->blocks.m != NULL);
    mc_block *stone = r->blocks.m[stone_id];
    CHECK(stone != NULL && r->blocks.named[2] != NULL);
    const int32_t dx[] = {0, -1, 1, 0, 0}, dz[] = {0, 0, 0, -1, 1};
    size_t indices[10];
    uint8_t saved[10];
    const size_t nibble = block_index(64) / 2u, height = 8u * 16u + 8u;
    const uint8_t sky = c->f->a[nibble], block = c->g->a[nibble], old_height = c->h[height];
    mc_world_chunk_nibble *saved_f = c->f, *saved_g = c->g;
    uint8_t *saved_h = c->h;
    mc_block **saved_m = r->blocks.m;
    bool *saved_o = r->blocks.o;
    mc_block *saved_dirt = r->blocks.named[2];
    mc_material *saved_material = stone->fields.bA;
    const size_t count = f->providers[side].cache->count;
    const size_t insertions = f->providers[side].cache->insertion_count;
    for (size_t p = 0; p < COUNT(dx); ++p) {
        for (size_t level = 0; level < 2; ++level) {
            const size_t i = p * 2u + level;
            indices[i] = ((size_t)(8 + dx[p]) << 11u) | ((size_t)(8 + dz[p]) << 7u) | (63u + level);
            saved[i] = c->data->blocks[indices[i]];
            uint8_t id = 0;
            if (p == 0)
                id = level == 0 ? t->below : t->current;
            else if (level == (t->lower_neighbors ? 0u : 1u)) {
                if ((t->water & (1u << (p - 1u))) != 0)
                    id = water_id;
                else if ((t->solid & (1u << (p - 1u))) != 0)
                    id = stone_id;
            }
            c->data->blocks[indices[i]] = id;
        }
    }
    c->f->a[nibble] = (uint8_t)(sky & 0xf0u);
    c->g->a[nibble] = (uint8_t)((block & 0xf0u) | t->light);
    c->h[height] = t->visible ? 64u : 65u;
    if (t->control == PLANT_NULL_O)
        r->blocks.o = NULL;
    if (t->control == PLANT_NULL_M)
        r->blocks.m = NULL;
    if (t->control == PLANT_NULL_DIRT)
        r->blocks.named[2] = NULL;
    if (t->control == PLANT_NULL_STONE_MATERIAL)
        stone->fields.bA = NULL;
    if (t->control == PLANT_NULL_HEIGHT)
        c->h = NULL;
    mc_world_plant_error e;
    const mc_world_storage_status status =
        t->placement ? mc_world_runtime_plant_can_place(w, t->receiver, 8, 64, 8, &value, &e)
                     : mc_world_runtime_plant_survives(w, t->receiver, 8, 64, 8, &value, &e);
    /* Restore every prepared input BEFORE CHECK can leave this scope. The
     * actual shared Chunk.a side effect and World diagnostics are retained. */
    r->blocks.m = saved_m;
    r->blocks.o = saved_o;
    r->blocks.named[2] = saved_dirt;
    stone->fields.bA = saved_material;
    c->f = saved_f;
    c->g = saved_g;
    c->h = saved_h;
    c->f->a[nibble] = sky;
    c->g->a[nibble] = block;
    c->h[height] = old_height;
    for (size_t i = 0; i < COUNT(saved); ++i)
        c->data->blocks[indices[i]] = saved[i];
    CHECK(status == t->status && value == t->want && w->active_calls == 0 &&
          f->providers[side].cache->count == count &&
          f->providers[side].cache->insertion_count == insertions &&
          mc_chunk_pending_contains(f->providers[side].pending, mc_chunk_cache_key(0, 0)) &&
          mc_world_runtime_chunk_light_seen(f->classes[side]));
    if (status == MC_WORLD_STORAGE_OK)
        CHECK(plant_cleared(&e));
    else {
        CHECK(e.bytecode == t->pc && e.caller_bytecode == t->caller_pc &&
              e.declaring_class != NULL && strcmp(e.declaring_class, t->declaration) == 0 &&
              e.declaring_method != NULL && strcmp(e.declaring_method, t->method) == 0 &&
              same_cause(&e.cause, mc_world_runtime_last_error(w)));
        if (t->caller_pc != 0) {
            const char *expected =
                t->caller_pc == 20 ? (side == 0 ? "or" : "je") : (side == 0 ? "wb" : "nw");
            const char *method = t->caller_pc == 35 ? (side == 0 ? "g" : "f") : "a";
            CHECK(e.caller_class != NULL && strcmp(e.caller_class, expected) == 0 &&
                  e.caller_method != NULL && strcmp(e.caller_method, method) == 0);
        }
        if (t->control == PLANT_NULL_HEIGHT)
            CHECK(e.query.bytecode == 19 && e.query.chunk_bytecode == 10 &&
                  e.registry.status == MC_REGISTRY_OK);
        else
            CHECK(e.registry.status == MC_REGISTRY_NULL_REFERENCE &&
                  e.registry.initializer_pc == -1 && e.registry.constructor_pc == -1 &&
                  e.registry.active_initializer_pc == -1 &&
                  same_registry(&e.registry, mc_world_runtime_last_registry_error(w)));
        if (t->control == PLANT_NULL_M && !t->placement)
            CHECK(e.material.bytecode == 23 && same_registry(&e.material.registry, &e.registry));
    }
    ok = true;
cleanup:
    return ok;
}

static bool plant_predicate_contracts(fixture *f, size_t side) {
    bool ok = false, value = true;
    const char *const names[] = {"ae", "Y", "Z",  "ag", "aY", "aW", "bb",
                                 "v",  "w", "aB", "u",  "F",  "C"};
    const mc_block *b[COUNT(names)];
    for (size_t i = 0; i < COUNT(names); ++i)
        CHECK(mc_registry_block_static_get(f->registry[side], names[i], &b[i], NULL) ==
                  MC_REGISTRY_OK &&
              b[i] != NULL && b[i]->fields.bn > 0 && b[i]->fields.bn <= UINT8_MAX);
    CHECK(f->registry[side]->blocks.named[2] == b[8] &&
          mc_world_runtime_chunk_light_seen(f->classes[side]));
    const uint8_t grass = (uint8_t)b[7]->fields.bn, dirt = (uint8_t)b[8]->fields.bn;
    const uint8_t farm = (uint8_t)b[9]->fields.bn, stone = (uint8_t)b[10]->fields.bn;
    const uint8_t sand = (uint8_t)b[11]->fields.bn, water = (uint8_t)b[12]->fields.bn;
    const uint8_t reed = (uint8_t)b[4]->fields.bn, cactus = (uint8_t)b[5]->fields.bn;
    const char *wb = side == 0 ? "wb" : "nw", *tl = side == 0 ? "tl" : "md";
    const char *cc = side == 0 ? "or" : "je", *base = side == 0 ? "uu" : "na";
    const char *survive = side == 0 ? "g" : "f", *soil = side == 0 ? "d" : "c";
    const plant_case trials[] = {
        {.receiver = b[0], .below = grass, .light = 8, .want = true},
        {.receiver = b[0], .below = dirt, .light = 8, .want = true},
        {.receiver = b[0], .below = farm, .light = 8, .want = true},
        {.receiver = b[0], .below = stone, .light = 8},
        {.receiver = b[0], .below = grass, .placement = true, .want = true},
        {.receiver = b[0],
         .below = grass,
         .placement = true,
         .want = true,
         .control = PLANT_NULL_M},
        {.receiver = b[0], .below = grass, .light = 7},
        {.receiver = b[0], .below = grass, .visible = true, .want = true},
        {.receiver = b[0], .below = grass, .light = 8, .want = true, .control = PLANT_NULL_HEIGHT},
        {.receiver = b[1], .below = farm, .light = 8, .want = true},
        {.receiver = b[1], .below = stone, .light = 8},
        {.receiver = b[1], .below = farm, .placement = true, .want = true},
        {.receiver = b[2], .below = sand, .light = 8, .want = true},
        {.receiver = b[2], .below = grass, .light = 8},
        {.receiver = b[2], .below = sand, .placement = true, .want = true},
        {.receiver = b[2], .below = grass, .placement = true},
        {.receiver = b[3], .below = stone, .light = 12, .want = true},
        {.receiver = b[3], .below = stone, .light = 13},
        {.receiver = b[3], .below = water, .light = 12},
        {.receiver = b[3], .below = stone, .light = 13, .placement = true, .want = true},
        {.receiver = b[3],
         .below = stone,
         .current = stone,
         .placement = true,
         .control = PLANT_NULL_O},
        {.receiver = b[4], .below = reed, .want = true},
        {.receiver = b[4], .below = grass, .lower_neighbors = true},
        {.receiver = b[4], .below = dirt, .water = 8, .lower_neighbors = true, .want = true},
        {.receiver = b[4], .below = sand, .water = 15, .lower_neighbors = true},
        {.receiver = b[5], .below = cactus, .want = true},
        {.receiver = b[5], .below = sand, .placement = true, .want = true},
        {.receiver = b[5], .below = dirt},
        {.receiver = b[6], .below = stone, .placement = true, .want = true},
        {.receiver = b[6], .below = stone, .current = water, .placement = true, .want = true},
        {.receiver = b[6], .below = stone, .current = stone, .placement = true},
        {.receiver = b[6], .below = cactus, .placement = true},
        {.receiver = b[6], .want = true},
        {.receiver = b[0],
         .below = grass,
         .want = true,
         .control = PLANT_NULL_HEIGHT,
         .status = MC_WORLD_STORAGE_UNSUPPORTED_STATE,
         .declaration = wb,
         .method = survive,
         .pc = 18},
        {.receiver = b[0],
         .below = dirt,
         .light = 8,
         .want = true,
         .control = PLANT_NULL_DIRT,
         .status = MC_WORLD_STORAGE_UNSUPPORTED_STATE,
         .declaration = wb,
         .method = soil,
         .pc = 14,
         .caller_pc = 35},
        {.receiver = b[3],
         .below = stone,
         .placement = true,
         .want = true,
         .control = PLANT_NULL_O,
         .status = MC_WORLD_STORAGE_UNSUPPORTED_STATE,
         .declaration = tl,
         .method = soil,
         .pc = 4,
         .caller_pc = 23},
        {.receiver = b[0],
         .current = stone,
         .placement = true,
         .want = true,
         .control = PLANT_NULL_M,
         .status = MC_WORLD_STORAGE_UNSUPPORTED_STATE,
         .declaration = base,
         .method = "a",
         .pc = 20,
         .caller_pc = 6},
        {.receiver = b[5],
         .below = cactus,
         .solid = 1,
         .want = true,
         .control = PLANT_NULL_M,
         .status = MC_WORLD_STORAGE_UNSUPPORTED_STATE,
         .declaration = cc,
         .method = survive,
         .pc = 7},
        {.receiver = b[5],
         .below = cactus,
         .solid = 1,
         .placement = true,
         .want = true,
         .control = PLANT_NULL_STONE_MATERIAL,
         .status = MC_WORLD_STORAGE_UNSUPPORTED_STATE,
         .declaration = cc,
         .method = survive,
         .pc = 10,
         .caller_pc = 20},
        {.receiver = b[4],
         .below = grass,
         .water = 2,
         .solid = 1,
         .lower_neighbors = true,
         .want = true,
         .control = PLANT_NULL_STONE_MATERIAL},
    };
    for (size_t i = 0; i < COUNT(trials); ++i)
        CHECK(plant_trial(f, side, &trials[i], water, stone));
    for (unsigned int direction = 0; direction < 4; ++direction) {
        const plant_case reed_case = {.receiver = b[4],
                                      .below = grass,
                                      .water = (uint8_t)(1u << direction),
                                      .lower_neighbors = true,
                                      .placement = true,
                                      .want = true};
        const plant_case cactus_case = {
            .receiver = b[5], .below = sand, .solid = (uint8_t)(1u << direction)};
        CHECK(plant_trial(f, side, &reed_case, water, stone));
        CHECK(plant_trial(f, side, &cactus_case, water, stone));
    }
    for (unsigned int direction = 1; direction < 4; ++direction) {
        const unsigned int invoke_pc[] = {10, 28, 46, 64};
        const plant_case later_null = {.receiver = b[5],
                                       .below = sand,
                                       .water = (uint8_t)((1u << direction) - 1u),
                                       .solid = (uint8_t)(1u << direction),
                                       .want = true,
                                       .control = PLANT_NULL_STONE_MATERIAL,
                                       .status = MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                       .declaration = cc,
                                       .method = survive,
                                       .pc = invoke_pc[direction]};
        CHECK(plant_trial(f, side, &later_null, water, stone));
    }
    mc_world_plant_error e;
    mc_world_runtime *w = f->worlds[side];
    const mc_world_storage_error previous = *mc_world_runtime_last_error(w);
    CHECK(mc_world_runtime_plant_survives(w, b[10], 8, 64, 8, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value && e.declaring_class == NULL &&
          same_cause(&previous, mc_world_runtime_last_error(w)));
    const mc_block *foreign;
    CHECK(mc_registry_block_static_get(f->registry[1u - side], "ae", &foreign, NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_world_runtime_plant_can_place(w, foreign, 8, 64, 8, &value, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value && e.declaring_class == NULL &&
          same_cause(&previous, mc_world_runtime_last_error(w)));
    CHECK(mc_world_runtime_plant_can_place(w, b[0], 8, 64, 8, NULL, &e) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          e.declaring_class == NULL);
    CHECK(mc_world_runtime_plant_survives(w, NULL, 8, 64, 8, &value, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value && e.declaring_class == NULL && w->active_calls == 0);
    for (size_t phase = 0; phase < 2; ++phase) {
        mc_world_runtime *raw = f->worlds[4u + phase * 2u + side];
        value = true;
        CHECK(raw != NULL &&
              mc_world_runtime_plant_survives(raw, b[3], 8, -1, 8, &value, &e) ==
                  MC_WORLD_STORAGE_OK &&
              !value && plant_cleared(&e));
        CHECK(mc_world_runtime_plant_survives(raw, b[3], 8, 128, 8, &value, &e) ==
                  MC_WORLD_STORAGE_OK &&
              !value && plant_cleared(&e));
        CHECK(mc_world_runtime_plant_survives(raw, b[6], INT32_MIN, INT32_MAX, INT32_MAX, &value,
                                              &e) == MC_WORLD_STORAGE_OK &&
              value && plant_cleared(&e));
        CHECK(mc_world_runtime_plant_can_place(raw, b[6], INT32_MIN, 64, INT32_MIN, &value, &e) ==
                  MC_WORLD_STORAGE_OK &&
              !value && plant_cleared(&e));
        value = true;
        CHECK(mc_world_runtime_plant_survives(raw, b[0], 8, -1, 8, &value, &e) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              value && e.bytecode == 18 && e.query.bytecode == 7 &&
              e.query.provider_bytecode == 6 && e.registry.status == MC_REGISTRY_OK &&
              raw->active_calls == 0);
    }
    ok = true;
cleanup:
    return ok;
}

static bool nullable_block_provider(fixture *f, size_t side) {
    bool ok = false, changed = true;
    mc_world_runtime *w = f->worlds[side];
    mc_chunk_provider *provider = w->fields.v;
    int32_t block = -123, metadata = -456, outside = -789;
    mc_world_storage_error block_error, metadata_error, write_error, outside_error;
    w->fields.v = NULL;
    const mc_world_storage_status b = mc_world_runtime_block_get(w, 8, 64, 8, &block, &block_error);
    const mc_world_storage_status m =
        mc_world_runtime_metadata_get(w, 8, 64, 8, &metadata, &metadata_error);
    const mc_world_storage_status s =
        mc_world_runtime_metadata_set_quiet(w, 8, 64, 8, 7, &changed, &write_error);
    const mc_world_storage_status o =
        mc_world_runtime_block_get(w, 8, 128, 8, &outside, &outside_error);
    w->fields.v = provider;
    CHECK(b == MC_WORLD_STORAGE_UNSUPPORTED_STATE && m == b && s == b && block == -123 &&
          metadata == -456 && changed && block_error.status == b && metadata_error.status == b &&
          write_error.status == b);
    CHECK(o == MC_WORLD_STORAGE_OK && outside == 0 && outside_error.status == o &&
          outside_error.operation == NULL && w->active_calls == 0 && w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static size_t block_light_index(int32_t x, int32_t y, int32_t z) {
    return ((size_t)(uint32_t)x << 11u) | ((size_t)(uint32_t)z << 7u) | (size_t)(uint32_t)y;
}

static void block_light_nibble(mc_world_chunk_nibble *owner, size_t cell, uint8_t value) {
    const unsigned int shift = (unsigned int)(cell & 1u) * 4u;
    const uint32_t mask = 15u << shift;
    owner->a[cell / 2u] =
        (uint8_t)(((uint32_t)owner->a[cell / 2u] & ~mask) | ((uint32_t)value << shift));
}

static bool block_light_contracts(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    mc_world_chunk *c = mc_world_store_chunk_object(f->stores[side], f->normal[side][0]);
    mc_world_chunk *other = mc_world_store_chunk_object(f->stores[side], f->normal[side][1]);
    CHECK(c != NULL && other != NULL && c->f != NULL && c->g != NULL && other->g != NULL);
    const char *const names[] = {"al", "aB", "aI", "au", "u"};
    const mc_block *blocks[COUNT(names)];
    size_t named[COUNT(names)];
    for (size_t i = 0; i < COUNT(names); ++i) {
        CHECK(mc_registry_block_static_get(r, names[i], &blocks[i], NULL) == MC_REGISTRY_OK &&
              blocks[i] != NULL && blocks[i]->fields.bn > 0 && blocks[i]->fields.bn <= UINT8_MAX);
        for (named[i] = 0; named[i] < COUNT(r->blocks.named); ++named[i]) {
            if (strcmp(mc_registry_block_static_name(named[i]), names[i]) == 0)
                break;
        }
        CHECK(named[i] < COUNT(r->blocks.named) && r->blocks.named[named[i]] == blocks[i]);
    }
    const size_t cell = block_index(64), packed = cell / 2u;
    const uint8_t old_id = c->data->blocks[cell];
    const uint8_t old_sky = c->f->a[packed], old_block = c->g->a[packed];
    const int32_t old_subtraction = w->fields.f;

    const struct {
        uint8_t sky, block;
        int32_t subtraction, light, plant;
    } scalar[] = {{12, 2, 5, 7, 12},         {3, 11, 5, 11, 11},      {0, 0, -5, 5, 0},
                  {15, 1, INT32_MIN, 1, 15}, {0, 0, INT32_MIN, 0, 0}, {15, 4, 15, 4, 15}};

    for (size_t i = 0; i < COUNT(scalar); ++i) {
        int32_t value = 777, plant = 777;
        mc_world_query_error e, pe;
        c->data->blocks[cell] = (uint8_t)blocks[4]->fields.bn;
        block_light_nibble(c->f, cell, scalar[i].sky);
        block_light_nibble(c->g, cell, scalar[i].block);
        w->fields.f = scalar[i].subtraction;
        const mc_world_storage_status status =
            mc_world_runtime_block_light(w, 8, 64, 8, &value, &e);
        const mc_world_storage_status ps = mc_world_runtime_plant_light(w, 8, 64, 8, &plant, &pe);
        c->data->blocks[cell] = old_id;
        c->f->a[packed] = old_sky;
        c->g->a[packed] = old_block;
        w->fields.f = old_subtraction;
        CHECK(status == MC_WORLD_STORAGE_OK && ps == MC_WORLD_STORAGE_OK && cleared(&e) &&
              cleared(&pe) && value == scalar[i].light && plant == scalar[i].plant &&
              w->active_calls == 0 && w->local_leases == 0);
    }
    const size_t high = block_index(127) / 2u;
    const uint8_t high_sky = c->f->a[high], high_block = c->g->a[high];
    c->f->a[high] = UINT8_C(0xb0);
    c->g->a[high] = UINT8_C(0xd0);
    w->fields.f = 3;
    int32_t top = 777;
    mc_world_query_error top_error;
    const mc_world_storage_status top_status =
        mc_world_runtime_block_light(w, 8, INT32_MAX, 8, &top, &top_error);
    c->f->a[high] = high_sky;
    c->g->a[high] = high_block;
    w->fields.f = old_subtraction;
    CHECK(top_status == MC_WORLD_STORAGE_OK && top == 13 && cleared(&top_error));
    /* Each of the four live statics must select all five samples. Rotate the
     * unique maximum across every direction, with center light deliberately
     * larger than any neighbor so a target-cell shortcut cannot pass. */
    const size_t cells[] = {block_light_index(8, 65, 8), block_light_index(9, 64, 8),
                            block_light_index(7, 64, 8), block_light_index(8, 64, 9),
                            block_light_index(8, 64, 7)};
    uint8_t sky[COUNT(cells)], block[COUNT(cells)];
    for (size_t i = 0; i < COUNT(cells); ++i) {
        sky[i] = c->f->a[cells[i] / 2u];
        block[i] = c->g->a[cells[i] / 2u];
    }
    for (size_t kind = 0; kind < 4; ++kind) {
        for (size_t winner = 0; winner < COUNT(cells); ++winner) {
            int32_t value = 777;
            mc_world_query_error e;
            c->data->blocks[cell] = (uint8_t)blocks[kind]->fields.bn;
            block_light_nibble(c->f, cell, 15);
            block_light_nibble(c->g, cell, 15);
            for (size_t i = 0; i < COUNT(cells); ++i) {
                block_light_nibble(c->f, cells[i], i == winner ? 15u : 4u);
                block_light_nibble(c->g, cells[i], 1);
            }
            w->fields.f = 2;
            const mc_world_storage_status status =
                mc_world_runtime_block_light(w, 8, 64, 8, &value, &e);
            c->data->blocks[cell] = old_id;
            c->f->a[packed] = old_sky;
            c->g->a[packed] = old_block;
            for (size_t i = 0; i < COUNT(cells); ++i) {
                c->f->a[cells[i] / 2u] = sky[i];
                c->g->a[cells[i] / 2u] = block[i];
            }
            w->fields.f = old_subtraction;
            CHECK(status == MC_WORLD_STORAGE_OK && value == 13 && cleared(&e) &&
                  w->active_calls == 0 && w->local_leases == 0);
        }
    }
    /* An earlier match must skip later static dereferences. Conversely, even
     * negative y reaches every nonmatching static before its ordinary zero. */
    mc_block *saved_static = r->blocks.named[named[1]];
    r->blocks.named[named[1]] = NULL;
    c->data->blocks[cell] = (uint8_t)blocks[0]->fields.bn;
    int32_t value = 777;
    mc_world_query_error e;
    mc_world_storage_status status = mc_world_runtime_block_light(w, 8, 64, 8, &value, &e);
    r->blocks.named[named[1]] = saved_static;
    c->data->blocks[cell] = old_id;
    CHECK(status == MC_WORLD_STORAGE_OK && cleared(&e));
    const unsigned int field_pc[] = {46, 57, 68, 79};
    for (size_t i = 0; i < COUNT(field_pc); ++i) {
        mc_chunk_provider *provider = w->fields.v;
        saved_static = r->blocks.named[named[i]];
        r->blocks.named[named[i]] = NULL;
        w->fields.v = NULL;
        value = 777;
        status = mc_world_runtime_block_light(w, 8, INT32_MIN, 8, &value, &e);
        r->blocks.named[named[i]] = saved_static;
        w->fields.v = provider;
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value == 777 &&
              failure_is(w, &e, field_pc[i], 0, MC_REGISTRY_NULL_REFERENCE) &&
              w->active_calls == 0 && w->local_leases == 0);
    }
    /* All later samples remain required when the first already returns 15.
     * The second sample crosses into the other genuinely loaded Chunk, whose
     * BlockLight receiver fails after the completed first sample. */
    const size_t edge_cell = block_light_index(15, 64, 8);
    const uint8_t edge_id = c->data->blocks[edge_cell];
    const uint8_t edge_sky = c->f->a[edge_cell / 2u];
    const uint8_t edge_block = c->g->a[edge_cell / 2u];
    mc_world_chunk_nibble *saved_g = other->g;
    c->data->blocks[edge_cell] = (uint8_t)blocks[0]->fields.bn;
    block_light_nibble(c->f, edge_cell + 1u, 15);
    block_light_nibble(c->g, edge_cell + 1u, 0);
    other->g = NULL;
    w->fields.f = 0;
    value = 777;
    status = mc_world_runtime_block_light(w, 15, 64, 8, &value, &e);
    c->data->blocks[edge_cell] = edge_id;
    c->f->a[edge_cell / 2u] = edge_sky;
    c->g->a[edge_cell / 2u] = edge_block;
    other->g = saved_g;
    w->fields.f = old_subtraction;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value == 777 &&
          failure_is(w, &e, 239, 35, MC_REGISTRY_OK) &&
          mc_world_runtime_chunk_light_seen(f->classes[side]) && w->active_calls == 0 &&
          w->local_leases == 0 &&
          mc_world_store_entity_containers_validate(f->stores[side], w, NULL) ==
              MC_WORLD_STORAGE_OK);
    mc_chunk_provider *provider = w->fields.v;
    w->fields.v = NULL;
    value = 777;
    status = mc_world_runtime_block_light(w, 8, 128, 8, &value, &e);
    w->fields.v = provider;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && value == 777 && e.bytecode == 215 &&
          e.provider_bytecode == 6 && e.chunk_bytecode == 0);

    const struct {
        int32_t x, y, z, expected;
        bool inside;
    } borders[] = {{-32000001, INT32_MIN, 8, 15, false}, {32000000, INT32_MIN, 8, 15, false},
                   {8, INT32_MIN, -32000001, 15, false}, {8, INT32_MIN, 32000001, 15, false},
                   {-32000000, 64, 8, 777, true},        {31999999, 64, 8, 777, true},
                   {8, 64, -32000000, 777, true},        {8, 64, 32000000, 777, true},
                   {8, INT32_MIN, 8, 0, false}};

    for (size_t i = 0; i < COUNT(borders); ++i) {
        w->fields.v = NULL;
        value = 777;
        status =
            mc_world_runtime_block_light(w, borders[i].x, borders[i].y, borders[i].z, &value, &e);
        w->fields.v = provider;
        CHECK(value == borders[i].expected && w->active_calls == 0 && w->local_leases == 0);
        if (borders[i].inside)
            CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && e.bytecode == 36);
        else
            CHECK(status == MC_WORLD_STORAGE_OK && cleared(&e));
    }
    w->fields.f = INT32_MIN;
    status = mc_world_runtime_block_light(w, 40, INT32_MAX, 8, &value, &e);
    w->fields.f = old_subtraction;
    CHECK(status == MC_WORLD_STORAGE_OK && value == 0 && cleared(&e) &&
          mc_chunk_pending_contains(f->providers[side].pending, mc_chunk_cache_key(0, 0)));
    ok = true;
cleanup:
    return ok;
}

#include "population/lake_spring_contracts.h"
#include "population/ore_clay_contracts.h"
#include "population/plants_contracts.h"
#include "population/trees_contracts.h"

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'Q', 'u', 'e', 'r', 'y'};
    if (argc != 3 || strcmp(argv[1], "world-queries") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t slot = 0; slot < COUNT(f->stores); ++slot) {
        CHECK(open_store(f, slot, root) && seed_record(f, slot, 0));
        if (slot < 2)
            CHECK(seed_record(f, slot, 1));
        CHECK(image(f, root, slot, false, true));
        if (slot < 2)
            CHECK(seed_record(f, slot, -1) && image(f, root, slot, true, true));
    }
    for (size_t side = 0; side < COUNT(f->registry); ++side) {
        resource *r = &f->resources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = (stream){.bytes = language};
        r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        r->registry = f->registry[side];
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        r->classes = f->classes[side];
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(create_world(f, side));
        CHECK(cold_paths(f, side) && capture_and_pending(f, side) && top_contracts(f, side) &&
              light_and_height(f, side) && plant_flag(f, side) && denied_native_policy(f, side));
    }
    for (size_t side = 0; side < 2; ++side)
        CHECK(prefixes_and_admission(f, side));
    for (size_t side = 0; side < 2; ++side)
        CHECK(detailed_material_contracts(f, side) && captured_o_contracts(f, side) &&
              captured_p_contracts(f, side));
    for (size_t side = 0; side < 2; ++side)
        CHECK(plant_predicate_contracts(f, side));
    for (size_t side = 0; side < 2; ++side)
        CHECK(nullable_block_provider(f, side));
    for (size_t side = 0; side < 2; ++side)
        CHECK(block_light_contracts(f, side));
    for (size_t side = 0; side < 2; ++side)
        CHECK(population_ore_clay_contracts(f, side));
    for (size_t side = 0; side < 2; ++side)
        CHECK(population_lake_spring_contracts(f, side));
    for (size_t side = 0; side < 2; ++side)
        CHECK(population_tree_contracts(f, side));
    for (size_t side = 0; side < 2; ++side)
        CHECK(population_plant_contracts(f, side));
    for (size_t slot = 0; slot < COUNT(f->stores); ++slot) {
        CHECK(image(f, root, slot, false, false));
        if (slot < 2)
            CHECK(image(f, root, slot, true, false));
    }
    result = 0;
cleanup:
    if (!cleanup_fixture(f, result == 0)) {
        fputs("query cleanup refused; live dependencies retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u World query checks (world-queries)\n", checks) < 0)
        result = 1;
    return result;
}
