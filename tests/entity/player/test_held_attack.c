/* Actual Player/App/World construction; the included entry point is unused. */
#define MC_PLAYER_STATISTICS_WORLD_SEED INT64_C(23)
#define MC_PLAYER_ATTACK_MAIN mc_held_unused_unarmed_main
#include "test_attack.c"
#undef MC_PLAYER_ATTACK_MAIN

#include "item/stack/source_internal.h"

#include <errno.h>
#include <inttypes.h>

typedef struct held_case {
    char profile[128], mode[32], target[16];
    int32_t id, count, damage, health, hurt, last_damage, reward;
    bool falling, saddled, cow;
} held_case;

typedef struct held_log {
    mc_world_runtime *world;
    char names[8][64], events[16];
    uint32_t volume[8], pitch[8];
    mc_entity *items[8];
    size_t sounds, particles, breaks, item_count, event_count;
} held_log;

static bool literal_name(const mc_registry_string *name, char out[64]) {
    mc_registry_string_view text;
    if (!mc_registry_string_read(name, &text) || text.length >= 64)
        return false;
    for (size_t i = 0; i < text.length; ++i) {
        if (text.units[i] > 127 || text.units[i] == '"' || text.units[i] == '\\')
            return false;
        out[i] = (char)text.units[i];
    }
    out[text.length] = 0;
    return true;
}

static uint32_t held_word(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static mc_world_storage_status held_sound(void *context, mc_world_runtime *world,
                                          const mc_registry_string *name, double x, double y,
                                          double z, float volume, float pitch,
                                          mc_world_storage_error *error) {
    (void)x;
    (void)y;
    (void)z;
    held_log *log = context;
    if (world != log->world || log->sounds == 8 || log->event_count == 16 ||
        !literal_name(name, log->names[log->sounds])) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                          "held test sound observation bound", 0};
        return error->status;
    }
    log->volume[log->sounds] = held_word(volume);
    log->pitch[log->sounds] = held_word(pitch);
    log->breaks += strcmp(log->names[log->sounds], "random.break") == 0;
    ++log->sounds;
    log->events[log->event_count++] = 'S';
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status held_particle(void *context, mc_world_runtime *world,
                                             const mc_registry_string *name, double x, double y,
                                             double z, double vx, double vy, double vz,
                                             mc_world_storage_error *error) {
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)vx;
    (void)vy;
    (void)vz;
    held_log *log = context;
    if (world != log->world || log->particles == SIZE_MAX) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT,
                                          "held test particle observation bound", 0};
        return error->status;
    }
    ++log->particles;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status held_added(void *context, mc_world_runtime *world, mc_entity *item,
                                          mc_world_storage_error *error) {
    held_log *log = context;
    if (world != log->world || item == NULL || log->item_count == 8 || log->event_count == 16) {
        *error =
            (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, "held test Item observation bound", 0};
        return error->status;
    }
    log->items[log->item_count++] = item;
    log->events[log->event_count++] = 'A';
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool statistic_value(attack_fixture *f, const mc_statistic *statistic, int32_t *out) {
    CHECK(mc_client_stats_writer_value(f->base.app.stats_writer, statistic, out, NULL) ==
          MC_CLIENT_STATS_OK);
    return true;
}

static bool statistic_prepare(attack_fixture *f, const mc_statistic *statistic, int32_t value) {
    int32_t before;
    CHECK(statistic_value(f, statistic, &before) &&
          mc_client_stats_writer_increment(f->base.app.stats_writer, statistic,
                                           mc_java_i32_sub(value, before),
                                           NULL) == MC_CLIENT_STATS_OK);
    return true;
}

static int saddled(mc_entity *entity) {
    if (!mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_PIG))
        return -1;
    mc_entity_watch_record *value = record(entity, 16);
    return value == NULL ? -2 : (value->byte_value & 1) != 0;
}

static const char *attack_owner(mc_item_kind kind) {
    return kind == MC_ITEM_SHOVEL || kind == MC_ITEM_PICKAXE || kind == MC_ITEM_AXE ? "dq"
           : kind == MC_ITEM_SWORD                                                  ? "qd"
                                                                                    : "gm";
}

