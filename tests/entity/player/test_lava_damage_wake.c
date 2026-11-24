/* Genuine retained Player damage and Lava neighbor/fizz observations.
 * Imported authored fixture entry points are never executed. */
int lv_unused_plant_damage_wake_main(int, char **);
#define MC_PLANT_DAMAGE_WAKE_ENTRY lv_unused_plant_damage_wake_main
#define MC_SLEEP_WAKE_PRELOAD_NEIGHBORS 0
#define PW_EVENT_CAPACITY 64
#include "test_plant_damage_wake.c"
#undef MC_PLANT_DAMAGE_WAKE_ENTRY
#include "mc/world/runtime/scheduling.h"
#include <emmintrin.h>

typedef struct lv_profile {
    const char *mode;
    int32_t id, metadata, xyz[3], water_direction, water_id;
    int64_t seed, scheduled_before;
    int32_t error_at[4]; /* Cell, Sound, Particle, Range callback ordinals. */
    bool scheduled_initialized;
    int control; /* 0 ordinary, 1 host FP, 2 absent MathHelper, 3/4 callback descendants. */
} lv_profile;

typedef struct lv_event {
    int kind;
    int32_t xyz[6], head[2], lava[2], water[5];
    bool sleeping, world_o;
    uint64_t position[3], motion[3], random[2], gaussian[2];
    bool cache[2];
    uint32_t volume, pitch;
    char name[64];
} lv_event;

typedef struct lv_observation {
    pw_observation base;
    const lv_profile *profile;
    lv_event events[64];
    size_t ordinals[4];
    mc_entity_tick_execution_world_token presentation_parent, descendant;
    bool descendant_open;
} lv_observation;

static bool lv_water(const lv_observation *o, int32_t out[5]) {
    if (o->profile->water_direction < 0)
        return true;
    const int32_t offsets[5][3] = {{-1, 0, 0}, {1, 0, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}};
    for (size_t i = 0; i < 3; ++i)
        out[i] = o->profile->xyz[i] + offsets[o->profile->water_direction][i];
    return pw_cell_read(o->base.fixture->world->states[0]->data, out[0], out[1], out[2], out + 3);
}

