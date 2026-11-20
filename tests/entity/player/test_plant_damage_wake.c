/* Actual retained Player damage/wake over genuine World, Bed and Chunk owners.
 * The imported fixture entry point is never executed. */
#define MC_SLEEP_WAKE_ENTRY pw_unused_sleep_wake_main
#include "test_sleep_wake.c"
#undef MC_SLEEP_WAKE_ENTRY

#include "../../../src/entity/tick/tick_internal.h"
#include "entity/movement/damage_execution_internal.h"
#include "mc/entity/movement/effects.h"
#include "world/runtime/mutation/drops/execution_internal.h"
#include "world/runtime/mutation/mutation_internal.h"
#include "world/runtime/world_access_internal.h"
#include "world/storage/objects_internal.h"

typedef struct pw_profile {
    const char *name;
    int32_t id, metadata, floor, light;
    int64_t seed;
    size_t publication_error_at;
    bool removal_error, missing_math, initial_sky_seen;
} pw_profile;

#define PW_PROFILE(name, id, metadata, floor, light, seed, added, removed, missing, seen)          \
    { name, id, metadata, floor, light, INT64_C(seed), added, removed, missing, seen }
static const pw_profile pw_profiles[] = {
    PW_PROFILE("dead-bush-supported", 32, 0, 12, 15, 1, 0, false, false, false),
    PW_PROFILE("dead-bush-unsupported", 32, 0, 2, 15, 1, 0, false, false, true),
    PW_PROFILE("crop-supported-stage0", 59, 0, 60, 15, 1, 0, false, false, true),
    PW_PROFILE("crop-supported-stage7", 59, 7, 60, 15, 1, 0, false, false, true),
    PW_PROFILE("crop-unsupported-stage0-seed1", 59, 0, 0, 15, 1, 0, false, false, true),
    PW_PROFILE("crop-unsupported-stage0-seed1234", 59, 0, 0, 15, 0x1234, 0, false, false, true),
    PW_PROFILE("crop-unsupported-stage0-seed7fffffff", 59, 0, 0, 15, 0x7fffffff, 0, false, false,
               true),
    PW_PROFILE("crop-unsupported-stage7-seed1", 59, 7, 0, 15, 1, 0, false, false, true),
    PW_PROFILE("crop-unsupported-stage7-seed1234", 59, 7, 0, 15, 0x1234, 0, false, false, true),
    PW_PROFILE("crop-unsupported-stage7-seed7fffffff", 59, 7, 0, 15, 0x7fffffff, 0, false, false,
               true),
    PW_PROFILE("crop-publication-error-first", 59, 7, 0, 15, 1, 1, false, false, true),
    PW_PROFILE("crop-publication-error-second", 59, 7, 0, 15, 1, 2, false, false, true),
    PW_PROFILE("crop-removal-error", 59, 7, 0, 15, 1, 0, true, false, true),
    PW_PROFILE("mushroom39-supported-light0", 39, 0, 2, 0, 1, 0, false, false, true),
    PW_PROFILE("mushroom39-supported-light12", 39, 0, 2, 12, 1, 0, false, false, true),
    PW_PROFILE("mushroom39-unsupported", 39, 0, 0, 12, 1, 0, false, false, true),
    PW_PROFILE("mushroom39-high-light", 39, 0, 2, 13, 1, 0, false, false, true),
    PW_PROFILE("mushroom39-publication-error", 39, 0, 0, 12, 1, 1, false, false, true),
    PW_PROFILE("mushroom39-removal-error", 39, 0, 0, 12, 1, 0, true, false, true),
    PW_PROFILE("mushroom40-supported-light0", 40, 0, 2, 0, 1, 0, false, false, true),
    PW_PROFILE("mushroom40-supported-light12", 40, 0, 2, 12, 1, 0, false, false, true),
    PW_PROFILE("mushroom40-unsupported", 40, 0, 0, 12, 1, 0, false, false, true),
    PW_PROFILE("mushroom40-high-light", 40, 0, 2, 13, 1, 0, false, false, true),
    PW_PROFILE("mushroom40-publication-error", 40, 0, 0, 12, 1, 1, false, false, true),
    PW_PROFILE("mushroom40-removal-error", 40, 0, 0, 12, 1, 0, true, false, true),
    PW_PROFILE("dead-bush-unsupported-math-missing", 32, 0, 2, 15, 1, 0, false, true, true),
    PW_PROFILE("crop-unsupported-stage0-seed1-math-missing", 59, 0, 0, 15, 1, 0, false, true, true),
    PW_PROFILE("crop-unsupported-stage7-seed1-math-missing", 59, 7, 0, 15, 1, 0, false, true, true),
    PW_PROFILE("mushroom39-unsupported-math-missing", 39, 0, 0, 12, 1, 0, false, true, true),
    PW_PROFILE("mushroom40-unsupported-math-missing", 40, 0, 0, 12, 1, 0, false, true, true),
};
#undef PW_PROFILE

