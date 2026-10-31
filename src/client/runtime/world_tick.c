#include "world_tick.h"

#include "../application_state.h"
#include "../world/render_global_internal.h"
#include "mc/client/particles/effect_renderer.h"
#include "mc/core/math/math.h"

#include <string.h>
#include <xmmintrin.h>

static bool stop(mc_client_world_tick *call, const char *operation, unsigned int pc) {
    call->failed = true;
    call->operation = operation;
    call->site =
        (mc_client_graphics_site){"net/minecraft/client/Minecraft", "k", "()V", (int32_t)pc};
    return false;
}

static bool fields(mc_client_world_tick *call, mc_world_runtime *world,
                   mc_world_runtime_fields *out, unsigned int pc) {
    return mc_world_runtime_fields_get(world, out) == MC_WORLD_STORAGE_OK ||
           stop(call, "actual client World tick field receiver", pc);
}

static bool body(client_application *app, mc_math_classes *math, mc_client_world_tick *call) {
    if (app->active_world == NULL) {
        call->returned = true;
        call->site = (mc_client_graphics_site){"net/minecraft/client/Minecraft", "k", "()V", 1328};
        return true;
    }
    if (app->h != NULL) {
        app->ah = mc_java_i32_add(app->ah, 1);
        call->counter_stored = true;
        if (app->ah == 30) {
            app->ah = 0;
            call->counter_reset = true;
            const mc_world_storage_status status = mc_world_runtime_refresh_entity_begin(
                app->active_world, app->h, &call->refresh, &call->refresh_error);
            if (call->refresh != NULL)
                (void)mc_world_runtime_refresh_entity_inspect(call->refresh, &call->refresh_state);
            if (status != MC_WORLD_STORAGE_OK)
                return stop(call, call->refresh_error.operation, 1119);
        }
    }
    mc_world_runtime *world = app->active_world; /* Actual LHS f1123, before RHS. */
    mc_game_settings_state settings;
    if (!mc_game_settings_read_state(app->settings, &settings))
        return stop(call, "actual client World difficulty Settings receiver", 1130);
    if (mc_world_runtime_client_difficulty_store(world, settings.values.difficulty,
                                                 &call->fields) != MC_WORLD_STORAGE_OK)
        return stop(call, call->fields.operation, 1133);
    call->difficulty_stored = true;
    mc_world_runtime_fields state;
    if (!fields(call, app->active_world, &state, 1140))
        return false;
    if (state.B && mc_world_runtime_client_difficulty_store(app->active_world, 3, &call->fields) !=
                       MC_WORLD_STORAGE_OK)
        return stop(call, call->fields.operation, 1151);
    if (!atomic_load(&app->o)) {
        const mc_client_renderer_tick_status status = mc_client_entity_renderer_tick_begin(
            app->t, math, &call->renderer, &call->renderer_error);
        if (call->renderer != NULL)
            (void)mc_client_entity_renderer_tick_inspect(call->renderer, &call->renderer_state);
        if (status != MC_CLIENT_RENDERER_TICK_OK) {
            stop(call, call->renderer_error.operation, 1165);
            if (call->renderer_error.site.class_name != NULL)
                call->site = call->renderer_error.site;
            return false;
        }
        call->renderer_returned = true;
    }
    if (!atomic_load(&app->o)) {
        mc_client_render_global *global = app->g;
        if (global == NULL)
            return stop(call, "actual RenderGlobal tick null receiver", 1179);
        if (global->active || global->phase == MC_CLIENT_RENDER_GLOBAL_RETIRING)
            return stop(call, "RenderGlobal tick native owner admission", 1179);
        global->active = true;
        global->fields.x = mc_java_i32_add(global->fields.x, 1); /* Actual n.d putfield7. */
        global->active = false;
        call->render_global_returned = true;
    }
    if (!atomic_load(&app->o)) {
        if (mc_world_runtime_client_lightning_decrement(app->active_world, &call->fields) !=
            MC_WORLD_STORAGE_OK)
            return stop(call, call->fields.operation, 1193);
        mc_world_entity_tick_context context = {0};
        context.player.travel.code_registry = app->registry;
        context.player.travel.trig = mc_math_classes_table(math);
        if (mc_world_runtime_tick_entities(app->active_world, &context, NULL, &call->entities) !=
            MC_WORLD_STORAGE_OK)
            return stop(call, call->entities.operation, 1216);
        call->entities_returned = true;
    }
    bool simulation = !atomic_load(&app->o);
    if (!simulation && app->active_world != NULL) {
        if (!fields(call, app->active_world, &state, 1227))
            return false;
        simulation = state.B;
    }
    if (simulation) {
        world = app->active_world; /* Actual LHS f1234 before Settings RHS. */
        if (!mc_game_settings_read_state(app->settings, &settings))
            return stop(call, "actual client spawn Settings receiver", 1241);
        if (mc_world_runtime_client_spawn_flags(world, settings.values.difficulty > 0, true,
                                                &call->fields) != MC_WORLD_STORAGE_OK)
            return stop(call, call->fields.operation, 1253);
        call->spawn_flags_stored = true;
        const mc_world_storage_status status = mc_world_runtime_tick_begin(
            app->active_world, math, &call->scheduler, &call->scheduler_error);
        if (call->scheduler != NULL) {
            (void)mc_world_runtime_tick_inspect(call->scheduler, &call->scheduler_state);
            call->scheduler_error = *call->scheduler_state.error;
        }
        if (status != MC_WORLD_STORAGE_OK) {
            stop(call, call->scheduler_error.operation, 1260);
            if (call->scheduler != NULL)
                call->site = (mc_client_graphics_site){"fd", "l", "()V",
                                                       (int32_t)call->scheduler_error.bytecode};
            return false;
        }
    }
    if (!atomic_load(&app->o) && app->active_world != NULL) {
        mc_world_runtime *ambience_world = app->active_world; /* Captured receiver1278. */
        static const unsigned int read_pcs[] = {1285, 1295, 1305};
        static const unsigned int floor_pcs[] = {1288, 1298, 1308};
        for (size_t axis = 0; axis < 3; ++axis) {
            if (app->h == NULL)
                return stop(call, "actual ambience null Player coordinate receiver",
                            read_pcs[axis]);
            mc_entity_snapshot entity;
            if (mc_entity_get_snapshot(app->h, &entity) != MC_ENTITY_OK)
                return stop(call, "actual ambience Player coordinate fields", read_pcs[axis]);
            if (mc_math_classes_ensure(math, &call->ambience_math) != MC_MATH_CLASS_OK)
                return stop(call, "actual ambience MathHelper active use", floor_pcs[axis]);
            if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
                return stop(call, "ambience native floating environment", floor_pcs[axis]);
            const double value = axis == 0   ? entity.base.x
                                 : axis == 1 ? entity.base.y
                                             : entity.base.z;
            call->ambience_coordinates[axis] = mc_math_floor_double(value);
            ++call->ambience_arguments;
        }
        if (ambience_world == NULL)
            return stop(call, "actual ambience null World receiver", 1311);
        return stop(call, "actual World ambience fd.q body is not connected", 1311);
    }
    if (!atomic_load(&app->o)) {
        if (mc_client_effect_renderer_tick(app->effect_renderer, &call->effects) !=
            MC_EFFECT_RENDERER_OK)
            return stop(call, call->effects.operation, 1325);
    }
    call->returned = true;
    call->site = (mc_client_graphics_site){"net/minecraft/client/Minecraft", "k", "()V", 1328};
    return true;
}

