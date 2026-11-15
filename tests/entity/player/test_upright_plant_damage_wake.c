/* Actual retained Player damage with Cactus and Reeds neighbor recursion.
 * Imported fixture entry points are never run. */
int up_unused_plant_damage_wake_main(int, char **);
#define MC_PLANT_DAMAGE_WAKE_ENTRY up_unused_plant_damage_wake_main
#define PW_EVENT_CAPACITY 8
#define MC_SLEEP_WAKE_PRELOAD_NEIGHBORS 0
#include "test_plant_damage_wake.c"
#undef MC_PLANT_DAMAGE_WAKE_ENTRY
#include "mc/world/runtime/scheduling.h"

typedef struct up_profile {
    pw_profile plant;
    int32_t columns, water_direction, water_id, removal_error_y;
    bool range_error;
} up_profile;

#define UP_PROFILE(name, id, floor, columns, direction, water, added, removed, missing)            \
    {                                                                                              \
        {name, id, 0, floor, 15, INT64_C(1), added, removed != 0, missing, false}, columns,        \
            direction, water, removed, false                                                       \
    }
static const up_profile up_profiles[] = {
    UP_PROFILE("cactus-bed", 81, 12, 1, -1, 0, 0, 0, false),
    UP_PROFILE("cactus-publication-error", 81, 12, 1, -1, 0, 1, 0, false),
    UP_PROFILE("cactus-removal-error", 81, 12, 1, -1, 0, 0, 64, false),
    UP_PROFILE("cactus-column3", 81, 12, 3, -1, 0, 0, 0, false),
    UP_PROFILE("cactus-column3-publication-error-second", 81, 12, 3, -1, 0, 2, 0, false),
    UP_PROFILE("cactus-column3-removal-error-middle", 81, 12, 3, -1, 0, 0, 65, false),
    UP_PROFILE("reeds-sand", 83, 12, 1, -1, 0, 0, 0, false),
    UP_PROFILE("reeds-air", 83, 0, 1, -1, 0, 0, 0, false),
    UP_PROFILE("reeds-dry-grass", 83, 2, 1, -1, 0, 0, 0, false),
    UP_PROFILE("reeds-west-water", 83, 2, 1, 0, 9, 0, 0, false),
    UP_PROFILE("reeds-east-water", 83, 2, 1, 1, 9, 0, 0, false),
    UP_PROFILE("reeds-north-water", 83, 2, 1, 2, 9, 0, 0, false),
    UP_PROFILE("reeds-south-water", 83, 2, 1, 3, 9, 0, 0, false),
    UP_PROFILE("reeds-flowing-water", 83, 3, 1, 0, 8, 0, 0, false),
    UP_PROFILE("reeds-stacked", 83, 83, 1, -1, 0, 0, 0, false),
    UP_PROFILE("reeds-publication-error", 83, 2, 1, -1, 0, 1, 0, false),
    UP_PROFILE("reeds-removal-error", 83, 2, 1, -1, 0, 0, 64, false),
    UP_PROFILE("reeds-column3", 83, 2, 3, -1, 0, 0, 0, false),
    UP_PROFILE("reeds-column3-publication-error-second", 83, 2, 3, -1, 0, 2, 0, false),
    UP_PROFILE("reeds-column3-removal-error-middle", 83, 2, 3, -1, 0, 0, 65, false),
    {.plant = {.name = "reeds-east-water-range-error",
               .id = 83,
               .floor = 2,
               .light = 15,
               .seed = INT64_C(1)},
     .columns = 1,
     .water_direction = 1,
     .water_id = 9,
     .range_error = true},
    UP_PROFILE("cactus-bed-math-missing", 81, 12, 1, -1, 0, 0, 0, true),
    UP_PROFILE("reeds-dry-grass-math-missing", 83, 2, 1, -1, 0, 0, 0, true),
    UP_PROFILE("reeds-west-water-math-missing", 83, 2, 1, 0, 9, 0, 0, true),
    UP_PROFILE("reeds-column3-math-missing", 83, 2, 3, -1, 0, 0, 0, true),
};
#undef UP_PROFILE

