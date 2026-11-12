#include "application_quad.h"

#include "mc/core/java/java.h"

#include <xmmintrin.h>

#define MC_CLASS "net/minecraft/client/Minecraft"
#define QUAD_DESCRIPTOR "(IIIIII)V"

static bool vertex(mc_client_tessellator *nw, int32_t x, int32_t y, int32_t u, int32_t v,
                   int32_t conversion_pc, int32_t invoke_pc,
                   mc_client_application_quad_error *error) {
    error->pc = conversion_pc;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        mc_client_graphics_fail(
            &error->graphics, MC_CLIENT_GRAPHICS_UNSUPPORTED_FP,
            "Minecraft textured rectangle reached native FP conversion",
            (mc_client_graphics_site){MC_CLASS, "a", QUAD_DESCRIPTOR, conversion_pc}, true);
        return false;
    }
    const double px = (double)x, py = (double)y;
    /* Source i2f/fmul/f2d, not a double UV expression narrowed by nw later. */
    volatile float uf = (float)u, vf = (float)v;
    volatile float scaled_u = uf * 0.00390625f;
    volatile float scaled_v = vf * 0.00390625f;
    error->pc = invoke_pc;
    return mc_client_nw_vertex_uv(nw, px, py, 0.0, (double)scaled_u, (double)scaled_v,
                                  &error->graphics);
}

bool mc_client_application_textured_rectangle(mc_client_render_engine_classes *classes,
                                              mc_client_display *display, int32_t x, int32_t y,
                                              int32_t texture_x, int32_t texture_y, int32_t width,
                                              int32_t height,
                                              mc_client_application_quad_error *error) {
    if (error == NULL)
        return false;
    *error = (mc_client_application_quad_error){.pc = 8};
    mc_client_tessellator *nw = NULL;
    if (!mc_client_nw_active_use(classes,
                                 (mc_client_graphics_site){MC_CLASS, "a", QUAD_DESCRIPTOR, 8}, &nw,
                                 &error->graphics))
        return false;
    error->pc = 15;
    if (!mc_client_nw_begin_quads(nw, &error->graphics))
        return false;
    if (!vertex(nw, x, mc_java_i32_add(y, height), texture_x, mc_java_i32_add(texture_y, height),
                23, 48, error) ||
        !vertex(nw, mc_java_i32_add(x, width), mc_java_i32_add(y, height),
                mc_java_i32_add(texture_x, width), mc_java_i32_add(texture_y, height), 57, 83,
                error) ||
        !vertex(nw, mc_java_i32_add(x, width), y, mc_java_i32_add(texture_x, width), texture_y, 92,
                116, error) ||
        !vertex(nw, x, y, texture_x, texture_y, 124, 147, error))
        return false;
    error->pc = 152;
    if (!mc_client_nw_draw(nw, display, &error->graphics))
        return false;
    error->pc = 155;
    return true;
}
