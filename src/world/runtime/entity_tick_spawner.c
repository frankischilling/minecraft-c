#include "../../block/registry/runtime_internal.h"
#include "entity_tick_internal.h"
#include "mc/world/runtime/presentation.h"
#include "tile_entities/tick_internal.h"
#include "tile_entities/tile_entities_internal.h"

static bool world_random(wt_scope *s, mc_tile_entity *tile, unsigned int pc, mc_java_random **out) {
    s->error.spawner_bytecode = pc;
    if (tile->world == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Spawner null World Random field receiver");
    if (tile->world->fields.r == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Spawner null Random invocation receiver");
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
    }
    if (tile->state.spawner.delay > 0) {
        tile->state.spawner.delay = mc_java_i32_sub(tile->state.spawner.delay, 1);
        return true;
    }
    /* The actual name and current World are evaluated before the real factory
     * invocation. There is no successful null lookup or fake mob substitute. */
    s->error.spawner_bytecode = 223;
    return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "Spawner actual jc.a named Entity factory dependency");
}
