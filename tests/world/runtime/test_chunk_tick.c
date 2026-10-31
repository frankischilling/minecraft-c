/* Source-derived captured-Chunk contracts; no original observation replay. */
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/world/chunk/object_state_internal.h"
#include "../../../src/world/runtime/chunk_provider_internal.h"
#include "../../../src/world/runtime/construction_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/storage/files.h"
#include "../../../src/world/storage/objects_internal.h"
#include "mc/resources/translation/translation.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/runtime/chunk_tick.h"
#include "mc/world/runtime/entities.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
static mc_math_trig trig;
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Chunk tick check%u line%d: %s\n", checks, __LINE__, #c);              \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct input {
    const uint8_t *bytes;
    size_t length, offset;
    bool opened, closed;
} input;

typedef struct fixture {
    mc_world_regions *regions;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *registry;
    mc_registry_resources resources;
    input streams[3];
    mc_world_info_name *name;
    mc_world_runtime_classes *classes[2];
    mc_world_store *store[2];
    mc_world_provider *dimension[2];
    mc_world_runtime *world[2];
    mc_chunk_provider_fields provider[2];
    mc_chunk_state *normal[2][2];
    mc_world_chunk_tick_capture *capture[2];
    bool outer_call;
    mc_ws_file_bytes region_before[2];
    char *region_path[2];
} fixture;