typedef struct pw_item {
    uint64_t position[3], random, gaussian;
    uint64_t motion[3];
    uint32_t phase, yaw;
    int32_t id, count, damage, delay;
    bool inserted, world_same, have_gaussian;
} pw_item;

typedef struct pw_model {
    uint64_t random;
    size_t draws, count;
    pw_item items[4];
    uint64_t publication_random[4];
    bool supported, source_error, math_failed, removed;
} pw_model;

typedef struct pw_event {
    bool added, sleeping;
    int32_t xyz[3], head[2], plant[2];
    uint64_t random;
    pw_item item;
} pw_event;

typedef struct pw_state {
    uint64_t position[3], world_random, player_random, world_gaussian, player_gaussian;
    uint64_t math_random, math_gaussian;
    uint32_t dimensions[3];
    unsigned int math_phase;
    int32_t head[2], plant[2], floor[2], timer, health, idle;
    int32_t light[3], world_f;
    size_t count, world_entities, events, cells;
    bool sleeping, world_sleeping, world_same, sky_seen, world_cache, player_cache;
    bool math_present, math_cache;
    pw_item items[4];
} pw_state;

#ifndef PW_EVENT_CAPACITY
#define PW_EVENT_CAPACITY 6
#endif
typedef struct pw_observation {
    sw_fixture *fixture;
    const pw_profile *profile;
    const pw_model *model;
    mc_entity_tick_execution *execution;
    mc_world_access *listener;
    mc_world_access_receipt *receipt;
    mc_entity *items[4]; /* Actual published World occurrences, borrowed while live. */
    pw_event events[PW_EVENT_CAPACITY];
    size_t count, event_count, cells, base_entities;
    bool valid;
} pw_observation;

/* Independent unsigned Java LCG, without game selector calls. */
static uint32_t pw_bits(pw_model *model, unsigned int bits) {
    model->random =
        (model->random * UINT64_C(0x5deece66d) + UINT64_C(11)) & UINT64_C(0xffffffffffff);
    ++model->draws;
    return (uint32_t)(model->random >> (48u - bits));
}

static float pw_float(pw_model *model) {
    return (float)pw_bits(model, 24) / 16777216.0f;
}

static int32_t pw_bounded(pw_model *model) {
    for (;;) {
        const uint32_t bits = pw_bits(model, 31), value = bits % 15u;
        if (bits - value + 14u <= (uint32_t)INT32_MAX)
            return (int32_t)value;
    }
}

static bool pw_model_item(pw_model *model, const pw_profile *profile, int32_t id, bool crop) {
    pw_item item = {.id = id, .count = 1, .delay = 10, .inserted = true, .world_same = true};
    const float spread = .7f;
    const int32_t xyz[] = {7, 64, 8};
    for (size_t i = 0; i < 3; ++i) {
        const float scaled = pw_float(model) * spread;
        const float edge = 1.0f - spread;
        double value;
        if (crop) {
            const float margin = edge * .5f;
            const float offset = scaled + margin;
            const float position = (float)xyz[i] + offset;
            value = (double)position;
        } else {
            const double margin = (double)edge * .5;
            const double offset = (double)scaled + margin;
            value = (double)xyz[i] + offset;
        }
        item.position[i] = movement_double_bits(value);
    }
    if (profile->missing_math) {
        model->math_failed = true;
        return false;
    }
    if (model->count == COUNT(model->items))
        return false;
    model->items[model->count] = item;
    model->publication_random[model->count++] = model->random;
    if (profile->publication_error_at == model->count) {
        model->source_error = true;
        return false;
    }
    return true;
}

