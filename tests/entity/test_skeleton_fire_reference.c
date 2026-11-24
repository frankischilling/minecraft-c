/* Actual protected FR calls over the genuine shared fixture. Default clocks,
 * own Random streams and returned Gaussian values are never supplied as input. */
#include <stdint.h>
static int64_t srf_world_seed;
#define MC_PLAYER_STATISTICS_WORLD_SEED srf_world_seed
#define MC_SKELETON_FIRE_MAIN mc_skeleton_reference_unused_controls_main
#include "test_skeleton_fire.c"
#undef MC_SKELETON_FIRE_MAIN
#undef MC_PLAYER_STATISTICS_WORLD_SEED

#include "entity/watcher_payload_internal.h"
#include <errno.h>
#include <inttypes.h>

enum { SRF_ROWS = 24, SRF_COLUMNS = 10 };

typedef struct srf_profile {
    int32_t v[SRF_COLUMNS];
    uint32_t distance;
} srf_profile;

typedef struct srf_row {
    sf_fixture *fixture;
    const srf_profile *profile;
    mc_entity *target, *visible;
    mc_world_access *access_a;
    mc_java_random *nulled_arrow_random;
    size_t callback_count;
    bool callback_output_failed;
} srf_row;

static mc_world_runtime *srf_world_a;
static mc_entity *srf_wolf;

static const char *srf_bool(bool value) {
    return value ? "true" : "false";
}

static float srf_float(uint32_t bits) {
    float value;
    memcpy(&value, &bits, 4);
    return value;
}

static void srf_d(double value) {
    printf("\"%016" PRIx64 "\"", ac_dbits(value));
}

static void srf_f(float value) {
    uint32_t bits;
    memcpy(&bits, &value, 4);
    printf("\"%08" PRIx32 "\"", bits);
}

static void srf_random(const mc_java_random *value) {
    if (value == NULL) {
        fputs("null", stdout);
        return;
    }
    printf("[\"%016" PRIx64 "\",%s,", value->state, srf_bool(value->have_next_next_gaussian));
    srf_d(value->next_next_gaussian);
    putchar(']');
}

static void srf_math_random(const mc_java_random_profile *profile) {
    if (profile == NULL || profile->math == NULL) {
        fputs("null", stdout);
        return;
    }
    const mc_java_random value = {.state = atomic_load(&profile->math->state),
                                  .next_next_gaussian = profile->math->next_next_gaussian,
                                  .have_next_next_gaussian =
                                      profile->math->have_next_next_gaussian};
    srf_random(&value);
}

static void srf_doubles(const double *values, size_t count) {
    putchar('[');
    for (size_t i = 0; i < count; ++i) {
        if (i != 0)
            putchar(',');
        srf_d(values[i]);
    }
    putchar(']');
}

static void srf_floats(const float *values, size_t count) {
    putchar('[');
    for (size_t i = 0; i < count; ++i) {
        if (i != 0)
            putchar(',');
        srf_f(values[i]);
    }
    putchar(']');
}

static bool srf_watch(const mc_entity_watcher *watcher) {
    if (watcher == NULL) {
        fputs("null", stdout);
        return true;
    }
    printf("{\"map_null\":%s,\"dirty\":%s,\"records\":[", srf_bool(watcher->map == NULL),
           srf_bool(watcher->dirty));
    const mc_entity_watch_node *nodes[64];
    size_t count = 0;
    if (watcher->map != NULL) {
        for (const mc_entity_watch_node *node = watcher->map->head; node != NULL;
             node = node->next) {
            CHECK(count < sizeof(nodes) / sizeof(*nodes) && node->record != NULL);
            size_t at = count++;
            while (at != 0 && nodes[at - 1]->key > node->key) {
                nodes[at] = nodes[at - 1];
                --at;
            }
            nodes[at] = node;
        }
        CHECK(count == watcher->map->size);
    }
    for (size_t i = 0; i < count; ++i) {
        const mc_entity_watch_record *r = nodes[i]->record;
        printf("%s{\"key\":%" PRId32 ",\"type\":%" PRId32 ",\"id\":%" PRId32
               ",\"value\":{\"kind\":",
               i == 0 ? "" : ",", nodes[i]->key, r->type, r->index);
        switch (mc_entity_watch_payload_kind_of(r)) {
            case MC_ENTITY_WATCH_PAYLOAD_NULL:
                fputs("\"Null\",\"value\":null", stdout);
                break;
            case MC_ENTITY_WATCH_PAYLOAD_BYTE:
                printf("\"Byte\",\"value\":%d", (int)r->byte_value);
                break;
            case MC_ENTITY_WATCH_PAYLOAD_INTEGER:
                printf("\"Integer\",\"value\":%" PRId32, r->integer_value);
                break;
            case MC_ENTITY_WATCH_PAYLOAD_STRING: {
                mc_player_string_view text;
                CHECK(mc_player_string_read(r->string_value, &text));
                fputs("\"String\",\"value\":[", stdout);
                for (size_t j = 0; j < text.length; ++j)
                    printf("%s%u", j == 0 ? "" : ",", (unsigned int)text.units[j]);
                putchar(']');
                break;
            }
            default:
                CHECK(false);
        }
        printf("},\"dirty\":%s}", srf_bool(r->dirty));
    }
    fputs("]}", stdout);
    return true;
}