static bool selected_row(const held_case *input, attack_fixture *f, mc_item_stack *held,
                         const mc_item *item, int32_t power, held_log *log,
                         const mc_registry_statistic_view *view, int before_saddle) {
    mc_item_stack_fields after;
    const mc_item_stack *selected = NULL;
    int32_t used, broken = -1, kills, dealt;
    CHECK(mc_item_stack_fields_get(held, &after) == MC_ITEM_STACK_OK &&
          mc_inventory_player_current(f->base.player->local_player->fields.player.c, &selected,
                                      NULL) == MC_INVENTORY_OK &&
          statistic_value(f, view->E[input->id], &used) && stat(f, 20, &kills) &&
          stat(f, 17, &dealt));
    if (view->F[input->id] != NULL)
        CHECK(statistic_value(f, view->F[input->id], &broken));
    const mc_living_entity_fields *v = &f->victim->mob->living;
    const char *owner = attack_owner(item->kind);
    printf("{\"profile\":\"%s\",\"mode\":\"%s\",\"id-before\":%d,\"count-before\":%d,"
           "\"damage-before\":%d,\"animation-before\":5,\"id-after\":%d,\"count-after\":%d,"
           "\"damage-after\":%d,\"animation-after\":%d,\"selected-same\":%s,"
           "\"selected-cleared\":%s,\"inventory-changed\":%s,\"health\":%d,"
           "\"previous-health\":%d,\"hurt\":%d,\"hurt-time\":%d,\"max-hurt\":%d,"
           "\"last-damage\":%d,\"idle\":%d,\"dying\":%s,\"score\":%d,\"mob-kills\":%d,"
           "\"damage-dealt\":%d,\"used-before\":11,\"used-after\":%d,\"drops\":%zu,"
           "\"item-class\":\"%s\",\"attack-owner\":\"%s\",\"hit-owner\":\"%s\","
           "\"use-owner\":\"%s\",\"target-kind\":\"%s\",\"max-damage\":%d,"
           "\"attack-power\":%d,\"saddled-before\":%d,\"saddled-after\":%d,"
           "\"broken-before\":%d,\"broken-after\":%d,\"sound-events\":%zu,"
           "\"particle-events\":%zu,\"break-sounds\":%zu}",
           input->profile, input->mode, input->id, input->count, input->damage, after.id,
           after.count, after.damage, after.animation, selected == held ? "true" : "false",
           selected == NULL ? "true" : "false",
           f->base.player->local_player->fields.player.c->changed ? "true" : "false", v->Y, v->Z,
           f->victim->base.by, v->aa, v->ab, v->au, v->av, v->ah ? "true" : "false",
           f->base.player->local_player->fields.player.g, kills, dealt, used, log->item_count,
           item->constructor_class, owner, item->kind == MC_ITEM_SADDLE ? "sa" : owner,
           item->kind == MC_ITEM_SADDLE ? "sa"
           : item->kind == MC_ITEM_DYE  ? "km"
                                        : "gm",
           input->target, item->fields.a, power, before_saddle, saddled(f->victim),
           view->F[input->id] == NULL ? -1 : 17, broken, log->sounds, log->particles, log->breaks);
    return true;
}

static bool cow_items(attack_fixture *f, const held_log *log) {
    mc_world_chunk *chunk = mc_world_store_chunk_object(f->base.store, f->chunk);
    mc_world_chunk_fields fields;
    CHECK(chunk != NULL && mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK);
    printf(",\"items\":[");
    for (size_t i = 0; i < log->item_count; ++i) {
        mc_entity *item = log->items[i];
        mc_item_stack_fields stack;
        CHECK(mc_entity_is_kind(item, MC_ENTITY_CLASS_KIND_ITEM) && item->random != NULL &&
              mc_item_stack_fields_get(item->stack, &stack) == MC_ITEM_STACK_OK);
        size_t memberships = 0;
        for (size_t s = 0; s < 8; ++s) {
            const mc_world_chunk_entity_list *slice = NULL;
            size_t count;
            CHECK(mc_world_chunk_slice_at(fields.m, s, &slice) == MC_WORLD_STORAGE_OK &&
                  mc_world_chunk_entity_count(slice, &count) == MC_WORLD_STORAGE_OK);
            for (size_t j = 0; j < count; ++j) {
                mc_entity *member = NULL;
                CHECK(mc_world_chunk_entity_at(slice, j, &member) == MC_WORLD_STORAGE_OK);
                memberships += member == item;
            }
        }
        bool world_member = false, distinct = true;
        for (size_t j = 0; j < f->base.world->entity_membership.count; ++j)
            world_member |= f->base.world->entity_membership.entries[j] == item;
        for (size_t j = 0; j < i; ++j)
            distinct &= log->items[j] != item && log->items[j]->stack != item->stack;
        printf("%s{\"stack\":[%d,%d,%d,%d],\"pickup-delay\":%d,\"world-member\":%s,"
               "\"chunk-members\":%zu,\"distinct\":%s,\"private-random\":\"%016" PRIx64 "\"}",
               i == 0 ? "" : ",", stack.id, stack.count, stack.damage, stack.animation,
               item->item.c, world_member ? "true" : "false", memberships,
               distinct ? "true" : "false", item->random->state);
    }
    printf("],\"events\":[");
    for (size_t i = 0; i < log->event_count; ++i)
        printf("%s\"%s\"", i == 0 ? "" : ",", log->events[i] == 'S' ? "SOUND" : "ADD");
    printf("]}");
    return true;
}

