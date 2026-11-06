/* Prepared component inputs and lossless observations over the genuine damage
 * API. Expected state comes only from the independent original packet. */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#define MC_SLEEP_WAKE_ENTRY mc_damage_wake_original_unused_sleep_main
#include "test_sleep_wake.c"
#undef MC_SLEEP_WAKE_ENTRY

#include "../../../src/client/statistics/internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "mc/entity/movement/effects.h"
#include "mc/world/runtime/world_access.h"

typedef struct dw_original_sound {
    double position[3];
    float volume, pitch;
} dw_original_sound;

typedef struct dw_original_fixture {
    sw_fixture sleep;
    client_application *app;
    mc_inventory_player *inventory;
    mc_world_access *observer;
    dw_original_sound sound;
    unsigned int sounds;
    bool bad_callback;
} dw_original_fixture;

/* The original observer's two notification methods are explicitly empty.
 * A missing native callback would instead be an unsupported invocation. */
static mc_world_storage_status dw_original_cell(void *context, mc_world_runtime *world, int32_t x,
                                                int32_t y, int32_t z,
                                                mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status dw_original_range(void *context, mc_world_runtime *world,
                                                 int32_t min_x, int32_t min_y, int32_t min_z,
                                                 int32_t max_x, int32_t max_y, int32_t max_z,
                                                 mc_world_storage_error *error) {
    (void)context;
    (void)world;
    (void)min_x;
    (void)min_y;
    (void)min_z;
    (void)max_x;
    (void)max_y;
    (void)max_z;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status dw_original_sound_call(void *context, mc_world_runtime *world,
                                                      const mc_registry_string *name, double x,
                                                      double y, double z, float volume, float pitch,
                                                      mc_world_storage_error *error) {
    dw_original_fixture *s = context;
    mc_registry_string_view text = {0};
    static const char expected[] = "random.hurt";
    if (world != s->sleep.world->worlds[0] || s->sounds != 0 ||
        !mc_registry_string_read(name, &text) || text.length != sizeof(expected) - 1)
        s->bad_callback = true;
    if (!s->bad_callback)
        for (size_t i = 0; i < text.length; ++i)
            if (text.units[i] != (uint16_t)expected[i])
                s->bad_callback = true;
    if (s->bad_callback) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "damage wake original sound transport", 0};
        return error->status;
    }
    s->sound = (dw_original_sound){{x, y, z}, volume, pitch};
    ++s->sounds;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool dw_original_prepare(dw_original_fixture *s, const char *root, const char *resource_path,
                                const int32_t input[5]) {
    bool ok = false;
    mc_player_string *username = NULL, *token = NULL;
    s->app = calloc(1, sizeof(*s->app));
    CHECK(s->app != NULL &&
          mc_resource_files_create(resource_path, &s->app->files) == MC_REGISTRY_OK);
    tick_resources = mc_resource_files_source(s->app->files);
    const int32_t sleep_input[5] = {input[0], 1, input[1], 0, 0};
    const bool prepared = sw_prepare(&s->sleep, root, sleep_input);
    tick_resources = NULL;
    CHECK(prepared && app_setup(s->sleep.world, s->app, root));
    static const uint16_t user[] = {'B', 'e', 'd', 'N', 'i', 'g', 'h', 't', 'C',
                                    'o', 'm', 'p', 'o', 'n', 'e', 'n', 't'},
                          sid[] = {'-'};
    CHECK(mc_player_string_create(user, COUNT(user), &username) == MC_PLAYER_OK &&
          mc_player_string_create(sid, COUNT(sid), &token) == MC_PLAYER_OK &&
          mc_client_application_create_session(s->app, username, token, NULL) ==
              MC_CLIENT_STATISTICS_OWNER_OK);
    mc_entity *actor = s->sleep.actor;
    mc_local_player_snapshot *fields = &actor->local_player->fields;
    mc_world_runtime *world = s->sleep.world->worlds[0];
    /* The fixture publishes this genuinely constructed App as the prepared
     * dc.b input before observation. It does not compare constructor sequences. */
    fields->minecraft = s->app;
    s->app->active_world = world;
    s->app->h = s->app->i = actor;
    s->inventory = fields->player.c;
    fields->living.E = 20;
    fields->living.Y = input[2];
    fields->living.Z = 17;
    actor->base.by = 0;
    fields->living.aa = 3;
    fields->living.ab = 4;
    fields->living.ad = 0;
    fields->living.ac = 99;
    fields->living.al = .25f;
    fields->living.au = 0;
    fields->living.av = 17;
    fields->living.ah = false;
    actor->base.bb = actor->base.be = false;
    actor->base.yaw = 30;
    fields->player.private_bO = 0;
    mc_java_random_reseed(actor->random, 38);
    mc_java_random_reseed(world->fields.r, 2468);
    double discarded;
    CHECK(mc_java_math_random(s->app->profile, &discarded) == MC_JAVA_RANDOM_PROFILE_OK &&
          s->app->profile->math != NULL);
    mc_java_random seeded;
    mc_java_random_seed(&seeded, 0);
    atomic_store(&s->app->profile->math->state, seeded.state);
    s->app->profile->math->have_next_next_gaussian = false;
    s->app->profile->math->next_next_gaussian = 0;
    const mc_world_access_callbacks callbacks = {
        .cell = dw_original_cell, .range = dw_original_range, .sound = dw_original_sound_call};
    CHECK(mc_world_access_create(&callbacks, s, &s->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, s->observer, NULL) == MC_WORLD_STORAGE_OK);
    const int32_t mode = input[4];
    if (mode == 1 || mode == 10)
        world->fields.B = true;
    if (mode == 2 || mode == 7 || mode == 12)
        actor->base.world = NULL;
    if (mode == 3 || mode == 8 || mode == 9 || mode == 10)
        actor->bounds = NULL;
    if (mode == 4 || mode == 9 || mode == 12) {
        fields->player.v = NULL;
        mc_player_nbt_coordinates_collect_held(actor);
    }
    if (mode == 5 || mode == 8) {
        const int32_t direction = input[1];
        mc_chunk_data *data = s->sleep.world->states[0]->data;
        CHECK(sw_cell(data, 8, 64, 8, 0, 0) &&
              sw_cell(data, 8 - sw_facing[direction][0], 64, 8 - sw_facing[direction][1], 0, 0));
    }
    if (mode == 6)
        fields->minecraft = NULL;
    if (mode == 7 || mode == 13)
        fields->player.u = false;
    if (mode == 11)
        fields->player.c = NULL;
    s->sleep.before_v = fields->player.v;
    s->sleep.before_q = fields->player.private_b;
    s->sleep.before_bn = fields->player.private_bN;
    ok = true;
cleanup:
    tick_resources = NULL;
    mc_player_string_release(username);
    mc_player_string_release(token);
    return ok;
}

static bool dw_original_map(const mc_client_stats_map *map) {
    if (putchar('[') == EOF)
        return false;
    size_t count = 0;
    int32_t last = INT32_MIN;
    /* The selected source calls can introduce one general statistic only.
     * Reject additional entries rather than silently truncating the map. */
    for (const stats_entry *entry = map->head; entry != NULL; entry = entry->next) {
        const mc_statistic_fields *fields = mc_statistic_fields_get(entry->key);
        if (fields == NULL || (count != 0 && fields->e <= last) ||
            printf("%s[%" PRId32 ",%" PRId32 "]", count == 0 ? "" : ",", fields->e, entry->value) <
                0)
            return false;
        last = fields->e;
        ++count;
    }
    return count == map->count && count <= 1 && putchar(']') != EOF;
}

static bool dw_original_state(const dw_original_fixture *s) {
    const sw_fixture *sleep = &s->sleep;
    const mc_entity *actor = sleep->actor;
    const mc_local_player_snapshot *fields = &actor->local_player->fields;
    const mc_player_fields *p = &fields->player;
    const mc_living_entity_fields *v = &fields->living;
    const mc_world_runtime_fields *world = &sleep->world->worlds[0]->fields;
    const mc_chunk_data *data = sleep->world->states[0]->data;
    uint8_t metadata;
    if (!mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, &metadata) ||
        printf("{\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
               "\"],\"motion\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
               "\"],\"dimensions\":[\"%08" PRIx32 "\",\"%08" PRIx32 "\",\"%08" PRIx32
               "\"],\"sleep_offsets\":[\"%08" PRIx32 "\",\"%08" PRIx32
               "\"],\"sleeping\":%s,\"timer\":%" PRId32 ",\"bed\":",
               movement_double_bits(actor->base.x), movement_double_bits(actor->base.y),
               movement_double_bits(actor->base.z), movement_double_bits(actor->base.velocity_x),
               movement_double_bits(actor->base.velocity_y),
               movement_double_bits(actor->base.velocity_z), movement_float_bits(actor->base.width),
               movement_float_bits(actor->base.height), movement_float_bits(actor->base.offset),
               movement_float_bits(p->w), movement_float_bits(p->y), sw_bool(p->u),
               p->private_a) < 0 ||
        !sw_xyz(p->v) || printf(",\"spawn\":") < 0 || !sw_xyz(p->private_b) ||
        printf(",\"private_coordinate\":") < 0 || !sw_xyz(p->private_bN) ||
        printf(",\"bounds\":") < 0 || !sw_bounds(actor->bounds) ||
        printf(",\"original_bounds\":") < 0 || !sw_bounds(sleep->original_bounds))
        return false;
    if (printf(",\"world_same\":%s,\"bounds_same\":%s,\"bed_same\":%s,\"spawn_same\":%s,"
               "\"private_same\":%s,\"health\":%" PRId32 ",\"previous_health\":%" PRId32
               ",\"hurt_resistance\":%" PRId32 ",\"hurt_time\":%" PRId32
               ",\"hurt_maximum\":%" PRId32 ",\"previous_damage\":%" PRId32 ",\"idle\":%" PRId32
               ",\"attacked\":%s,\"removed\":%s,\"armor_remainder\":%" PRId32
               ",\"world_sleeping\":%s,\"remote\":%s,\"world_players\":%zu,\"world_entities\":%zu,"
               "\"head\":[%u,%u],\"planes\":",
               sw_bool(actor->base.world == sleep->world->worlds[0]),
               sw_bool(actor->bounds == sleep->original_bounds), sw_bool(p->v == sleep->before_v),
               sw_bool(p->private_b == sleep->before_q), sw_bool(p->private_bN == sleep->before_bn),
               v->Y, v->Z, actor->base.by, v->aa, v->ab, v->au, v->av, sw_bool(actor->base.bb),
               sw_bool(actor->base.be), p->private_bO, sw_bool(world->J), sw_bool(world->B),
               world->d->count, world->b->count, (unsigned int)data->blocks[boxes_index(8, 64, 8)],
               (unsigned int)metadata) < 0 ||
        !sw_planes(data) || printf(",\"random\":[") < 0 || !sw_random(actor->random) ||
        putchar(',') == EOF || !sw_random(world->r))
        return false;
    const mc_java_math_random_owner *math = s->app->profile->math;
    const mc_client_stats_writer *writer = s->app->stats_writer_retained;
    if (math == NULL || writer == NULL ||
        printf(",[\"%016" PRIx64 "\",%s,\"%016" PRIx64 "\"]],\"statistics\":{\"total\":",
               atomic_load(&math->state), sw_bool(math->have_next_next_gaussian),
               movement_double_bits(math->next_next_gaussian)) < 0 ||
        !dw_original_map(writer->total) || printf(",\"unsent\":") < 0 ||
        !dw_original_map(writer->unsent) ||
        printf(",\"dirty\":%s},\"sounds\":[", sw_bool(writer->dirty)) < 0)
        return false;
    if (s->sounds != 0) {
        if (s->sounds != 1 ||
            printf("{\"name\":\"random.hurt\",\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64
                   "\",\"%016" PRIx64 "\"],\"volume\":\"%08" PRIx32 "\",\"pitch\":\"%08" PRIx32
                   "\"}",
                   movement_double_bits(s->sound.position[0]),
                   movement_double_bits(s->sound.position[1]),
                   movement_double_bits(s->sound.position[2]), movement_float_bits(s->sound.volume),
                   movement_float_bits(s->sound.pitch)) < 0)
            return false;
    }
    return printf("]}") >= 0;
}

