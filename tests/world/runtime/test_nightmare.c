/* Genuine World/Registry/Player/Mob/Path owners. The imported fixture main is
 * never run; its source World constructor receives this declared seed. */
#include "mc/world/runtime/construction.h"

static mc_world_storage_status
nightmare_world_construct(mc_world_runtime *world,
                          const mc_world_runtime_world_arguments *arguments,
                          mc_world_storage_error *error) {
    mc_world_runtime_world_arguments input = *arguments;
    input.seed = 5143;
    return mc_world_runtime_construct_world(world, &input, error);
}

#define mc_world_runtime_construct_world nightmare_world_construct
#define main mc_nightmare_unused_random_main
#include "test_world_random_tick.c"
#undef main
#undef mc_world_runtime_construct_world

#include "../../../src/entity/classes/memory_internal.h"
#include "../../../src/entity/living/fields_internal.h"
#include "../../../src/entity/player/nbt_internal.h"
#include "../../../src/entity/player/player_internal.h"
#include "../../../src/world/path/finder_internal.h"
#include "../../../src/world/runtime/entity_tick_internal.h"
#include "../../../src/world/runtime/nightmare_internal.h"
#include "../../../src/world/runtime/world_access_internal.h"
#include "../../support/java_random.h"
#include "mc/entity/player/coordinates.h"
#include "mc/entity/player/sleep.h"
#include "mc/world/runtime/nightmare.h"

enum { NIGHTMARE_EVENTS = 4096 };

typedef struct nightmare_event {
    unsigned int kind;
    const char *class_name;
    int32_t id, integers[6];
    size_t entities, players;
    double position[3];
    float volume, pitch;
    char sound[64];
} nightmare_event;

typedef struct nightmare_fixture {
    random_fixture base;
    int32_t input[8];
    mc_entity *actors[2];
    const mc_entity_bounds *bounds[2];
    const mc_player_coordinates *coordinates[2][3];
    mc_java_random player_randoms[2];
    mc_world_nightmare_call *call;
    mc_world_nightmare_state state;
    mc_world_reference_list empty, replacement;
    void *replacement_items[2];
    mc_world_reference_list *passed, *original_players;
    mc_world_access *observer;
    mc_world_access_receipt *receipt;
    nightmare_event events[NIGHTMARE_EVENTS];
    size_t event_count, additions;
    bool bad, busy_seen;
} nightmare_fixture;

static const char *nj_bool(bool value) {
    return value ? "true" : "false";
}

static bool nj_xyz(FILE *out, const mc_player_coordinates *value) {
    return value == NULL ? fputs("null", out) >= 0
                         : fprintf(out, "[%" PRId32 ",%" PRId32 ",%" PRId32 "]", value->x, value->y,
                                   value->z) >= 0;
}

static bool nj_random(FILE *out, const mc_java_random *value) {
    return value == NULL ? fputs("null", out) >= 0
                         : fprintf(out, "[\"%016" PRIx64 "\",%s,\"%016" PRIx64 "\"]", value->state,
                                   nj_bool(value->have_next_next_gaussian),
                                   double_bits(value->next_next_gaussian)) >= 0;
}

static bool nj_runs(FILE *out, const uint8_t *bytes, size_t length) {
    if (fprintf(out, "{\"length\":%zu,\"runs\":[", length) < 0)
        return false;
    bool first = true;
    for (size_t index = 0; index < length;) {
        const uint8_t value = bytes[index];
        size_t end = index + 1;
        while (end < length && bytes[end] == value)
            ++end;
        if (value != 0) {
            if (fprintf(out, "%s[%zu,%zu,%u]", first ? "" : ",", index, end - index,
                        (unsigned int)value) < 0)
                return false;
            first = false;
        }
        index = end;
    }
    return fputs("]}", out) >= 0;
}

static bool nj_nullable_id(FILE *out, const mc_entity *value) {
    return value == NULL ? fputs("null", out) >= 0 : fprintf(out, "%" PRId32, value->base.id) >= 0;
}

