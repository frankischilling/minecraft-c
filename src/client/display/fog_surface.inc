/* Included by the canonical Display translation unit. The source fog state
 * feeds its own draw program; no scene entry, clear, swap or GL error query. */
#include "fog_surface.h"
#include "source_lighting_state.h"

bool mc_client_display_source_fog_inspect(const mc_client_display *display,
                                          mc_client_source_fog_state *out) {
    if (display == NULL || out == NULL || !display->source.initialized ||
        display->source.active_calls != 0)
        return false;
    *out = display->source.fog;
    out->enabled = display->source.fog_enabled;
    out->lighting_enabled = display->source.lighting_enabled;
    out->matrix_mode = display->source.matrix_mode;
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
    float color[4];
    for (size_t i = 0; i < 4; ++i)
        color[i] = rgba[i] < 0 ? 0 : rgba[i] > 1 ? 1 : rgba[i];
    if (!source_material_vector_equal(display->source.fog.color, color, 4))
        source_material_vertices(&display->source);
    memcpy(display->source.fog.color, color, sizeof(color));
    return source_leave(display, true, error);
}

bool mc_client_display_source_normal(mc_client_display *display, float x, float y, float z,
                                     mc_client_display_error *error) {
    if (!source_setting_enter(display, "source current normal", error))
        return false;
    if (!source_material_profile_admit(display, false, "source current normal", error))
        return source_leave(display, false, error);
    const float normal[3] = {x, y, z};
    source_material_stage_normal(&display->source, normal);
    return source_leave(display, true, error);
}

bool mc_client_display_source_normalize(mc_client_display *display, bool enabled,
                                        mc_client_display_error *error) {
    if (!source_setting_enter(display, "source normal normalization enable", error))
        return false;
    if (display->source.fog.normalize_enabled != enabled)
        source_material_vertices(&display->source);
    display->source.fog.normalize_enabled = enabled;
    return source_leave(display, true, error);
}

bool mc_client_display_source_rescale_normal(mc_client_display *display, bool enabled,
                                             mc_client_display_error *error) {
    if (!source_setting_enter(display, "source normal rescaling enable", error))
        return false;
    if (display->source.fog.rescale_normal_enabled != enabled)
        source_material_vertices(&display->source);
    display->source.fog.rescale_normal_enabled = enabled;
    return source_leave(display, true, error);
}

bool mc_client_display_source_fog_i(mc_client_display *display, int32_t pname, int32_t value,
                                    mc_client_display_error *error) {
    if (!source_setting_enter(display, "source integer fog parameter", error))
        return false;
    if (pname == GL_FOG_MODE && (value == GL_LINEAR || value == GL_EXP || value == GL_EXP2)) {
        if (display->source.fog.mode != value)
            source_material_vertices(&display->source);
        display->source.fog.mode = value;
    } else if (pname == 34138 && (value == 34139 || value == GL_EYE_PLANE || value == 34140)) {
        if (display->source.fog.distance_mode != value)
            source_material_vertices(&display->source);
        display->source.fog.distance_mode = value;
    } else
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
    if (pname == GL_FOG_DENSITY && !(value < 0)) {
        if (!source_material_float_equal(display->source.fog.density, value))
            source_material_vertices(&display->source);
        display->source.fog.density = value;
    } else if (pname == GL_FOG_START) {
        if (!source_material_float_equal(display->source.fog.start, value))
            source_material_vertices(&display->source);
        display->source.fog.start = value;
    } else if (pname == GL_FOG_END) {
        if (!source_material_float_equal(display->source.fog.end, value))
            source_material_vertices(&display->source);
        display->source.fog.end = value;
    } else
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
    if (display->source.fog_enabled != enabled)
        source_material_vertices(&display->source);
    display->source.fog_enabled = enabled;
    return source_leave(display, true, error);
}

