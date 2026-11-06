#include "render_global_clouds_internal.h"

#include "../../entity/entity_internal.h"
#include "../../entity/player/tick_math_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../application_state.h"
#include "../display/camera_surface.h"
#include "../display/fog_surface.h"
#include "../display/world_frame_surface.h"
#include "../render/model_graph_internal.h"
#include "mc/client/world/entity_renderer.h"
#include "render_global_internal.h"

#include <stdlib.h>

struct mc_client_render_global_clouds_call {
    mc_client_render_global_clouds_state state;
    mc_client_render_global_clouds_error failure, cleanup;
    mc_client_render_global_error owner_error, owner_cleanup;
    mc_math_classes *math;
    mc_client_tessellator *nw;
    mc_world_runtime *worlds[16];
    mc_world_provider *dimensions[4];
    mc_entity *cameras[16];
    size_t world_count, dimension_count, camera_count;
    mc_client_render_engine *engine;
    bool lookup_owned;
    mc_entity_vector *color;
    mc_entity_classes *color_classes;
};

static bool fail(mc_client_render_global_clouds_call *c, mc_client_render_global_status status,
                 const char *operation, unsigned int pc, bool native) {
    c->failure.status = status;
    c->failure.operation = operation;
    c->failure.site =
        (mc_client_graphics_site){"n", c->state.fancy ? "c" : "b", "(F)V", (int32_t)pc};
    c->failure.native_policy = native;
    c->state.source_pc = pc;
    return false;
}

static bool fp(mc_client_render_global_clouds_call *c, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u || fail(c, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP,
                                                       "cloud strict floating controls", pc, true);
}

static bool gl(mc_client_render_global_clouds_call *c, bool ok, unsigned int pc) {
    if (!ok)
        return fail(c,
                    c->failure.display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                        ? MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
                        : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                    c->failure.display.operation, pc, true);
    ++c->state.gl_calls;
    c->state.source_pc = pc;
    return true;
}

static bool graphics(mc_client_render_global_clouds_call *c, bool ok, unsigned int pc) {
    if (!ok) {
        c->failure.child = c->failure.graphics.site;
        return fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, c->failure.graphics.operation, pc,
                    c->failure.graphics.native_policy);
    }
    c->state.source_pc = pc;
    return true;
}

static client_application *app(mc_client_render_global_clouds_call *c, unsigned int pc) {
    client_application *a = c->state.owner->fields.t;
    if (a == NULL)
        fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual clouds App receiver", pc, false);
    return a;
}

static bool world_hold(mc_client_render_global_clouds_call *c, mc_world_runtime *world,
                       unsigned int pc) {
    if (world == NULL)
        return true;
    for (size_t i = 0; i < c->world_count; ++i)
        if (c->worlds[i] == world)
            return true;
    if (c->world_count == 16 || !mc_world_runtime_binding_retain(world))
        return fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "cloud World native occurrence", pc, true);
    c->worlds[c->world_count++] = world;
    return true;
}

static mc_world_provider *dimension(mc_client_render_global_clouds_call *c, mc_world_runtime *world,
                                    unsigned int pc) {
    if (!world_hold(c, world, pc))
        return NULL;
    if (world == NULL) {
        fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual cloud World Dimension receiver", pc,
             false);
        return NULL;
    }
    mc_world_runtime_fields fields;
    if (mc_world_runtime_fields_get(world, &fields) != MC_WORLD_STORAGE_OK) {
        fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "cloud actual World fields", pc, true);
        return NULL;
    }
    if (fields.t == NULL)
        return NULL;
    for (size_t i = 0; i < c->dimension_count; ++i)
        if (c->dimensions[i] == fields.t)
            return fields.t;
    if (c->dimension_count == 4 || mc_world_provider_retain(fields.t) != MC_WORLD_PROVIDER_OK) {
        fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "cloud Dimension native occurrence", pc, true);
        return NULL;
    }
    c->dimensions[c->dimension_count++] = fields.t;
    return fields.t;
}

static bool settings(mc_client_render_global_clouds_call *c, unsigned int app_pc, unsigned int pc,
                     mc_game_settings_state *out) {
    client_application *a = app(c, app_pc);
    if (a == NULL)
        return false;
    if (!mc_game_settings_read_state(a->settings, out))
        return fail(c,
                    a->settings == NULL ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                                        : MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                    "actual cloud Settings receiver", pc, a->settings != NULL);
    return true;
}

