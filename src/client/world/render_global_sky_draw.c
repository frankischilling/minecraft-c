#include "render_global_sky_internal.h"

#include "../../entity/player/tick_math_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../application_state.h"
#include "../display/camera_surface.h"
#include "../display/fog_surface.h"
#include "../display/startup_surface.h"
#include "../render_engine/lighting.h"
#include "../render_engine/nw.h"
#include "render_global_internal.h"

#include <stdlib.h>
#include <string.h>

struct mc_client_render_global_sky_call {
    mc_client_render_global_sky_state state;
    mc_client_render_global_sky_error failure, cleanup;
    mc_client_render_global_error owner_error, owner_cleanup;
    mc_client_render_global *owner;
    bool owner_held;
    mc_math_classes *math;
    mc_client_tessellator *nw;
    mc_world_runtime *worlds[16];
    size_t world_count;
    mc_world_provider *dimensions[4];
    size_t dimension_count;
    mc_client_render_engine *engines[2];
    size_t engine_count;
    mc_entity_vector *sky;
    mc_entity_classes *sky_classes;
};

static bool fail(mc_client_render_global_sky_call *c, mc_client_render_global_status status,
                 const char *operation, unsigned int pc, bool native) {
    c->failure.status = status;
    c->failure.operation = operation;
    c->failure.site = (mc_client_graphics_site){"n", "a", "(F)V", (int32_t)pc};
    c->failure.native_policy = native;
    c->state.source_pc = pc;
    return false;
}

static bool fp(mc_client_render_global_sky_call *c, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP,
                "sky reached strict floating environment", pc, true);
}

static bool gl(mc_client_render_global_sky_call *c, bool ok, unsigned int pc) {
    if (!ok)
        return fail(c,
                    c->failure.display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                        ? MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
                    : c->failure.display.status == MC_CLIENT_DISPLAY_BUSY
                        ? MC_CLIENT_RENDER_GLOBAL_BUSY
                        : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                    c->failure.display.operation, pc, true);
    ++c->state.gl_calls;
    c->state.source_pc = pc;
    return true;
}

static bool graphics(mc_client_render_global_sky_call *c, bool ok, unsigned int pc) {
    if (!ok) {
        c->failure.child = c->failure.graphics.site;
        return fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, c->failure.graphics.operation, pc,
                    c->failure.graphics.native_policy);
    }
    c->state.source_pc = pc;
    return true;
}

static bool world_failure(mc_client_render_global_sky_call *c, const mc_world_color_error *e,
                          unsigned int pc) {
    const mc_world_color_site *s = e->child.class_name == NULL ? &e->site : &e->child;
    c->failure.child =
        (mc_client_graphics_site){s->class_name, s->method, s->descriptor, (int32_t)s->bytecode};
    return fail(c,
                e->native_policy ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                                 : MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                e->operation, pc, e->native_policy);
}

static bool dimension_failure(mc_client_render_global_sky_call *c, const char *descriptor,
                              unsigned int pc) {
    const mc_world_provider_sky_error *e = &c->failure.dimension;
    c->failure.child =
        (mc_client_graphics_site){e->source_class, "a", descriptor, (int32_t)e->bytecode};
    if (strcmp(descriptor, "()Z") == 0)
        c->failure.child.method = "c";
    return fail(c,
                e->native_policy ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                                 : MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                e->operation, pc, e->native_policy);
}

static client_application *app(mc_client_render_global_sky_call *c, unsigned int pc) {
    client_application *value = c->owner->fields.t;
    if (value == NULL)
        fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual sky null App receiver", pc, false);
    return value;
}

static bool world_hold(mc_client_render_global_sky_call *c, mc_world_runtime *world,
                       unsigned int pc) {
    if (world == NULL)
        return true; /* Preserve source receiver null until its actual dereference/invoke. */
    for (size_t i = 0; i < c->world_count; ++i)
        if (c->worlds[i] == world)
            return true;
    if (c->world_count == sizeof(c->worlds) / sizeof(c->worlds[0]))
        return fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "sky native captured World bound", pc, true);
    if (!mc_world_runtime_binding_retain(world)) {
        c->failure.world = *mc_world_runtime_last_error(world);
        return fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "sky actual World native occurrence", pc,
                    true);
    }
    c->worlds[c->world_count++] = world;
    return true;
}

