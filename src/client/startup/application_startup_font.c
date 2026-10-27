#include "application_owner_internal.h"
#include "application_startup_internal.h"

#define MC_CLASS "net/minecraft/client/Minecraft"

/* Actual MC ldc250 is cached only in its already-live native class namespace.
 * This is a scoped literal identity, not a global interning implementation. */
static mc_client_startup_status font_resource(mc_client_application_classes *classes,
                                              mc_player_string **out,
                                              mc_client_startup_error *error) {
    if (mtx_lock(&classes->mutex) != thrd_success)
        return application_startup_fail(error, MC_CLIENT_STARTUP_OWNER_FAILED,
                                        "startup font literal native namespace lock", NULL, NULL,
                                        NULL, 0);
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return application_startup_fail(error, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "startup font literal live namespace", NULL, NULL, NULL, 0);
    }
    if (classes->startup_font_resource == NULL) {
        static const uint16_t units[] = {'/', 'f', 'o', 'n', 't', '/', 'd', 'e', 'f',
                                         'a', 'u', 'l', 't', '.', 'p', 'n', 'g'};
        error->player_string = mc_player_string_create(units, sizeof(units) / sizeof(units[0]),
                                                       &classes->startup_font_resource);
        if (error->player_string != MC_PLAYER_OK) {
            (void)mtx_unlock(&classes->mutex);
            return application_startup_fail(error, MC_CLIENT_STARTUP_STRING_FAILED,
                                            "actual font resource ldc", MC_CLASS, "a", "()V", 250);
        }
    }
    *out = classes->startup_font_resource;
    (void)mtx_unlock(&classes->mutex);
    return MC_CLIENT_STARTUP_OK;
}

static mc_client_startup_status font_failure(mc_client_application_startup *startup,
                                             const char *operation) {
    const mc_client_bitmap_font_error *cause = &startup->failure.bitmap_font;
    return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_BITMAP_FONT_FAILED,
                                    operation, cause->method == NULL ? NULL : "sj", cause->method,
                                    cause->descriptor,
                                    cause->pc < 0 ? 0u : (unsigned int)cause->pc);
}

mc_client_startup_status application_startup_font(mc_client_application_startup *startup) {
    client_application *app = startup->app;
    startup->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 242};
    /* sj has no class initializer. Allocation retains only the SAME cold GUI
     * namespace already owned by actual App.s; no fp/context/resource active use. */
    startup->font_classes = mc_client_loading_screen_classes(app->loading_screen);
    const mc_client_bitmap_font_status allocated = mc_client_bitmap_font_allocate(
        startup->font_classes, &startup->options.bitmap_font, &app->font_renderer_retained);
    if (allocated != MC_CLIENT_BITMAP_FONT_OK) {
        startup->failure.bitmap_font.status = allocated;
        startup->failure.bitmap_font.pc = -1;
        startup->failure.bitmap_font.native_policy = true;
        return font_failure(startup, "actual new sj RAW native allocation");
    }
    startup->font_candidate = app->font_renderer_retained;
    mc_game_settings *settings = app->settings; /* Actual getfield247, after new242. */
    startup->failure.caller.pc = 250;
    if (font_resource(app->source_namespace, &startup->font_resource, &startup->failure) !=
        MC_CLIENT_STARTUP_OK)
        return startup->failure.status;
    mc_client_render_engine *engine = app->render_engine; /* Actual getfield253. */
    startup->failure.caller.pc = 256;
    /* Native class-loader prerequisite, on the same provider already borrowed
     * by this runtime. Binding allocates the cold fp owner but does not read it. */
    if (mc_client_loading_classes_bind_resources(startup->font_classes,
                                                 startup->services.texture_resources,
                                                 &startup->failure.gui) != MC_CLIENT_LOADING_OK)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_BITMAP_FONT_FAILED,
                                        "sj same-provider cold GUI resource binding", NULL, NULL,
                                        NULL, 0);
    app->font_renderer_constructing = true;
    const mc_client_bitmap_font_status constructed = mc_client_bitmap_font_construct(
        app->font_renderer_retained, settings, startup->font_resource, engine, startup->display,
        &startup->failure.bitmap_font);
    app->font_renderer_constructing = false;
    if (constructed != MC_CLIENT_BITMAP_FONT_OK)
        return font_failure(startup, "actual sj(kv,String,ji) constructor");
    mc_client_bitmap_font_state state;
    if (!mc_client_bitmap_font_inspect(app->font_renderer_retained, &state) ||
        state.phase != MC_CLIENT_BITMAP_FONT_READY)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "actual sj READY publication admission", NULL, NULL, NULL,
                                        0);
    app->font_renderer = app->font_renderer_retained; /* Actual SAME q259. */
    startup->q_stored = true;
    startup->last_startup_store = 259;
    return application_startup_colors(startup);
}
