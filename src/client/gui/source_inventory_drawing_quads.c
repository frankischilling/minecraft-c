#include "source_inventory_drawing_internal.h"
#include "source_inventory_quad_kernels.h"

static bool nw(mc_client_inventory_draw_call *c, bool ok, unsigned int pc, const char *method,
               const char *descriptor) {
    if (ok)
        return true;
    c->error.screen.parent_site = (mc_client_screen_site){"ub", method, descriptor, pc};
    const mc_client_graphics_site s = c->error.graphics.site;
    const mc_client_screen_status status =
        c->error.graphics.status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP
            ? MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY
        : c->error.graphics.status == MC_CLIENT_GRAPHICS_NULL_REFERENCE &&
                !c->error.graphics.native_policy
            ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
            : MC_CLIENT_SCREEN_TEXTURE_FAILED;
    return mc_inventory_draw_fail(c, status, c->error.graphics.operation, s.class_name, s.method,
                                  s.descriptor, (unsigned int)s.pc,
                                  c->error.graphics.native_policy);
}

static bool entered(mc_client_inventory_draw_call *c, const mc_inventory_quad_context *prefix,
                    bool ok, const char *method) {
    c->error.graphics = prefix->error;
    return nw(c, ok, prefix->source_pc, method, "(IIIIII)V");
}

bool mc_inventory_draw_texture(mc_client_inventory_draw_call *c, mc_client_render_engine *engine,
                               const char *path, const char *owner, unsigned int pc,
                               mc_client_texture *out) {
    /* The source String literal is represented by its exact UTF16 content;
     * JI consumes a borrowed String view without substituting a cache query. */
    uint16_t units[64];
    size_t n = 0;
    while (path[n] != 0 && n < sizeof(units) / sizeof(*units)) {
        units[n] = (uint8_t)path[n];
        ++n;
    }
    if (path[n] != 0)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_LIMIT, "Inventory texture literal bound",
                                      owner, "a", mc_inventory_draw_descriptor(c, owner), pc, true);
    if (engine == NULL)
        return mc_inventory_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                      "Inventory actual JI null receiver", owner, "a",
                                      mc_inventory_draw_descriptor(c, owner), pc, false);
    const bool pending_on_entry = mc_client_texture_lookup_pending(engine);
    if (mc_client_render_engine_texture(engine, c->services.display, NULL,
                                        (mc_texture_pack_text){units, n, false}, out,
                                        &c->error.texture) != MC_CLIENT_LOOKUP_OK) {
        if (!pending_on_entry && mc_client_texture_lookup_pending(engine))
            c->pending_lookup = engine;
        c->error.screen.parent_site =
            (mc_client_screen_site){owner, "a", mc_inventory_draw_descriptor(c, owner), pc};
        return mc_inventory_draw_fail(
            c, MC_CLIENT_SCREEN_TEXTURE_FAILED, c->error.texture.operation,
            c->error.texture.site.class_name, c->error.texture.site.method,
            c->error.texture.site.descriptor, (unsigned int)c->error.texture.site.pc,
            c->error.texture.native_policy);
    }
    return true;
}

bool mc_inventory_draw_gradient(mc_client_inventory_draw_call *c, int32_t left, int32_t top,
                                int32_t right, int32_t bottom, uint32_t upper, uint32_t lower) {
    const char *desc = "(IIIIII)V";
    mc_inventory_quad_context prefix = {0};
    mc_inventory_quad_colors colors;
    if (!entered(c, &prefix,
                 mc_inventory_quad_gradient_colors_capture(&prefix, upper, lower, &colors), "a"))
        return false;
    mc_client_display *d = c->services.display;
#define GL(expr, pc)                                                                               \
    do {                                                                                           \
        if (!mc_inventory_draw_gl(c, (expr), "ub", (pc)))                                          \
            return false;                                                                          \
    } while (0)
    GL(mc_client_display_source_texture_enable(d, false, &c->error.display), 117);
    GL(mc_client_display_source_blend_enable(d, true, &c->error.display), 123);
    GL(mc_client_display_source_alpha_test(d, false, &c->error.display), 129);
    GL(mc_client_display_source_blend_function(d, 770, 771, &c->error.display), 138);
    GL(mc_client_display_source_shade_model(d, 7425, &c->error.display), 144);
    mc_client_tessellator *t = NULL;
    if (!nw(c,
            mc_client_nw_active_use(c->services.graphics,
                                    (mc_client_graphics_site){"ub", "a", desc, 147}, &t,
                                    &c->error.graphics),
            147, "a", desc))
        return false;
    prefix.nw = t;
    if (!entered(c, &prefix,
                 mc_inventory_quad_gradient_entered(&prefix, &colors, left, top, right, bottom),
                 "a") ||
        !nw(c, mc_client_nw_draw(t, d, &c->error.graphics), 227, "a", desc))
        return false;
    GL(mc_client_display_source_shade_model(d, 7424, &c->error.display), 233);
    GL(mc_client_display_source_blend_enable(d, false, &c->error.display), 239);
    GL(mc_client_display_source_alpha_test(d, true, &c->error.display), 245);
    GL(mc_client_display_source_texture_enable(d, true, &c->error.display), 251);
#undef GL
    return true;
}

bool mc_inventory_draw_tile(mc_client_inventory_draw_call *c, int32_t x, int32_t y, int32_t u,
                            int32_t v, int32_t width, int32_t height) {
    const char *desc = "(IIIIII)V";
    mc_client_tessellator *t = NULL;
    if (!nw(c,
            mc_client_nw_active_use(c->services.graphics,
                                    (mc_client_graphics_site){"ub", "b", desc, 8}, &t,
                                    &c->error.graphics),
            8, "b", desc))
        return false;
    mc_inventory_quad_context prefix = {.nw = t};
    if (!entered(c, &prefix,
                 mc_inventory_quad_tile_entered(&prefix, &c->owner->base.fields.k, x, y, u, v,
                                                width, height),
                 "b"))
        return false;
    return nw(c, mc_client_nw_draw(t, c->services.display, &c->error.graphics), 168, "b", desc);
}