static bool dimension_hold(mc_client_render_global_sky_call *c, mc_world_provider *dimension,
                           unsigned int pc) {
    if (dimension == NULL)
        return true;
    for (size_t i = 0; i < c->dimension_count; ++i)
        if (c->dimensions[i] == dimension)
            return true;
    if (c->dimension_count == sizeof(c->dimensions) / sizeof(c->dimensions[0]))
        return fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "sky native Dimension bound", pc, true);
    if (mc_world_provider_retain(dimension) != MC_WORLD_PROVIDER_OK)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "sky Dimension native occurrence", pc, true);
    c->dimensions[c->dimension_count++] = dimension;
    return true;
}

static mc_world_runtime *world_at(mc_client_render_global_sky_call *c, unsigned int pc) {
    mc_world_runtime *world = c->owner->fields.k;
    if (!world_hold(c, world, pc))
        return NULL;
    return world;
}

static mc_world_provider *dimension_at(mc_client_render_global_sky_call *c, mc_world_runtime *world,
                                       unsigned int pc) {
    if (world == NULL) {
        fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual sky null World Dimension receiver",
             pc, false);
        return NULL;
    }
    mc_world_runtime_fields fields;
    if (mc_world_runtime_fields_get(world, &fields) != MC_WORLD_STORAGE_OK) {
        fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "sky canonical World fields", pc, true);
        return NULL;
    }
    return dimension_hold(c, fields.t, pc) ? fields.t : NULL;
}

static bool options(mc_client_render_global_sky_call *c, unsigned int app_pc, unsigned int pc,
                    bool *anaglyph) {
    client_application *a = app(c, app_pc);
    if (a == NULL)
        return false;
    mc_game_settings_state settings;
    if (!mc_game_settings_read_state(a->settings, &settings))
        return fail(c,
                    a->settings == NULL ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                        : MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                    "actual sky Settings receiver", pc, a->settings != NULL);
    *anaglyph = settings.values.anaglyph;
    return true;
}

static void anaglyph(float rgb[3]) {
    const float r = rgb[0], g = rgb[1], b = rgb[2];
    rgb[0] = at_fdiv(at_fadd(at_fadd(at_fmul(r, 30), at_fmul(g, 59)), at_fmul(b, 11)), 100);
    rgb[1] = at_fdiv(at_fadd(at_fmul(r, 30), at_fmul(g, 70)), 100);
    rgb[2] = at_fdiv(at_fadd(at_fmul(r, 30), at_fmul(b, 70)), 100);
}

static bool color(mc_client_render_global_sky_call *c, float r, float g, float b, float a,
                  unsigned int pc) {
    return gl(c, mc_client_display_source_color(c->owner->display, r, g, b, a, &c->failure.display),
              pc);
}

static bool push(mc_client_render_global_sky_call *c, unsigned int pc) {
    if (!gl(c, mc_client_display_source_push(c->owner->display, &c->failure.display), pc))
        return false;
    ++c->state.matrix_pushes;
    return true;
}

static bool pop(mc_client_render_global_sky_call *c, unsigned int pc) {
    if (!gl(c, mc_client_display_source_pop(c->owner->display, &c->failure.display), pc))
        return false;
    ++c->state.matrix_pops;
    return true;
}

static bool rotate(mc_client_render_global_sky_call *c, float angle, float x, float y, float z,
                   unsigned int pc) {
    return gl(
        c, mc_client_display_source_rotate(c->owner->display, angle, x, y, z, &c->failure.display),
        pc);
}

