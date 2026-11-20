#include "application_startup_rendering.h"
#include "../display/source_surface.h"
#include "application_startup_effects.h"
#include "application_startup_internal.h"
#include "mc/world/runtime/runtime.h"

#include <stdlib.h>

#define MC_CLASS "net/minecraft/client/Minecraft"

static mc_client_startup_status fail(mc_client_application_startup *s,
                                     mc_client_startup_status status, const char *operation,
                                     const char *child, const char *method, const char *descriptor,
                                     unsigned int pc) {
    s->rendering->failed_pc = s->failure.caller.pc;
    return application_startup_fail(&s->failure, status, operation, child, method, descriptor, pc);
}

static bool capture_engine(mc_client_render_engine *engine, mc_client_render_engine **slot,
                           mc_client_render_engine_status *status) {
    if (engine == NULL)
        return true;
    *status = mc_client_render_engine_retain(engine);
    if (*status != MC_CLIENT_RENDER_ENGINE_OK)
        return false;
    *slot = engine;
    return true;
}

static void snapshot_global(mc_client_startup_rendering_call *call) {
    mc_client_render_global_state state;
    if (mc_client_render_global_inspect(call->global, &state)) {
        call->global_last_store = state.last_store_pc;
        call->global_constructor_pc = state.last_constructor_pc;
        call->global_star_vertices = state.star_vertices;
    }
}

static mc_client_startup_status global(mc_client_application_startup *s) {
    mc_client_startup_rendering_call *call = s->rendering;
    s->failure.caller.pc = 616;
    if (mc_client_render_global_allocate(s->engine_classes, &s->options.render_global,
                                         &call->global,
                                         &call->global_error) != MC_CLIENT_RENDER_GLOBAL_OK)
        return fail(s, MC_CLIENT_STARTUP_RENDER_GLOBAL_FAILED, call->global_error.operation, NULL,
                    NULL, NULL, 0);
    call->global_allocated = true;
    call->last_pc = 616;
    s->failure.caller.pc = 622; /* NEW616 precedes live p argument read622. */
    if (!capture_engine(s->app->render_engine, &call->global_engine, &call->global_hold))
        return fail(s, MC_CLIENT_STARTUP_RENDER_ENGINE_FAILED,
                    "native captured n constructor Engine occurrence", NULL, NULL, NULL, 0);
    s->failure.caller.pc = 625;
    const mc_client_render_global_status result = mc_client_render_global_construct(
        call->global, s->app, call->global_engine, s->display, &call->global_error);
    snapshot_global(call); /* Only scalar history; no borrowed arrays survive consumption. */
    if (result != MC_CLIENT_RENDER_GLOBAL_OK)
        return fail(s, MC_CLIENT_STARTUP_RENDER_GLOBAL_FAILED, call->global_error.operation,
                    call->global_error.source_class, call->global_error.source_method,
                    call->global_error.source_descriptor, call->global_error.source_pc);
    call->global_constructor_returned = true;
    call->last_pc = 625;
    s->failure.caller.pc = 628;
    if (s->app->g != NULL)
        return fail(s, MC_CLIENT_STARTUP_INVALID_PHASE, "native foreign g publication guard", NULL,
                    NULL, NULL, 0);
    s->app->g = call->global; /* Actual source g628, same owned candidate. */
    call->g_stored = true;
    call->last_pc = 628;
    s->last_startup_store = 628;
    return MC_CLIENT_STARTUP_OK;
}

static mc_client_startup_status viewport(mc_client_application_startup *s) {
    mc_client_startup_rendering_call *call = s->rendering;
    call->viewport_width = s->app->d; /* Actual independent d634 then e638 reads. */
    call->viewport_height = s->app->e;
    s->failure.caller.pc = 641;
    if (!mc_client_display_source_viewport(s->display, call->viewport_width, call->viewport_height,
                                           &call->viewport_error))
        return fail(s, MC_CLIENT_STARTUP_DISPLAY_FAILED, call->viewport_error.operation,
                    "org/lwjgl/opengl/GL11", "glViewport", "(IIII)V", 0);
    call->viewport_returned = true;
    call->last_pc = 641;
    return MC_CLIENT_STARTUP_OK;
}

