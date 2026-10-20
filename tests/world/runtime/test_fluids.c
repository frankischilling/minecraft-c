#include "mc/block/registry/runtime.h"
#include "mc/entity/entity.h"
#include "mc/resources/translation/translation.h"
#include "mc/storage/region/region.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/scheduling.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/platform/time/millis.h"
#include "../../../src/world/runtime/construction_internal.h"
#include "../../../src/world/runtime/mutation/mutation_internal.h"
#include "../../../src/world/runtime/provider_internal.h"
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

/* Native constructed fluid contracts, not original observations. Every World,
 * Registry/Entity class, Chunk and queue is genuinely constructed. All fixed
 * block/metadata input below is serialized BEFORE loading; source effects and
 * shared om/jc scratch are never reset for the next scenario. The only later
 * temporary source-field control is the actual World immediate flag. */
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
typedef struct child {
    mc_world_store *store;
    mc_chunk_state *state;
} child;
typedef struct fixture {
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_world_regions *regions;
    mc_world_store *stores[4];
    mc_world_info_name *name;
    resources sources[2];
    mc_registry_classes *registry[2];
    mc_entity_classes *entities[2];
    mc_world_runtime_classes *classes[2];
    mc_world_runtime *worlds[4];
    mc_world_dimension *dimensions[4];
    mc_world_provider_fields providers[4];
    bool bound[4];
    child children[32];
    mc_chunk_state *chunks[4][2][2];
} fixture;
typedef struct fixture_record {
    mc_nbt_tag root, level, fields[11];
    const mc_nbt_tag *root_fields[1], *level_fields[11];
    uint16_t names[12][20];
    uint8_t arrays[5][MC_CHUNK_BLOCKS];
} fixture_record;

static size_t cell(int32_t x, int32_t y, int32_t z)
{
    return (((size_t)(uint32_t)x & 15u) << 11u) | (((size_t)(uint32_t)z & 15u) << 7u) | (size_t)y;
}

static bool same_cause(const mc_world_storage_error *a, const mc_world_storage_error *b)
{
    return a->status == b->status && a->operation == b->operation &&
           a->system_error == b->system_error;
}

/* Common ownership/transport helpers below are copied from the frozen native
 * support test's genuine fixture, not from the fluid implementation. */

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

