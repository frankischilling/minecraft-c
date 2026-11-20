/* Actual damage/helper calls over genuine App, statistics, World and Mob owners.
 * This observer serializes live fields; it contains no original expected state. */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#define MC_PLAYER_STATISTICS_MAX_PROVIDER_CHUNKS 64
#define main mc_ally_alert_unused_statistics_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/entity/construction_internal.h"
#include "../../../src/entity/living/construction_internal.h"
#include "../../../src/entity/living/fields_internal.h"
#include "../../../src/entity/mob/internal.h"
#include "../../../src/entity/mob/references_internal.h"
#include "../../../src/world/chunk/object_state_internal.h"
#include "../../../src/world/runtime/chunk_provider_internal.h"
#include "../../../src/world/runtime/construction_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/storage/objects_internal.h"
#include "mc/core/math/classes.h"
#include "mc/entity/bounds/bounds.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/movement/effects.h"
#include "mc/entity/player/ally_alert.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/world_access.h"
#include <inttypes.h>

typedef struct aa_fixture {
    fixture base;
    mc_math_classes *math;
    mc_chunk_provider_fields provider;
    mc_chunk_state *chunks[25];
    mc_entity *entities[9], *attacker, *sentinel, *wolves[6];
    const char *roles[9];
    size_t count;
    mc_player_string *ally, *foreign, *token;
    mc_player_session *other_session;
    mc_world_access *observer;
    mc_player_ally_alert_call *call;
    mc_entity_effect_error error;
    mc_entity_watcher *fault_watcher;
    mc_entity *fault_entity;
    mc_entity_watch_map *fault_map;
    mc_entity_watch_node *fault_node, *fault_previous;
    mc_entity_watch_record *fault_record;
    mc_entity_watch_record record_before;
    mc_inventory_player *inventory;
    const mc_player_string *name;
    int32_t input[8];
    size_t sounds;
    double sound_position[3];
    float volume, pitch;
} aa_fixture;