static mc_client_startup_status particles(mc_client_application_startup *s) {
    mc_client_startup_rendering_call *call = s->rendering;
    client_application *app = s->app;
    s->failure.caller.pc = 645;
    if (app->effect_renderer_constructing || app->effect_renderer != NULL ||
        app->effect_renderer_retained != NULL)
        return fail(s, MC_CLIENT_STARTUP_INVALID_PHASE,
                    "native foreign j candidate/publication guard", NULL, NULL, NULL, 0);
    app->effect_renderer_constructing = true;
    call->particle_attempted = true;
    mc_client_startup_status status = MC_CLIENT_STARTUP_OK;
    if (mc_client_effect_renderer_allocate(app->entities, &s->options.particles,
                                           &app->effect_renderer_retained,
                                           &call->particle_error) != MC_EFFECT_RENDERER_OK) {
        status = fail(s, MC_CLIENT_STARTUP_PARTICLES_FAILED, call->particle_error.operation, NULL,
                      NULL, NULL, 0);
        goto done;
    }
    call->particle = app->effect_renderer_retained;
    call->particle_allocated = true;
    call->last_pc = 645;
    s->failure.caller.pc = 650;
    mc_world_runtime *world = app->active_world; /* Actual f650, not an inspection session. */
    if (world != NULL) {
        if (!mc_world_runtime_binding_retain(world)) {
            status = fail(s, MC_CLIENT_STARTUP_PARTICLES_FAILED,
                          "native captured dn World argument occurrence", NULL, NULL, NULL, 0);
            goto done;
        }
        call->world_held = true;
        call->particle_world = world;
    }
    s->failure.caller.pc = 654;
    if (!capture_engine(app->render_engine, &call->particle_engine, &call->particle_hold)) {
        status = fail(s, MC_CLIENT_STARTUP_RENDER_ENGINE_FAILED,
                      "native captured dn constructor Engine occurrence", NULL, NULL, NULL, 0);
        goto done;
    }
    s->failure.caller.pc = 657;
    const mc_effect_renderer_status result = mc_client_effect_renderer_construct(
        call->particle, call->particle_world, call->particle_engine, &call->particle_error);
    mc_effect_renderer_snapshot state;
    if (mc_client_effect_renderer_read(call->particle, &state))
        call->particle_constructor_pc = state.constructor_pc;
    if (result != MC_EFFECT_RENDERER_OK) {
        status = fail(s, MC_CLIENT_STARTUP_PARTICLES_FAILED, call->particle_error.operation, "dn",
                      call->particle_error.source_method, call->particle_error.source_descriptor,
                      call->particle_error.source_pc);
        goto done;
    }
    call->particle_constructor_returned = true;
    call->last_pc = 657;
    s->failure.caller.pc = 660;
    app->effect_renderer = app->effect_renderer_retained; /* Actual source j660. */
    call->j_stored = true;
    call->last_pc = 660;
    s->last_startup_store = 660;
done:
    app->effect_renderer_constructing = false;
    return status;
}

mc_client_startup_status application_startup_rendering(mc_client_application_startup *s) {
    if (s == NULL || s->app == NULL)
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    if (s->rendering != NULL)
        return s->rendering->active ? MC_CLIENT_STARTUP_BUSY : MC_CLIENT_STARTUP_INVALID_PHASE;
    if (s->app->startup != s || s->app->constructor_phase != MC_CLIENT_APP_READY ||
        s->phase == MC_CLIENT_STARTUP_RETIRING || s->effects == NULL ||
        s->effects->registrations_completed != 9 || s->effects->last_pc != 612)
        return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "actual startup completed effect prefix required", NULL,
                                        NULL, NULL, 0);
    s->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 616};
    mc_client_startup_rendering_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                                        "native retained rendering invocation storage", NULL, NULL,
                                        NULL, 0);
    s->rendering = call;
    s->failure.rendering = call;
    const bool outer_active = s->active;
    s->active = call->active = call->entered = true;
    mc_client_startup_status status = global(s);
    if (status == MC_CLIENT_STARTUP_OK)
        status = viewport(s);
    if (status == MC_CLIENT_STARTUP_OK)
        status = particles(s);
    if (status == MC_CLIENT_STARTUP_OK) {
        call->calls_completed = true;
        status = application_startup_resources(s);
    }
    call->active = false;
    s->active = outer_active;
    return status;
}

