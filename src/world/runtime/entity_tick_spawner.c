#include "../../block/registry/runtime_internal.h"
#include "../../entity/construction_internal.h"
#include "../../entity/mob/spawn_failure_internal.h"
#include "entity_tick_internal.h"
#include "mc/world/runtime/presentation.h"
#include "tile_entities/tick_internal.h"
#include "tile_entities/tile_entities_internal.h"

void mc_world_spawner_tick_error_retire(mc_world_spawner_tick_error *e) {
    if (e == NULL)
        return;
    if (e->child == MC_WORLD_SPAWNER_TICK_CHILD_EFFECT)
        mc_living_spawn_effect_error_retire(&e->detail.effect);
    mc_world_access_failure_release(&e->captured_failure);
    mc_world_runtime_binding_release(e->evaluated_world);
    mc_entity_release(e->candidate);
    memset(e, 0, sizeof(*e));
}

static void spawner_child(wt_scope *s, mc_world_spawner_tick_child child, unsigned int pc) {
    s->error.spawner_bytecode = pc;
    s->error.child_error.spawner.child = child;
    memset(&s->error.child_error.spawner.detail, 0, sizeof(s->error.child_error.spawner.detail));
}

static void capture_failure(wt_scope *s, const mc_world_access_failure *failure) {
    if (failure->receipt == NULL && !failure->source_null_receiver)
        return;
    mc_world_spawner_tick_error *e = &s->error.child_error.spawner;
    e->capture_attempted = true;
    e->capture_status = mc_world_access_failure_retain(failure, &e->captured_failure);
}

static bool hold_world(wt_scope *s, mc_world_runtime *world) {
    mc_world_spawner_tick_error *e = &s->error.child_error.spawner;
    if (world != NULL && !mc_world_runtime_binding_retain(world))
        return wt_cause(s, world->last_error);
    e->evaluated_world = world;
    return true;
}

static void release_world(wt_scope *s) {
    mc_world_spawner_tick_error *e = &s->error.child_error.spawner;
    mc_world_runtime_binding_release(e->evaluated_world);
    e->evaluated_world = NULL;
}

static bool has_failure(const mc_world_access_failure *failure) {
    return failure->receipt != NULL || failure->source_null_receiver;
}

static void capture_nearby(wt_scope *s, const mc_world_nearby_error *nearby) {
    /* These are exact descendants of this failed acquiring invocation. Capture
     * the reached concrete callback receipt, never an ambient World error. */
    const mc_world_access_failure *failure = &nearby->tile_entity.world_access_failure;
    if (!has_failure(failure))
        failure = &nearby->tile_entity.publication.world_access_failure;
    if (!has_failure(failure))
        failure = &nearby->tile_entity.batch.publication.world_access_failure;
    if (!has_failure(failure))
        failure = &nearby->batch.publication.world_access_failure;
    if (has_failure(failure))
        capture_failure(s, failure);
}

static void capture_predicate(wt_scope *s) {
    mc_mob_spawn_failure_view view =
        mc_mob_spawn_error_project(&s->error.child_error.spawner.detail.predicate);
    if (view.nearby != NULL)
        capture_nearby(s, view.nearby);
    else if (view.acquisition != NULL) {
        const mc_world_runtime_acquisition_error *a = view.acquisition;
        const mc_world_access_failure *failure = &a->tile_entity.world_access_failure;
        if (!has_failure(failure))
            failure = &a->tile_entity.publication.world_access_failure;
        if (!has_failure(failure))
            failure = &a->tile_entity.batch.publication.world_access_failure;
        if (!has_failure(failure))
            failure = &a->batch.publication.world_access_failure;
        if (has_failure(failure))
            capture_failure(s, failure);
    }
}

static bool reset_delay(wt_scope *s, mc_tile_entity *tile, unsigned int pc);
static bool expired(wt_scope *s, mc_tile_entity *tile);

