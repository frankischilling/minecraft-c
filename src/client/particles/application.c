#include "application.h"
#include "../application_state.h"

mc_effect_renderer_status mc_client_application_publish_world(client_application *app,
                                                              mc_world_runtime *world) {
    if (app == NULL || app->profile == NULL)
        return MC_EFFECT_RENDERER_INVALID_ARGUMENT;
    if (app->effect_renderer_constructing)
        return MC_EFFECT_RENDERER_BUSY;
    if (world != NULL && mc_world_runtime_random_profile(world) != app->profile)
        return MC_EFFECT_RENDERER_INVALID_ARGUMENT;
    if (world != NULL && !mc_world_runtime_binding_retain(world))
        return MC_EFFECT_RENDERER_WORLD_FAILED;
    mc_world_runtime *old = app->active_world;
    app->active_world = world;
    mc_world_runtime_binding_release(old);
    return MC_EFFECT_RENDERER_OK;
}

mc_effect_renderer_status
mc_client_application_construct_effect_renderer(client_application *app,
                                                const mc_effect_renderer_options *options,
                                                mc_effect_renderer_error *out) {
    mc_effect_renderer_error e = {.layer = SIZE_MAX, .index = SIZE_MAX};
    if (app == NULL || app->entities == NULL)
        e.status = MC_EFFECT_RENDERER_INVALID_ARGUMENT;
    else if (app->effect_renderer_constructing)
        e.status = MC_EFFECT_RENDERER_BUSY;
    else if (app->effect_renderer != NULL || app->effect_renderer_retained != NULL)
        e.status = MC_EFFECT_RENDERER_INVALID_PHASE;
    else {
        app->effect_renderer_constructing = true;
        e.status = mc_client_effect_renderer_allocate(app->entities, options,
                                                      &app->effect_renderer_retained, &e);
        if (e.status == MC_EFFECT_RENDERER_OK)
            e.status = mc_client_effect_renderer_construct(
                app->effect_renderer_retained, app->active_world, app->render_engine, &e);
        if (e.status == MC_EFFECT_RENDERER_OK)
            app->effect_renderer = app->effect_renderer_retained; /* Actual j660. */
        app->effect_renderer_constructing = false;
    }
    if (out != NULL)
        *out = e;
    return e.status;
}

mc_effect_renderer_status mc_client_application_retire_effect_renderer(client_application *app) {
    if (app == NULL)
        return MC_EFFECT_RENDERER_INVALID_ARGUMENT;
    if (app->effect_renderer_constructing)
        return MC_EFFECT_RENDERER_BUSY;
    /* Published j aliases the privately retained owner, never a second hold. */
    const mc_effect_renderer_status status =
        mc_client_effect_renderer_free(&app->effect_renderer_retained);
    if (status == MC_EFFECT_RENDERER_OK)
        app->effect_renderer = NULL;
    return status;
}