static bool nj_entity(FILE *out, const nightmare_fixture *f, const mc_entity *value) {
    const mc_entity_base_fields *b = &value->base;
    const mc_living_entity_fields *living = mc_entity_living_fields((mc_entity *)value);
    const char *name = value->class_literal->client_name;
    if (fprintf(out,
                "{\"class\":\"%s\",\"id\":%" PRId32 ",\"position\":[\"%016" PRIx64
                "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],\"motion\":[\"%016" PRIx64
                "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],\"history\":[\"%016" PRIx64
                "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                "\",\"%016" PRIx64 "\"],\"angles\":[\"%08" PRIx32 "\",\"%08" PRIx32
                "\",\"%08" PRIx32 "\",\"%08" PRIx32 "\"],\"dimensions\":[\"%08" PRIx32
                "\",\"%08" PRIx32 "\",\"%08" PRIx32 "\"],\"health\":%" PRId32 ",\"random\":",
                name, b->id, double_bits(b->x), double_bits(b->y), double_bits(b->z),
                double_bits(b->velocity_x), double_bits(b->velocity_y), double_bits(b->velocity_z),
                double_bits(b->aJ), double_bits(b->aK), double_bits(b->aL), double_bits(b->bl),
                double_bits(b->bm), double_bits(b->bn), float_bits(b->yaw), float_bits(b->aT),
                float_bits(b->aU), float_bits(b->aV), float_bits(b->width), float_bits(b->height),
                float_bits(b->offset), living == NULL ? 0 : living->Y) < 0 ||
        !nj_random(out, value->random) || fputs(",\"mount_id\":", out) < 0 ||
        !nj_nullable_id(out, b->aH) || fputs(",\"rider_id\":", out) < 0 ||
        !nj_nullable_id(out, b->aG) || fputs(",\"bounds\":", out) < 0)
        return false;
    if (value->bounds == NULL) {
        if (fputs("null", out) < 0)
            return false;
    } else {
        const mc_entity_bounds *box = value->bounds;
        if (fprintf(out,
                    "[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                    "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]",
                    double_bits(box->min_x), double_bits(box->min_y), double_bits(box->min_z),
                    double_bits(box->max_x), double_bits(box->max_y), double_bits(box->max_z)) < 0)
            return false;
    }
    if (mc_entity_is_kind(value, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        const mc_player_fields *p = &value->local_player->fields.player;
        size_t index = 0;
        while (index < 2 && f->actors[index] != value)
            ++index;
        if (index == 2 ||
            fprintf(out, ",\"sleeping\":%s,\"timer\":%" PRId32 ",\"bed\":", nj_bool(p->u),
                    p->private_a) < 0 ||
            !nj_xyz(out, p->v) || fputs(",\"spawn\":", out) < 0 || !nj_xyz(out, p->private_b) ||
            fputs(",\"private_bN\":", out) < 0 || !nj_xyz(out, p->private_bN) ||
            fprintf(out,
                    ",\"bounds_same\":%s,\"bed_same\":%s,\"spawn_same\":%s,"
                    "\"private_bN_same\":%s,\"spawn_bed_same\":%s",
                    nj_bool(value->bounds == f->bounds[index]),
                    nj_bool(p->v == f->coordinates[index][0]),
                    nj_bool(p->private_b == f->coordinates[index][1]),
                    nj_bool(p->private_bN == f->coordinates[index][2]),
                    nj_bool(p->private_b == p->v)) < 0)
            return false;
    }
    return fputc('}', out) != EOF;
}

static bool nj_list(FILE *out, const mc_world_reference_list *list) {
    if (list == NULL)
        return fputs("null", out) >= 0;
    if (fputc('[', out) == EOF)
        return false;
    for (size_t i = 0; i < list->count; ++i)
        if ((i != 0 && fputc(',', out) == EOF) ||
            !nj_nullable_id(out, (const mc_entity *)list->items[i]))
            return false;
    return fputc(']', out) != EOF;
}

static bool nj_state(FILE *out, nightmare_fixture *f) {
    mc_world_runtime *w = f->base.world;
    if (fputs("{\"players\":[", out) < 0)
        return false;
    for (int32_t i = 0; i < f->input[4]; ++i)
        if ((i != 0 && fputc(',', out) == EOF) || !nj_entity(out, f, f->actors[i]))
            return false;
    if (fputs("],\"entities\":[", out) < 0)
        return false;
    for (size_t i = 0; i < w->fields.b->count; ++i)
        if ((i != 0 && fputc(',', out) == EOF) ||
            !nj_entity(out, f, (mc_entity *)w->fields.b->items[i]))
            return false;
    if (fputs("],\"world_random\":", out) < 0 || !nj_random(out, w->fields.r))
        return false;
    const mc_java_math_random_owner *math = f->base.profile->math;
    int64_t time = 0;
    if (math == NULL || mc_world_info_time_get(w->fields.x, &time) != MC_WORLD_INFO_OK ||
        fprintf(out,
                ",\"math_random\":[\"%016" PRIx64 "\",%s,\"%016" PRIx64 "\"],"
                "\"time\":\"%016" PRIx64 "\",\"sky_darkness\":%" PRId32
                ",\"world_sleeping\":%s,\"entity_counter\":%" PRId32 ",\"world_player_ids\":",
                atomic_load(&math->state), nj_bool(math->have_next_next_gaussian),
                double_bits(math->next_next_gaussian), (uint64_t)time, w->fields.f,
                nj_bool(w->fields.J), f->base.entities->next_id) < 0 ||
        !nj_list(out, w->fields.d) || fputs(",\"world_entity_ids\":", out) < 0 ||
        !nj_list(out, w->fields.b) || fputs(",\"passed_ids\":", out) < 0 ||
        !nj_list(out, f->passed) ||
        fprintf(out, ",\"passed_is_world_players\":%s,\"class_array\":[",
                nj_bool(f->passed == w->fields.d)) < 0)
        return false;
    for (size_t i = 0; i < 3; ++i) {
        const mc_world_nightmare_class_value cell = f->base.classes->nightmare.defaults[i];
        if (i != 0 && fputc(',', out) == EOF)
            return false;
        if (cell.kind == MC_WORLD_NIGHTMARE_CLASS_NULL) {
            if (fputs("null", out) < 0)
                return false;
        } else if (fprintf(out, "\"%s\"",
                           cell.kind == MC_WORLD_NIGHTMARE_CLASS_WORLD
                               ? "fd"
                               : cell.value.entity->client_name) < 0)
            return false;
    }
    if (fputs("],\"chunks\":[", out) < 0)
        return false;
    bool first = true;
    for (int32_t x = -4; x <= 4; ++x)
        for (int32_t z = -4; z <= 4; ++z) {
            mc_chunk_state *chunk = NULL;
            if (!mc_chunk_cache_lookup(f->base.provider.cache, x, z, &chunk) || chunk == NULL)
                return false;
            if (fprintf(out, "%s{\"x\":%" PRId32 ",\"z\":%" PRId32 ",\"planes\":{\"blocks\":",
                        first ? "" : ",", x, z) < 0 ||
                !nj_runs(out, chunk->data->blocks, MC_CHUNK_BLOCKS) ||
                fputs(",\"metadata\":", out) < 0 ||
                !nj_runs(out, chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES) ||
                fputs(",\"sky\":", out) < 0 ||
                !nj_runs(out, chunk->data->skylight, MC_CHUNK_NIBBLE_BYTES) ||
                fputs(",\"block_light\":", out) < 0 ||
                !nj_runs(out, chunk->data->block_light, MC_CHUNK_NIBBLE_BYTES) ||
                fputs(",\"height\":", out) < 0 ||
                !nj_runs(out, chunk->data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES) ||
                fputs("}}", out) < 0)
                return false;
            first = false;
        }
    return fputs("]}", out) >= 0;
}