typedef struct up_observation {
    pw_observation base;
    const up_profile *profile;
    int32_t columns[PW_EVENT_CAPACITY][3][2];
    int32_t range_max[PW_EVENT_CAPACITY][3];
    int32_t water_cells[PW_EVENT_CAPACITY][5];
    bool ranges[PW_EVENT_CAPACITY], world_o[PW_EVENT_CAPACITY];
} up_observation;

static bool up_column(const mc_chunk_data *data, int32_t out[3][2]) {
    for (int32_t i = 0; i < 3; ++i)
        if (!pw_cell_read(data, 7, 64 + i, 8, out[i]))
            return false;
    return true;
}

static bool up_begin(up_observation *o, pw_event *event) {
    if (!pw_event_begin(&o->base, event) ||
        !up_column(o->base.fixture->world->states[0]->data, o->columns[o->base.event_count]))
        return false;
    o->world_o[o->base.event_count] = o->base.fixture->world->worlds[0]->fields.o;
    if (o->profile->water_direction >= 0) {
        const int32_t directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        int32_t *water = o->water_cells[o->base.event_count];
        water[0] = 7 + directions[o->profile->water_direction][0];
        water[1] = 63;
        water[2] = 8 + directions[o->profile->water_direction][1];
        if (!pw_cell_read(o->base.fixture->world->states[0]->data, water[0], water[1], water[2],
                          water + 3))
            return false;
    }
    return true;
}