static bool srf_snapshot(const mc_entity *p, const mc_entity *shooter) {
    if (p == NULL) {
        fputs("null", stdout);
        return true;
    }
    mc_entity_class_literal_view kind;
    CHECK(mc_entity_class_literal_inspect(p->class_literal, &kind) == MC_ENTITY_OK);
    printf("{\"class\":\"%s\",\"id\":%" PRId32 ",\"world\":", kind.client_name, p->base.id);
    CHECK(p->base.world == NULL || p->base.world == srf_world_a);
    fputs(p->base.world == NULL ? "null" : "\"world-a\"", stdout);
    fputs(",\"arrow\":", stdout);
    if (p->arrow == NULL)
        fputs("null", stdout);
    else {
        const mc_arrow_fields *a = &p->arrow->fields;
        CHECK(a->c == NULL || a->c == shooter || a->c == srf_wolf);
        printf("{\"a\":%s,\"b\":%" PRId32 ",\"c\":%s,\"d\":%" PRId32 ",\"e\":%" PRId32
               ",\"f\":%" PRId32 ",\"g\":%" PRId32 ",\"h\":%" PRId32 ",\"i\":%s,\"j\":%" PRId32
               ",\"k\":%" PRId32 "}",
               srf_bool(a->a), a->b,
               a->c == NULL      ? "null"
               : a->c == shooter ? "\"skeleton\""
                                 : "\"wolf\"",
               a->d, a->e, a->f, a->g, a->h, srf_bool(a->i), a->j, a->k);
    }
    const mc_entity_base_fields *b = &p->base;
    const double position[] = {b->x, b->y, b->z},
                 motion[] = {b->velocity_x, b->velocity_y, b->velocity_z},
                 history[] = {b->aJ, b->aK, b->aL, b->bl, b->bm, b->bn};
    const float angles[] = {b->yaw, b->aT, b->aU, b->aV},
                dimensions[] = {b->width, b->height, b->offset};
    fputs(",\"position\":", stdout);
    srf_doubles(position, 3);
    fputs(",\"motion\":", stdout);
    srf_doubles(motion, 3);
    fputs(",\"history\":", stdout);
    srf_doubles(history, 6);
    fputs(",\"angles\":", stdout);
    srf_floats(angles, 4);
    fputs(",\"dimensions\":", stdout);
    srf_floats(dimensions, 3);
    fputs(",\"bounds\":", stdout);
    if (p->bounds == NULL)
        fputs("null", stdout);
    else {
        const double bounds[] = {p->bounds->min_x, p->bounds->min_y, p->bounds->min_z,
                                 p->bounds->max_x, p->bounds->max_y, p->bounds->max_z};
        srf_doubles(bounds, 6);
    }
    printf(",\"dead\":%s,\"random\":", srf_bool(b->be));
    srf_random(p->random);
    fputs(",\"watcher\":", stdout);
    CHECK(srf_watch(p->watcher));
    putchar('}');
    return true;
}

