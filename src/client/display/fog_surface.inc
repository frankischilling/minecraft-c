/* Included by the canonical Display translation unit. The source fog state
 * feeds its own draw program; no scene entry, clear, swap or GL error query. */
#include "fog_surface.h"

bool mc_client_display_source_fog_inspect(const mc_client_display *display,
                                          mc_client_source_fog_state *out) {
    if (display == NULL || out == NULL || !display->source.initialized ||
        display->source.active_calls != 0)
        return false;
    *out = display->source.fog;
    out->enabled = display->source.fog_enabled;
    memcpy(out->current_color, display->source.color, sizeof(out->current_color));
    out->current_color_known = display->source.color_known;
    return true;
}

bool mc_client_display_source_fog_color(mc_client_display *display, const float rgba[4],
                                        mc_client_display_error *error) {
    if (rgba == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "source fog color", "null span", 0);
    if (!source_setting_enter(display, "source fog color", error))
        return false;
    for (size_t i = 0; i < 4; ++i)
        display->source.fog.color[i] = rgba[i] < 0 ? 0 : rgba[i] > 1 ? 1 : rgba[i];
    return source_leave(display, true, error);
}

bool mc_client_display_source_normal(mc_client_display *display, float x, float y, float z,
                                     mc_client_display_error *error) {
    if (!source_setting_enter(display, "source current normal", error))
        return false;
    display->source.fog.normal[0] = x;
    display->source.fog.normal[1] = y;
    display->source.fog.normal[2] = z;
    return source_leave(display, true, error);
}

bool mc_client_display_source_fog_i(mc_client_display *display, int32_t pname, int32_t value,
                                    mc_client_display_error *error) {
    if (!source_setting_enter(display, "source integer fog parameter", error))
        return false;
    if (pname == GL_FOG_MODE && (value == GL_LINEAR || value == GL_EXP || value == GL_EXP2))
        display->source.fog.mode = value;
    else if (pname == 34138 && (value == 34139 || value == GL_EYE_PLANE || value == 34140))
        display->source.fog.distance_mode = value;
    else
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                                 "source integer fog parameter", "unimplemented pname/value", 0),
                            error);
    return source_leave(display, true, error);
}

bool mc_client_display_source_fog_f(mc_client_display *display, int32_t pname, float value,
                                    mc_client_display_error *error) {
    if (!source_setting_enter(display, "source float fog parameter", error))
        return false;
    if (pname == GL_FOG_DENSITY && !(value < 0))
        display->source.fog.density = value;
    else if (pname == GL_FOG_START)
        display->source.fog.start = value;
    else if (pname == GL_FOG_END)
        display->source.fog.end = value;
    else
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                                 "source float fog parameter", "unimplemented pname/value", 0),
                            error);
    return source_leave(display, true, error);
}

bool mc_client_display_source_nv_fog_distance(mc_client_display *display, bool *advertised,
                                              mc_client_display_error *error) {
    if (display == NULL || advertised == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "current NV fog capability",
                    "null owner/output", 0);
    if (display->context == NULL || display->window == NULL ||
        SDL_GL_GetCurrentContext() != display->context ||
        SDL_GL_GetCurrentWindow() != display->window)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, "current NV fog capability",
                    "this Display context is not current", 0);
    if (!source_enter(display, "current NV fog capability", error))
        return false;
    mc_client_source_state *state = &display->source;
    if (!display->functions_ready || display->native_context_generation == 0)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                                 "current NV fog capability", "incomplete GL context", 0),
                            error);
    if (!state->fog.nv_capability_known ||
        state->fog_capability_generation != display->native_context_generation) {
        const PFNGLGETSTRINGIPROC get_string_i =
            (PFNGLGETSTRINGIPROC)SDL_GL_GetProcAddress("glGetStringi");
        if (get_string_i == NULL || display->gl.GetIntegerv == NULL)
            return source_leave(display,
                                fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                                     "current NV fog capability", "GL3 extension query unavailable",
                                     0),
                                error);
        GLint count = 0;
        display->gl.GetIntegerv(GL_NUM_EXTENSIONS, &count);
        if (count < 0)
            return source_leave(display,
                                fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                                     "current NV fog capability", "negative extension count", 0),
                                error);
        bool present = false;
        for (GLint i = 0; i < count; ++i) {
            const GLubyte *token = get_string_i(GL_EXTENSIONS, (GLuint)i);
            if (token == NULL)
                return source_leave(display,
                                    fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                                         "current NV fog capability", "null extension token", 0),
                                    error);
            if (strcmp((const char *)token, "GL_NV_fog_distance") == 0)
                present = true;
        }
        state->fog.nv_advertised = present;
        state->fog_capability_generation = display->native_context_generation;
        state->fog.nv_capability_known = true;
    }
    *advertised = state->fog.nv_advertised;
    return source_leave(display, true, error);
}