static mc_world_storage_status up_range(void *context, mc_world_runtime *world, int32_t min_x,
                                        int32_t min_y, int32_t min_z, int32_t max_x, int32_t max_y,
                                        int32_t max_z, mc_world_storage_error *error,
                                        mc_world_access_failure *failure) {
    up_observation *o = context;
    pw_event event = {.xyz = {min_x, min_y, min_z}};
    o->base.valid &= up_begin(o, &event) && world == o->base.fixture->world->worlds[0];
    if (o->base.event_count < COUNT(o->base.events)) {
        const size_t index = o->base.event_count++;
        o->base.events[index] = event;
        o->ranges[index] = true;
        o->range_max[index][0] = max_x;
        o->range_max[index][1] = max_y;
        o->range_max[index][2] = max_z;
    }
    *error = (mc_world_storage_error){0};
    if (!o->base.valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    return o->profile->range_error ? pw_throw(&o->base, error, failure, 67) : MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status up_cell(void *context, mc_world_runtime *world, int32_t x, int32_t y,
                                       int32_t z, mc_world_storage_error *error,
                                       mc_world_access_failure *failure) {
    up_observation *o = context;
    pw_observation *base = &o->base;
    pw_event event = {.xyz = {x, y, z}};
    const bool first = base->cells == 0;
    base->valid &= up_begin(o, &event) && world == base->fixture->world->worlds[0] &&
                   x == (first ? 8 : 7) && y == (first ? 64 : 63 + (int32_t)base->cells) &&
                   z == 8 && (first || base->cells <= (size_t)o->profile->columns);
    if (base->event_count < COUNT(base->events))
        base->events[base->event_count++] = event;
    ++base->cells;
    *error = (mc_world_storage_error){0};
    if (!base->valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    return !first && y == o->profile->removal_error_y ? pw_throw(base, error, failure, 37)
                                                      : MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status up_added(void *context, mc_world_runtime *world, mc_entity *entity,
                                        mc_world_storage_error *error,
                                        mc_world_access_failure *failure) {
    up_observation *o = context;
    pw_observation *base = &o->base;
    pw_event event = {.added = true};
    mc_entity_tick_execution_world_token token;
    base->valid &= up_begin(o, &event) && world == base->fixture->world->worlds[0] &&
                   base->count < (size_t)o->profile->columns && base->count < COUNT(base->items) &&
                   pw_item_read(entity, world, &event.item) && event.item.count == 1 &&
                   event.item.id == (o->profile->plant.id == 81 ? 81 : 338) &&
                   event.item.damage == 0 && event.item.delay == 10 && event.item.inserted &&
                   event.item.world_same &&
                   world->fields.b->count == base->base_entities + base->count + 1 &&
                   world->fields.b->items[world->fields.b->count - 1] == entity &&
                   mc_entity_tick_execution_current_world(base->execution, &token) &&
                   base->execution->leases[token.slot].phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED;
    if (base->count < COUNT(base->items))
        base->items[base->count++] = entity;
    if (base->event_count < COUNT(base->events))
        base->events[base->event_count++] = event;
    *error = (mc_world_storage_error){0};
    if (!base->valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    return base->profile->publication_error_at == base->count ? pw_throw(base, error, failure, 57)
                                                              : MC_WORLD_STORAGE_OK;
}

static void up_print_column(const void *source) {
    int32_t column[6];
    memcpy(column, source, sizeof(column));
    printf("[[%d,%d],[%d,%d],[%d,%d]]", column[0], column[1], column[2], column[3], column[4],
           column[5]);
}

static void up_print_water(const up_observation *o, const int32_t *water) {
    if (o->profile->water_direction < 0)
        printf("null");
    else
        printf("[%d,%d,%d,%d,%d]", water[0], water[1], water[2], water[3], water[4]);
}

static bool up_print_scheduled(const mc_scheduled_tick **entries, size_t written) {
    putchar('[');
    for (size_t i = 0; i < written; ++i) {
        mc_scheduled_tick_fields fields;
        if (mc_scheduled_tick_fields_get(entries[i], &fields) != MC_SCHEDULED_TICK_OK)
            return false;
        printf("%s{\"xyz\":[%" PRId32 ",%" PRId32 ",%" PRId32 "],\"block\":%" PRId32
               ",\"time\":\"%016" PRIx64 "\",\"ordinal\":\"%016" PRIx64 "\"}",
               i == 0 ? "" : ",", fields.x, fields.y, fields.z, fields.block,
               (uint64_t)fields.due_time, (uint64_t)fields.sequence);
    }
    putchar(']');
    return true;
}

static bool up_print_runtime(const up_observation *o, const char *phase) {
    mc_world_runtime *world = o->base.fixture->world->worlds[0];
    mc_world_schedule_class_state sequence;
    const mc_scheduled_tick *entries[16];
    size_t written;
    int32_t water[5] = {0};
    if (o->profile->water_direction >= 0) {
        const int32_t directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
        water[0] = 7 + directions[o->profile->water_direction][0];
        water[1] = 63;
        water[2] = 8 + directions[o->profile->water_direction][1];
        if (!pw_cell_read(o->base.fixture->world->states[0]->data, water[0], water[1], water[2],
                          water + 3))
            return false;
    }
    if (mc_world_runtime_schedule_class_get(world->classes, &sequence) != MC_WORLD_STORAGE_OK)
        return false;
    printf("UPRIGHT_WAKE_RUNTIME {\"phase\":\"%s\",\"world_o\":%s,\"water_cell\":", phase,
           sw_bool(world->fields.o));
    up_print_water(o, water);
    printf(",\"scheduled\":{\"initialized\":%s,\"sequence\":\"%016" PRIx64 "\",\"ordered\":",
           sw_bool(sequence.initialized), (uint64_t)sequence.next);
    if (mc_scheduled_ordered_snapshot(world->fields.E, entries, COUNT(entries), &written) !=
            MC_SCHEDULED_ORDERED_OK ||
        !up_print_scheduled(entries, written))
        return false;
    printf(",\"members\":");
    if (mc_scheduled_membership_snapshot(world->fields.F, entries, COUNT(entries), &written) !=
            MC_SCHEDULED_MEMBERSHIP_OK ||
        !up_print_scheduled(entries, written))
        return false;
    puts("}}");
    return true;
}

static bool up_print_state(const up_observation *o, const pw_state *state, const char *phase,
                           const int32_t *expected) {
    int32_t column[3][2];
    if (!up_column(o->base.fixture->world->states[0]->data, column) ||
        memcmp(column, expected, sizeof(column)) != 0)
        return false;
    pw_print_state(&o->base, state, phase);
    printf("UPRIGHT_WAKE_COLUMN {\"phase\":\"%s\",\"column\":", phase);
    up_print_column(&column[0][0]);
    puts("}");
    return up_print_runtime(o, phase);
}

static void up_print_events(const up_observation *o) {
    printf("PLANT_WAKE_EVENTS [");
    for (size_t i = 0; i < o->base.event_count; ++i) {
        const pw_event *event = &o->base.events[i];
        printf("%s{\"kind\":\"%s\",\"head\":[%d,%d],\"plant\":[%d,%d],\"sleeping\":%s,"
               "\"world_seed\":\"%016" PRIx64 "\",\"column\":",
               i == 0 ? "" : ",",
               o->ranges[i]   ? "range"
               : event->added ? "added"
                              : "cell",
               event->head[0], event->head[1], event->plant[0], event->plant[1],
               sw_bool(event->sleeping), event->random);
        up_print_column(&o->columns[i][0][0]);
        printf(",\"world_o\":%s,\"water_cell\":", sw_bool(o->world_o[i]));
        up_print_water(o, o->water_cells[i]);
        if (event->added) {
            printf(",\"item\":");
            pw_print_item(&event->item);
        } else if (o->ranges[i])
            printf(",\"xyz\":[%d,%d,%d,%d,%d,%d]", event->xyz[0], event->xyz[1], event->xyz[2],
                   o->range_max[i][0], o->range_max[i][1], o->range_max[i][2]);
        else
            printf(",\"xyz\":[%d,%d,%d]", event->xyz[0], event->xyz[1], event->xyz[2]);
        putchar('}');
    }
    puts("]");
}

static bool up_run(const char *root, const up_profile *profile, int64_t scheduled_before,
                   bool scheduled_initialized) {
    bool ok = false, contexts = false, entered = false, registered = false, retired = false;
    sw_fixture source = {0};
    mc_entity_effect_error *error = calloc(1, sizeof(*error));
    mc_entity_damage_execution *damage = calloc(1, sizeof(*damage));
    mc_entity_tick_execution *execution = calloc(1, sizeof(*execution));
    mc_math_execution math = {0};
    mc_math_classes *math_owner = NULL;
    mc_world_access *listener = NULL;
    uint64_t occurrence = 0;
    size_t receipt_holds = 0;
    bool saturated = false;
    up_observation observation = {.base = {.fixture = &source,
                                           .profile = &profile->plant,
                                           .execution = execution,
                                           .valid = true},
                                  .profile = profile};
    const int32_t input[5] = {0, 1, 0, 0, 0};
    CHECK(error != NULL && damage != NULL && execution != NULL && sw_prepare(&source, root, input));
    fixture *f = source.world;
    CHECK(mc_world_store_chunk_count(f->stores[0]) == 1 && f->providers[0].cache != NULL &&
          f->providers[0].cache->count == 1 && f->providers[0].cache->insertion_count == 1 &&
          f->providers[0].cache->insertions[0] == f->states[0]);
    mc_world_runtime *world = f->worlds[0];
    mc_entity *player = source.actor;
    mc_chunk_data *data = f->states[0]->data;
    CHECK(sw_cell(data, 7, 63, 8, profile->plant.floor, 0));
    for (int32_t i = 0; i < profile->columns; ++i)
        CHECK(sw_cell(data, 7, 64 + i, 8, profile->plant.id, 0));
    const int32_t directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    if (profile->water_direction >= 0)
        CHECK(sw_cell(data, 7 + directions[profile->water_direction][0], 63,
                      8 + directions[profile->water_direction][1], profile->water_id, 0));
    mc_java_random_reseed(world->fields.r, 1);
    double discarded;
    CHECK(mc_java_math_random(f->profile, &discarded) == MC_JAVA_RANDOM_PROFILE_OK &&
          f->profile->math != NULL);
    mc_java_random seeded;
    mc_java_random_seed(&seeded, 0);
    atomic_store(&f->profile->math->state, seeded.state);
    f->profile->math->have_next_next_gaussian = false;
    f->profile->math->next_next_gaussian = 0;
    /* Prepared source-class input, captured passively in the original VM.
     * Keep this same actual class owner throughout the native damage call. */
    f->classes[0]->scheduled_initialized = scheduled_initialized;
    f->classes[0]->scheduled_sequence.next = scheduled_before;
    observation.base.base_entities = world->fields.b->count;
    const mc_world_access_source_callbacks callbacks = {
        .cell = up_cell, .range = up_range, .entity_added = up_added};
    CHECK(mc_world_access_source_create(&callbacks, &observation, &listener, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_access_add_receipted(world, listener, &occurrence, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    observation.base.listener = listener;
    if (!profile->plant.missing_math)
        CHECK(mc_math_classes_create(SIZE_MAX, &math_owner, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&math, math_owner);
    mc_entity_tick_execution_init(execution, &math);
    contexts = true;
    const mc_world_runtime_execution_code code = {{"lm", "e", "()V", 6}, {"lm", "e", "()V", 33}};
    mc_entity_damage_execution_init(damage, execution, f->classes[0], code, f->registry[0]);
    player->local_player->fields.living.av = 99;
    CHECK(up_print_runtime(&observation, "before"));
    bool accepted = true;
    mc_entity_status raw = MC_ENTITY_BUSY;
    const bool continued =
        mc_entity_effect_damage_in_execution(damage, player, NULL, 0, &accepted, error, &raw);
    entered = true;
    const bool supported =
        profile->plant.id == 83 &&
        (profile->plant.floor == 83 || profile->water_id == 8 || profile->water_id == 9);
    const bool math_failed = !supported && profile->plant.missing_math;
    const bool source_error = profile->plant.publication_error_at != 0 ||
                              profile->removal_error_y != 0 || profile->range_error;
    const bool normal = !source_error && !math_failed;
    pw_state state;
    int32_t column[3][2];
    CHECK(pw_state_read(&observation.base, &state) && up_column(data, column) &&
          observation.base.valid && continued == normal && error->player_wake.entered &&
          error->player_bytecode == 35 && (continued ? !accepted : accepted) &&
          math.failed == math_failed && state.sleeping == !normal &&
          state.world_sleeping == !normal && state.timer == (normal ? 0 : 37) &&
          state.health == 20 && state.idle == 0 &&
          state.world_entities == observation.base.base_entities + observation.base.count);
    const char *exception = "NONE";
    if (source_error) {
        mc_entity_damage_failure failure = {0};
        CHECK(raw != MC_ENTITY_OK && mc_entity_damage_execution_source_failure(damage, &failure) &&
              failure.source_proven && failure.kind == MC_ENTITY_DAMAGE_FAILURE_SOURCE_EXCEPTION &&
              failure.exception_class != NULL &&
              strcmp(failure.exception_class, "java/lang/IllegalStateException") == 0 &&
              observation.base.receipt != NULL &&
              mc_world_access_receipt_retire(&observation.base.receipt, NULL) ==
                  MC_WORLD_STORAGE_BUSY);
        exception = failure.exception_class;
    } else
        CHECK(raw == MC_ENTITY_OK);
    printf(
        "PLANT_WAKE_OUTCOME {\"mode\":\"%s\",\"input\":{\"id\":%d,\"metadata\":0,\"floor\":%d,"
        "\"light\":15,\"world_seed\":1,\"columns\":%d,\"water_direction\":%d,\"water_id\":%d,"
        "\"publication_error_at\":%zu,\"removal_error_y\":%d,\"range_error\":%s,\"math_owned\":%s,"
        "\"scheduled_before\":{\"sequence\":\"%016" PRIx64
        "\",\"initialized\":%s}},\"continued\":%s,"
        "\"raw\":%d,\"pc\":%u,\"wake_entered\":%s,\"result\":%s,\"exception\":\"%s\",\"math_"
        "failed\":%s}\n",
        profile->plant.name, profile->plant.id, profile->plant.floor, profile->columns,
        profile->water_direction, profile->water_id, profile->plant.publication_error_at,
        profile->removal_error_y, sw_bool(profile->range_error),
        sw_bool(!profile->plant.missing_math), (uint64_t)scheduled_before,
        sw_bool(scheduled_initialized), sw_bool(continued), (int)raw,
        (unsigned int)error->player_bytecode, sw_bool(error->player_wake.entered),
        continued ? sw_bool(accepted) : "null", exception, sw_bool(math.failed));
    CHECK(up_print_state(&observation, &state, normal ? "returned" : "diagnostic", &column[0][0]));
    up_print_events(&observation);
    if (observation.base.receipt != NULL) {
        receipt_holds = observation.base.receipt->holds;
        observation.base.receipt->holds = SIZE_MAX;
        saturated = true;
    }
    CHECK(mc_entity_damage_execution_native_close(damage) == MC_WORLD_STORAGE_OK);
    pw_state after;
    CHECK(pw_state_read(&observation.base, &after) && pw_state_equal(&state, &after) &&
          up_print_state(&observation, &after, "native-close", &column[0][0]));
    CHECK(mc_entity_damage_execution_cleanup(damage, error) == MC_ENTITY_OK && damage->cleaned &&
          !mc_entity_effect_has_pending(player) &&
          (!saturated || observation.base.receipt->holds == SIZE_MAX));
    if (saturated) {
        observation.base.receipt->holds = receipt_holds;
        saturated = false;
    }
    CHECK(pw_state_read(&observation.base, &after) && pw_state_equal(&state, &after) &&
          up_print_state(&observation, &after, "cleanup", &column[0][0]));
    CHECK(mc_entity_tick_execution_retire(execution) == MC_WORLD_STORAGE_OK &&
          mc_math_execution_retire(&math) == MC_MATH_CLASS_OK &&
          mc_math_classes_release(&math_owner, NULL) == MC_MATH_CLASS_OK);
    retired = true;
    CHECK(pw_state_read(&observation.base, &after) && pw_state_equal(&state, &after) &&
          up_print_state(&observation, &after, "retired", &column[0][0]));
    ok = true;
cleanup:
    if (saturated)
        observation.base.receipt->holds = receipt_holds;
    if (entered && !damage->cleaned &&
        mc_entity_damage_execution_cleanup(damage, error) != MC_ENTITY_OK)
        return false;
    if (contexts && !retired &&
        (mc_entity_tick_execution_retire(execution) != MC_WORLD_STORAGE_OK ||
         mc_math_execution_retire(&math) != MC_MATH_CLASS_OK ||
         mc_math_classes_release(&math_owner, NULL) != MC_MATH_CLASS_OK))
        return false;
    if (observation.base.receipt != NULL &&
        mc_world_access_receipt_retire(&observation.base.receipt, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (registered && mc_world_access_detach_occurrence(source.world->worlds[0], listener,
                                                        occurrence, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (listener != NULL && mc_world_access_free(listener, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    if (source.world != NULL && !sw_retire(&source))
        return false;
    free(error);
    free(damage);
    free(execution);
    return ok;
}

int main(int argc, char **argv) {
    char root[8192];
    uint64_t sequence = 0;
    bool initialized = false;
    if ((argc != 3 && argc != 5) || !decode_path(argv[2], root, sizeof(root)) ||
        !mc_math_trig_init(&trig))
        return 2;
    if (argc == 5) {
        if (strlen(argv[3]) != 16 || strlen(argv[4]) != 1 ||
            (argv[4][0] != '0' && argv[4][0] != '1'))
            return 2;
        for (size_t i = 0; i < 16; ++i) {
            const char value = argv[3][i];
            const uint64_t digit = value >= '0' && value <= '9'   ? (uint64_t)(value - '0')
                                   : value >= 'a' && value <= 'f' ? (uint64_t)(value - 'a' + 10)
                                                                  : UINT64_MAX;
            if (digit > 15)
                return 2;
            sequence = sequence * UINT64_C(16) + digit;
        }
        initialized = argv[4][0] == '1';
    }
    const int64_t before = sequence <= (uint64_t)INT64_MAX
                               ? (int64_t)sequence
                               : -(int64_t)(UINT64_MAX - sequence) - INT64_C(1);
    for (size_t i = 0; i < COUNT(up_profiles); ++i)
        if (strcmp(argv[1], up_profiles[i].plant.name) == 0) {
            if (!up_run(root, &up_profiles[i], before, initialized))
                return 1;
            return printf("PASS: %u retained upright plant wake checks;mode=%s\n", checks,
                          argv[1]) < 0;
        }
    return 2;
}