/* Reload the App and camera separately for every actual field operand. */
static bool coordinate(mc_client_render_global_clouds_call *c, unsigned int axis, unsigned int kind,
                       unsigned int app_pc, unsigned int field_pc, double *out) {
    client_application *a = app(c, app_pc);
    if (a == NULL)
        return false;
    mc_entity *camera = a->i;
    if (camera == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                    "actual cloud camera coordinate receiver", field_pc, false);
    bool held = false;
    for (size_t i = 0; i < c->camera_count; ++i)
        if (c->cameras[i] == camera)
            held = true;
    if (!held) {
        if (c->camera_count == 16 || mc_entity_retain(camera) != MC_ENTITY_OK)
            return fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "cloud camera native occurrence", field_pc,
                        true);
        c->cameras[c->camera_count++] = camera;
    }
    *out = kind == 0   ? (axis == 0   ? camera->base.x
                          : axis == 1 ? camera->base.y
                                      : camera->base.z)
           : kind == 1 ? (axis == 0   ? camera->base.aJ
                          : axis == 1 ? camera->base.aK
                                      : camera->base.aL)
                       : (axis == 0   ? camera->base.bl
                          : axis == 1 ? camera->base.bm
                                      : camera->base.bn);
    return true;
}

static bool interpolation(mc_client_render_global_clouds_call *c, unsigned int axis,
                          unsigned int kind, const unsigned int pc[3], unsigned int sub,
                          double *out) {
    double previous, current, repeated;
    if (!coordinate(c, axis, kind, pc[0] - 3u, pc[0], &previous) ||
        !coordinate(c, axis, 0, pc[1] - 3u, pc[1], &current) ||
        !coordinate(c, axis, kind, pc[2] - 3u, pc[2], &repeated) || !fp(c, sub))
        return false;
    *out = at_add(previous, at_mul(at_sub(current, repeated), (double)c->state.partial));
    return true;
}

static bool math_floor(mc_client_render_global_clouds_call *c, double value, unsigned int pc,
                       int32_t *out) {
    if (mc_math_classes_ensure(c->math, &c->failure.math) != MC_MATH_CLASS_OK)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY, c->failure.math.operation,
                    pc, true);
    if (!fp(c, pc))
        return false;
    *out = mc_math_floor_double(value);
    return true;
}

static bool texture(mc_client_render_global_clouds_call *c) {
    const unsigned int pc = c->state.fancy ? 225u : 103u;
    mc_client_render_engine *engine = c->state.owner->fields.l;
    if (engine == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                    "actual cloud texture Engine receiver", pc, false);
    if (mc_client_render_engine_retain(engine) != MC_CLIENT_RENDER_ENGINE_OK)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "cloud texture Engine native occurrence", pc,
                    true);
    c->engine = engine;
    static const uint16_t key[] = {'/', 'e', 'n', 'v', 'i', 'r', 'o', 'n', 'm', 'e', 'n', 't',
                                   '/', 'c', 'l', 'o', 'u', 'd', 's', '.', 'p', 'n', 'g'};
    const bool pending = mc_client_texture_lookup_pending(engine);
    mc_client_texture id;
    const mc_client_texture_lookup_status status = mc_client_render_engine_texture(
        engine, c->state.owner->display, NULL,
        (mc_texture_pack_text){key, sizeof key / sizeof(*key), false}, &id, &c->failure.texture);
    c->lookup_owned = !pending && mc_client_texture_lookup_pending(engine);
    if (status != MC_CLIENT_LOOKUP_OK) {
        c->failure.child = c->failure.texture.site;
        return fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                    c->failure.texture.operation, pc, c->failure.texture.native_policy);
    }
    if (!gl(c, mc_client_display_source_bind_id(c->state.owner->display, id, &c->failure.display),
            c->state.fancy ? 228u : 106u))
        return false;
    c->state.texture_bound = true;
    return true;
}

