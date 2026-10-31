#include "../gui/loading_screen.h"
#include "application_startup_internal.h"
#include <string.h>

static mc_client_startup_status engine_failure(mc_client_application_startup *startup,
                                               const char *operation) {
    const mc_client_render_engine_error *cause = &startup->failure.engine;
    const bool ge = cause->source_class != NULL && strcmp(cause->source_class, "ge") == 0;
    const char *method = ge && cause->ge_method != NULL ? cause->ge_method : "<init>";
    const char *descriptor = "(Lik;Lkv;)V";
    if (ge && cause->ge_method != NULL) {
        if (cause->ge_method[0] == '<')
            descriptor = "()V";
        else if (cause->ge_method[0] == 'd')
            descriptor = "(I)Ljava/nio/IntBuffer;";
        else
            descriptor = "(I)Ljava/nio/ByteBuffer;";
    }
    return application_startup_fail(
        &startup->failure, MC_CLIENT_STARTUP_RENDER_ENGINE_FAILED, operation, cause->source_class,
        cause->source_class == NULL ? NULL : method,
        cause->source_class == NULL ? NULL : descriptor, cause->source_pc);
}

mc_client_startup_status application_startup_render_engine(mc_client_application_startup *startup) {
    startup->failure.caller =
        (mc_client_application_owner_site){"net/minecraft/client/Minecraft", "a", "()V", 223};
    /* SAME namespace already retained for the actual App.s, not a new context
     * or an inferred proxy. Borrow is protected by the startup App attachment. */
    startup->engine_classes = mc_client_loading_screen_graphics(startup->app->loading_screen);
    if (mc_client_render_engine_classes_active_use(
            startup->engine_classes, &startup->failure.engine) != MC_CLIENT_RENDER_ENGINE_OK)
        return engine_failure(startup, "actual new ji class active use");
    startup->ji_active_use_completed = true;
    /* Cold native provider occurrence only. Actual system/font/FreeType use is
     * deferred by the SAME provider to Graphics.drawString at ji150. */
    startup->failure.default_font = mc_client_default_font_create(
        startup->options.maximum_default_font_bytes, &startup->default_font);
    if (startup->failure.default_font != MC_CLIENT_FONT_OK)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_RENDER_ENGINE_FAILED,
                                        "ji native cold default-font provider allocation", NULL,
                                        NULL, NULL, 0);
    const mc_client_render_engine_status allocated = mc_client_render_engine_allocate(
        startup->engine_classes, startup->default_font, &startup->options.render_engine,
        &startup->engine_candidate);
    if (allocated != MC_CLIENT_RENDER_ENGINE_OK) {
        startup->failure.engine.status = allocated;
        startup->failure.engine.operation = "actual new ji RAW native allocation";
        return engine_failure(startup, "actual new ji RAW allocation");
    }
    /* Actual argument evaluation follows successful class use/new223. */
    mc_client_texture_pack_list *packs = startup->app->D; /* getfield228 */
    mc_game_settings *settings = startup->app->settings;  /* getfield232 */
    startup->failure.caller.pc = 235;
    if (mc_client_render_engine_construct(startup->engine_candidate, packs, settings,
                                          &startup->failure.engine) != MC_CLIENT_RENDER_ENGINE_OK)
        return engine_failure(startup, "actual ji(ik,kv) constructor");
    mc_client_render_engine_state state;
    if (!mc_client_render_engine_inspect(startup->engine_candidate, &state) ||
        state.phase != MC_CLIENT_RENDER_ENGINE_READY)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "actual ji READY publication admission", NULL, NULL, NULL,
                                        0);
    startup->app->render_engine = startup->engine_candidate; /* Actual SAME p238. */
    startup->p_stored = true;
    startup->last_startup_store = 238;
    return application_startup_font(startup);
}