static bool region_path(fixture *f, size_t slot, char **out)
{
    return mc_ws_path_join(mc_world_store_root(f->stores[slot]), "region/r.0.0.mcr", out, NULL) ==
           MC_WORLD_STORAGE_OK;
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

enum pattern {
    SOURCE,
    BELOW,
    ROUTE,
    REGENERATE,
    NO_REGENERATE,
    CONVERT,
    STILL_LAVA,
    REENTER,
    LAVA_DELAY,
    LAVA_CHANGE,
    MIX_SOURCE,
    MIX_LEVEL,
    MIX_DEEP,
    DROP_FRONTIER,
    QUOTA,
    FIRE_FRONTIER,
    CACHE_FRONTIER,
    NULL_RANDOM
};
typedef struct patch {
    int32_t x, y, z;
    enum pattern kind;
} patch;
static const patch patches[] = {
    /* Normal cells and their immediate neighbors need the complete source
     * radius-eight footprint inside loaded coordinates0..31. The separate
     * CACHE_FRONTIER deliberately retains its missing negative Chunk. */
    {9, 24, 9, SOURCE},          {9, 40, 9, BELOW},           {9, 56, 9, ROUTE},
    {22, 24, 9, REGENERATE},     {22, 40, 9, NO_REGENERATE},  {22, 56, 9, CONVERT},
    {22, 72, 9, STILL_LAVA},     {22, 88, 9, REENTER},        {9, 72, 9, LAVA_DELAY},
    {9, 88, 9, LAVA_CHANGE},     {9, 24, 22, MIX_SOURCE},     {9, 40, 22, MIX_LEVEL},
    {9, 56, 22, MIX_DEEP},       {9, 72, 22, DROP_FRONTIER},  {22, 24, 22, QUOTA},
    {22, 88, 22, FIRE_FRONTIER}, {1, 104, 8, CACHE_FRONTIER}, {22, 104, 22, NULL_RANDOM}};

static const patch *location(enum pattern kind)
{
    for (size_t i = 0; i < COUNT(patches); ++i)
        if (patches[i].kind == kind)
            return &patches[i];
    return NULL;
}

typedef struct input_cell {
    uint8_t id, metadata;
} input_cell;

/* Constructed stored inputs only. No caller runs this over live Chunk storage. */
static input_cell prepared(int32_t x, int32_t y, int32_t z)
{
    input_cell v = {0};
    for (size_t i = 0; i < COUNT(patches); ++i) {
        const patch *p = &patches[i];
        const int32_t dx = x - p->x, dz = z - p->z;
        if (y == p->y - 1 && dx >= -5 && dx <= 5 && dz >= -5 && dz <= 5)
            v = (input_cell){1, 0};
        if ((p->kind == BELOW && dx == 0 && dz == 0 && y == p->y - 1) ||
            ((p->kind == ROUTE || p->kind == DROP_FRONTIER) && dx == -1 && dz == 0 &&
             y == p->y - 1))
            v = (input_cell){0};
        if (p->kind == BELOW && dx == 0 && dz == 0 && y == p->y - 2)
            v = (input_cell){1, 0};
        if (p->kind == NO_REGENERATE && dx == 0 && dz == 0 && y == p->y - 1)
            v = (input_cell){9, 0};
        if (p->kind == STILL_LAVA && y == p->y + 1 && dx >= -1 && dx <= 1 && dz >= -1 && dz <= 1)
            v = (input_cell){1, 0};
        if (p->kind == FIRE_FRONTIER && y == p->y + 2 && dx >= -1 && dx <= 1 && dz >= -1 && dz <= 1)
            v = (input_cell){5, 0};
        if (y != p->y)
            continue;
        const bool cardinal =
            (dx == 0 && (dz == -1 || dz == 1)) || (dz == 0 && (dx == -1 || dx == 1));
        if (cardinal && p->kind != ROUTE && p->kind != DROP_FRONTIER && p->kind != CACHE_FRONTIER)
            v = (input_cell){1, 0};
        if ((p->kind == REGENERATE || p->kind == NO_REGENERATE) && dz == 0 && (dx == -1 || dx == 1))
            v = (input_cell){9, 0};
        if ((p->kind == LAVA_DELAY || p->kind == LAVA_CHANGE || p->kind == NULL_RANDOM) &&
            dx == -1 && dz == 0)
            v = (input_cell){11, (uint8_t)(p->kind == LAVA_DELAY ? 0u : 1u)};
        if ((p->kind == CONVERT || p->kind == REENTER || p->kind == QUOTA) && dx == 1 && dz == 0)
            v = (input_cell){0};
        if ((p->kind == MIX_SOURCE || p->kind == MIX_LEVEL || p->kind == MIX_DEEP) && dx == 1 &&
            dz == 0)
            v = (input_cell){9, 0};
        if (p->kind == DROP_FRONTIER && dx == -1 && dz == 0)
            v = (input_cell){37, 0};
        if (dx != 0 || dz != 0)
            continue;
        switch (p->kind) {
            case REGENERATE:
            case NO_REGENERATE:
                v = (input_cell){8, 1};
                break;
            case CONVERT:
            case QUOTA:
                v = (input_cell){9, 3};
                break;
            case REENTER:
                v = (input_cell){9, 0};
                break;
            case STILL_LAVA:
            case FIRE_FRONTIER:
                v = (input_cell){11, 0};
                break;
            case LAVA_DELAY:
            case LAVA_CHANGE:
            case NULL_RANDOM:
                v = (input_cell){10, 1};
                break;
            case MIX_SOURCE:
            case MIX_LEVEL:
                v = (input_cell){0};
                break;
            case MIX_DEEP:
                v = (input_cell){10, 5};
                break;
            default:
                v = (input_cell){8, 0};
                break;
        }
    }
    return v;
}

static bool open_store(fixture *f, size_t slot, const char *root)
{
    bool ok = false;
    char path[8192];
    uint8_t map[256];
    const uint16_t title[] = {'F', 'l', 'u', 'i', 'd'};
    const int n = snprintf(path, sizeof(path), "%s/world%zu", root, slot);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options options = {.session_token = 127,
                                      .block_map = map,
                                      .block_map_length = sizeof(map),
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = 4;
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

static bool create_world(fixture *f, size_t slot)
{
    bool ok = false;
    mc_world_runtime_construction state;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 4;
    limits.max_scheduled_entries = slot < 2 ? 128u : 0u;
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
    CHECK(mc_world_provider_bind_fields(f->worlds[slot]->fields.v, &f->providers[slot], NULL) ==
          MC_WORLD_STORAGE_OK);
    f->bound[slot] = true;
    CHECK(f->providers[slot].cache->count == 0 && f->providers[slot].cache->capacity == 4);
    ok = true;
cleanup:
    return ok;
}

static bool seed_record(fixture *f, size_t slot, int32_t cx, int32_t cz)
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
    r->fields[0].value.int_value = cx;
    r->fields[1].value.int_value = cz;
    r->fields[2].value.long_value = -123;
    r->fields[8].value.byte_value = 1;
    bool prepared_ok = true;
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z)
            for (int32_t y = 0; y < 128; ++y) {
                const input_cell v = prepared(cx * 16 + x, y, cz * 16 + z);
                r->arrays[0][cell(x, y, z)] = v.id;
                if (!mc_chunk_nibbles_set(r->arrays[1], MC_CHUNK_NIBBLE_BYTES, x, y, z, v.metadata))
                    prepared_ok = false;
            }
    CHECK(prepared_ok);
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
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[slot]), cx, cz, encoded,
                                 length, fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool load_square(fixture *f, size_t slot)
{
    bool ok = false;
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z)
            CHECK(seed_record(f, slot, x, z));
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            mc_chunk_state *state = NULL;
            const mc_chunk_acquire_status status =
                mc_world_runtime_get(f->worlds[slot], x, z, &state);
            CHECK(remember(f, slot));
            CHECK(status == MC_CHUNK_ACQUIRE_OK && state != NULL && state->active &&
                  state->terrain_populated && !state->dirty && state->x == x && state->z == z);
            f->chunks[slot][x][z] = state;
        }
    CHECK(f->providers[slot].cache->count == 4 && f->providers[slot].cache->insertion_count == 4 &&
          mc_world_store_chunk_count(f->stores[slot]) == 4);
    ok = true;
cleanup:
    return ok;
}

typedef struct schedule_state {
    int64_t next;
    int32_t e, f;
} schedule_state;

static bool scheduled(mc_world_runtime *world, schedule_state *out)
{
    mc_world_schedule_class_state state;
    return mc_world_runtime_schedule_class_get(world->classes, &state) == MC_WORLD_STORAGE_OK &&
           mc_scheduled_ordered_size(world->fields.E, &out->e) == MC_SCHEDULED_ORDERED_OK &&
           mc_scheduled_membership_size(world->fields.F, &out->f) == MC_SCHEDULED_MEMBERSHIP_OK &&
           (out->next = state.next, true);
}

static bool delta(mc_world_runtime *world, const schedule_state *before, int64_t requests,
                  int32_t queued)
{
    schedule_state after;
    return scheduled(world, &after) && after.next == mc_java_i64_add(before->next, requests) &&
           after.e == before->e + queued && after.f == before->f + queued;
}

