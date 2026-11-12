#include "application.h"
#include "../application_state.h"

#include <string.h>

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
    /* The complete movement child belongs in caller storage. This wrapper
     * needs only status when diagnostics are omitted, without a large native
     * automatic aggregate or an additional allocation policy. */
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->layer = out->index = SIZE_MAX;
    }
    mc_effect_renderer_status status;
    if (app == NULL || app->entities == NULL)
        status = MC_EFFECT_RENDERER_INVALID_ARGUMENT;
    else if (app->effect_renderer_constructing)
        status = MC_EFFECT_RENDERER_BUSY;
    else if (app->effect_renderer != NULL || app->effect_renderer_retained != NULL)
        status = MC_EFFECT_RENDERER_INVALID_PHASE;
    else {
        app->effect_renderer_constructing = true;
        status = mc_client_effect_renderer_allocate(app->entities, options,
                                                    &app->effect_renderer_retained, out);
        if (status == MC_EFFECT_RENDERER_OK)
            status = mc_client_effect_renderer_construct(
                app->effect_renderer_retained, app->active_world, app->render_engine, out);
        if (status == MC_EFFECT_RENDERER_OK)
            app->effect_renderer = app->effect_renderer_retained; /* Actual j660. */
        app->effect_renderer_constructing = false;
    }
    if (out != NULL)
        out->status = status;
    return status;
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
