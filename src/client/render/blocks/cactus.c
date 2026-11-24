#include "internal.h"

#include "../../../entity/player/tick_math_internal.h"
#include "../model_graph_internal.h"
#include "mc/client/world/entity_renderer.h"
#include "mc/core/java/java.h"

static bool cactus_light(mc_cv_context *c, const mc_block *block, int32_t side, float center,
                         float *light) {
    static const unsigned int field_pc[6] = {0, 256, 368, 489, 610, 731};
    static const unsigned int compare_pc[6] = {0, 260, 372, 493, 614, 735};
    if (side == 0)
        return true;
    const mc_block_fields *fields;
    if (!mc_cv_fields(c, block, field_pc[side], &fields) || !mc_cv_fp(c, compare_pc[side]))
        return false;
    if (side == 1) {
        /* dcmpl/ifeq reaches Material.d for NaN as well as partial tops. The
         * neighbor brightness query has already returned before this read. */
        if (fields->bw != 1.0) {
            mc_cv_pc(c, 268);
            if (fields->bA == NULL)
                return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                  "actual Cactus top null Material receiver", 268, false);
            if (!mc_material_d(fields->bA))
                *light = center;
        }
    } else if ((side == 2 && fields->bu > 0.0) || (side == 3 && fields->bx < 1.0) ||
               (side == 4 && fields->bs > 0.0) || (side == 5 && fields->bv < 1.0)) {
        *light = center;
    }
    return true;
}

/* Actual cv.c(Block,III,FFF). Its caller already owns a begun NW batch.
 * Unlike the flat standard body, this method leaves every CV field intact,
 * including the live AO flag and corner scratch used by the face methods. */
bool mc_cv_cactus_geometry(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                           float red, float green, float blue, bool *out) {
    if (!mc_cv_nw(c, 0) || !mc_cv_fp(c, 27))
        return false;
    static const float scale[4] = {.5f, 1, .8f, .6f};
    const float input[3] = {red, green, blue};
    float shaded[4][3];
    /* Preserve the original channel-major products, including the top *1. */
    for (size_t channel = 0; channel < 3; ++channel)
        for (size_t shade = 0; shade < 4; ++shade)
            shaded[shade][channel] = at_fmul(scale[shade], input[channel]);
    float center;
    if (!mc_cv_brightness(c, block, x, y, z, 120, &center))
        return false;
    static const int32_t neighbor[6][3] = {{0, -1, 0}, {0, 1, 0},  {0, 0, -1},
                                           {0, 0, 1},  {-1, 0, 0}, {1, 0, 0}};
    static const unsigned int side_pc[6] = {144, 233, 345, 466, 587, 708};
    static const unsigned int light_pc[6] = {161, 250, 362, 483, 604, 725};
    static const unsigned int multiply_pc[6] = {172, 284, 386, 507, 628, 749};
    static const unsigned int color_pc[6] = {183, 295, 397, 518, 639, 760};
    static const unsigned int texture_pc[6] = {205, 317, 428, 550, 670, 792};
    static const unsigned int face_pc[6] = {208, 320, 431, 553, 673, 795};
    static const unsigned int offset_pc[4][2] = {{406, 441}, {528, 562}, {648, 683}, {770, 804}};
    /* Source fconst_0 stays positive, including the inverse translations. */
    static const float offset[4][2][3] = {{{0, 0, .0625f}, {0, 0, -.0625f}},
                                          {{0, 0, -.0625f}, {0, 0, .0625f}},
                                          {{.0625f, 0, 0}, {-.0625f, 0, 0}},
                                          {{-.0625f, 0, 0}, {.0625f, 0, 0}}};
    bool rendered = false;
    for (int32_t side = 0; side < 6; ++side) {
        const int32_t nx = mc_java_i32_add(x, neighbor[side][0]);
        const int32_t ny = mc_java_i32_add(y, neighbor[side][1]);
        const int32_t nz = mc_java_i32_add(z, neighbor[side][2]);
        bool visible = c->fields->f;
        if (!visible && !mc_cv_side(c, block, nx, ny, nz, side, side_pc[side], &visible))
            return false;
        if (!visible)
            continue;
        float light;
        if (!mc_cv_brightness(c, block, nx, ny, nz, light_pc[side], &light) ||
            !cactus_light(c, block, side, center, &light) || !mc_cv_fp(c, multiply_pc[side]))
            return false;
        const float *rgb = shaded[side < 2 ? side : side < 4 ? 2 : 3];
        if (!mc_cv_color(c, at_fmul(rgb[0], light), at_fmul(rgb[1], light), at_fmul(rgb[2], light),
                         color_pc[side]))
            return false;
        if (side >= 2) {
            const float *delta = offset[side - 2][0];
            if (!mc_cv_translation_add(c, delta[0], delta[1], delta[2], offset_pc[side - 2][0]))
                return false;
        }
        int32_t texture;
        if (!mc_cv_texture(c, block, x, y, z, side, texture_pc[side], &texture) ||
            !mc_cv_face(c, side, block, (double)x, (double)y, (double)z, texture, face_pc[side]))
            return false;
        if (side >= 2) {
            const float *delta = offset[side - 2][1];
            if (!mc_cv_translation_add(c, delta[0], delta[1], delta[2], offset_pc[side - 2][1]))
                return false;
        }
        rendered = true;
    }
    mc_cv_pc(c, 812);
    *out = rendered;
    return true;
}

/* Actual cv.m(Block,III). The World dispatcher supplies this source frame. */
bool mc_cv_cactus(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                  bool *out) {
    int32_t color;
    if (!mc_cv_block_color(c, block, x, y, z, 9, &color) || !mc_cv_fp(c, 26))
        return false;
    float red = at_fdiv((float)(((uint32_t)color >> 16u) & 255u), 255);
    float green = at_fdiv((float)(((uint32_t)color >> 8u) & 255u), 255);
    float blue = at_fdiv((float)((uint32_t)color & 255u), 255);
    mc_cv_pc(c, 56);
    if (mc_client_entity_renderer_class_active_use(c->models, NULL) != MC_CLIENT_ENTITY_RENDERER_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                          "Cactus actual PX defining class active use", 56, true);
    if (c->models->px_a) {
        const float r =
            at_fdiv(at_fadd(at_fadd(at_fmul(red, 30), at_fmul(green, 59)), at_fmul(blue, 11)), 100);
        const float g = at_fdiv(at_fadd(at_fmul(red, 30), at_fmul(green, 70)), 100);
        const float b = at_fdiv(at_fadd(at_fmul(red, 30), at_fmul(blue, 70)), 100);
        red = r;
        green = g;
        blue = b;
    }
    mc_cv_pc(c, 140);
    if (!mc_cv_push(c, "c", "(Luu;IIIFFF)Z") ||
        !mc_cv_cactus_geometry(c, block, x, y, z, red, green, blue, out))
        return false;
    mc_cv_pop(c);
    mc_cv_pc(c, 143);
    return true;
}
