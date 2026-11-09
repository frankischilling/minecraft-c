#include "../../world/runtime/execution_children_internal.h"
#include "../mob/tick/internal.h"
#include "../watcher_payload_internal.h"
#include "arrow_base_execution_internal.h"
#include "tick_internal.h"

void mc_tick_arrow_base_execution_init(mc_tick_arrow_base_execution *state,
                                       mc_entity_tick_execution *execution,
                                       mc_world_runtime_classes *code_classes,
                                       mc_world_runtime_execution_code code,
                                       mc_world_runtime_execution_result *owned) {
    if (state != NULL)
        *state = (mc_tick_arrow_base_execution){
            .execution = execution, .code_classes = code_classes, .code = code, .owned = owned};
    mc_world_runtime_execution_result_init(owned);
}

static bool base_result_empty(const mc_world_runtime_execution_result *owned) {
    if (owned->selection_ready || owned->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (owned->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (owned->closes[i].used)
            return false;
    return true;
}

bool mc_tick_arrow_base_world_begin(mc_tick_arrow_base_execution *state, mc_world_runtime *world,
                                    mc_entity_tick_execution_site site,
                                    mc_tick_arrow_base_child child) {
    if (state == NULL)
        return true;
    if (state->world_open || state->owned->ownership_status != MC_WORLD_STORAGE_OK ||
        !base_result_empty(state->owned)) {
        mc_world_runtime_execution_result_note(state->owned, MC_WORLD_STORAGE_BUSY,
                                               "Arrow Base prior World/query not drained", site);
        return false;
    }
    state->receiver_site = site;
    state->child = child;
    state->child_returned = false;
    state->scan = (mc_world_material_scan_query){0};
    const mc_world_storage_status status =
        mc_entity_tick_execution_push_world(state->execution, world, site, &state->world);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_execution_result_note(state->owned, status,
                                               "Arrow Base evaluated World ownership", site);
        return false;
    }
    state->world_open = true;
    state->close_attempted = false;
    return true;
}

bool mc_tick_arrow_base_world_end(mc_tick_arrow_base_execution *state, bool completed) {
    if (state == NULL)
        return completed;
    if (!state->world_open || state->close_attempted)
        return false;
    if (completed && (state->owned->ownership_status != MC_WORLD_STORAGE_OK ||
                      !base_result_empty(state->owned))) {
        mc_world_runtime_execution_result_note(state->owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                               "Arrow Base completed query not drained",
                                               state->receiver_site);
        completed = false;
    }
    state->close_attempted = true;
    const mc_world_storage_status status =
        mc_entity_tick_execution_pop_world(state->execution, &state->world, !completed);
    if (status != MC_WORLD_STORAGE_OK) {
        /* A pop can consume its token and still return a Native error. Read
         * ONLY this created token's VALUE phase/generation, even if another
         * checked producer invariant currently refuses. Never select a scope. */
        state->world_open = state->world.owner == state->execution &&
                            state->world.slot < MC_ENTITY_TICK_EXECUTION_WORLDS &&
                            state->execution->world_slots[state->world.slot].generation ==
                                state->world.generation &&
                            state->execution->world_slots[state->world.slot].phase ==
                                MC_ENTITY_TICK_EXECUTION_WORLD_OPEN;
        mc_world_runtime_execution_result_note(
            state->owned, status, "Arrow Base actual World checked close", state->receiver_site);
        return false;
    }
    state->world_open = false;
    if (completed)
        state->child = MC_TICK_ARROW_BASE_NO_CHILD;
    return completed;
}

/* This observes only the owned child's explicit Native capture channel. Never
 * dereference an unprotected outward receipt to classify a failed capture. */
static bool base_world_child_returned(mc_tick_arrow_base_execution *state,
                                      mc_world_storage_status status) {
    if (state == NULL)
        return true;
    state->child_returned = true;
    state->child_status = status;
    if (state->owned->ownership_status != MC_WORLD_STORAGE_OK)
        return false; /* The actual detailed SDK already recorded the first refusal. */
    if (state->execution->capture_pending) {
        const mc_entity_tick_execution_error capture = state->execution->capture;
        mc_world_runtime_execution_result_note(
            state->owned,
            capture.status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                                                  : capture.status,
            "Arrow Base World child pending capture", state->receiver_site);
        return false;
    }
    return state->owned->ownership_status == MC_WORLD_STORAGE_OK;
}

static void base_at(mc_entity_tick_error *e, unsigned int pc) {
    e->source_class = "sn";
    e->source_method = "U()V";
    e->base_bytecode = pc;
    e->bytecode = pc;
}

static void living_at(mc_entity_tick_error *e, unsigned int pc) {
    e->source_class = "ls";
    e->source_method = "U()V";
    e->living_bytecode = pc;
    e->bytecode = pc;
}

static bool next_float(mc_entity *entity, float *out, mc_entity_tick_error *e, unsigned int pc) {
    if (entity->random == NULL)
        return mc_tick_null(e, "Tick null Entity Random", pc);
    *out = mc_java_random_next_float(entity->random);
    return true;
}

static bool literal(mc_entity *entity, mc_registry_classes *classes, const char *text,
                    const mc_registry_string **out, mc_entity_tick_error *e, unsigned int pc) {
    return mc_tick_code(entity, classes, e, pc) &&
           mc_tick_registry(e, mc_registry_runtime_literal_ascii(classes, text, out, &e->registry),
                            "Tick actual code literal", pc);
}

static bool work(mc_entity_tick_error *e, unsigned int pc) {
    if (e->particle_work == e->max_particle_work)
        return mc_tick_fail(e, MC_ENTITY_LIMIT, "Tick pending particle iteration ceiling", pc);
    ++e->particle_work;
    return true;
}

static bool remote(mc_entity *entity, bool *out, mc_entity_tick_error *e, unsigned int pc,
                   mc_tick_arrow_base_execution *state) {
    mc_world_runtime *world = entity->base.world;
    if (!mc_tick_arrow_base_world_begin(
            state, world, (mc_entity_tick_execution_site){"sn", "U", "()V", (int)pc - 3},
            MC_TICK_ARROW_BASE_REMOTE))
        return false;
    if (world == NULL)
        return mc_tick_arrow_base_world_end(state,
                                            mc_tick_null(e, "Tick null World B receiver", pc));
    *out = world->fields.B;
    return mc_tick_arrow_base_world_end(state, true);
}

static bool particle(mc_world_runtime *world, const mc_registry_string *name, double x, double y,
                     double z, double vx, double vy, double vz, mc_entity_tick_error *e,
                     unsigned int pc, mc_tick_arrow_base_execution *state) {
    if (world == NULL)
        return mc_tick_arrow_base_world_end(
            state, mc_tick_null(e, "Tick null World particle receiver", pc));
    const mc_world_storage_status status =
        state == NULL
            ? mc_world_runtime_particle(world, name, x, y, z, vx, vy, vz, &e->world.presentation)
            : mc_world_runtime_particle_detailed_in_execution(state->execution, world, name, x, y,
                                                              z, vx, vy, vz, state->receiver_site,
                                                              &e->world.presentation, state->owned);
    const bool completed = base_world_child_returned(state, status) &&
                           mc_tick_world(e, status, "Tick actual World particle", pc);
    return mc_tick_arrow_base_world_end(state, completed);
}

static bool math_ready(mc_math_execution *execution, const char *method, const char *descriptor,
                       mc_entity_tick_error *e, unsigned int pc) {
    if (execution == NULL)
        return true;
    base_at(e, pc);
    const mc_math_class_status status = mc_math_execution_require(
        execution, (mc_math_execution_site){"sn", "U", "()V", (int)pc}, method, descriptor);
    if (status == MC_MATH_CLASS_OK)
        return true;
    const mc_math_class_error *child = mc_math_execution_cause(execution);
    e->source_exception = false;
    e->exception_class = NULL;
    return mc_tick_fail(e,
                        status == MC_MATH_CLASS_LIMIT               ? MC_ENTITY_LIMIT
                        : status == MC_MATH_CLASS_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                        : status == MC_MATH_CLASS_UNSUPPORTED_FP    ? MC_ENTITY_UNSUPPORTED_FP
                        : status == MC_MATH_CLASS_BUSY              ? MC_ENTITY_BUSY
                                                                    : MC_ENTITY_STATE_ERROR,
                        child->operation, pc);
}

static bool splash(mc_entity *entity, mc_registry_classes *classes, mc_entity_tick_error *e,
                   mc_math_execution *execution, mc_tick_arrow_base_execution *state) {
    base_at(e, 138);
    if (!mc_tick_fp(e, 110))
        return false;
    const double vx = entity->base.velocity_x;
    double squared = mc_tick_dmul(mc_tick_dmul(vx, entity->base.velocity_x), mc_tick_widen(.2f));
    squared = mc_tick_dadd(squared, mc_tick_dmul(entity->base.velocity_y, entity->base.velocity_y));
    squared = mc_tick_dadd(
        squared, mc_tick_dmul(mc_tick_dmul(entity->base.velocity_z, entity->base.velocity_z),
                              mc_tick_widen(.2f)));
    if (!math_ready(execution, "a", "(D)F", e, 138))
        return false;
    const double root = _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(squared)));
    const float root_float = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(root)));
    float volume = mc_tick_fmul(root_float, .2f);
    if (volume > 1.0f)
        volume = 1.0f;
    mc_world_runtime *sound_world = entity->base.world;
    if (!mc_tick_arrow_base_world_begin(state, sound_world,
                                        (mc_entity_tick_execution_site){"sn", "U", "()V", 154},
                                        MC_TICK_ARROW_BASE_SOUND))
        return false;
    const mc_registry_string *name = NULL;
    if (!literal(entity, classes, "random.splash", &name, e, 158))
        return false;
    float first, second;
    if (!next_float(entity, &first, e, 166) || !next_float(entity, &second, e, 173))
        return false;
    const float pitch = mc_tick_fadd(1.0f, mc_tick_fmul(mc_tick_fsub(first, second), .4f));
    base_at(e, 181);
    if (sound_world == NULL)
        return mc_tick_arrow_base_world_end(
            state, mc_tick_null(e, "Tick null World splash sound receiver", 181));
    const mc_world_storage_status status =
        state == NULL
            ? mc_world_runtime_entity_sound(sound_world, entity, name, volume, pitch, &e->world)
            : mc_world_runtime_entity_sound_detailed_in_execution(
                  state->execution, sound_world, entity, name, volume, pitch, state->receiver_site,
                  &e->world, state->owned);
    const bool sounded = base_world_child_returned(state, status) &&
                         mc_tick_world(e, status, "Tick actual splash sound", 181);
    if (!mc_tick_arrow_base_world_end(state, sounded))
        return false;
    base_at(e, 188);
    if (entity->bounds == NULL)
        return mc_tick_null(e, "Tick splash null permanent bounds", 188);
    const double min_y = entity->bounds->min_y;
    if (!math_ready(execution, "b", "(D)I", e, 191) || !mc_tick_fp(e, 191))
        return false;
    const float floor_y = mc_tick_i2f(mc_math_floor_double(min_y));
    for (int kind = 0; kind < 2; ++kind) {
        int32_t index = 0;
        for (;;) {
            base_at(e, kind == 0 ? 209u : 321u);
            if (!mc_tick_fp(e, e->base_bytecode))
                return false;
            const float count = mc_tick_i2f(index);
            const float ceiling = mc_tick_fadd(1.0f, mc_tick_fmul(entity->base.width, 20.0f));
            if (!(count < ceiling))
                break;
            if (!work(e, kind == 0 ? 217u : 329u))
                return false;
            float draw;
            if (!next_float(entity, &draw, e, kind == 0 ? 217u : 329u))
                return false;
            const float ox =
                mc_tick_fmul(mc_tick_fsub(mc_tick_fmul(draw, 2.0f), 1.0f), entity->base.width);
            if (!next_float(entity, &draw, e, kind == 0 ? 235u : 347u))
                return false;
            const float oz =
                mc_tick_fmul(mc_tick_fsub(mc_tick_fmul(draw, 2.0f), 1.0f), entity->base.width);
            mc_world_runtime *world = entity->base.world;
            if (!mc_tick_arrow_base_world_begin(
                    state, world,
                    (mc_entity_tick_execution_site){"sn", "U", "()V", kind == 0 ? 250 : 362},
                    MC_TICK_ARROW_BASE_PARTICLE))
                return false;
            if (!literal(entity, classes, kind == 0 ? "bubble" : "splash", &name, e,
                         kind == 0 ? 253u : 365u))
                return false;
            const double x = mc_tick_dadd(entity->base.x, mc_tick_widen(ox));
            const double y = mc_tick_widen(mc_tick_fadd(floor_y, 1.0f));
            const double z = mc_tick_dadd(entity->base.z, mc_tick_widen(oz));
            const double velocity_x = entity->base.velocity_x;
            double velocity_y = entity->base.velocity_y;
            if (kind == 0) {
                if (!next_float(entity, &draw, e, 287))
                    return false;
                velocity_y = mc_tick_dsub(velocity_y, mc_tick_widen(mc_tick_fmul(draw, .2f)));
            }
            const double velocity_z = entity->base.velocity_z;
            base_at(e, kind == 0 ? 299u : 399u);
            if (!particle(world, name, x, y, z, velocity_x, velocity_y, velocity_z, e,
                          e->base_bytecode, state))
                return false;
            index = mc_java_i32_add(index, 1);
        }
    }
    return true;
}