static pw_model pw_predict(const pw_profile *profile) {
    pw_model model = {.random = ((uint64_t)profile->seed ^ UINT64_C(0x5deece66d)) &
                                UINT64_C(0xffffffffffff)};
    model.supported = profile->id == 32   ? profile->floor == 12
                      : profile->id == 59 ? profile->floor == 60
                                          : profile->light < 13 && profile->floor == 2;
    if (model.supported)
        return model;
    (void)pw_float(&model); /* Inherited chance float before virtual item selection. */
    if (profile->id != 32 && (profile->id != 59 || profile->metadata == 7) &&
        !pw_model_item(&model, profile, profile->id == 59 ? 296 : profile->id, false))
        return model;
    if (profile->id == 59)
        for (size_t i = 0; i < 3; ++i)
            if (pw_bounded(&model) <= profile->metadata &&
                !pw_model_item(&model, profile, 295, true))
                return model;
    model.removed = true;
    model.source_error = profile->removal_error;
    return model;
}

static bool pw_cell_read(const mc_chunk_data *data, int32_t x, int32_t y, int32_t z,
                         int32_t out[2]) {
    uint8_t metadata;
    if (!mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z, &metadata))
        return false;
    out[0] = data->blocks[boxes_index(x, y, z)];
    out[1] = metadata;
    return true;
}

static bool pw_item_read(const mc_entity *item, const mc_world_runtime *world, pw_item *out) {
    mc_item_stack_fields stack = {0};
    if (item == NULL || !mc_entity_is_kind(item, MC_ENTITY_CLASS_KIND_ITEM) ||
        item->random == NULL || mc_item_stack_fields_get(item->stack, &stack) != MC_ITEM_STACK_OK)
        return false;
    *out = (pw_item){.position = {movement_double_bits(item->base.x),
                                  movement_double_bits(item->base.y),
                                  movement_double_bits(item->base.z)},
                     .motion = {movement_double_bits(item->base.velocity_x),
                                movement_double_bits(item->base.velocity_y),
                                movement_double_bits(item->base.velocity_z)},
                     .phase = movement_float_bits(item->item.d),
                     .yaw = movement_float_bits(item->base.yaw),
                     .random = item->random->state,
                     .gaussian = movement_double_bits(item->random->next_next_gaussian),
                     .id = stack.id,
                     .count = stack.count,
                     .damage = stack.damage,
                     .delay = item->item.c,
                     .inserted = item->base.bF,
                     .world_same = item->base.world == world,
                     .have_gaussian = item->random->have_next_next_gaussian};
    return true;
}

static bool pw_item_equal(const pw_item *a, const pw_item *b, bool full) {
    for (size_t i = 0; i < 3; ++i)
        if (a->position[i] != b->position[i] || (full && a->motion[i] != b->motion[i]))
            return false;
    return a->id == b->id && a->count == b->count && a->damage == b->damage &&
           a->delay == b->delay && a->inserted == b->inserted && a->world_same == b->world_same &&
           (!full || (a->phase == b->phase && a->yaw == b->yaw && a->random == b->random &&
                      a->gaussian == b->gaussian && a->have_gaussian == b->have_gaussian));
}

static bool pw_event_begin(pw_observation *o, pw_event *event) {
    const mc_chunk_data *data = o->fixture->world->states[0]->data;
    const mc_player_fields *p = &o->fixture->actor->local_player->fields.player;
    const bool result = o->event_count < COUNT(o->events) &&
                        pw_cell_read(data, 8, 64, 8, event->head) &&
                        pw_cell_read(data, 7, 64, 8, event->plant);
    event->sleeping = p->u;
    event->random = o->fixture->world->worlds[0]->fields.r->state;
    return result && event->head[0] == 26 && event->head[1] == 8 && p->u && p->private_a == 37 &&
           o->fixture->world->worlds[0]->fields.J;
}

