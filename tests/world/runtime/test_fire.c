/* The imported fixture supplies genuine owners and a pre-load NBT encoder.
 * Its main and gameplay scenarios are never invoked by this transport. */
#define main mc_fire_unused_fluid_fixture_main
#include "test_fluids.c"
#undef main

#include "../../../src/block/registry/runtime_internal.h"
#include "../../../src/entity/entity_internal.h"
#include "../../../src/world/info/tick_source_internal.h"
#include "../../../src/world/info/weather_source_internal.h"
#include "../../../src/world/runtime/world_access_internal.h"
#include "mc/entity/player/player.h"
#include "mc/entity/primed_tnt/construction.h"
#include "mc/world/biome/registry.h"
#include "mc/world/dimension/world_chunk_manager.h"
#include "mc/world/runtime/random_tick.h"
#include "mc/world/runtime/world_tick.h"
#include <errno.h>
#include <inttypes.h>

enum { FR_MAX_CELLS = 512, FR_MAX_CHUNKS = 512, FR_MAX_EVENTS = 4096 };

typedef struct fr_cell {
    int32_t x, y, z, id, metadata;
} fr_cell;

typedef struct fr_profile {
    const char *mode, *operation;
    int32_t xyz[3], entry_id, source_id, errors[4], prequeue_delay;
    int64_t seed, scheduled_before;
    bool scheduled_initialized, rain;
    char biome;
    bool null_encouragement, null_flammability, null_provider, random_player;
    bool entity_initialized;
    int32_t g, cooldown, entity_next;
    fr_cell cells[FR_MAX_CELLS];
    int32_t heights[FR_MAX_CELLS][3];
    size_t cell_count, height_count;
} fr_profile;

typedef struct fr_observation {
    fixture *f;
    const fr_profile *profile;
    mc_chunk_state *chunks[FR_MAX_CHUNKS];
    size_t chunk_count, events, kinds[5];
    mc_world_access *listener;
    mc_world_access_receipt *receipt;
    mc_math_classes *math;
    mc_entity *player;
    mc_player_session_classes *sessions;
    mc_chunk_provider *prepared_provider;
    int32_t *encouragement, *flammability;
    bool valid;
} fr_observation;

static const fr_profile *fr_prepared_profile;

static const char *fr_bool(bool value) {
    return value ? "true" : "false";
}