static bool scalar(mc_client_render_global_sky_call *c, bool rain, float *value, unsigned int pc) {
    mc_world_runtime *world = world_at(c, pc);
    if (c->failure.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    const mc_world_storage_status status =
        rain
            ? mc_world_runtime_sky_rain_strength(world, c->state.partial, value, &c->failure.scalar)
            : mc_world_runtime_sky_angle(world, c->state.partial, value, &c->failure.scalar);
    if (status != MC_WORLD_STORAGE_OK)
        return world_failure(c, &c->failure.scalar, pc);
    return true;
}

static bool vertex(mc_client_render_global_sky_call *c, double x, double y, double z,
                   unsigned int pc) {
    if (!graphics(c, mc_client_nw_vertex(c->nw, x, y, z, &c->failure.graphics), pc))
        return false;
    ++c->state.vertices;
    return true;
}

static bool uv(mc_client_render_global_sky_call *c, double x, double y, double z, double u,
               double v, unsigned int pc) {
    if (!graphics(c, mc_client_nw_vertex_uv(c->nw, x, y, z, u, v, &c->failure.graphics), pc))
        return false;
    ++c->state.vertices;
    return true;
}

static bool draw(mc_client_render_global_sky_call *c, unsigned int pc) {
    if (!graphics(c, mc_client_nw_draw(c->nw, c->owner->display, &c->failure.graphics), pc))
        return false;
    ++c->state.draws;
    return true;
}

static bool nw_color(mc_client_render_global_sky_call *c, float r, float g, float b, float a,
                     unsigned int pc) {
    if (!fp(c, pc)) {
        c->failure.child = (mc_client_graphics_site){"nw", "a", "(FFFF)V", 4};
        return false;
    }
    /* Actual nw.a(FFFF) converts each *255 with Java f2i, then delegates to
     * this SAME nw.a(IIII), including its source q guard and component clamps. */
    const bool ok =
        graphics(c,
                 mc_client_nw_color(c->nw, mc_java_float_to_i32(at_fmul(r, 255)),
                                    mc_java_float_to_i32(at_fmul(g, 255)),
                                    mc_java_float_to_i32(at_fmul(b, 255)),
                                    mc_java_float_to_i32(at_fmul(a, 255)), &c->failure.graphics),
                 pc);
    if (!ok)
        c->failure.graphics.parent = (mc_client_graphics_site){"nw", "a", "(FFFF)V", 22};
    return ok;
}

static bool sunrise(mc_client_render_global_sky_call *c) {
    mc_world_runtime *world = world_at(c, 203);
    if (c->failure.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    mc_world_provider *dimension = dimension_at(c, world, 206);
    if (c->failure.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    float angle;
    if (!scalar(c, false, &angle, 214))
        return false;
    c->state.sunrise_angle = angle;
    const float *rgba = NULL;
    if (mc_world_provider_sunrise_color(dimension, angle, c->state.partial, c->math, &rgba,
                                        &c->failure.dimension) != MC_WORLD_PROVIDER_OK)
        return dimension_failure(c, "(FF)[F", 220);
    c->state.sunrise_returned = true;
    c->state.sunrise = rgba;
    c->state.sunrise_present = rgba != NULL;
    if (rgba == NULL)
        return true;
    if (!gl(c,
            mc_client_display_source_texture_enable(c->owner->display, false, &c->failure.display),
            231) ||
        !gl(c, mc_client_display_source_shade_model(c->owner->display, 7425, &c->failure.display),
            237) ||
        !push(c, 240) || !rotate(c, 90, 1, 0, 0, 248) || !scalar(c, false, &angle, 256) ||
        !rotate(c, angle > .5f ? 180.0f : 0.0f, 0, 0, 1, 278))
        return false;
    float rgb[3] = {rgba[0], rgba[1], rgba[2]};
    if (!options(c, 303, 306, &c->state.sunrise_anaglyph))
        return false;
    if (c->state.sunrise_anaglyph) {
        if (!fp(c, 316))
            return false;
        anaglyph(rgb);
    }
    memcpy(c->state.center_rgba, rgb, sizeof(rgb));
    c->state.center_rgba[3] = rgba[3];
    if (!graphics(c, mc_client_nw_begin(c->nw, 6, &c->failure.graphics), 382) ||
        !nw_color(c, rgb[0], rgb[1], rgb[2], rgba[3], 397) || !vertex(c, 0, 100, 0, 407))
        return false;
    /* Source edge color deliberately reloads the raw array, rather than the
     * anaglyph-transformed center locals. Alpha is zero for this whole rim. */
    memcpy(c->state.edge_rgb, rgba, 3 * sizeof(float));
    if (!nw_color(c, rgba[0], rgba[1], rgba[2], 0, 429))
        return false;
    for (int32_t index = 0; index <= 16; ++index) {
        if (!fp(c, 444))
            return false;
        const float converted = _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), index));
        const float theta = at_fdiv(at_fmul(at_fmul(converted, 3.1415927f), 2), 16);
        if (mc_math_classes_ensure(c->math, &c->failure.math) != MC_MATH_CLASS_OK)
            return fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                        "sky rim MathHelper active use", 458, true);
        if (!fp(c, 458))
            return false;
        const float sine = mc_math_sin(mc_math_classes_table(c->math), theta);
        const float cosine = mc_math_cos(mc_math_classes_table(c->math), theta);
        if (!vertex(c, (double)at_fmul(sine, 120), (double)at_fmul(cosine, 120),
                    (double)at_fmul(at_fmul(-cosine, 40), rgba[3]), 496))
            return false;
    }
    if (!draw(c, 507) || !pop(c, 510) ||
        !gl(c, mc_client_display_source_shade_model(c->owner->display, 7424, &c->failure.display),
            516))
        return false;
    c->state.fan_drawn = true;
    return true;
}

