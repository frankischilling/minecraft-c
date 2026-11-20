/* Direct native replay of the separately executed original activation packet.
 * Saved stable inputs only; no captured entropy or Source outcome is injected. */
#define main mc_piston_reference_unused_publication_main
#include "../test_entity_publication.c"
#undef main

#include "mc/world/runtime/entity_tick.h"
#include "mc/world/runtime/placement.h"
#include "world/chunk/object_state_internal.h"
#include "world/runtime/tile_entities/piston_list_internal.h"
#include "world/runtime/tile_entities/tile_entities_internal.h"

#include <inttypes.h>

static size_t cell_at(int32_t x, int32_t y, int32_t z) {
    return ((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y;
}

static void input_cell(fixture_record *r, int32_t x, int32_t y, int32_t z, uint8_t id,
                       uint8_t metadata) {
    const size_t index = cell_at(x, y, z);
    r->arrays[0][index] = id;
    const unsigned int shift = (unsigned int)(index & 1u) * 4u;
    r->arrays[1][index / 2u] =
        (uint8_t)((r->arrays[1][index / 2u] & ~(15u << shift)) | ((unsigned int)metadata << shift));
    if (id != 0 && r->arrays[4][(size_t)z * 16u + (size_t)x] < y + 1)
        r->arrays[4][(size_t)z * 16u + (size_t)x] = (uint8_t)(y + 1);
}

/* Pre-acquisition wire only. No MovingPiston/Tile/progress is authored. */
static bool input_record(fixture *f, int32_t base_id, size_t stones, uint8_t carried) {
    bool ok = false;
    fixture_record *r = calloc(1, sizeof(*r));
    uint8_t *wire = NULL;
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
    CHECK(r != NULL && stones <= 13);
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
    r->fields[8].value.byte_value = 1;
    r->fields[9].value.list.element_type = r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    memset(r->arrays[2], 255, MC_CHUNK_NIBBLE_BYTES);
    memset(r->arrays[4], 64, MC_CHUNK_HEIGHTMAP_BYTES);
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z)
            input_cell(r, x, 63, z, 1, 0);
    input_cell(r, 8, 64, 1, (uint8_t)base_id, 3);
    input_cell(r, 7, 64, 1, 69, 4);
    input_cell(r, 7, 64, 2, 1, 0);
    for (size_t i = 0; i < stones; ++i)
        input_cell(r, 8, 64, (int32_t)i + 2, carried, 0);
    r->level.value.compound.items = r->level_fields;
    r->level.value.compound.length = COUNT(r->level_fields);
    r->root_fields[0] = &r->level;
    r->root.value.compound.items = r->root_fields;
    r->root.value.compound.length = 1;
    CHECK(mc_nbt_measure(&r->root, NULL, &length, NULL) == MC_NBT_OK);
    wire = malloc(length);
    CHECK(wire != NULL && mc_nbt_write(&r->root, NULL, wire, length, &written, NULL) == MC_NBT_OK &&
          written == length);
    uint32_t growth;
    CHECK(mc_world_regions_write(f->regions, mc_world_store_root(f->stores[0]), 0, 0, wire, length,
                                 fixture_timestamp, &growth, NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(wire);
    free(r);
    return ok;
}

static bool piston_fixture(fixture *f, const char *root, int32_t base_id, size_t stones,
                           uint8_t carried) {
    bool ok = false;
    static const uint16_t title[] = {'P', 'i', 's', 't', 'o', 'n'};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials) &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
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
                  MC_REGISTRY_OK &&
              mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
                  MC_ENTITY_OK &&
              mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK);
    }
    CHECK(open_store(f, 0, root) && input_record(f, base_id, stones, carried));
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK &&
          mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                          NULL, &f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[0], .name = f->name, .seed = -99, .dimension = f->dimensions[0]};
    CHECK(mc_world_runtime_construct_world(f->worlds[0], &arguments, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_provider_bind_fields(f->worlds[0]->fields.v, &f->providers[0], NULL) ==
              MC_WORLD_STORAGE_OK);
    f->bound[0] = true;
    mc_chunk_state *loaded = NULL;
    const mc_chunk_acquire_status acquired = mc_world_runtime_get(f->worlds[0], 0, 0, &loaded);
    if (f->providers[0].cache->insertion_count != 0)
        f->states[0] = f->providers[0].cache->insertions[0];
    CHECK(acquired == MC_CHUNK_ACQUIRE_OK && loaded != NULL && loaded == f->states[0]);
    f->chunks[0] = mc_world_store_chunk_object(f->stores[0], loaded);
    CHECK(f->chunks[0] != NULL && mc_live_tile_entities_map_valid(f->chunks[0]) &&
          f->worlds[0]->fields.c->count == 0 && f->chunks[0]->l->count == 0);
    ok = true;
cleanup:
    return ok;
}