static const char *srf_world_name(const mc_world_runtime *world) {
    return world == NULL ? "null" : world == srf_world_a ? "\"world-a\"" : "\"UNDECLARED\"";
}

static bool srf_actor_state(srf_row *row) {
    mc_entity *actor = sf_actor(row->fixture);
    fputs("{\"entity\":", stdout);
    CHECK(srf_snapshot(actor, actor));
    printf(",\"cooldown\":%" PRId32 ",\"attacking\":%s}", actor->mob->living.ae,
           srf_bool(actor->mob->creature_attacking));
    return true;
}

static bool srf_members(const mc_world_reference_list *list, const mc_entity *arrow) {
    CHECK(list != NULL && list->count <= 16);
    putchar('[');
    for (size_t i = 0; i < list->count; ++i) {
        CHECK(arrow != NULL && list->items[i] == arrow);
        printf("%s\"arrow\"", i == 0 ? "" : ",");
    }
    putchar(']');
    return true;
}

static bool srf_chunk(srf_row *row, mc_world_runtime *world, mc_world_store *store,
                      mc_chunk_state *state) {
    if (state == NULL) {
        fputs("null", stdout);
        return true;
    }
    mc_world_chunk *chunk = mc_world_store_chunk_object(store, state);
    mc_world_chunk_fields fields;
    CHECK(chunk != NULL && mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK &&
          fields.d == world && fields.j == state->x && fields.k == state->z);
    printf("{\"x\":%" PRId32 ",\"z\":%" PRId32 ",\"flags\":[%s,%s,%s,%s],\"slices\":[", fields.j,
           fields.k, srf_bool(fields.n), srf_bool(fields.o), srf_bool(fields.p),
           srf_bool(fields.q));
    for (size_t i = 0; i < 8; ++i) {
        const mc_world_chunk_entity_list *list = NULL;
        size_t count = 0;
        CHECK(mc_world_chunk_slice_at(fields.m, i, &list) == MC_WORLD_STORAGE_OK &&
              mc_world_chunk_entity_count(list, &count) == MC_WORLD_STORAGE_OK && count <= 16);
        printf("%s[", i == 0 ? "" : ",");
        for (size_t j = 0; j < count; ++j) {
            mc_entity *entity = NULL;
            CHECK(mc_world_chunk_entity_at(list, j, &entity) == MC_WORLD_STORAGE_OK &&
                  row->visible != NULL && entity == row->visible);
            printf("%s\"arrow\"", j == 0 ? "" : ",");
        }
        putchar(']');
    }
    fputs("]}", stdout);
    return true;
}

static bool srf_world_state(srf_row *row, mc_world_runtime *world) {
    if (world == NULL) {
        fputs("null", stdout);
        return true;
    }
    CHECK(world == srf_world_a);
    const mc_world_reference_list *accesses = world->fields.u;
    CHECK(accesses != NULL && accesses->count <= 2 &&
          accesses->count == world->world_accesses.count);
    printf("{\"identity\":%s,\"remote\":%s,\"entities\":", srf_world_name(world),
           srf_bool(world->fields.B));
    CHECK(srf_members(world->fields.b, row->visible));
    fputs(",\"players\":", stdout);
    CHECK(srf_members(world->fields.d, row->visible));
    fputs(",\"accesses\":[", stdout);
    for (size_t i = 0; i < accesses->count; ++i) {
        CHECK(accesses->items[i] == NULL || accesses->items[i] == row->access_a);
        printf("%s%s", i == 0 ? "" : ",", accesses->items[i] == NULL ? "null" : "\"probe\"");
    }
    fputs("],\"random\":", stdout);
    srf_random(world->fields.r);
    fputs(",\"chunk\":", stdout);
    CHECK(srf_chunk(row, world, row->fixture->construction.base.store, row->fixture->loaded));
    putchar('}');
    return true;
}

static bool srf_context(srf_row *row) {
    fixture *base = &row->fixture->construction.base;
    printf("{\"counter\":%" PRId32 ",\"actor\":", base->app.entities->next_id);
    CHECK(srf_actor_state(row));
    fputs(",\"target\":", stdout);
    CHECK(srf_snapshot(row->target, sf_actor(row->fixture)));
    fputs(",\"wolf\":", stdout);
    CHECK(srf_snapshot(srf_wolf, sf_actor(row->fixture)));
    fputs(",\"world_a\":", stdout);
    CHECK(srf_world_state(row, srf_world_a));
    fputs(",\"world_b\":null", stdout);
    fputs(",\"math_random\":", stdout);
    srf_math_random(base->app.profile);
    putchar('}');
    return true;
}