static bool nightmare_put(nightmare_fixture *f, int32_t x, int32_t y, int32_t z, int32_t id,
                          int32_t metadata) {
    mc_chunk_state *chunk = NULL;
    CHECK(y >= 0 && y < 128 &&
          mc_chunk_cache_lookup(f->base.provider.cache, mc_java_i32_shr(x, 4),
                                mc_java_i32_shr(z, 4), &chunk) &&
          chunk != NULL);
    const int32_t lx = (int32_t)((uint32_t)x & 15u), lz = (int32_t)((uint32_t)z & 15u);
    const size_t index = (size_t)((lx << 11) | (lz << 7) | y);
    chunk->data->blocks[index] = (uint8_t)id;
    CHECK(mc_chunk_nibbles_set(chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES, lx, y, lz,
                               (uint8_t)metadata));
    return true;
}

static bool nightmare_player(nightmare_fixture *f, size_t index) {
    const int32_t x = index == 0 ? 8 : -8, z = x, direction = f->input[3];
    static const int32_t facing[4][2] = {{0, 1}, {-1, 0}, {0, -1}, {1, 0}};
    CHECK(nightmare_put(f, x, 64, z, 26, direction | 8) &&
          nightmare_put(f, x - facing[direction][0], 64, z - facing[direction][1], 26, direction));
    mc_player_session *session = NULL;
    mc_player_string *username = NULL, *token = NULL;
    static const uint16_t name[] = {'N', 'i', 'g', 'h', 't', 'm', 'a', 'r', 'e',
                                    'C', 'o', 'm', 'p', 'o', 'n', 'e', 'n', 't'};
    const uint16_t sid[] = {'-'};
    CHECK(mc_player_string_create(name, COUNT(name), &username) == MC_PLAYER_OK &&
          mc_player_string_create(sid, COUNT(sid), &token) == MC_PLAYER_OK &&
          mc_player_session_create(f->base.sessions, username, token, &session, NULL) ==
              MC_PLAYER_OK);
    mc_entity *value = NULL;
    const bool constructed =
        mc_local_player_allocate(f->base.entities, &value, NULL) == MC_ENTITY_OK &&
        mc_local_player_construct(value, f->base.world, NULL, session, 0, NULL, NULL) ==
            MC_PLAYER_OK;
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    mc_player_string_release(token);
    CHECK(constructed);
    f->actors[index] = value;
    if (index == 0)
        f->base.player = value;
    CHECK(mc_entity_set_position(value, (double)x + .5, 64.0 + (double)1.62f, (double)z + .5) ==
          MC_ENTITY_OK);
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->base.world, value, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    const mc_player_coordinates spawn = {42, 66, 44};
    CHECK(mc_player_saved_spawn_set(value, &spawn, NULL) == MC_PLAYER_OK);
    struct mc_player_nbt_coordinate *coordinate = NULL;
    CHECK(mc_player_coordinate_new_held(value, (mc_player_site){0}, "declared CQ private BR input",
                                        &coordinate, NULL) == MC_PLAYER_OK);
    coordinate->value = (mc_player_coordinates){-11, 65, 13};
    mc_player_nbt_coordinate_store_held(value, coordinate,
                                        &value->local_player->fields.player.private_bN);
    const mc_player_sleep_context context = {.code_registry = f->base.registry};
    mc_player_sleep_call *call = NULL;
    mc_player_sleep_error error;
    const mc_player_sleep_status status =
        mc_player_try_sleep_begin(value, x, 64, z, &context, &call, &error);
    mc_player_sleep_state state;
    CHECK(status == MC_PLAYER_SLEEP_OK && mc_player_sleep_inspect(call, &state) && state.returned &&
          state.result != NULL && value->local_player->fields.player.u &&
          mc_player_sleep_retire(&call, NULL) == MC_PLAYER_SLEEP_OK);
    value->local_player->fields.player.private_a = 100;
    CHECK(nightmare_put(f, x, 64, z, 26, direction | 12));
    f->bounds[index] = value->bounds;
    f->coordinates[index][0] = value->local_player->fields.player.v;
    f->coordinates[index][1] = value->local_player->fields.player.private_b;
    f->coordinates[index][2] = value->local_player->fields.player.private_bN;
    f->player_randoms[index] = *value->random;
    return true;
}