static mc_world_storage_status pw_throw(pw_observation *o, mc_world_storage_error *error,
                                        mc_world_access_failure *failure, uint32_t number) {
    if (o->receipt != NULL)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    const mc_world_storage_status status =
        mc_world_access_receipt_create(o->listener, NULL, &o->receipt, failure, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    o->receipt->exception_class = "java/lang/IllegalStateException";
    *error =
        (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "Plant wake actual observer", number};
    return error->status;
}

static mc_world_storage_status pw_cell(void *context, mc_world_runtime *world, int32_t x, int32_t y,
                                       int32_t z, mc_world_storage_error *error,
                                       mc_world_access_failure *failure) {
    pw_observation *o = context;
    pw_event event = {.xyz = {x, y, z}};
    const bool first = o->cells == 0;
    o->valid &= pw_event_begin(o, &event) && world == o->fixture->world->worlds[0] &&
                o->cells < 2 && x == (first ? 8 : 7) && y == 64 && z == 8 &&
                event.plant[0] == (first ? o->profile->id : 0) &&
                event.plant[1] == (first ? o->profile->metadata : 0) &&
                o->count == (first ? 0 : o->model->count);
    if (o->event_count < COUNT(o->events))
        o->events[o->event_count++] = event;
    ++o->cells;
    *error = (mc_world_storage_error){0};
    if (!o->valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    return !first && o->profile->removal_error ? pw_throw(o, error, failure, 37)
                                               : MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status pw_added(void *context, mc_world_runtime *world, mc_entity *entity,
                                        mc_world_storage_error *error,
                                        mc_world_access_failure *failure) {
    pw_observation *o = context;
    pw_event event = {.added = true};
    mc_entity_tick_execution_world_token token;
    o->valid &= pw_event_begin(o, &event) && world == o->fixture->world->worlds[0] &&
                o->cells == 1 && o->count < o->model->count && o->count < COUNT(o->items) &&
                event.plant[0] == o->profile->id && event.plant[1] == o->profile->metadata &&
                pw_item_read(entity, world, &event.item) &&
                pw_item_equal(&event.item, &o->model->items[o->count], false) &&
                event.random == o->model->publication_random[o->count] &&
                world->fields.b->count == o->base_entities + o->count + 1 &&
                world->fields.b->items[world->fields.b->count - 1] == entity &&
                mc_entity_tick_execution_current_world(o->execution, &token) &&
                o->execution->leases[token.slot].phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED;
    if (o->count < COUNT(o->items))
        o->items[o->count++] = entity;
    if (o->event_count < COUNT(o->events))
        o->events[o->event_count++] = event;
    *error = (mc_world_storage_error){0};
    if (!o->valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    return o->profile->publication_error_at == o->count ? pw_throw(o, error, failure, 57)
                                                        : MC_WORLD_STORAGE_OK;
}

static bool pw_state_read(const pw_observation *o, pw_state *out) {
    fixture *f = o->fixture->world;
    const mc_entity *p = o->fixture->actor;
    const mc_player_fields *v = &p->local_player->fields.player;
    const mc_java_random *wr = f->worlds[0]->fields.r, *pr = p->random;
    const mc_chunk_data *data = f->states[0]->data;
    uint8_t sky, block_light;
    if (!mc_chunk_nibbles_get(data->skylight, MC_CHUNK_NIBBLE_BYTES, 7, 64, 8, &sky) ||
        !mc_chunk_nibbles_get(data->block_light, MC_CHUNK_NIBBLE_BYTES, 7, 64, 8, &block_light))
        return false;
    *out = (pw_state){.position = {movement_double_bits(p->base.x), movement_double_bits(p->base.y),
                                   movement_double_bits(p->base.z)},
                      .world_random = wr->state,
                      .player_random = pr->state,
                      .world_gaussian = movement_double_bits(wr->next_next_gaussian),
                      .player_gaussian = movement_double_bits(pr->next_next_gaussian),
                      .dimensions = {movement_float_bits(p->base.width),
                                     movement_float_bits(p->base.height),
                                     movement_float_bits(p->base.offset)},
                      .math_phase = atomic_load(&f->profile->holder_phase),
                      .light = {sky, block_light, data->heightmap[(8u << 4u) | 7u]},
                      .world_f = f->worlds[0]->fields.f,
                      .timer = v->private_a,
                      .health = p->local_player->fields.living.Y,
                      .idle = p->local_player->fields.living.av,
                      .count = o->count,
                      .world_entities = f->worlds[0]->fields.b->count,
                      .events = o->event_count,
                      .cells = o->cells,
                      .sleeping = v->u,
                      .world_sleeping = f->worlds[0]->fields.J,
                      .world_same = p->base.world == f->worlds[0],
                      .sky_seen = f->classes[0]->chunk_a,
                      .world_cache = wr->have_next_next_gaussian,
                      .player_cache = pr->have_next_next_gaussian,
                      .math_present = f->profile->math != NULL};
    if (f->profile->math != NULL) {
        out->math_random = atomic_load(&f->profile->math->state);
        out->math_gaussian = movement_double_bits(f->profile->math->next_next_gaussian);
        out->math_cache = f->profile->math->have_next_next_gaussian;
    }
    if (!pw_cell_read(data, 8, 64, 8, out->head) || !pw_cell_read(data, 7, 64, 8, out->plant) ||
        !pw_cell_read(data, 7, 63, 8, out->floor))
        return false;
    for (size_t i = 0; i < out->count; ++i)
        if (!pw_item_read(o->items[i], f->worlds[0], &out->items[i]))
            return false;
    return true;
}

static bool pw_state_equal(const pw_state *a, const pw_state *b) {
    for (size_t i = 0; i < 3; ++i)
        if (a->position[i] != b->position[i] || a->dimensions[i] != b->dimensions[i] ||
            a->light[i] != b->light[i])
            return false;
    for (size_t i = 0; i < 2; ++i)
        if (a->head[i] != b->head[i] || a->plant[i] != b->plant[i] || a->floor[i] != b->floor[i])
            return false;
    if (a->world_random != b->world_random || a->player_random != b->player_random ||
        a->world_gaussian != b->world_gaussian || a->player_gaussian != b->player_gaussian ||
        a->math_random != b->math_random || a->math_gaussian != b->math_gaussian ||
        a->math_phase != b->math_phase || a->timer != b->timer || a->health != b->health ||
        a->idle != b->idle || a->count != b->count || a->world_entities != b->world_entities ||
        a->events != b->events || a->cells != b->cells || a->sleeping != b->sleeping ||
        a->world_sleeping != b->world_sleeping || a->world_same != b->world_same ||
        a->sky_seen != b->sky_seen || a->world_cache != b->world_cache ||
        a->player_cache != b->player_cache || a->math_present != b->math_present ||
        a->math_cache != b->math_cache || a->world_f != b->world_f)
        return false;
    for (size_t i = 0; i < a->count; ++i)
        if (!pw_item_equal(&a->items[i], &b->items[i], true))
            return false;
    return true;
}

static void pw_print_item(const pw_item *item) {
    printf("{\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
           "\"],\"delay\":%d,\"stack\":[%d,%d,%d],\"inserted\":%s,\"world_same\":%s,"
           "\"random\":[\"%016" PRIx64 "\",%s,\"%016" PRIx64 "\"],"
           "\"phase\":\"%08" PRIx32 "\",\"yaw\":\"%08" PRIx32 "\","
           "\"motion\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]}",
           item->position[0], item->position[1], item->position[2], item->delay, item->id,
           item->count, item->damage, sw_bool(item->inserted), sw_bool(item->world_same),
           item->random, sw_bool(item->have_gaussian), item->gaussian, item->phase, item->yaw,
           item->motion[0], item->motion[1], item->motion[2]);
}

static void pw_print_state(const pw_observation *o, const pw_state *s, const char *phase) {
    printf("PLANT_WAKE_STATE {\"phase\":\"%s\",\"pending\":%s,\"position\":[\"%016" PRIx64
           "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],\"dimensions\":[\"%08" PRIx32
           "\",\"%08" PRIx32 "\",\"%08" PRIx32 "\"],\"sleeping\":%s,\"timer\":%d,"
           "\"health\":%d,\"idle\":%d,\"head\":[%d,%d],\"plant\":[%d,%d],"
           "\"plant_floor\":[%d,%d],\"world_sleeping\":%s,\"world_same\":%s,"
           "\"light\":[%d,%d,%d],\"world_f\":%d,\"sky_seen\":%s,\"world_random\":[\"%016" PRIx64
           "\",%s,\"%016" PRIx64 "\"],\"player_random\":[\"%016" PRIx64 "\",%s,\"%016" PRIx64
           "\"],\"math_random\":{\"present\":%s,\"phase\":%u,"
           "\"state\":\"%016" PRIx64 "\",\"cache\":%s,\"gaussian\":\"%016" PRIx64
           "\"},\"world_entities\":%zu,\"cells\":%zu,\"event_count\":%zu,\"items\":[",
           phase, sw_bool(mc_entity_effect_has_pending(o->fixture->actor)), s->position[0],
           s->position[1], s->position[2], s->dimensions[0], s->dimensions[1], s->dimensions[2],
           sw_bool(s->sleeping), s->timer, s->health, s->idle, s->head[0], s->head[1], s->plant[0],
           s->plant[1], s->floor[0], s->floor[1], sw_bool(s->world_sleeping),
           sw_bool(s->world_same), s->light[0], s->light[1], s->light[2], s->world_f,
           sw_bool(s->sky_seen), s->world_random, sw_bool(s->world_cache), s->world_gaussian,
           s->player_random, sw_bool(s->player_cache), s->player_gaussian, sw_bool(s->math_present),
           s->math_phase, s->math_random, sw_bool(s->math_cache), s->math_gaussian,
           s->world_entities, s->cells, s->events);
    for (size_t i = 0; i < s->count; ++i) {
        if (i != 0)
            putchar(',');
        pw_print_item(&s->items[i]);
    }
    puts("]}");
    printf("PLANT_WAKE_PLANES {\"phase\":\"%s\",\"planes\":", phase);
    (void)sw_planes(o->fixture->world->states[0]->data);
    puts("}");
}

static void pw_print_events(const pw_observation *o) {
    printf("PLANT_WAKE_EVENTS [");
    for (size_t i = 0; i < o->event_count; ++i) {
        const pw_event *event = &o->events[i];
        printf("%s{\"kind\":\"%s\",\"head\":[%d,%d],\"plant\":[%d,%d],\"sleeping\":%s,"
               "\"world_seed\":\"%016" PRIx64 "\"",
               i == 0 ? "" : ",", event->added ? "added" : "cell", event->head[0], event->head[1],
               event->plant[0], event->plant[1], sw_bool(event->sleeping), event->random);
        if (event->added) {
            printf(",\"item\":");
            pw_print_item(&event->item);
        } else
            printf(",\"xyz\":[%d,%d,%d]", event->xyz[0], event->xyz[1], event->xyz[2]);
        putchar('}');
    }
    puts("]");
}

static bool pw_run(const char *root, const pw_profile *profile) {
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
    const pw_model model = pw_predict(profile);
    const bool normal = !model.source_error && !model.math_failed;
    pw_observation observation = {.fixture = &source,
                                  .profile = profile,
                                  .model = &model,
                                  .execution = execution,
                                  .valid = true};
    const int32_t input[5] = {0, 1, 0, 0, 0};
    CHECK(error != NULL && damage != NULL && execution != NULL && sw_prepare(&source, root, input));
    fixture *f = source.world;
    mc_world_runtime *world = f->worlds[0];
    mc_entity *p = source.actor;
    mc_chunk_data *data = f->states[0]->data;
    CHECK(sw_cell(data, 7, 64, 8, profile->id, profile->metadata) &&
          sw_cell(data, 7, 63, 8, profile->floor, 0) &&
          mc_chunk_nibbles_set(data->skylight, MC_CHUNK_NIBBLE_BYTES, 7, 64, 8,
                               (uint8_t)profile->light));
    f->classes[0]->chunk_a = profile->initial_sky_seen;
    mc_java_random_reseed(world->fields.r, profile->seed);
    /* Match the prepared original Math stream without changing constructor entropy. */
    double discarded;
    CHECK(mc_java_math_random(f->profile, &discarded) == MC_JAVA_RANDOM_PROFILE_OK &&
          f->profile->math != NULL);
    mc_java_random seeded;
    mc_java_random_seed(&seeded, 0);
    atomic_store(&f->profile->math->state, seeded.state);
    f->profile->math->have_next_next_gaussian = false;
    f->profile->math->next_next_gaussian = 0;
    observation.base_entities = world->fields.b->count;
    const mc_java_random before_player = *p->random;
    const mc_world_access_source_callbacks callbacks = {.cell = pw_cell, .entity_added = pw_added};
    CHECK(mc_world_access_source_create(&callbacks, &observation, &listener, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_access_add_receipted(world, listener, &occurrence, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    observation.listener = listener;
    if (!profile->missing_math)
        CHECK(mc_math_classes_create(SIZE_MAX, &math_owner, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&math, math_owner);
    mc_entity_tick_execution_init(execution, &math);
    contexts = true;
    const mc_world_runtime_execution_code code = {{"lm", "e", "()V", 6}, {"lm", "e", "()V", 33}};
    mc_entity_damage_execution_init(damage, execution, f->classes[0], code, f->registry[0]);
    p->local_player->fields.living.av = 99;
    bool accepted = true;
    mc_entity_status raw = MC_ENTITY_BUSY;
    const bool continued =
        mc_entity_effect_damage_in_execution(damage, p, NULL, 0, &accepted, error, &raw);
    entered = true;
    pw_state state;
    CHECK(pw_state_read(&observation, &state) && observation.valid && continued == normal &&
          error->player_wake.entered && error->player_bytecode == 35 &&
          (continued ? !accepted : accepted) && state.health == 20 && state.idle == 0 &&
          state.sleeping == !normal && state.timer == (normal ? 0 : 37) &&
          state.world_sleeping == !normal && state.world_same && state.head[0] == 26 &&
          state.head[1] == 8 && state.plant[0] == (model.removed ? 0 : profile->id) &&
          state.plant[1] == (model.removed ? 0 : profile->metadata) && state.count == model.count &&
          state.cells == (model.removed ? 2u : 1u) &&
          state.world_entities == observation.base_entities + model.count &&
          state.world_random == model.random && state.player_random == before_player.state &&
          state.player_cache == before_player.have_next_next_gaussian &&
          state.player_gaussian == movement_double_bits(before_player.next_next_gaussian) &&
          state.sky_seen == (profile->initial_sky_seen || profile->light > 0) &&
          math.failed == model.math_failed && mc_entity_effect_has_pending(p) == !normal);
    for (size_t i = 0; i < model.count; ++i)
        CHECK(pw_item_equal(&state.items[i], &model.items[i], false));
    const char *exception = "NONE";
    mc_entity_damage_failure failure = {0};
    if (model.source_error) {
        CHECK(raw != MC_ENTITY_OK && mc_entity_damage_execution_source_failure(damage, &failure) &&
              failure.source_proven && failure.kind == MC_ENTITY_DAMAGE_FAILURE_SOURCE_EXCEPTION &&
              failure.exception_class != NULL &&
              strcmp(failure.exception_class, "java/lang/IllegalStateException") == 0 &&
              failure.site.pc == (profile->removal_error ? 34 : 29) &&
              observation.receipt != NULL &&
              mc_world_access_receipt_retire(&observation.receipt, NULL) == MC_WORLD_STORAGE_BUSY);
        exception = failure.exception_class;
    } else
        CHECK(raw == MC_ENTITY_OK);
    if (!normal) {
        mc_entity_damage_execution_view view;
        CHECK(mc_entity_damage_execution_inspect(damage, &view) && view.wake != NULL);
        if (model.math_failed) {
            const mc_world_mutation_drop_execution *drop =
                mc_player_wake_execution_failed_drop(view.wake);
            CHECK(damage->math_failed && view.result->math_failed && drop != NULL &&
                  drop->entity != NULL && !drop->active && state.count == 0);
        }
    }
    printf("PLANT_WAKE_OUTCOME {\"mode\":\"%s\",\"input\":{\"id\":%d,\"metadata\":%d,"
           "\"floor\":%d,\"light\":%d,\"world_seed\":%" PRId64
           ",\"publication_error_at\":%zu,\"removal_error\":%s,\"math_owned\":%s,"
           "\"initial_sky_seen\":%s},\"continued\":%s,\"raw\":%d,\"pc\":%u,"
           "\"wake_entered\":%s,\"result\":%s,\"exception\":\"%s\",\"math_failed\":%s,"
           "\"model_draws\":%zu}\n",
           profile->name, profile->id, profile->metadata, profile->floor, profile->light,
           profile->seed, profile->publication_error_at, sw_bool(profile->removal_error),
           sw_bool(!profile->missing_math), sw_bool(profile->initial_sky_seen), sw_bool(continued),
           (int)raw, (unsigned int)error->player_bytecode, sw_bool(error->player_wake.entered),
           continued ? sw_bool(accepted) : "null", exception, sw_bool(math.failed), model.draws);
    pw_print_state(&observation, &state, normal ? "returned" : "diagnostic");
    pw_print_events(&observation);
    if (observation.receipt != NULL) {
        receipt_holds = observation.receipt->holds;
        observation.receipt->holds = SIZE_MAX;
        saturated = true;
    }
    CHECK(mc_entity_damage_execution_native_close(damage) == MC_WORLD_STORAGE_OK);
    pw_state after;
    CHECK(pw_state_read(&observation, &after) && pw_state_equal(&state, &after));
    pw_print_state(&observation, &after, "native-close");
    CHECK(mc_entity_damage_execution_cleanup(damage, error) == MC_ENTITY_OK && damage->cleaned &&
          !mc_entity_effect_has_pending(p) &&
          (!saturated || observation.receipt->holds == SIZE_MAX));
    if (saturated) {
        observation.receipt->holds = receipt_holds;
        saturated = false;
    }
    CHECK(pw_state_read(&observation, &after) && pw_state_equal(&state, &after));
    pw_print_state(&observation, &after, "cleanup");
    CHECK(mc_entity_tick_execution_retire(execution) == MC_WORLD_STORAGE_OK &&
          mc_math_execution_retire(&math) == MC_MATH_CLASS_OK &&
          mc_math_classes_release(&math_owner, NULL) == MC_MATH_CLASS_OK);
    retired = true;
    CHECK(pw_state_read(&observation, &after) && pw_state_equal(&state, &after));
    pw_print_state(&observation, &after, "retired");
    ok = true;
cleanup:
    if (saturated)
        observation.receipt->holds = receipt_holds;
    if (entered && !damage->cleaned &&
        mc_entity_damage_execution_cleanup(damage, error) != MC_ENTITY_OK)
        return false;
    if (contexts && !retired &&
        (mc_entity_tick_execution_retire(execution) != MC_WORLD_STORAGE_OK ||
         mc_math_execution_retire(&math) != MC_MATH_CLASS_OK ||
         mc_math_classes_release(&math_owner, NULL) != MC_MATH_CLASS_OK))
        return false;
    if (observation.receipt != NULL &&
        mc_world_access_receipt_retire(&observation.receipt, NULL) != MC_WORLD_STORAGE_OK)
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

#ifndef MC_PLANT_DAMAGE_WAKE_ENTRY
#define MC_PLANT_DAMAGE_WAKE_ENTRY main
#endif
int MC_PLANT_DAMAGE_WAKE_ENTRY(int argc, char **argv) {
    char root[8192];
    if (argc != 3 || !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 2;
    for (size_t i = 0; i < COUNT(pw_profiles); ++i)
        if (strcmp(argv[1], pw_profiles[i].name) == 0) {
            if (!pw_run(root, &pw_profiles[i]))
                return 1;
            return printf("PASS: %u retained plant wake checks;mode=%s\n", checks, argv[1]) < 0;
        }
    return 2;
}