static bool dw_original_retire(dw_original_fixture *s, mc_entity_effect_error *error) {
    if (error != NULL && !mc_entity_effect_error_retire(error))
        return false;
    if (s->sleep.actor != NULL) {
        if (mc_entity_effect_has_pending(s->sleep.actor) &&
            mc_entity_effect_cleanup(s->sleep.actor, NULL) != MC_ENTITY_OK)
            return false;
        s->sleep.actor->local_player->fields.minecraft = s->app;
        if (s->inventory != NULL)
            s->sleep.actor->local_player->fields.player.c = s->inventory;
    }
    if (s->observer != NULL) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(s->sleep.world->worlds[0], s->observer, &removed,
                                                 NULL) != MC_WORLD_STORAGE_OK ||
            !removed || mc_world_access_free(s->observer, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        s->observer = NULL;
    }
    if (s->app != NULL &&
        (mc_client_application_retire_statistics(s->app, NULL) != MC_CLIENT_STATISTICS_OWNER_OK ||
         mc_client_application_retire_water_effect(s->app, NULL) != MC_CLIENT_STATISTICS_OWNER_OK ||
         mc_client_achievement_notice_free(&s->app->achievement_notice, NULL) !=
             MC_CLIENT_NOTICE_OK ||
         (s->app->render_classes != NULL &&
          mc_client_render_classes_free(s->app->render_classes) != MC_CLIENT_MODEL_OK)))
        return false;
    if (s->sleep.world != NULL && !sw_retire(&s->sleep))
        return false;
    if (s->app != NULL) {
        if (mc_game_settings_retire(&s->app->settings, NULL) != MC_GAME_SETTINGS_OK)
            return false;
        mc_client_game_directory_free(s->app->diagnostic_game_directory);
        mc_resource_files_free(s->app->files);
        free(s->app);
        s->app = NULL;
    }
    tick_chunk = NULL;
    tick_context = (mc_world_entity_tick_context){0};
    return true;
}