static bool run_case(const char *resources, const held_case *input) {
    attack_fixture *f = calloc(1, sizeof(*f));
    mc_item_stack *held = NULL;
    const mc_entity_class_kind kind = strcmp(input->target, "Pig") == 0 ? MC_ENTITY_CLASS_KIND_PIG
                                      : strcmp(input->target, "Sheep") == 0
                                          ? MC_ENTITY_CLASS_KIND_SHEEP
                                          : MC_ENTITY_CLASS_KIND_COW;
    CHECK(f != NULL && prepare_kind(f, resources, input->profile, kind, false));
    f->victim->mob->living.Y = input->health;
    f->victim->base.by = input->hurt;
    f->victim->mob->living.au = input->last_damage;
    f->victim->mob->living.T = input->reward;
    f->base.player->base.velocity_y = input->falling ? -.1 : 0.;
    if (kind == MC_ENTITY_CLASS_KIND_PIG) {
        mc_entity_watch_record *value = record(f->victim, 16);
        CHECK(value != NULL);
        value->byte_value = input->saddled ? 1 : 0;
    }
    CHECK(mc_item_stack_allocate(&held) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(held, input->id, input->count, input->damage) ==
              MC_ITEM_STACK_OK &&
          mc_item_stack_source_animation_set(held, 5) == MC_ITEM_STACK_OK &&
          mc_inventory_player_set(f->base.player->local_player->fields.player.c, 0, held, NULL) ==
              MC_INVENTORY_OK);
    const mc_registry_item_array *array = NULL;
    const mc_item *item = NULL;
    mc_registry_statistic_view view;
    int32_t power;
    CHECK(mc_registry_item_c_get(f->base.app.registry, &array, NULL) == MC_REGISTRY_OK &&
          mc_registry_item_c_at(f->base.app.registry, array, input->id, &item, NULL) ==
              MC_REGISTRY_OK &&
          item != NULL &&
          mc_registry_classes_inspect_statistics(f->base.app.registry, &view, NULL) ==
              MC_REGISTRY_OK &&
          statistic_prepare(f, view.E[input->id], 11) &&
          mc_item_combat_damage(f->base.app.registry, item, f->victim, &power, NULL) ==
              MC_ENTITY_OK);
    if (view.F[input->id] != NULL)
        CHECK(statistic_prepare(f, view.F[input->id], 17));
    held_log log = {.world = f->base.world};
    const mc_world_access_callbacks callbacks = {
        .sound = held_sound, .particle = held_particle, .entity_added = held_added};
    mc_world_access *access = NULL;
    CHECK(mc_world_access_create(&callbacks, &log, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->base.world, access, NULL) == MC_WORLD_STORAGE_OK);
    const int before_saddle = saddled(f->victim);
    const uint64_t cow_before = f->victim->random->state;
    const uint64_t player_before = f->base.player->random->state;
    const uint64_t world_before = f->base.world->fields.r->state;
    const mc_entity_status status = mc_local_player_attack_begin(
        f->base.app.registry, f->math, f->base.player, f->victim, &f->call, NULL);
    mc_player_attack_state state;
    CHECK(mc_local_player_attack_inspect(f->call, &state));
    if (status != MC_ENTITY_OK) {
        fprintf(stderr, "Held attack %s status=%d at %s.%s:%u child=%s:%u: %s\n", input->profile,
                (int)status, state.error->site.class_name, state.error->site.method,
                state.error->site.pc, state.error->child.class_name, state.error->child.pc,
                state.error->operation);
        return false;
    }
    CHECK(state.returned && state.damage_returned && state.fresh_stack_read &&
          state.item_hit_returned && state.statistic_returned && log.item_count == f->added);
    if (input->cow)
        printf("HELD_ITEM_COW {\"profile\":\"%s\",\"selected\":", input->profile);
    else
        printf("HELD_ITEM ");
    CHECK(selected_row(input, f, held, item, power, &log, &view, before_saddle));
    if (input->cow) {
        printf(",\"cow-before\":\"%016" PRIx64 "\",\"cow-after\":\"%016" PRIx64
               "\",\"player-before\":\"%016" PRIx64 "\",\"player-after\":\"%016" PRIx64
               "\",\"world-before\":\"%016" PRIx64 "\",\"world-after\":\"%016" PRIx64
               "\",\"sounds\":[",
               cow_before, f->victim->random->state, player_before, f->base.player->random->state,
               world_before, f->base.world->fields.r->state);
        for (size_t i = 0; i < log.sounds; ++i)
            printf("%s{\"name\":\"%s\",\"volume\":\"%08" PRIx32 "\",\"pitch\":\"%08" PRIx32 "\"}",
                   i == 0 ? "" : ",", log.names[i], log.volume[i], log.pitch[i]);
        printf("]");
        CHECK(cow_items(f, &log));
    }
    printf("\n");
    /* Source PrintStream writes use the native handle. Complete this stdio
     * record before another real constructor emits its source messages. */
    CHECK(fflush(stdout) == 0);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->base.world, access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    mc_item_stack_release(held);
    CHECK(cleanup(f));
    free(f);
    return true;
}