bool mc_client_display_source_fog_enable(mc_client_display *display, bool enabled,
                                         mc_client_display_error *error) {
    if (!source_setting_enter(display, "source fog enable", error))
        return false;
    display->source.fog_enabled = enabled;
    return source_leave(display, true, error);
}

bool mc_client_display_source_color_material_enable(mc_client_display *display, bool enabled,
                                                    mc_client_display_error *error) {
    if (!source_setting_enter(display, "source color material enable", error))
        return false;
    display->source.fog.color_material = enabled;
    return source_leave(display, true, error);
}

bool mc_client_display_source_color_material(mc_client_display *display, int32_t face, int32_t mode,
                                             mc_client_display_error *error) {
    if (!source_setting_enter(display, "source color material parameters", error))
        return false;
    if ((face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) ||
        (mode != GL_AMBIENT && mode != GL_DIFFUSE && mode != GL_SPECULAR && mode != GL_EMISSION &&
         mode != GL_AMBIENT_AND_DIFFUSE))
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL,
                                 "source color material parameters", "unimplemented face/mode", 0),
                            error);
    display->source.fog.material_face = face;
    display->source.fog.material_mode = mode;
    return source_leave(display, true, error);
}

bool mc_client_display_source_light_enable(mc_client_display *display, unsigned int light,
                                           bool enabled, mc_client_display_error *error) {
    if (!source_setting_enter(display, "source light enable", error))
        return false;
    if (light >= 2)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source light enable",
                                 "unimplemented source light index", 0),
                            error);
    display->source.fog.lights[light] = enabled;
    return source_leave(display, true, error);
}

bool mc_client_display_source_depth_mask(mc_client_display *display, bool enabled,
                                         mc_client_display_error *error) {
    if (!source_setting_enter(display, "source depth write mask", error))
        return false;
    if (display->gl.DepthMask == NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source depth write mask",
                                 "actual GL function unavailable", 0),
                            error);
    display->gl.DepthMask(enabled ? GL_TRUE : GL_FALSE);
    return source_leave(display, true, error);
}

bool mc_client_display_source_blend_enable(mc_client_display *display, bool enabled,
                                           mc_client_display_error *error) {
    if (!source_setting_enter(display, "source blend enable", error))
        return false;
    if ((enabled && display->gl.Enable == NULL) || (!enabled && display->gl.Disable == NULL))
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source blend enable",
                                 "actual GL function unavailable", 0),
                            error);
    if (enabled)
        display->gl.Enable(GL_BLEND);
    else
        display->gl.Disable(GL_BLEND);
    return source_leave(display, true, error);
}

bool mc_client_display_source_blend_function(mc_client_display *display, int32_t source,
                                             int32_t destination, mc_client_display_error *error) {
    if (!source_setting_enter(display, "source blend function", error))
        return false;
    if (source != GL_SRC_ALPHA ||
        (destination != GL_ONE_MINUS_SRC_ALPHA && destination != GL_ONE) ||
        display->gl.BlendFunc == NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source blend function",
                                 "unimplemented blend pair or missing GL function", 0),
                            error);
    display->gl.BlendFunc((GLenum)source, (GLenum)destination);
    return source_leave(display, true, error);
}
