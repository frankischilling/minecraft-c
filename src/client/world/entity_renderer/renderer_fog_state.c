#include "renderer_fog_state_internal.h"

#include "../../../world/runtime/runtime_internal.h"
#include "../../application_state.h"
#include "../../display/fog_surface.h"
#include "../../display/source_surface.h"
#include "internal.h"
#include "mc/block/registry/runtime.h"

#include <emmintrin.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Source fog-state arithmetic requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Source fog-state arithmetic requires strict floating transformations"
#endif

struct mc_client_renderer_fog_state_call {
    mc_client_renderer_fog_state_view state;
    mc_client_renderer_fog_state_error failure;
    mc_registry_classes *code_registry;
    bool camera_held, world_held;
};

static mc_client_graphics_site site(const char *descriptor, unsigned int pc) {
    return (mc_client_graphics_site){"px", "a", descriptor, (int32_t)pc};
}

static bool fail(mc_client_renderer_fog_state_call *c, mc_client_renderer_fog_state_status status,
                 const char *operation, unsigned int pc, bool native) {
    c->failure.status = status;
    c->failure.operation = operation;
    c->failure.site = site("(IF)V", pc);
    c->failure.native_policy = native;
    c->state.source_pc = pc;
    return false;
}

static bool fp(mc_client_renderer_fog_state_call *c, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_CLIENT_RENDERER_FOG_STATE_UNSUPPORTED_FP,
                "fog-state native floating environment", pc, true);
}