static bool activate(fixture *f, bool context, mc_world_placement_error *error,
                     mc_world_storage_status expected) {
    const mc_block *lever = NULL;
    bool out = false;
    const mc_world_block_activation_context source = {.block_code_classes = f->classes[0]};
    return mc_registry_block_get(f->registry[0], 69, &lever, NULL) == MC_REGISTRY_OK &&
           lever != NULL &&
           mc_world_block_activate_with_context(f->registry[0], lever, f->worlds[0], 7, 64, 1, NULL,
                                                context ? &source : NULL, &out,
                                                error) == expected &&
           (expected != MC_WORLD_STORAGE_OK || out);
}

enum { REF_ROOTS = 32, REF_EVENTS = 128, REF_STEPS = 11, REF_PLANES = 5 };

typedef struct ref_tile {
    size_t label;
    bool world_same;
    mc_tile_entity_snapshot value;
} ref_tile;

typedef struct ref_map_entry {
    int32_t x, y, z;
    ref_tile tile;
} ref_map_entry;

typedef struct ref_state {
    int64_t time, seed;
    char name[128];
    bool remote, iterating, suppression;
    size_t entities, players, weather, unloaded, chunks;
    mc_chunk_state flags;
    int32_t minimum_height;
    uint8_t planes[REF_PLANES][MC_CHUNK_BLOCKS];
    ref_tile active[REF_ROOTS], pending[REF_ROOTS], retained[REF_ROOTS];
    ref_map_entry map[REF_ROOTS];
    size_t active_count, pending_count, map_count, retained_count;
    int32_t cells[18][5];
    mc_java_random random;
} ref_state;

typedef enum ref_event_kind { REF_CELL, REF_RANGE, REF_SOUND } ref_event_kind;

typedef struct ref_event {
    ref_event_kind kind;
    const char *stage;
    int32_t args[6];
    char name[128];
    double position[3];
    float volume, pitch;
    ref_state *state;
} ref_event;

typedef struct ref_observer {
    fixture *fixture;
    const mc_block *base;
    const char *stage;
    mc_tile_entity *roots[REF_ROOTS];
    size_t root_count, event_count;
    ref_event events[REF_EVENTS];
    bool failed;
} ref_observer;

typedef struct ref_step {
    const char *stage;
    bool activation, initial;
    size_t event_start, event_end;
    ref_state *state;
} ref_step;

static const char *ref_bool(bool value) {
    return value ? "true" : "false";
}

static bool ref_ascii(mc_nbt_string text, char *out, size_t capacity) {
    if (text.length >= capacity || (text.length != 0 && text.units == NULL))
        return false;
    for (size_t i = 0; i < text.length; ++i) {
        if (text.units[i] > 127 || text.units[i] == 0)
            return false;
        out[i] = (char)text.units[i];
    }
    out[text.length] = '\0';
    return true;
}

static bool ref_tile_read(ref_observer *observer, mc_tile_entity *tile, ref_tile *out) {
    if (tile == NULL || mc_tile_entity_get_snapshot(tile, &out->value) != MC_TILE_ENTITY_OK ||
        out->value.kind != MC_TILE_ENTITY_PISTON ||
        mc_tile_entity_piston_code_classes_held(tile) != observer->fixture->classes[0])
        return false;
    size_t label = 0;
    while (label < observer->root_count && observer->roots[label] != tile)
        ++label;
    if (label == observer->root_count) {
        if (label >= REF_ROOTS || mc_tile_entity_retain(tile) != MC_TILE_ENTITY_OK)
            return false;
        observer->roots[observer->root_count++] = tile;
    }
    out->label = label;
    out->world_same = out->value.world == observer->fixture->worlds[0];
    return true;
}