static bool nightmare_barrier(nightmare_fixture *f, int32_t x, int32_t z) {
    for (int32_t xx = x - 3; xx <= x + 3; ++xx)
        for (int32_t zz = z - 3; zz <= z + 3; ++zz)
            for (int32_t y = 64; y <= 67; ++y)
                if (!((xx == x && zz == z) || (xx == x && zz == z - 1)))
                    CHECK(nightmare_put(f, xx, y, zz, 1, 0));
    return true;
}

static bool nightmare_prepare(nightmare_fixture *f, const char *resources, const char *world) {
    CHECK(setup(&f->base, resources, world));
    mc_world_runtime *w = f->base.world;
    for (int32_t x = -4; x <= 4; ++x)
        for (int32_t z = -4; z <= 4; ++z) {
            mc_chunk_state *chunk = NULL;
            CHECK(load_chunk(&f->base, x, z, &chunk));
            memset(chunk->data->blocks, 0, MC_CHUNK_BLOCKS);
            memset(chunk->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
            memset(chunk->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
            memset(chunk->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
            memset(chunk->data->heightmap, 64, MC_CHUNK_HEIGHTMAP_BYTES);
            for (size_t column = 0; column < 256; ++column)
                chunk->data->blocks[column * 128 + 63] = 1;
            chunk->data->minimum_height = 64;
            chunk->terrain_populated = true;
            CHECK(mc_chunk_cache_publish(f->base.provider.cache, x, z, chunk));
        }
    w->fields.B = false;
    w->fields.q = 2;
    w->fields.f = 13;
    CHECK(mc_world_info_spawn_set(w->fields.x, 8, 64, 8) == MC_WORLD_INFO_OK);
    f->base.entities->next_id = 0;
    for (int32_t i = 0; i < f->input[4]; ++i)
        CHECK(nightmare_player(f, (size_t)i));
    const int32_t scene = f->input[1];
    for (size_t i = 0; i < f->base.owned_count; ++i) {
        mc_chunk_data *data = f->base.owned[i]->data;
        if (scene == 0 || scene == 1) {
            memset(data->blocks, scene == 0 ? 0 : 1, MC_CHUNK_BLOCKS);
            memset(data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
        }
        if (scene == 3) {
            memset(data->skylight, 255, MC_CHUNK_NIBBLE_BYTES);
            memset(data->block_light, 255, MC_CHUNK_NIBBLE_BYTES);
        }
    }
    if (scene == 4 || scene == 6)
        CHECK(nightmare_barrier(f, 8, 8));
    if (scene == 7)
        CHECK(nightmare_barrier(f, -8, -8));
    if (scene == 5)
        for (int32_t x = 5; x <= 11; ++x)
            for (int32_t z = 5; z <= 11; ++z)
                if (!((x == 8 && z == 8) || (x == 8 && z == 7)))
                    CHECK(nightmare_put(f, x, 64, z, 50, 5));
    if (scene == 8)
        CHECK(nightmare_put(f, 6, 64, 8, 1, 0) && nightmare_put(f, 7, 64, 9, 1, 0) &&
              nightmare_put(f, 7, 65, 8, 1, 0));
    CHECK(mc_world_nightmare_classes_ensure(f->base.classes, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_nightmare_namespace *n = &f->base.classes->nightmare;
    for (size_t i = 0; i < 3; ++i) {
        if (f->input[5] == 1)
            n->defaults[i] = (mc_world_nightmare_class_value){0};
        else if (f->input[5] == 2)
            n->defaults[i] = (mc_world_nightmare_class_value){
                .kind = MC_WORLD_NIGHTMARE_CLASS_WORLD, .value.world = f->base.classes};
        else if (f->input[5] == 5 || f->input[5] == 6) {
            const mc_entity_class_literal *literal = NULL;
            CHECK(mc_entity_class_literal_get(f->base.entities,
                                              f->input[5] == 5 ? MC_ENTITY_CLASS_KIND_ZOMBIE
                                                               : MC_ENTITY_CLASS_KIND_SPIDER,
                                              &literal) == MC_ENTITY_OK);
            n->defaults[i] = (mc_world_nightmare_class_value){
                .kind = MC_WORLD_NIGHTMARE_CLASS_ENTITY, .value.entity = literal};
        }
    }
    f->original_players = w->fields.d;
    f->passed = f->input[6] == 1 ? &f->empty : f->input[6] == 2 ? NULL : w->fields.d;
    mc_java_random_reseed(w->fields.r, f->input[2]);
    double discarded;
    CHECK(mc_java_math_random(f->base.profile, &discarded) == MC_JAVA_RANDOM_PROFILE_OK &&
          f->base.profile->math != NULL);
    mc_java_random seeded;
    mc_java_random_seed(&seeded, 0);
    atomic_store(&f->base.profile->math->state, seeded.state);
    f->base.profile->math->have_next_next_gaussian = false;
    f->base.profile->math->next_next_gaussian = 0.0;
    CHECK(mc_entity_base_class_initialize(f->base.entities, NULL) == MC_ENTITY_OK);
    f->base.entities->next_id = 1000;
    CHECK(mc_entity_bounds_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK &&
          mc_entity_vector_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK);
    return true;
}

static nightmare_event *nightmare_event_begin(nightmare_fixture *f, mc_world_runtime *world,
                                              unsigned int kind, mc_world_storage_error *error) {
    if (world != f->base.world || f->event_count == NIGHTMARE_EVENTS) {
        f->bad = true;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, "nightmare observer buffer", 0};
        return NULL;
    }
    nightmare_event *event = &f->events[f->event_count++];
    event->kind = kind;
    mc_world_nightmare_cleanup cleanup;
    if (f->call == NULL || mc_world_nightmare_retire(&f->call, &cleanup) != MC_WORLD_STORAGE_BUSY ||
        f->call == NULL) {
        f->bad = true;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_DATA,
                                          "nightmare active call retirement control", 0};
        return NULL;
    }
    f->busy_seen = true;
    *error = (mc_world_storage_error){0};
    return event;
}

static mc_world_storage_status nightmare_added(void *context, mc_world_runtime *world,
                                               mc_entity *entity, mc_world_storage_error *error,
                                               mc_world_access_failure *failure) {
    nightmare_fixture *f = context;
    nightmare_event *event = nightmare_event_begin(f, world, 0, error);
    if (event == NULL)
        return error->status;
    ++f->additions;
    event->class_name = entity->class_literal->client_name;
    event->id = entity->base.id;
    event->entities = world->fields.b->count;
    event->players = world->fields.d->count;
    if (f->additions != 1)
        return MC_WORLD_STORAGE_OK;
    if (f->input[7] == 1 && f->input[4] > 1) {
        wt_scope scope;
        memset(&scope, 0, sizeof(scope));
        scope.world = world;
        mc_world_entity_tick_error_init(&scope.error);
        if (!wt_list_remove(&scope, f->original_players, 1, 0)) {
            *error = scope.error.cause;
            return error->status;
        }
    } else if (f->input[7] == 2) {
        mc_world_entity_error appended;
        if (!mc_world_runtime_entity_append_owned(world, f->original_players,
                                                  &world->player_membership, f->actors[0], 0,
                                                  &appended)) {
            *error = appended.cause;
            return appended.status;
        }
    } else if (f->input[7] == 3) {
        if (world->fields.d->count > COUNT(f->replacement_items)) {
            f->bad = true;
            *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                              "nightmare authored replacement List capacity", 0};
            return error->status;
        }
        memcpy(f->replacement_items, world->fields.d->items,
               world->fields.d->count * sizeof(*f->replacement_items));
        f->replacement = (mc_world_reference_list){f->replacement_items, world->fields.d->count,
                                                   COUNT(f->replacement_items), 0};
        world->fields.d = &f->replacement;
    } else if (f->input[7] == 4) {
        const mc_world_storage_status status =
            mc_world_access_receipt_create(f->observer, NULL, &f->receipt, failure, error);
        if (status != MC_WORLD_STORAGE_OK)
            return status;
        f->receipt->exception_class = "java/lang/IllegalStateException";
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "authored nightmare publication callback", 0};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status nightmare_cell(void *context, mc_world_runtime *world, int32_t x,
                                              int32_t y, int32_t z, mc_world_storage_error *error) {
    nightmare_event *event = nightmare_event_begin(context, world, 2, error);
    if (event == NULL)
        return error->status;
    event->integers[0] = x;
    event->integers[1] = y;
    event->integers[2] = z;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status nightmare_range(void *context, mc_world_runtime *world, int32_t x,
                                               int32_t y, int32_t z, int32_t a, int32_t b,
                                               int32_t c, mc_world_storage_error *error) {
    nightmare_event *event = nightmare_event_begin(context, world, 3, error);
    if (event == NULL)
        return error->status;
    const int32_t values[6] = {x, y, z, a, b, c};
    memcpy(event->integers, values, sizeof(values));
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status nightmare_sound(void *context, mc_world_runtime *world,
                                               const mc_registry_string *name, double x, double y,
                                               double z, float volume, float pitch,
                                               mc_world_storage_error *error) {
    nightmare_fixture *f = context;
    nightmare_event *event = nightmare_event_begin(f, world, 1, error);
    if (event == NULL)
        return error->status;
    mc_registry_string_view text = {0};
    if (!mc_registry_string_read(name, &text) || text.length >= sizeof(event->sound)) {
        f->bad = true;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_DATA, "nightmare sound text", 0};
        return error->status;
    }
    for (size_t i = 0; i < text.length; ++i) {
        if (text.units[i] < 32 || text.units[i] > 126 || text.units[i] == '"' ||
            text.units[i] == '\\') {
            f->bad = true;
            *error = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_DATA,
                                              "nightmare sound code literal", 0};
            return error->status;
        }
        event->sound[i] = (char)text.units[i];
    }
    event->position[0] = x;
    event->position[1] = y;
    event->position[2] = z;
    event->volume = volume;
    event->pitch = pitch;
    return MC_WORLD_STORAGE_OK;
}

static bool nightmare_observer(nightmare_fixture *f) {
    const mc_world_access_source_callbacks source = {.entity_added = nightmare_added};
    CHECK(mc_world_access_source_create(&source, f, &f->observer, NULL) == MC_WORLD_STORAGE_OK);
    f->observer->callbacks.cell = nightmare_cell;
    f->observer->callbacks.range = nightmare_range;
    f->observer->callbacks.sound = nightmare_sound;
    CHECK(mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
          MC_WORLD_STORAGE_OK);
    return true;
}

static bool nj_events(FILE *out, const nightmare_fixture *f) {
    if (fputc('[', out) == EOF)
        return false;
    for (size_t i = 0; i < f->event_count; ++i) {
        const nightmare_event *event = &f->events[i];
        if (i != 0 && fputc(',', out) == EOF)
            return false;
        if (event->kind == 0) {
            if (fprintf(out,
                        "{\"event\":\"entity_added\",\"class\":\"%s\",\"id\":%" PRId32
                        ",\"world_entities\":%zu,\"world_players\":%zu}",
                        event->class_name, event->id, event->entities, event->players) < 0)
                return false;
        } else if (event->kind == 1) {
            if (fprintf(out,
                        "{\"event\":\"sound\",\"name\":\"%s\",\"position\":[\"%016" PRIx64
                        "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],\"volume\":\"%08" PRIx32
                        "\",\"pitch\":\"%08" PRIx32 "\"}",
                        event->sound, double_bits(event->position[0]),
                        double_bits(event->position[1]), double_bits(event->position[2]),
                        float_bits(event->volume), float_bits(event->pitch)) < 0)
                return false;
        } else {
            const size_t count = event->kind == 2 ? 3 : 6;
            if (fprintf(out, "{\"event\":\"%s\",\"%s\":[", event->kind == 2 ? "cell" : "range",
                        event->kind == 2 ? "position" : "bounds") < 0)
                return false;
            for (size_t j = 0; j < count; ++j)
                if (fprintf(out, "%s%" PRId32, j == 0 ? "" : ",", event->integers[j]) < 0)
                    return false;
            if (fputs("]}", out) < 0)
                return false;
        }
    }
    return fputc(']', out) != EOF;
}

static bool nightmare_case(nightmare_fixture *f, const char *directory) {
    char path[8192];
    int length =
        snprintf(path, sizeof(path), "%s/case%" PRId32 "-before.json", directory, f->input[0]);
    CHECK(length > 0 && (size_t)length < sizeof(path));
    FILE *out = open_file(path, "wb");
    CHECK(out != NULL && nj_state(out, f) && fclose(out) == 0);
    const mc_world_nightmare_context context = {f->base.classes, f->base.world, f->base.math};
    mc_world_nightmare_error admission;
    const mc_world_storage_status status = mc_world_nightmare_begin(
        f->input[6] == 3 || f->input[6] == 4 ? NULL : f->base.world,
        f->input[6] == 4 ? NULL : f->passed, &context, NULL, &f->call, &admission);
    CHECK(f->call != NULL && mc_world_nightmare_inspect(f->call, &f->state) && !f->state.active &&
          !f->bad && (f->state.returned == (status == MC_WORLD_STORAGE_OK)));
    if (status != MC_WORLD_STORAGE_OK && !f->state.failure->source_exception &&
        !f->state.failure->print_stack_trace_pending)
        fprintf(stderr, "Nightmare case%" PRId32 " pc%u child%u: %s\n", f->input[0],
                f->state.source_pc, (unsigned int)f->state.failure->child,
                f->state.failure->operation == NULL ? "-" : f->state.failure->operation);
    length = snprintf(path, sizeof(path), "%s/case%" PRId32 "-after.json", directory, f->input[0]);
    CHECK(length > 0 && (size_t)length < sizeof(path));
    out = open_file(path, "wb");
    CHECK(out != NULL && nj_state(out, f) && fclose(out) == 0);
    length = snprintf(path, sizeof(path), "%s/case%" PRId32 ".json", directory, f->input[0]);
    CHECK(length > 0 && (size_t)length < sizeof(path));
    out = open_file(path, "wb");
    CHECK(out != NULL);
    fprintf(out, "{\"case\":%" PRId32 ",\"input\":[", f->input[0]);
    for (size_t i = 0; i < COUNT(f->input); ++i)
        fprintf(out, "%s%" PRId32, i == 0 ? "" : ",", f->input[i]);
    fprintf(out,
            "],\"returned\":%s,\"result\":%s,\"exception\":\"%s\",\"native_status\":%d,"
            "\"source_pc\":%u,\"print_pending\":%s,\"counters\":{\"players_started\":%zu,"
            "\"attempts_started\":%zu,\"attempts_returned\":%zu,\"random_calls\":%zu,"
            "\"constructors\":%zu,\"predicates\":%zu,\"paths\":%zu,\"publications\":%zu,"
            "\"postspawn\":%zu,\"wakes\":%zu,\"ambient\":%zu},\"finder\":",
            nj_bool(f->state.returned), f->state.returned ? nj_bool(f->state.result) : "null",
            f->state.failure->exception_class == NULL ? "NONE" : f->state.failure->exception_class,
            (int)status, f->state.source_pc, nj_bool(f->state.failure->print_stack_trace_pending),
            f->state.players_started, f->state.attempts_started, f->state.attempts_returned,
            f->state.random_calls, f->state.constructors, f->state.predicates, f->state.paths,
            f->state.publications, f->state.postspawn_calls, f->state.wakes,
            f->state.ambient_calls);
    mc_path_finder_state finder;
    if (f->state.finder != NULL && mc_path_finder_inspect(f->state.finder, &finder))
        fprintf(out,
                "{\"searches\":%zu,\"heap_count\":%" PRId32 ",\"heap_capacity\":%zu,"
                "\"map_count\":%" PRId32 ",\"map_capacity\":%zu,\"map_threshold\":%" PRId32
                ",\"map_modifications\":%" PRId32 ",\"native_query_count\":%zu,"
                "\"native_points\":%zu,\"native_cell_queries\":%zu,\"native_work\":%zu}",
                finder.searches, finder.heap_count, finder.heap_capacity, finder.map_count,
                finder.map_capacity, finder.map_threshold, finder.map_modifications,
                finder.query_count, finder.points, finder.cell_queries, finder.work);
    else
        fputs("null", out);
    fprintf(out, ",\"native_working\":{\"bytes\":%zu,\"peak\":%zu,\"allocations\":%zu}",
            f->state.native_bytes, f->state.native_peak_bytes, f->state.native_allocations);
    fputs(",\"notifications\":", out);
    CHECK(nj_events(out, f) && fputs("}\n", out) >= 0 && fclose(out) == 0);
    for (int32_t i = 0; i < f->input[4]; ++i)
        CHECK(mc_test_java_random_equal(&f->player_randoms[i], f->actors[i]->random));
    if (f->receipt != NULL) {
        CHECK(f->receipt->holds >= 2 &&
              mc_world_access_receipt_retire(&f->receipt, NULL) == MC_WORLD_STORAGE_BUSY);
        bool removed = false;
        CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              removed && mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_BUSY);
    }
    if (f->input[0] == 0) {
        /* A consumed FW must not leave a dangling borrowed child when later
         * Registry retirement refuses. This is native metadata fault input,
         * after every source observation; cleanup retries no source work. */
        const size_t borrows = f->base.registry->runtime_borrows;
        const mc_java_random random = *f->base.world->fields.r;
        f->base.registry->runtime_borrows = 0;
        mc_world_nightmare_cleanup cleanup;
        CHECK(mc_world_nightmare_retire(&f->call, &cleanup) != MC_WORLD_STORAGE_OK &&
              f->call != NULL && cleanup.child == MC_WORLD_NIGHTMARE_CLASSES &&
              mc_world_nightmare_inspect(f->call, &f->state) && f->state.returned &&
              !f->state.result && f->state.failure->status == MC_WORLD_STORAGE_OK &&
              f->state.finder == NULL && f->state.failure->finder == NULL &&
              mc_test_java_random_equal(&random, f->base.world->fields.r));
        f->base.registry->runtime_borrows = borrows;
    } else if (f->input[0] == 1) {
        const size_t bindings = f->base.world->persistent_bindings;
        const mc_java_random random = *f->base.world->fields.r;
        f->base.world->persistent_bindings = 0;
        mc_world_nightmare_cleanup cleanup;
        CHECK(mc_world_nightmare_retire(&f->call, &cleanup) != MC_WORLD_STORAGE_OK &&
              f->call != NULL && mc_world_nightmare_inspect(f->call, &f->state) &&
              f->state.returned && !f->state.result &&
              f->state.failure->status == MC_WORLD_STORAGE_OK && f->state.finder == NULL &&
              mc_test_java_random_equal(&random, f->base.world->fields.r));
        f->base.world->persistent_bindings = bindings;
    }
    CHECK(mc_world_nightmare_retire(&f->call, NULL) == MC_WORLD_STORAGE_OK && f->call == NULL);
    if (f->receipt != NULL) {
        CHECK(f->receipt->holds == 1 &&
              mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_BUSY &&
              mc_world_access_receipt_retire(&f->receipt, NULL) == MC_WORLD_STORAGE_OK &&
              f->receipt == NULL && mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK);
        f->observer = NULL;
    }
    printf("NIGHTMARE_NATIVE case=%" PRId32 " returned=%u result=%u pc=%u attempts=%zu paths=%zu\n",
           f->input[0], f->state.returned ? 1u : 0u, f->state.result ? 1u : 0u, f->state.source_pc,
           f->state.attempts_started, f->state.paths);
    CHECK(fflush(stdout) == 0);
    return true;
}

static bool nightmare_literal_limit(nightmare_fixture *f) {
    const mc_world_nightmare_context context = {f->base.classes, f->base.world, f->base.math};
    mc_world_nightmare_limits limits = mc_world_nightmare_default_limits();
    limits.path.max_points = 0;
    mc_world_nightmare_error admission;
    CHECK(mc_world_nightmare_begin(f->base.world, f->passed, &context, &limits, &f->call,
                                   &admission) == MC_WORLD_STORAGE_LIMIT &&
          f->call != NULL && mc_world_nightmare_inspect(f->call, &f->state) && !f->state.returned &&
          f->state.source_pc == 410 && f->state.paths == 0 && f->state.constructors == 1 &&
          f->state.mob != NULL && f->state.mob->phase == MC_ENTITY_READY &&
          f->state.failure->finder != NULL &&
          f->state.failure->finder->path.status == MC_PATH_LIMIT && f->state.finder != NULL &&
          f->base.world->fields.b->count == 1 && f->actors[0]->local_player->fields.player.u);
    const mc_java_random random = *f->base.world->fields.r;
    const size_t borrows = f->base.registry->runtime_borrows;
    f->base.registry->runtime_borrows = 0;
    mc_world_nightmare_cleanup cleanup;
    CHECK(mc_world_nightmare_retire(&f->call, &cleanup) != MC_WORLD_STORAGE_OK && f->call != NULL &&
          mc_world_nightmare_inspect(f->call, &f->state) &&
          f->state.failure->status == MC_WORLD_STORAGE_LIMIT && f->state.failure->pc == 410 &&
          f->state.failure->finder == NULL && f->state.finder == NULL &&
          mc_test_java_random_equal(&random, f->base.world->fields.r));
    f->base.registry->runtime_borrows = borrows;
    CHECK(mc_world_nightmare_retire(&f->call, NULL) == MC_WORLD_STORAGE_OK && f->call == NULL &&
          mc_test_java_random_equal(&random, f->base.world->fields.r));
    puts("NIGHTMARE_LIMITS literal_points=0 source_pc=410 checked_cleanup=1 source_replay=0");
    return true;
}

int main(int argc, char **argv) {
    if (argc != 5)
        return 2;
    FILE *profiles = open_file(argv[3], "rb");
    if (profiles == NULL)
        return 2;
    char line[512];
    unsigned int cases = 0;
    while (fgets(line, sizeof(line), profiles) != NULL) {
        nightmare_fixture *f = calloc(1, sizeof(*f));
        if (f == NULL)
            return 2;
#ifdef _MSC_VER
        const int fields = sscanf_s(line,
                                    "%" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32
#else
        const int fields = sscanf(line,
                                  "%" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32
#endif
                                    " %" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32,
                                    &f->input[0], &f->input[1], &f->input[2], &f->input[3],
                                    &f->input[4], &f->input[5], &f->input[6], &f->input[7]);
        char world[8192];
        const int length = snprintf(world, sizeof(world), "%s/case%u", argv[2], cases);
        const bool passed = fields == 8 && f->input[0] == (int32_t)cases && f->input[4] >= 0 &&
                            f->input[4] <= 2 && length > 0 && (size_t)length < sizeof(world) &&
                            nightmare_prepare(f, argv[1], world) && nightmare_observer(f) &&
                            nightmare_case(f, argv[4]);
        if (f->call != NULL) {
            fputs("Nightmare source failure remains owned; fixture retained\n", stderr);
            return 1;
        }
        /* End only the declared foreign source projection after observations
         * and checked call retirement. No source prefix is repaired/replayed. */
        if (f->base.world != NULL)
            f->base.world->fields.d = f->original_players;
        if (f->actors[1] != NULL)
            mc_entity_release(f->actors[1]);
        if (!teardown(&f->base) ||
            (f->observer != NULL &&
             mc_world_access_free(f->observer, NULL) != MC_WORLD_STORAGE_OK)) {
            fputs("Nightmare terminal fixture cleanup refused\n", stderr);
            return 1;
        }
        free(f);
        if (!passed)
            return 1;
        ++cases;
    }
    if (ferror(profiles) || fclose(profiles) != 0 || cases != 28)
        return 1;
    nightmare_fixture *control = calloc(1, sizeof(*control));
    if (control == NULL)
        return 2;
    const int32_t input[8] = {28, 2, 6, 0, 1, 0, 0, 0};
    memcpy(control->input, input, sizeof(input));
    char world[8192];
    const int length = snprintf(world, sizeof(world), "%s/case28", argv[2]);
    const bool passed = length > 0 && (size_t)length < sizeof(world) &&
                        nightmare_prepare(control, argv[1], world) && nightmare_observer(control) &&
                        nightmare_literal_limit(control);
    if (control->call != NULL || !passed || !teardown(&control->base) ||
        mc_world_access_free(control->observer, NULL) != MC_WORLD_STORAGE_OK)
        return 1;
    free(control);
    printf("Nightmare: %u checks;28 genuine profiles\n", checks);
    return 0;
}