static bool texture(mc_client_render_global_sky_call *c, bool moon, mc_client_texture *out) {
    const unsigned int pc = moon ? 707u : 610u;
    mc_client_render_engine *engine = c->owner->fields.l;
    if (engine == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual sky null texture Engine", pc,
                    false);
    bool held = false;
    for (size_t i = 0; i < c->engine_count; ++i)
        if (c->engines[i] == engine)
            held = true;
    if (!held) {
        if (c->engine_count == 2 ||
            mc_client_render_engine_retain(engine) != MC_CLIENT_RENDER_ENGINE_OK)
            return fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "sky texture Engine native occurrence", pc,
                        true);
        c->engines[c->engine_count++] = engine;
    }
    static const uint16_t sun[] = {'/', 't', 'e', 'r', 'r', 'a', 'i', 'n',
                                   '/', 's', 'u', 'n', '.', 'p', 'n', 'g'};
    static const uint16_t moon_key[] = {'/', 't', 'e', 'r', 'r', 'a', 'i', 'n', '/',
                                        'm', 'o', 'o', 'n', '.', 'p', 'n', 'g'};
    const mc_texture_pack_text key = {
        moon ? moon_key : sun,
        moon ? sizeof(moon_key) / sizeof(*moon_key) : sizeof(sun) / sizeof(*sun), false};
    if (mc_client_render_engine_texture(engine, c->owner->display, NULL, key, out,
                                        &c->failure.texture) != MC_CLIENT_LOOKUP_OK) {
        c->failure.child = c->failure.texture.site;
        return fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                    c->failure.texture.operation, pc, c->failure.texture.native_policy);
    }
    return gl(c, mc_client_display_source_bind_id(c->owner->display, *out, &c->failure.display),
              moon ? 710u : 613u);
}

