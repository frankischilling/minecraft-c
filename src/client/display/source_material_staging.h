#ifndef MC_CLIENT_DISPLAY_SOURCE_MATERIAL_STAGING_H
#define MC_CLIENT_DISPLAY_SOURCE_MATERIAL_STAGING_H

/* Included only in the actual Display translation unit, after profile lookup.
 * Fixed float Color4/Normal3/Material attributes do not queue immediate-mode
 * vertices. First-slot fixup only enables their fixed size; the Mesa >8
 * immediate-vertex upgrade/reset heuristic cannot be reached by these calls. */
enum { SOURCE_STAGE_NORMAL = 0, SOURCE_STAGE_COLOR = 1, SOURCE_STAGE_PROPERTIES = 6 };

enum { SOURCE_STAGE_STORED = 1, SOURCE_STAGE_CURRENT = 2 };

typedef struct source_material_field {
    float *values;
    bool *known;
    size_t width;
} source_material_field;

static int source_material_property(int32_t pname) {
    switch (pname) {
        case GL_AMBIENT:
            return 0;
        case GL_DIFFUSE:
            return 1;
        case GL_SPECULAR:
            return 2;
        case GL_EMISSION:
            return 3;
        case GL_SHININESS:
            return 4;
        case GL_COLOR_INDEXES:
            return 5;
        default:
            return -1;
    }
}

static source_material_field source_material_field_for(mc_client_source_material_state *m,
                                                       unsigned int property) {
    switch (property) {
        case 0:
            return (source_material_field){m->ambient, &m->ambient_known, 4};
        case 1:
            return (source_material_field){m->diffuse, &m->diffuse_known, 4};
        case 2:
            return (source_material_field){m->specular, &m->specular_known, 4};
        case 3:
            return (source_material_field){m->emission, &m->emission_known, 4};
        case 4:
            return (source_material_field){&m->shininess, &m->shininess_known, 1};
        default:
            return (source_material_field){m->color_indexes, &m->color_indexes_known, 3};
    }
}

static uint32_t source_material_stage_bit(size_t side, unsigned int property) {
    return UINT32_C(1) << (2u + side * SOURCE_STAGE_PROPERTIES + property);
}

static bool source_material_profile_admit(mc_client_display *display, bool tracking,
                                          const char *operation, mc_client_display_error *error) {
    if (source_attribute_profile_for(display) == SOURCE_ATTRIBUTE_PROFILE_UNKNOWN &&
        (tracking || display->source.fog.color_material ||
         display->source.attribute_staging.enabled != 0))
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, operation,
                    "native implementation has no measured material commit profile", 0);
    return true;
}

static bool source_material_is_staged(const mc_client_source_state *s) {
    return s->attribute_profile == SOURCE_ATTRIBUTE_PROFILE_MESA_25_2_8;
}

/* Equality used by source state early returns, without FP evaluation. NaNs
 * compare unequal and either signed zero compares equal, as in Mesa's ==. */
static bool source_material_float_equal(float a, float b) {
    uint32_t left, right;
    memcpy(&left, &a, sizeof(left));
    memcpy(&right, &b, sizeof(right));
    const uint32_t lm = left & UINT32_C(0x7fffffff);
    const uint32_t rm = right & UINT32_C(0x7fffffff);
    if (lm > UINT32_C(0x7f800000) || rm > UINT32_C(0x7f800000))
        return false;
    return left == right || (lm == 0 && rm == 0);
}

static bool source_material_vector_equal(const float *a, const float *b, size_t width) {
    for (size_t i = 0; i < width; ++i)
        if (!source_material_float_equal(a[i], b[i]))
            return false;
    return true;
}

static void source_material_stage_color(mc_client_source_state *s, const float rgba[4]) {
    if (!source_material_is_staged(s)) {
        memcpy(s->color, rgba, sizeof(s->color));
        s->color_known = true;
        source_material_color(&s->fog, rgba, true);
        return;
    }
    memcpy(s->attribute_staging.color, rgba, sizeof(s->attribute_staging.color));
    s->attribute_staging.color_known = true;
    s->attribute_staging.enabled |= UINT32_C(1) << SOURCE_STAGE_COLOR;
    s->attribute_staging.flags |= SOURCE_STAGE_CURRENT;
}

static void source_material_stage_normal(mc_client_source_state *s, const float xyz[3]) {
    if (!source_material_is_staged(s)) {
        memcpy(s->fog.normal, xyz, sizeof(s->fog.normal));
        s->fog.normal_known = true;
        return;
    }
    memcpy(s->attribute_staging.normal, xyz, sizeof(s->attribute_staging.normal));
    s->attribute_staging.normal_known = true;
    s->attribute_staging.enabled |= UINT32_C(1) << SOURCE_STAGE_NORMAL;
    s->attribute_staging.flags |= SOURCE_STAGE_CURRENT;
}