static bool at(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t id,
               int32_t metadata)
{
    int32_t actual_id = -1, actual_metadata = -1;
    return mc_world_runtime_block_get(world, x, y, z, &actual_id, NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_runtime_metadata_get(world, x, y, z, &actual_metadata, NULL) ==
               MC_WORLD_STORAGE_OK &&
           actual_id == id && actual_metadata == metadata;
}

/* A genuine direct virtual call, like a spring's explicit Block operand, not
 * a pretend World scheduler. The receiver and caller-supplied Random are kept;
 * the real World has a hold across dispatch/reentry. No current-ID gate added. */
static mc_world_storage_status tick(fixture *f, size_t slot, int32_t id, const patch *p,
                                    mc_java_random *random, mc_world_mutation_error *error)
{
    mc_world_runtime *world = f->worlds[slot];
    *error = mc_world_mutation_empty_error();
    mc_world_storage_status status = mc_world_runtime_call_enter(world, &error->cause);
    if (status != MC_WORLD_STORAGE_OK) {
        error->status = status;
        return status;
    }
    const mc_block *block = NULL;
    mc_registry_error cause;
    if (mc_registry_block_get(f->registry[slot % 2u], id, &block, &cause) != MC_REGISTRY_OK) {
        (void)mc_world_mutation_registry_fail(world, error, &cause, "fluid test direct receiver");
        status = error->status;
    } else {
        const bool complete =
            mc_world_mutation_block_tick(world, block, p->x, p->y, p->z, random, error);
        status = complete ? MC_WORLD_STORAGE_OK : error->status;
    }
    mc_world_runtime_call_leave(world);
    return status;
}

static mc_java_random math_snapshot(const fixture *f)
{
    return (mc_java_random){
        .state = atomic_load_explicit(&f->profile->math->state, memory_order_relaxed),
        .next_next_gaussian = f->profile->math->next_next_gaussian,
        .have_next_next_gaussian = f->profile->math->have_next_next_gaussian};
}

static bool math_is(const fixture *f, const mc_java_random *expected)
{
    const mc_java_random actual = math_snapshot(f);
    return mc_test_java_random_equal(&actual, expected);
}

static bool source_and_below(fixture *f, size_t slot)
{
    bool ok = false;
    mc_world_runtime *w = f->worlds[slot];
    mc_block *flow = f->registry[slot % 2u]->blocks.m[8];
    bool *const b = flow->subtype.om.b;
    int32_t *const c = flow->subtype.om.c;
    const int32_t count = flow->subtype.om.a;
    const mc_java_random world_random = *w->fields.r;
    const mc_java_random math = math_snapshot(f);
    mc_world_mutation_error error;
    schedule_state before;
    CHECK(b != NULL && c != NULL && scheduled(w, &before));
    const patch *p = location(SOURCE);
    CHECK(tick(f, slot, 8, p, NULL, &error) == MC_WORLD_STORAGE_OK &&
          at(w, p->x, p->y, p->z, 9, 0) && delta(w, &before, 0, 0));
    CHECK(flow->subtype.om.a == count && flow->subtype.om.b == b && flow->subtype.om.c == c);
    for (size_t i = 0; i < 4; ++i)
        CHECK(c[i] == 1000 && b[i]);
    p = location(BELOW);
    mc_java_random passed;
    mc_java_random_seed(&passed, 17);
    const mc_java_random untouched = passed;
    CHECK(scheduled(w, &before));
    CHECK(tick(f, slot, 8, p, &passed, &error) == MC_WORLD_STORAGE_OK &&
          at(w, p->x, p->y - 1, p->z, 8, 8) && at(w, p->x, p->y, p->z, 8, 0) &&
          delta(w, &before, 3, 2) && !w->fields.o);
    mc_scheduled_tick *first = NULL;
    CHECK(mc_scheduled_ordered_first(w->fields.E, &first) == MC_SCHEDULED_ORDERED_OK);
    mc_scheduled_tick_fields fields;
    bool member = false;
    const bool record =
        mc_scheduled_tick_fields_get(first, &fields) == MC_SCHEDULED_TICK_OK &&
        mc_scheduled_membership_contains(w->fields.F, first, &member) == MC_SCHEDULED_MEMBERSHIP_OK;
    mc_scheduled_tick_release(first);
    CHECK(record && member && fields.x == p->x && fields.y == p->y - 1 && fields.z == p->z &&
          fields.block == 8 && fields.due_time == 5 && fields.sequence == before.next);
    CHECK(mc_test_java_random_equal(&passed, &untouched) &&
          mc_test_java_random_equal(w->fields.r, &world_random) && math_is(f, &math));
    ok = true;
cleanup:
    return ok;
}

static bool routes_and_regeneration(fixture *f, size_t slot)
{
    bool ok = false;
    mc_world_runtime *w = f->worlds[slot];
    mc_block *flow = f->registry[slot % 2u]->blocks.m[8];
    bool *const flags = flow->subtype.om.b;
    int32_t *const costs = flow->subtype.om.c;
    mc_java_random passed;
    mc_java_random_seed(&passed, 27);
    const mc_java_random untouched = passed, world_random = *w->fields.r;
    mc_world_mutation_error error;
    schedule_state before;
    const patch *p = location(ROUTE);
    CHECK(scheduled(w, &before));
    CHECK(tick(f, slot, 8, p, &passed, &error) == MC_WORLD_STORAGE_OK &&
          at(w, p->x - 1, p->y, p->z, 8, 1) && at(w, p->x, p->y, p->z, 8, 0) &&
          at(w, p->x + 1, p->y, p->z, 0, 0) && at(w, p->x, p->y, p->z - 1, 0, 0) &&
          at(w, p->x, p->y, p->z + 1, 0, 0) && delta(w, &before, 3, 2));
    CHECK(flow->subtype.om.b == flags && flow->subtype.om.c == costs && costs[0] == 0 && flags[0]);
    for (size_t i = 1; i < 4; ++i)
        CHECK(costs[i] > 0 && !flags[i]);
    p = location(REGENERATE);
    CHECK(scheduled(w, &before));
    CHECK(tick(f, slot, 8, p, &passed, &error) == MC_WORLD_STORAGE_OK &&
          at(w, p->x, p->y, p->z, 8, 0) && at(w, p->x - 1, p->y, p->z, 8, 0) &&
          at(w, p->x + 1, p->y, p->z, 8, 0) && flow->subtype.om.a == 2 && delta(w, &before, 5, 3));
    p = location(NO_REGENERATE);
    CHECK(scheduled(w, &before));
    CHECK(tick(f, slot, 8, p, &passed, &error) == MC_WORLD_STORAGE_OK &&
          at(w, p->x, p->y, p->z, 9, 1) && at(w, p->x, p->y - 1, p->z, 9, 0) &&
          flow->subtype.om.a == 2 && delta(w, &before, 0, 0));
    /* The original else path reads CENTER metadata246, not below metadata.
     * A later level0 direct call does not clear the actual a=2 source count. */
    p = location(SOURCE);
    CHECK(tick(f, slot, 8, p, NULL, &error) == MC_WORLD_STORAGE_OK && flow->subtype.om.a == 2 &&
          at(w, p->x, p->y, p->z, 9, 0) && mc_test_java_random_equal(&passed, &untouched) &&
          mc_test_java_random_equal(w->fields.r, &world_random));
    ok = true;
cleanup:
    return ok;
}

static bool still_conversions(fixture *f, size_t slot)
{
    bool ok = false, value = false;
    mc_world_runtime *w = f->worlds[slot];
    mc_world_mutation_error error;
    schedule_state before;
    const patch *p = location(CONVERT);
    CHECK(scheduled(w, &before));
    CHECK(mc_world_runtime_block_set(w, p->x + 1, p->y, p->z, 1, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value && at(w, p->x, p->y, p->z, 8, 3) && !w->fields.o && delta(w, &before, 2, 1));
    p = location(REENTER);
    const mc_java_random world_random = *w->fields.r;
    const bool saved_a = w->fields.a;
    CHECK(scheduled(w, &before));
    w->fields.a = true;
    const mc_world_storage_status status =
        mc_world_runtime_block_set(w, p->x + 1, p->y, p->z, 1, &value, &error);
    w->fields.a = saved_a;
    CHECK(status == MC_WORLD_STORAGE_OK && value && !w->fields.o && at(w, p->x, p->y, p->z, 9, 0) &&
          delta(w, &before, 2, 0) && mc_test_java_random_equal(w->fields.r, &world_random));
    mc_block *flow = f->registry[slot % 2u]->blocks.m[8];
    for (size_t i = 0; i < 4; ++i)
        CHECK(flow->subtype.om.c[i] == 1000 && flow->subtype.om.b[i]);
    CHECK(w->active_calls == 0 && w->local_leases == 0 && f->providers[slot].cache->count == 4);
    ok = true;
cleanup:
    return ok;
}

static bool lava_randoms(fixture *f, size_t slot)
{
    bool ok = false;
    mc_world_runtime *w = f->worlds[slot];
    const mc_java_random world_random = *w->fields.r;
    const mc_java_random math = math_snapshot(f);
    mc_block *lava = f->registry[slot % 2u]->blocks.m[10];
    mc_world_mutation_error error;
    schedule_state before;
    mc_java_random passed, expected;
    int32_t draw = -1;
    const patch *p = location(LAVA_DELAY);
    mc_java_random_seed(&passed, 0);
    expected = passed;
    CHECK(mc_java_random_next_bounded(&expected, 4, &draw) && draw != 0 && scheduled(w, &before));
    CHECK(tick(f, slot, 10, p, &passed, &error) == MC_WORLD_STORAGE_OK &&
          at(w, p->x, p->y, p->z, 10, 1) && lava->subtype.om.a == 1 && delta(w, &before, 0, 0) &&
          mc_test_java_random_equal(&passed, &expected));
    p = location(NULL_RANDOM);
    CHECK(tick(f, slot, 10, p, NULL, &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.hook_bytecode == 289 && lava->subtype.om.a == 0 && at(w, p->x, p->y, p->z, 10, 1) &&
          delta(w, &before, 0, 0));
    p = location(LAVA_CHANGE);
    mc_java_random_seed(&passed, 4096);
    expected = passed;
    CHECK(mc_java_random_next_bounded(&expected, 4, &draw) && draw == 0 && scheduled(w, &before));
    CHECK(tick(f, slot, 10, p, &passed, &error) == MC_WORLD_STORAGE_OK &&
          at(w, p->x, p->y, p->z, 10, 3) && at(w, p->x - 1, p->y, p->z, 10, 1) &&
          delta(w, &before, 3, 2) && mc_test_java_random_equal(&passed, &expected));
    const int64_t seeds[] = {0, 2, 3};
    bool saw_zero = false, saw_step = false;
    p = location(STILL_LAVA);
    CHECK(scheduled(w, &before));
    for (size_t i = 0; i < COUNT(seeds); ++i) {
        mc_java_random_seed(&passed, seeds[i]);
        expected = passed;
        CHECK(mc_java_random_next_bounded(&expected, 3, &draw));
        if (draw == 0)
            saw_zero = true;
        else {
            int32_t ignored;
            saw_step = true;
            CHECK(mc_java_random_next_bounded(&expected, 3, &ignored) &&
                  mc_java_random_next_bounded(&expected, 3, &ignored));
        }
        CHECK(tick(f, slot, 11, p, &passed, &error) == MC_WORLD_STORAGE_OK &&
              mc_test_java_random_equal(&passed, &expected) && at(w, p->x, p->y, p->z, 11, 0));
    }
    CHECK(saw_zero && saw_step && delta(w, &before, 0, 0) &&
          mc_test_java_random_equal(w->fields.r, &world_random) && math_is(f, &math));
    expected = *w->fields.r;
    CHECK(mc_java_random_next_bounded(&expected, 3, &draw));
    if (draw != 0) {
        int32_t ignored;
        CHECK(mc_java_random_next_bounded(&expected, 3, &ignored) &&
              mc_java_random_next_bounded(&expected, 3, &ignored));
    }
    const mc_java_random unchanged_passed = passed;
    const bool saved_a = w->fields.a;
    mc_world_schedule_error scheduled_error;
    w->fields.a = true;
    const mc_world_storage_status status =
        mc_world_runtime_schedule(w, p->x, p->y, p->z, 11, INT32_MIN, &scheduled_error);
    w->fields.a = saved_a;
    CHECK(status == MC_WORLD_STORAGE_OK && mc_test_java_random_equal(w->fields.r, &expected) &&
          delta(w, &before, 1, 0) && mc_test_java_random_equal(&passed, &unchanged_passed));
    /* Only the actual World.r is used by scheduler142. No cross-owner
     * inequality or prediction of platform-seeded World.r is an oracle. */
    ok = true;
cleanup:
    return ok;
}

static bool mixing(fixture *f, size_t slot)
{
    bool ok = false;
    mc_world_runtime *w = f->worlds[slot];
    const enum pattern kinds[] = {MIX_SOURCE, MIX_LEVEL, MIX_DEEP};
    for (size_t i = 0; i < COUNT(kinds); ++i) {
        const patch *p = location(kinds[i]);
        mc_java_random world_expected = *w->fields.r;
        mc_java_random math_expected = math_snapshot(f);
        (void)mc_java_random_next_float(&world_expected);
        (void)mc_java_random_next_float(&world_expected);
        for (size_t draw = 0; draw < 16; ++draw)
            (void)mc_java_random_next_double(&math_expected);
        const uint64_t u = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
        schedule_state before;
        CHECK(scheduled(w, &before));
        mc_world_mutation_error error;
        bool value = false;
        if (kinds[i] == MIX_DEEP) {
            CHECK(mc_world_runtime_block_set(w, p->x, p->y + 1, p->z, 1, &value, &error) ==
                      MC_WORLD_STORAGE_OK &&
                  value && at(w, p->x, p->y, p->z, 10, 5) && at(w, p->x + 1, p->y, p->z, 9, 0) &&
                  delta(w, &before, 0, 0));
        } else {
            const mc_block *solid = NULL;
            CHECK(mc_registry_block_static_get(f->registry[slot % 2u], i == 0 ? "aq" : "x", &solid,
                                               NULL) == MC_REGISTRY_OK &&
                  solid != NULL);
            CHECK(mc_world_runtime_block_metadata_set_quiet(w, p->x, p->y, p->z, 11, i == 0 ? 0 : 3,
                                                            &value,
                                                            &error) == MC_WORLD_STORAGE_OK &&
                  value && at(w, p->x, p->y, p->z, mc_block_fields_get(solid)->bn, 0) &&
                  at(w, p->x + 1, p->y, p->z, 8, 0) && delta(w, &before, 2, 1));
        }
        CHECK(mc_test_java_random_equal(w->fields.r, &world_expected) &&
              math_is(f, &math_expected) &&
              atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed) == u &&
              !w->fields.o && w->active_calls == 0 && w->local_leases == 0);
    }
    ok = true;
cleanup:
    return ok;
}

static bool frontiers(fixture *f, size_t slot)
{
    bool ok = false;
    mc_world_runtime *w = f->worlds[slot];
    mc_registry_classes *registry = f->registry[slot % 2u];
    mc_block *flow = registry->blocks.m[8];
    mc_world_mutation_error error;
    schedule_state before;
    mc_java_random passed;
    const char *flow_class = slot % 2u == 0 ? "om" : "jc";
    const char *still_class = slot % 2u == 0 ? "nx" : "is";

    /* Genuine cache quota, not an injected array fault: c[0] is written before
     * west distance recursion reaches unloaded chunk(-1,0). All other scratch
     * values are the actual retained values produced by preceding source calls. */
    const patch *p = location(CACHE_FRONTIER);
    bool saved_b[4];
    int32_t saved_c[4];
    memcpy(saved_b, flow->subtype.om.b, sizeof(saved_b));
    memcpy(saved_c, flow->subtype.om.c, sizeof(saved_c));
    bool *const b = flow->subtype.om.b;
    int32_t *const c = flow->subtype.om.c;
    const int32_t count = flow->subtype.om.a;
    mc_java_random_seed(&passed, 27);
    const mc_java_random untouched = passed, random = *w->fields.r;
    CHECK(scheduled(w, &before));
    CHECK(tick(f, slot, 8, p, &passed, &error) == MC_WORLD_STORAGE_LIMIT &&
          error.hook_class != NULL && strcmp(error.hook_class, flow_class) == 0 &&
          error.hook_bytecode == 492 && error.cause_hook_class != NULL &&
          strcmp(error.cause_hook_class, flow_class) == 0 && error.cause_hook_bytecode == 5 &&
          at(w, p->x, p->y, p->z, 9, 0) && delta(w, &before, 0, 0));
    CHECK(flow->subtype.om.a == count && flow->subtype.om.b == b && flow->subtype.om.c == c &&
          c[0] == 1000 && memcmp(b, saved_b, sizeof(saved_b)) == 0 &&
          mc_test_java_random_equal(&passed, &untouched) &&
          mc_test_java_random_equal(w->fields.r, &random));
    for (size_t i = 1; i < 4; ++i)
        CHECK(c[i] == saved_c[i]);
    CHECK(f->providers[slot].cache->count == 4 && mc_world_store_chunk_count(f->stores[slot]) == 4);

    /* Still-lava ignition reaches the actual fire addition barrier after a
     * genuine byte write. The three-by-three wood ceiling makes every first
     * random-walk destination have a real flammable +Y material. */
    p = location(FIRE_FRONTIER);
    mc_java_random_seed(&passed, 2);
    mc_java_random expected = passed;
    int32_t steps = 0, dx = 0, dz = 0;
    CHECK(mc_java_random_next_bounded(&expected, 3, &steps) && steps > 0 &&
          mc_java_random_next_bounded(&expected, 3, &dx) &&
          mc_java_random_next_bounded(&expected, 3, &dz));
    const mc_block *fire = NULL;
    CHECK(mc_registry_block_static_get(registry, "as", &fire, NULL) == MC_REGISTRY_OK &&
          fire != NULL);
    CHECK(scheduled(w, &before));
    CHECK(tick(f, slot, 11, p, &passed, &error) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error.hook_class != NULL && strcmp(error.hook_class, still_class) == 0 &&
          error.hook_bytecode == 165 && error.cause_hook_class != NULL &&
          strcmp(error.cause_hook_class, slot % 2u == 0 ? "yq" : "pt") == 0 &&
          error.cause_hook_bytecode == 0 &&
          at(w, p->x + dx - 1, p->y + 1, p->z + dz - 1, mc_block_fields_get(fire)->bn, 0) &&
          mc_test_java_random_equal(&passed, &expected) && delta(w, &before, 0, 0));

    ok = true;
cleanup:
    return ok;
}

static bool quota_prefix(fixture *f, size_t slot)
{
    bool ok = false, value = true;
    mc_world_runtime *w = f->worlds[slot];
    const patch *p = location(QUOTA);
    schedule_state before;
    mc_world_mutation_error error;
    CHECK(scheduled(w, &before) && before.e == 0 && before.f == 0 && !w->fields.o);
    const mc_world_storage_status status =
        mc_world_runtime_block_set(w, p->x + 1, p->y, p->z, 1, &value, &error);
    const mc_world_storage_error retained_cause = *mc_world_runtime_last_error(w);
    CHECK(status == MC_WORLD_STORAGE_LIMIT && value && error.hook_class != NULL &&
          strcmp(error.hook_class, slot % 2u == 0 ? "nx" : "is") == 0 &&
          error.cause_hook_class != NULL &&
          strcmp(error.cause_hook_class, slot % 2u == 0 ? "om" : "jc") == 0 &&
          error.cause_hook_bytecode == 37 && error.schedule_bytecode == 219 &&
          error.membership == MC_SCHEDULED_MEMBERSHIP_LIMIT && at(w, p->x, p->y, p->z, 8, 3) &&
          at(w, p->x + 1, p->y, p->z, 1, 0) && w->fields.o && delta(w, &before, 1, 0) &&
          same_cause(&error.cause, &retained_cause));
    /* o=true is a genuine failed conversion effect. It is NOT restored. */
    CHECK(w->active_calls == 0 && w->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool check_disk(fixture *f, size_t slot, uint32_t low, uint32_t high)
{
    bool ok = false;
    char *path = NULL;
    mc_ws_file_bytes bytes = {0};
    mc_world_regions *disk = NULL;
    mc_compression_buffer payload = {0};
    mc_nbt_document *document = NULL;
    CHECK(region_path(f, slot, &path));
    CHECK(mc_ws_read_file(path, 1024u * 1024u, &bytes, NULL) == MC_WORLD_STORAGE_OK &&
          bytes.length >= MC_REGION_HEADER_BYTES);
    bool slots_ok = true;
    for (size_t i = 0; i < MC_REGION_ENTRY_COUNT; ++i) {
        const uint32_t location_bits = be32(bytes.data + 4u * i);
        const uint32_t timestamp = be32(bytes.data + MC_REGION_SECTOR_BYTES + 4u * i);
        if (i == 0 || i == 1 || i == 32 || i == 33) {
            if (location_bits == 0 || timestamp < low || timestamp > high ||
                timestamp == fixture_timestamp)
                slots_ok = false;
        } else if (location_bits != 0 || timestamp != 0) {
            slots_ok = false;
        }
    }
    CHECK(slots_ok && mc_world_regions_create(NULL, &disk, NULL) == MC_WORLD_STORAGE_OK);
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            CHECK(mc_world_regions_read(disk, mc_world_store_root(f->stores[slot]), x, z, &payload,
                                        NULL) == MC_WORLD_STORAGE_OK);
            CHECK(mc_nbt_read_compound(payload.data, payload.length, NULL, &document, NULL, NULL) ==
                  MC_NBT_OK);
            const mc_nbt_tag *level = field(mc_nbt_document_root(document), "Level");
            const mc_nbt_tag *update = field(level, "LastUpdate");
            const mc_nbt_tag *blocks = field(level, "Blocks");
            const mc_nbt_tag *metadata = field(level, "Data");
            CHECK(update != NULL && update->type == MC_NBT_LONG &&
                  update->value.long_value == 1000 + (int64_t)slot && blocks != NULL &&
                  blocks->type == MC_NBT_BYTE_ARRAY &&
                  blocks->value.byte_array.length == MC_CHUNK_BLOCKS &&
                  memcmp(blocks->value.byte_array.data, f->chunks[slot][x][z]->data->blocks,
                         MC_CHUNK_BLOCKS) == 0 &&
                  metadata != NULL && metadata->type == MC_NBT_BYTE_ARRAY &&
                  metadata->value.byte_array.length == MC_CHUNK_NIBBLE_BYTES &&
                  memcmp(metadata->value.byte_array.data, f->chunks[slot][x][z]->data->metadata,
                         MC_CHUNK_NIBBLE_BYTES) == 0);
            mc_nbt_document_free(document);
            document = NULL;
            mc_compression_buffer_free(&payload);
            payload = (mc_compression_buffer){0};
        }
    ok = true;
cleanup:
    mc_nbt_document_free(document);
    mc_compression_buffer_free(&payload);
    mc_world_regions_free(disk);
    mc_ws_free_file_bytes(&bytes);
    free(path);
    return ok;
}

static bool maintained(fixture *f)
{
    bool ok = false;
    for (size_t slot = 0; slot < COUNT(f->worlds); ++slot) {
        for (int32_t x = 0; x < 2; ++x)
            for (int32_t z = 0; z < 2; ++z) {
                bool changed = false;
                CHECK(mc_world_runtime_metadata_set_quiet(f->worlds[slot], x * 16 + 1, 1,
                                                          z * 16 + 1, 0, &changed,
                                                          NULL) == MC_WORLD_STORAGE_OK &&
                      changed && f->chunks[slot][x][z]->dirty);
            }
        CHECK(mc_world_info_time_set(f->worlds[slot]->fields.x, 1000 + (int64_t)slot) ==
              MC_WORLD_INFO_OK);
        int64_t before, after;
        CHECK(mc_platform_current_time_millis(&before) == MC_PLATFORM_MILLIS_OK && before >= 0);
        CHECK(drain(f, slot));
        CHECK(mc_platform_current_time_millis(&after) == MC_PLATFORM_MILLIS_OK && after >= before &&
              after / INT64_C(1000) <= UINT32_MAX);
        for (int32_t x = 0; x < 2; ++x)
            for (int32_t z = 0; z < 2; ++z)
                CHECK(!f->chunks[slot][x][z]->active && f->chunks[slot][x][z]->dirty &&
                      f->chunks[slot][x][z]->last_save_time == 1000 + (int64_t)slot);
        CHECK(check_disk(f, slot, (uint32_t)(before / INT64_C(1000)),
                         (uint32_t)(after / INT64_C(1000))));
    }
    ok = true;
cleanup:
    return ok;
}

/* Run after the genuine empty-Entity maintenance round trip. A displaced flower
 * now publishes its real ItemEntity in both source containers and flow resumes.
 * Persistence must refuse this live Entity rather than silently erase it. */
static bool drop_publication(fixture *f, size_t slot)
{
    bool ok = false;
    mc_world_runtime *w = f->worlds[slot];
    const patch *p = location(DROP_FRONTIER);
    char *path = NULL;
    mc_ws_file_bytes saved = {0}, current = {0};
    mc_item_stack *stack = NULL;
    for (int32_t x = 0; x < 2; ++x)
        for (int32_t z = 0; z < 2; ++z) {
            mc_chunk_state *state = NULL;
            const mc_chunk_acquire_status status = mc_world_runtime_get(w, x, z, &state);
            CHECK(remember(f, slot));
            CHECK(status == MC_CHUNK_ACQUIRE_OK && state != NULL);
            f->chunks[slot][x][z] = state;
        }
    CHECK(region_path(f, slot, &path) &&
          mc_ws_read_file(path, 1024u * 1024u, &saved, NULL) == MC_WORLD_STORAGE_OK);
    mc_entity_class_snapshot before_entities = {0}, after_entities = {0};
    CHECK(w->fields.b->count == 0 &&
          mc_entity_classes_get_snapshot(f->entities[slot % 2u], &before_entities) == MC_ENTITY_OK);
    mc_java_random expected = *w->fields.r, passed;
    for (size_t i = 0; i < 4; ++i)
        (void)mc_java_random_next_float(&expected);
    mc_java_random_seed(&passed, 27);
    const mc_java_random untouched = passed;
    schedule_state before;
    mc_world_mutation_error error;
    CHECK(scheduled(w, &before));
    CHECK(tick(f, slot, 8, p, &passed, &error) == MC_WORLD_STORAGE_OK &&
          at(w, p->x, p->y, p->z, 8, 0) && at(w, p->x - 1, p->y, p->z, 8, 1) &&
          delta(w, &before, 3, 2) && mc_test_java_random_equal(w->fields.r, &expected) &&
          mc_test_java_random_equal(&passed, &untouched));
    CHECK(w->fields.b->count == 1 && w->fields.b->items[0] != NULL);
    mc_entity *entity = w->fields.b->items[0];
    mc_entity_snapshot snapshot = {0};
    mc_item_stack_fields payload = {0};
    CHECK(mc_entity_get_snapshot(entity, &snapshot) == MC_ENTITY_OK &&
          snapshot.phase == MC_ENTITY_READY && snapshot.base.world == w && snapshot.item.c == 10 &&
          snapshot.base.bF && snapshot.base.bG == 0 && snapshot.base.bH == 4 &&
          snapshot.base.bI == 1 && mc_item_entity_stack_get(entity, &stack) == MC_ENTITY_OK &&
          stack == snapshot.stack &&
          mc_item_stack_fields_get(stack, &payload) == MC_ITEM_STACK_OK && payload.id == 37 &&
          payload.count == 1 && payload.damage == 0);
    mc_world_chunk *chunk = mc_world_store_chunk_object(f->stores[slot], f->chunks[slot][0][1]);
    mc_world_chunk_fields fields = {0};
    const mc_world_chunk_entity_list *slice = NULL;
    mc_entity *occurrence = NULL;
    size_t count = SIZE_MAX;
    CHECK(chunk != NULL && mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK &&
          fields.q && mc_world_chunk_slice_at(fields.m, 4, &slice) == MC_WORLD_STORAGE_OK &&
          mc_world_chunk_entity_count(slice, &count) == MC_WORLD_STORAGE_OK && count == 1 &&
          mc_world_chunk_entity_at(slice, 0, &occurrence) == MC_WORLD_STORAGE_OK &&
          occurrence == entity);
    CHECK(mc_entity_classes_get_snapshot(f->entities[slot % 2u], &after_entities) == MC_ENTITY_OK &&
          after_entities.next_id == mc_java_i32_add(before_entities.next_id, 1) &&
          after_entities.live_entities == before_entities.live_entities + 1u &&
          w->active_calls == 0 && w->local_leases == 0);
    uint32_t growth = UINT32_MAX;
    CHECK(mc_world_store_write_runtime_chunk(f->stores[slot], f->chunks[slot][0][1], 2000, 12345,
                                             &growth, NULL) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          growth == UINT32_MAX && f->chunks[slot][0][1]->has_entities &&
          mc_ws_read_file(path, 1024u * 1024u, &current, NULL) == MC_WORLD_STORAGE_OK &&
          current.length == saved.length && memcmp(current.data, saved.data, saved.length) == 0);
    ok = true;
cleanup:
    mc_item_stack_release(stack);
    mc_ws_free_file_bytes(&current);
    mc_ws_free_file_bytes(&saved);
    free(path);
    return ok;
}

static bool cleanup_fixture(fixture *f)
{
    if (f == NULL)
        return true;
    for (size_t i = 0; i < COUNT(f->worlds); ++i) {
        if (f->worlds[i] == NULL)
            continue;
        if (!remember(f, i) ||
            mc_world_runtime_retire_entity_containers(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
        /* Native terminal disposal, without save, source unload or active-bit
         * edits. The actual Entity lists have already released their holds. */
        if (f->bound[i]) {
            mc_chunk_cache *cache = f->providers[i].cache;
            if (mc_chunk_pending_clear(f->providers[i].pending) != MC_CHUNK_ACQUIRE_OK ||
                !mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                                     cache->insertion_capacity))
                return false;
        }
        if (mc_world_runtime_retire_world(f->worlds[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
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
    for (size_t i = 0; i < COUNT(f->entities); ++i)
        if (mc_entity_classes_free(f->entities[i]) != MC_ENTITY_OK)
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

int main(int argc, char **argv)
{
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    const uint16_t title[] = {'F', 'l', 'u', 'i', 'd'};
    if (argc != 3 || strcmp(argv[1], "world-fluids") != 0 ||
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
                                       NULL, &f->entities[side]) == MC_ENTITY_OK);
    }
    CHECK(mc_world_runtime_classes_bind_entities(f->classes[0], f->entities[1]) ==
          MC_ENTITY_INVALID_ARGUMENT);
    for (size_t side = 0; side < 2; ++side) {
        mc_entity_class_snapshot before, after;
        CHECK(mc_entity_classes_get_snapshot(f->entities[side], &before) == MC_ENTITY_OK);
        const uint64_t u = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
        CHECK(mc_world_runtime_classes_bind_entities(f->classes[side], f->entities[side]) ==
              MC_ENTITY_OK);
        CHECK(mc_world_runtime_classes_bind_entities(f->classes[side], f->entities[side]) ==
                  MC_ENTITY_BUSY &&
              mc_entity_classes_free(f->entities[side]) == MC_ENTITY_BUSY);
        CHECK(mc_entity_classes_get_snapshot(f->entities[side], &after) == MC_ENTITY_OK &&
              before.entity_class == MC_ENTITY_CLASS_COLD &&
              after.entity_class == before.entity_class && before.next_id == after.next_id &&
              before.live_entities == after.live_entities &&
              atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed) == u);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              f->sources[side].opens == 3 && f->sources[side].closes == 1 && !f->sources[side].bad);
    }
    for (size_t i = 0; i < COUNT(f->worlds); ++i)
        CHECK(create_world(f, i) && load_square(f, i));
    double warmup;
    CHECK(mc_java_math_random(f->profile, &warmup) == MC_JAVA_RANDOM_PROFILE_OK &&
          atomic_load_explicit(&f->profile->holder_phase, memory_order_acquire) ==
              MC_JAVA_RANDOM_HOLDER_READY &&
          f->profile->math != NULL);
    for (size_t side = 0; side < 2; ++side) {
        CHECK(source_and_below(f, side));
        CHECK(routes_and_regeneration(f, side));
        CHECK(still_conversions(f, side));
        CHECK(lava_randoms(f, side));
        CHECK(mixing(f, side));
        CHECK(frontiers(f, side));
        CHECK(quota_prefix(f, side + 2u));
    }
    CHECK(maintained(f));
    /* Maintenance removed the native cache references. Retire its still-owned
     * empty records before loading copies, preserving the real four-owner cap. */
    for (size_t i = 0; i < COUNT(f->children); ++i)
        if (f->children[i].state != NULL) {
            CHECK(mc_world_store_retire_chunk(f->children[i].store, f->children[i].state, NULL) ==
                  MC_WORLD_STORAGE_OK);
            f->children[i] = (child){0};
        }
    for (size_t side = 0; side < 2; ++side)
        CHECK(drop_publication(f, side));
    result = 0;
cleanup:
    if (!cleanup_fixture(f)) {
        fputs("fluid cleanup refused; live dependencies retained\n", stderr);
        return 1;
    }
    if (result == 0 && printf("PASS: %u World fluid checks (world-fluids)\n", checks) < 0)
        result = 1;
    return result;
}
