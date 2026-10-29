#include "source_world_list_surface.h"

bool mc_client_display_world_list_blend(mc_client_display *display, bool enabled,
                                        mc_client_display_error *error) {
    if (!source_setting_enter(display, "lg source blend enable", error))
        return false;
    if ((enabled && display->gl.Enable == NULL) || (!enabled && display->gl.Disable == NULL))
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "lg source blend enable",
                                 "actual GL function unavailable", 0),
                            error);
    if (enabled)
        display->gl.Enable(GL_BLEND);
    else
        display->gl.Disable(GL_BLEND);
    return source_leave(display, true, error);
}

bool mc_client_display_world_list_blend_function(mc_client_display *display,
                                                 mc_client_display_error *error) {
    if (!source_setting_enter(display, "lg source glBlendFunc", error))
        return false;
    if (display->gl.BlendFunc == NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "lg source glBlendFunc",
                                 "actual GL function unavailable", 0),
                            error);
    display->gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return source_leave(display, true, error);
}

bool mc_client_display_world_list_flat(mc_client_display *display, mc_client_display_error *error) {
    if (!source_setting_enter(display, "lg source GL_FLAT", error))
        return false;
    display->source.shade_flat = true;
    return source_leave(display, true, error);
}