static bool world_random(wt_scope *s, mc_tile_entity *tile, unsigned int pc, mc_java_random **out) {
    s->error.spawner_bytecode = pc;
    if (tile->world == NULL) {
        if (s->error.child == MC_WORLD_ENTITY_TICK_CHILD_SPAWNER) {
            s->error.child_error.spawner.source_exception = true;
            s->error.child_error.spawner.exception_class = "java/lang/NullPointerException";
            s->error.spawner_bytecode = pc == 372 ? 368 : pc - 3;
        }
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Spawner null World Random field receiver");
    }
    if (tile->world->fields.r == NULL) {
        if (s->error.child == MC_WORLD_ENTITY_TICK_CHILD_SPAWNER) {
            s->error.child_error.spawner.source_exception = true;
            s->error.child_error.spawner.exception_class = "java/lang/NullPointerException";
        }
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Spawner null Random invocation receiver");
    }
    *out = tile->world->fields.r;
    return true;
}

static bool particle(wt_scope *s, mc_tile_entity *tile, const char *literal, unsigned int pc,
                     double x, double y, double z) {
    mc_world_runtime *world = tile->world; /* cy72/88, before ldc and arguments. */
    const mc_registry_string *name = NULL;
    s->error.spawner_bytecode = pc;
    s->error.child = MC_WORLD_ENTITY_TICK_CHILD_REGISTRY;
    const mc_registry_status status = mc_registry_runtime_literal_ascii(
        mc_live_tile_tick_code_registry(tile), literal, &name, &s->error.child_error.registry);
    if (status != MC_REGISTRY_OK)
        return wt_fail(s, mc_world_runtime_registry_status(status),
                       "Spawner actual particle code literal");
    s->error.child = MC_WORLD_ENTITY_TICK_CHILD_PRESENTATION;
    if (mc_world_runtime_particle(world, name, x, y, z, 0.0, 0.0, 0.0,
                                  &s->error.child_error.presentation) != MC_WORLD_STORAGE_OK)
        return wt_cause(s, s->error.child_error.presentation.cause);
    s->error.child = MC_WORLD_ENTITY_TICK_CHILD_NONE;
    if (literal[0] == 's')
        ++s->error.spawner.smoke_calls;
    else
        ++s->error.spawner.flame_calls;
    return true;
}

