#include "../application_state.h"
#include "../render_engine/font_lease.h"
#include "../render_engine/nw.h"
#include "loading_internal.h"

mc_client_loading_status mc_client_loading_draw(mc_client_loading_screen *p, int32_t progress,
                                                mc_client_loading_error *error) {
    mc_scaled_resolution resolution;
    mc_client_loading_status status =
        mc_client_loading_resolution(p->fields.b, &resolution, error, 74);
    if (status != MC_CLIENT_LOADING_OK) {
        error->site.class_name = "mk";
        error->site.method = "a";
        error->site.descriptor = "(I)V";
        return status;
    }
    status = mc_client_loading_progress_projection(p->fields.b, &resolution, error);
    if (status != MC_CLIENT_LOADING_OK)
        return status;
    if (!mc_client_display_loading_clear_color_depth(p->fields.b->display, &error->display))
        return mc_client_loading_fail(error, MC_CLIENT_LOADING_DISPLAY_FAILED,
                                      "LoadingScreen second source clear", "a", "(I)V", 148);
    /* Actual independent nw active use151 precedes loading nullable App.p160.
     * This uses the native context retained by the real s owner before its
     * constructor, and never derives a class loader from nullable p or q. */
    mc_client_tessellator *tessellator;
    if (!mc_client_nw_active_use(p->classes->graphics,
                                 (mc_client_graphics_site){"mk", "a", "(I)V", 151}, &tessellator,
                                 &error->graphics)) {
        error->child =
            (mc_client_loading_site){error->graphics.site.class_name, error->graphics.site.method,
                                     error->graphics.site.descriptor, error->graphics.site.pc};
        return mc_client_loading_fail(error,
                                      error->graphics.status == MC_CLIENT_GRAPHICS_ALLOCATION_FAILED
                                          ? MC_CLIENT_LOADING_ALLOCATION_FAILED
                                      : error->graphics.status == MC_CLIENT_GRAPHICS_LIMIT
                                          ? MC_CLIENT_LOADING_LIMIT
                                      : error->graphics.status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP
                                          ? MC_CLIENT_LOADING_UNSUPPORTED_FP
                                      : error->graphics.status == MC_CLIENT_GRAPHICS_DISPLAY_FAILED
                                          ? MC_CLIENT_LOADING_DISPLAY_FAILED
                                          : MC_CLIENT_LOADING_UNIMPLEMENTED_DEPENDENCY,
                                      "LoadingScreen actual nw active use", "a", "(I)V", 151);
    }
    client_application *app = p->fields.b;
    if (app == NULL)
        return mc_client_loading_fail(error, MC_CLIENT_LOADING_NULL_REFERENCE,
                                      "LoadingScreen actual App.p receiver", "a", "(I)V", 160);
    mc_client_display *display = app->display; /* Borrowed actual native Display, not a new one. */
    mc_client_render_engine *engine = app->render_engine; /* Captured actual App.p160. */
    if (!mc_client_loading_call_engine(p, engine, 160))
        return error->status;
    const mc_player_string *literal;
    if (!mc_client_loading_background_literal(p, &literal)) /* actual ldc163 before invoke165 */
        return error->status;
    if (engine == NULL)
        return mc_client_loading_fail(error, MC_CLIENT_LOADING_NULL_REFERENCE,
                                      "LoadingScreen actual ji.b(String) receiver", "a", "(I)V",
                                      165);
    if (mc_client_render_engine_class_identity(engine) != p->classes->graphics)
        return mc_client_loading_fail(error, MC_CLIENT_LOADING_INVALID_ARGUMENT,
                                      "LoadingScreen captured Engine native namespace", "a", "(I)V",
                                      165);
    mc_player_string_view key;
    if (!mc_player_string_read(literal, &key))
        return mc_client_loading_fail(error, MC_CLIENT_LOADING_INVALID_ARGUMENT,
                                      "LoadingScreen literal immutable native view", "a", "(I)V",
                                      163);
    mc_client_texture id = 0;
    p->call->lookup_called = true;
    const mc_client_texture_lookup_status lookup = mc_client_render_engine_texture(
        engine, display, NULL, (mc_texture_pack_text){key.units, key.length, false}, &id,
        &p->call->lookup);
    if (lookup != MC_CLIENT_LOOKUP_OK) {
        p->call->lookup_failed = true;
        error->lookup = &p->call->lookup;
        error->child =
            (mc_client_loading_site){p->call->lookup.site.class_name, p->call->lookup.site.method,
                                     p->call->lookup.site.descriptor, p->call->lookup.site.pc};
        const bool unsupported_fp =
            p->call->lookup.buffer.status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP ||
            p->call->lookup.texture.display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP ||
            p->call->lookup.texture.resources.status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP;
        return mc_client_loading_fail(
            error,
            unsupported_fp                                 ? MC_CLIENT_LOADING_UNSUPPORTED_FP
            : lookup == MC_CLIENT_LOOKUP_NULL_REFERENCE    ? MC_CLIENT_LOADING_NULL_REFERENCE
            : lookup == MC_CLIENT_LOOKUP_ALLOCATION_FAILED ? MC_CLIENT_LOADING_ALLOCATION_FAILED
            : lookup == MC_CLIENT_LOOKUP_LIMIT             ? MC_CLIENT_LOADING_LIMIT
            : lookup == MC_CLIENT_LOOKUP_BUSY              ? MC_CLIENT_LOADING_BUSY
            : lookup == MC_CLIENT_LOOKUP_UNIMPLEMENTED ? MC_CLIENT_LOADING_UNIMPLEMENTED_DEPENDENCY
                                                       : MC_CLIENT_LOADING_TEXTURE_FAILED,
            "LoadingScreen actual selected-Pack ji.b child", "a", "(I)V", 165);
    }
    return mc_client_loading_draw_body(p, tessellator, display, &resolution, progress, id, error);
}