static bool ref_list_read(ref_observer *observer, const mc_world_reference_list *list,
                          ref_tile *out, size_t *count) {
    if (list == NULL || list->count > REF_ROOTS || list->count > list->capacity)
        return false;
    *count = list->count;
    for (size_t i = 0; i < *count; ++i)
        if (!ref_tile_read(observer, list->items[i], &out[i]))
            return false;
    return true;
}

static ref_state *ref_snapshot(ref_observer *observer) {
    fixture *f = observer->fixture;
    mc_world_runtime *world = f->worlds[0];
    mc_chunk_state *chunk = f->states[0];
    mc_world_info_fields info;
    ref_state *state = calloc(1, sizeof(*state));
    if (state == NULL || !mc_live_tile_entities_lists_valid(world) ||
        !mc_live_tile_entities_map_valid(f->chunks[0]) ||
        mc_world_info_fields_get(world->fields.x, &info) != MC_WORLD_INFO_OK ||
        !ref_ascii(mc_world_info_name_text(info.name), state->name, sizeof(state->name)) ||
        world->fields.r == NULL || chunk == NULL || chunk->data == NULL ||
        chunk->data->blocks == NULL || chunk->data->blocks_capacity < MC_CHUNK_BLOCKS ||
        world->fields.b == NULL || world->fields.d == NULL || world->fields.e == NULL ||
        world->fields.D == NULL)
        goto failed;
    state->time = info.value.time;
    state->seed = info.value.seed;
    state->remote = world->fields.B;
    state->iterating = world->fields.L;
    state->suppression = observer->base->subtype.jq.b;
    state->entities = world->fields.b->count;
    state->players = world->fields.d->count;
    state->weather = world->fields.e->count;
    state->unloaded = world->fields.D->count;
    state->chunks = mc_world_store_chunk_count(f->stores[0]);
    state->flags = *chunk;
    state->minimum_height = chunk->data->minimum_height;
    state->random = *world->fields.r;
    const uint8_t *const arrays[] = {chunk->data->blocks, chunk->data->metadata,
                                     chunk->data->skylight, chunk->data->block_light,
                                     chunk->data->heightmap};
    static const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                     MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    for (size_t i = 0; i < COUNT(arrays); ++i)
        memcpy(state->planes[i], arrays[i], lengths[i]);
    /* Same first-observation order as the original passive observer: actual
     * c, then G, then raw Map encounter order, then all retained roots. */
    if (!ref_list_read(observer, world->fields.c, state->active, &state->active_count) ||
        !ref_list_read(observer, world->fields.G, state->pending, &state->pending_count) ||
        f->chunks[0]->l->count > REF_ROOTS)
        goto failed;
    state->map_count = f->chunks[0]->l->count;
    for (size_t i = 0; i < state->map_count; ++i) {
        mc_live_tile_entity_map_entry entry;
        if (mc_live_tile_entities_map_entry_at(f->chunks[0], i, &entry) != MC_WORLD_STORAGE_OK ||
            !ref_tile_read(observer, entry.tile_entity, &state->map[i].tile))
            goto failed;
        state->map[i].x = entry.x;
        state->map[i].y = entry.y;
        state->map[i].z = entry.z;
    }
    state->retained_count = observer->root_count;
    for (size_t i = 0; i < state->retained_count; ++i)
        if (!ref_tile_read(observer, observer->roots[i], &state->retained[i]))
            goto failed;
    for (size_t i = 0; i < 18; ++i) {
        const int32_t x = i < 16 ? 8 : 7;
        const int32_t z = i < 16 ? (int32_t)i : (int32_t)i - 15;
        const size_t at = cell_at(x, 64, z);
        state->cells[i][0] = x;
        state->cells[i][1] = 64;
        state->cells[i][2] = z;
        state->cells[i][3] = chunk->data->blocks[at];
        state->cells[i][4] = (chunk->data->metadata[at / 2u] >> ((at & 1u) * 4u)) & 15;
    }
    return state;
failed:
    free(state);
    observer->failed = true;
    return NULL;
}