bool wt_spawner(wt_scope *s, mc_tile_entity *tile) {
    s->error.virtual_class = "cy";
    s->error.virtual_method = "n_()V";
    tile->state.spawner.previous_rotation = tile->state.spawner.rotation;
    s->error.spawner_bytecode = 9;
    /* cy.b evaluates all coordinates before the nullable World invocation34. */
    if (!wt_fp(s, s->error.bytecode))
        return false;
    mc_world_runtime *world = tile->world;
    const double near_x = wt_add((double)tile->x, .5);
    const double near_y = wt_add((double)tile->y, .5);
    const double near_z = wt_add((double)tile->z, .5);
    mc_entity *nearest = NULL;
    s->error.tile_bytecode = 34;
    if (!wt_nearest(s, world, near_x, near_y, near_z, 16.0, &nearest))
        return false;
    const bool enabled = nearest != NULL;
    mc_entity_release(nearest);
    s->error.tile_bytecode = 0;
    if (!enabled)
        return true;
    mc_java_random *random;
    const float fx = wt_i2f(tile->x);
    if (!world_random(s, tile, 28, &random))
        return false;
    const double x = wt_widen(wt_fadd(fx, mc_java_random_next_float(random)));
    const float fy = wt_i2f(tile->y);
    if (!world_random(s, tile, 46, &random))
        return false;
    const double y = wt_widen(wt_fadd(fy, mc_java_random_next_float(random)));
    const float fz = wt_i2f(tile->z);
    if (!world_random(s, tile, 64, &random))
        return false;
    const double z = wt_widen(wt_fadd(fz, mc_java_random_next_float(random)));
    if (!particle(s, tile, "smoke", 84, x, y, z) || !particle(s, tile, "flame", 100, x, y, z))
        return false;
    s->error.spawner_bytecode = 114;
    if (!wt_fp(s, s->error.bytecode))
        return false;
    tile->state.spawner.rotation =
        wt_add(tile->state.spawner.rotation,
               wt_widen(wt_fdiv(1000.0f, wt_fadd(wt_i2f(tile->state.spawner.delay), 200.0f))));
    /* dcmpl/ifle skips subtraction for NaN; positive infinity enters. */
    while (tile->state.spawner.rotation > 360.0) {
        s->error.spawner_bytecode = 143;
        if (!wt_work(s, s->error.bytecode))
            return false;
        tile->state.spawner.rotation = wt_sub(tile->state.spawner.rotation, 360.0);
        tile->state.spawner.previous_rotation =
            wt_sub(tile->state.spawner.previous_rotation, 360.0);
    }
    s->error.spawner_bytecode = 166;
    world = tile->world;
    if (world == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Spawner null World remote field receiver");
    if (world->fields.B)
        return true;
    if (tile->state.spawner.delay == -1) {
        s->error.spawner_bytecode = 181;
        if (!world_random(s, tile, 181, &random))
            return false;
        int32_t delay;
        if (!mc_java_random_next_bounded(random, 600, &delay))
            return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Spawner actual delay nextInt");
        tile->state.spawner.delay = mc_java_i32_add(200, delay);
        ++s->error.spawner.delay_resets;
    }
    if (tile->state.spawner.delay > 0) {
        tile->state.spawner.delay = mc_java_i32_sub(tile->state.spawner.delay, 1);
        return true;
    }
    return expired(s, tile);
}

static bool reset_delay(wt_scope *s, mc_tile_entity *tile, unsigned int pc) {
    s->error.spawner_bytecode = pc;
    mc_world_spawner_tick_error *e = &s->error.child_error.spawner;
    e->delay_bytecode = 8;
    mc_world_runtime *world = tile->world;
    if (world == NULL) {
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Spawner delay null World Random field receiver");
    }
    mc_java_random *random = world->fields.r;
    e->delay_bytecode = 14;
    if (random == NULL) {
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Spawner delay null Random invocation receiver");
    }
    int32_t draw;
    if (!mc_java_random_next_bounded(random, 600, &draw))
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Spawner actual delay nextInt");
    tile->state.spawner.delay = mc_java_i32_add(200, draw);
    e->delay_bytecode = 0;
    ++s->error.spawner.delay_resets;
    return true;
}

static bool spawned_particle(wt_scope *s, mc_tile_entity *tile, const char *literal,
                             unsigned int pc, double x, double y, double z) {
    mc_world_runtime *world = tile->world;
    const mc_registry_string *name = NULL;
    spawner_child(s, MC_WORLD_SPAWNER_TICK_CHILD_REGISTRY, pc);
    mc_world_spawner_tick_error *e = &s->error.child_error.spawner;
    const mc_registry_status literal_status = mc_registry_runtime_literal_ascii(
        mc_live_tile_tick_code_registry(tile), literal, &name, &e->detail.registry);
    if (literal_status != MC_REGISTRY_OK)
        return wt_fail(s, mc_world_runtime_registry_status(literal_status),
                       "Spawner actual particle code literal");
    spawner_child(s, MC_WORLD_SPAWNER_TICK_CHILD_PARTICLE, pc);
    if (world == NULL) {
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Spawner particle null World invocation receiver");
    }
    if (!hold_world(s, world))
        return false;
    const mc_world_storage_status status =
        mc_world_runtime_particle(world, name, x, y, z, 0.0, 0.0, 0.0, &e->detail.particle);
    if (status != MC_WORLD_STORAGE_OK) {
        capture_failure(s, &e->detail.particle.world_access_failure);
        return wt_cause(s, e->detail.particle.cause);
    }
    e->child = MC_WORLD_SPAWNER_TICK_CHILD_NONE;
    release_world(s);
    if (literal[0] == 's')
        ++s->error.spawner.smoke_calls;
    else
        ++s->error.spawner.flame_calls;
    return true;
}

static bool population(wt_scope *s, mc_tile_entity *tile, mc_world_runtime_classes *code,
                       size_t *out) {
    /* World241 is captured before Entity.getClass246 and either pooled box. */
    mc_world_runtime *world = tile->world;
    mc_world_spawner_tick_error *e = &s->error.child_error.spawner;
    if (!hold_world(s, world))
        return false;
    const mc_entity_class_literal *type = e->candidate->class_literal;
    mc_entity_bounds *unit = NULL, *expanded = NULL;
    spawner_child(s, MC_WORLD_SPAWNER_TICK_CHILD_BOUNDS, 285);
    mc_entity_status status = mc_entity_bounds_pool_get(
        code->entities, (double)tile->x, (double)tile->y, (double)tile->z,
        (double)mc_java_i32_add(tile->x, 1), (double)mc_java_i32_add(tile->y, 1),
        (double)mc_java_i32_add(tile->z, 1), &unit, &e->detail.bounds);
    if (status != MC_ENTITY_OK)
        return wt_fail(s, wt_entity_status(status), e->detail.bounds.operation);
    spawner_child(s, MC_WORLD_SPAWNER_TICK_CHILD_BOUNDS, 297);
    status =
        mc_entity_bounds_expand(code->entities, unit, 8.0, 4.0, 8.0, &expanded, &e->detail.bounds);
    (void)mc_entity_bounds_release(code->entities, unit);
    if (status != MC_ENTITY_OK)
        return wt_fail(s, wt_entity_status(status), e->detail.bounds.operation);
    mc_world_class_entity_list *list = NULL;
    spawner_child(s, MC_WORLD_SPAWNER_TICK_CHILD_QUERY, 300);
    if (world == NULL) {
        (void)mc_entity_bounds_release(code->entities, expanded);
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Spawner population null World invocation receiver");
    }
    ++s->error.spawner.queries;
    mc_world_storage_status result =
        mc_world_runtime_class_entities(world, type, expanded, &list, &e->detail.query);
    (void)mc_entity_bounds_release(code->entities, expanded);
    if (result != MC_WORLD_STORAGE_OK) {
        capture_nearby(s, &e->detail.query);
        return wt_cause(s, e->detail.query.query.cause);
    }
    s->error.spawner_bytecode = 303;
    result = mc_world_class_entity_list_count(list, out);
    mc_world_class_entity_list_release(list);
    if (result != MC_WORLD_STORAGE_OK)
        return wt_fail(s, result, "Spawner actual fresh population List.size");
    e->child = MC_WORLD_SPAWNER_TICK_CHILD_NONE;
    release_world(s);
    return true;
}

static bool position(wt_scope *s, mc_tile_entity *tile) {
    mc_java_random *random;
    const double tile_x = (double)tile->x;
    if (!world_random(s, tile, 339, &random))
        return false;
    const double x_first = mc_java_random_next_double(random);
    if (!world_random(s, tile, 349, &random))
        return false;
    const double x =
        wt_add(tile_x, wt_mul(wt_sub(x_first, mc_java_random_next_double(random)), 4.0));
    const int32_t tile_y = tile->y;
    if (!world_random(s, tile, 372, &random))
        return false;
    int32_t draw;
    if (!mc_java_random_next_bounded(random, 3, &draw))
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Spawner actual height nextInt");
    const double y = (double)mc_java_i32_sub(mc_java_i32_add(tile_y, draw), 1);
    const double tile_z = (double)tile->z;
    if (!world_random(s, tile, 393, &random))
        return false;
    const double z_first = mc_java_random_next_double(random);
    if (!world_random(s, tile, 403, &random))
        return false;
    const double z =
        wt_add(tile_z, wt_mul(wt_sub(z_first, mc_java_random_next_double(random)), 4.0));
    if (!world_random(s, tile, 429, &random))
        return false;
    const float yaw = _mm_cvtss_f32(
        _mm_mul_ss(_mm_set_ss(mc_java_random_next_float(random)), _mm_set_ss(360.0f)));
    mc_entity_position_rotation_error pose = {0};
    spawner_child(s, MC_WORLD_SPAWNER_TICK_CHILD_POSE, 436);
    mc_world_spawner_tick_error *e = &s->error.child_error.spawner;
    const mc_entity_status status =
        mc_entity_source_position_rotation(e->candidate, x, y, z, yaw, 0.0f, &pose);
    if (status != MC_ENTITY_OK) {
        e->detail.pose.status = pose.status;
        e->detail.pose.operation = pose.operation;
        e->detail.pose.bytecode = pose.bytecode;
        e->detail.pose.position_bytecode = pose.position_bytecode;
        e->detail.pose.source_null = pose.source_null;
        e->detail.pose.native_policy = pose.native_policy;
        return wt_fail(s, wt_entity_status(status), pose.operation);
    }
    e->child = MC_WORLD_SPAWNER_TICK_CHILD_NONE;
    return true;
}

static bool spawn_effects(wt_scope *s, mc_tile_entity *tile) {
    for (size_t i = 0; i < 20; ++i) {
        s->error.spawner_bytecode = 464;
        if (!wt_work(s, s->error.bytecode))
            return false;
        mc_java_random *random;
        const double tile_x = (double)tile->x;
        if (!world_random(s, tile, 483, &random))
            return false;
        const double x =
            wt_add(wt_add(tile_x, .5),
                   wt_mul(wt_sub(wt_widen(mc_java_random_next_float(random)), .5), 2.0));
        const double tile_y = (double)tile->y;
        if (!world_random(s, tile, 513, &random))
            return false;
        const double y =
            wt_add(wt_add(tile_y, .5),
                   wt_mul(wt_sub(wt_widen(mc_java_random_next_float(random)), .5), 2.0));
        const double tile_z = (double)tile->z;
        if (!world_random(s, tile, 543, &random))
            return false;
        const double z =
            wt_add(wt_add(tile_z, .5),
                   wt_mul(wt_sub(wt_widen(mc_java_random_next_float(random)), .5), 2.0));
        if (!spawned_particle(s, tile, "smoke", 571, x, y, z) ||
            !spawned_particle(s, tile, "flame", 587, x, y, z))
            return false;
    }
    spawner_child(s, MC_WORLD_SPAWNER_TICK_CHILD_EFFECT, 598);
    mc_world_spawner_tick_error *e = &s->error.child_error.spawner;
    mc_living_spawn_effect_state state = {0};
    const mc_world_storage_status status = mc_living_spawn_effect(
        e->candidate, mc_live_tile_tick_code_registry(tile), NULL, &state, &e->detail.effect);
    s->error.spawner.explode_calls += state.particles_returned;
    if (status != MC_WORLD_STORAGE_OK)
        return wt_fail(s, status, e->detail.effect.operation);
    mc_living_spawn_effect_error_retire(&e->detail.effect);
    e->child = MC_WORLD_SPAWNER_TICK_CHILD_NONE;
    return reset_delay(s, tile, 602);
}

static bool expired(wt_scope *s, mc_tile_entity *tile) {
    s->error.spawner_bytecode = 223;
    if (!s->has_error_output)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World expired Spawner tick requires retained child output");
    s->error.child = MC_WORLD_ENTITY_TICK_CHILD_SPAWNER;
    memset(&s->error.child_error.spawner, 0, sizeof(s->error.child_error.spawner));
    mc_world_spawner_tick_error *e = &s->error.child_error.spawner;
    mc_world_runtime_classes *code = mc_live_tile_tick_code_classes(tile);
    if (code == NULL || code->entities == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Spawner actual defining Entity namespace");
    for (size_t attempt = 0; attempt < 4; ++attempt) {
        s->error.spawner_bytecode = 212;
        if (!wt_work(s, s->error.bytecode))
            return false;
        ++s->error.spawner.attempts;
        /* Keep the actual String value live across constructor descendants. */
        mc_tile_entity_name *name = NULL;
        e->tile_status = mc_tile_entity_spawner_get_name(tile, &name);
        if (e->tile_status != MC_TILE_ENTITY_OK)
            return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Spawner actual name operand");
        mc_tile_entity_name_view view;
        e->tile_status = mc_tile_entity_name_get_view(name, &view);
        if (e->tile_status != MC_TILE_ENTITY_OK) {
            mc_tile_entity_name_release(name);
            return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Spawner actual String view");
        }
        mc_world_runtime *world = tile->world; /* Actual field220. */
        spawner_child(s, MC_WORLD_SPAWNER_TICK_CHILD_FACTORY, 223);
        ++s->error.spawner.factory_calls;
        const mc_entity_factory_status made =
            mc_entity_factory_named(code->entities, mc_live_tile_tick_code_registry(tile),
                                    view.text, world, &e->candidate, &e->detail.factory);
        mc_tile_entity_name_release(name);
        if (made != MC_ENTITY_FACTORY_OK) {
            const mc_world_storage_status status =
                made == MC_ENTITY_FACTORY_LIMIT               ? MC_WORLD_STORAGE_LIMIT
                : made == MC_ENTITY_FACTORY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                : made == MC_ENTITY_FACTORY_ARGUMENT          ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                              : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
            return wt_fail(s, status, e->detail.factory.operation);
        }
        e->child = MC_WORLD_SPAWNER_TICK_CHILD_NONE;
        s->error.spawner_bytecode = 226;
        if (e->candidate == NULL) {
            ++s->error.spawner.null_returns;
            s->error.child = MC_WORLD_ENTITY_TICK_CHILD_NONE;
            return true;
        }
        const mc_entity_class_literal *living = NULL;
        bool matches = false;
        e->entity_status = mc_entity_class_literal_get(code->entities,
                                                       MC_ENTITY_CLASS_KIND_LIVING_ENTITY, &living);
        if (e->entity_status == MC_ENTITY_OK)
            e->entity_status = mc_entity_class_literal_is_assignable_from(
                living, e->candidate->class_literal, &matches);
        if (e->entity_status != MC_ENTITY_OK)
            return wt_fail(s, wt_entity_status(e->entity_status),
                           "Spawner actual Living cast Class");
        if (!matches) {
            e->source_exception = true;
            e->exception_class = "java/lang/ClassCastException";
            return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Spawner actual Living cast");
        }
        /* The second cast229 observes the same already-checked reference. */
        size_t count;
        if (!population(s, tile, code, &count))
            return false;
        if (count >= 6) {
            if (!reset_delay(s, tile, 318))
                return false;
            ++s->error.spawner.cap_returns;
            mc_world_spawner_tick_error_retire(e);
            s->error.child = MC_WORLD_ENTITY_TICK_CHILD_NONE;
            return true;
        }
        if (!position(s, tile))
            return false;
        spawner_child(s, MC_WORLD_SPAWNER_TICK_CHILD_PREDICATE, 441);
        if (!hold_world(s, e->candidate->base.world))
            return false;
        ++s->error.spawner.predicate_calls;
        bool eligible;
        mc_mob_spawn_state state = {0};
        const mc_world_storage_status spawn =
            mc_mob_spawn_eligible(e->candidate, s->context == NULL ? NULL : s->context->math, code,
                                  &eligible, &state, &e->detail.predicate);
        if (spawn != MC_WORLD_STORAGE_OK) {
            capture_predicate(s);
            return wt_fail(s, spawn, e->detail.predicate.operation);
        }
        release_world(s);
        e->child = MC_WORLD_SPAWNER_TICK_CHILD_NONE;
        if (eligible) {
            ++s->error.spawner.accepted;
            spawner_child(s, MC_WORLD_SPAWNER_TICK_CHILD_PUBLICATION, 453);
            world = tile->world;
            if (world == NULL) {
                e->source_exception = true;
                e->exception_class = "java/lang/NullPointerException";
                return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "Spawner publication null World invocation receiver");
            }
            if (!hold_world(s, world))
                return false;
            ++s->error.spawner.insertion_calls;
            bool ordinary;
            const mc_world_storage_status published = mc_world_runtime_insert_entity(
                world, e->candidate, &ordinary, &e->detail.publication);
            if (published != MC_WORLD_STORAGE_OK) {
                capture_failure(s, &e->detail.publication.world_access_failure);
                return wt_cause(s, e->detail.publication.cause);
            }
            if (ordinary)
                ++s->error.spawner.insertion_true;
            else
                ++s->error.spawner.insertion_false;
            e->child = MC_WORLD_SPAWNER_TICK_CHILD_NONE;
            release_world(s);
            /* CY discards the returned boolean456, including normal false. */
            if (!spawn_effects(s, tile))
                return false;
        }
        mc_entity_release(e->candidate);
        e->candidate = NULL;
    }
    ++s->error.spawner.completed;
    mc_world_spawner_tick_error_retire(e);
    s->error.child = MC_WORLD_ENTITY_TICK_CHILD_NONE;
    return true; /* Actual OW.n_612 is the inherited literal return. */
}