bool mc_client_display_source_color_material_enable(mc_client_display *display, bool enabled,
                                                    mc_client_display_error *error) {
    if (!source_setting_enter(display, "source color material enable", error))
        return false;
    if (display->source.fog.color_material == enabled)
        return source_leave(display, true, error);
    if (!source_material_profile_admit(display, enabled, "source color material enable", error))
        return source_leave(display, false, error);
    source_material_vertices(&display->source);
    source_material_current(&display->source);
    display->source.fog.color_material = enabled;
    if (enabled)
        source_material_color(&display->source.fog, display->source.color,
                              display->source.color_known);
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
    if (display->source.fog.material_face == face && display->source.fog.material_mode == mode)
        return source_leave(display, true, error);
    if (!source_material_profile_admit(display, false, "source color material parameters", error))
        return source_leave(display, false, error);
    source_material_vertices(&display->source);
    display->source.fog.material_face = face;
    display->source.fog.material_mode = mode;
    if (display->source.fog.color_material) {
        source_material_current(&display->source);
        source_material_color(&display->source.fog, display->source.color,
                              display->source.color_known);
    }
    return source_leave(display, true, error);
}

bool mc_client_display_source_material(mc_client_display *display, int32_t face, int32_t pname,
                                       const void *storage, size_t remaining,
                                       mc_client_display_error *error) {
    const char *operation = "source material parameter";
    if (!source_setting_enter(display, operation, error))
        return false;
    if (storage == NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation,
                                 "null remaining FloatBuffer storage", 0),
                            error);
    if (remaining < 4)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_BUFFER_TOO_SMALL, operation,
                                 "FloatBuffer requires at least four remaining floats", 0),
                            error);
    if ((face != GL_FRONT && face != GL_BACK && face != GL_FRONT_AND_BACK) ||
        (pname != GL_AMBIENT && pname != GL_DIFFUSE && pname != GL_SPECULAR &&
         pname != GL_EMISSION && pname != GL_AMBIENT_AND_DIFFUSE && pname != GL_SHININESS &&
         pname != GL_COLOR_INDEXES))
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                 "unimplemented source material face or parameter", 0),
                            error);
    float values[4];
    memcpy(values, storage, sizeof(values));
    if (pname == GL_SHININESS) {
        if (!source_fp_admit("source material shininess validation FP", error))
            return source_leave(display, false, error);
        if (!(values[0] >= 0 && values[0] <= 128))
            return source_leave(display,
                                fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                     "source shininess outside the supported range0..128", 0),
                                error);
    }
    if (!source_material_profile_admit(display, false, operation, error))
        return source_leave(display, false, error);
    for (size_t side = 0; side < 2; ++side)
        if (face == GL_FRONT_AND_BACK || face == (side == 0 ? GL_FRONT : GL_BACK)) {
            if (pname == GL_AMBIENT_AND_DIFFUSE) {
                source_material_assign(&display->source, side, GL_AMBIENT, values);
                source_material_assign(&display->source, side, GL_DIFFUSE, values);
            } else
                source_material_assign(&display->source, side, pname, values);
        }
    return source_leave(display, true, error);
}

bool mc_client_display_source_material_get(mc_client_display *display, int32_t face, int32_t pname,
                                           float *values, size_t capacity,
                                           mc_client_display_error *error) {
    const char *operation = "source material query";
    if (values == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation, "null output span", 0);
    if (!source_enter(display, operation, error))
        return false;
    const int property = source_material_property(pname);
    if (property >= 0) {
        source_material_field field =
            source_material_field_for(&display->source.fog.materials[0], (unsigned int)property);
        if (capacity < field.width)
            return source_leave(display,
                                fail(error, MC_CLIENT_DISPLAY_BUFFER_TOO_SMALL, operation,
                                     "output requires the selected material property width", 0),
                                error);
    }
    if (!source_material_profile_admit(display, false, operation, error))
        return source_leave(display, false, error);
    /* GetMaterial reaches these source barriers before enum validation. */
    source_material_vertices(&display->source);
    source_material_current(&display->source);
    if ((face != GL_FRONT && face != GL_BACK) || property < 0)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                 "unimplemented source material query face or property", 0),
                            error);
    const size_t side = face == GL_FRONT ? 0u : 1u;
    source_material_field field =
        source_material_field_for(&display->source.fog.materials[side], (unsigned int)property);
    if (!*field.known)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                 "unspecified material after prior array draw", 0),
                            error);
    memcpy(values, field.values, field.width * sizeof(float));
    return source_leave(display, true, error);
}