static bool cloud_color(mc_client_render_global_clouds_call *c) {
    const unsigned int pc = c->state.fancy ? 251u : 129u;
    mc_world_runtime *world = c->state.owner->fields.k;
    if (!world_hold(c, world, pc))
        return false;
    if (mc_world_runtime_cloud_color(world, c->state.partial, c->math, &c->color,
                                     &c->failure.colors) != MC_WORLD_STORAGE_OK) {
        const mc_world_color_error *e = &c->failure.colors;
        const mc_world_color_site *site = e->child.class_name == NULL ? &e->site : &e->child;
        c->failure.child = (mc_client_graphics_site){site->class_name, site->method,
                                                     site->descriptor, (int32_t)site->bytecode};
        return fail(c,
                    e->native_policy ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                                     : MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                    e->operation, pc, e->native_policy);
    }
    c->color_classes = mc_world_runtime_entities(world);
    c->state.color_vector = c->color;
    c->state.color_returned = true;
    if (c->color == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual cloud color Vec3 receiver",
                    c->state.fancy ? 258u : 136u, false);
    if (!fp(c, c->state.fancy ? 261u : 139u))
        return false;
    float *rgb = c->state.rgb;
    rgb[0] = (float)c->color->x;
    rgb[1] = (float)c->color->y;
    rgb[2] = (float)c->color->z;
    mc_game_settings_state options;
    if (!settings(c, c->state.fancy ? 284u : 162u, c->state.fancy ? 287u : 165u, &options))
        return false;
    c->state.anaglyph = options.values.anaglyph;
    if (c->state.anaglyph) {
        const float r = rgb[0], g = rgb[1], b = rgb[2];
        rgb[0] = at_fdiv(at_fadd(at_fadd(at_fmul(r, 30), at_fmul(g, 59)), at_fmul(b, 11)), 100);
        rgb[1] = at_fdiv(at_fadd(at_fmul(r, 30), at_fmul(g, 70)), 100);
        rgb[2] = at_fdiv(at_fadd(at_fmul(r, 30), at_fmul(b, 70)), 100);
    }
    return true;
}

