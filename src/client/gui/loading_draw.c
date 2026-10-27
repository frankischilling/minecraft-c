#include "../application_state.h"
#include "../render_engine/nw.h"
#include "loading_internal.h"

mc_client_loading_status mc_client_loading_draw(mc_client_loading_screen *p, int32_t progress,
                                                mc_client_loading_error *error) {
    (void)progress; /* Bar work follows the genuine unfinished ji String lookup. */
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
    (void)tessellator; /* The actual singleton is initialized, not yet begun. */
    mc_client_render_engine *engine = p->fields.b->render_engine; /* App.p160 */
    if (engine == NULL)
        return mc_client_loading_fail(error, MC_CLIENT_LOADING_NULL_REFERENCE,
                                      "LoadingScreen actual ji.b(String) receiver", "a", "(I)V",
                                      165);
    error->child = (mc_client_loading_site){"ji", "b", "(Ljava/lang/String;)I", 0};
    return mc_client_loading_fail(
        error, MC_CLIENT_LOADING_UNIMPLEMENTED_DEPENDENCY,
        "actual selected-Pack/String-cache ji.b lookup for /gui/background.png", "a", "(I)V", 165);
}
