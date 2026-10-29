#include "../display/source_surface.h"
#include "render_global_internal.h"

static bool vertex(mc_client_render_global *o, int32_t x, float y, int32_t z, unsigned int pc) {
    o->last_constructor_pc = pc;
    if (!mc_render_global_fp(o, "<init>", pc))
        return false;
    return mc_client_nw_vertex(o->tessellator, (double)x, (double)y, (double)z,
                               &o->failure.graphics) ||
           mc_render_global_graphics(o, pc);
}

bool mc_render_global_sky(mc_client_render_global *o) {
    o->fields.z = mc_java_i32_add(o->fields.y, 1);
    o->last_store_pc = 306;
    o->last_constructor_pc = 316;
    if (!mc_client_display_lists_begin(o->display, o->fields.z, &o->failure.display))
        return mc_render_global_display(o, 316);
    o->open_list = o->fields.z;
    const int32_t step = 64;
    const int32_t extent = 256 / step + 2; /* Actual local imul/ineg loop bounds. */
    const int32_t maximum = step * extent;
    for (int32_t x = -maximum; x <= maximum; x += step) {
        for (int32_t z = -maximum; z <= maximum; z += step) {
            o->last_constructor_pc = 375;
            if (!mc_client_nw_begin_quads(o->tessellator, &o->failure.graphics))
                return mc_render_global_graphics(o, 375);
            if (!vertex(o, x, 16.0f, z, 393) ||
                !vertex(o, mc_java_i32_add(x, step), 16.0f, z, 412) ||
                !vertex(o, mc_java_i32_add(x, step), 16.0f, mc_java_i32_add(z, step), 432) ||
                !vertex(o, x, 16.0f, mc_java_i32_add(z, step), 451))
                return false;
            o->last_constructor_pc = 456;
            if (!mc_client_nw_draw(o->tessellator, o->display, &o->failure.graphics))
                return mc_render_global_graphics(o, 456);
            ++o->top_quads;
        }
    }
    o->last_constructor_pc = 479;
    if (!mc_client_display_lists_end(o->display, &o->failure.display))
        return mc_render_global_display(o, 479);
    o->open_list = 0;
    o->fields.A = mc_java_i32_add(o->fields.y, 2);
    o->last_store_pc = 489;
    o->last_constructor_pc = 499;
    if (!mc_client_display_lists_begin(o->display, o->fields.A, &o->failure.display))
        return mc_render_global_display(o, 499);
    o->open_list = o->fields.A;
    o->last_constructor_pc = 508;
    if (!mc_client_nw_begin_quads(o->tessellator, &o->failure.graphics))
        return mc_render_global_graphics(o, 508);
    for (int32_t x = -maximum; x <= maximum; x += step) {
        for (int32_t z = -maximum; z <= maximum; z += step) {
            if (!vertex(o, mc_java_i32_add(x, step), -16.0f, z, 563) ||
                !vertex(o, x, -16.0f, z, 582) ||
                !vertex(o, x, -16.0f, mc_java_i32_add(z, step), 600) ||
                !vertex(o, mc_java_i32_add(x, step), -16.0f, mc_java_i32_add(z, step), 620))
                return false;
            ++o->bottom_quads;
        }
    }
    o->last_constructor_pc = 645;
    if (!mc_client_nw_draw(o->tessellator, o->display, &o->failure.graphics))
        return mc_render_global_graphics(o, 645);
    o->last_constructor_pc = 648;
    if (!mc_client_display_lists_end(o->display, &o->failure.display))
        return mc_render_global_display(o, 648);
    o->open_list = 0;
    o->last_constructor_pc = 651;
    o->failure.source_class = "n";
    o->failure.source_method = "<init>";
    o->failure.source_descriptor = "(Lnet/minecraft/client/Minecraft;Lji;)V";
    o->failure.source_pc = 651;
    return true;
}