bool mc_client_world_tick_run(client_application *app, mc_math_classes *math,
                              mc_client_world_tick *call) {
    if (app == NULL || call == NULL || call->active || call->failed || call->refresh != NULL ||
        call->renderer != NULL || call->scheduler != NULL)
        return false;
    memset(call, 0, sizeof(*call));
    call->active = true;
    const bool result = body(app, math, call);
    call->active = false;
    return result;
}

bool mc_client_world_tick_cleanup(mc_client_world_tick *call) {
    if (call == NULL || call->active)
        return false;
    mc_world_tick_error scheduler;
    if (mc_world_runtime_tick_retire(&call->scheduler, &scheduler) != MC_WORLD_STORAGE_OK)
        return false;
    call->scheduler_state.error = &call->scheduler_error;
    call->scheduler_state.world = NULL;
    call->scheduler_state.math = NULL;
    call->scheduler_error.weather = NULL;
    call->scheduler_error.lifecycle = NULL;
    call->scheduler_error.scheduled = NULL;
    call->scheduler_error.biome = NULL;
    call->scheduler_error.spawning = NULL;
    call->scheduler_error.mob = NULL;
    call->scheduler_error.eligibility = NULL;
    call->scheduler_error.publication = NULL;
    call->scheduler_state.eligibility = NULL;
    call->scheduler_state.spawn_class = NULL;
    call->scheduler_state.first_constructor.class_literal = NULL;
    call->scheduler_state.spawn_entity = NULL;
    call->scheduler_state.spawn_jockey = NULL;
    call->scheduler_state.spawn_pick.entry = NULL;
    call->scheduler_error.world_access = (mc_world_access_failure){0};
    mc_client_renderer_tick_error renderer;
    if (mc_client_entity_renderer_tick_retire(&call->renderer, &renderer) !=
        MC_CLIENT_RENDERER_TICK_OK)
        return false;
    call->renderer_state.held_item = NULL;
    call->renderer_state.rain = NULL;
    call->renderer_state.failure = &call->renderer_error;
    call->renderer_error.owner = NULL;
    call->renderer_error.math = NULL;
    call->renderer_error.brightness = NULL;
    call->renderer_error.held_item = NULL;
    call->renderer_error.rain = NULL;
    mc_world_refresh_entity_error refresh;
    if (mc_world_runtime_refresh_entity_retire(&call->refresh, &refresh) != MC_WORLD_STORAGE_OK)
        return false;
    call->refresh_state.error = &call->refresh_error;
    call->refresh_state.world = NULL;
    call->refresh_state.entity = NULL;
    call->refresh_error.acquisition = NULL;
    call->refresh_error.publication = NULL;
    return true;
}