bool mc_client_display_source_current_attribute(mc_client_display *display, int32_t pname,
                                                float *values, size_t capacity,
                                                mc_client_display_error *error) {
    const char *operation = "source current attribute query";
    if (values == NULL)
        return fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation, "null output span", 0);
    if (!source_enter(display, operation, error))
        return false;
    if (pname != GL_CURRENT_COLOR && pname != GL_CURRENT_NORMAL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                 "unimplemented source current attribute", 0),
                            error);
    const size_t width = pname == GL_CURRENT_COLOR ? 4u : 3u;
    if (capacity < width)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_BUFFER_TOO_SMALL, operation,
                                 "output requires the selected current attribute width", 0),
                            error);
    if (!source_material_profile_admit(display, false, operation, error))
        return source_leave(display, false, error);
    source_material_current(&display->source);
    const bool known =
        pname == GL_CURRENT_COLOR ? display->source.color_known : display->source.fog.normal_known;
    if (!known)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                 "unspecified current attribute after prior array draw", 0),
                            error);
    const float *source =
        pname == GL_CURRENT_COLOR ? display->source.color : display->source.fog.normal;
    memcpy(values, source, width * sizeof(float));
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
    if (display->source.fog.lights[light] != enabled)
        source_material_vertices(&display->source);
    display->source.fog.lights[light] = enabled;
    return source_leave(display, true, error);
}

bool mc_client_display_source_lighting_enable(mc_client_display *display, bool enabled,
                                              mc_client_display_error *error) {
    if (!source_setting_enter(display, "source lighting enable", error))
        return false;
    if (display->source.lighting_enabled != enabled)
        source_material_vertices(&display->source);
    display->source.lighting_enabled = enabled;
    return source_leave(display, true, error);
}

bool mc_client_display_source_light_parameter(mc_client_display *display, unsigned int light,
                                              int32_t pname, const void *storage, size_t remaining,
                                              mc_client_display_error *error) {
    const char *operation = "source light parameter";
    if (!source_setting_enter(display, operation, error))
        return false;
    if (storage == NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation,
                                 "null remaining FloatBuffer storage", 0),
                            error);
    if (remaining < 4)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_BUFFER_TOO_SMALL, operation,
                                 "FloatBuffer requires at least four remaining floats", 0),
                            error);
    if (light >= 2 || (pname != GL_AMBIENT && pname != GL_DIFFUSE && pname != GL_SPECULAR &&
                       pname != GL_POSITION))
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                 "unimplemented source light index or parameter", 0),
                            error);
    mc_client_source_light_state *parameters = &display->source.fog.light_parameters[light];
    float values[4];
    memcpy(values, storage, sizeof(values));
    if (pname == GL_POSITION) {
        if (!source_fp_admit("source light position transform FP", error))
            return source_leave(display, false, error);
        /* glLight's POSITION always uses MODELVIEW, independently of the
         * currently selected matrix mode. Later matrix changes do not replay it. */
        const float *matrix = display->source.modelview.column_major;
        float eye[4];
        for (size_t row = 0; row < 4; ++row) {
            float value = mc_client_source_product(matrix[row], values[0]);
            for (size_t column = 1; column < 4; ++column)
                value = mc_client_source_sum(
                    value, mc_client_source_product(matrix[column * 4 + row], values[column]));
            eye[row] = value;
        }
        if (!source_material_vector_equal(parameters->position_eye, eye, 4))
            source_material_vertices(&display->source);
        memcpy(parameters->position, values, sizeof(values));
        memcpy(parameters->position_eye, eye, sizeof(eye));
    } else if (pname == GL_AMBIENT) {
        if (!source_material_vector_equal(parameters->ambient, values, 4))
            source_material_vertices(&display->source);
        memcpy(parameters->ambient, values, sizeof(values));
    } else if (pname == GL_DIFFUSE) {
        if (!source_material_vector_equal(parameters->diffuse, values, 4))
            source_material_vertices(&display->source);
        memcpy(parameters->diffuse, values, sizeof(values));
    } else {
        if (!source_material_vector_equal(parameters->specular, values, 4))
            source_material_vertices(&display->source);
        memcpy(parameters->specular, values, sizeof(values));
    }
    return source_leave(display, true, error);
}