static bool damage(mc_entity *entity, mc_registry_classes *classes, mc_math_classes *math,
                   int32_t amount, mc_entity_tick_error *e, unsigned int pc) {
    bool ordinary;
    return mc_tick_effect(e,
                          mc_entity_effect_damage_with_math(entity, classes, math, NULL, amount,
                                                            &ordinary, &e->effect),
                          "Tick actual Entity damage virtual", pc);
}

static bool lava_damage(mc_entity *entity, mc_registry_classes *classes, mc_math_classes *math,
                        mc_entity_tick_error *e) {
    e->source_class = "sn";
    e->source_method = "ae()V";
    e->lava_bytecode = 1;
    if (entity->base.bC)
        return true;
    e->lava_bytecode = 13;
    if (!damage(entity, classes, math, 4, e, 13))
        return false;
    entity->base.bv = 600;
    return true;
}

static bool void_damage(mc_entity *entity, mc_registry_classes *classes, mc_math_classes *math,
                        mc_entity_tick_error *e) {
    const bool arrow = mc_entity_has_arrow(entity);
    if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) || arrow) {
        e->source_class = "sn";
        e->source_method = "ab()V";
        e->void_bytecode = 1;
        return mc_tick_effect(
            e, mc_entity_effect_dead(entity, &e->effect),
            arrow ? "Arrow void actual inherited K" : "Item void actual inherited K", 1);
    }
    e->source_class = "ls";
    e->source_method = "ab()V";
    e->void_bytecode = 3;
    return damage(entity, classes, math, 4, e, 3);
}