static bool dw_original_profile(const char *root, const char *resource_path,
                                const int32_t input[5]) {
    bool ok = false;
    dw_original_fixture s = {0};
    mc_entity_effect_error *error = calloc(1, sizeof(*error));
    char path[8192];
    CHECK(error != NULL && snprintf(path, sizeof(path), "%s/case-%" PRId32, root, input[0]) > 0 &&
          dw_original_prepare(&s, path, resource_path, input));
    const size_t holds = s.sleep.actor->holds;
    CHECK(fflush(stdout) == 0 &&
          printf("PLAYER_DAMAGE_WAKE_NATIVE {\"case\":%" PRId32 ",\"input\":[%" PRId32 ",%" PRId32
                 ",%" PRId32 ",%" PRId32 ",%" PRId32 "],\"before\":",
                 input[0], input[0], input[1], input[2], input[3], input[4]) >= 0 &&
          dw_original_state(&s) && fflush(stdout) == 0);
    bool result = false;
    const mc_entity_status status = mc_entity_effect_damage_in_context(
        s.sleep.actor, s.sleep.world->registry[0], NULL, input[3], &result, error);
    if (status != MC_ENTITY_OK &&
        (status != MC_ENTITY_STATE_ERROR || error->cause != MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE))
        fprintf(stderr, "damage wake original case%" PRId32 " status%d cause%d pc%u: %s\n",
                input[0], (int)status, (int)error->cause, (unsigned int)error->source_bytecode,
                error->operation == NULL ? "-" : error->operation);
    CHECK(!s.bad_callback &&
          (status == MC_ENTITY_OK || (status == MC_ENTITY_STATE_ERROR &&
                                      error->cause == MC_ENTITY_EFFECT_CAUSE_NULL_REFERENCE)));
    CHECK(printf(",\"after\":") >= 0 && dw_original_state(&s) &&
          printf(",\"result\":%s,\"exception\":\"%s\"}\n",
                 status == MC_ENTITY_OK ? sw_bool(result) : "null",
                 status == MC_ENTITY_OK ? "NONE" : "java/lang/NullPointerException") >= 0 &&
          fflush(stdout) == 0);
    CHECK(mc_entity_effect_error_retire(error) && !mc_entity_effect_has_pending(s.sleep.actor) &&
          s.sleep.actor->holds == holds && s.sleep.world->worlds[0]->active_calls == 0 &&
          s.sleep.world->worlds[0]->local_leases == 0);
    ok = true;
cleanup:
    if (!dw_original_retire(&s, error))
        ok = false;
    free(error);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    char root[8192], resource_path[8192];
    FILE *profiles = NULL;
    if (argc != 5 || strcmp(argv[1], "damage-wake-original") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) ||
        !decode_path(argv[3], resource_path, sizeof(resource_path)) || !mc_math_trig_init(&trig))
        return 2;
    profiles = fopen(argv[4], "r");
    CHECK(profiles != NULL);
    char line[256];
    int32_t count = 0;
    while (fgets(line, sizeof(line), profiles) != NULL) {
        int32_t input[5] = {0};
        char *cursor = line;
        for (size_t i = 0; i < COUNT(input); ++i) {
            while (isspace((unsigned char)*cursor))
                ++cursor;
            errno = 0;
            char *end = NULL;
            const intmax_t value = strtoimax(cursor, &end, 10);
            CHECK(errno == 0 && end != cursor && value >= INT32_MIN && value <= INT32_MAX &&
                  (*end == '\0' || isspace((unsigned char)*end)));
            input[i] = (int32_t)value;
            cursor = end;
        }
        while (isspace((unsigned char)*cursor))
            ++cursor;
        CHECK(*cursor == '\0' && count < 32 && input[0] == count && input[1] >= 0 && input[1] < 4 &&
              (input[3] == 0 || input[3] == 4) && input[4] >= 0 && input[4] <= 13 &&
              dw_original_profile(root, resource_path, input));
        ++count;
    }
    CHECK(!ferror(profiles) && count == 32);
    result = 0;
cleanup:
    if (profiles != NULL && fclose(profiles) != 0)
        result = 1;
    if (result == 0 && printf("PASS: 32 Player damage wake original rows; %u checks\n", checks) < 0)
        return 1;
    return result;
}