static ref_event *ref_event_new(ref_observer *observer, ref_event_kind kind) {
    if (observer->event_count == REF_EVENTS) {
        observer->failed = true;
        return NULL;
    }
    ref_event *event = &observer->events[observer->event_count++];
    event->stage = observer->stage;
    event->kind = kind;
    return event;
}

static mc_world_storage_status ref_cell(void *context, mc_world_runtime *world, int32_t x,
                                        int32_t y, int32_t z, mc_world_storage_error *error) {
    (void)world;
    (void)error;
    ref_event *event = ref_event_new(context, REF_CELL);
    if (event != NULL) {
        event->args[0] = x;
        event->args[1] = y;
        event->args[2] = z;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status ref_range(void *context, mc_world_runtime *world, int32_t x,
                                         int32_t y, int32_t z, int32_t a, int32_t b, int32_t c,
                                         mc_world_storage_error *error) {
    (void)world;
    (void)error;
    ref_event *event = ref_event_new(context, REF_RANGE);
    if (event != NULL) {
        const int32_t args[] = {x, y, z, a, b, c};
        memcpy(event->args, args, sizeof(args));
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status ref_sound(void *context, mc_world_runtime *world,
                                         const mc_registry_string *name, double x, double y,
                                         double z, float volume, float pitch,
                                         mc_world_storage_error *error) {
    (void)world;
    (void)error;
    ref_observer *observer = context;
    ref_event *event = ref_event_new(observer, REF_SOUND);
    if (event != NULL) {
        mc_registry_string_view text;
        if (!mc_registry_string_read(name, &text) ||
            !ref_ascii((mc_nbt_string){text.units, text.length}, event->name, sizeof(event->name)))
            observer->failed = true;
        event->position[0] = x;
        event->position[1] = y;
        event->position[2] = z;
        event->volume = volume;
        event->pitch = pitch;
        event->state = ref_snapshot(observer);
    }
    return MC_WORLD_STORAGE_OK;
}

/* All real interface slots are supplied. An unexpected family records a
 * harness failure while returning normally, so it cannot manufacture a new
 * Source exception or substitute a gameplay return value. */
static mc_world_storage_status ref_particle(void *context, mc_world_runtime *world,
                                            const mc_registry_string *name, double x, double y,
                                            double z, double a, double b, double c,
                                            mc_world_storage_error *error) {
    (void)world;
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)a;
    (void)b;
    (void)c;
    (void)error;
    ((ref_observer *)context)->failed = true;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status ref_entity(void *context, mc_world_runtime *world, mc_entity *entity,
                                          mc_world_storage_error *error) {
    (void)world;
    (void)entity;
    (void)error;
    ((ref_observer *)context)->failed = true;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status ref_dirty(void *context, mc_world_runtime *world, int32_t x,
                                         int32_t y, int32_t z, mc_tile_entity *tile,
                                         mc_world_storage_error *error) {
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    (void)tile;
    (void)error;
    ((ref_observer *)context)->failed = true;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status ref_sky(void *context, mc_world_runtime *world,
                                       mc_world_storage_error *error) {
    (void)world;
    (void)error;
    ((ref_observer *)context)->failed = true;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status ref_aux(void *context, mc_world_runtime *world, mc_entity *player,
                                       int32_t a, int32_t x, int32_t y, int32_t z, int32_t b,
                                       mc_world_storage_error *error) {
    (void)world;
    (void)player;
    (void)a;
    (void)x;
    (void)y;
    (void)z;
    (void)b;
    (void)error;
    ((ref_observer *)context)->failed = true;
    return MC_WORLD_STORAGE_OK;
}

static void ref_string(FILE *file, const char *text) {
    fputc('"', file);
    for (const unsigned char *p = (const unsigned char *)text; *p != 0; ++p) {
        if (*p == '"' || *p == '\\')
            fputc('\\', file);
        if (*p < 32)
            fprintf(file, "\\u%04x", (unsigned int)*p);
        else
            fputc(*p, file);
    }
    fputc('"', file);
}

static void ref_hex(FILE *file, const uint8_t *bytes, size_t length) {
    static const char digits[] = "0123456789abcdef";
    fputc('"', file);
    for (size_t i = 0; i < length; ++i) {
        fputc(digits[bytes[i] >> 4u], file);
        fputc(digits[bytes[i] & 15u], file);
    }
    fputc('"', file);
}

static uint32_t ref_fbits(float value) {
    uint32_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static uint64_t ref_dbits(double value) {
    uint64_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static void ref_tile_json(FILE *file, const ref_tile *tile) {
    const mc_tile_entity_snapshot *v = &tile->value;
    fprintf(
        file,
        "{\"label\":\"T%zu\",\"class\":\"uk\",\"world_same\":%s,\"xyz\":[%" PRId32 ",%" PRId32
        ",%" PRId32 "],\"invalid\":%s,\"piston\":{\"block\":%" PRId32 ",\"metadata\":%" PRId32
        ",\"facing\":%" PRId32 ",\"extending\":%s,\"render_head\":%s,\"current_bits\":\"%08" PRIx32
        "\",\"previous_bits\":\"%08" PRIx32 "\"}}",
        tile->label, ref_bool(tile->world_same), v->x, v->y, v->z, ref_bool(v->invalid),
        v->piston_block_id, v->piston_block_data, v->piston_facing, ref_bool(v->piston_extending),
        ref_bool(v->piston_render_head), v->piston_progress_bits, v->piston_previous_progress_bits);
}

static void ref_tiles_json(FILE *file, const ref_tile *tiles, size_t count) {
    fputc('[', file);
    for (size_t i = 0; i < count; ++i) {
        if (i != 0)
            fputc(',', file);
        ref_tile_json(file, &tiles[i]);
    }
    fputc(']', file);
}

static void ref_state_json(FILE *file, const ref_state *s) {
    fprintf(file, "{\"world_class\":\"fd\",\"world_time\":%" PRId64 ",\"world_name\":", s->time);
    ref_string(file, s->name);
    fprintf(file,
            ",\"world_seed\":%" PRId64
            ",\"remote\":%s,\"iterating\":%s,\"entity_count\":%zu,\"player_count\":%zu,"
            "\"weather_entities\":%zu,\"unloaded_entities\":%zu,\"loaded_chunks\":%zu,"
            "\"chunk_flags\":{\"loaded\":%s,\"terrain_populated\":%s,\"dirty\":%s,"
            "\"never_save\":%s,\"has_entities\":%s,\"last_save_time\":%" PRId64
            ",\"minimum_height\":%" PRId32 "},\"chunk_array_hex\":{",
            s->seed, ref_bool(s->remote), ref_bool(s->iterating), s->entities, s->players,
            s->weather, s->unloaded, s->chunks, ref_bool(s->flags.active),
            ref_bool(s->flags.terrain_populated), ref_bool(s->flags.dirty),
            ref_bool(s->flags.never_save), ref_bool(s->flags.has_entities), s->flags.last_save_time,
            s->minimum_height);
    static const char *const names[] = {"Blocks", "Data", "SkyLight", "BlockLight", "HeightMap"};
    static const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                     MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    for (size_t i = 0; i < COUNT(names); ++i) {
        if (i != 0)
            fputc(',', file);
        ref_string(file, names[i]);
        fputc(':', file);
        ref_hex(file, s->planes[i], lengths[i]);
    }
    fputs("},\"active\":", file);
    ref_tiles_json(file, s->active, s->active_count);
    fputs(",\"pending\":", file);
    ref_tiles_json(file, s->pending, s->pending_count);
    fputs(",\"map\":[", file);
    for (size_t i = 0; i < s->map_count; ++i) {
        if (i != 0)
            fputc(',', file);
        fprintf(file,
                "{\"key_class\":\"wf\",\"key_fields\":{\"a\":%" PRId32 ",\"b\":%" PRId32
                ",\"c\":%" PRId32 "},\"tile\":",
                s->map[i].x, s->map[i].y, s->map[i].z);
        ref_tile_json(file, &s->map[i].tile);
        fputc('}', file);
    }
    fputs("],\"retained\":", file);
    ref_tiles_json(file, s->retained, s->retained_count);
    fputs(",\"cells\":[", file);
    for (size_t i = 0; i < COUNT(s->cells); ++i) {
        if (i != 0)
            fputc(',', file);
        fprintf(file, "[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "]",
                s->cells[i][0], s->cells[i][1], s->cells[i][2], s->cells[i][3], s->cells[i][4]);
    }
    fprintf(file,
            "],\"suppression\":%s,\"world_random\":{\"seed\":%" PRIu64
            ",\"cached\":%s,\"gaussian_bits\":\"%016" PRIx64 "\"}}",
            ref_bool(s->suppression), s->random.state, ref_bool(s->random.have_next_next_gaussian),
            ref_dbits(s->random.next_next_gaussian));
}

static void ref_event_json(FILE *file, const ref_event *event, size_t index) {
    fprintf(file, "{\"index\":%zu,\"stage\":", index);
    ref_string(file, event->stage);
    if (event->kind == REF_SOUND) {
        fputs(",\"kind\":\"sound\",\"name\":", file);
        ref_string(file, event->name);
        fprintf(file,
                ",\"position_bits\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                "\"],\"volume_bits\":\"%08" PRIx32 "\",\"pitch_bits\":\"%08" PRIx32 "\",\"state\":",
                ref_dbits(event->position[0]), ref_dbits(event->position[1]),
                ref_dbits(event->position[2]), ref_fbits(event->volume), ref_fbits(event->pitch));
        ref_state_json(file, event->state);
    } else {
        fprintf(file, ",\"kind\":\"%s\",\"arguments\":[",
                event->kind == REF_CELL ? "cell" : "range");
        const size_t count = event->kind == REF_CELL ? 3u : 6u;
        for (size_t i = 0; i < count; ++i) {
            if (i != 0)
                fputc(',', file);
            fprintf(file, "%" PRId32, event->args[i]);
        }
        fputc(']', file);
    }
    fputc('}', file);
}

static bool ref_step_call(ref_observer *observer, ref_step *step,
                          mc_world_placement_error *activation_error,
                          mc_world_entity_tick_error *tick_error) {
    observer->stage = step->stage;
    step->event_start = observer->event_count;
    bool returned = true;
    if (step->activation)
        returned = activate(observer->fixture, true, activation_error, MC_WORLD_STORAGE_OK);
    else if (!step->initial)
        returned = mc_world_runtime_tick_entities(observer->fixture->worlds[0], NULL, NULL,
                                                  tick_error) == MC_WORLD_STORAGE_OK;
    step->event_end = observer->event_count;
    step->state = ref_snapshot(observer);
    const bool retired = mc_world_placement_error_retire(activation_error) &&
                         mc_world_entity_tick_error_retire(tick_error);
    return returned && retired && step->state != NULL && !observer->failed;
}

static bool ref_case(FILE *file, const char *root, size_t index) {
    static const char *const names[] = {"normal-push",  "sticky-pull", "twelve-push",
                                        "thirteen-cap", "obsidian",    "bedrock"};
    static const char *const positive[] = {"initial",   "activate-on",  "extend-1",  "extend-2",
                                           "extend-3",  "activate-off", "retract-1", "retract-2",
                                           "retract-3", "quiet-1",      "quiet-2"};
    static const char *const blocked[] = {"initial", "activate-on", "blocked-quiet-1",
                                          "blocked-quiet-2", "blocked-quiet-3"};
    bool ok = false, registered = false;
    fixture *f = calloc(1, sizeof(*f));
    ref_observer *observer = calloc(1, sizeof(*observer));
    mc_world_placement_error *activation_error = calloc(1, sizeof(*activation_error));
    mc_world_entity_tick_error *tick_error = calloc(1, sizeof(*tick_error));
    mc_world_access *access = NULL;
    ref_step steps[REF_STEPS] = {0};
    const bool is_blocked = index >= 3;
    const int32_t base = index == 1 ? 29 : 33;
    const size_t stones = index == 2 ? 12u : index == 3 ? 13u : 1u;
    const uint8_t carried = index == 4 ? 49 : index == 5 ? 7 : 1;
    const size_t step_count = is_blocked ? COUNT(blocked) : COUNT(positive);
    CHECK(f != NULL && observer != NULL && activation_error != NULL && tick_error != NULL &&
          piston_fixture(f, root, base, stones, carried));
    observer->fixture = f;
    CHECK(mc_registry_block_get(f->registry[0], base, &observer->base, NULL) == MC_REGISTRY_OK &&
          observer->base != NULL);
    const mc_world_access_callbacks callbacks = {.cell = ref_cell,
                                                 .range = ref_range,
                                                 .sound = ref_sound,
                                                 .particle = ref_particle,
                                                 .entity_added = ref_entity,
                                                 .tile_entity_dirty = ref_dirty,
                                                 .entity_removed = ref_entity,
                                                 .sky_changed = ref_sky,
                                                 .auxiliary = ref_aux};
    CHECK(mc_world_access_create(&callbacks, observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->worlds[0], access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    for (size_t i = 0; i < step_count; ++i) {
        steps[i].stage = is_blocked ? blocked[i] : positive[i];
        steps[i].activation = i == 1 || (!is_blocked && i == 5);
        steps[i].initial = i == 0;
        CHECK(ref_step_call(observer, &steps[i], activation_error, tick_error));
    }
    fprintf(file,
            "%s{\"index\":%zu,\"profile\":[\"%zu\",\"case%02zu\",\"%s\",\"%" PRId32
            "\",\"%zu\",\"%u\",\"%s\"],\"steps\":[",
            index == 0 ? "" : ",", index, index, index, names[index], base, stones,
            (unsigned int)carried, ref_bool(is_blocked));
    for (size_t i = 0; i < step_count; ++i) {
        if (i != 0)
            fputc(',', file);
        const ref_step *step = &steps[i];
        fputs("{\"stage\":", file);
        ref_string(file, step->stage);
        fprintf(file,
                ",\"operation\":\"%s\",\"returned\":true,\"activation_result\":%s,"
                "\"failure\":null,\"event_start\":%zu,\"event_end\":%zu,\"state\":",
                step->initial      ? "observe"
                : step->activation ? "XR.a(FD,III,NULL)"
                                   : "FD.g()V",
                step->activation ? "true" : "null", step->event_start, step->event_end);
        ref_state_json(file, step->state);
        fputc('}', file);
    }
    fputs("],\"events\":[", file);
    for (size_t i = 0; i < observer->event_count; ++i) {
        if (i != 0)
            fputc(',', file);
        ref_event_json(file, &observer->events[i], i);
    }
    fputs("]}", file);
    CHECK(!ferror(file));
    ok = true;
cleanup:
    if (activation_error != NULL && !mc_world_placement_error_retire(activation_error))
        ok = false;
    if (tick_error != NULL && !mc_world_entity_tick_error_retire(tick_error))
        ok = false;
    if (registered) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(f->worlds[0], access, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            ok = false;
    }
    if (access != NULL && mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    if (observer != NULL) {
        for (size_t i = 0; i < observer->root_count; ++i)
            mc_tile_entity_release(observer->roots[i]);
        for (size_t i = 0; i < observer->event_count; ++i)
            free(observer->events[i].state);
    }
    for (size_t i = 0; i < step_count; ++i)
        free(steps[i].state);
    if (!cleanup_fixture(f))
        ok = false;
    free(observer);
    free(activation_error);
    free(tick_error);
    return ok;
}

int main(int argc, char **argv) {
    char root[8192], path[8192];
    if (argc != 3 || !decode_path(argv[1], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    mc_ws_output output = {0};
    if (mc_ws_output_open(argv[2], 32u * 1024u * 1024u, &output, NULL) != MC_WORLD_STORAGE_OK)
        return 1;
    FILE *file = output.file;
    bool ok = true;
    fputs("{\"schema\":\"NATIVE_PISTON_ACTIVATION_REPLAY_V1\",\"rows\":[", file);
    for (size_t i = 0; i < 6; ++i) {
        const int n = snprintf(path, sizeof(path), "%s/case%02zu", root, i);
        if (n <= 0 || (size_t)n >= sizeof(path) || !ref_case(file, path, i)) {
            ok = false;
            break;
        }
    }
    if (ok)
        fputs("],\"cleanup\":true,\"source_field_repairs\":0}\n", file);
    if (ferror(file))
        ok = false;
    const long length = ftell(file);
    if (length < 0 || (unsigned long)length > 32u * 1024u * 1024u)
        ok = false;
    if (mc_ws_output_close(&output, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    if (ok)
        printf(
            "PASS: Piston reference replay; cases=6; checks=%u; cleanup=1; source-field-repair=0\n",
            checks);
    return ok ? 0 : 1;
}