static bool height(mc_client_render_global_clouds_call *c) {
    const unsigned int field = c->state.fancy ? 157u : 377u;
    const unsigned int pc = c->state.fancy ? 160u : 380u;
    mc_world_provider *d = dimension(c, c->state.owner->fields.k, field);
    if (c->failure.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    float value;
    if (mc_world_provider_cloud_height(d, &value, &c->failure.dimension) != MC_WORLD_PROVIDER_OK)
        return fail(c,
                    c->failure.dimension.native_policy ? MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE
                                                       : MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                    c->failure.dimension.operation, pc, c->failure.dimension.native_policy);
    if (!fp(c, c->state.fancy ? 164u : 384u))
        return false;
    c->state.height = at_fadd(at_fsub(value, c->state.camera_y), 0.33f);
    return true;
}

static bool nw_color(mc_client_render_global_clouds_call *c, float r, float g, float b,
                     unsigned int pc) {
    if (!fp(c, pc))
        return false;
    return graphics(c,
                    mc_client_nw_color(c->nw, mc_java_float_to_i32(at_fmul(r, 255)),
                                       mc_java_float_to_i32(at_fmul(g, 255)),
                                       mc_java_float_to_i32(at_fmul(b, 255)),
                                       mc_java_float_to_i32(at_fmul(0.8f, 255)),
                                       &c->failure.graphics),
                    pc);
}

static bool vertex(mc_client_render_global_clouds_call *c, double x, double y, double z, double u,
                   double v, unsigned int pc) {
    if (!fp(c, pc) ||
        !graphics(c, mc_client_nw_vertex_uv(c->nw, x, y, z, u, v, &c->failure.graphics), pc))
        return false;
    ++c->state.vertices;
    return true;
}

static bool draw(mc_client_render_global_clouds_call *c, unsigned int pc) {
    if (!graphics(c, mc_client_nw_draw(c->nw, c->state.owner->display, &c->failure.graphics), pc))
        return false;
    ++c->state.draws;
    return true;
}

static bool tail(mc_client_render_global_clouds_call *c) {
    mc_client_display *d = c->state.owner->display;
    if (!gl(c, mc_client_display_source_color(d, 1, 1, 1, 1, &c->failure.display),
            c->state.fancy ? 2069u : 653u) ||
        !gl(c, mc_client_display_source_blend_enable(d, false, &c->failure.display),
            c->state.fancy ? 2075u : 659u) ||
        !gl(c, mc_client_display_source_cull_test(d, true, &c->failure.display),
            c->state.fancy ? 2081u : 665u))
        return false;
    c->state.state_restored = c->state.returned = true;
    c->state.source_pc = c->state.fancy ? 2084u : 668u;
    return true;
}

static bool flat(mc_client_render_global_clouds_call *c) {
    const unsigned int ypc[3] = {49, 59, 69}, xpc[3] = {248, 258, 268}, zpc[3] = {297, 307, 317};
    double y;
    mc_client_display *d = c->state.owner->display;
    if (!gl(c, mc_client_display_source_cull_test(d, false, &c->failure.display), 39) ||
        !interpolation(c, 1, 2, ypc, 72, &y))
        return false;
    c->state.camera_y = (float)y;
    if (!graphics(c,
                  mc_client_nw_active_use(c->state.owner->classes,
                                          (mc_client_graphics_site){"n", "b", "(F)V", 89}, &c->nw,
                                          &c->failure.graphics),
                  89) ||
        !texture(c) ||
        !gl(c, mc_client_display_source_blend_enable(d, true, &c->failure.display), 112) ||
        !gl(c, mc_client_display_source_blend_function(d, 770, 771, &c->failure.display), 121) ||
        !cloud_color(c) || !interpolation(c, 0, 1, xpc, 271, &c->state.scroll_x))
        return false;
    c->state.scroll_x =
        at_add(c->state.scroll_x,
               (double)at_fmul(at_fadd((float)c->state.owner->fields.x, c->state.partial), 0.03f));
    if (!interpolation(c, 2, 1, zpc, 320, &c->state.scroll_z))
        return false;
    int32_t x, z;
    if (!math_floor(c, at_div(c->state.scroll_x, 2048.0), 333, &x) ||
        !math_floor(c, at_div(c->state.scroll_z, 2048.0), 344, &z))
        return false;
    c->state.scroll_x = at_sub(c->state.scroll_x, (double)mc_java_i32_mul(x, 2048));
    c->state.scroll_z = at_sub(c->state.scroll_z, (double)mc_java_i32_mul(z, 2048));
    if (!height(c))
        return false;
    const float scale = 0.00048828125f;
    c->state.offset_u = (float)at_mul(c->state.scroll_x, (double)scale);
    c->state.offset_v = (float)at_mul(c->state.scroll_z, (double)scale);
    if (!graphics(c, mc_client_nw_begin_quads(c->nw, &c->failure.graphics), 410) ||
        !nw_color(c, c->state.rgb[0], c->state.rgb[1], c->state.rgb[2], 423))
        return false;
    for (int32_t xx = -256; xx < 256; xx += 32)
        for (int32_t zz = -256; zz < 256; zz += 32) {
            const float u0 = at_fadd(at_fmul((float)xx, scale), c->state.offset_u);
            const float u1 = at_fadd(at_fmul((float)(xx + 32), scale), c->state.offset_u);
            const float v0 = at_fadd(at_fmul((float)zz, scale), c->state.offset_v);
            const float v1 = at_fadd(at_fmul((float)(zz + 32), scale), c->state.offset_v);
            if (!vertex(c, (double)xx, (double)c->state.height, (double)(zz + 32), u0, v1, 497) ||
                !vertex(c, (double)(xx + 32), (double)c->state.height, (double)(zz + 32), u1, v1,
                        539) ||
                !vertex(c, (double)(xx + 32), (double)c->state.height, (double)zz, u1, v0, 581) ||
                !vertex(c, (double)xx, (double)c->state.height, (double)zz, u0, v0, 623))
                return false;
            ++c->state.completed_cells;
        }
    if (!draw(c, 646))
        return false;
    c->state.passes = 1;
    return tail(c);
}

static bool normal(mc_client_render_global_clouds_call *c, float x, float y, float z,
                   unsigned int pc) {
    return graphics(c, mc_client_nw_normal(c->nw, x, y, z, &c->failure.graphics), pc);
}

static bool fancy_cell(mc_client_render_global_clouds_call *c, int32_t cell_x, int32_t cell_z) {
    if (!graphics(c, mc_client_nw_begin_quads(c->nw, &c->failure.graphics), 541) || !fp(c, 549))
        return false;
    const float tx = (float)(cell_x * 8), tz = (float)(cell_z * 8);
    const float x = at_fsub(tx, c->state.fraction_x), z = at_fsub(tz, c->state.fraction_z);
    const float h = c->state.height, epsilon = 0.0009765625f, tex = 0.00390625f;
    const float *rgb = c->state.rgb;
    const float ou = c->state.offset_u, ov = c->state.offset_v;
    /* Preserve all the source float additions, including the explicit +0 and
     * the epsilon subtraction after the preceding additions. */
#define ADD(a, b) at_fadd((a), (b))
#define SUB(a, b) at_fsub((a), (b))
#define U(a) ADD(at_fmul((a), tex), ou)
#define V(a) ADD(at_fmul((a), tex), ov)
#define P(a, b, d, u, v, pc)                                                                       \
    vertex(c, (double)(a), (double)(b), (double)(d), (double)(u), (double)(v), (pc))
    if (h > -5.0f) {
        if (!nw_color(c, at_fmul(rgb[0], 0.7f), at_fmul(rgb[1], 0.7f), at_fmul(rgb[2], 0.7f),
                      603) ||
            !normal(c, 0, -1, 0, 611) ||
            !P(ADD(x, 0), ADD(h, 0), ADD(z, 8), U(ADD(tx, 0)), V(ADD(tz, 8)), 656) ||
            !P(ADD(x, 8), ADD(h, 0), ADD(z, 8), U(ADD(tx, 8)), V(ADD(tz, 8)), 705) ||
            !P(ADD(x, 8), ADD(h, 0), ADD(z, 0), U(ADD(tx, 8)), V(ADD(tz, 0)), 750) ||
            !P(ADD(x, 0), ADD(h, 0), ADD(z, 0), U(ADD(tx, 0)), V(ADD(tz, 0)), 791))
            return false;
    }
    if (h <= 5.0f) {
        if (!nw_color(c, rgb[0], rgb[1], rgb[2], 813) || !normal(c, 0, 1, 0, 820) ||
            !P(ADD(x, 0), SUB(ADD(h, 4), epsilon), ADD(z, 8), U(ADD(tx, 0)), V(ADD(tz, 8)), 869) ||
            !P(ADD(x, 8), SUB(ADD(h, 4), epsilon), ADD(z, 8), U(ADD(tx, 8)), V(ADD(tz, 8)), 922) ||
            !P(ADD(x, 8), SUB(ADD(h, 4), epsilon), ADD(z, 0), U(ADD(tx, 8)), V(ADD(tz, 0)), 971) ||
            !P(ADD(x, 0), SUB(ADD(h, 4), epsilon), ADD(z, 0), U(ADD(tx, 0)), V(ADD(tz, 0)), 1016))
            return false;
    }
    if (!nw_color(c, at_fmul(rgb[0], 0.9f), at_fmul(rgb[1], 0.9f), at_fmul(rgb[2], 0.9f), 1037))
        return false;
    if (cell_x > -1) {
        if (!normal(c, -1, 0, 0, 1051))
            return false;
        for (int32_t slice = 0; slice < 8; ++slice) {
            const float n = (float)slice;
            if (!P(ADD(ADD(x, n), 0), ADD(h, 0), ADD(z, 8), U(ADD(ADD(tx, n), 0.5f)), V(ADD(tz, 8)),
                   1115) ||
                !P(ADD(ADD(x, n), 0), ADD(h, 4), ADD(z, 8), U(ADD(ADD(tx, n), 0.5f)), V(ADD(tz, 8)),
                   1170) ||
                !P(ADD(ADD(x, n), 0), ADD(h, 4), ADD(z, 0), U(ADD(ADD(tx, n), 0.5f)), V(ADD(tz, 0)),
                   1221) ||
                !P(ADD(ADD(x, n), 0), ADD(h, 0), ADD(z, 0), U(ADD(ADD(tx, n), 0.5f)), V(ADD(tz, 0)),
                   1271))
                return false;
        }
    }
    if (cell_x <= 1) {
        if (!normal(c, 1, 0, 0, 1290))
            return false;
        for (int32_t slice = 0; slice < 8; ++slice) {
            const float n = (float)slice;
            if (!P(SUB(ADD(ADD(x, n), 1), epsilon), ADD(h, 0), ADD(z, 8), U(ADD(ADD(tx, n), 0.5f)),
                   V(ADD(tz, 8)), 1357) ||
                !P(SUB(ADD(ADD(x, n), 1), epsilon), ADD(h, 4), ADD(z, 8), U(ADD(ADD(tx, n), 0.5f)),
                   V(ADD(tz, 8)), 1415) ||
                !P(SUB(ADD(ADD(x, n), 1), epsilon), ADD(h, 4), ADD(z, 0), U(ADD(ADD(tx, n), 0.5f)),
                   V(ADD(tz, 0)), 1469) ||
                !P(SUB(ADD(ADD(x, n), 1), epsilon), ADD(h, 0), ADD(z, 0), U(ADD(ADD(tx, n), 0.5f)),
                   V(ADD(tz, 0)), 1522))
                return false;
        }
    }
    if (!nw_color(c, at_fmul(rgb[0], 0.8f), at_fmul(rgb[1], 0.8f), at_fmul(rgb[2], 0.8f), 1549))
        return false;
    if (cell_z > -1) {
        if (!normal(c, 0, 0, -1, 1563))
            return false;
        for (int32_t slice = 0; slice < 8; ++slice) {
            const float n = (float)slice;
            if (!P(ADD(x, 0), ADD(h, 4), ADD(ADD(z, n), 0), U(ADD(tx, 0)), V(ADD(ADD(tz, n), 0.5f)),
                   1624) ||
                !P(ADD(x, 8), ADD(h, 4), ADD(ADD(z, n), 0), U(ADD(tx, 8)), V(ADD(ADD(tz, n), 0.5f)),
                   1679) ||
                !P(ADD(x, 8), ADD(h, 0), ADD(ADD(z, n), 0), U(ADD(tx, 8)), V(ADD(ADD(tz, n), 0.5f)),
                   1733) ||
                !P(ADD(x, 0), ADD(h, 0), ADD(ADD(z, n), 0), U(ADD(tx, 0)), V(ADD(ADD(tz, n), 0.5f)),
                   1783))
                return false;
        }
    }
    if (cell_z <= 1) {
        if (!normal(c, 0, 0, 1, 1802))
            return false;
        for (int32_t slice = 0; slice < 8; ++slice) {
            const float n = (float)slice;
            if (!P(ADD(x, 0), ADD(h, 4), SUB(ADD(ADD(z, n), 1), epsilon), U(ADD(tx, 0)),
                   V(ADD(ADD(tz, n), 0.5f)), 1866) ||
                !P(ADD(x, 8), ADD(h, 4), SUB(ADD(ADD(z, n), 1), epsilon), U(ADD(tx, 8)),
                   V(ADD(ADD(tz, n), 0.5f)), 1924) ||
                !P(ADD(x, 8), ADD(h, 0), SUB(ADD(ADD(z, n), 1), epsilon), U(ADD(tx, 8)),
                   V(ADD(ADD(tz, n), 0.5f)), 1981) ||
                !P(ADD(x, 0), ADD(h, 0), SUB(ADD(ADD(z, n), 1), epsilon), U(ADD(tx, 0)),
                   V(ADD(ADD(tz, n), 0.5f)), 2034))
                return false;
        }
    }
#undef P
#undef V
#undef U
#undef SUB
#undef ADD
    if (!draw(c, 2044))
        return false;
    ++c->state.completed_cells;
    return true;
}

static bool fancy(mc_client_render_global_clouds_call *c) {
    const unsigned int ypc[3] = {13, 23, 33}, xpc[3] = {62, 72, 82}, zpc[3] = {115, 125, 135};
    mc_client_display *d = c->state.owner->display;
    double y;
    if (!gl(c, mc_client_display_source_cull_test(d, false, &c->failure.display), 3) ||
        !interpolation(c, 1, 2, ypc, 36, &y))
        return false;
    c->state.camera_y = (float)y;
    if (!graphics(c,
                  mc_client_nw_active_use(c->state.owner->classes,
                                          (mc_client_graphics_site){"n", "c", "(F)V", 43}, &c->nw,
                                          &c->failure.graphics),
                  43) ||
        !interpolation(c, 0, 1, xpc, 85, &c->state.scroll_x))
        return false;
    const float motion = at_fmul(at_fadd((float)c->state.owner->fields.x, c->state.partial), 0.03f);
    c->state.scroll_x = at_div(at_add(c->state.scroll_x, (double)motion), 12.0);
    if (!interpolation(c, 2, 1, zpc, 138, &c->state.scroll_z))
        return false;
    c->state.scroll_z = at_add(at_div(c->state.scroll_z, 12.0), (double)0.33f);
    if (!height(c))
        return false;
    int32_t x, z;
    if (!math_floor(c, at_div(c->state.scroll_x, 2048.0), 176, &x) ||
        !math_floor(c, at_div(c->state.scroll_z, 2048.0), 187, &z))
        return false;
    c->state.scroll_x = at_sub(c->state.scroll_x, (double)mc_java_i32_mul(x, 2048));
    c->state.scroll_z = at_sub(c->state.scroll_z, (double)mc_java_i32_mul(z, 2048));
    if (!texture(c) ||
        !gl(c, mc_client_display_source_blend_enable(d, true, &c->failure.display), 234) ||
        !gl(c, mc_client_display_source_blend_function(d, 770, 771, &c->failure.display), 243) ||
        !cloud_color(c) || !fp(c, 362))
        return false;
    /* These unused products execute before the later floor calls overwrite
     * their locals; no source result is substituted for them. */
    volatile float unused_x = (float)at_mul(c->state.scroll_x, 0.0);
    volatile float unused_z = (float)at_mul(c->state.scroll_z, 0.0);
    (void)unused_x;
    (void)unused_z;
    if (!math_floor(c, c->state.scroll_x, 379, &x))
        return false;
    c->state.offset_u = at_fmul((float)x, 0.00390625f);
    if (!math_floor(c, c->state.scroll_z, 390, &z))
        return false;
    c->state.offset_v = at_fmul((float)z, 0.00390625f);
    if (!math_floor(c, c->state.scroll_x, 403, &x))
        return false;
    c->state.fraction_x = (float)at_sub(c->state.scroll_x, (double)x);
    if (!math_floor(c, c->state.scroll_z, 415, &z))
        return false;
    c->state.fraction_z = (float)at_sub(c->state.scroll_z, (double)z);
    if (!gl(c, mc_client_display_source_scale(d, 12.0, 1.0, 12.0, &c->failure.display), 439))
        return false;
    c->state.scaled = true;
    for (unsigned int pass = 0; pass < 2; ++pass) {
        if (pass == 0) {
            if (!gl(c,
                    mc_client_display_source_color_mask(d, false, false, false, false,
                                                        &c->failure.display),
                    460))
                return false;
        } else {
            mc_game_settings_state options;
            if (!settings(c, 470, 473, &options))
                return false;
            bool red = true, green = true, blue = true;
            unsigned int pc = 509;
            if (options.values.anaglyph) {
                mc_client_render_classes *models = c->state.owner->fields.t->render_classes;
                if (mc_client_entity_renderer_class_active_use(models, NULL) !=
                    MC_CLIENT_ENTITY_RENDERER_OK)
                    return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                                "cloud actual PX defining namespace", 479, true);
                if (models->px_b == 0) {
                    red = false;
                    pc = 489;
                } else {
                    green = blue = false;
                    pc = 499;
                }
            }
            if (!gl(c,
                    mc_client_display_source_color_mask(d, red, green, blue, true,
                                                        &c->failure.display),
                    pc))
                return false;
        }
        for (int32_t xx = -2; xx <= 3; ++xx)
            for (int32_t zz = -2; zz <= 3; ++zz)
                if (!fancy_cell(c, xx, zz))
                    return false;
        ++c->state.passes;
    }
    return tail(c);
}

