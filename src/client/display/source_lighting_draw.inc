/* Compiled inside the canonical Display translation unit. These operations
 * consume its source state; they never create a second lighting namespace. */
#include <stdio.h>

static bool source_finite_float(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return (bits & UINT32_C(0x7f800000)) != UINT32_C(0x7f800000);
}

static bool source_normal_transform(const mc_client_source_state *s, float out[16], float *rescale,
                                    mc_client_display_error *error) {
    const float *m = s->modelview.column_major;
    for (size_t i = 0; i < 16; ++i)
        if (!source_finite_float(m[i]))
            return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source normal transform",
                        "nonfinite modelview matrix", 0);
    if (m[3] != 0 || m[7] != 0 || m[11] != 0 || m[15] != 1)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source normal transform",
                    "projective modelview normal plane is not implemented", 0);
    const double a = m[0], b = m[4], c = m[8];
    const double d = m[1], e = m[5], f = m[9];
    const double g = m[2], h = m[6], i = m[10];
    const double cofactor[3][3] = {{e * i - f * h, f * g - d * i, d * h - e * g},
                                   {c * h - b * i, a * i - c * g, b * g - a * h},
                                   {b * f - c * e, c * d - a * f, a * e - b * d}};
    const double determinant = (a * cofactor[0][0] + b * cofactor[0][1]) + c * cofactor[0][2];
    if (determinant == 0)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source normal transform",
                    "singular modelview normal transform", 0);
    memset(out, 0, 16 * sizeof(float));
    out[15] = 1;
    for (size_t row = 0; row < 3; ++row)
        for (size_t column = 0; column < 3; ++column) {
            const double value = cofactor[row][column] / determinant;
            out[column * 4 + row] =
                _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(value)));
            if (!source_finite_float(out[column * 4 + row]))
                return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source normal transform",
                            "normal transform exceeds the finite native profile", 0);
        }
    *rescale = 1;
    if (s->fog.rescale_normal_enabled) {
        const double x = out[8], y = out[9], z = out[10];
        const double square = (x * x + y * y) + z * z;
        const double length = _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(square)));
        const double factor = 1.0 / length;
        *rescale = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(factor)));
        if (!source_finite_float(*rescale))
            return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source normal rescale",
                        "normal rescale exceeds the finite native profile", 0);
    }
    return true;
}

static bool source_lighting_admit(const mc_client_source_state *s, unsigned int arrays,
                                  mc_client_display_error *error) {
    if (!s->lighting_enabled)
        return true;
    const bool vertex_color = (arrays & (1u << MC_CLIENT_SOURCE_COLOR_ARRAY)) != 0;
    if ((arrays & (1u << MC_CLIENT_SOURCE_NORMAL_ARRAY)) == 0 && !s->fog.normal_known)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source current normal",
                    "unspecified normal after prior normal-array draw", 0);
    for (size_t side = 0; side < (s->fog.light_model_two_side ? 2u : 1u); ++side) {
        const mc_client_source_material_state *m = &s->fog.materials[side];
        const bool known[4] = {m->ambient_known, m->diffuse_known, m->specular_known,
                               m->emission_known};
        const int32_t properties[4] = {4608, 4609, 4610, 5632};
        for (size_t property = 0; property < 4; ++property) {
            if (source_material_property_tracked(&s->fog, side, properties[property])) {
                if (!vertex_color && !s->color_known)
                    return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source color material",
                                "unspecified tracked color after prior color-array draw", 0);
            } else if (!known[property])
                return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source material state",
                            "unspecified material after prior color-array draw", 0);
        }
        if (!m->shininess_known)
            return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source material state",
                        "unspecified source shininess", 0);
    }
    return true;
}