static bool base_body(mc_entity *entity, mc_registry_classes *classes, mc_math_classes *math,
                      const mc_entity_tick_limits *limits, mc_entity_tick_error *e,
                      mc_math_execution *execution, mc_tick_arrow_base_execution *state) {
    base_at(e, 1);
    mc_entity *rider = entity->base.aH;
    if (rider != NULL) {
        base_at(e, 11);
        if (rider->classes == NULL || rider->holds == 0 || rider->phase != MC_ENTITY_READY)
            return mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Tick riding Entity live field owner",
                                11);
        const mc_entity_status held = mc_entity_retain(rider);
        if (!mc_tick_effect(e, held, "Tick riding Entity lifetime hold", 11))
            return false;
        const bool dead = rider->base.be;
        mc_entity_release(rider);
        if (dead)
            (void)mc_entity_mount_vehicle_store(entity, NULL);
    }
    entity->base.bt = mc_java_i32_add(entity->base.bt, 1);
    entity->base.bi = entity->base.bj;
    entity->base.aJ = entity->base.x;
    entity->base.aK = entity->base.y;
    entity->base.aL = entity->base.z;
    entity->base.aV = entity->base.aT;
    entity->base.aU = entity->base.yaw;
    bool water;
    base_at(e, 81);
    if (state != NULL) {
        if (!mc_entity_environment_liquid_in_tick_execution(
                entity, classes, true, state, limits == NULL ? NULL : &limits->material, &water,
                &e->environment)) {
            if (state->owned->ownership_status == MC_WORLD_STORAGE_OK)
                (void)mc_tick_effect(e, e->environment.status, "Tick actual water virtual", 81);
            return false;
        }
    } else {
        const mc_entity_status water_status =
            execution == NULL
                ? mc_entity_environment_water(entity, classes,
                                              limits == NULL ? NULL : &limits->material, &water,
                                              &e->environment)
                : mc_entity_environment_water_held_in_execution(
                      entity, classes, execution, limits == NULL ? NULL : &limits->material, &water,
                      &e->environment);
        if (!mc_tick_effect(e, water_status, "Tick actual water virtual", 81))
            return false;
    }
    if (water) {
        if (!entity->base.bx && !entity->base.private_c &&
            !splash(entity, classes, e, execution, state))
            return false;
        entity->base.bk = 0.0f;
        entity->base.bx = true;
        entity->base.bv = 0;
    } else
        entity->base.bx = false;
    bool is_remote;
    base_at(e, 435);
    if (!remote(entity, &is_remote, e, 435, state))
        return false;
    if (is_remote)
        entity->base.bv = 0;
    else if (entity->base.bv > 0) {
        if (entity->base.bC) {
            entity->base.bv = mc_java_i32_sub(entity->base.bv, 4);
            if (entity->base.bv < 0)
                entity->base.bv = 0;
        } else {
            if (entity->base.bv % 20 == 0) {
                base_at(e, 501);
                if (!damage(entity, classes, math, 1, e, 501))
                    return false;
            }
            entity->base.bv = mc_java_i32_sub(entity->base.bv, 1);
        }
    }
    bool lava;
    base_at(e, 516);
    if (state != NULL) {
        if (!mc_entity_environment_liquid_in_tick_execution(
                entity, classes, false, state, limits == NULL ? NULL : &limits->material, &lava,
                &e->environment)) {
            if (state->owned->ownership_status == MC_WORLD_STORAGE_OK)
                (void)mc_tick_effect(e, e->environment.status, "Tick actual lava virtual", 516);
            return false;
        }
    } else {
        const mc_entity_status lava_status =
            execution == NULL
                ? mc_entity_environment_lava(entity, classes,
                                             limits == NULL ? NULL : &limits->material, &lava,
                                             &e->environment)
                : mc_entity_environment_lava_held_in_execution(
                      entity, classes, execution, limits == NULL ? NULL : &limits->material, &lava,
                      &e->environment);
        if (!mc_tick_effect(e, lava_status, "Tick actual lava virtual", 516))
            return false;
    }
    if (lava) {
        base_at(e, 523);
        if (!lava_damage(entity, classes, math, e))
            return false;
    }
    base_at(e, 533);
    if (!mc_tick_fp(e, 533))
        return false;
    if (entity->base.y < -64.0) {
        base_at(e, 538);
        if (!void_damage(entity, classes, math, e))
            return false;
    }
    base_at(e, 545);
    if (!remote(entity, &is_remote, e, 545, state))
        return false;
    if (!is_remote) {
        base_at(e, 565);
        if (!mc_tick_flag(entity, 0, entity->base.bv > 0, e))
            return false;
        base_at(e, 582);
        if (!mc_tick_flag(entity, 2, entity->base.aH != NULL, e))
            return false;
    }
    entity->base.private_c = false;
    base_at(e, 590);
    return true;
}