static bool srf_callback_start(srf_row *row) {
    CHECK(row->callback_count < 4);
    if (row->callback_count++ != 0)
        putchar(',');
    return true;
}

static mc_world_storage_status srf_callback_failure(srf_row *row, mc_world_storage_error *error,
                                                    const char *operation, bool output_failed) {
    row->callback_output_failed = row->callback_output_failed || output_failed;
    *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation, 0};
    return error->status;
}

static mc_world_storage_status srf_sound_callback(void *context, mc_world_runtime *world,
                                                  const mc_registry_string *name, double x,
                                                  double y, double z, float volume, float pitch,
                                                  mc_world_storage_error *error) {
    srf_row *row = context;
    mc_registry_string_view text;
    static const uint16_t bow[] = {'r', 'a', 'n', 'd', 'o', 'm', '.', 'b', 'o', 'w'};
    if (world != srf_world_a || !mc_registry_string_read(name, &text) || text.length != 10 ||
        memcmp(text.units, bow, sizeof(bow)) != 0 || !srf_callback_start(row))
        return srf_callback_failure(row, error, "native reference sound transport", true);
    fputs("{\"kind\":\"sound\",\"world\":\"world-a\",\"name\":\"random.bow\",\"position\":",
          stdout);
    const double position[] = {x, y, z};
    srf_doubles(position, 3);
    fputs(",\"volume\":", stdout);
    srf_f(volume);
    fputs(",\"pitch\":", stdout);
    srf_f(pitch);
    fputs(",\"actor\":", stdout);
    if (!srf_actor_state(row))
        return srf_callback_failure(row, error, "native reference actor transport", true);
    putchar('}');
    const int32_t mode = row->profile->v[7];
    if (mode == 1)
        return srf_callback_failure(row, error, "authored native sound refusal; no Java Throwable",
                                    false);
    if (mode == 7)
        sf_actor(row->fixture)->base.world = NULL;
    if (!srf_callback_start(row))
        return srf_callback_failure(row, error, "native reference sound return transport", true);
    printf("{\"kind\":\"sound_return\",\"world\":\"world-a\",\"actor_world\":%s}",
           srf_world_name(sf_actor(row->fixture)->base.world));
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status srf_added_callback(void *context, mc_world_runtime *world,
                                                  mc_entity *arrow, mc_world_storage_error *error) {
    srf_row *row = context;
    if (world != srf_world_a || arrow == NULL || arrow->arrow == NULL || row->visible != NULL ||
        !srf_callback_start(row))
        return srf_callback_failure(row, error, "native reference publication transport", true);
    row->visible = arrow; /* Only this actual entity_added argument has ordinary visibility. */
    printf("{\"kind\":\"added\",\"world\":%s,\"arrow\":", srf_world_name(world));
    if (!srf_snapshot(arrow, sf_actor(row->fixture)))
        return srf_callback_failure(row, error, "native reference published Arrow transport", true);
    fputs(",\"world_state\":", stdout);
    if (!srf_world_state(row, world))
        return srf_callback_failure(row, error, "native reference publication World transport",
                                    true);
    putchar('}');
    const int32_t mode = row->profile->v[7];
    if (mode == 2)
        return srf_callback_failure(
            row, error, "authored native publication refusal; no Java Throwable", false);
    if ((mode == 3 || mode == 5) &&
        mc_arrow_shooter_store(arrow, mode == 3 ? NULL : srf_wolf, NULL) != MC_ENTITY_OK)
        return srf_callback_failure(row, error, "native reference authored shooter ownership",
                                    true);
    if (mode == 4) {
        row->nulled_arrow_random = arrow->random;
        arrow->random = NULL;
    }
    if (mode == 8)
        mc_java_random_reseed(arrow->random, row->profile->v[9]);
    if (!srf_callback_start(row))
        return srf_callback_failure(row, error, "native reference publication return transport",
                                    true);
    printf("{\"kind\":\"added_return\",\"world\":%s,\"arrow\":", srf_world_name(world));
    if (!srf_snapshot(arrow, sf_actor(row->fixture)))
        return srf_callback_failure(row, error, "native reference returned Arrow transport", true);
    putchar('}');
    return MC_WORLD_STORAGE_OK;
}

static bool srf_attach(srf_row *row) {
    const mc_world_access_callbacks callbacks = {.sound = srf_sound_callback,
                                                 .entity_added = srf_added_callback};
    CHECK(mc_world_access_create(&callbacks, row, &row->access_a, NULL) == MC_WORLD_STORAGE_OK);
    if (row->profile->v[6] == 1)
        CHECK(mc_world_runtime_add_world_access(srf_world_a, NULL, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_add_world_access(srf_world_a, row->access_a, NULL) ==
          MC_WORLD_STORAGE_OK);
    return true;
}

static bool srf_detach(mc_world_runtime *world, mc_world_access *access) {
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

static void srf_site(const mc_mob_site *site) {
    if (site->class_name == NULL) {
        fputs("null", stdout);
        return;
    }
    printf("{\"class\":\"%s\",\"method\":\"%s\",\"descriptor\":\"%s\",\"pc\":%" PRId32 "}",
           site->class_name, site->method, site->descriptor, site->pc);
}

static void srf_arrow_site(const mc_arrow_site *site) {
    const mc_mob_site converted = {site->class_name, site->method, site->descriptor, site->pc};
    srf_site(&converted);
}

static void srf_heading_receipt(const mc_arrow_error *error,
                                const mc_arrow_heading_arguments *arguments) {
    const mc_arrow_heading_arguments observed =
        error->heading_entered ? *arguments : (mc_arrow_heading_arguments){0};
    fputs("{\"site\":", stdout);
    srf_arrow_site(&error->site);
    fputs(",\"heading_site\":", stdout);
    srf_arrow_site(&error->heading_site);
    printf(",\"heading_entered\":%s,\"heading_returned\":%s,\"gaussian_calls\":%u,\"random_"
           "captured\":%s,\"arguments\":{\"direction\":",
           srf_bool(error->heading_entered), srf_bool(error->heading_returned),
           error->gaussian_calls, srf_bool(error->random_captured));
    const double direction[] = {observed.x, observed.y, observed.z};
    srf_doubles(direction, 3);
    fputs(",\"speed\":", stdout);
    srf_f(observed.speed);
    fputs(",\"spread\":", stdout);
    srf_f(observed.spread);
    fputs("},\"random_before\":", stdout);
    srf_random(error->random_captured ? &error->random_before : NULL);
    fputs(",\"random_after\":", stdout);
    srf_random(error->random_captured ? &error->random_after : NULL);
    fputs(",\"gaussian_values\":", stdout);
    srf_doubles(error->gaussian_values, 3);
    putchar('}');
}

static void srf_native(srf_row *row, mc_entity_status status, const mc_mob_tick_error *error,
                       const mt_skeleton_fire_observation *c, const mc_entity *arrow) {
    printf("{\"status\":%d,\"native_policy\":%s,\"source_exception\":%s,\"site\":", (int)status,
           srf_bool(error->native_policy), srf_bool(error->source_exception));
    srf_site(&error->site);
    fputs(",\"child_site\":", stdout);
    srf_site(&error->child_site);
    printf(
        ",\"capture\":{\"entered\":%s,\"returned\":%s,\"allocated\":%s,\"constructor_invoked\":%s,"
        "\"constructor_returned\":%s,\"eye_returned\":%s,\"actor_random_captured\":%s,\"actor_"
        "float_returned\":%s,\"sound_invoked\":%s,\"sound_returned\":%s,\"publication_invoked\":%s,"
        "\"publication_returned\":%s,\"publication_result\":%s,\"heading_invoked\":%s,\"heading_"
        "returned\":%s,\"cooldown_stored\":%s,\"facing_stored\":%s,\"attacking_stored\":%s",
        srf_bool(c->entered), srf_bool(c->returned), srf_bool(c->allocated),
        srf_bool(c->constructor_invoked), srf_bool(c->constructor_returned),
        srf_bool(c->eye_returned), srf_bool(c->actor_random_captured),
        srf_bool(c->actor_float_returned), srf_bool(c->sound_invoked), srf_bool(c->sound_returned),
        srf_bool(c->publication_invoked), srf_bool(c->publication_returned),
        srf_bool(c->publication_result), srf_bool(c->heading_invoked),
        srf_bool(c->heading_returned), srf_bool(c->cooldown_stored), srf_bool(c->facing_stored),
        srf_bool(c->attacking_stored));
    printf(",\"target_same\":%s,\"arrow_same\":%s,\"constructor_world\":%s,\"sound_world\":%s,"
           "\"publication_world\":%s,\"actor_random_same\":%s,\"distance\":",
           srf_bool(c->target_identity == (uintptr_t)row->target),
           srf_bool(c->arrow_identity == (uintptr_t)arrow),
           srf_world_name((mc_world_runtime *)c->constructor_world_identity),
           srf_world_name((mc_world_runtime *)c->sound_world_identity),
           srf_world_name((mc_world_runtime *)c->publication_world_identity),
           srf_bool(c->actor_random_identity == (uintptr_t)sf_actor(row->fixture)->random));
    srf_f(c->distance);
    fputs(",\"eye_height\":", stdout);
    srf_f(c->eye_height);
    fputs(",\"lift\":", stdout);
    srf_f(c->lift);
    const double direction[] = {c->dx, c->dy, c->dz};
    fputs(",\"direction\":", stdout);
    srf_doubles(direction, 3);
    fputs(",\"heading_arguments\":", stdout);
    if (!c->heading_invoked)
        fputs("null", stdout);
    else {
        const double arguments[] = {c->heading_x, c->heading_y, c->heading_z};
        fputs("{\"direction\":", stdout);
        srf_doubles(arguments, 3);
        fputs(",\"speed\":", stdout);
        srf_f(c->heading_speed);
        fputs(",\"spread\":", stdout);
        srf_f(c->heading_spread);
        putchar('}');
    }
    fputs(",\"actor_float\":", stdout);
    srf_f(c->actor_float);
    fputs(",\"sound_volume\":", stdout);
    srf_f(c->sound_volume);
    fputs(",\"sound_pitch\":", stdout);
    srf_f(c->sound_pitch);
    fputs(",\"actor_random_before\":", stdout);
    srf_random(c->actor_random_captured ? &c->actor_random_before : NULL);
    fputs(",\"actor_random_after\":", stdout);
    srf_random(c->actor_float_returned ? &c->actor_random_after : NULL);
    fputs(",\"constructor_result\":", stdout);
    if (!c->constructor_returned)
        fputs("null", stdout);
    else {
        fputs("{\"motion\":", stdout);
        srf_doubles(c->constructor_motion, 3);
        fputs(",\"angles\":", stdout);
        srf_floats(c->constructor_angles, 4);
        printf(",\"j\":%" PRId32 "}", c->constructor_j);
    }
    fputs(",\"constructor\":", stdout);
    if (c->constructor_invoked)
        srf_heading_receipt(&c->constructor, &c->constructor_arguments);
    else
        fputs("null", stdout);
    fputs(",\"final_heading\":", stdout);
    if (c->heading_invoked)
        srf_heading_receipt(&c->final_heading, &c->final_arguments);
    else
        fputs("null", stdout);
    fputs("}}", stdout);
}

static bool srf_profiles(const char *path, srf_profile rows[SRF_ROWS]) {
    FILE *file = NULL;
#ifdef _MSC_VER
    if (fopen_s(&file, path, "rb") != 0)
        return false;
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL)
        return false;
    char bytes[8193];
    const size_t size = fread(bytes, 1, sizeof(bytes), file);
    const bool valid = size < sizeof(bytes) && ferror(file) == 0;
    const int closed = fclose(file);
    CHECK(valid && closed == 0);
    bytes[size] = '\0';
    char *cursor = bytes;
    for (size_t i = 0; i < SRF_ROWS; ++i) {
        for (size_t j = 0; j < SRF_COLUMNS; ++j) {
            while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')
                ++cursor;
            errno = 0;
            char *end = NULL;
            if (j == 1) {
                const uintmax_t value = strtoumax(cursor, &end, 16);
                CHECK(end - cursor == 8 && errno == 0 && value <= UINT32_MAX);
                rows[i].distance = (uint32_t)value;
                rows[i].v[j] = 0;
            } else {
                const intmax_t value = strtoimax(cursor, &end, 10);
                CHECK(end != cursor && errno == 0 && value >= INT32_MIN && value <= INT32_MAX);
                rows[i].v[j] = (int32_t)value;
            }
            CHECK(*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n' || *end == '\0');
            cursor = end;
        }
        const int32_t *p = rows[i].v;
        CHECK(p[0] == (int32_t)i && p[2] >= -1 && p[2] <= 1 && p[3] >= 0 && p[3] <= 5 &&
              p[4] >= 0 && p[4] <= 2 && (p[5] == 0 || p[5] == 1) && (p[6] == 0 || p[6] == 1) &&
              p[7] >= 0 && p[7] <= 8 && (p[8] == 0 || p[8] == 1) &&
              (p[9] == 0 || (i == 23 && p[9] == 271828)));
    }
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')
        ++cursor;
    CHECK(cursor == bytes + size);
    return true;
}

static void srf_input(const srf_profile *profile) {
    putchar('[');
    for (size_t i = 0; i < SRF_COLUMNS; ++i) {
        if (i != 0)
            putchar(',');
        if (i == 1)
            printf("\"%08" PRIx32 "\"", profile->distance);
        else
            printf("%" PRId32, profile->v[i]);
    }
    putchar(']');
}

static bool srf_run(const char *resources, const srf_profile *profile) {
    sf_fixture *f = calloc(1, sizeof(*f));
    mc_mob_tick_error *error = calloc(1, sizeof(*error));
    srf_row row = {.fixture = f, .profile = profile};
    char directory[64];
    srf_world_seed = 5143 + 2 * (int64_t)profile->v[0];
    CHECK(f != NULL && error != NULL &&
          snprintf(directory, sizeof(directory), "skeleton-reference-%" PRId32, profile->v[0]) >
              0 &&
          sf_setup(f, resources, directory, false));
    srf_world_a = f->construction.base.world;
    CHECK(mc_world_info_spawn_set(srf_world_a->fields.x, 8, 64, 8) == MC_WORLD_INFO_OK);
    srf_wolf = f->construction.shooters[1];
    mc_entity *extra = NULL;
    switch (profile->v[3]) {
        case 0:
            break;
        case 1:
            row.target = f->construction.base.player;
            break;
        case 2:
            row.target = srf_wolf;
            break;
        case 3:
            CHECK(ac_allocate(&f->construction, &extra) &&
                  mc_arrow_construct_world(extra, srf_world_a, NULL) == MC_ENTITY_OK);
            row.target = extra;
            break;
        case 4:
            CHECK(sf_new_mob(f, MC_ENTITY_CLASS_KIND_GHAST, &extra));
            row.target = extra;
            break;
        default:
            CHECK(sf_new_mob(f, MC_ENTITY_CLASS_KIND_SKELETON, &extra));
            row.target = extra;
            break;
    }
    CHECK(sf_reset(f, row.target));
    if (profile->v[0] == 12) {
        CHECK(mc_entity_source_position_rotation(sf_actor(f), 32008.5, 64, 8.5, 30, 10, NULL) ==
                  MC_ENTITY_OK &&
              mc_entity_source_position_rotation(row.target, 32015.5, 66.25, 11.25, 0, 0, NULL) ==
                  MC_ENTITY_OK &&
              !mc_chunk_cache_contains(f->provider.cache, 2000, 0));
    }
    /* Genuine source-requested Chunks, after the authored target pose. */
    if (profile->v[5] != 0) {
        CHECK(mc_world_runtime_get(srf_world_a, 0, 0, &f->loaded) == MC_CHUNK_ACQUIRE_OK &&
              f->loaded != NULL && !f->loaded->blank && f->loaded->active);
    }
    sf_actor(f)->mob->living.ae = profile->v[2];
    srf_world_a->fields.B = profile->v[4] == 2;
    if (profile->v[4] == 0)
        sf_actor(f)->base.world = NULL;
    CHECK(srf_attach(&row));
    mc_java_random *actor_random = sf_actor(f)->random;
    if (profile->v[8] == 1)
        sf_actor(f)->random = NULL;
    printf("SKELETON_FIRE_NATIVE {\"schema\":\"SKELETON_FIRE_NATIVE_RAW_V1\",\"case\":%" PRId32
           ",\"input\":",
           profile->v[0]);
    srf_input(profile);
    fputs(",\"before\":", stdout);
    CHECK(srf_context(&row));
    fputs(",\"callbacks\":[", stdout);
    mc_entity *arrow = NULL;
    mt_skeleton_fire_observation capture;
    const mc_entity_status status =
        sf_call(f, row.target, srf_float(profile->distance), &arrow, &capture, error);
    CHECK(!row.callback_output_failed);
    printf("],\"executed\":true,\"complete_outcome\":%s,\"returned\":%s,\"exception\":",
           srf_bool(profile->v[0] != 15 && profile->v[0] != 17), srf_bool(status == MC_ENTITY_OK));
    if (status == MC_ENTITY_OK)
        fputs("\"NONE\"", stdout);
    else if (error->source_exception && error->exception_class != NULL)
        printf("\"%s\"", error->exception_class);
    else
        fputs("null", stdout);
    fputs(",\"after\":", stdout);
    CHECK(srf_context(&row));
    fputs(",\"arrow_after\":", stdout);
    CHECK(srf_snapshot(row.visible, sf_actor(f)));
    fputs(",\"local_arrow\":", stdout);
    CHECK(srf_snapshot(arrow, sf_actor(f)));
    fputs(",\"native\":", stdout);
    srf_native(&row, status, error, &capture, arrow);
    fputs("}\n", stdout);
    CHECK(ferror(stdout) == 0 && fflush(stdout) == 0);
    /* Authored nullable source fields are restored only after all observed
     * outputs. Native owners are retired through their actual captured edges. */
    sf_actor(f)->random = actor_random;
    sf_actor(f)->base.world = srf_world_a;
    if (row.nulled_arrow_random != NULL)
        arrow->random = row.nulled_arrow_random;
    CHECK(srf_detach(srf_world_a, row.access_a));
    if (profile->v[6] == 1) {
        bool removed = false;
        CHECK(mc_world_runtime_remove_world_access(srf_world_a, NULL, &removed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              removed);
    }
    mc_entity_release(arrow);
    mc_entity_release(extra);
    CHECK(sf_finish(f));
    free(error);
    free(f);
    srf_world_a = NULL;
    srf_wolf = NULL;
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    srf_profile profiles[SRF_ROWS];
    if (!srf_profiles(argv[3], profiles))
        return 1;
    size_t executed = 0, unexecuted = 0;
    for (size_t i = 0; i < SRF_ROWS; ++i) {
        if (i == 21) {
            /* Source publication allows Arrow.world=A in WorldB, while the
             * native terminal collector currently admits one World. Do not
             * execute this graph then rebind fields to manufacture cleanup. */
            printf("SKELETON_FIRE_NATIVE_BOUNDARY {\"schema\":\"SKELETON_FIRE_NATIVE_BOUNDARY_V1\","
                   "\"case\":21,\"input\":");
            srf_input(&profiles[i]);
            fputs(",\"executed\":false,\"complete_outcome\":false,"
                  "\"boundary\":\"cross-world-terminal-ownership\"}\n",
                  stdout);
            if (ferror(stdout) != 0 || fflush(stdout) != 0)
                return 1;
            ++unexecuted;
            continue;
        }
        printf("SKELETON_FIRE_FIXTURE_BEGIN case=%zu\n", i);
        if (fflush(stdout) != 0 || !srf_run(argv[1], &profiles[i]))
            return 1;
        ++executed;
    }
    if (executed != 23 || unexecuted != 1)
        return 1;
    printf("Skeleton firing reference: %zu checks;24 profiles;%zu genuine calls;%zu "
           "unexecuted;cleanup=1\n",
           checks, executed, unexecuted);
    return ferror(stdout) == 0 && fflush(stdout) == 0 ? 0 : 1;
}