static bool source_lighting_uniforms(mc_client_display *display, mc_client_display_error *error) {
    mc_client_source_state *s = &display->source;

    const struct {
        int *location;
        const char *name;
    } scalars[] = {{&s->lighting_uniform, "lightingEnabled"},
                   {&s->normal_matrix_uniform, "normalMatrix"},
                   {&s->normalize_uniform, "normalNormalize"},
                   {&s->rescale_uniform, "normalRescale"},
                   {&s->normal_scale_uniform, "normalScale"},
                   {&s->lights_uniform, "lightsEnabled"},
                   {&s->global_ambient_uniform, "globalAmbient"},
                   {&s->local_viewer_uniform, "localViewer"},
                   {&s->two_side_uniform, "twoSide"},
                   {&s->separate_specular_uniform, "separateSpecular"},
                   {&s->color_material_uniform, "colorMaterial"},
                   {&s->material_face_uniform, "materialFace"},
                   {&s->material_mode_uniform, "materialMode"},
                   {&s->flat_uniform, "flatShading"}};

    for (size_t i = 0; i < sizeof(scalars) / sizeof(scalars[0]); ++i) {
        *scalars[i].location = display->gl.GetUniformLocation(s->program, scalars[i].name);
        if (*scalars[i].location < 0)
            return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "source GPU lighting uniforms",
                        scalars[i].name, 0);
    }

    const struct {
        int *locations;
        const char *name;
    } vectors[] = {{s->light_position_uniform, "lightPosition"},
                   {s->light_ambient_uniform, "lightAmbient"},
                   {s->light_diffuse_uniform, "lightDiffuse"},
                   {s->light_specular_uniform, "lightSpecular"},
                   {s->material_ambient_uniform, "materialAmbient"},
                   {s->material_diffuse_uniform, "materialDiffuse"},
                   {s->material_specular_uniform, "materialSpecular"},
                   {s->material_emission_uniform, "materialEmission"},
                   {s->material_shininess_uniform, "materialShininess"}};

    for (size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); ++i)
        for (size_t side = 0; side < 2; ++side) {
            char name[64];
            (void)snprintf(name, sizeof(name), "%s[%zu]", vectors[i].name, side);
            vectors[i].locations[side] = display->gl.GetUniformLocation(s->program, name);
            if (vectors[i].locations[side] < 0)
                return fail(error, MC_CLIENT_DISPLAY_GL_ERROR, "source GPU lighting uniforms",
                            "missing source lighting array uniform", 0);
        }
    return gl_check(display, "source GPU lighting uniforms", error);
}

static void source_lighting_upload(mc_client_display *display, const float normal_matrix[16],
                                   float normal_scale) {
    const mc_client_source_state *s = &display->source;
    const mc_client_source_fog_state *f = &s->fog;
    display->gl.Uniform1i(s->lighting_uniform, s->lighting_enabled ? 1 : 0);
    display->gl.UniformMatrix4fv(s->normal_matrix_uniform, 1, GL_FALSE, normal_matrix);
    display->gl.Uniform1i(s->normalize_uniform, f->normalize_enabled ? 1 : 0);
    display->gl.Uniform1i(s->rescale_uniform, f->rescale_normal_enabled ? 1 : 0);
    display->gl.Uniform1f(s->normal_scale_uniform, normal_scale);
    display->gl.Uniform1i(s->lights_uniform, (f->lights[0] ? 1 : 0) | (f->lights[1] ? 2 : 0));
    display->gl.Uniform4fv(s->global_ambient_uniform, 1, f->light_model_ambient);
    display->gl.Uniform1i(s->local_viewer_uniform, f->light_model_local_viewer ? 1 : 0);
    display->gl.Uniform1i(s->two_side_uniform, f->light_model_two_side ? 1 : 0);
    display->gl.Uniform1i(s->separate_specular_uniform,
                          f->light_model_color_control == 33274 ? 1 : 0);
    display->gl.Uniform1i(s->color_material_uniform, f->color_material ? 1 : 0);
    display->gl.Uniform1i(s->material_face_uniform, f->material_face);
    display->gl.Uniform1i(s->material_mode_uniform, f->material_mode);
    display->gl.Uniform1i(s->flat_uniform, s->shade_flat ? 1 : 0);
    for (size_t i = 0; i < 2; ++i) {
        const mc_client_source_light_state *light = &f->light_parameters[i];
        const mc_client_source_material_state *material = &f->materials[i];
        display->gl.Uniform4fv(s->light_position_uniform[i], 1, light->position_eye);
        display->gl.Uniform4fv(s->light_ambient_uniform[i], 1, light->ambient);
        display->gl.Uniform4fv(s->light_diffuse_uniform[i], 1, light->diffuse);
        display->gl.Uniform4fv(s->light_specular_uniform[i], 1, light->specular);
        display->gl.Uniform4fv(s->material_ambient_uniform[i], 1, material->ambient);
        display->gl.Uniform4fv(s->material_diffuse_uniform[i], 1, material->diffuse);
        display->gl.Uniform4fv(s->material_specular_uniform[i], 1, material->specular);
        display->gl.Uniform4fv(s->material_emission_uniform[i], 1, material->emission);
        display->gl.Uniform1f(s->material_shininess_uniform[i], material->shininess);
    }
}