static mc_client_startup_status drop_engine(mc_client_render_engine **slot,
                                            mc_client_render_engine_status *status) {
    if (*slot == NULL)
        return MC_CLIENT_STARTUP_OK;
    *status = mc_client_render_engine_release(*slot);
    if (*status != MC_CLIENT_RENDER_ENGINE_OK)
        return *status == MC_CLIENT_RENDER_ENGINE_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                       : MC_CLIENT_STARTUP_RENDER_ENGINE_FAILED;
    *slot = NULL;
    return MC_CLIENT_STARTUP_OK;
}

mc_client_startup_status application_startup_rendering_retire(mc_client_application_startup *s) {
    mc_client_startup_rendering_call *call = s->rendering;
    s->retirement.rendering = call;
    client_application *app = s->app;
    if (app->effect_renderer_constructing || (call != NULL && call->active))
        return MC_CLIENT_STARTUP_BUSY;
    if (call == NULL)
        return app->effect_renderer != NULL || app->effect_renderer_retained != NULL
                   ? MC_CLIENT_STARTUP_BUSY
                   : MC_CLIENT_STARTUP_OK;
    if (call->retired)
        return app->effect_renderer != NULL || app->effect_renderer_retained != NULL
                   ? MC_CLIENT_STARTUP_BUSY
                   : MC_CLIENT_STARTUP_OK;
    if ((call->g_stored ? app->g != call->global : app->g != NULL) ||
        app->effect_renderer_retained != call->particle ||
        (call->j_stored ? app->effect_renderer != call->particle : app->effect_renderer != NULL))
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    /* Preflight the external RG occurrence BEFORE consuming dn or any earlier
     * App child. The strict leaf repeats this check at actual retirement. */
    if (call->global != NULL) {
        mc_client_render_global_state state;
        if (!mc_client_render_global_inspect(call->global, &state) || state.holds != 1 ||
            state.phase == MC_CLIENT_RENDER_GLOBAL_CONSTRUCTING)
            return MC_CLIENT_STARTUP_BUSY;
    }
    call->active = true;
    mc_client_startup_status status = MC_CLIENT_STARTUP_OK;
    call->particle_cleanup = mc_client_effect_renderer_free(&app->effect_renderer_retained);
    if (app->effect_renderer_retained == NULL) {
        call->particle = NULL;
        if (call->j_stored) {
            app->effect_renderer = NULL; /* Native view ended, no source putfield. */
            call->j_stored = false;
        }
    }
    if (call->particle_cleanup != MC_EFFECT_RENDERER_OK) {
        status = call->particle_cleanup == MC_EFFECT_RENDERER_BUSY
                     ? MC_CLIENT_STARTUP_BUSY
                     : MC_CLIENT_STARTUP_PARTICLES_FAILED;
        goto done;
    }
    if (call->world_held) {
        mc_world_runtime *world = call->particle_world;
        call->particle_world = NULL;
        call->world_held = false;
        mc_world_runtime_binding_release(world);
    }
    status = drop_engine(&call->particle_engine, &call->particle_release);
    if (status != MC_CLIENT_STARTUP_OK)
        goto done;
    const mc_client_render_global_status retired =
        mc_client_render_global_retire(&call->global, &call->global_cleanup);
    if (call->global == NULL && call->g_stored) {
        app->g = NULL; /* Native source view ended, not source g=null. */
        call->g_stored = false;
    }
    if (retired != MC_CLIENT_RENDER_GLOBAL_OK) {
        status = retired == MC_CLIENT_RENDER_GLOBAL_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                         : MC_CLIENT_STARTUP_RENDER_GLOBAL_FAILED;
        goto done;
    }
    status = drop_engine(&call->global_engine, &call->global_release);
    if (status == MC_CLIENT_STARTUP_OK)
        call->retired = true;
done:
    call->active = false;
    return status;
}