static bool drowning_particles(mc_entity *entity, mc_registry_classes *classes,
                               mc_entity_tick_error *e) {
    for (int index = 0; index < 8; ++index) {
        living_at(e, 152);
        if (!work(e, 152))
            return false;
        float offsets[3];
        static const unsigned int pcs[] = {152, 159, 168, 175, 184, 191};
        for (size_t i = 0; i < 3; ++i) {
            float first, second;
            if (!next_float(entity, &first, e, pcs[2 * i]) ||
                !next_float(entity, &second, e, pcs[2 * i + 1]))
                return false;
            if (i == 0) {
                living_at(e, 162);
                if (!mc_tick_fp(e, 162))
                    return false;
            }
            offsets[i] = mc_tick_fsub(first, second);
        }
        mc_world_runtime *world = entity->base.world;
        const mc_registry_string *name;
        if (!literal(entity, classes, "bubble", &name, e, 201))
            return false;
        const double x = mc_tick_dadd(entity->base.x, mc_tick_widen(offsets[0]));
        const double y = mc_tick_dadd(entity->base.y, mc_tick_widen(offsets[1]));
        const double z = mc_tick_dadd(entity->base.z, mc_tick_widen(offsets[2]));
        const double vx = entity->base.velocity_x, vy = entity->base.velocity_y,
                     vz = entity->base.velocity_z;
        living_at(e, 237);
        if (!particle(world, name, x, y, z, vx, vy, vz, e, 237, NULL))
            return false;
    }
    return true;
}

