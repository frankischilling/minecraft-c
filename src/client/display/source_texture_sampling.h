#ifndef MC_CLIENT_DISPLAY_SOURCE_TEXTURE_SAMPLING_H
#define MC_CLIENT_DISPLAY_SOURCE_TEXTURE_SAMPLING_H

/* Compiled inside the canonical Display. Source texture state and its derived
 * core sampler remain on the same real slot/generation. No image is copied. */
#define SOURCE_TEXTURE_SAMPLING_GLSL                                                               \
    "uniform int legacyClampS,legacyClampT;\n"                                                     \
    "vec4 sourceTextureSample(sampler2D image,vec2 uv){\n"                                         \
    "if(legacyClampS==0&&legacyClampT==0)return texture(image,uv);\n"                              \
    "vec2 dx=dFdx(uv),dy=dFdy(uv),p=uv;\n"                                                         \
    "if(legacyClampS!=0)p.x=clamp(p.x,0.0,1.0);\n"                                                 \
    "if(legacyClampT!=0)p.y=clamp(p.y,0.0,1.0);\n"                                                 \
    "return textureGrad(image,p,dx,dy);}\n"

static mc_client_texture_source_state source_texture_defaults(uint64_t generation) {
    return (mc_client_texture_source_state){.generation = generation,
                                            .min_filter = GL_NEAREST_MIPMAP_LINEAR,
                                            .mag_filter = GL_LINEAR,
                                            .wrap_s = GL_REPEAT,
                                            .wrap_t = GL_REPEAT,
                                            .known = true,
                                            .physical_known = true,
                                            .physical_wrap_s = GL_REPEAT,
                                            .physical_wrap_t = GL_REPEAT};
}

static display_texture *source_texture_slot(mc_client_display *display, GLuint id) {
    for (size_t i = 0; i < display->texture_count; ++i)
        if (display->textures[i].id == id)
            return &display->textures[i];
    return NULL;
}

bool mc_client_display_texture_state_inspect(const mc_client_display *display,
                                             mc_client_texture texture,
                                             mc_client_texture_source_state *out) {
    if (display == NULL || out == NULL || texture == 0)
        return false;
    for (size_t i = 0; i < display->texture_count; ++i) {
        const display_texture *slot = &display->textures[i];
        if (slot->id == texture) {
            if (slot->resource != NULL &&
                (!slot->resource->live || !slot->resource->driver_generated))
                return false;
            *out = slot->source;
            return true;
        }
    }
    return false;
}

static GLint *source_texture_parameter_field(mc_client_texture_source_state *state, GLenum pname) {
    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
            return &state->min_filter;
        case GL_TEXTURE_MAG_FILTER:
            return &state->mag_filter;
        case GL_TEXTURE_WRAP_S:
            return &state->wrap_s;
        case GL_TEXTURE_WRAP_T:
            return &state->wrap_t;
        default:
            return NULL;
    }
}

static GLint source_texture_physical_wrap(const mc_client_texture_source_state *state, GLint wrap) {
    if (wrap != GL_CLAMP)
        return wrap;
    /* A legal temporary mixed/mipmap state is stored but not sampled. Its
     * provisional physical EDGE value is never credited as source CLAMP. */
    return state->min_filter == GL_LINEAR && state->mag_filter == GL_LINEAR ? GL_CLAMP_TO_BORDER
                                                                            : GL_CLAMP_TO_EDGE;
}

static bool source_texture_reconcile_wrap(mc_client_display *display, display_texture *slot,
                                          mc_client_display_error *error) {
    mc_client_texture_source_state *state = &slot->source;
    const GLint wraps[2] = {source_texture_physical_wrap(state, state->wrap_s),
                            source_texture_physical_wrap(state, state->wrap_t)};
    GLint *const actual[2] = {&state->physical_wrap_s, &state->physical_wrap_t};
    const GLenum names[2] = {GL_TEXTURE_WRAP_S, GL_TEXTURE_WRAP_T};
    for (size_t i = 0; i < 2; ++i) {
        if (!state->physical_known || *actual[i] != wraps[i]) {
            display->gl.TexParameteri(GL_TEXTURE_2D, names[i], wraps[i]);
            if (!gl_check(display, "legacy clamp physical wrap", error)) {
                state->physical_known = false;
                return false;
            }
            *actual[i] = wraps[i];
        }
    }
    state->physical_known = true;
    return true;
}

static bool source_texture_sampling(mc_client_display *display, GLuint texture, int *clamp_s,
                                    int *clamp_t, mc_client_display_error *error) {
    *clamp_s = *clamp_t = 0;
    display_texture *slot = source_texture_slot(display, texture);
    if (slot == NULL)
        return true; /* Existing non-owned/non-clamp source draw admission. */
    const mc_client_texture_source_state *state = &slot->source;
    if (!state->known)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source texture sampling",
                    "source texture parameters are unknown after driver failure", 0);
    if (state->wrap_s != GL_CLAMP && state->wrap_t != GL_CLAMP)
        return true;
    if (!state->physical_known || !state->image_uploaded)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source legacy clamp sampling",
                    "known uploaded RGBA generation and physical sampler required", 0);
    if (state->min_filter != state->mag_filter ||
        (state->min_filter != GL_NEAREST && state->min_filter != GL_LINEAR))
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source legacy clamp sampling",
                    "coherent level-zero NEAREST/NEAREST or LINEAR/LINEAR required", 0);
    if (state->min_filter == GL_LINEAR) {
        *clamp_s = state->wrap_s == GL_CLAMP;
        *clamp_t = state->wrap_t == GL_CLAMP;
    }
    return true;
}

static bool source_texture_bound_sampling(mc_client_display *display, bool textured, int *clamp_s,
                                          int *clamp_t, mc_client_display_error *error) {
    *clamp_s = *clamp_t = 0;
    if (!textured)
        return true;
    GLint bound = 0;
    display->gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    return gl_check(display, "source texture sampling binding", error) &&
           source_texture_sampling(display, (GLuint)bound, clamp_s, clamp_t, error);
}

#endif