bool mc_client_display_source_light_model(mc_client_display *display, int32_t pname,
                                          const void *storage, size_t remaining,
                                          mc_client_display_error *error) {
    const char *operation = "source light model parameter";
    if (!source_setting_enter(display, operation, error))
        return false;
    if (storage == NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_INVALID_ARGUMENT, operation,
                                 "null remaining FloatBuffer storage", 0),
                            error);
    if (remaining < 4)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_BUFFER_TOO_SMALL, operation,
                                 "FloatBuffer requires at least four remaining floats", 0),
                            error);
    if (pname != GL_LIGHT_MODEL_AMBIENT)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                 "unimplemented source light model parameter", 0),
                            error);
    float values[4];
    memcpy(values, storage, sizeof(values));
    if (!source_material_vector_equal(display->source.fog.light_model_ambient, values, 4))
        source_material_vertices(&display->source);
    memcpy(display->source.fog.light_model_ambient, values, sizeof(values));
    return source_leave(display, true, error);
}

bool mc_client_display_source_light_model_i(mc_client_display *display, int32_t pname,
                                            int32_t value, mc_client_display_error *error) {
    const char *operation = "source integer light model parameter";
    if (!source_setting_enter(display, operation, error))
        return false;
    if (pname == GL_LIGHT_MODEL_LOCAL_VIEWER) {
        if (display->source.fog.light_model_local_viewer != (value != 0))
            source_material_vertices(&display->source);
        display->source.fog.light_model_local_viewer = value != 0;
    } else if (pname == GL_LIGHT_MODEL_TWO_SIDE) {
        if (display->source.fog.light_model_two_side != (value != 0))
            source_material_vertices(&display->source);
        display->source.fog.light_model_two_side = value != 0;
    } else if (pname == GL_LIGHT_MODEL_COLOR_CONTROL &&
               (value == GL_SINGLE_COLOR || value == GL_SEPARATE_SPECULAR_COLOR)) {
        if (display->source.fog.light_model_color_control != value)
            source_material_vertices(&display->source);
        display->source.fog.light_model_color_control = value;
    } else
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                                 "unimplemented source light model parameter or value", 0),
                            error);
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
    source_material_core_integer(display, GL_DEPTH_WRITEMASK, enabled ? GL_TRUE : GL_FALSE);
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
    if (!source_material_blend_integer(display, GL_BLEND, enabled ? GL_TRUE : GL_FALSE, error))
        return source_leave(display, false, error);
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
    const bool alpha =
        source == GL_SRC_ALPHA && (destination == GL_ONE_MINUS_SRC_ALPHA || destination == GL_ONE);
    const bool vignette = source == GL_ZERO && destination == GL_ONE_MINUS_SRC_COLOR;
    const bool crosshair =
        source == GL_ONE_MINUS_DST_COLOR && destination == GL_ONE_MINUS_SRC_COLOR;
    if ((!alpha && !vignette && !crosshair) || display->gl.BlendFunc == NULL)
        return source_leave(display,
                            fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source blend function",
                                 "unimplemented blend pair or missing GL function", 0),
                            error);
    if (!source_material_blend_integer(display, GL_BLEND_SRC_RGB, source, error) ||
        !source_material_blend_integer(display, GL_BLEND_SRC_ALPHA, source, error) ||
        !source_material_blend_integer(display, GL_BLEND_DST_RGB, destination, error) ||
        !source_material_blend_integer(display, GL_BLEND_DST_ALPHA, destination, error))
        return source_leave(display, false, error);
    display->gl.BlendFunc((GLenum)source, (GLenum)destination);
    return source_leave(display, true, error);
}