static bool celestial(mc_client_render_global_sky_call *c) {
    mc_client_display *d = c->owner->display;
    if (!gl(c, mc_client_display_source_texture_enable(d, true, &c->failure.display), 522) ||
        !gl(c, mc_client_display_source_blend_function(d, 770, 1, &c->failure.display), 529) ||
        !push(c, 532))
        return false;
    float rain;
    if (!scalar(c, true, &rain, 541) || !fp(c, 544))
        return false;
    c->state.rain_returned = true;
    c->state.rain_attenuation = at_fsub(1, rain);
    if (!color(c, 1, 1, 1, c->state.rain_attenuation, 561) ||
        !gl(c, mc_client_display_source_translate(d, 0, 0, 0, &c->failure.display), 570) ||
        !rotate(c, 0, 0, 0, 1, 577) || !scalar(c, false, &c->state.sun_angle, 585) || !fp(c, 590) ||
        !rotate(c, at_fmul(c->state.sun_angle, 360), 1, 0, 0, 594) ||
        !texture(c, false, &c->state.sun_texture) ||
        !graphics(c, mc_client_nw_begin_quads(c->nw, &c->failure.graphics), 618) ||
        !uv(c, -30, 100, -30, 0, 0, 636) || !uv(c, 30, 100, -30, 1, 0, 653) ||
        !uv(c, 30, 100, 30, 1, 1, 669) || !uv(c, -30, 100, 30, 0, 1, 686) || !draw(c, 691))
        return false;
    c->state.sun_drawn = true;
    if (!texture(c, true, &c->state.moon_texture) ||
        !graphics(c, mc_client_nw_begin_quads(c->nw, &c->failure.graphics), 715) ||
        !uv(c, -20, -100, 20, 1, 1, 732) || !uv(c, 20, -100, 20, 0, 1, 748) ||
        !uv(c, 20, -100, -20, 0, 0, 765) || !uv(c, -20, -100, -20, 1, 0, 783) || !draw(c, 788) ||
        !gl(c, mc_client_display_source_texture_enable(d, false, &c->failure.display), 794))
        return false;
    c->state.moon_drawn = true;
    mc_world_runtime *world = world_at(c, 798);
    if (c->failure.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    float stars;
    if (mc_world_runtime_star_brightness(world, c->state.partial, c->math, &stars,
                                         &c->failure.scalar) != MC_WORLD_STORAGE_OK)
        return world_failure(c, &c->failure.scalar, 802);
    if (!fp(c, 807))
        return false;
    c->state.stars_returned = true;
    c->state.star_brightness = at_fmul(stars, c->state.rain_attenuation);
    if (c->state.star_brightness > 0) {
        if (!color(c, c->state.star_brightness, c->state.star_brightness, c->state.star_brightness,
                   c->state.star_brightness, 825))
            return false;
        c->state.star_list = c->owner->fields.y;
        if (!gl(c, mc_client_display_lists_call(d, &c->state.star_list, 1, &c->failure.display),
                832))
            return false;
        c->state.stars_called = true;
    }
    return color(c, 1, 1, 1, 1, 839) &&
           gl(c, mc_client_display_source_blend_enable(d, false, &c->failure.display), 845) &&
           gl(c, mc_client_display_source_alpha_test(d, true, &c->failure.display), 851) &&
           gl(c, mc_client_display_source_fog_enable(d, true, &c->failure.display), 857) &&
           pop(c, 860);
}

static bool body(mc_client_render_global_sky_call *c) {
    client_application *a = app(c, 4);
    if (a == NULL || !world_hold(c, a->active_world, 7))
        return false;
    mc_world_provider *dimension = dimension_at(c, a->active_world, 7);
    if (c->failure.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    if (dimension == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                    "actual sky null Dimension c receiver", 10, false);
    mc_world_provider_fields fields;
    if (mc_world_provider_fields_get(dimension, &fields) != MC_WORLD_PROVIDER_OK)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "sky canonical Dimension c field", 10,
                    true);
    if (fields.c) {
        c->state.nether_skipped = c->state.returned = true;
        c->state.source_pc = 16;
        return true;
    }
    mc_client_display *d = c->owner->display;
    if (!gl(c, mc_client_display_source_texture_enable(d, false, &c->failure.display), 20))
        return false;
    mc_world_runtime *world = world_at(c, 24);
    if (c->failure.status != MC_CLIENT_RENDER_GLOBAL_OK || (a = app(c, 31)) == NULL)
        return false;
    if (mc_world_runtime_sky_color(world, a->i, c->state.partial, c->math, &c->sky,
                                   &c->failure.colors) != MC_WORLD_STORAGE_OK)
        return world_failure(c, &c->failure.colors, 35);
    c->sky_classes = mc_world_runtime_entities(world);
    c->state.sky_vector = c->sky;
    c->state.sky_returned = true;
    if (c->sky == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual sky color Vec3 receiver", 40,
                    false);
    if (!fp(c, 43))
        return false;
    c->state.sky_rgb[0] = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(c->sky->x)));
    c->state.sky_rgb[1] = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(c->sky->y)));
    c->state.sky_rgb[2] = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(c->sky->z)));
    if (!options(c, 63, 66, &c->state.sky_anaglyph))
        return false;
    if (c->state.sky_anaglyph) {
        if (!fp(c, 75))
            return false;
        anaglyph(c->state.sky_rgb);
    }
    const float *rgb = c->state.sky_rgb;
    if (!color(c, rgb[0], rgb[1], rgb[2], 1, 139) ||
        !graphics(c,
                  mc_client_nw_active_use(c->owner->classes,
                                          (mc_client_graphics_site){"n", "a", "(F)V", 142}, &c->nw,
                                          &c->failure.graphics),
                  142) ||
        !gl(c, mc_client_display_source_depth_mask(d, false, &c->failure.display), 150) ||
        !gl(c, mc_client_display_source_fog_enable(d, true, &c->failure.display), 154) ||
        !color(c, rgb[0], rgb[1], rgb[2], 1, 162))
        return false;
    c->state.top_list = c->owner->fields.z;
    if (!gl(c, mc_client_display_lists_call(d, &c->state.top_list, 1, &c->failure.display), 169))
        return false;
    c->state.top_called = true;
    if (!gl(c, mc_client_display_source_fog_enable(d, false, &c->failure.display), 175) ||
        !gl(c, mc_client_display_source_alpha_test(d, false, &c->failure.display), 181) ||
        !gl(c, mc_client_display_source_blend_enable(d, true, &c->failure.display), 187) ||
        !gl(c, mc_client_display_source_blend_function(d, 770, 771, &c->failure.display), 196))
        return false;
    const bool disabled = mc_client_lighting_disable(
        c->owner->classes, d, (mc_client_graphics_site){"n", "a", "(F)V", 199},
        &c->failure.graphics);
    c->state.gl_calls += (unsigned int)c->failure.graphics.retained_count;
    if (!graphics(c, disabled, 199))
        return false;
    c->state.lighting_disabled = true;
    if (!sunrise(c) || !celestial(c))
        return false;
    world = world_at(c, 864);
    if (c->failure.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    dimension = dimension_at(c, world, 867);
    if (c->failure.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    if (mc_world_provider_horizon_tint(dimension, &c->state.horizon_tint, &c->failure.dimension) !=
        MC_WORLD_PROVIDER_OK)
        return dimension_failure(c, "()Z", 870);
    c->state.horizon_returned = true;
    if (c->state.horizon_tint) {
        if (!fp(c, 879) ||
            !color(c, at_fadd(at_fmul(rgb[0], .2f), .04f), at_fadd(at_fmul(rgb[1], .2f), .04f),
                   at_fadd(at_fmul(rgb[2], .6f), .1f), 1, 899))
            return false;
    } else if (!color(c, rgb[0], rgb[1], rgb[2], 1, 910))
        return false;
    if (!gl(c, mc_client_display_source_texture_enable(d, false, &c->failure.display), 916))
        return false;
    c->state.bottom_list = c->owner->fields.A;
    if (!gl(c, mc_client_display_lists_call(d, &c->state.bottom_list, 1, &c->failure.display), 923))
        return false;
    c->state.bottom_called = true;
    if (!gl(c, mc_client_display_source_texture_enable(d, true, &c->failure.display), 929) ||
        !gl(c, mc_client_display_source_depth_mask(d, true, &c->failure.display), 933))
        return false;
    c->state.depth_restored = c->state.returned = true;
    c->state.source_pc = 936;
    return true;
}

static mc_client_render_global_status admission(mc_client_render_global_sky_error *out,
                                                mc_client_render_global_status status,
                                                const char *operation) {
    if (out != NULL)
        *out = (mc_client_render_global_sky_error){.status = status,
                                                   .operation = operation,
                                                   .native_policy =
                                                       status != MC_CLIENT_RENDER_GLOBAL_OK};
    return status;
}

mc_client_render_global_status
mc_client_render_global_sky_begin(mc_client_render_global *owner, mc_math_classes *math,
                                  float partial, mc_client_render_global_sky_call **out,
                                  mc_client_render_global_sky_error *error) {
    if (owner == NULL || math == NULL || out == NULL || *out != NULL)
        return admission(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                         "sky native owner/Math/output");
    mc_client_render_global_world_state attachment = {0};
    const bool attachment_ready =
        owner->world_call == NULL ||
        (mc_client_render_global_world_inspect(owner->world_call, &attachment) &&
         !attachment.active && attachment.source_returned);
    if (owner->active || owner->world_retiring || !attachment_ready || owner->sky_call != NULL ||
        owner->cull_call != NULL || owner->update_call != NULL || owner->submit_call != NULL ||
        owner->phase == MC_CLIENT_RENDER_GLOBAL_CONSTRUCTING ||
        owner->phase == MC_CLIENT_RENDER_GLOBAL_RETIRING)
        return admission(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                         "sky native entered/pending/retiring owner");
    mc_client_render_global_sky_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return admission(error, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                         "sky retained call allocation");
    if (mc_client_render_global_retain(owner, &c->owner_error) != MC_CLIENT_RENDER_GLOBAL_OK) {
        const mc_client_render_global_status status = c->owner_error.status;
        free(c);
        return admission(error, status, "sky native n occurrence");
    }
    c->owner = owner;
    c->owner_held = true;
    c->state.owner = owner;
    c->state.partial = partial;
    c->state.failure = &c->failure;
    c->state.cleanup = &c->cleanup;
    c->failure.owner = &c->owner_error;
    owner->sky_call = c;
    *out = c;
    if (mc_math_classes_retain(math, &c->failure.math) != MC_MATH_CLASS_OK)
        fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "sky Math native occurrence", 0, true);
    else {
        c->math = math;
        owner->active = c->state.active = true;
        (void)body(c);
        owner->active = c->state.active = false;
    }
    if (error != NULL)
        *error = c->failure;
    return c->failure.status;
}