static void source_material_assign(mc_client_source_state *s, size_t side, int32_t pname,
                                   const float values[4]) {
    if (source_material_property_tracked(&s->fog, side, pname))
        return;
    const unsigned int property = (unsigned int)source_material_property(pname);
    mc_client_source_material_state *material = source_material_is_staged(s)
                                                    ? &s->attribute_staging.materials[side]
                                                    : &s->fog.materials[side];
    source_material_field field = source_material_field_for(material, property);
    memcpy(field.values, values, field.width * sizeof(float));
    *field.known = true;
    if (source_material_is_staged(s)) {
        s->attribute_staging.enabled |= source_material_stage_bit(side, property);
        s->attribute_staging.flags |= SOURCE_STAGE_CURRENT;
    }
}

static void source_material_copy_current(mc_client_source_state *s) {
    mc_client_source_attribute_staging *stage = &s->attribute_staging;
    bool color_changed = false;
    if ((stage->enabled & (UINT32_C(1) << SOURCE_STAGE_COLOR)) != 0) {
        color_changed = !s->color_known || !stage->color_known ||
                        memcmp(s->color, stage->color, sizeof(s->color)) != 0;
        memcpy(s->color, stage->color, sizeof(s->color));
        s->color_known = stage->color_known;
    }
    if ((stage->enabled & (UINT32_C(1) << SOURCE_STAGE_NORMAL)) != 0) {
        memcpy(s->fog.normal, stage->normal, sizeof(s->fog.normal));
        s->fog.normal_known = stage->normal_known;
    }
    for (size_t side = 0; side < 2; ++side)
        for (unsigned int property = 0; property < SOURCE_STAGE_PROPERTIES; ++property)
            if ((stage->enabled & source_material_stage_bit(side, property)) != 0) {
                source_material_field from =
                    source_material_field_for(&stage->materials[side], property);
                source_material_field to =
                    source_material_field_for(&s->fog.materials[side], property);
                memcpy(to.values, from.values, to.width * sizeof(float));
                *to.known = *from.known;
            }
    if (color_changed && s->fog.color_material)
        source_material_color(&s->fog, s->color, s->color_known);
}

static void source_material_flush(mc_client_source_state *s, uint32_t flags) {
    if ((flags & SOURCE_STAGE_STORED) != 0) {
        if (s->attribute_staging.enabled != 0)
            source_material_copy_current(s);
        s->attribute_staging.enabled = 0;
        s->attribute_staging.flags = 0;
    } else {
        source_material_copy_current(s);
        /* Exact Mesa25.2.8 assignment: staged attributes remain active and
         * STORED becomes set. This is not an &=~CURRENT operation. */
        s->attribute_staging.flags = ~UINT32_C(2);
    }
}

static void source_material_vertices(mc_client_source_state *s) {
    if (source_material_is_staged(s) && (s->attribute_staging.flags & SOURCE_STAGE_STORED) != 0)
        source_material_flush(s, SOURCE_STAGE_STORED);
}

static void source_material_current(mc_client_source_state *s) {
    if (source_material_is_staged(s) && (s->attribute_staging.flags & SOURCE_STAGE_CURRENT) != 0)
        source_material_flush(s, SOURCE_STAGE_CURRENT);
}

static void source_material_draw(mc_client_source_state *s) {
    /* llvmpipe's measured profile has allow_draw_out_of_order=false. */
    if (source_material_is_staged(s) && s->attribute_staging.flags != 0)
        source_material_flush(s, s->attribute_staging.flags);
}

static void source_material_publish(mc_client_source_state *s, const mc_client_source_state *next) {
    memcpy(s->color, next->color, sizeof(s->color));
    s->color_known = next->color_known;
    memcpy(s->fog.normal, next->fog.normal, sizeof(s->fog.normal));
    s->fog.normal_known = next->fog.normal_known;
    memcpy(s->fog.materials, next->fog.materials, sizeof(s->fog.materials));
    s->attribute_staging = next->attribute_staging;
}

/* Query only when STORED makes an early-return decision observable. These
 * are supported core-state enums, not legacy GL state or error queries. */
static void source_material_core_integer(mc_client_display *display, GLenum pname, GLint desired) {
    mc_client_source_state *s = &display->source;
    if (!source_material_is_staged(s) || (s->attribute_staging.flags & SOURCE_STAGE_STORED) == 0)
        return;
    GLint actual = desired;
    display->gl.GetIntegerv(pname, &actual);
    if (actual != desired)
        source_material_vertices(s);
}