static bool number(const char *text, int32_t *out) {
    errno = 0;
    char *end = NULL;
    const long long value = strtoll(text, &end, 10);
    if (errno != 0 || end == text || *end != 0 || value < INT32_MIN || value > INT32_MAX)
        return false;
    *out = (int32_t)value;
    return true;
}

static bool read_case(char *line, held_case *out) {
    char *fields[13] = {line};
    for (size_t i = 1; i < 13; ++i) {
        char *tab = strchr(fields[i - 1], '\t');
        if (tab == NULL)
            return false;
        *tab = 0;
        fields[i] = tab + 1;
    }
    if (strchr(fields[12], '\t') != NULL || strlen(fields[0]) >= sizeof(out->profile) ||
        strlen(fields[1]) >= sizeof(out->mode) || strlen(fields[5]) >= sizeof(out->target))
        return false;
    memcpy(out->profile, fields[0], strlen(fields[0]) + 1);
    memcpy(out->mode, fields[1], strlen(fields[1]) + 1);
    memcpy(out->target, fields[5], strlen(fields[5]) + 1);
    if (strcmp(out->target, "Cow") != 0 && strcmp(out->target, "Pig") != 0 &&
        strcmp(out->target, "Sheep") != 0)
        return false;
    int32_t falling, saddle, cow;
    if (!number(fields[2], &out->id) || !number(fields[3], &out->count) ||
        !number(fields[4], &out->damage) || !number(fields[6], &out->health) ||
        !number(fields[7], &out->hurt) || !number(fields[8], &out->last_damage) ||
        !number(fields[9], &out->reward) || !number(fields[10], &falling) ||
        !number(fields[11], &saddle) || !number(fields[12], &cow) || out->id < 0 ||
        out->id >= 32000 || (falling != 0 && falling != 1) || (saddle != 0 && saddle != 1) ||
        (cow != 0 && cow != 1))
        return false;
    out->falling = falling != 0;
    out->saddled = saddle != 0;
    out->cow = cow != 0;
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    FILE *cases = fopen(argv[3], "rb");
    if (cases == NULL)
        return 2;
    char line[1024];
    size_t count = 0;
    while (fgets(line, sizeof(line), cases) != NULL) {
        char *newline = strchr(line, '\n');
        if (newline == NULL || ++count > 1000) {
            (void)fclose(cases);
            return 2;
        }
        *newline = 0;
        held_case input = {0};
        if (!read_case(line, &input) || !run_case(argv[1], &input)) {
            (void)fclose(cases);
            return 1;
        }
    }
    const bool complete = !ferror(cases) && count == 425;
    if (fclose(cases) != 0 || !complete)
        return 2;
    printf("PASS: %zu Player held Item attack checks; %zu genuine calls\n", checks, count);
    return 0;
}
