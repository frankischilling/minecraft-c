#include "client/display/fog_surface.h"
#include "source_inventory_quad_kernels.h"
#include "source_sign_drawing_internal.h"

bool mc_sign_draw_gradient(mc_client_sign_draw_call *c, int32_t width, int32_t height) {
    static const char desc[] = "(IIIIII)V";
    mc_inventory_quad_context prefix = {0};
    mc_inventory_quad_colors colors;
    if (!mc_inventory_quad_gradient_colors_capture(&prefix, UINT32_C(0xc0101010),
                                                   UINT32_C(0xd0101010), &colors)) {
        c->error.graphics = prefix.error;
        return mc_sign_draw_nw(c, false, "a", desc, prefix.source_pc);
    }
    mc_client_display *d = c->services.draw.display;
#define GL(expr, pc)                                                                               \
    do {                                                                                           \
        if (!mc_sign_draw_gl(c, (expr), "ub", "a", desc, pc))                                      \
            return false;                                                                          \
    } while (0)
    GL(mc_client_display_source_texture_enable(d, false, &c->error.display), 117);
    GL(mc_client_display_source_blend_enable(d, true, &c->error.display), 123);
    GL(mc_client_display_source_alpha_test(d, false, &c->error.display), 129);
    GL(mc_client_display_source_blend_function(d, 770, 771, &c->error.display), 138);
    GL(mc_client_display_source_shade_model(d, 7425, &c->error.display), 144);
    mc_client_tessellator *nw = NULL;
    if (!mc_sign_draw_nw(c,
                         mc_client_nw_active_use(c->services.draw.graphics,
                                                 (mc_client_graphics_site){"ub", "a", desc, 147},
                                                 &nw, &c->error.graphics),
                         "a", desc, 147))
        return false;
    prefix.nw = nw;
    if (!mc_inventory_quad_gradient_entered(&prefix, &colors, 0, 0, width, height)) {
        c->error.graphics = prefix.error;
        return mc_sign_draw_nw(c, false, "a", desc, prefix.source_pc);
    }
    if (!mc_sign_draw_nw(c, mc_client_nw_draw(nw, d, &c->error.graphics), "a", desc, 227))
        return false;
    GL(mc_client_display_source_shade_model(d, 7424, &c->error.display), 233);
    GL(mc_client_display_source_blend_enable(d, false, &c->error.display), 239);
    GL(mc_client_display_source_alpha_test(d, true, &c->error.display), 245);
    GL(mc_client_display_source_texture_enable(d, true, &c->error.display), 251);
#undef GL
    return true;
}