static bool body(mc_client_render_global_clouds_call *c) {
    client_application *a = app(c, 4);
    if (a == NULL)
        return false;
    mc_world_provider *provider = dimension(c, a->active_world, 7);
    if (c->failure.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    if (provider == NULL)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual cloud Dimension c receiver",
                    10, false);
    mc_world_provider_fields fields;
    if (mc_world_provider_fields_get(provider, &fields) != MC_WORLD_PROVIDER_OK)
        return fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "cloud actual Dimension flags", 10,
                    true);
    if (fields.c) {
        c->state.nether_skipped = c->state.returned = true;
        c->state.source_pc = 16;
        return true;
    }
    mc_game_settings_state options;
    if (!settings(c, 21, 24, &options))
        return false;
    c->state.fancy = options.values.fancy_graphics;
    if (c->state.fancy)
        return fancy(c);
    return flat(c);
}

static mc_client_render_global_status admission(mc_client_render_global_clouds_error *error,
                                                mc_client_render_global_status status,
                                                const char *operation) {
    if (error != NULL)
        *error = (mc_client_render_global_clouds_error){.status = status,
                                                        .operation = operation,
                                                        .native_policy =
                                                            status != MC_CLIENT_RENDER_GLOBAL_OK};
    return status;
}

mc_client_render_global_status
mc_client_render_global_clouds_begin(mc_client_render_global *owner, mc_math_classes *math,
                                     float partial, mc_client_render_global_clouds_call **out,
                                     mc_client_render_global_clouds_error *error) {
    if (owner == NULL || math == NULL || out == NULL || *out != NULL)
        return admission(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                         "cloud native owner/Math/output");
    mc_client_render_global_world_state attachment = {0};
    const bool attached = owner->world_call == NULL ||
                          (mc_client_render_global_world_inspect(owner->world_call, &attachment) &&
                           !attachment.active && attachment.source_returned);
    if (owner->active || owner->world_retiring || !attached || owner->clouds_call != NULL ||
        owner->sky_call != NULL || owner->entities_call != NULL || owner->cull_call != NULL ||
        owner->update_call != NULL || owner->submit_call != NULL ||
        owner->phase == MC_CLIENT_RENDER_GLOBAL_CONSTRUCTING ||
        owner->phase == MC_CLIENT_RENDER_GLOBAL_RETIRING)
        return admission(error, MC_CLIENT_RENDER_GLOBAL_BUSY,
                         "cloud native entered/pending/retiring owner");
    mc_client_render_global_clouds_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return admission(error, MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
                         "cloud retained call allocation");
    if (mc_client_render_global_retain(owner, &c->owner_error) != MC_CLIENT_RENDER_GLOBAL_OK) {
        const mc_client_render_global_status status = c->owner_error.status;
        free(c);
        return admission(error, status, "cloud N native occurrence");
    }
    c->state.owner = owner;
    c->state.partial = partial;
    c->state.failure = &c->failure;
    c->state.cleanup = &c->cleanup;
    c->failure.owner = &c->owner_error;
    owner->clouds_call = c;
    *out = c;
    if (mc_math_classes_retain(math, &c->failure.math) != MC_MATH_CLASS_OK)
        fail(c, MC_CLIENT_RENDER_GLOBAL_BUSY, "cloud Math native occurrence", 0, true);
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

bool mc_client_render_global_clouds_inspect(const mc_client_render_global_clouds_call *c,
                                            mc_client_render_global_clouds_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    return true;
}

static mc_client_render_global_status cleanup_failed(mc_client_render_global_clouds_call *c,
                                                     mc_client_render_global_clouds_error *out,
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
mc_client_render_global_clouds_retire(mc_client_render_global_clouds_call **slot,
                                      mc_client_render_global_clouds_error *error) {
    if (slot == NULL)
        return admission(error, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT, "cloud retirement slot");
    mc_client_render_global_clouds_call *c = *slot;
    if (c == NULL)
        return admission(error, MC_CLIENT_RENDER_GLOBAL_OK, NULL);
    mc_client_render_global *owner = c->state.owner;
    if (c->state.active || owner->active)
        return admission(error, MC_CLIENT_RENDER_GLOBAL_BUSY, "cloud active cleanup");
    c->state.retiring = true;
    if (c->engine != NULL) {
        if (c->lookup_owned && mc_client_render_engine_retire_lookup(
                                   c->engine, &c->cleanup.texture) != MC_CLIENT_LOOKUP_OK)
            return cleanup_failed(c, error, "cloud owned texture attempt cleanup");
        c->lookup_owned = false;
        if (mc_client_render_engine_release(c->engine) != MC_CLIENT_RENDER_ENGINE_OK)
            return cleanup_failed(c, error, "cloud Engine cleanup");
        c->engine = NULL;
    }
    if (c->color != NULL) {
        if (mc_entity_vector_release(c->color_classes, c->color) != MC_ENTITY_OK)
            return cleanup_failed(c, error, "cloud pooled color Vec3 cleanup");
        c->color = NULL;
        c->state.color_vector = NULL;
    }
    while (c->camera_count != 0)
        mc_entity_release(c->cameras[--c->camera_count]);
    while (c->dimension_count != 0)
        mc_world_provider_release(c->dimensions[--c->dimension_count]);
    while (c->world_count != 0)
        mc_world_runtime_binding_release(c->worlds[--c->world_count]);
    if (mc_math_classes_release(&c->math, &c->cleanup.math) != MC_MATH_CLASS_OK)
        return cleanup_failed(c, error, "cloud Math cleanup");
    owner->clouds_call = NULL;
    mc_client_render_global *held = owner;
    if (mc_client_render_global_release(&held, &c->owner_cleanup) != MC_CLIENT_RENDER_GLOBAL_OK) {
        owner->clouds_call = c;
        return cleanup_failed(c, error, "cloud N occurrence cleanup");
    }
    free(c);
    *slot = NULL;
    return admission(error, MC_CLIENT_RENDER_GLOBAL_OK, NULL);
}