static bool source_material_blend_integer(mc_client_display *display, GLenum pname, GLint desired,
                                          mc_client_display_error *error) {
    mc_client_source_state *s = &display->source;
    if (!source_material_is_staged(s) || (s->attribute_staging.flags & SOURCE_STAGE_STORED) == 0)
        return true;
    PFNGLGETINTEGERI_VPROC get_indexed = NULL;
    SDL_FunctionPointer address = SDL_GL_GetProcAddress("glGetIntegeri_v");
    _Static_assert(sizeof(get_indexed) == sizeof(address), "GL indexed query representation");
    if (address == NULL)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source indexed blend state",
                    "actual indexed core state query unavailable", 0);
    memcpy(&get_indexed, &address, sizeof(get_indexed));
    GLint buffers = 0;
    display->gl.GetIntegerv(GL_MAX_DRAW_BUFFERS, &buffers);
    if (buffers <= 0)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source indexed blend state",
                    "actual core draw-buffer count unavailable", 0);
    /* Nonindexed Enable/BlendFunc affect all draw buffers in this profile. */
    for (GLint index = 0; index < buffers; ++index) {
        GLint actual = desired;
        get_indexed(pname, (GLuint)index, &actual);
        if (actual != desired) {
            source_material_vertices(s);
            break;
        }
    }
    return true;
}

static bool source_material_clear_depth(mc_client_display *display,
                                        mc_client_display_error *error) {
    mc_client_source_state *s = &display->source;
    if (!source_material_is_staged(s) || (s->attribute_staging.flags & SOURCE_STAGE_STORED) == 0)
        return true;
    typedef void(GLAPIENTRY * source_get_double_fn)(GLenum, GLdouble *);
    source_get_double_fn get_double = NULL;
    SDL_FunctionPointer address = SDL_GL_GetProcAddress("glGetDoublev");
    _Static_assert(sizeof(get_double) == sizeof(address), "GL query pointer representation");
    if (address == NULL)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source clear-depth state",
                    "actual core state query unavailable", 0);
    memcpy(&get_double, &address, sizeof(get_double));
    GLdouble actual = 1;
    get_double(GL_DEPTH_CLEAR_VALUE, &actual);
    uint64_t bits;
    memcpy(&bits, &actual, sizeof(bits));
    if (bits != UINT64_C(0x3ff0000000000000))
        source_material_vertices(s);
    return true;
}

static bool source_material_texture_bind(mc_client_texture_resource *resource,
                                         mc_client_display_error *error) {
    mc_client_display *display = resource->display;
    mc_client_source_state *s = &display->source;
    if (!source_material_is_staged(s) || (s->attribute_staging.flags & SOURCE_STAGE_STORED) == 0)
        return true;
    if (!resource->live || !resource->driver_generated || resource->id == 0)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source texture bind barrier",
                    "pending attributes require a live generated 2D texture", 0);
    /* Mesa texobj.c bind_texture_object returns before FLUSH_VERTICES when
     * this unshared, nonexternal target already has the requested object. */
    GLint actual = 0;
    display->gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &actual);
    if ((GLuint)actual != resource->id)
        source_material_vertices(s);
    return true;
}

static void source_material_texture_image(mc_client_display *display) {
    /* Mesa teximage.c teximage flushes before its native image validation.
     * This hook follows the existing C ownership/unpack admission only. */
    source_material_vertices(&display->source);
}