static bool death_particles(mc_entity *entity, mc_registry_classes *classes,
                            mc_entity_tick_error *e) {
    for (int index = 0; index < 20; ++index) {
        living_at(e, 374);
        if (!work(e, 374) || !mc_tick_fp(e, 374))
            return false;
        double velocity[3];
        static const unsigned int pcs[] = {374, 386, 399};
        for (size_t i = 0; i < 3; ++i) {
            if (entity->random == NULL)
                return mc_tick_null(e, "Death null Entity Random", pcs[i]);
            velocity[i] = mc_tick_dmul(mc_java_random_next_gaussian(entity->random), .02);
        }
        mc_world_runtime *world = entity->base.world;
        const mc_registry_string *name;
        if (!literal(entity, classes, "explode", &name, e, 412))
            return false;
        const double old_x = entity->base.x;
        float draw;
        if (!next_float(entity, &draw, e, 422))
            return false;
        const double x = mc_tick_dsub(
            mc_tick_dadd(old_x,
                         mc_tick_widen(mc_tick_fmul(mc_tick_fmul(draw, entity->base.width), 2.0f))),
            mc_tick_widen(entity->base.width));
        const double old_y = entity->base.y;
        if (!next_float(entity, &draw, e, 448))
            return false;
        const double y =
            mc_tick_dadd(old_y, mc_tick_widen(mc_tick_fmul(draw, entity->base.height)));
        const double old_z = entity->base.z;
        if (!next_float(entity, &draw, e, 466))
            return false;
        const double z = mc_tick_dsub(
            mc_tick_dadd(old_z,
                         mc_tick_widen(mc_tick_fmul(mc_tick_fmul(draw, entity->base.width), 2.0f))),
            mc_tick_widen(entity->base.width));
        living_at(e, 489);
        if (!particle(world, name, x, y, z, velocity[0], velocity[1], velocity[2], e, 489, NULL))
            return false;
    }
    return true;
}

static bool wolf_flag(mc_entity *entity, int32_t mask, bool *out, mc_entity_tick_error *e,
                      unsigned int pc) {
    int8_t value;
    if (!mc_tick_effect(e, mc_entity_effect_watcher_byte(entity, 16, &value, &e->effect),
                        "Wolf ambient actual typed flag", pc))
        return false;
    *out = ((int32_t)value & mask) != 0;
    return true;
}