static bool lv_name(const mc_registry_string *name, char out[64]) {
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

static bool lv_begin(lv_observation *o, mc_world_runtime *world, int kind, lv_event *event) {
    if (o->base.event_count >= COUNT(o->events) || world != o->base.fixture->world->worlds[0])
        return false;
    const mc_entity *player = o->base.fixture->actor;
    const mc_player_fields *fields = &player->local_player->fields.player;
    const fixture *f = o->base.fixture->world;
    event->kind = kind;
    event->sleeping = fields->u;
    event->world_o = world->fields.o;
    event->random[0] = world->fields.r->state;
    event->cache[0] = world->fields.r->have_next_next_gaussian;
    event->gaussian[0] = movement_double_bits(world->fields.r->next_next_gaussian);
    event->random[1] = atomic_load(&f->profile->math->state);
    event->cache[1] = f->profile->math->have_next_next_gaussian;
    event->gaussian[1] = movement_double_bits(f->profile->math->next_next_gaussian);
    return pw_cell_read(f->states[0]->data, 8, 64, 8, event->head) &&
           pw_cell_read(f->states[0]->data, o->profile->xyz[0], o->profile->xyz[1],
                        o->profile->xyz[2], event->lava) &&
           lv_water(o, event->water) && event->head[0] == 26 && event->head[1] == 8 &&
           event->sleeping && fields->private_a == 37 && world->fields.J;
}

static mc_world_storage_status lv_record(lv_observation *o, const lv_event *event,
                                         mc_world_storage_error *error,
                                         mc_world_access_failure *failure) {
    *error = (mc_world_storage_error){0};
    if (!o->base.valid || o->base.event_count >= COUNT(o->events))
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    o->events[o->base.event_count++] = *event;
    const size_t ordinal = ++o->ordinals[event->kind];
    return ordinal == (size_t)o->profile->error_at[event->kind]
               ? pw_throw(&o->base, error, failure, (uint32_t)(71 + event->kind))
               : MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status lv_cell(void *context, mc_world_runtime *world, int32_t x, int32_t y,
                                       int32_t z, mc_world_storage_error *error,
                                       mc_world_access_failure *failure) {
    lv_observation *o = context;
    lv_event event = {.xyz = {x, y, z}};
    o->base.valid &= lv_begin(o, world, 0, &event);
    ++o->base.cells;
    return lv_record(o, &event, error, failure);
}

static mc_world_storage_status lv_range(void *context, mc_world_runtime *world, int32_t x0,
                                        int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1,
                                        mc_world_storage_error *error,
                                        mc_world_access_failure *failure) {
    lv_observation *o = context;
    lv_event event = {.xyz = {x0, y0, z0, x1, y1, z1}};
    o->base.valid &= lv_begin(o, world, 3, &event);
    return lv_record(o, &event, error, failure);
}

static mc_world_storage_status lv_sound(void *context, mc_world_runtime *world,
                                        const mc_registry_string *name, double x, double y,
                                        double z, float volume, float pitch,
                                        mc_world_storage_error *error,
                                        mc_world_access_failure *failure) {
    lv_observation *o = context;
    lv_event event = {
        .position = {movement_double_bits(x), movement_double_bits(y), movement_double_bits(z)},
        .volume = movement_float_bits(volume),
        .pitch = movement_float_bits(pitch)};
    o->base.valid &= lv_begin(o, world, 1, &event) && lv_name(name, event.name);
    const mc_world_storage_status status = lv_record(o, &event, error, failure);
    if (status != MC_WORLD_STORAGE_OK || o->profile->control != 3 || o->ordinals[1] != 1)
        return status;
    const mc_entity_tick_execution_site site = {"native.lava", "soundDescendant", "()V", 0};
    o->base.valid &=
        mc_entity_tick_execution_current_world(o->base.execution, &o->presentation_parent);
    if (!o->base.valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    const mc_world_storage_status pushed =
        mc_entity_tick_execution_push_world(o->base.execution, world, site, &o->descendant);
    o->descendant_open = pushed == MC_WORLD_STORAGE_OK;
    return pushed;
}

static mc_world_storage_status lv_particle(void *context, mc_world_runtime *world,
                                           const mc_registry_string *name, double x, double y,
                                           double z, double vx, double vy, double vz,
                                           mc_world_storage_error *error,
                                           mc_world_access_failure *failure) {
    lv_observation *o = context;
    lv_event event = {
        .position = {movement_double_bits(x), movement_double_bits(y), movement_double_bits(z)},
        .motion = {movement_double_bits(vx), movement_double_bits(vy), movement_double_bits(vz)}};
    o->base.valid &= lv_begin(o, world, 2, &event) && lv_name(name, event.name);
    const mc_world_storage_status status = lv_record(o, &event, error, failure);
    if (status != MC_WORLD_STORAGE_OK || o->profile->control != 4 || o->ordinals[2] != 4)
        return status;
    const mc_entity_tick_execution_site site = {"native.lava", "particleDescendant", "()V", 0};
    o->base.valid &=
        mc_entity_tick_execution_current_world(o->base.execution, &o->presentation_parent);
    if (!o->base.valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    const mc_world_storage_status pushed =
        mc_entity_tick_execution_push_world(o->base.execution, world, site, &o->descendant);
    o->descendant_open = pushed == MC_WORLD_STORAGE_OK;
    return pushed;
}

static void lv_print_water(const lv_observation *o, const int32_t *water) {
    if (o->profile->water_direction < 0)
        printf("null");
    else
        printf("[%d,%d,%d,%d,%d]", water[0], water[1], water[2], water[3], water[4]);
}

static bool lv_print_queue(const mc_scheduled_tick **entries, size_t count) {
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
    putchar(']');
    return true;
}

static bool lv_print_runtime(const lv_observation *o, const char *phase) {
    const fixture *f = o->base.fixture->world;
    mc_world_runtime *world = f->worlds[0];
    int32_t lava[2], water[5] = {0};
    mc_world_schedule_class_state sequence;
    const mc_scheduled_tick *entries[16];
    size_t count;
    if (!pw_cell_read(f->states[0]->data, o->profile->xyz[0], o->profile->xyz[1],
                      o->profile->xyz[2], lava) ||
        !lv_water(o, water) ||
        mc_world_runtime_schedule_class_get(world->classes, &sequence) != MC_WORLD_STORAGE_OK)
        return false;
    printf("LAVA_WAKE_RUNTIME {\"phase\":\"%s\",\"lava\":[%d,%d],\"world_o\":%s,\"water_cell\":",
           phase, lava[0], lava[1], sw_bool(world->fields.o));
    lv_print_water(o, water);
    printf(",\"scheduled\":{\"initialized\":%s,\"sequence\":\"%016" PRIx64 "\",\"ordered\":",
           sw_bool(sequence.initialized), (uint64_t)sequence.next);
    if (mc_scheduled_ordered_snapshot(world->fields.E, entries, COUNT(entries), &count) !=
            MC_SCHEDULED_ORDERED_OK ||
        !lv_print_queue(entries, count))
        return false;
    printf(",\"members\":");
    if (mc_scheduled_membership_snapshot(world->fields.F, entries, COUNT(entries), &count) !=
            MC_SCHEDULED_MEMBERSHIP_OK ||
        !lv_print_queue(entries, count))
        return false;
    printf("},\"sky_seen\":%s,\"lighting\":{\"depth\":%" PRId32 ",\"regions\":[",
           sw_bool(f->classes[0]->chunk_a), f->classes[0]->enqueue_depth);
    mc_world_light_list_view lighting;
    if (mc_world_runtime_light_list_get(world, &lighting, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    for (size_t i = 0; i < lighting.count; ++i) {
        const mc_world_light_region *region = NULL;
        if (mc_world_runtime_light_at(world, i, &region, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        const mc_world_light_region_fields *fields =
            mc_world_runtime_light_region_fields_get(region);
        const mc_world_light_type_fields *type =
            fields == NULL ? NULL : mc_world_runtime_light_type_fields_get(fields->a);
        if (fields == NULL || type == NULL ||
            (type->kind != MC_LIGHT_SKY && type->kind != MC_LIGHT_BLOCK))
            return false;
        printf("%s{\"kind\":\"%s\",\"bounds\":[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32
               ",%" PRId32 ",%" PRId32 "]}",
               i == 0 ? "" : ",", type->kind == MC_LIGHT_SKY ? "Sky" : "Block",
               fields->bounds.min_x, fields->bounds.min_y, fields->bounds.min_z,
               fields->bounds.max_x, fields->bounds.max_y, fields->bounds.max_z);
    }
    puts("]}}");
    return true;
}

static bool lv_print_state(const lv_observation *o, const pw_state *state, const char *phase) {
    pw_print_state(&o->base, state, phase);
    return lv_print_runtime(o, phase);
}

static void lv_print_random(uint64_t state, bool cache, uint64_t gaussian) {
    printf("[\"%016" PRIx64 "\",%s,\"%016" PRIx64 "\"]", state, sw_bool(cache), gaussian);
}

static void lv_print_events(const lv_observation *o) {
    const char *names[4] = {"cell", "sound", "particle", "range"};
    printf("LAVA_WAKE_EVENTS [");
    for (size_t i = 0; i < o->base.event_count; ++i) {
        const lv_event *event = &o->events[i];
        printf("%s{\"kind\":\"%s\",\"head\":[%d,%d],\"lava\":[%d,%d],\"sleeping\":%s,\"world_o\":%"
               "s,\"water_cell\":",
               i == 0 ? "" : ",", names[event->kind], event->head[0], event->head[1],
               event->lava[0], event->lava[1], sw_bool(event->sleeping), sw_bool(event->world_o));
        lv_print_water(o, event->water);
        printf(",\"world_random\":");
        lv_print_random(event->random[0], event->cache[0], event->gaussian[0]);
        printf(",\"math_random\":");
        lv_print_random(event->random[1], event->cache[1], event->gaussian[1]);
        if (event->kind == 0 || event->kind == 3) {
            printf(",\"xyz\":[%d,%d,%d", event->xyz[0], event->xyz[1], event->xyz[2]);
            if (event->kind == 3)
                printf(",%d,%d,%d", event->xyz[3], event->xyz[4], event->xyz[5]);
            putchar(']');
        } else {
            printf(",\"name\":\"%s\",\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64
                   "\",\"%016" PRIx64 "\"]",
                   event->name, event->position[0], event->position[1], event->position[2]);
            if (event->kind == 1)
                printf(",\"volume\":\"%08" PRIx32 "\",\"pitch\":\"%08" PRIx32 "\"", event->volume,
                       event->pitch);
            else
                printf(",\"motion\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]",
                       event->motion[0], event->motion[1], event->motion[2]);
        }
        putchar('}');
    }
    puts("]");
}

static bool lv_run(const char *root, const lv_profile *profile) {
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
    const pw_profile common = {.name = profile->mode,
                               .id = profile->id,
                               .metadata = profile->metadata,
                               .floor = 2,
                               .light = 15,
                               .seed = profile->seed};
    lv_observation observation = {
        .base = {.fixture = &source, .profile = &common, .execution = execution, .valid = true},
        .profile = profile};
    const int32_t input[5] = {0, 1, 0, 0, 0};
    CHECK(error != NULL && damage != NULL && execution != NULL && sw_prepare(&source, root, input));
    fixture *f = source.world;
    mc_world_runtime *world = f->worlds[0];
    mc_entity *player = source.actor;
    mc_chunk_data *data = f->states[0]->data;
    CHECK(mc_world_store_chunk_count(f->stores[0]) == 1 && f->providers[0].cache->count == 1 &&
          f->providers[0].cache->insertion_count == 1 &&
          f->providers[0].cache->insertions[0] == f->states[0] &&
          sw_cell(data, profile->xyz[0], profile->xyz[1], profile->xyz[2], profile->id,
                  profile->metadata));
    if (profile->water_direction >= 0) {
        int32_t water[5];
        CHECK(lv_water(&observation, water) &&
              sw_cell(data, water[0], water[1], water[2], profile->water_id, 0));
    }
    mc_java_random_reseed(world->fields.r, profile->seed);
    double discarded;
    CHECK(mc_java_math_random(f->profile, &discarded) == MC_JAVA_RANDOM_PROFILE_OK &&
          f->profile->math != NULL);
    mc_java_random seeded;
    mc_java_random_seed(&seeded, 0);
    atomic_store(&f->profile->math->state, seeded.state);
    f->profile->math->have_next_next_gaussian = false;
    f->profile->math->next_next_gaussian = 0;
    f->classes[0]->scheduled_initialized = profile->scheduled_initialized;
    f->classes[0]->scheduled_sequence.next = profile->scheduled_before;
    observation.base.base_entities = world->fields.b->count;
    const mc_world_access_source_callbacks callbacks = {
        .cell = lv_cell, .range = lv_range, .sound = lv_sound, .particle = lv_particle};
    CHECK(mc_world_access_source_create(&callbacks, &observation, &listener, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_access_add_receipted(world, listener, &occurrence, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    observation.base.listener = listener;
    if (profile->control != 2)
        CHECK(mc_math_classes_create(SIZE_MAX, &math_owner, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&math, math_owner);
    mc_entity_tick_execution_init(execution, &math);
    contexts = true;
    const mc_world_runtime_execution_code code = {{"lm", "e", "()V", 6}, {"lm", "e", "()V", 33}};
    mc_entity_damage_execution_init(damage, execution, f->classes[0], code, f->registry[0]);
    player->local_player->fields.living.av = 17;
    pw_state before;
    CHECK(pw_state_read(&observation.base, &before) &&
          lv_print_state(&observation, &before, "before"));
    bool accepted = true;
    mc_entity_status raw = MC_ENTITY_BUSY;
    const unsigned int saved_mxcsr = _mm_getcsr();
    if (profile->control == 1)
        _mm_setcsr(saved_mxcsr | 0x6000u);
    const bool continued =
        mc_entity_effect_damage_in_execution(damage, player, NULL, 0, &accepted, error, &raw);
    _mm_setcsr(saved_mxcsr);
    entered = true;
    const bool source_error = observation.base.receipt != NULL;
    const bool normal =
        !source_error && profile->control != 1 && profile->control != 3 && profile->control != 4;
    pw_state state;
    CHECK(pw_state_read(&observation.base, &state) && observation.base.valid &&
          continued == normal && error->player_wake.entered && error->player_bytecode == 35 &&
          (continued ? !accepted : accepted) && !math.failed && state.sleeping == !normal &&
          state.world_sleeping == !normal && state.timer == (normal ? 0 : 37) &&
          state.health == 20 && state.idle == 0 &&
          state.world_entities == observation.base.base_entities && state.count == 0);
    const char *exception = "NONE";
    if (source_error) {
        mc_entity_damage_failure failure = {0};
        CHECK(raw != MC_ENTITY_OK && mc_entity_damage_execution_source_failure(damage, &failure) &&
              failure.source_proven && failure.kind == MC_ENTITY_DAMAGE_FAILURE_SOURCE_EXCEPTION &&
              failure.exception_class != NULL &&
              strcmp(failure.exception_class, "java/lang/IllegalStateException") == 0 &&
              mc_world_access_receipt_retire(&observation.base.receipt, NULL) ==
                  MC_WORLD_STORAGE_BUSY);
        exception = failure.exception_class;
    } else
        CHECK(raw == MC_ENTITY_OK);
    mc_entity_damage_execution_view view = {0};
    CHECK(mc_entity_damage_execution_inspect(damage, &view) && view.result != NULL);
    if (profile->control == 1)
        CHECK(view.result->ownership_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              view.result->ownership_error.site.pc == 2 &&
              strcmp(view.result->ownership_error.site.class_name, "rp") == 0);
    if (profile->control == 3 || profile->control == 4)
        CHECK(view.result->ownership_status == MC_WORLD_STORAGE_BUSY &&
              observation.descendant_open &&
              execution->current_lease == observation.descendant.slot &&
              observation.ordinals[1] == 1 &&
              observation.ordinals[2] == (profile->control == 3 ? 0u : 4u) && !source_error &&
              execution->world_slots[observation.presentation_parent.slot].generation ==
                  observation.presentation_parent.generation &&
              execution->world_slots[observation.presentation_parent.slot].phase ==
                  MC_ENTITY_TICK_EXECUTION_WORLD_OPEN);
    printf("LAVA_WAKE_OUTCOME "
           "{\"mode\":\"%s\",\"input\":{\"id\":%d,\"metadata\":%d,\"xyz\":[%d,%d,%d],"
           "\"water_direction\":%d,\"water_id\":%d,\"world_seed\":%" PRId64 ",\"cell_error_at\":%d,"
           "\"sound_error_at\":%d,\"particle_error_at\":%d,\"range_error_at\":%d},\"control\":%d,"
           "\"scheduled_before\":{\"sequence\":\"%016" PRIx64
           "\",\"initialized\":%s},\"continued\":%s,"
           "\"raw\":%d,\"pc\":%u,\"wake_entered\":%s,\"result\":%s,\"exception\":\"%s\",\"native_"
           "status\":%d}\n",
           profile->mode, profile->id, profile->metadata, profile->xyz[0], profile->xyz[1],
           profile->xyz[2], profile->water_direction, profile->water_id, profile->seed,
           profile->error_at[0], profile->error_at[1], profile->error_at[2], profile->error_at[3],
           profile->control, (uint64_t)profile->scheduled_before,
           sw_bool(profile->scheduled_initialized), sw_bool(continued), (int)raw,
           (unsigned int)error->player_bytecode, sw_bool(error->player_wake.entered),
           continued ? sw_bool(accepted) : "null", exception, (int)view.result->ownership_status);
    CHECK(lv_print_state(&observation, &state, normal ? "returned" : "diagnostic"));
    lv_print_events(&observation);
    if (source_error) {
        receipt_holds = observation.base.receipt->holds;
        observation.base.receipt->holds = SIZE_MAX;
        saturated = true;
    }
    if (profile->control == 3 || profile->control == 4) {
        const mc_world_storage_status refused = mc_entity_damage_execution_native_close(damage);
        pw_state blocked;
        CHECK(refused == MC_WORLD_STORAGE_BUSY && pw_state_read(&observation.base, &blocked) &&
              pw_state_equal(&state, &blocked) &&
              execution->current_lease == observation.descendant.slot &&
              execution->world_slots[observation.presentation_parent.slot].phase ==
                  MC_ENTITY_TICK_EXECUTION_WORLD_OPEN);
        const mc_world_storage_status popped =
            mc_entity_tick_execution_pop_world(execution, &observation.descendant, false);
        CHECK(popped == MC_WORLD_STORAGE_OK);
        observation.descendant_open = false;
        const mc_world_storage_status closed = mc_entity_damage_execution_native_close(damage);
        CHECK(closed == MC_WORLD_STORAGE_OK &&
              execution->world_slots[observation.presentation_parent.slot].phase !=
                  MC_ENTITY_TICK_EXECUTION_WORLD_OPEN &&
              execution->world_slots[observation.descendant.slot].phase ==
                  MC_ENTITY_TICK_EXECUTION_WORLD_FREE);
        printf("LAVA_WAKE_DESCENDANT "
               "{\"raw_callback\":0,\"callback_kind\":\"%s\",\"first_close\":%d,\"child_pop\":%d,"
               "\"second_close\":%d,"
               "\"parent_slot\":%zu,\"parent_generation\":%" PRIu64
               ",\"child_slot\":%zu,\"child_generation\":%" PRIu64
               ",\"source_exception\":false,\"explicit_child_pops\":1}\n",
               profile->control == 3 ? "sound" : "particle", (int)refused, (int)popped, (int)closed,
               observation.presentation_parent.slot, observation.presentation_parent.generation,
               observation.descendant.slot, observation.descendant.generation);
    } else
        CHECK(mc_entity_damage_execution_native_close(damage) == MC_WORLD_STORAGE_OK);
    pw_state after;
    CHECK(pw_state_read(&observation.base, &after) && pw_state_equal(&state, &after) &&
          lv_print_state(&observation, &after, "native-close"));
    CHECK(mc_entity_damage_execution_cleanup(damage, error) == MC_ENTITY_OK && damage->cleaned &&
          !mc_entity_effect_has_pending(player) &&
          (!saturated || observation.base.receipt->holds == SIZE_MAX));
    if (saturated) {
        observation.base.receipt->holds = receipt_holds;
        saturated = false;
    }
    CHECK(pw_state_read(&observation.base, &after) && pw_state_equal(&state, &after) &&
          lv_print_state(&observation, &after, "cleanup"));
    CHECK(mc_entity_tick_execution_retire(execution) == MC_WORLD_STORAGE_OK &&
          mc_math_execution_retire(&math) == MC_MATH_CLASS_OK &&
          mc_math_classes_release(&math_owner, NULL) == MC_MATH_CLASS_OK);
    retired = true;
    CHECK(pw_state_read(&observation.base, &after) && pw_state_equal(&state, &after) &&
          lv_print_state(&observation, &after, "retired"));
    ok = true;
cleanup:
    if (observation.descendant_open) {
        if (mc_entity_tick_execution_pop_world(execution, &observation.descendant, false) !=
            MC_WORLD_STORAGE_OK)
            return false;
        observation.descendant_open = false;
    }
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

static bool lv_integer(const char *text, int64_t *out) {
    if (text == NULL || *text == '\0')
        return false;
    const char *at = *text == '-' ? text + 1 : text;
    if (*at == '\0')
        return false;
    for (; *at != '\0'; ++at)
        if (*at < '0' || *at > '9')
            return false;
    errno = 0;
    char *end;
    const long long value = strtoll(text, &end, 10);
    if (errno == ERANGE || *end != '\0')
        return false;
    *out = (int64_t)value;
    return true;
}

int main(int argc, char **argv) {
    char root[8192];
    lv_profile profile = {0};
    if (argc != 18 || !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 2;
    if (strlen(argv[1]) == 0 || strlen(argv[1]) > 127)
        return 2;
    for (const char *at = argv[1]; *at != '\0'; ++at)
        if (!((*at >= 'a' && *at <= 'z') || (*at >= '0' && *at <= '9') || *at == '-'))
            return 2;
    profile.mode = argv[1];
    int64_t values[12];
    for (size_t i = 0; i < COUNT(values); ++i)
        if (!lv_integer(argv[3 + i], &values[i]) ||
            (i != 7 && (values[i] < INT32_MIN || values[i] > INT32_MAX)))
            return 2;
    profile.id = (int32_t)values[0];
    profile.metadata = (int32_t)values[1];
    for (size_t i = 0; i < 3; ++i)
        profile.xyz[i] = (int32_t)values[i + 2];
    profile.water_direction = (int32_t)values[5];
    profile.water_id = (int32_t)values[6];
    profile.seed = values[7];
    for (size_t i = 0; i < 4; ++i)
        profile.error_at[i] = (int32_t)values[i + 8];
    for (size_t i = 0; i < 4; ++i)
        if (profile.error_at[i] < 0 || profile.error_at[i] > 64)
            return 2;
    if ((profile.id != 10 && profile.id != 11) || profile.metadata < 0 || profile.metadata > 15 ||
        profile.xyz[0] < 1 || profile.xyz[0] > 14 || profile.xyz[1] < 1 || profile.xyz[1] > 126 ||
        profile.xyz[2] < 1 || profile.xyz[2] > 14 || profile.water_direction < -1 ||
        profile.water_direction > 4 ||
        (profile.water_id != 0 && profile.water_id != 8 && profile.water_id != 9) ||
        strlen(argv[15]) != 16 || strlen(argv[16]) != 1 ||
        (argv[16][0] != '0' && argv[16][0] != '1'))
        return 2;
    uint64_t sequence = 0;
    for (size_t i = 0; i < 16; ++i) {
        const char value = argv[15][i];
        const uint64_t digit = value >= '0' && value <= '9'   ? (uint64_t)(value - '0')
                               : value >= 'a' && value <= 'f' ? (uint64_t)(value - 'a' + 10)
                                                              : UINT64_MAX;
        if (digit > 15)
            return 2;
        sequence = sequence * UINT64_C(16) + digit;
    }
    profile.scheduled_before = sequence <= (uint64_t)INT64_MAX
                                   ? (int64_t)sequence
                                   : -(int64_t)(UINT64_MAX - sequence) - INT64_C(1);
    profile.scheduled_initialized = argv[16][0] == '1';
    profile.control = strcmp(argv[17], "none") == 0                  ? 0
                      : strcmp(argv[17], "fp") == 0                  ? 1
                      : strcmp(argv[17], "no-math-helper") == 0      ? 2
                      : strcmp(argv[17], "sound-descendant") == 0    ? 3
                      : strcmp(argv[17], "particle-descendant") == 0 ? 4
                                                                     : -1;
    if (profile.control < 0 || !lv_run(root, &profile))
        return 1;
    return printf("PASS: %u retained Lava wake checks;mode=%s\n", checks, profile.mode) < 0;
}