static bool source_material_texture_parameter(mc_client_texture_resource *resource, GLenum pname,
                                              GLint value, mc_client_display_error *error) {
    mc_client_display *display = resource->display;
    mc_client_source_state *s = &display->source;
    if (!source_material_is_staged(s) || (s->attribute_staging.flags & SOURCE_STAGE_STORED) == 0)
        return true;
    bool valid = false, extension_dependent = false;
    switch (pname) {
        case GL_TEXTURE_MIN_FILTER:
            valid = value == GL_NEAREST || value == GL_LINEAR ||
                    value == GL_NEAREST_MIPMAP_NEAREST || value == GL_LINEAR_MIPMAP_NEAREST ||
                    value == GL_NEAREST_MIPMAP_LINEAR || value == GL_LINEAR_MIPMAP_LINEAR;
            break;
        case GL_TEXTURE_MAG_FILTER:
            valid = value == GL_NEAREST || value == GL_LINEAR;
            break;
        case GL_TEXTURE_WRAP_S:
        case GL_TEXTURE_WRAP_T:
            valid = value == GL_CLAMP || value == GL_REPEAT || value == GL_MIRRORED_REPEAT ||
                    value == GL_CLAMP_TO_EDGE || value == GL_CLAMP_TO_BORDER ||
                    value == GL_MIRROR_CLAMP_TO_EDGE;
            extension_dependent =
                value == GL_MIRROR_CLAMP_EXT || value == GL_MIRROR_CLAMP_TO_BORDER_EXT;
            break;
        default:
            return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source texture parameter barrier",
                        "pending attributes require a supported texture parameter", 0);
    }
    /* Mesa rejects invalid enum values without FLUSH_VERTICES. Let the
     * original call retain its native error behavior on that path. */
    if (!valid && !extension_dependent)
        return true;
    if (!resource->live || !resource->driver_generated)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source texture parameter barrier",
                    "pending attributes require a live generated 2D texture", 0);
    display_texture *slot = resource_slot(resource);
    GLint *source_value = source_texture_parameter_field(&slot->source, pname);
    if (source_value != NULL && (*source_value == GL_CLAMP || value == GL_CLAMP)) {
        if (!slot->source.known)
            return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source texture parameter barrier",
                        "legacy source texture state is unknown after driver failure", 0);
        if (*source_value != value)
            source_material_vertices(s);
        return true;
    }
    typedef void(GLAPIENTRY * source_get_texture_parameter_fn)(GLenum, GLenum, GLint *);
    source_get_texture_parameter_fn get_parameter = NULL;
    SDL_FunctionPointer address = SDL_GL_GetProcAddress("glGetTexParameteriv");
    _Static_assert(sizeof(get_parameter) == sizeof(address), "GL texture query representation");
    if (address == NULL)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source texture parameter barrier",
                    "actual core texture parameter query unavailable", 0);
    memcpy(&get_parameter, &address, sizeof(get_parameter));
    GLint actual = value;
    get_parameter(GL_TEXTURE_2D, pname, &actual);
    if (actual == value)
        return true;
    if (extension_dependent)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source texture parameter barrier",
                    "pending attributes do not admit extension-dependent wrap changes", 0);
    /* texparam.c validates these four 2D cases and flushes only on change. */
    source_material_vertices(s);
    return true;
}

static bool source_material_texture_sub_image(mc_client_display *display, GLint level, GLint x,
                                              GLint y, GLsizei width, GLsizei height,
                                              mc_client_display_error *error) {
    mc_client_source_state *s = &display->source;
    if (!source_material_is_staged(s) || (s->attribute_staging.flags & SOURCE_STAGE_STORED) == 0)
        return true;
    if (level != 0 || x < 0 || y < 0 || width < 0 || height < 0)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source texture subimage barrier",
                    "pending attributes require a nonnegative level-zero update", 0);
    GLint binding = 0;
    display->gl.GetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    mc_client_texture_resource *resource = NULL;
    for (size_t i = 0; i < display->texture_count; ++i)
        if (display->textures[i].id == (GLuint)binding) {
            resource = display->textures[i].resource;
            break;
        }
    if (binding == 0 || resource == NULL || !resource->live || !resource->driver_generated)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source texture subimage barrier",
                    "pending attributes require a bound live generated 2D texture", 0);
    typedef void(GLAPIENTRY * source_get_texture_level_fn)(GLenum, GLint, GLenum, GLint *);
    source_get_texture_level_fn get_level = NULL;
    SDL_FunctionPointer address = SDL_GL_GetProcAddress("glGetTexLevelParameteriv");
    _Static_assert(sizeof(get_level) == sizeof(address), "GL texture level query representation");
    if (address == NULL)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source texture subimage barrier",
                    "actual core texture level query unavailable", 0);
    memcpy(&get_level, &address, sizeof(get_level));
    GLint image_width = 0, image_height = 0, internal_format = 0;
    get_level(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &image_width);
    get_level(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &image_height);
    get_level(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);
    /* Owned core uploads use border zero and RGBA/UNSIGNED_BYTE. No removed
     * legacy border enum is queried. Other native image classes stay outside
     * this proof, including integer and compressed destination formats. */
    if (image_width <= 0 || image_height <= 0 ||
        (internal_format != GL_RGBA && internal_format != GL_RGBA8) ||
        (uint64_t)(uint32_t)x + (uint32_t)width > (uint32_t)image_width ||
        (uint64_t)(uint32_t)y + (uint32_t)height > (uint32_t)image_height)
        return fail(error, MC_CLIENT_DISPLAY_UNSUPPORTED_GL, "source texture subimage barrier",
                    "pending attributes require a proven in-bounds owned RGBA image", 0);
    /* teximage.c checks errors first, then texture_sub_image flushes even for
     * a valid zero-area update, before deciding whether to copy any pixels. */
    source_material_vertices(s);
    return true;
}

#endif
