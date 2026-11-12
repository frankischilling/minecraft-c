#include "internal.h"

#include "../../../entity/player/tick_math_internal.h"
#include "mc/core/java/java.h"

static bool flat_color(mc_cv_context *c, const float rgb[3], float light, unsigned int multiply_pc,
                       unsigned int color_pc) {
    if (!mc_cv_fp(c, multiply_pc))
        return false;
    const float red = at_fmul(rgb[0], light);
    const float green = at_fmul(rgb[1], light);
    const float blue = at_fmul(rgb[2], light);
    return mc_cv_color(c, red, green, blue, color_pc);
}

static bool overlay_flag(mc_cv_context *c, unsigned int pc, bool *out) {
    bool ready = false;
    mc_cv_pc(c, pc);
    if (!mc_client_render_blocks_static_get(c->models, &ready, out) || !ready)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                          "flat native entered CV class namespace", pc, true);
    return true;
}

static bool substitute_light(mc_cv_context *c, const mc_block *block, int32_t side, float center,
                             float *light) {
    static const unsigned int field_pc[6] = {0, 300, 412, 580, 748, 916};
    static const unsigned int compare_pc[6] = {0, 304, 416, 584, 752, 920};
    if (side == 0)
        return true;
    const mc_block_fields *fields = NULL;
    if (!mc_cv_fields(c, block, field_pc[side], &fields) || !mc_cv_fp(c, compare_pc[side]))
        return false;
    if (side == 1) {
        /* Query above first, then test the CURRENT bounds and Material.d.
         * dcmpl makes NaN != 1, so it reaches that same Material invocation. */
        if (fields->bw != 1.0) {
            mc_cv_pc(c, 309);
            mc_material *material = fields->bA;
            if (material == NULL)
                return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                  "actual flat top null Material receiver", 312, false);
            mc_cv_pc(c, 312);
            if (!mc_material_d(material))
                *light = center;
        }
    } else if ((side == 2 && fields->bu > 0.0) || (side == 3 && fields->bx < 1.0) ||
               (side == 4 && fields->bs > 0.0) || (side == 5 && fields->bv < 1.0)) {
        /* Original dcmpl/dcmpg ordered branches skip substitution for NaN. */
        *light = center;
    }
    return true;
}

bool mc_cv_flat(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z, float red,
                float green, float blue, bool *out) {
    c->fields->m = false;
    mc_cv_store(c, 2);
    if (!mc_cv_nw(c, 5))
        return false;
    /* These three source products precede even Block's grass-static use. */
    if (!mc_cv_fp(c, 32))
        return false;
    const float top[3] = {at_fmul(1.0f, red), at_fmul(1.0f, green), at_fmul(1.0f, blue)};
    float bottom[3] = {.5f, .5f, .5f};
    float north_south[3] = {.8f, .8f, .8f};
    float west_east[3] = {.6f, .6f, .6f};
    const mc_block *grass = NULL;
    if (!mc_cv_grass(c, 86, &grass))
        return false;
    if (block != grass) {
        if (!mc_cv_fp(c, 96))
            return false;
        bottom[0] = at_fmul(bottom[0], red);
        north_south[0] = at_fmul(north_south[0], red);
        west_east[0] = at_fmul(west_east[0], red);
        bottom[1] = at_fmul(bottom[1], green);
        north_south[1] = at_fmul(north_south[1], green);
        west_east[1] = at_fmul(west_east[1], green);
        bottom[2] = at_fmul(bottom[2], blue);
        north_south[2] = at_fmul(north_south[2], blue);
        west_east[2] = at_fmul(west_east[2], blue);
    }
    float center = 0;
    if (!mc_cv_brightness(c, block, x, y, z, 164, &center))
        return false;
    static const int32_t offsets[6][3] = {{0, -1, 0}, {0, 1, 0},  {0, 0, -1},
                                          {0, 0, 1},  {-1, 0, 0}, {1, 0, 0}};
    static const unsigned int side_pc[6] = {188, 277, 389, 557, 725, 893};
    static const unsigned int light_pc[6] = {205, 294, 406, 574, 742, 910};
    static const unsigned int multiply_pc[6] = {216, 328, 430, 598, 766, 934};
    static const unsigned int color_pc[6] = {227, 339, 441, 609, 777, 945};
    static const unsigned int texture_pc[6] = {249, 361, 454, 622, 790, 958};
    static const unsigned int face_pc[6] = {252, 364, 470, 638, 806, 974};
    static const unsigned int flag_pc[4] = {473, 641, 809, 977};
    static const unsigned int override_pc[4] = {486, 654, 822, 990};
    static const unsigned int overlay_multiply_pc[4] = {498, 666, 834, 1002};
    static const unsigned int overlay_color_pc[4] = {518, 686, 854, 1022};
    static const unsigned int overlay_face_pc[4] = {532, 700, 868, 1036};
    bool rendered = false;
    for (int32_t side = 0; side < 6; ++side) {
        const int32_t nx = mc_java_i32_add(x, offsets[side][0]);
        const int32_t ny = mc_java_i32_add(y, offsets[side][1]);
        const int32_t nz = mc_java_i32_add(z, offsets[side][2]);
        bool visible = c->fields->f;
        if (!visible && !mc_cv_side(c, block, nx, ny, nz, side, side_pc[side], &visible))
            return false;
        if (!visible)
            continue;
        float light = 0;
        if (!mc_cv_brightness(c, block, nx, ny, nz, light_pc[side], &light) ||
            !substitute_light(c, block, side, center, &light))
            return false;
        const float *rgb = side == 0   ? bottom
                           : side == 1 ? top
                           : side < 4  ? north_south
                                       : west_east;
        if (!flat_color(c, rgb, light, multiply_pc[side], color_pc[side]))
            return false;
        int32_t texture = 0;
        if (!mc_cv_texture(c, block, x, y, z, side, texture_pc[side], &texture) ||
            !mc_cv_face(c, side, block, (double)x, (double)y, (double)z, texture, face_pc[side]))
            return false;
        if (side >= 2) {
            bool overlay = false;
            if (!overlay_flag(c, flag_pc[side - 2], &overlay))
                return false;
            if (overlay && texture == 3) {
                mc_cv_pc(c, override_pc[side - 2]);
                if (c->fields->d < 0) {
                    if (!mc_cv_fp(c, overlay_multiply_pc[side - 2]))
                        return false;
                    /* Apply source left association, including a SECOND tint
                     * when a nongrass Block happens to return texture3. */
                    const float r = at_fmul(at_fmul(rgb[0], light), red);
                    const float g = at_fmul(at_fmul(rgb[1], light), green);
                    const float b = at_fmul(at_fmul(rgb[2], light), blue);
                    if (!mc_cv_color(c, r, g, b, overlay_color_pc[side - 2]) ||
                        !mc_cv_face(c, side, block, (double)x, (double)y, (double)z, 38,
                                    overlay_face_pc[side - 2]))
                        return false;
                }
            }
        }
        rendered = true; /* Source local store occurs after all reached children. */
    }
    mc_cv_pc(c, 1044);
    *out = rendered;
    return true;
}