static mc_java_properties_status read_input(void *owner, uint8_t *out, size_t capacity,
                                            size_t *count) {
    input *s = owner;
    if (!s || !out || !capacity || !count || !s->opened || s->closed || s->offset > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > capacity)
        take = capacity;
    if (take)
        memcpy(out, s->bytes + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_input(void *owner, const char *path, mc_java_properties_input *out) {
    fixture *f = owner;
    static const uint8_t language[] =
        "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\nstat.breakItem=Break "
        "%1$s\nstat.craftItem=Craft %1$s\n";
    static const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang",
                                        "/achievement/map.txt"};
    size_t index = 0;
    while (index < COUNT(names) && strcmp(path, names[index]))
        ++index;
    if (index == COUNT(names) || f->streams[index].opened)
        return MC_REGISTRY_IO_FAILED;
    f->streams[index] = (input){
        .bytes = language, .length = index == 0 ? sizeof(language) - 1u : 0, .opened = true};
    *out = (mc_java_properties_input){&f->streams[index], read_input};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_input(void *owner, const mc_java_properties_input *in) {
    fixture *f = owner;
    for (size_t i = 0; in && i < COUNT(f->streams); ++i)
        if (in->context == &f->streams[i] && in->read == read_input && f->streams[i].opened) {
            f->streams[i].closed = true;
            return MC_REGISTRY_OK;
        }
    return MC_REGISTRY_INVALID_ARGUMENT;
}

static bool ready_input(void *owner, const mc_java_properties_input *in) {
    (void)owner;
    (void)in;
    return false;
}

typedef struct record {
    mc_nbt_tag root, level, fields[11];
    const mc_nbt_tag *root_fields[1], *level_fields[11];
    uint16_t names[12][20];
    uint8_t arrays[5][MC_CHUNK_BLOCKS];
} record;

static mc_nbt_string ascii(const char *text, uint16_t *units) {
    const size_t n = strlen(text);
    for (size_t i = 0; i < n; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return (mc_nbt_string){units, n};
}

static bool seed_chunk(fixture *f, size_t side, int32_t x) {
    record *r = calloc(1, sizeof(*r));
    CHECK(r != NULL);
    static const char *const names[] = {
        "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
        "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
    static const mc_nbt_type types[] = {MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,
                                        MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
                                        MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE,
                                        MC_NBT_LIST,       MC_NBT_LIST};
    static const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                     MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    r->root.type = MC_NBT_COMPOUND;
    r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    r->fields[0].value.int_value = x;
    r->fields[8].value.byte_value = 1;
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[3u + i].value.byte_array.data = r->arrays[i];
        r->fields[3u + i].value.byte_array.length = lengths[i];
    }
    r->fields[9].value.list.element_type = MC_NBT_COMPOUND;
    r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(r->level_fields);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = COUNT(r->root_fields);
    size_t length = 0, written = 0;
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK);
    uint8_t *encoded = malloc(length);
    CHECK(encoded && mc_nbt_write(&r->root, NULL, encoded, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth = 0;
    const mc_world_storage_status status = mc_world_regions_write(
        f->regions, mc_world_store_root(f->store[side]), x, 0, encoded, length, 127, &growth, NULL);
    free(encoded);
    free(r);
    CHECK(status == MC_WORLD_STORAGE_OK);
    return true;
}

static bool setup(fixture *f, const char *directory) {
    CHECK(mc_ws_mkdirs(directory, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    f->resources = (mc_registry_resources){f, open_input, close_input, ready_input};
    const mc_registry_options registry = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                          &f->resources};
    CHECK(mc_registry_classes_allocate(&registry, &f->registry, NULL) == MC_REGISTRY_OK);
    const uint16_t title[] = {'C', 'h', 'u', 'n', 'k'};
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t side = 0; side < 2; ++side) {
        char path[8192];
        const int n = snprintf(path, sizeof(path), "%s/world%zu", directory, side);
        CHECK(n > 0 && (size_t)n < sizeof(path));
        uint8_t map[256];
        for (size_t i = 0; i < COUNT(map); ++i)
            map[i] = (uint8_t)i;
        const mc_world_store_options store = {.session_token = 127,
                                              .block_map = map,
                                              .block_map_length = sizeof(map),
                                              .limits = mc_world_store_default_limits()};
        CHECK(mc_world_store_open(path, f->regions, &store, &f->store[side], NULL) ==
              MC_WORLD_STORAGE_OK);
        CHECK(seed_chunk(f, side, 0) && seed_chunk(f, side, 1));
        CHECK(mc_ws_path_join(path, "region/r.0.0.mcr", &f->region_path[side], NULL) ==
                  MC_WORLD_STORAGE_OK &&
              mc_ws_read_file(f->region_path[side], 2u * 1024u * 1024u, &f->region_before[side],
                              NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig,
                                              &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry, NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_world_provider_create(0, NULL, &f->dimension[side], NULL) == MC_WORLD_PROVIDER_OK);
        mc_world_runtime_construction_limits limits =
            mc_world_runtime_default_construction_limits();
        limits.max_provider_chunks = 8;
        CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                              f->classes[side], &limits, &f->world[side],
                                              NULL) == MC_WORLD_STORAGE_OK);
        const mc_world_runtime_world_arguments arguments = {
            .store = f->store[side], .name = f->name, .seed = 23, .dimension = f->dimension[side]};
        CHECK(mc_world_runtime_construct_world(f->world[side], &arguments, NULL) ==
              MC_WORLD_STORAGE_OK);
        CHECK(mc_chunk_provider_bind_fields(f->world[side]->fields.v, &f->provider[side], NULL) ==
              MC_WORLD_STORAGE_OK);
        for (size_t x = 0; x < 2; ++x)
            CHECK(mc_world_runtime_get(f->world[side], (int32_t)x, 0, &f->normal[side][x]) ==
                      MC_CHUNK_ACQUIRE_OK &&
                  f->normal[side][x] && !f->normal[side][x]->blank);
    }
    return true;
}

static bool failures(const mc_world_chunk_tick_error *e, const char *cl, int32_t pc,
                     const char *exception, int32_t child_pc) {
    return e->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && e->source_exception &&
           !e->native_policy && e->site.class_name && !strcmp(e->site.class_name, cl) &&
           e->site.pc == pc && e->exception_class && !strcmp(e->exception_class, exception) &&
           (child_pc < 0 ? e->child.class_name == NULL
                         : e->child.class_name && !strcmp(e->child.class_name, "wi") &&
                               e->child.pc == child_pc);
}

static bool contracts(fixture *f) {
    mc_world_runtime *w = f->world[0];
    mc_chunk_state *normal = f->normal[0][0], *blank = &w->fallback;
    mc_world_chunk *object = mc_world_store_chunk_object(f->store[0], normal);
    CHECK(object != NULL && !f->classes[0]->light_types_initialized);
    mc_world_chunk_tick_error error;
    mc_world_chunk_tick_state state;
    CHECK(mc_world_chunk_tick_capture_begin(w, blank, &f->capture[1], &error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(w->active_calls == 1 && w->local_leases == 1 &&
          mc_world_chunk_tick_capture_inspect(f->capture[1], &state) && state.blank &&
          state.ready && state.chunk_held && state.world_held && !state.active);
    int32_t value = 777;
    CHECK(mc_world_chunk_tick_block(f->capture[1], INT32_MIN, -1, INT32_MAX, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 0 && error.site.pc == 1);
    CHECK(mc_world_chunk_tick_metadata(f->capture[1], -1, INT32_MIN, -1, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 0);
    CHECK(mc_world_chunk_tick_saved_light(f->capture[1], NULL, -1, -1, -1, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 0 && !f->classes[0]->light_types_initialized);
    blank->data->blocks[0] = 231;
    CHECK(mc_world_chunk_tick_raw_block(f->capture[1], 0, 0, 0, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 231 && error.site.pc == 948);
    uint8_t *blank_bytes = blank->data->blocks;
    blank->data->blocks = NULL;
    value = 777;
    const mc_world_storage_status blank_failed =
        mc_world_chunk_tick_raw_block(f->capture[1], -1, -1, -1, &value, &error);
    const bool blank_preserved = value == 777;
    const bool blank_virtual =
        mc_world_chunk_tick_block(f->capture[1], -1, -1, -1, &value, NULL) == MC_WORLD_STORAGE_OK &&
        value == 0;
    blank->data->blocks = blank_bytes;
    CHECK(blank_failed != MC_WORLD_STORAGE_OK &&
          failures(&error, "fd", 944, "java/lang/NullPointerException", -1) && blank_preserved &&
          blank_virtual);
    value = 777;
    CHECK(mc_world_chunk_tick_raw_block(f->capture[1], 16, 0, 0, &value, &error) !=
              MC_WORLD_STORAGE_OK &&
          value == 777 &&
          failures(&error, "fd", 944, "java/lang/ArrayIndexOutOfBoundsException", -1));
    CHECK(mc_world_chunk_tick_capture_retire(&f->capture[1], &error) == MC_WORLD_STORAGE_OK &&
          !f->capture[1] && w->local_leases == 0 && w->active_calls == 0);

    mc_chunk_state *alias = NULL;
    CHECK(mc_world_runtime_get(w, 65536, 0, &alias) == MC_CHUNK_ACQUIRE_OK && alias == normal &&
          alias->x == 0 && alias->z == 0);
    uint8_t *blocks = normal->data->blocks;
    normal->data->blocks = NULL;
    const mc_world_storage_status captured_null =
        mc_world_chunk_tick_capture_begin(w, alias, &f->capture[0], &error);
    value = 777;
    const mc_world_storage_status failed_null =
        captured_null == MC_WORLD_STORAGE_OK
            ? mc_world_chunk_tick_block(f->capture[0], -1, -1, -1, &value, &error)
            : captured_null;
    normal->data->blocks = blocks;
    CHECK(captured_null == MC_WORLD_STORAGE_OK && failed_null != MC_WORLD_STORAGE_OK &&
          value == 777 && failures(&error, "lm", 15, "java/lang/NullPointerException", -1));
    CHECK(mc_world_chunk_tick_capture_inspect(f->capture[0], &state) && state.captured == normal &&
          state.chunk_x == 0 && state.chunk_z == 0 && state.ready &&
          state.failure->source_exception);
    CHECK(w->local_leases == 1 && w->active_calls == 1 &&
          mc_world_runtime_retire_world(w, NULL) == MC_WORLD_STORAGE_BUSY &&
          mc_world_runtime_free(w, NULL) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_store_entity_containers_validate(f->store[0], w, NULL) ==
              MC_WORLD_STORAGE_BUSY &&
          mc_world_store_retire_chunk(f->store[0], normal, NULL) == MC_WORLD_STORAGE_BUSY);
    const uint64_t shared_u = atomic_load(&f->profile->uniquifier), world_rng = w->fields.r->state;
    const unsigned int math_phase = atomic_load(&f->profile->holder_phase);
    const uint64_t math = f->profile->math ? atomic_load(&f->profile->math->state) : 0;
    bool changed = false;
    const int32_t pending = mc_chunk_cache_key(7, 9);
    CHECK(mc_chunk_pending_add(f->provider[0].pending, pending, &changed) == MC_CHUNK_ACQUIRE_OK &&
          changed);
    CHECK(mc_chunk_cache_publish(f->provider[0].cache, 0, 0, f->normal[0][1]));
    mc_world_chunk_tick_capture *untouched = f->capture[0];
    CHECK(mc_world_chunk_tick_capture_begin(w, f->normal[1][0], &untouched, &error) !=
              MC_WORLD_STORAGE_OK &&
          untouched == f->capture[0] && error.native_policy);
    CHECK(mc_world_chunk_tick_capture_begin(w, &f->world[1]->fallback, &untouched, &error) !=
              MC_WORLD_STORAGE_OK &&
          untouched == f->capture[0]);
    mc_world_runtime *source_world = object->d;
    object->d = NULL;
    blocks[0] = 255;
    const mc_world_storage_status no_d =
        mc_world_chunk_tick_block(f->capture[0], 0, 0, 0, &value, &error);
    object->d = source_world;
    CHECK(no_d == MC_WORLD_STORAGE_OK && value == 255 && w->local_leases == 1 &&
          w->active_calls == 1);
    for (int32_t i = 0; i < 80; ++i) {
        const int32_t x = i & 15, y = (i * 7) & 127, z = (i * 11) & 15;
        const size_t index = (size_t)((x << 11) | (z << 7) | y);
        const uint8_t expected = (uint8_t)(129 + i);
        blocks[index] = expected;
        CHECK(mc_world_chunk_tick_raw_block(f->capture[0], x, y, z, &value, &error) ==
                  MC_WORLD_STORAGE_OK &&
              value == expected);
        blocks[index] ^= 127;
        CHECK(mc_world_chunk_tick_block(f->capture[0], x, y, z, &value, &error) ==
                  MC_WORLD_STORAGE_OK &&
              value == blocks[index]);
    }
    blocks[128] = 202;
    CHECK(mc_world_chunk_tick_block(f->capture[0], 0, 128, 0, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 202 && error.packed_index == 128);
    blocks[0] = 203;
    CHECK(mc_world_chunk_tick_block(f->capture[0], INT32_C(-2097152), 0, 0, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 203 && error.packed_index == 0);
    value = 777;
    CHECK(mc_world_chunk_tick_block(f->capture[0], 16, 0, 0, &value, &error) !=
              MC_WORLD_STORAGE_OK &&
          value == 777 && error.array_index == 32768 &&
          failures(&error, "lm", 15, "java/lang/ArrayIndexOutOfBoundsException", -1));
    mc_world_chunk_nibble *metadata = object->e;
    object->e = NULL;
    const mc_world_storage_status no_metadata =
        mc_world_chunk_tick_metadata(f->capture[0], 0, 0, 0, &value, &error);
    object->e = metadata;
    CHECK(no_metadata != MC_WORLD_STORAGE_OK && value == 777 &&
          failures(&error, "lm", 7, "java/lang/NullPointerException", -1));
    metadata->a[0] = 0xba;
    CHECK(mc_world_chunk_tick_metadata(f->capture[0], 0, 0, 0, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 10 && error.child.pc == 40);
    CHECK(mc_world_chunk_tick_metadata(f->capture[0], 0, 1, 0, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 11 && error.child.pc == 53);
    for (int32_t y = 0; y < 2; ++y) {
        uint8_t *array = metadata->a;
        metadata->a = NULL;
        value = 777;
        const mc_world_storage_status status =
            mc_world_chunk_tick_metadata(f->capture[0], 0, y, 0, &value, &error);
        metadata->a = array;
        CHECK(status != MC_WORLD_STORAGE_OK && value == 777 &&
              failures(&error, "lm", 7, "java/lang/NullPointerException", y ? 47 : 36));
    }
    CHECK(mc_world_chunk_tick_metadata(f->capture[0], -1, -1, -1, &value, &error) !=
              MC_WORLD_STORAGE_OK &&
          value == 777 && error.array_index == -1 &&
          failures(&error, "lm", 7, "java/lang/ArrayIndexOutOfBoundsException", 47));
    CHECK(!f->classes[0]->light_types_initialized);
    CHECK(mc_world_chunk_tick_saved_light(f->capture[0], NULL, INT32_MIN, -1, -1, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 0 && error.site.pc == 39 && f->classes[0]->light_types_initialized);
    const mc_world_light_type *sky = NULL, *block = NULL, *foreign = NULL;
    CHECK(mc_world_runtime_light_type_get(f->classes[0], MC_LIGHT_SKY, &sky, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_light_type_get(f->classes[0], MC_LIGHT_BLOCK, &block, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_light_type_get(f->classes[1], MC_LIGHT_SKY, &foreign, NULL) ==
              MC_WORLD_STORAGE_OK);
    object->f->a[0] = 0xed;
    object->g->a[0] = 0x21;
    w->fields.f = 15;
    CHECK(mc_world_chunk_tick_saved_light(f->capture[0], sky, 0, 0, 0, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 13 && error.site.pc == 18);
    CHECK(mc_world_chunk_tick_saved_light(f->capture[0], block, 0, 1, 0, &value, &error) ==
              MC_WORLD_STORAGE_OK &&
          value == 2 && error.site.pc == 37);
    f->classes[0]->light_statics[0] = NULL;
    const mc_world_storage_status null_static =
        mc_world_chunk_tick_saved_light(f->capture[0], NULL, 0, 1, 0, &value, &error);
    f->classes[0]->light_statics[0] = sky;
    CHECK(null_static == MC_WORLD_STORAGE_OK && value == 14 && error.site.pc == 18);
    f->classes[0]->light_statics[0] = block;
    const mc_world_storage_status aliased_static =
        mc_world_chunk_tick_saved_light(f->capture[0], block, 0, 0, 0, &value, &error);
    f->classes[0]->light_statics[0] = sky;
    CHECK(aliased_static == MC_WORLD_STORAGE_OK && value == 13 && error.site.pc == 18);
    value = 777;
    CHECK(mc_world_chunk_tick_saved_light(f->capture[0], foreign, 0, 0, 0, &value, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value == 777 && error.native_policy && !error.source_exception);
    mc_world_chunk_nibble *sky_nibble = object->f;
    object->f = NULL;
    const mc_world_storage_status no_sky =
        mc_world_chunk_tick_saved_light(f->capture[0], sky, 0, 0, 0, &value, &error);
    object->f = sky_nibble;
    CHECK(no_sky != MC_WORLD_STORAGE_OK && value == 777 &&
          failures(&error, "lm", 15, "java/lang/NullPointerException", -1));
    mc_chunk_provider *provider = w->fields.v;
    w->fields.v = NULL;
    const mc_world_storage_status no_provider =
        mc_world_chunk_tick_metadata(f->capture[0], 0, 0, 0, &value, &error);
    w->fields.v = provider;
    CHECK(no_provider == MC_WORLD_STORAGE_OK && value == 10);
    CHECK(mc_world_chunk_tick_capture_inspect(f->capture[0], &state) && state.ready &&
          !state.active && state.captured == normal && state.reads > 160 &&
          state.failure->source_exception && state.failure->site.pc == 15);
    CHECK(atomic_load(&f->profile->uniquifier) == shared_u &&
          atomic_load(&f->profile->holder_phase) == math_phase &&
          (!f->profile->math || atomic_load(&f->profile->math->state) == math) &&
          w->fields.r->state == world_rng &&
          mc_chunk_pending_contains(f->provider[0].pending, pending));
    CHECK(mc_world_chunk_tick_capture_retire(NULL, &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          f->capture[0]);
    CHECK(mc_world_runtime_call_enter(w, NULL) == MC_WORLD_STORAGE_OK);
    f->outer_call = true;
    CHECK(w->active_calls == 2);
    CHECK(mc_world_chunk_tick_capture_retire(&f->capture[0], &error) == MC_WORLD_STORAGE_OK &&
          !f->capture[0] && w->active_calls == 1 && w->local_leases == 0 &&
          mc_world_store_entity_containers_validate(f->store[0], w, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_runtime_call_leave(w);
    f->outer_call = false;
    CHECK(w->active_calls == 0);
    CHECK(mc_world_chunk_tick_capture_retire(&f->capture[0], &error) == MC_WORLD_STORAGE_OK);
    return true;
}

static bool cleanup(fixture *f) {
    for (size_t i = 0; i < COUNT(f->capture); ++i)
        CHECK(mc_world_chunk_tick_capture_retire(&f->capture[i], NULL) == MC_WORLD_STORAGE_OK);
    if (f->outer_call) {
        mc_world_runtime_call_leave(f->world[0]);
        f->outer_call = false;
    }
    for (size_t side = 0; side < 2; ++side) {
        if (f->world[side]) {
            if (f->provider[side].cache) {
                CHECK(mc_world_runtime_retire_entity_containers(f->world[side], NULL) ==
                      MC_WORLD_STORAGE_OK);
                CHECK(mc_chunk_pending_clear(f->provider[side].pending) == MC_CHUNK_ACQUIRE_OK);
                mc_chunk_cache *cache = f->provider[side].cache;
                CHECK(mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                                          cache->insertion_capacity));
            }
            CHECK(mc_world_runtime_retire_world(f->world[side], NULL) == MC_WORLD_STORAGE_OK);
            for (size_t x = 0; x < 2; ++x)
                if (f->normal[side][x])
                    CHECK(mc_world_store_retire_chunk(f->store[side], f->normal[side][x], NULL) ==
                          MC_WORLD_STORAGE_OK);
            CHECK(mc_world_runtime_free(f->world[side], NULL) == MC_WORLD_STORAGE_OK);
        }
        if (f->dimension[side])
            mc_world_provider_release(f->dimension[side]);
        if (f->store[side])
            CHECK(mc_world_store_free_checked(&f->store[side], NULL) == MC_WORLD_STORAGE_OK);
        if (f->classes[side])
            CHECK(mc_world_runtime_classes_free(f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        if (f->region_path[side]) {
            mc_ws_file_bytes after = {0};
            CHECK(mc_ws_read_file(f->region_path[side], 2u * 1024u * 1024u, &after, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  after.length == f->region_before[side].length &&
                  !memcmp(after.data, f->region_before[side].data, after.length));
            mc_ws_free_file_bytes(&after);
        }
        mc_ws_free_file_bytes(&f->region_before[side]);
        free(f->region_path[side]);
    }
    if (f->name)
        mc_world_info_name_release(f->name);
    if (f->registry)
        CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    mc_world_regions_free(f->regions);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2 || !mc_math_trig_init(&trig))
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (!f)
        return 2;
    const bool passed = setup(f, argv[1]) && contracts(f);
    const bool retired = cleanup(f);
    if (!retired) {
        fputs("Chunk tick cleanup refused; live dependencies retained\n", stderr);
        return 1;
    }
    free(f);
    if (!passed)
        return 1;
    printf("Captured Chunk tick: %u checks; genuine LM/installed LI source reads and retained "
           "ownership\n",
           checks);
    return 0;
}