static uint64_t aa_double(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t aa_float(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static const char *aa_bool(bool value) {
    return value ? "true" : "false";
}

static bool aa_string(FILE *out, const mc_player_string *value) {
    if (value == NULL)
        return fputs("null", out) >= 0;
    mc_player_string_view text;
    if (!mc_player_string_read(value, &text) || fputs("{\"utf16\":[", out) < 0)
        return false;
    for (size_t i = 0; i < text.length; ++i)
        if (fprintf(out, "%s%u", i ? "," : "", (unsigned)text.units[i]) < 0)
            return false;
    return fputs("]}", out) >= 0;
}

static bool aa_random(FILE *out, const mc_java_random *value) {
    return fprintf(out, "[\"%016" PRIx64 "\",%s,\"%016" PRIx64 "\"]", value->state,
                   aa_bool(value->have_next_next_gaussian),
                   aa_double(value->next_next_gaussian)) >= 0;
}

static bool aa_id(FILE *out, const mc_entity *entity) {
    return entity == NULL ? fputs("null", out) >= 0
                          : fprintf(out, "%" PRId32, entity->base.id) >= 0;
}

static mc_entity_watch_record *aa_record(mc_entity *entity, int32_t key) {
    for (mc_entity_watch_node *node = entity->watcher->map->head; node; node = node->next)
        if (node->key == key)
            return node->record;
    return NULL;
}

static bool aa_watcher(FILE *out, const mc_entity_watcher *watcher) {
    if (watcher == NULL)
        return fputs("null", out) >= 0;
    if (fprintf(out, "{\"map_null\":%s,\"dirty\":%s,\"records\":[", aa_bool(watcher->map == NULL),
                aa_bool(watcher->dirty)) < 0)
        return false;
    const mc_entity_watch_node *ordered[16];
    size_t count = 0;
    if (watcher->map != NULL)
        for (const mc_entity_watch_node *node = watcher->map->head; node; node = node->next) {
            if (count == 16)
                return false;
            size_t at = count++;
            while (at && ordered[at - 1]->key > node->key) {
                ordered[at] = ordered[at - 1];
                --at;
            }
            ordered[at] = node;
        }
    for (size_t i = 0; i < count; ++i) {
        const mc_entity_watch_record *record = ordered[i]->record;
        if (record == NULL ||
            fprintf(out,
                    "%s{\"key\":%" PRId32 ",\"type\":%" PRId32 ",\"id\":%" PRId32 ",\"value\":",
                    i ? "," : "", ordered[i]->key, record->type, record->index) < 0)
            return false;
        switch (record->payload_kind) {
            case MC_ENTITY_WATCH_PAYLOAD_BYTE:
                if (fprintf(out, "%d", (int)record->byte_value) < 0)
                    return false;
                break;
            case MC_ENTITY_WATCH_PAYLOAD_INTEGER:
                if (fprintf(out, "%" PRId32, record->integer_value) < 0)
                    return false;
                break;
            case MC_ENTITY_WATCH_PAYLOAD_STRING:
                if (!aa_string(out, record->string_value))
                    return false;
                break;
            case MC_ENTITY_WATCH_PAYLOAD_NULL:
                if (fputs("null", out) < 0)
                    return false;
                break;
            default:
                return false;
        }
        if (fprintf(out, ",\"dirty\":%s}", aa_bool(record->dirty)) < 0)
            return false;
    }
    return fputs("]}", out) >= 0;
}

static bool aa_field_i(FILE *out, const char *name, int32_t value, bool *first) {
    const bool result = fprintf(out, "%s\"%s\":%" PRId32, *first ? "" : ",", name, value) >= 0;
    *first = false;
    return result;
}

static bool aa_field_b(FILE *out, const char *name, bool value, bool *first) {
    const bool result = fprintf(out, "%s\"%s\":%s", *first ? "" : ",", name, aa_bool(value)) >= 0;
    *first = false;
    return result;
}

static bool aa_field_f(FILE *out, const char *name, float value, bool *first) {
    const bool result = fprintf(out, "%s\"%s\":{\"F\":\"%08" PRIx32 "\"}", *first ? "" : ",", name,
                                aa_float(value)) >= 0;
    *first = false;
    return result;
}

static bool aa_field_d(FILE *out, const char *name, double value, bool *first) {
    const bool result = fprintf(out, "%s\"%s\":{\"D\":\"%016" PRIx64 "\"}", *first ? "" : ",", name,
                                aa_double(value)) >= 0;
    *first = false;
    return result;
}

static bool aa_entity(FILE *out, const mc_entity *entity, const char *role) {
    const mc_entity_base_fields *b = &entity->base;
    const mc_living_entity_fields *living = mc_entity_living_fields((mc_entity *)entity);
    bool first = true;
    if (living == NULL ||
        fprintf(out, "{\"role\":\"%s\",\"class\":\"%s\",\"id\":%" PRId32 ",\"fields\":{", role,
                entity->class_literal->client_name, b->id) < 0)
        return false;
#define AAI(name, value)                                                                           \
    do {                                                                                           \
        if (!aa_field_i(out, name, value, &first))                                                 \
            return false;                                                                          \
    } while (0)
#define AAB(name, value)                                                                           \
    do {                                                                                           \
        if (!aa_field_b(out, name, value, &first))                                                 \
            return false;                                                                          \
    } while (0)
#define AAF(name, value)                                                                           \
    do {                                                                                           \
        if (!aa_field_f(out, name, value, &first))                                                 \
            return false;                                                                          \
    } while (0)
#define AAD(name, value)                                                                           \
    do {                                                                                           \
        if (!aa_field_d(out, name, value, &first))                                                 \
            return false;                                                                          \
    } while (0)
    AAI("sn.aD", b->id);
    AAD("sn.aM", b->x);
    AAD("sn.aN", b->y);
    AAD("sn.aO", b->z);
    AAD("sn.aP", b->velocity_x);
    AAD("sn.aQ", b->velocity_y);
    AAD("sn.aR", b->velocity_z);
    AAD("sn.aJ", b->aJ);
    AAD("sn.aK", b->aK);
    AAD("sn.aL", b->aL);
    AAD("sn.bl", b->bl);
    AAD("sn.bm", b->bm);
    AAD("sn.bn", b->bn);
    AAF("sn.aS", b->yaw);
    AAF("sn.aT", b->aT);
    AAF("sn.aU", b->aU);
    AAF("sn.aV", b->aV);
    AAF("sn.bg", b->width);
    AAF("sn.bh", b->height);
    AAF("sn.bf", b->offset);
    AAB("sn.bb", b->bb);
    AAB("sn.be", b->be);
    AAI("ls.E", living->E);
    AAI("ls.Y", living->Y);
    AAI("ls.Z", living->Z);
    AAI("sn.by", b->by);
    AAI("ls.aa", living->aa);
    AAI("ls.ab", living->ab);
    AAF("ls.ac", living->ac);
    AAF("ls.al", living->al);
    AAI("ls.au", living->au);
    AAI("ls.av", living->av);
    AAB("ls.ah", living->ah);
    if (entity->local_player != NULL) {
        const mc_player_fields *p = &entity->local_player->fields.player;
        AAI("gs.bO", p->private_bO);
        AAB("gs.u", p->u);
        AAI("gs.g", p->g);
    }
#undef AAI
#undef AAB
#undef AAF
#undef AAD
    if (fputs("},\"bounds\":", out) < 0)
        return false;
    const mc_entity_bounds *box = entity->bounds;
    if (box == NULL) {
        if (fputs("null", out) < 0)
            return false;
    } else if (fprintf(out,
                       "[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                       "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]",
                       aa_double(box->min_x), aa_double(box->min_y), aa_double(box->min_z),
                       aa_double(box->max_x), aa_double(box->max_y), aa_double(box->max_z)) < 0)
        return false;
    return fputs(",\"watcher\":", out) >= 0 && aa_watcher(out, entity->watcher) &&
           fputs(",\"target\":", out) >= 0 &&
           aa_id(out, entity->mob ? entity->mob->creature_target : NULL) &&
           fputs(",\"random\":", out) >= 0 && aa_random(out, entity->random) &&
           fputc('}', out) != EOF;
}

static bool aa_ids(FILE *out, const mc_world_reference_list *list) {
    if (fputc('[', out) == EOF)
        return false;
    for (size_t i = 0; i < list->count; ++i)
        if ((i && fputc(',', out) == EOF) || !aa_id(out, list->items[i]))
            return false;
    return fputc(']', out) != EOF;
}

static bool aa_runs(FILE *out, const uint8_t *bytes, size_t length) {
    if (fprintf(out, "{\"length\":%zu,\"runs\":[", length) < 0)
        return false;
    bool first = true;
    for (size_t i = 0; i < length;) {
        size_t end = i + 1;
        while (end < length && bytes[end] == bytes[i])
            ++end;
        if (bytes[i] &&
            fprintf(out, "%s[%zu,%zu,%u]", first ? "" : ",", i, end - i, (unsigned)bytes[i]) < 0)
            return false;
        if (bytes[i])
            first = false;
        i = end;
    }
    return fputs("]}", out) >= 0;
}

static bool aa_chunk(FILE *out, const mc_world_store *store, const mc_chunk_state *state) {
    const mc_world_chunk *chunk = mc_world_store_chunk_object(store, state);
    if (chunk == NULL ||
        fprintf(out, "{\"x\":%" PRId32 ",\"z\":%" PRId32 ",\"slices\":[", state->x, state->z) < 0)
        return false;
    for (size_t slice = 0; slice < 8; ++slice) {
        const mc_world_chunk_entity_list *list = chunk->m->items[slice];
        if (fprintf(out, "%s[", slice ? "," : "") < 0)
            return false;
        for (size_t i = 0; i < list->count; ++i)
            if ((i && fputc(',', out) == EOF) || !aa_id(out, list->items[i]))
                return false;
        if (fputc(']', out) == EOF)
            return false;
    }
    return fputs("],\"raw_planes\":{\"blocks\":", out) >= 0 &&
           aa_runs(out, state->data->blocks, MC_CHUNK_BLOCKS) &&
           fputs(",\"metadata\":", out) >= 0 &&
           aa_runs(out, state->data->metadata, MC_CHUNK_NIBBLE_BYTES) &&
           fputs(",\"sky\":", out) >= 0 &&
           aa_runs(out, state->data->skylight, MC_CHUNK_NIBBLE_BYTES) &&
           fputs(",\"block_light\":", out) >= 0 &&
           aa_runs(out, state->data->block_light, MC_CHUNK_NIBBLE_BYTES) &&
           fputs(",\"height\":", out) >= 0 &&
           aa_runs(out, state->data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES) && fputs("}}", out) >= 0;
}

static bool aa_stats_map(FILE *out, const mc_client_stats_map *map) {
    const stats_entry *ordered[128];
    size_t count = 0;
    for (const stats_entry *entry = map->head; entry; entry = entry->next) {
        if (count == 128)
            return false;
        size_t at = count++;
        while (at && mc_statistic_fields_get(ordered[at - 1]->key)->e >
                         mc_statistic_fields_get(entry->key)->e) {
            ordered[at] = ordered[at - 1];
            --at;
        }
        ordered[at] = entry;
    }
    if (fputc('[', out) == EOF)
        return false;
    for (size_t i = 0; i < count; ++i)
        if (fprintf(out, "%s[%" PRId32 ",%" PRId32 "]", i ? "," : "",
                    mc_statistic_fields_get(ordered[i]->key)->e, ordered[i]->value) < 0)
            return false;
    return fputc(']', out) != EOF;
}

static bool aa_state(aa_fixture *f, const char *output, const char *phase) {
    char path[8192];
    CHECK(snprintf(path, sizeof(path), "%s/case%" PRId32 "-%s.json", output, f->input[0], phase) >
          0);
    FILE *out = fopen(path, "wb");
    CHECK(out != NULL);
    const mc_local_player_snapshot *p = &f->base.player->local_player->fields;
    CHECK(fputs("{\"actor_name\":", out) >= 0 && aa_string(out, p->player.l));
    CHECK(fprintf(out,
                  ",\"actor_world_null\":%s,\"actor_app_null\":%s,\"actor_inventory_null\":%s,"
                  "\"world_entities\":",
                  aa_bool(f->base.player->base.world == NULL), aa_bool(p->minecraft == NULL),
                  aa_bool(p->player.c == NULL)) >= 0 &&
          aa_ids(out, f->base.world->fields.b) && fputs(",\"world_players\":", out) >= 0 &&
          aa_ids(out, f->base.world->fields.d));
    CHECK(fputs(",\"chunks\":[", out) >= 0);
    for (size_t i = 0; i < 25; ++i)
        CHECK((i == 0 || fputc(',', out) != EOF) && aa_chunk(out, f->base.store, f->chunks[i]));
    CHECK(fputs("],\"statistics\":{\"total\":", out) >= 0 &&
          aa_stats_map(out, f->base.app.stats_writer->total) && fputs(",\"unsent\":", out) >= 0 &&
          aa_stats_map(out, f->base.app.stats_writer->unsent));
    CHECK(fprintf(out, ",\"dirty\":%s},\"box_pool_used\":%" PRId32 ",\"events\":[",
                  aa_bool(f->base.app.stats_writer->dirty),
                  f->base.app.entities->bounds_cursor) >= 0);
    if (f->sounds)
        CHECK(fprintf(out,
                      "{\"event\":\"sound\",\"name\":\"random.hurt\",\"position\":[\"%016" PRIx64
                      "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],\"volume\":\"%08" PRIx32
                      "\",\"pitch\":\"%08" PRIx32 "\"}",
                      aa_double(f->sound_position[0]), aa_double(f->sound_position[1]),
                      aa_double(f->sound_position[2]), aa_float(f->volume),
                      aa_float(f->pitch)) >= 0);
    CHECK(fputs("],\"entities\":[", out) >= 0);
    for (size_t i = 0; i < f->count; ++i)
        CHECK((i == 0 || fputc(',', out) != EOF) && aa_entity(out, f->entities[i], f->roles[i]));
    CHECK(fputs("],\"world_random\":", out) >= 0 && aa_random(out, f->base.world->fields.r) &&
          fputs("}\n", out) >= 0 && fclose(out) == 0);
    return true;
}

static mc_world_storage_status aa_sound(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, float volume, float pitch,
                                        mc_world_storage_error *error) {
    aa_fixture *f = context;
    mc_registry_string_view text;
    if (world != f->base.world || f->sounds || !mc_registry_string_read(name, &text) ||
        text.length != 11)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    static const char expected[] = "random.hurt";
    for (size_t i = 0; i < text.length; ++i)
        if (text.units[i] != (uint16_t)expected[i])
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    f->sound_position[0] = x;
    f->sound_position[1] = y;
    f->sound_position[2] = z;
    f->volume = volume;
    f->pitch = pitch;
    ++f->sounds;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool aa_publish(aa_fixture *f, mc_entity *entity, const char *role, double x, double z,
                       float yaw) {
    mc_entity_position_rotation_error pose;
    CHECK(f->count < 9 &&
          mc_entity_source_position_rotation(entity, x, 64, z, yaw, 0, &pose) == MC_ENTITY_OK);
    entity->base.velocity_x = entity->base.velocity_y = entity->base.velocity_z = 0;
    bool inserted = false;
    mc_world_entity_error error;
    CHECK(mc_world_runtime_insert_entity(f->base.world, entity, &inserted, &error) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    f->entities[f->count] = entity;
    f->roles[f->count++] = role;
    return true;
}

static bool aa_mob(aa_fixture *f, mc_entity_class_kind kind, mc_entity **out) {
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(f->base.app.entities, kind, &literal) == MC_ENTITY_OK &&
          mc_mob_allocate(f->base.app.entities, f->base.app.registry, literal, out, &error) ==
              MC_MOB_OK &&
          mc_mob_construct(*out, f->base.world, &error) == MC_MOB_OK);
    return true;
}

static bool aa_strings(aa_fixture *f) {
    static const uint16_t ally[] = {'A', 'l', 'l', 'y', 'P', 'l', 'a', 'y', 'e', 'r'},
                          other[] = {'F', 'o', 'r', 'e', 'i', 'g', 'n',
                                     'P', 'l', 'a', 'y', 'e', 'r'},
                          token[] = {'-'};
    CHECK(mc_player_string_create(ally, sizeof(ally) / sizeof(*ally), &f->ally) == MC_PLAYER_OK &&
          mc_player_string_create(other, sizeof(other) / sizeof(*other), &f->foreign) ==
              MC_PLAYER_OK &&
          mc_player_string_create(token, 1, &f->token) == MC_PLAYER_OK);
    return true;
}

static bool aa_remove_record(aa_fixture *f, mc_entity *entity, int32_t key) {
    f->fault_entity = entity;
    f->fault_map = entity->watcher->map;
    mc_entity_watch_node *previous = NULL;
    for (mc_entity_watch_node *node = f->fault_map->head; node; previous = node, node = node->next)
        if (node->key == key) {
            f->fault_node = node;
            f->fault_previous = previous;
            if (previous)
                previous->next = node->next;
            else
                f->fault_map->head = node->next;
            --f->fault_map->size;
            return true;
        }
    return false;
}

static bool aa_payload(aa_fixture *f, mc_entity *entity, int32_t key, bool is_null) {
    f->fault_record = aa_record(entity, key);
    CHECK(f->fault_record != NULL);
    f->record_before = *f->fault_record;
    f->fault_record->payload_kind =
        is_null ? MC_ENTITY_WATCH_PAYLOAD_NULL : MC_ENTITY_WATCH_PAYLOAD_INTEGER;
    f->fault_record->integer_value = 7;
    return true;
}

static bool aa_prepare(aa_fixture *f, const char *resources, const char *directory) {
    CHECK(setup(&f->base, resources, directory, false) && aa_strings(f));
    mc_entity_release(f->base.player);
    f->base.player = NULL;
    CHECK(mc_client_application_create_session(&f->base.app, f->ally, f->token, NULL) ==
          MC_CLIENT_STATISTICS_OWNER_OK);
    mc_client_stats_options options = mc_client_stats_default_options();
    options.charset = MC_JAVA_READER_UTF8;
    options.diagnostics.emit = quiet;
    CHECK(mc_client_application_construct_statistics(&f->base.app, &options, NULL, NULL) ==
              MC_CLIENT_STATISTICS_OWNER_OK &&
          mc_client_stats_writer_join(f->base.app.stats_writer, NULL) == MC_CLIENT_STATS_OK &&
          mc_math_classes_create(SIZE_MAX, &f->math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(f->math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_chunk_provider_bind_fields(f->base.world->fields.v, &f->provider, NULL) ==
          MC_WORLD_STORAGE_OK);
    for (int32_t x = -2; x <= 2; ++x)
        for (int32_t z = -2; z <= 2; ++z) {
            mc_chunk_state **slot = &f->chunks[(size_t)(x + 2) * 5 + (size_t)(z + 2)];
            CHECK(mc_world_store_load_game_chunk(f->base.store, f->base.world, 0, x, z, slot,
                                                 NULL) == MC_WORLD_STORAGE_OK &&
                  *slot && !(*slot)->blank);
            mc_chunk_data *data = (*slot)->data;
            memset(data->blocks, 0, MC_CHUNK_BLOCKS);
            memset(data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
            memset(data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
            memset(data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
            memset(data->heightmap, 64, MC_CHUNK_HEIGHTMAP_BYTES);
            for (size_t i = 0; i < 256; ++i)
                data->blocks[i * 128 + 63] = 1;
            data->minimum_height = 64;
            (*slot)->terrain_populated = true;
            CHECK(mc_chunk_cache_publish(f->provider.cache, x, z, *slot));
        }
    f->base.app.entities->next_id = 0;
    CHECK(mc_world_info_spawn_set(f->base.world->fields.x, 8, 64, 8) == MC_WORLD_INFO_OK &&
          mc_local_player_allocate(f->base.app.entities, &f->base.player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(f->base.player, f->base.world, &f->base.app,
                                    f->base.app.session, 0, NULL, NULL) == MC_PLAYER_OK &&
          aa_publish(f, f->base.player, "actor", 8.5, 8.5, 30));
    f->base.app.active_world = f->base.world;
    f->base.app.h = f->base.app.i = f->base.player;
    const int32_t kind = f->input[2];
    if (kind == 6) {
        static const uint16_t user[] = {'O', 't', 'h', 'e', 'r', 'P', 'l', 'a', 'y', 'e', 'r'};
        mc_player_string *name = NULL;
        CHECK(mc_player_string_create(user, sizeof(user) / sizeof(*user), &name) == MC_PLAYER_OK &&
              mc_player_session_create(f->base.app.session_classes, name, f->token,
                                       &f->other_session, NULL) == MC_PLAYER_OK);
        mc_player_string_release(name);
        CHECK(mc_local_player_allocate(f->base.app.entities, &f->attacker, NULL) == MC_ENTITY_OK &&
              mc_local_player_construct(f->attacker, f->base.world, &f->base.app, f->other_session,
                                        0, NULL, NULL) == MC_PLAYER_OK);
    } else if (kind != 7) {
        const mc_entity_class_kind kinds[] = {
            MC_ENTITY_CLASS_KIND_ZOMBIE, MC_ENTITY_CLASS_KIND_WOLF,    MC_ENTITY_CLASS_KIND_WOLF,
            MC_ENTITY_CLASS_KIND_WOLF,   MC_ENTITY_CLASS_KIND_CREEPER, MC_ENTITY_CLASS_KIND_GHAST};
        CHECK(kind >= 0 && kind < 6 && aa_mob(f, kinds[kind], &f->attacker));
    }
    if (f->attacker)
        CHECK(aa_publish(f, f->attacker, "attacker", 10.5, 8.5, 0));
    if (kind == 2 || kind == 3) {
        aa_record(f->attacker, 16)->byte_value = 4;
        aa_record(f->attacker, 17)->string_value = kind == 2 ? f->ally : f->foreign;
    }
    CHECK(aa_mob(f, MC_ENTITY_CLASS_KIND_COW, &f->sentinel) &&
          aa_publish(f, f->sentinel, "sentinel", -10.5, -10.5, 0));
    static const char *const names[] = {"wolf0", "wolf1", "wolf2", "wolf3", "wolf4", "wolf5"};
    for (size_t i = 0; i < 6; ++i) {
        CHECK(aa_mob(f, MC_ENTITY_CLASS_KIND_WOLF, &f->wolves[i]));
        aa_record(f->wolves[i], 16)->byte_value = (int8_t)(i == 3 ? 0 : (i == 1 || i == 4 ? 5 : 4));
        aa_record(f->wolves[i], 17)->string_value = i == 2 ? f->foreign : f->ally;
        if (i == 4)
            CHECK(mc_mob_creature_target_store(f->wolves[i], f->sentinel, NULL) == MC_ENTITY_OK);
        CHECK(aa_publish(f, f->wolves[i], names[i], i == 5 ? 40.5 : 9.5 + (double)i, 9.5, 0));
    }
    mc_local_player_snapshot *p = &f->base.player->local_player->fields;
    p->living.E = 20;
    p->living.Y = f->input[5];
    p->living.Z = 17;
    f->base.player->base.by = 0;
    p->living.aa = 3;
    p->living.ab = 4;
    p->living.ad = 0;
    p->living.ac = 99;
    p->living.al = .25f;
    p->living.au = 0;
    p->living.av = 17;
    p->living.ah = false;
    p->player.private_bO = 0;
    p->player.u = false;
    f->base.player->base.velocity_x = .25;
    f->base.player->base.velocity_y = -.375;
    f->base.player->base.velocity_z = -.125;
    f->base.player->base.bb = false;
    f->base.player->base.be = false;
    for (size_t i = 0; i < f->count; ++i) {
        f->entities[i]->watcher->dirty = false;
        for (mc_entity_watch_node *n = f->entities[i]->watcher->map->head; n; n = n->next)
            n->record->dirty = false;
    }
    CHECK(mc_entity_bounds_pool_reset(f->base.app.entities, NULL) == MC_ENTITY_OK);
    f->inventory = p->player.c;
    f->name = p->player.l;
    const int32_t mode = f->input[3];
    if (mode == 1)
        p->player.l = NULL;
    if (mode == 2 || mode == 9 || mode == 10) {
        f->fault_entity = mode == 2 ? f->attacker : f->wolves[mode == 9 ? 0 : 1];
        f->fault_watcher = f->fault_entity->watcher;
        f->fault_entity->watcher = NULL;
    }
    if (mode == 3 || mode == 11) {
        f->fault_entity = mode == 3 ? f->attacker : f->wolves[0];
        f->fault_map = f->fault_entity->watcher->map;
        f->fault_entity->watcher->map = NULL;
    }
    if (mode == 4 || mode == 12 || mode == 17)
        CHECK(aa_remove_record(f, mode == 4 ? f->attacker : f->wolves[0], mode == 17 ? 17 : 16));
    if (mode >= 5 && mode <= 8)
        CHECK(aa_payload(f, f->attacker, mode <= 6 ? 16 : 17, mode == 5 || mode == 7));
    if (mode >= 13 && mode <= 16)
        CHECK(aa_payload(f, f->wolves[0], mode <= 14 ? 16 : 17, mode == 13 || mode == 15));
    if (mode == 18)
        f->base.player->base.world = NULL;
    if (mode == 19)
        p->minecraft = NULL;
    if (mode == 20)
        p->player.c = NULL;
    f->base.world->fields.B = f->input[7] != 0;
    f->base.world->fields.q = f->input[6];
    const mc_world_access_callbacks callbacks = {.sound = aa_sound};
    CHECK(mc_world_access_create(&callbacks, f, &f->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    return true;
}

static bool aa_retire(aa_fixture *f) {
    CHECK(mc_player_ally_alert_retire(&f->call, NULL) == MC_ENTITY_OK &&
          mc_entity_effect_cleanup(f->base.player, NULL) == MC_ENTITY_OK &&
          mc_entity_effect_error_retire(&f->error));
    if (f->fault_watcher)
        f->fault_entity->watcher = f->fault_watcher;
    else if (f->fault_map && !f->fault_node)
        f->fault_entity->watcher->map = f->fault_map;
    if (f->fault_node) {
        if (f->fault_previous)
            f->fault_previous->next = f->fault_node;
        else
            f->fault_map->head = f->fault_node;
        ++f->fault_map->size;
    }
    if (f->fault_record)
        *f->fault_record = f->record_before;
    f->base.player->base.world = f->base.world;
    f->base.player->local_player->fields.player.l = f->name;
    f->base.player->local_player->fields.player.c = f->inventory;
    f->base.player->local_player->fields.minecraft = &f->base.app;
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = f->count; i > 1; --i)
        mc_entity_release(f->entities[i - 1]);
    mc_chunk_state *retiring[64];
    const size_t loaded = f->provider.cache->insertion_count;
    CHECK(loaded <= sizeof(retiring) / sizeof(*retiring));
    if (loaded != 0)
        memcpy(retiring, f->provider.cache->insertions, loaded * sizeof(*retiring));
    CHECK(mc_world_runtime_retire_entity_containers(f->base.world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_pending_clear(f->provider.pending) == MC_CHUNK_ACQUIRE_OK);
    CHECK(mc_chunk_cache_init(f->provider.cache, f->provider.cache->entries,
                              f->provider.cache->capacity, f->provider.cache->insertions,
                              f->provider.cache->insertion_capacity));
    CHECK(mc_world_runtime_retire_world(f->base.world, NULL) == MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < loaded; ++i) {
        bool duplicate = false;
        for (size_t earlier = 0; earlier < i; ++earlier)
            duplicate = duplicate || retiring[earlier] == retiring[i];
        if (!duplicate)
            CHECK(mc_world_store_retire_chunk(f->base.store, retiring[i], NULL) ==
                  MC_WORLD_STORAGE_OK);
    }
    mc_player_session_retire(&f->other_session);
    mc_player_string_release(f->ally);
    mc_player_string_release(f->foreign);
    mc_player_string_release(f->token);
    CHECK(mc_math_classes_release(&f->math, NULL) == MC_MATH_CLASS_OK && teardown(&f->base));
    return true;
}

static bool aa_profile(aa_fixture *f, const char *resources, const char *directory,
                       const char *output) {
    CHECK(aa_prepare(f, resources, directory) && aa_state(f, output, "before"));
    bool result = false;
    mc_entity_status status;
    const char *exception = "NONE";
    unsigned int pc = 0;
    bool returned = false;
    size_t examined = 0, alerted = 0;
    if (f->input[1] == 0) {
        status = mc_entity_effect_damage_with_math(f->base.player, f->base.app.registry, f->math,
                                                   f->attacker, f->input[4], &result, &f->error);
        returned = status == MC_ENTITY_OK;
        pc = f->error.player_bytecode;
        examined = f->error.player_alert.wolves_examined;
        alerted = f->error.player_alert.wolves_alerted;
        if (!returned) {
            if (f->error.player_alert.source_exception)
                exception = f->error.player_alert.exception_class;
            else if (f->error.cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE ||
                     f->error.statistic_error.status == MC_PLAYER_STATISTIC_NULL_REFERENCE)
                exception = "java/lang/NullPointerException";
            else if (f->error.living_damage.source_exception)
                exception = f->error.living_damage.exception_class;
            else if (f->error.armor.source_exception)
                exception = f->error.armor.exception_class;
            else
                exception = "NATIVE_POLICY";
        }
    } else {
        status =
            mc_player_ally_alert_begin(f->base.player, f->attacker, f->input[1] == 2, &f->call);
        mc_player_ally_alert_state state;
        CHECK(mc_player_ally_alert_inspect(f->call, &state));
        returned = state.returned;
        pc = state.source_pc;
        examined = state.wolves_examined;
        alerted = state.wolves_alerted;
        if (!returned)
            exception =
                state.error->source_exception ? state.error->exception_class : "NATIVE_POLICY";
    }
    CHECK(exception != NULL && aa_state(f, output, "after"));
    char path[8192];
    CHECK(snprintf(path, sizeof(path), "%s/case%" PRId32 ".json", output, f->input[0]) > 0);
    FILE *out = fopen(path, "wb");
    CHECK(out != NULL);
    CHECK(fprintf(out, "{\"case\":%" PRId32 ",\"input\":[", f->input[0]) >= 0);
    for (size_t i = 0; i < 8; ++i)
        CHECK(fprintf(out, "%s%" PRId32, i ? "," : "", f->input[i]) >= 0);
    CHECK(fprintf(out,
                  "],\"returned\":%s,\"result\":%s,\"exception\":\"%s\",\"native_status\":%d,"
                  "\"source_pc\":%u,\"examined\":%zu,\"alerted\":%zu}\n",
                  aa_bool(returned), returned && f->input[1] == 0 ? aa_bool(result) : "null",
                  exception, (int)status, pc, examined, alerted) >= 0 &&
          fclose(out) == 0);
    CHECK(aa_retire(f));
    CHECK(printf("PLAYER_ALLY_ALERT_NATIVE case=%" PRId32 "\n", f->input[0]) >= 0 &&
          fflush(stdout) == 0);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 5 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    FILE *profiles = fopen(argv[3], "rb");
    if (profiles == NULL)
        return 2;
    for (int32_t index = 0; index < 40; ++index) {
        aa_fixture *f = calloc(1, sizeof(*f));
        if (f == NULL)
            return 1;
        for (size_t i = 0; i < 8; ++i)
            if (fscanf(profiles, "%" SCNd32, &f->input[i]) != 1)
                return 2;
        char directory[32];
        if (f->input[0] != index ||
            snprintf(directory, sizeof(directory), "case%" PRId32, index) <= 0 ||
            !aa_profile(f, argv[1], directory, argv[4]))
            return 1;
        free(f);
    }
    if (fclose(profiles) != 0)
        return 1;
    if (printf("Player ally alert: %zu checks;40 genuine profiles\n", checks) < 0 ||
        fflush(stdout) != 0)
        return 1;
    return 0;
}