static bool wolf_health(mc_entity *entity, int32_t *out, mc_entity_tick_error *e) {
    e->watcher_index = 18;
    e->watcher_bytecode = 8;
    if (entity->watcher == NULL || entity->watcher->map == NULL)
        return mc_tick_null(e, "Wolf ambient null watcher Map", 34);
    mc_entity_watch_map *map = entity->watcher->map;
    mc_entity_watch_node *node = map->head;
    size_t count = 0;
    while (node != NULL) {
        if (count++ == map->size)
            return mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Wolf ambient watcher Map ownership", 34);
        if (node->key == 18)
            break;
        node = node->next;
    }
    e->watcher_bytecode = 16;
    const mc_entity_watch_record *record = node == NULL ? NULL : node->record;
    if (record == NULL)
        return mc_tick_null(e, "Wolf ambient null Integer record", 34);
    e->watcher_bytecode = 19;
    const mc_entity_watch_payload_result payload =
        mc_entity_watch_payload_check(record, MC_ENTITY_WATCH_PAYLOAD_INTEGER);
    if (payload != MC_ENTITY_WATCH_PAYLOAD_OK) {
        if (payload == MC_ENTITY_WATCH_PAYLOAD_NULL_UNBOX) {
            e->watcher_bytecode = 22;
            return mc_tick_null(e, "Wolf ambient null Integer unbox receiver", 34);
        }
        if (payload == MC_ENTITY_WATCH_PAYLOAD_WRONG_CLASS) {
            e->source_exception = true;
            e->exception_class = "java/lang/ClassCastException";
        }
        return mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Wolf ambient actual Integer value", 34);
    }
    *out = record->integer_value;
    return true;
}

bool mc_tick_ambient_entered(mc_entity *entity, mc_registry_classes *classes,
                             mc_entity_tick_error *e) {
    const char *text = NULL;
    e->source_class = "ls";
    e->source_method = "T()V";
    e->ambient_bytecode = 1;
    if (mc_entity_has_mob(entity)) {
        if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SPIDER))
            text = "mob.spider";
        else if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ZOMBIE))
            text = "mob.zombie";
        else if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SKELETON))
            text = "mob.skeleton";
        else if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_PIG))
            text = "mob.pig";
        else if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SHEEP))
            text = "mob.sheep";
        else if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_COW))
            text = "mob.cow";
        else if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_CHICKEN))
            text = "mob.chicken";
        else if (mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_WOLF)) {
            bool angry;
            e->source_class = "gi";
            e->source_method = "g()Ljava/lang/String;";
            if (!wolf_flag(entity, 2, &angry, e, 1))
                return false;
            if (angry)
                text = "mob.wolf.growl";
            else {
                if (entity->random == NULL)
                    return mc_tick_null(e, "Wolf ambient null Random", 15);
                int32_t draw;
                if (!mc_java_random_next_bounded(entity->random, 3, &draw))
                    return mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Wolf ambient Random bound", 15);
                if (draw != 0)
                    text = "mob.wolf.bark";
                else {
                    bool tame;
                    if (!wolf_flag(entity, 4, &tame, e, 22))
                        return false;
                    int32_t health = 0;
                    if (tame && !wolf_health(entity, &health, e))
                        return false;
                    text = tame && health < 10 ? "mob.wolf.whine" : "mob.wolf.panting";
                }
            }
        }
    }
    e->source_class = "ls";
    e->source_method = "T()V";
    if (text == NULL) {
        e->ambient_bytecode = 42;
        return true;
    }
    const mc_registry_string *name;
    if (!literal(entity, classes, text, &name, e, 1))
        return false;
    e->ambient_bytecode = 10;
    mc_world_runtime *const world = entity->base.world;
    const float volume = mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_COW) ||
                                 mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_WOLF)
                             ? .4f
                             : 1.0f;
    float first, second;
    e->ambient_bytecode = 23;
    if (!next_float(entity, &first, e, 23))
        return false;
    e->ambient_bytecode = 30;
    if (!next_float(entity, &second, e, 30) || !mc_tick_fp(e, 33))
        return false;
    const float pitch = mc_tick_fadd(mc_tick_fmul(mc_tick_fsub(first, second), .2f), 1.0f);
    e->ambient_bytecode = 39;
    if (world == NULL)
        return mc_tick_null(e, "Living ambient null World sound receiver", 39);
    if (!mc_tick_world(e,
                       mc_world_runtime_entity_sound(world, entity, name, volume, pitch, &e->world),
                       "Living ambient actual World sound", 39))
        return false;
    e->ambient_bytecode = 42;
    return true;
}

