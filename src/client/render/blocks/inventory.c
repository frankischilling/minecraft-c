#include "internal.h"

#include "../../../entity/player/tick_math_internal.h"
#include "../../display/source_surface.h"

bool mc_cv_inventory_type_3d(int32_t type) {
    return type == 0 || type == 13 || type == 10 || type == 11 || type == 16;
}

static bool inventory_gl(mc_cv_context *c, bool ok, unsigned int pc) {
    mc_cv_pc(c, pc);
    return ok || mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                            c->failure.display.operation, pc, true);
}

static bool inventory_nw(mc_cv_context *c, bool ok, unsigned int pc) {
    mc_cv_pc(c, pc);
    if (ok)
        return true;
    c->failure.child = c->failure.graphics.site;
    return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, c->failure.graphics.operation, pc,
                      c->failure.graphics.native_policy);
}

bool mc_cv_inventory(mc_cv_context *c, const mc_block *block, int32_t metadata, float brightness) {
    if (!mc_cv_push(c, "a", "(Luu;IF)V") || !mc_cv_nw(c, 0))
        return false;
    if (c->fields->b) {
        if (block == NULL)
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                              "inventory Block color null receiver", 14, false);
        int32_t color;
        const mc_registry_status status = mc_block_render_inventory_color(block, metadata, &color);
        if (status != MC_REGISTRY_OK) {
            c->failure.child = (mc_client_graphics_site){"uu", "b", "(I)I", 0};
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                              "inventory actual Block metadata-color body", 14, true);
        }
        if (!mc_cv_fp(c, 31))
            return false;
        const float red = at_fdiv((float)(((uint32_t)color >> 16u) & 255u), 255.0f);
        const float green = at_fdiv((float)(((uint32_t)color >> 8u) & 255u), 255.0f);
        const float blue = at_fdiv((float)((uint32_t)color & 255u), 255.0f);
        if (!inventory_gl(c,
                          mc_client_display_source_color(
                              c->display, at_fmul(red, brightness), at_fmul(green, brightness),
                              at_fmul(blue, brightness), 1.0f, &c->failure.display),
                          74))
            return false;
    }
    if (block == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "inventory Block render-type null receiver", 78, false);
    mc_block_render_description description;
    if (mc_block_render_describe(block, &description) != MC_REGISTRY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                          "inventory actual Block render-type dispatch", 78, true);
    if (description.type != 0 && description.type != 16)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                          "inventory actual specialized Block geometry", 324, true);
    if (description.type == 16)
        metadata = 1;
    /* Registry lookup exposes a const view; the actual Source Block receiver
     * remains mutable and its virtual g() may replace its live bounds. */
    if (mc_block_render_inventory_shape((mc_block *)(void *)block) != MC_REGISTRY_OK) {
        c->failure.child = (mc_client_graphics_site){"uu", "g", "()V", 0};
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                          "inventory actual Block item-bound hook", 105, true);
    }
    if (!inventory_gl(c,
                      mc_client_display_source_translate(c->display, -0.5f, -0.5f, -0.5f,
                                                         &c->failure.display),
                      114))
        return false;
    static const float normals[6][3] = {{0, -1, 0}, {0, 1, 0},  {0, 0, -1},
                                        {0, 0, 1},  {-1, 0, 0}, {1, 0, 0}};
    static const unsigned int begin[6] = {119, 152, 184, 217, 249, 282};
    static const unsigned int normal[6] = {128, 160, 193, 225, 258, 290};
    static const unsigned int texture[6] = {139, 171, 204, 236, 269, 301};
    static const unsigned int face[6] = {142, 174, 207, 239, 272, 304};
    static const unsigned int draw[6] = {147, 179, 212, 244, 277, 309};
    for (int32_t side = 0; side < 6; ++side) {
        if (!inventory_nw(c, mc_client_nw_begin_quads(c->tessellator, &c->failure.graphics),
                          begin[side]) ||
            !inventory_nw(c,
                          mc_client_nw_normal(c->tessellator, normals[side][0], normals[side][1],
                                              normals[side][2], &c->failure.graphics),
                          normal[side]))
            return false;
        int32_t icon;
        if (mc_block_render_texture_metadata(block, side, metadata, &icon) != MC_REGISTRY_OK) {
            c->failure.child = (mc_client_graphics_site){"uu", "a", "(II)I", 0};
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                              "inventory actual Block face texture", texture[side], true);
        }
        if (!mc_cv_face(c, side, block, 0.0, 0.0, 0.0, icon, face[side]) ||
            !inventory_nw(c, mc_client_nw_draw(c->tessellator, c->display, &c->failure.graphics),
                          draw[side]))
            return false;
        ++c->inventory_faces;
    }
    if (!inventory_gl(
            c,
            mc_client_display_source_translate(c->display, 0.5f, 0.5f, 0.5f, &c->failure.display),
            318))
        return false;
    mc_cv_pc(c, 1410);
    c->returned = true;
    mc_cv_pop(c);
    return true;
}