static float add(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float mul(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float divide(float a, float b) {
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static client_application *app_at(mc_client_renderer_fog_state_call *c, unsigned int pc) {
    client_application *app = c->state.owner->fields.j;
    if (app == NULL)
        (void)fail(c, MC_CLIENT_RENDERER_FOG_STATE_SOURCE_NULL, "actual fog-state App receiver", pc,
                   false);
    return app;
}

static bool display_result(mc_client_renderer_fog_state_call *c, bool ok, unsigned int pc) {
    return ok || fail(c,
                      c->failure.display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                          ? MC_CLIENT_RENDERER_FOG_STATE_UNSUPPORTED_FP
                          : MC_CLIENT_RENDERER_FOG_STATE_NATIVE_FAILED,
                      c->failure.display.operation, pc, true);
}

static bool buffer_result(mc_client_renderer_fog_state_call *c, bool ok, unsigned int pc) {
    c->state.helper_pc = pc;
    if (ok)
        return true;
    const mc_client_graphics_error *g = &c->failure.graphics;
    (void)fail(c,
               g->status == MC_CLIENT_GRAPHICS_NULL_REFERENCE
                   ? MC_CLIENT_RENDERER_FOG_STATE_SOURCE_NULL
                   : MC_CLIENT_RENDERER_FOG_STATE_CHILD_FAILED,
               g->operation, 25, g->native_policy);
    c->failure.site = site("(FFFF)Ljava/nio/FloatBuffer;", pc);
    c->failure.parent = site("(IF)V", 25);
    c->failure.child = g->site;
    return false;
}

static bool color_buffer(mc_client_renderer_fog_state_call *c) {
    mc_client_source_px *owner = c->state.owner;
    const mc_client_graphics_site clear = site("(FFFF)Ljava/nio/FloatBuffer;", 4);
    if (!buffer_result(c, mc_client_ge_float_clear(owner->fields.f, clear, &c->failure.graphics),
                       4))
        return false;
    mc_client_direct_float_buffer *buffer = owner->fields.f; /* Actual fresh f9. */
    static const unsigned int pcs[] = {13, 17, 21, 26};
    for (size_t i = 0; i < 4; ++i) {
        if (!buffer_result(c,
                           mc_client_ge_float_put(buffer, c->state.rgba[i],
                                                  site("(FFFF)Ljava/nio/FloatBuffer;", pcs[i]),
                                                  &c->failure.graphics),
                           pcs[i]))
            return false;
        ++c->state.buffer_puts;
    }
    if (!buffer_result(c,
                       mc_client_ge_float_flip(owner->fields.f,
                                               site("(FFFF)Ljava/nio/FloatBuffer;", 34),
                                               &c->failure.graphics),
                       34))
        return false;
    buffer = owner->fields.f; /* Actual final f39 return42. */
    c->state.buffer = buffer;
    c->state.helper_pc = 42;
    c->state.color_buffer_returned = true;
    void *storage;
    size_t remaining;
    if (!mc_client_ge_float_writable_remaining(buffer, &storage, &remaining, site("(IF)V", 28),
                                               &c->failure.graphics))
        return buffer_result(c, false, 42);
    if (remaining < 4)
        return fail(c, MC_CLIENT_RENDERER_FOG_STATE_NATIVE_FAILED,
                    "actual fog RGBA FloatBuffer span", 28, true);
    memcpy(c->state.rgba, storage, sizeof(c->state.rgba));
    return true;
}

static bool immersion(mc_client_renderer_fog_state_call *c, bool lava, bool *out) {
    const unsigned int active_pc = lava ? 282u : 161u, invoke_pc = lava ? 285u : 164u;
    mc_material *material = NULL;
    if (mc_registry_material_get(c->code_registry, lava ? MC_MATERIAL_h : MC_MATERIAL_g, &material,
                                 &c->failure.registry) != MC_REGISTRY_OK)
        return fail(c, MC_CLIENT_RENDERER_FOG_STATE_CHILD_FAILED,
                    "fog-state actual Material active use", active_pc, true);
    if (c->state.camera == NULL)
        return fail(c, MC_CLIENT_RENDERER_FOG_STATE_SOURCE_NULL,
                    "actual fog-state camera immersion receiver", invoke_pc, false);
    const mc_entity_status status =
        mc_entity_inside_material(c->state.camera, material, out, &c->failure.material);
    if (status == MC_ENTITY_OK)
        return true;
    const bool source_null = status == MC_ENTITY_WORLD_FAILED &&
                             c->failure.material.world.status == MC_WORLD_STORAGE_INVALID_ARGUMENT;
    c->failure.child =
        (mc_client_graphics_site){"sn", "a", "(Lln;)Z", (int32_t)c->failure.material.bytecode};
    return fail(c,
                source_null ? MC_CLIENT_RENDERER_FOG_STATE_SOURCE_NULL
                            : MC_CLIENT_RENDERER_FOG_STATE_CHILD_FAILED,
                c->failure.material.operation, invoke_pc, !source_null);
}

static bool exponential(mc_client_renderer_fog_state_call *c, float density, const float rgb[3],
                        unsigned int mode_pc, unsigned int density_pc, unsigned int app_pc,
                        unsigned int settings_pc, unsigned int arithmetic_pc) {
    mc_client_display *display = c->state.owner->services.display;
    if (!display_result(c, mc_client_display_source_fog_i(display, 2917, 2048, &c->failure.display),
                        mode_pc))
        return false;
    c->state.mode = 2048;
    c->state.mode_set = true;
    if (!display_result(c,
                        mc_client_display_source_fog_f(display, 2914, density, &c->failure.display),
                        density_pc))
        return false;
    c->state.density = density;
    c->state.density_set = true;
    memcpy(c->state.unused_rgb, rgb, sizeof(c->state.unused_rgb));
    client_application *app = app_at(c, app_pc);
    if (app == NULL)
        return false;
    mc_game_settings_state settings;
    if (!mc_game_settings_read_state(app->settings, &settings))
        return fail(c,
                    app->settings == NULL ? MC_CLIENT_RENDERER_FOG_STATE_SOURCE_NULL
                                          : MC_CLIENT_RENDERER_FOG_STATE_NATIVE_FAILED,
                    "actual fog-state anaglyph Settings receiver", settings_pc,
                    app->settings != NULL);
    c->state.anaglyph_read = true;
    c->state.anaglyph = settings.values.anaglyph;
    if (c->state.anaglyph) {
        if (!fp(c, arithmetic_pc))
            return false;
        const float red = divide(add(add(mul(rgb[0], 30), mul(rgb[1], 59)), mul(rgb[2], 11)), 100);
        const float green = divide(add(mul(rgb[0], 30), mul(rgb[1], 70)), 100);
        const float blue = divide(add(mul(rgb[0], 30), mul(rgb[2], 70)), 100);
        c->state.unused_rgb[0] = red;
        c->state.unused_rgb[1] = green;
        c->state.unused_rgb[2] = blue;
        c->state.unused_rgb_computed = true;
    }
    return true;
}

static bool linear(mc_client_renderer_fog_state_call *c) {
    mc_client_source_px *owner = c->state.owner;
    mc_client_display *display = owner->services.display;
    if (!display_result(c, mc_client_display_source_fog_i(display, 2917, 9729, &c->failure.display),
                        407))
        return false;
    c->state.mode = 9729;
    c->state.mode_set = true;
    if (!fp(c, 419))
        return false;
    const float start = mul(owner->fields.k, 0.25f);
    if (!display_result(
            c, mc_client_display_source_fog_f(display, 2915, start, &c->failure.display), 420))
        return false;
    c->state.start = start;
    c->state.start_set = true;
    const float end = owner->fields.k; /* Fresh source k427 after preceding GL effect. */
    if (!display_result(c, mc_client_display_source_fog_f(display, 2916, end, &c->failure.display),
                        430))
        return false;
    c->state.end = end;
    c->state.end_set = true;
    if (c->state.pass < 0) {
        if (!display_result(
                c, mc_client_display_source_fog_f(display, 2915, 0, &c->failure.display), 441))
            return false;
        c->state.start = 0;
        if (!fp(c, 453))
            return false;
        const float negative_end = mul(owner->fields.k, 0.8f); /* Fresh k448. */
        if (!display_result(
                c, mc_client_display_source_fog_f(display, 2916, negative_end, &c->failure.display),
                454))
            return false;
        c->state.end = negative_end;
        c->state.negative_pass_applied = true;
    }
    if (!display_result(c,
                        mc_client_display_source_nv_fog_distance(display, &c->state.nv_fog_distance,
                                                                 &c->failure.display),
                        457))
        return false;
    c->state.capability_read = true;
    if (c->state.nv_fog_distance) {
        if (!display_result(
                c, mc_client_display_source_fog_i(display, 34138, 34139, &c->failure.display), 470))
            return false;
        c->state.radial_set = true;
    }
    client_application *app = app_at(c, 477);
    if (app == NULL)
        return false;
    c->state.world = app->active_world;
    c->state.world_captured = true;
    if (c->state.world == NULL)
        return fail(c, MC_CLIENT_RENDERER_FOG_STATE_SOURCE_NULL,
                    "actual fog-state World Dimension receiver", 480, false);
    if (!mc_world_runtime_binding_retain(c->state.world)) {
        const mc_world_storage_error *error = mc_world_runtime_last_error(c->state.world);
        if (error != NULL)
            c->failure.world = *error;
        return fail(c, MC_CLIENT_RENDERER_FOG_STATE_NATIVE_FAILED,
                    "fog-state late World occurrence", 480, true);
    }
    c->world_held = true;
    mc_world_runtime_fields fields;
    const mc_world_storage_status status = mc_world_runtime_fields_get(c->state.world, &fields);
    if (status != MC_WORLD_STORAGE_OK) {
        c->failure.world = (mc_world_storage_error){status, "fog-state actual World.t access", 0};
        return fail(c, MC_CLIENT_RENDERER_FOG_STATE_NATIVE_FAILED,
                    "fog-state actual World.t access", 480, true);
    }
    if (fields.t == NULL)
        return fail(c, MC_CLIENT_RENDERER_FOG_STATE_SOURCE_NULL,
                    "actual fog-state Dimension.c receiver", 483, false);
    mc_world_provider_fields dimension;
    c->failure.dimension = mc_world_provider_fields_get(fields.t, &dimension);
    if (c->failure.dimension != MC_WORLD_PROVIDER_OK)
        return fail(c, MC_CLIENT_RENDERER_FOG_STATE_NATIVE_FAILED,
                    "fog-state actual Dimension.c access", 483, true);
    c->state.dimension_read = true;
    if (dimension.c) {
        if (!display_result(
                c, mc_client_display_source_fog_f(display, 2915, 0, &c->failure.display), 493))
            return false;
        c->state.start = 0;
        c->state.dimension_override = true;
    }
    return true;
}

static bool body(mc_client_renderer_fog_state_call *c) {
    mc_client_source_px *owner = c->state.owner;
    client_application *app = app_at(c, 4);
    if (app == NULL)
        return false;
    c->state.camera = app->i;
    c->code_registry = app->registry; /* Native code-namespace binding, not a World read. */
    c->state.camera_captured = true;
    if (c->state.camera != NULL) {
        c->failure.entity = mc_entity_retain(c->state.camera);
        if (c->failure.entity != MC_ENTITY_OK)
            return fail(c, MC_CLIENT_RENDERER_FOG_STATE_NATIVE_FAILED,
                        "fog-state captured camera occurrence", 7, true);
        c->camera_held = true;
    }
    c->state.rgba[0] = owner->fields.g;
    c->state.rgba[1] = owner->fields.h;
    c->state.rgba[2] = owner->fields.i;
    c->state.rgba[3] = 1;
    if (!color_buffer(c))
        return false;
    mc_client_display *display = owner->services.display;
    if (!display_result(
            c, mc_client_display_source_fog_color(display, c->state.rgba, &c->failure.display), 28))
        return false;
    c->state.color_set = true;
    if (!display_result(c, mc_client_display_source_normal(display, 0, -1, 0, &c->failure.display),
                        35))
        return false;
    c->state.normal_set = true;
    if (!display_result(c, mc_client_display_source_color(display, 1, 1, 1, 1, &c->failure.display),
                        42))
        return false;
    c->state.white_set = true;
    c->state.cloud = owner->fields.D;
    if (c->state.cloud) {
        const float rgb[] = {1, 1, 1};
        if (!exponential(c, 0.1f, rgb, 58, 66, 82, 85, 95))
            return false;
    } else {
        if (!immersion(c, false, &c->state.in_water))
            return false;
        c->state.water_tested = true;
        if (c->state.in_water) {
            const float rgb[] = {0.4f, 0.4f, 0.9f};
            if (!exponential(c, 0.1f, rgb, 176, 184, 203, 206, 216))
                return false;
        } else {
            if (!immersion(c, true, &c->state.in_lava))
                return false;
            c->state.lava_tested = true;
            if (c->state.in_lava) {
                const float rgb[] = {0.4f, 0.3f, 0.3f};
                if (!exponential(c, 2, rgb, 297, 304, 323, 326, 336))
                    return false;
            } else if (!linear(c))
                return false;
        }
    }
    if (!display_result(
            c, mc_client_display_source_color_material_enable(display, true, &c->failure.display),
            499))
        return false;
    c->state.color_material_enabled = true;
    if (!display_result(
            c, mc_client_display_source_color_material(display, 1028, 4608, &c->failure.display),
            508))
        return false;
    c->state.color_material_selected = c->state.returned = true;
    c->state.source_pc = 511;
    return true;
}

static mc_client_renderer_fog_state_status result(mc_client_renderer_fog_state_error *error,
                                                  mc_client_renderer_fog_state_status status,
                                                  const char *operation) {
    if (error != NULL)
        *error = (mc_client_renderer_fog_state_error){
            .status = status,
            .operation = operation,
            .native_policy = status != MC_CLIENT_RENDERER_FOG_STATE_OK};
    return status;
}

mc_client_renderer_fog_state_status
entity_renderer_fog_state_entered(mc_client_source_px *owner, int32_t pass, float partial,
                                  mc_client_renderer_fog_state_call **out,
                                  mc_client_renderer_fog_state_error *error) {
    if (owner == NULL || out == NULL || *out != NULL)
        return result(error, MC_CLIENT_RENDERER_FOG_STATE_ARGUMENT,
                      "fog-state native owner/output");
    if (!owner->active || owner->phase != MC_CLIENT_ENTITY_RENDERER_READY)
        return result(error, MC_CLIENT_RENDERER_FOG_STATE_BUSY, "fog-state borrowed parent guard");
    mc_client_renderer_fog_state_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return result(error, MC_CLIENT_RENDERER_FOG_STATE_ALLOCATION,
                      "fog-state retained invocation allocation");
    c->state.owner = owner;
    c->state.pass = pass;
    memcpy(&c->state.partial, &partial, sizeof(partial));
    c->state.failure = &c->failure;
    *out = c;
    c->state.active = true;
    (void)body(c);
    c->state.active = false;
    const mc_client_direct_float_buffer *buffer = owner->fields.f;
    if (buffer != NULL) {
        c->state.buffer = buffer;
        (void)mc_client_ge_float_inspect(buffer, &c->state.buffer_state);
    }
    if (error != NULL)
        *error = c->failure;
    return c->failure.status;
}

bool mc_client_entity_renderer_fog_state_inspect(const mc_client_renderer_fog_state_call *c,
                                                 mc_client_renderer_fog_state_view *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    return true;
}

mc_client_renderer_fog_state_status
mc_client_entity_renderer_fog_state_retire(mc_client_renderer_fog_state_call **slot,
                                           mc_client_renderer_fog_state_error *error) {
    if (slot == NULL)
        return result(error, MC_CLIENT_RENDERER_FOG_STATE_ARGUMENT, "fog-state retirement slot");
    mc_client_renderer_fog_state_call *c = *slot;
    if (c == NULL)
        return result(error, MC_CLIENT_RENDERER_FOG_STATE_OK, NULL);
    if (c->state.active)
        return result(error, MC_CLIENT_RENDERER_FOG_STATE_BUSY, "fog-state active retirement");
    if (c->world_held)
        mc_world_runtime_binding_release(c->state.world);
    if (c->camera_held)
        mc_entity_release(c->state.camera);
    free(c);
    *slot = NULL;
    return result(error, MC_CLIENT_RENDERER_FOG_STATE_OK, NULL);
}