static bool living_body(mc_entity *entity, mc_registry_classes *classes, mc_math_classes *math,
                        const mc_entity_tick_limits *limits, mc_entity_tick_error *e,
                        mc_mob_tick_scope *scope) {
    living_at(e, 0);
    mc_living_entity_fields *fields = mc_entity_living_fields(entity);
    if (fields == NULL)
        return mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Living tick represented embedded fields", 0);
    fields->W = fields->X;
    living_at(e, 9);
    if (!base_body(entity, classes, math, limits, e, NULL, NULL))
        return false;
    living_at(e, 19);
    if (entity->random == NULL)
        return mc_tick_null(e, "Living tick null ambient Random", 19);
    int32_t draw;
    if (!mc_java_random_next_bounded(entity->random, 1000, &draw))
        return mc_tick_fail(e, MC_ENTITY_STATE_ERROR, "Living tick actual Random.nextInt1000", 19);
    const int32_t old = fields->private_a;
    fields->private_a = mc_java_i32_add(old, 1);
    if (draw < old) {
        const bool creature = mc_entity_has_mob(entity) &&
                              !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SPIDER) &&
                              !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ZOMBIE) &&
                              !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SKELETON) &&
                              !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_CREEPER) &&
                              !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SLIME);
        fields->private_a = creature ? -120 : -80;
        living_at(e, 46);
        if (!mc_tick_ambient_entered(entity, classes, e))
            return false;
    }
    if (!entity->base.be && fields->Y > 0) {
        bool suffocating;
        living_at(e, 57);
        if (!mc_tick_suffocating_entered(entity, &suffocating, e, scope))
            return false;
        if (suffocating) {
            living_at(e, 66);
            if (!damage(entity, classes, math, 1, e, 66))
                return false;
        }
    }
    living_at(e, 71);
    if (entity->base.bC)
        entity->base.bv = 0;
    else {
        bool is_remote;
        living_at(e, 81);
        if (!remote(entity, &is_remote, e, 81, NULL))
            return false;
        if (is_remote)
            entity->base.bv = 0;
    }
    bool submerged = false;
    if (!entity->base.be && fields->Y > 0) {
        mc_material *water = NULL;
        living_at(e, 100);
        if (!mc_tick_code(entity, classes, e, 100) ||
            !mc_tick_registry(
                e, mc_registry_material_get(classes, MC_MATERIAL_g, &water, &e->registry),
                "Living tick actual Material.g", 100))
            return false;
        living_at(e, 103);
        if (!mc_tick_effect(e, mc_entity_inside_material(entity, water, &submerged, &e->material),
                            "Living tick actual eye immersion", 103))
            return false;
    }
    /* AR.d_ is true; the immersion query above still precedes that virtual. */
    if (submerged && !mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_SQUID)) {
        entity->base.bz = mc_java_i32_sub(entity->base.bz, 1);
        if (entity->base.bz == -20) {
            entity->base.bz = 0;
            if (!drowning_particles(entity, classes, e))
                return false;
            living_at(e, 249);
            if (!damage(entity, classes, math, 2, e, 249))
                return false;
        }
        entity->base.bv = 0;
    } else
        entity->base.bz = entity->base.bw;
    fields->af = fields->ag;
    if (fields->ae > 0)
        --fields->ae;
    if (fields->aa > 0)
        --fields->aa;
    if (entity->base.by > 0)
        --entity->base.by;
    if (fields->Y <= 0) {
        fields->ad = mc_java_i32_add(fields->ad, 1);
        if (fields->ad > 20) {
            living_at(e, 355); /* Actual inherited ls.aa() literal return0. */
            living_at(e, 359);
            if (!mc_tick_effect(
                    e, mc_entity_effect_dead_with_context(entity, classes, math, &e->effect),
                    "Living tick actual Player K close", 359))
                return false;
            if (!death_particles(entity, classes, e))
                return false;
        }
    }
    fields->M = fields->L;
    fields->I = fields->H;
    entity->base.aU = entity->base.yaw;
    entity->base.aV = entity->base.aT;
    living_at(e, 530);
    return true;
}

bool mc_tick_living_body_entered(mc_entity *entity, mc_registry_classes *classes,
                                 const mc_entity_tick_limits *limits, mc_entity_tick_error *e,
                                 mc_mob_tick_scope *scope) {
    return living_body(entity, classes, scope == NULL ? NULL : scope->math, limits, e, scope);
}

static mc_entity_status run(mc_entity *entity, mc_registry_classes *classes, mc_math_classes *math,
                            const mc_entity_tick_limits *limits, bool living,
                            mc_entity_tick_error *out) {
    mc_entity_tick_error frame;
    mc_entity_tick_error_init(&frame);
    frame.max_particle_work =
        limits == NULL ? MC_ENTITY_TICK_DEFAULT_PARTICLE_WORK : limits->max_particle_work;
    if (mc_tick_enter(entity, &frame)) {
        if (living)
            (void)living_body(entity, classes, math, limits, &frame, NULL);
        else
            (void)base_body(entity, classes, math, limits, &frame, NULL, NULL);
        mc_entity_release(entity);
    }
    return mc_tick_finish(&frame, out);
}

mc_entity_status mc_entity_tick_update_in_context(mc_entity *entity, mc_registry_classes *classes,
                                                  const mc_entity_tick_limits *limits,
                                                  mc_entity_tick_error *out) {
    return run(entity, classes, NULL, limits, true, out);
}

