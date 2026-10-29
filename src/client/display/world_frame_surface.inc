/* Included by the canonical Display translation unit after source_surface.inc. */
#include "world_frame_surface.h"

bool mc_client_display_source_cull_test(mc_client_display *display, bool enabled,
                                        mc_client_display_error *error) {
    if (!source_setting_enter(display, "source World cull enable", error))
        return false;
    if ((enabled && display->gl.Enable == NULL) || (!enabled && display->gl.Disable == NULL))
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                                 "source World cull enable",
                                 "actual GL function address unavailable", 0),
                            error);
    if (enabled)
        display->gl.Enable(GL_CULL_FACE);
    else
        display->gl.Disable(GL_CULL_FACE);
    return source_leave(display, true, error);
}

bool mc_client_display_source_color_mask(mc_client_display *display, bool red, bool green,
                                         bool blue, bool alpha, mc_client_display_error *error) {
    if (!source_setting_enter(display, "source World color mask", error))
        return false;
    typedef void(GLAPIENTRY * color_mask_function)(GLboolean, GLboolean, GLboolean, GLboolean);
    SDL_FunctionPointer address = SDL_GL_GetProcAddress("glColorMask");
    color_mask_function color_mask;
    _Static_assert(sizeof(color_mask) == sizeof(address), "desktop GL function representation");
    memcpy(&color_mask, &address, sizeof(color_mask));
    if (color_mask == NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source World color mask",
                                 "actual GL function address unavailable", 0),
                            error);
    color_mask(red ? GL_TRUE : GL_FALSE, green ? GL_TRUE : GL_FALSE, blue ? GL_TRUE : GL_FALSE,
               alpha ? GL_TRUE : GL_FALSE);
    return source_leave(display, true, error);
}