static uint64_t fr_double(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static uint32_t fr_float(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static void fr_random(const mc_java_random *random) {
    printf("[\"%016" PRIx64 "\",%s,\"%016" PRIx64 "\"]", random->state,
           fr_bool(random->have_next_next_gaussian), fr_double(random->next_next_gaussian));
}

static void fr_math(const fixture *f) {
    printf("[\"%016" PRIx64 "\",%s,\"%016" PRIx64 "\"]",
           atomic_load_explicit(&f->profile->math->state, memory_order_relaxed),
           fr_bool(f->profile->math->have_next_next_gaussian),
           fr_double(f->profile->math->next_next_gaussian));
}

static bool fr_runs(const uint8_t *bytes, size_t length) {
    putchar('[');
    bool first = true;
    for (size_t at = 0; at < length;) {
        const uint8_t value = bytes[at];
        size_t end = at + 1;
        while (end < length && bytes[end] == value)
            ++end;
        if (value != 0) {
            printf("%s[%zu,%zu,%u]", first ? "" : ",", at, end - at, (unsigned int)value);
            first = false;
        }
        at = end;
    }
    return putchar(']') != EOF;
}

static bool fr_planes(const mc_chunk_data *data) {
    return printf("{\"blocks\":") >= 0 && fr_runs(data->blocks, MC_CHUNK_BLOCKS) &&
           printf(",\"metadata\":") >= 0 && fr_runs(data->metadata, MC_CHUNK_NIBBLE_BYTES) &&
           printf(",\"sky\":") >= 0 && fr_runs(data->skylight, MC_CHUNK_NIBBLE_BYTES) &&
           printf(",\"block_light\":") >= 0 && fr_runs(data->block_light, MC_CHUNK_NIBBLE_BYTES) &&
           printf(",\"height\":") >= 0 && fr_runs(data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES) &&
           putchar('}') != EOF;
}

static input_cell fr_prepared(int32_t x, int32_t y, int32_t z) {
    input_cell value = {0};
    for (size_t i = 0; i < fr_prepared_profile->cell_count; ++i) {
        const fr_cell *c = &fr_prepared_profile->cells[i];
        if (c->x == x && c->y == y && c->z == z)
            value = (input_cell){(uint8_t)c->id, (uint8_t)c->metadata};
    }
    return value;
}

/* Passive direct Chunk observations never load a Chunk or initialize classes. */
static bool fr_cell_read(const fr_observation *o, int32_t x, int32_t y, int32_t z, int32_t out[2]) {
    if (y < 0 || y >= 128) {
        out[0] = out[1] = 0;
        return true;
    }
    for (size_t i = 0; i < o->chunk_count; ++i) {
        const mc_chunk_state *chunk = o->chunks[i];
        if (chunk->x == mc_java_i32_shr(x, 4) && chunk->z == mc_java_i32_shr(z, 4)) {
            uint8_t metadata;
            const int32_t lx = x & 15, lz = z & 15;
            if (!mc_chunk_nibbles_get(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, lx, y, lz,
                                      &metadata))
                return false;
            out[0] = chunk->data->blocks[cell(lx, y, lz)];
            out[1] = metadata;
            return true;
        }
    }
    return false;
}

static bool fr_name(const mc_registry_string *name, char out[64]) {
    mc_registry_string_view view;
    if (!mc_registry_string_read(name, &view) || view.length >= 64)
        return false;
    for (size_t i = 0; i < view.length; ++i) {
        if (view.units[i] < 32 || view.units[i] > 126 || view.units[i] == '"' ||
            view.units[i] == '\\')
            return false;
        out[i] = (char)view.units[i];
    }
    out[view.length] = '\0';
    return true;
}

static bool fr_queue(const mc_scheduled_tick **entries, size_t count) {
    putchar('[');
    for (size_t i = 0; i < count; ++i) {
        mc_scheduled_tick_fields fields;
        if (mc_scheduled_tick_fields_get(entries[i], &fields) != MC_SCHEDULED_TICK_OK)
            return false;
        printf("%s{\"xyz\":[%" PRId32 ",%" PRId32 ",%" PRId32 "],\"block\":%" PRId32
               ",\"time\":\"%016" PRIx64 "\",\"ordinal\":\"%016" PRIx64 "\"}",
               i == 0 ? "" : ",", fields.x, fields.y, fields.z, fields.block,
               (uint64_t)fields.due_time, (uint64_t)fields.sequence);
    }
    return putchar(']') != EOF;
}

static bool fr_scheduled(const mc_world_runtime *world) {
    mc_world_schedule_class_state sequence;
    const mc_scheduled_tick *entries[FR_MAX_EVENTS];
    size_t count;
    if (mc_world_runtime_schedule_class_get(world->classes, &sequence) != MC_WORLD_STORAGE_OK)
        return false;
    printf("{\"initialized\":%s,\"sequence\":\"%016" PRIx64 "\",\"ordered\":",
           fr_bool(sequence.initialized), (uint64_t)sequence.next);
    if (mc_scheduled_ordered_snapshot(world->fields.E, entries, COUNT(entries), &count) !=
            MC_SCHEDULED_ORDERED_OK ||
        !fr_queue(entries, count))
        return false;
    printf(",\"members\":");
    return mc_scheduled_membership_snapshot(world->fields.F, entries, COUNT(entries), &count) ==
               MC_SCHEDULED_MEMBERSHIP_OK &&
           fr_queue(entries, count) && putchar('}') != EOF;
}

static bool fr_lighting(const mc_world_runtime *world) {
    mc_world_light_list_view lighting;
    if (mc_world_runtime_light_list_get(world, &lighting, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    printf("{\"depth\":%" PRId32 ",\"regions\":[", world->classes->enqueue_depth);
    for (size_t i = 0; i < lighting.count; ++i) {
        const mc_world_light_region *region = NULL;
        if (mc_world_runtime_light_at(world, i, &region, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        const mc_world_light_region_fields *fields =
            mc_world_runtime_light_region_fields_get(region);
        const mc_world_light_type_fields *type =
            fields == NULL ? NULL : mc_world_runtime_light_type_fields_get(fields->a);
        if (fields == NULL || type == NULL)
            return false;
        printf("%s{\"kind\":\"%s\",\"bounds\":[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32
               ",%" PRId32 ",%" PRId32 "]}",
               i == 0 ? "" : ",", type->kind == MC_LIGHT_SKY ? "Sky" : "Block",
               fields->bounds.min_x, fields->bounds.min_y, fields->bounds.min_z,
               fields->bounds.max_x, fields->bounds.max_y, fields->bounds.max_z);
    }
    return printf("]}") >= 0;
}

static bool fr_entity(const fr_observation *o, const mc_entity *entity) {
    mc_entity_snapshot s;
    mc_primed_tnt_entity_snapshot t;
    if (mc_entity_get_snapshot(entity, &s) != MC_ENTITY_OK ||
        mc_primed_tnt_entity_get_snapshot(entity, &t) != MC_ENTITY_OK || s.bounds == NULL)
        return false;
    const mc_entity_base_fields *b = &s.base;
    printf("{\"class\":\"qw\",\"id\":%" PRId32 ",\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64
           "\",\"%016" PRIx64 "\"],\"motion\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
           "\"],\"previous\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
           "\"],\"dimensions\":[\"%08" PRIx32 "\",\"%08" PRIx32 "\"],\"offset\":\"%08" PRIx32
           "\",\"bounds\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
           "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],\"fuse\":%" PRId32
           ",\"prevent_spawning\":%s,\"removed\":%s,\"on_ground\":%s,\"world_same\":%s}",
           b->id, fr_double(b->x), fr_double(b->y), fr_double(b->z), fr_double(b->velocity_x),
           fr_double(b->velocity_y), fr_double(b->velocity_z), fr_double(b->aJ), fr_double(b->aK),
           fr_double(b->aL), fr_float(b->width), fr_float(b->height), fr_float(b->offset),
           fr_double(s.bounds->min_x), fr_double(s.bounds->min_y), fr_double(s.bounds->min_z),
           fr_double(s.bounds->max_x), fr_double(s.bounds->max_y), fr_double(s.bounds->max_z),
           t.fuse, fr_bool(b->aF), fr_bool(b->be), fr_bool(b->aX),
           fr_bool(b->world == o->f->worlds[0]));
    return !ferror(stdout);
}

static bool fr_entities(const fr_observation *o) {
    const mc_world_reference_list *list = o->f->worlds[0]->fields.b;
    if (list == NULL)
        return false;
    putchar('[');
    bool first = true;
    for (size_t i = 0; i < list->count; ++i) {
        const mc_entity *entity = list->items[i];
        if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_PRIMED_TNT))
            continue;
        if (!first)
            putchar(',');
        if (!fr_entity(o, entity))
            return false;
        first = false;
    }
    return putchar(']') != EOF;
}

static bool fr_players(const fr_observation *o) {
    const mc_world_reference_list *list = o->f->worlds[0]->fields.d;
    if (list == NULL)
        return false;
    putchar('[');
    for (size_t i = 0; i < list->count; ++i) {
        mc_entity_snapshot s;
        mc_local_player_snapshot p;
        if (mc_entity_get_snapshot(list->items[i], &s) != MC_ENTITY_OK || s.bounds == NULL ||
            mc_local_player_get_snapshot(list->items[i], &p) != MC_ENTITY_OK)
            return false;
        const mc_entity_base_fields *b = &s.base;
        printf("%s{\"class\":\"dc\",\"id\":%d,\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64
               "\",\"%016" PRIx64 "\"],\"motion\":[\"%016" PRIx64 "\",\"%016" PRIx64
               "\",\"%016" PRIx64 "\"],\"dimensions\":[\"%08" PRIx32 "\",\"%08" PRIx32
               "\"],\"offset\":\"%08" PRIx32 "\",\"bounds\":[\"%016" PRIx64 "\",\"%016" PRIx64
               "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
               "\"],\"removed\":%s,\"on_ground\":%s,\"health\":%d,\"sleeping\":%s}",
               i == 0 ? "" : ",", b->id, fr_double(b->x), fr_double(b->y), fr_double(b->z),
               fr_double(b->velocity_x), fr_double(b->velocity_y), fr_double(b->velocity_z),
               fr_float(b->width), fr_float(b->height), fr_float(b->offset),
               fr_double(s.bounds->min_x), fr_double(s.bounds->min_y), fr_double(s.bounds->min_z),
               fr_double(s.bounds->max_x), fr_double(s.bounds->max_y), fr_double(s.bounds->max_z),
               fr_bool(b->be), fr_bool(b->aX), p.living.Y, fr_bool(p.player.u));
    }
    return putchar(']') != EOF;
}

static bool fr_event_common(fr_observation *o, mc_world_runtime *world, const char *kind) {
    int32_t target[2];
    if (o->events >= FR_MAX_EVENTS || world != o->f->worlds[0] ||
        !fr_cell_read(o, o->profile->xyz[0], o->profile->xyz[1], o->profile->xyz[2], target))
        return false;
    ++o->events;
    printf("FIRE_EVENT {\"kind\":\"%s\",\"target\":[%d,%d],\"world_o\":%s,\"world_random\":", kind,
           target[0], target[1], fr_bool(world->fields.o));
    fr_random(world->fields.r);
    printf(",\"math_random\":");
    fr_math(o->f);
    printf(",\"entities\":");
    return fr_entities(o);
}

static mc_world_storage_status fr_event_finish(fr_observation *o, int kind,
                                               mc_world_storage_error *error,
                                               mc_world_access_failure *failure) {
    puts("}");
    if (!o->valid || ferror(stdout))
        return MC_WORLD_STORAGE_IO_ERROR;
    if (kind < 0)
        return MC_WORLD_STORAGE_OK;
    const size_t ordinal = ++o->kinds[kind];
    if (o->profile->errors[kind] == 0 || ordinal != (size_t)o->profile->errors[kind])
        return MC_WORLD_STORAGE_OK;
    if (o->receipt != NULL)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    const mc_world_storage_status status =
        mc_world_access_receipt_create(o->listener, NULL, &o->receipt, failure, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    o->receipt->exception_class = "java/lang/IllegalStateException";
    *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "Fire actual observer interruption", (uint32_t)ordinal};
    return error->status;
}

static mc_world_storage_status fr_cell_event(void *context, mc_world_runtime *world, int32_t x,
                                             int32_t y, int32_t z, mc_world_storage_error *error,
                                             mc_world_access_failure *failure) {
    fr_observation *o = context;
    int32_t current[2] = {-1, -1};
    o->valid &= fr_event_common(o, world, "cell") && fr_cell_read(o, x, y, z, current);
    printf(",\"xyz\":[%d,%d,%d],\"cell\":[%d,%d]", x, y, z, current[0], current[1]);
    return fr_event_finish(o, 0, error, failure);
}

static mc_world_storage_status fr_range_event(void *context, mc_world_runtime *world, int32_t x0,
                                              int32_t y0, int32_t z0, int32_t x1, int32_t y1,
                                              int32_t z1, mc_world_storage_error *error,
                                              mc_world_access_failure *failure) {
    fr_observation *o = context;
    o->valid &= fr_event_common(o, world, "range");
    printf(",\"xyz\":[%d,%d,%d,%d,%d,%d]", x0, y0, z0, x1, y1, z1);
    return fr_event_finish(o, 1, error, failure);
}

static mc_world_storage_status fr_added_event(void *context, mc_world_runtime *world,
                                              mc_entity *entity, mc_world_storage_error *error,
                                              mc_world_access_failure *failure) {
    fr_observation *o = context;
    o->valid &= fr_event_common(o, world, "added");
    printf(",\"entity\":");
    o->valid &= fr_entity(o, entity);
    return fr_event_finish(o, 2, error, failure);
}

static mc_world_storage_status fr_sound_event(void *context, mc_world_runtime *world,
                                              const mc_registry_string *name, double x, double y,
                                              double z, float volume, float pitch,
                                              mc_world_storage_error *error,
                                              mc_world_access_failure *failure) {
    fr_observation *o = context;
    char text[64] = {0};
    o->valid &= fr_event_common(o, world, "sound") && fr_name(name, text);
    printf(",\"name\":\"%s\",\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
           "\"],\"volume\":\"%08" PRIx32 "\",\"pitch\":\"%08" PRIx32 "\"",
           text, fr_double(x), fr_double(y), fr_double(z), fr_float(volume), fr_float(pitch));
    return fr_event_finish(o, 3, error, failure);
}

static mc_world_storage_status fr_sky_event(void *context, mc_world_runtime *world,
                                            mc_world_storage_error *error,
                                            mc_world_access_failure *failure) {
    fr_observation *o = context;
    o->valid &= fr_event_common(o, world, "sky");
    return fr_event_finish(o, -1, error, failure);
}

static bool fr_state(fr_observation *o, const char *phase) {
    mc_world_runtime *world = o->f->worlds[0];
    const mc_chunk_cache *cache = o->f->providers[0].cache;
    if (!mc_chunk_cache_validate(cache) || cache->count != o->chunk_count ||
        cache->insertion_count != o->chunk_count ||
        mc_world_store_chunk_count(o->f->stores[0]) != o->chunk_count)
        return false;
    for (size_t i = 0; i < o->chunk_count; ++i) {
        size_t j = 0;
        while (j < cache->insertion_count && cache->insertions[j] != o->chunks[i])
            ++j;
        if (j == cache->insertion_count)
            return false;
    }
    int32_t target[2], rain_time, thunder_time;
    int64_t time;
    bool raining, thundering;
    if (!fr_cell_read(o, o->profile->xyz[0], o->profile->xyz[1], o->profile->xyz[2], target) ||
        mc_world_info_time_get(world->fields.x, &time) != MC_WORLD_INFO_OK ||
        mc_world_info_source_rain_time_get(world->fields.x, &rain_time) != MC_WORLD_INFO_OK ||
        mc_world_info_source_thunder_time_get(world->fields.x, &thunder_time) != MC_WORLD_INFO_OK ||
        mc_world_info_source_raining_get(world->fields.x, &raining) != MC_WORLD_INFO_OK ||
        mc_world_info_source_thundering_get(world->fields.x, &thundering) != MC_WORLD_INFO_OK)
        return false;
    printf("FIRE_STATE {\"phase\":\"%s\",\"target\":[%d,%d],\"world_random\":", phase, target[0],
           target[1]);
    fr_random(world->fields.r);
    printf(",\"math_random\":");
    fr_math(o->f);
    printf(",\"time\":\"%016" PRIx64 "\",\"world_o\":%s,\"sky_seen\":%s,\"scheduled\":",
           (uint64_t)time, fr_bool(world->fields.o), fr_bool(world->classes->chunk_a));
    if (!fr_scheduled(world))
        return false;
    printf(",\"lighting\":");
    if (!fr_lighting(world))
        return false;
    const mc_block *fire = NULL;
    if (mc_registry_block_static_get(o->f->registry[0], "as", &fire, NULL) != MC_REGISTRY_OK ||
        fire == NULL)
        return false;
    printf(",\"fire_arrays\":{\"encouragement\":");
    if (fire->subtype.yq.a == NULL)
        printf("null");
    else {
        putchar('[');
        for (size_t i = 0; i < 256; ++i)
            printf("%s%d", i == 0 ? "" : ",", fire->subtype.yq.a[i]);
        putchar(']');
    }
    printf(",\"flammability\":");
    if (fire->subtype.yq.b == NULL)
        printf("null");
    else {
        putchar('[');
        for (size_t i = 0; i < 256; ++i)
            printf("%s%d", i == 0 ? "" : ",", fire->subtype.yq.b[i]);
        putchar(']');
    }
    printf("},\"entities\":");
    if (!fr_entities(o))
        return false;
    printf(",\"entity_class\":{\"initialized\":%s,\"next_id\":%d},\"entity_count\":%zu,\"player_"
           "count\":%zu,\"players\":",
           fr_bool(o->f->entities[0]->entity_phase == MC_ENTITY_CLASS_READY),
           o->f->entities[0]->next_id, world->fields.b->count, world->fields.d->count);
    if (!fr_players(o))
        return false;
    printf(",\"random_work\":{\"g\":%" PRId32 ",\"cooldown\":%" PRId32 ",\"candidates\":[",
           world->fields.g, world->fields.Q);
    int32_t keys[FR_MAX_CHUNKS];
    size_t key_count = 0;
    if (world->fields.P == NULL ||
        (world->fields.P->order != NULL &&
         mc_java_int_set_copy_order(world->fields.P->order, keys, COUNT(keys), &key_count) !=
             MC_JAVA_INT_SET_OK))
        return false;
    for (size_t i = 0; i < key_count; ++i) {
        size_t j = 0;
        while (j < world->fields.P->count && world->fields.P->entries[j].hash != keys[i])
            ++j;
        if (j == world->fields.P->count)
            return false;
        printf("%s[%d,%d]", i == 0 ? "" : ",", world->fields.P->entries[j].x,
               world->fields.P->entries[j].z);
    }
    printf("]},\"weather\":{\"rain_previous\":\"%08" PRIx32 "\",\"rain_current\":\"%08" PRIx32
           "\",\"thunder_previous\":\"%08" PRIx32 "\",\"thunder_current\":\"%08" PRIx32
           "\",\"rain_time\":%d,\"thunder_time\":%d,\"raining\":%s,\"thundering\":%s},\"chunk_"
           "planes\":[",
           fr_float(world->fields.i), fr_float(world->fields.j), fr_float(world->fields.k),
           fr_float(world->fields.l), rain_time, thunder_time, fr_bool(raining),
           fr_bool(thundering));
    for (size_t i = 0; i < o->chunk_count; ++i) {
        printf("%s{\"xyz\":[%d,%d],\"planes\":", i == 0 ? "" : ",", o->chunks[i]->x,
               o->chunks[i]->z);
        if (!fr_planes(o->chunks[i]->data))
            return false;
        putchar('}');
    }
    return puts("]}") >= 0;
}

static bool fr_player(fr_observation *o) {
    bool ok = false;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL, *token = NULL;
    const uint16_t user[] = {'F', 'i', 'r', 'e', 'T', 'i', 'c', 'k'}, sid[] = {'-'};
    CHECK(mc_player_session_classes_create(o->f->entities[0], o->f->registry[0], &o->sessions,
                                           NULL) == MC_PLAYER_OK &&
          mc_player_string_create(user, COUNT(user), &username) == MC_PLAYER_OK &&
          mc_player_string_create(sid, COUNT(sid), &token) == MC_PLAYER_OK &&
          mc_player_session_create(o->sessions, username, token, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(o->f->entities[0], &o->player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(o->player, o->f->worlds[0], NULL, session, 0, NULL, NULL) ==
              MC_PLAYER_OK &&
          mc_entity_set_position(o->player, -.5, 64.0, 32.5) == MC_ENTITY_OK);
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(o->f->worlds[0], o->player, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    ok = true;
cleanup:
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    mc_player_string_release(token);
    return ok;
}

static bool fr_seed_record(fr_observation *o, int32_t cx, int32_t cz) {
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
    r->root.type = r->level.type = MC_NBT_COMPOUND;
    r->level.name = ascii("Level", r->names[11]);
    for (size_t i = 0; i < COUNT(names); ++i) {
        r->fields[i].type = types[i];
        r->fields[i].name = ascii(names[i], r->names[i]);
        r->level_fields[i] = &r->fields[i];
    }
    r->fields[0].value.int_value = cx;
    r->fields[1].value.int_value = cz;
    r->fields[8].value.byte_value = 1;
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z)
            for (int32_t y = 0; y < 128; ++y) {
                const input_cell value = fr_prepared(cx * 16 + x, y, cz * 16 + z);
                r->arrays[0][cell(x, y, z)] = value.id;
                CHECK(mc_chunk_nibbles_set(r->arrays[1], MC_CHUNK_NIBBLE_BYTES, x, y, z,
                                           value.metadata));
            }
    for (size_t i = 0; i < o->profile->height_count; ++i) {
        const int32_t *h = o->profile->heights[i];
        if (mc_java_i32_shr(h[0], 4) == cx && mc_java_i32_shr(h[1], 4) == cz)
            r->arrays[4][((size_t)h[1] & 15u) * 16u + ((size_t)h[0] & 15u)] = (uint8_t)h[2];
    }
    for (size_t i = 0; i < COUNT(lengths); ++i) {
        r->fields[i + 3].value.byte_array.data = r->arrays[i];
        r->fields[i + 3].value.byte_array.length = lengths[i];
    }
    r->fields[9].value.list.element_type = r->fields[10].value.list.element_type = MC_NBT_COMPOUND;
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
    uint32_t growth;
    CHECK(mc_world_regions_write(o->f->regions, mc_world_store_root(o->f->stores[0]), cx, cz,
                                 encoded, length, fixture_timestamp, &growth,
                                 NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    free(encoded);
    free(r);
    return ok;
}

static bool fr_setup(fr_observation *o, const char *root) {
    bool ok = false;
    fixture *f = o->f = calloc(1, sizeof(*f));
    char path[8192];
    const uint16_t title[] = {'F', 'i', 'r', 'e'};
    uint8_t map[256];
    CHECK(f != NULL && snprintf(path, sizeof path, "%s/world0", root) > 0);
    CHECK(mc_math_classes_create(SIZE_MAX, &o->math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(o->math, NULL) == MC_MATH_CLASS_OK &&
          mc_materials_create(&f->materials) &&
          mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options store_options = {.session_token = 127,
                                            .block_map = map,
                                            .block_map_length = COUNT(map),
                                            .limits = mc_world_store_default_limits()};
    store_options.limits.max_loaded_chunks = FR_MAX_CHUNKS;
    CHECK(mc_world_store_open(path, f->regions, &store_options, &f->stores[0], NULL) ==
          MC_WORLD_STORAGE_OK);
    const mc_level_info info = {.seed = 23,
                                .spawn_x = 8,
                                .spawn_y = 64,
                                .spawn_z = 8,
                                .time = 0,
                                .level_name = {title, COUNT(title)},
                                .version = 19132};
    CHECK(mc_world_store_save_info(f->stores[0], &info, 123, NULL) == MC_WORLD_STORAGE_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    r->streams[1] = (stream){.bytes = language};
    r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options registry_options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                                  &r->api};
    CHECK(mc_registry_classes_allocate(&registry_options, &f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile,
                                          mc_math_classes_table(o->math), &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
              MC_REGISTRY_OK &&
          mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities[0]) ==
              MC_ENTITY_OK &&
          mc_world_runtime_classes_bind_entities(f->classes[0], f->entities[0]) == MC_ENTITY_OK &&
          mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK && !r->bad);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = FR_MAX_CHUNKS;
    CHECK(mc_world_provider_create(0, NULL, &f->dimensions[0], NULL) == MC_WORLD_PROVIDER_OK &&
          mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, f->classes[0],
                                          &limits, &f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {
        .store = f->stores[0], .name = f->name, .seed = 23, .dimension = f->dimensions[0]};
    CHECK(mc_world_runtime_construct_world(f->worlds[0], &arguments, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_provider_bind_fields(f->worlds[0]->fields.v, &f->providers[0], NULL) ==
              MC_WORLD_STORAGE_OK);
    f->bound[0] = true;
    fr_prepared_profile = o->profile;
    const int32_t low_x = o->profile->random_player ? -10 : 0;
    const int32_t high_x = o->profile->random_player ? 8 : 1;
    const int32_t low_z = o->profile->random_player ? -7 : 0;
    const int32_t high_z = o->profile->random_player ? 11 : 1;
    for (int32_t cx = low_x; cx <= high_x; ++cx)
        for (int32_t cz = low_z; cz <= high_z; ++cz) {
            CHECK(fr_seed_record(o, cx, cz));
            mc_chunk_state *state = NULL;
            CHECK(mc_world_runtime_get(f->worlds[0], cx, cz, &state) == MC_CHUNK_ACQUIRE_OK &&
                  state != NULL && !state->blank);
            o->chunks[o->chunk_count++] = state;
        }
    CHECK(f->providers[0].cache->count == o->chunk_count &&
          mc_world_store_chunk_count(f->stores[0]) == o->chunk_count);
    mc_world_runtime *world = f->worlds[0];
    f->entities[0]->entity_phase =
        o->profile->entity_initialized ? MC_ENTITY_CLASS_READY : MC_ENTITY_CLASS_COLD;
    f->entities[0]->next_id = o->profile->entity_next;
    if (o->profile->random_player)
        CHECK(fr_player(o));
    const mc_biome *biome = NULL;
    mc_world_chunk_manager *manager = NULL;
    const mc_world_provider_registry_binding binding = {f->registry[0], MC_REGISTRY_CLIENT,
                                                        f->profile};
    CHECK(mc_biome_static_get(f->registry[0], o->profile->biome, &biome, NULL) == MC_REGISTRY_OK &&
          mc_world_chunk_manager_fixed_create_game(f->dimensions[0], &binding, biome, .5, .5,
                                                   &manager, NULL) == MC_WORLD_PROVIDER_OK);
    const mc_world_provider_status assigned =
        mc_world_provider_assign_chunk_manager_game(f->dimensions[0], manager, NULL);
    mc_world_chunk_manager_release(manager);
    CHECK(assigned == MC_WORLD_PROVIDER_OK);
    world->fields.N = world->fields.O = false;
    world->fields.i = world->fields.j = o->profile->rain ? 1.0f : 0.0f;
    world->fields.k = world->fields.l = 0.0f;
    world->fields.f = o->profile->rain ? 5 : 0;
    world->fields.g = o->profile->g;
    world->fields.Q = o->profile->cooldown;
    CHECK(mc_world_info_source_rain_time_set(world->fields.x, 20) == MC_WORLD_INFO_OK &&
          mc_world_info_source_thunder_time_set(world->fields.x, 30) == MC_WORLD_INFO_OK &&
          mc_world_info_source_raining_set(world->fields.x, o->profile->rain) == MC_WORLD_INFO_OK &&
          mc_world_info_source_thundering_set(world->fields.x, false) == MC_WORLD_INFO_OK &&
          mc_world_info_source_time_set(world->fields.x, 0) == MC_WORLD_INFO_OK);
    mc_java_random_reseed(world->fields.r, o->profile->seed);
    double warmup;
    CHECK(mc_java_math_random(f->profile, &warmup) == MC_JAVA_RANDOM_PROFILE_OK &&
          f->profile->math != NULL);
    mc_java_random seeded;
    mc_java_random_seed(&seeded, 0);
    atomic_store_explicit(&f->profile->math->state, seeded.state, memory_order_relaxed);
    f->profile->math->have_next_next_gaussian = false;
    f->profile->math->next_next_gaussian = 0.0;
    f->classes[0]->scheduled_initialized = o->profile->scheduled_initialized;
    f->classes[0]->scheduled_sequence.next = o->profile->scheduled_before;
    if (o->profile->prequeue_delay >= 0)
        CHECK(mc_world_runtime_schedule(world, o->profile->xyz[0], o->profile->xyz[1],
                                        o->profile->xyz[2], o->profile->entry_id,
                                        o->profile->prequeue_delay, NULL) == MC_WORLD_STORAGE_OK);
    const mc_block *fire = NULL;
    CHECK(mc_registry_block_static_get(f->registry[0], "as", &fire, NULL) == MC_REGISTRY_OK &&
          fire != NULL);
    o->encouragement = fire->subtype.yq.a;
    o->flammability = fire->subtype.yq.b;
    if (o->profile->null_encouragement)
        ((mc_block *)fire)->subtype.yq.a = NULL;
    if (o->profile->null_flammability)
        ((mc_block *)fire)->subtype.yq.b = NULL;
    o->prepared_provider = world->fields.v;
    if (o->profile->null_provider)
        world->fields.v = NULL;
    ok = true;
cleanup:
    return ok;
}

static bool fr_dispose(fr_observation *o) {
    fixture *f = o->f;
    if (f == NULL)
        return true;
    mc_world_runtime *world = f->worlds[0];
    if (world != NULL && o->prepared_provider != NULL)
        world->fields.v = o->prepared_provider;
    if (f->registry[0] != NULL && o->encouragement != NULL) {
        const mc_block *fire = NULL;
        if (mc_registry_block_static_get(f->registry[0], "as", &fire, NULL) != MC_REGISTRY_OK ||
            fire == NULL)
            return false;
        ((mc_block *)fire)->subtype.yq.a = o->encouragement;
        ((mc_block *)fire)->subtype.yq.b = o->flammability;
    }
    mc_entity_release(o->player);
    o->player = NULL;
    if (mc_player_session_classes_free(&o->sessions, NULL) != MC_PLAYER_OK)
        return false;
    if (world != NULL &&
        mc_world_runtime_retire_entity_containers(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (f->bound[0]) {
        mc_chunk_cache *cache = f->providers[0].cache;
        if (mc_chunk_pending_clear(f->providers[0].pending) != MC_CHUNK_ACQUIRE_OK ||
            !mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                                 cache->insertion_capacity))
            return false;
    }
    if (world != NULL && mc_world_runtime_retire_world(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    for (size_t i = 0; i < o->chunk_count; ++i)
        if (mc_world_store_retire_chunk(f->stores[0], o->chunks[i], NULL) != MC_WORLD_STORAGE_OK)
            return false;
    if (world != NULL && mc_world_runtime_free(world, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    mc_world_provider_release(f->dimensions[0]);
    if (mc_world_runtime_classes_free(f->classes[0], NULL) != MC_WORLD_STORAGE_OK ||
        mc_registry_classes_free(f->registry[0], NULL) != MC_REGISTRY_OK ||
        mc_entity_classes_free(f->entities[0]) != MC_ENTITY_OK)
        return false;
    if (mc_math_classes_release(&o->math, NULL) != MC_MATH_CLASS_OK)
        return false;
    mc_world_info_name_release(f->name);
    mc_world_store_free(f->stores[0]);
    mc_world_regions_free(f->regions);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    free(f);
    o->f = NULL;
    return true;
}

static void fr_input(const fr_profile *p) {
    printf(
        "{\"mode\":\"%s\",\"operation\":\"%s\",\"xyz\":[%d,%d,%d],\"entry_id\":%d,\"source_id\":%d,"
        "\"random_seed\":%" PRId64
        ",\"rain\":%s,\"biome\":\"%c\",\"cell_error_at\":%d,\"range_error_at\":%d,"
        "\"added_error_at\":%d,\"sound_error_at\":%d,\"prequeue_delay\":%d,\"null_encouragement\":%"
        "s,"
        "\"null_flammability\":%s,\"null_provider\":%s,\"random_player\":%s,\"g\":%d,\"cooldown\":%"
        "d,\"cells\":[",
        p->mode, p->operation, p->xyz[0], p->xyz[1], p->xyz[2], p->entry_id, p->source_id, p->seed,
        fr_bool(p->rain), p->biome, p->errors[0], p->errors[1], p->errors[2], p->errors[3],
        p->prequeue_delay, fr_bool(p->null_encouragement), fr_bool(p->null_flammability),
        fr_bool(p->null_provider), fr_bool(p->random_player), p->g, p->cooldown);
    for (size_t i = 0; i < p->cell_count; ++i) {
        const fr_cell *c = &p->cells[i];
        printf("%s[%d,%d,%d,%d,%d]", i == 0 ? "" : ",", c->x, c->y, c->z, c->id, c->metadata);
    }
    printf("],\"heights\":[");
    for (size_t i = 0; i < p->height_count; ++i)
        printf("%s[%d,%d,%d]", i == 0 ? "" : ",", p->heights[i][0], p->heights[i][1],
               p->heights[i][2]);
    printf("]}");
}

static void fr_string(const char *text) {
    if (text == NULL)
        printf("null");
    else
        printf("\"%s\"", text);
}

static const char *fr_exception(const mc_world_mutation_error *e) {
    const char *exception = NULL;
    if (mc_world_access_failure_source_exception(&e->world_access_failure, &exception) ||
        mc_world_access_failure_source_exception(&e->publication.world_access_failure, &exception))
        return exception;
    if (e->fire_source_null || e->stairs_source_null ||
        e->registry.status == MC_REGISTRY_NULL_REFERENCE)
        return "java/lang/NullPointerException";
    if (e->fire_array_index || e->registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE)
        return "java/lang/ArrayIndexOutOfBoundsException";
    return NULL;
}

static bool fr_run(const char *root, const fr_profile *profile) {
    bool ok = false, inserted = false;
    fr_observation o = {.profile = profile, .valid = true};
    mc_world_mutation_error mutation;
    mc_world_mutation_error_init(&mutation);
    mc_world_tick_call *call = NULL;
    mc_world_tick_error admission = {0};
    mc_world_tick_state tick_state = {0};
    bool retained_failure = false, same_receipt = false, selected_source = false;
    mc_world_storage_status receipt_busy = MC_WORLD_STORAGE_OK;
    size_t receipt_before = 0, receipt_after_busy = 0;
    mc_world_access_receipt *producer = NULL;
    char receipt_exception[96] = {0};
    CHECK(fr_setup(&o, root));
    mc_world_runtime *world = o.f->worlds[0];
    const mc_world_access_source_callbacks callbacks = {.cell = fr_cell_event,
                                                        .range = fr_range_event,
                                                        .sound = fr_sound_event,
                                                        .entity_added = fr_added_event,
                                                        .sky_changed = fr_sky_event};
    CHECK(mc_world_access_source_create(&callbacks, &o, &o.listener, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, o.listener, NULL) == MC_WORLD_STORAGE_OK &&
          fr_state(&o, "before"));
    mc_world_storage_status raw;
    bool returned;
    const int32_t x = profile->xyz[0], y = profile->xyz[1], z = profile->xyz[2];
    if (strcmp(profile->operation, "added") == 0) {
        raw = mc_world_runtime_block_set(world, x, y, z, profile->entry_id, &inserted, &mutation);
        returned = raw == MC_WORLD_STORAGE_OK;
    } else if (strcmp(profile->operation, "world-tick") == 0) {
        raw = mc_world_runtime_tick_begin(world, o.math, &call, &admission);
        CHECK(call != NULL && mc_world_runtime_tick_inspect(call, &tick_state));
        returned = tick_state.returned;
        if (tick_state.error->scheduled != NULL)
            mutation = tick_state.error->scheduled->call.tick;
        else if (tick_state.error->random != NULL && tick_state.error->random->mutation != NULL)
            mutation = *tick_state.error->random->mutation;
    } else {
        mc_registry_block_m_view view;
        const mc_block *block = NULL;
        CHECK(mc_registry_block_m_get(o.f->registry[0], &view, NULL) == MC_REGISTRY_OK &&
              mc_registry_block_m_at(&view, profile->entry_id, &block, NULL) == MC_REGISTRY_OK &&
              block != NULL);
        returned =
            strcmp(profile->operation, "neighbor") == 0
                ? mc_world_mutation_block_neighbor(world, block, x, y, z, profile->source_id,
                                                   &mutation)
                : mc_world_mutation_block_tick(world, block, x, y, z, world->fields.r, &mutation);
        raw = returned ? MC_WORLD_STORAGE_OK : mutation.status;
    }
    CHECK(o.valid && !ferror(stdout));
    const char *exception = returned       ? NULL
                            : call != NULL ? tick_state.error->exception_class
                                           : fr_exception(&mutation);
    retained_failure = call != NULL && o.receipt != NULL;
    if (retained_failure) {
        producer = o.receipt;
        const char *selected_exception = NULL;
        selected_source = mc_world_access_failure_source_exception(&tick_state.error->world_access,
                                                                   &selected_exception);
        same_receipt = tick_state.error->world_access.receipt == producer;
        CHECK(!returned && selected_source && same_receipt && exception != NULL &&
              selected_exception != NULL && strcmp(exception, selected_exception) == 0 &&
              strlen(selected_exception) < sizeof receipt_exception);
        memcpy(receipt_exception, selected_exception, strlen(selected_exception) + 1);
        receipt_before = producer->holds;
        CHECK(receipt_before == 2);
        receipt_busy = mc_world_access_receipt_retire(&o.receipt, NULL);
        CHECK(receipt_busy == MC_WORLD_STORAGE_BUSY && o.receipt == producer);
        receipt_after_busy = producer->holds;
        CHECK(receipt_after_busy == receipt_before);
    }
    printf("FIRE_OUTCOME {\"mode\":\"%s\",\"input\":", profile->mode);
    fr_input(profile);
    printf(",\"prepared_class\":{\"scheduled\":{\"initialized\":%s,\"sequence\":\"%016" PRIx64
           "\"},\"entity\":{\"initialized\":%s,\"next_id\":%d}},\"raw\":%d,\"returned\":%s,"
           "\"result\":",
           fr_bool(profile->scheduled_initialized), (uint64_t)profile->scheduled_before,
           fr_bool(profile->entity_initialized), profile->entity_next, (int)raw, fr_bool(returned));
    if (strcmp(profile->operation, "added") == 0 && returned)
        printf("%s", fr_bool(inserted));
    else
        printf("null");
    printf(",\"exception\":");
    fr_string(exception);
    printf(
        ",\"native_policy\":%s,\"hook\":{\"class\":",
        fr_bool(call != NULL ? tick_state.error->native_policy : !returned && exception == NULL));
    fr_string(mutation.hook_class);
    printf(",\"method\":");
    fr_string(mutation.hook_method);
    printf(",\"pc\":%u},\"cause_hook\":{\"class\":", mutation.hook_bytecode);
    fr_string(mutation.cause_hook_class);
    printf(",\"method\":");
    fr_string(mutation.cause_hook_method);
    printf(",\"pc\":%u},\"source_sites\":{\"null\":%s,\"array_index\":%s,\"query_null\":%s,"
           "\"query_pc\":%u,\"provider_pc\":%u,\"fire\":{\"added\":%u,\"neighbor\":%u,\"tick\":%u,"
           "\"can_place\":%u,\"neighbors\":%u,\"flammable\":%u,\"encouragement\":%u,\"spread\":%u,"
           "\"burn\":%u}},\"world_tick\":",
           mutation.cause_hook_bytecode, fr_bool(mutation.fire_source_null),
           fr_bool(mutation.fire_array_index), fr_bool(mutation.query.source_null),
           mutation.query.bytecode, mutation.query.provider_bytecode, mutation.fire.added,
           mutation.fire.neighbor, mutation.fire.tick, mutation.fire.can_place,
           mutation.fire.neighbors, mutation.fire.flammable, mutation.fire.encouragement,
           mutation.fire.spread, mutation.fire.burn);
    if (call == NULL)
        printf("null");
    else
        printf("{\"pc\":%u,\"scheduled_returned\":%s,\"random_returned\":%s,\"chunks\":%zu,"
               "\"samples\":%zu,\"block_ticks\":%zu}",
               tick_state.source_pc, fr_bool(tick_state.scheduled_returned),
               fr_bool(tick_state.random_returned),
               tick_state.random == NULL ? 0 : tick_state.random->chunks_returned,
               tick_state.random == NULL ? 0 : tick_state.random->samples_returned,
               tick_state.random == NULL ? 0 : tick_state.random->block_ticks);
    puts("}");
    CHECK(fr_state(&o, "after"));
    CHECK(mc_world_runtime_tick_retire(&call, NULL) == MC_WORLD_STORAGE_OK &&
          fr_state(&o, "retired"));
    /* The listener owns the concrete Source receipt. Ending diagnostics and
     * detaching it are native cleanup, with no source replay or state repair. */
    if (retained_failure) {
        CHECK(o.receipt == producer && producer->holds == 1);
        const size_t after_call_holds = producer->holds;
        const mc_world_storage_status retired = mc_world_access_receipt_retire(&o.receipt, NULL);
        CHECK(retired == MC_WORLD_STORAGE_OK && o.receipt == NULL);
        printf("FIRE_RECEIPT {\"busy_before\":%d,\"before_holds\":%zu,\"after_busy_holds\":%zu,"
               "\"after_call_holds\":%zu,\"retire\":%d,\"same_identity\":%s,\"selected_source\":%s,"
               "\"exception\":\"%s\",\"attempts\":2}\n",
               (int)receipt_busy, receipt_before, receipt_after_busy, after_call_holds,
               (int)retired, fr_bool(same_receipt), fr_bool(selected_source), receipt_exception);
    } else if (o.receipt != NULL)
        CHECK(mc_world_access_receipt_retire(&o.receipt, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_remove_world_access(world, o.listener, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted && mc_world_access_free(o.listener, NULL) == MC_WORLD_STORAGE_OK);
    o.listener = NULL;
    CHECK(fr_state(&o, "cleanup"));
    ok = true;
cleanup:
    if (call != NULL && mc_world_runtime_tick_retire(&call, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (o.receipt != NULL &&
        mc_world_access_receipt_retire(&o.receipt, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (o.listener != NULL) {
        bool removed;
        if (o.f != NULL && o.f->worlds[0] != NULL &&
            mc_world_runtime_remove_world_access(o.f->worlds[0], o.listener, &removed, NULL) !=
                MC_WORLD_STORAGE_OK)
            return false;
        if (mc_world_access_free(o.listener, NULL) != MC_WORLD_STORAGE_OK)
            return false;
    }
    return fr_dispose(&o) && ok;
}

static bool fr_integer(const char *text, int64_t *out) {
    if (text == NULL || *text == '\0' || (*text != '-' && (*text < '0' || *text > '9')))
        return false;
    errno = 0;
    char *end;
    const long long value = strtoll(text, &end, 10);
    if (errno != 0 || *end != '\0')
        return false;
    *out = (int64_t)value;
    return true;
}

static bool fr_records(const char *text, int32_t out[FR_MAX_CELLS][5], size_t width,
                       size_t *count) {
    *count = 0;
    if (strcmp(text, "-") == 0)
        return true;
    const char *at = text;
    while (*at != '\0') {
        if (*count == FR_MAX_CELLS)
            return false;
        for (size_t column = 0; column < width; ++column) {
            errno = 0;
            char *end;
            const long value = strtol(at, &end, 10);
            if (end == at || errno != 0 || value < INT32_MIN || value > INT32_MAX ||
                (*end != (column + 1 == width ? ';' : ',') &&
                 !(column + 1 == width && *end == '\0')))
                return false;
            out[*count][column] = (int32_t)value;
            at = *end == '\0' ? end : end + 1;
        }
        ++*count;
    }
    return true;
}

#ifndef MC_FIRE_ENTRY
#define MC_FIRE_ENTRY main
#endif
int MC_FIRE_ENTRY(int argc, char **argv) {
    fr_profile p = {0};
    char root[8192];
    if (argc != 30 || !decode_path(argv[2], root, sizeof root))
        return 2;
    p.mode = argv[1];
    p.operation = argv[3];
    for (const char *c = p.mode; *c != '\0'; ++c)
        if ((*c < 'a' || *c > 'z') && (*c < '0' || *c > '9') && *c != '-')
            return 2;
    if (strcmp(p.operation, "added") != 0 && strcmp(p.operation, "neighbor") != 0 &&
        strcmp(p.operation, "tick") != 0 && strcmp(p.operation, "lava") != 0 &&
        strcmp(p.operation, "world-tick") != 0)
        return 2;
    int64_t values[21] = {0};
    const int arguments[] = {4,  5,  6,  7,  8,  9,  10, 12, 13, 14, 15,
                             16, 20, 21, 22, 23, 24, 25, 26, 27, 28};
    for (size_t i = 0; i < COUNT(arguments); ++i)
        if (!fr_integer(argv[arguments[i]], &values[i]) ||
            (i != 5 && (values[i] < INT32_MIN || values[i] > INT32_MAX)))
            return 2;
    for (size_t i = 0; i < 3; ++i)
        p.xyz[i] = (int32_t)values[i];
    p.entry_id = (int32_t)values[3];
    p.source_id = (int32_t)values[4];
    p.seed = values[5];
    if ((values[6] != 0 && values[6] != 1) || strlen(argv[11]) != 1 ||
        strchr("hik", argv[11][0]) == NULL)
        return 2;
    p.rain = values[6] != 0;
    p.biome = argv[11][0];
    for (size_t i = 0; i < 4; ++i)
        p.errors[i] = (int32_t)values[7 + i];
    p.prequeue_delay = (int32_t)values[11];
    int32_t records[FR_MAX_CELLS][5];
    if (!fr_records(argv[17], records, 5, &p.cell_count))
        return 2;
    for (size_t i = 0; i < p.cell_count; ++i)
        p.cells[i] =
            (fr_cell){records[i][0], records[i][1], records[i][2], records[i][3], records[i][4]};
    if (!fr_records(argv[18], records, 3, &p.height_count) || strlen(argv[19]) != 16)
        return 2;
    for (size_t i = 0; i < p.height_count; ++i)
        for (size_t j = 0; j < 3; ++j)
            p.heights[i][j] = records[i][j];
    uint64_t sequence = 0;
    for (size_t i = 0; i < 16; ++i) {
        const unsigned char c = (unsigned char)argv[19][i];
        const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                   : c >= 'a' && c <= 'f' ? c - 'a' + 10u
                                                          : 16u;
        if (digit > 15)
            return 2;
        sequence = sequence * 16u + digit;
    }
    memcpy(&p.scheduled_before, &sequence, sizeof sequence);
    for (size_t i = 12; i <= 16; ++i)
        if (values[i] != 0 && values[i] != 1)
            return 2;
    p.scheduled_initialized = values[12] != 0;
    p.null_encouragement = values[13] != 0;
    p.null_flammability = values[14] != 0;
    p.null_provider = values[15] != 0;
    p.random_player = values[16] != 0;
    p.g = (int32_t)values[17];
    p.cooldown = (int32_t)values[18];
    if (values[19] != 0 && values[19] != 1)
        return 2;
    p.entity_initialized = values[19] != 0;
    p.entity_next = (int32_t)values[20];
    if (strcmp(argv[29], "none") != 0 || !fr_run(root, &p))
        return 1;
    return printf("PASS: %u World Fire checks;mode=%s\n", checks, p.mode) < 0 ? 1 : 0;
}