mc_entity_status mc_entity_tick_base_body_in_context(mc_entity *entity,
                                                     mc_registry_classes *classes,
                                                     const mc_entity_tick_limits *limits,
                                                     mc_entity_tick_error *out) {
    return run(entity, classes, NULL, limits, false, out);
}

mc_entity_status mc_entity_tick_update_with_math(mc_entity *entity, mc_registry_classes *classes,
                                                 mc_math_classes *math,
                                                 const mc_entity_tick_limits *limits,
                                                 mc_entity_tick_error *out) {
    return run(entity, classes, math, limits, true, out);
}

mc_entity_status mc_entity_tick_base_body_with_math(mc_entity *entity, mc_registry_classes *classes,
                                                    mc_math_classes *math,
                                                    const mc_entity_tick_limits *limits,
                                                    mc_entity_tick_error *out) {
    return run(entity, classes, math, limits, false, out);
}

/* Private selected hl caller already owns the real Item and its native hold.
 * hl inherits U from sn; it must not run the separate Living body. */
bool mc_tick_item_base_body(mc_entity *entity, mc_registry_classes *classes,
                            const mc_entity_tick_limits *limits, mc_entity_tick_error *error) {
    error->max_particle_work =
        limits == NULL ? MC_ENTITY_TICK_DEFAULT_PARTICLE_WORK : limits->max_particle_work;
    return base_body(entity, classes, NULL, limits, error, NULL, NULL);
}

static bool arrow_base_body_run(mc_entity *entity, mc_registry_classes *classes,
                                mc_math_execution *execution, const mc_entity_tick_limits *limits,
                                mc_entity_tick_error *error, mc_tick_arrow_base_execution *state) {
    if (!mc_entity_has_arrow(entity))
        return mc_tick_fail(error, MC_ENTITY_STATE_ERROR, "Tick actual client Arrow base owner", 0);
    if (entity->phase != MC_ENTITY_READY)
        return mc_tick_fail(error, MC_ENTITY_INVALID_PHASE, "Tick READY Arrow base owner", 0);
    if (entity->holds == 0)
        return mc_tick_fail(error, MC_ENTITY_STATE_ERROR, "Tick held Arrow base owner", 0);
    if (execution == NULL)
        return mc_tick_fail(error, MC_ENTITY_INVALID_ARGUMENT,
                            "Tick Arrow private Math execution storage", 0);
    error->max_particle_work =
        limits == NULL ? MC_ENTITY_TICK_DEFAULT_PARTICLE_WORK : limits->max_particle_work;
    if (state != NULL)
        state->body_entered = true;
    return base_body(entity, classes, NULL, limits, error, execution, state);
}

bool mc_tick_arrow_base_body(mc_entity *entity, mc_registry_classes *classes,
                             mc_math_execution *execution, const mc_entity_tick_limits *limits,
                             mc_entity_tick_error *error) {
    return arrow_base_body_run(entity, classes, execution, limits, error, NULL);
}

bool mc_tick_arrow_base_body_in_execution(mc_entity *entity, mc_registry_classes *classes,
                                          mc_tick_arrow_base_execution *state,
                                          const mc_entity_tick_limits *limits,
                                          mc_entity_tick_error *error) {
    if (state == NULL)
        return false;
    const mc_entity_tick_execution_site site = {"sn", "U", "()V", 0};
    if (state->execution == NULL || state->execution->identity != state->execution ||
        state->execution->math == NULL || state->code_classes == NULL || state->owned == NULL ||
        error == NULL || state->world_open || state->body_entered) {
        mc_world_runtime_execution_result_note(state->owned, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "Arrow Base required fresh execution/code/frame",
                                               site);
        return false;
    }
    const bool returned =
        arrow_base_body_run(entity, classes, state->execution->math, limits, error, state);
    /* Argument-prefix failure has no lower return to run the ordinary close.
     * Attempt only this genuinely created scope once. A previous refusal stays
     * live for the parent's exact-token native unwind; never retry Source. */
    if (state->world_open && !state->close_attempted)
        (void)mc_tick_arrow_base_world_end(state, false);
    state->body_completed = returned && state->owned->ownership_status == MC_WORLD_STORAGE_OK &&
                            !state->world_open && base_result_empty(state->owned);
    return state->body_completed;
}

bool mc_tick_base_body_entered(mc_entity *entity, mc_registry_classes *classes,
                               const mc_entity_tick_limits *limits, mc_entity_tick_error *error) {
    return base_body(entity, classes, NULL, limits, error, NULL, NULL);
}

bool mc_entity_mob_environment_entered(mc_mob_tick_scope *scope, mc_entity_tick_error *error) {
    error->max_particle_work = scope->environment_limits == NULL
                                   ? MC_ENTITY_TICK_DEFAULT_PARTICLE_WORK
                                   : scope->environment_limits->max_particle_work;
    return mc_tick_living_body_entered(scope->entity, scope->registry, scope->environment_limits,
                                       error, scope);
}