bool mc_client_render_global_sky_inspect(const mc_client_render_global_sky_call *c,
                                         mc_client_render_global_sky_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    return true;
}

static mc_client_render_global_status cleanup_failed(mc_client_render_global_sky_call *c,
                                                     mc_client_render_global_sky_error *out,
                                                     const char *operation) {
    c->cleanup.status = MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED;
    c->cleanup.operation = operation;
    c->cleanup.native_policy = true;
    c->cleanup.owner = &c->owner_cleanup;
    if (out != NULL)
        *out = c->cleanup;
    return c->cleanup.status;
}

mc_client_render_global_status
mc_client_render_global_sky_retire(mc_client_render_global_sky_call **slot,
                                   mc_client_render_global_sky_error *error) {
    if (slot == NULL)
        return admission(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                         "sky native cleanup slot");
    mc_client_render_global_sky_call *c = *slot;
    if (c == NULL)
        return admission(error, MC_CLIENT_RENDER_GLOBAL_OK, NULL);
    if (c->state.active || c->owner->active)
        return admission(error, MC_CLIENT_RENDER_GLOBAL_BUSY, "sky source call active cleanup");
    c->state.retiring = true;
    for (size_t i = 0; i < c->engine_count; ++i) {
        mc_client_render_engine *engine = c->engines[i];
        if (engine == NULL)
            continue;
        if (mc_client_render_engine_retire_lookup(engine, &c->cleanup.texture) !=
            MC_CLIENT_LOOKUP_OK)
            return cleanup_failed(c, error, "sky reached texture prefix cleanup");
        if (mc_client_render_engine_release(engine) != MC_CLIENT_RENDER_ENGINE_OK)
            return cleanup_failed(c, error, "sky captured Engine cleanup");
        c->engines[i] = NULL;
    }
    if (c->sky != NULL) {
        if (mc_entity_vector_release(c->sky_classes, c->sky) != MC_ENTITY_OK)
            return cleanup_failed(c, error, "sky pooled Vec3 cleanup");
        c->sky = NULL;
        c->state.sky_vector = NULL;
    }
    c->state.sunrise = NULL;
    while (c->dimension_count != 0)
        mc_world_provider_release(c->dimensions[--c->dimension_count]);
    while (c->world_count != 0)
        mc_world_runtime_binding_release(c->worlds[--c->world_count]);
    if (mc_math_classes_release(&c->math, &c->cleanup.math) != MC_MATH_CLASS_OK)
        return cleanup_failed(c, error, "sky Math native cleanup");
    mc_client_render_global *owner = c->owner;
    owner->sky_call = NULL;
    if (c->owner_held) {
        mc_client_render_global *held = owner;
        if (mc_client_render_global_release(&held, &c->owner_cleanup) !=
            MC_CLIENT_RENDER_GLOBAL_OK) {
            owner->sky_call = c;
            return cleanup_failed(c, error, "sky retained n native cleanup");
        }
    }
    free(c);
    *slot = NULL;
    return admission(error, MC_CLIENT_RENDER_GLOBAL_OK, NULL);
}
