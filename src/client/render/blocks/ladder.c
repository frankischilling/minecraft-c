#include "internal.h"

#include "../../../entity/player/tick_math_internal.h"

/* Actual cv.h(Block,III). Invalid metadata still reads texture/brightness,
 * sets NW color and returns true, while emitting no geometry. */
bool mc_cv_ladder(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                  bool *out) {
    if (!mc_cv_nw(c, 0))
        return false;
    int32_t texture;
    if (!mc_cv_side_texture(c, block, 0, 7, &texture))
        return false;
    if (c->fields->d >= 0)
        texture = c->fields->d;
    float brightness;
    if (!mc_cv_brightness(c, block, x, y, z, 34, &brightness) ||
        !mc_cv_color(c, brightness, brightness, brightness, 47) || !mc_cv_fp(c, 69))
        return false;
    const int32_t tx = mc_java_i32_shl(texture & 15, 4), ty = texture & 240;
    const double u0 = at_widen(at_fdiv((float)tx, 256));
    const double u1 = at_widen(at_fdiv(at_fadd((float)tx, 15.99f), 256));
    const double v0 = at_widen(at_fdiv((float)ty, 256));
    const double v1 = at_widen(at_fdiv(at_fadd((float)ty, 15.99f), 256));
    int32_t metadata;
    if (!mc_cv_metadata(c, x, y, z, 117, &metadata))
        return false;
    if (metadata >= 2 && metadata <= 5) {
        /* Keep every source iadd/i2f and fadd/fsub, including explicit zero.
         * Opposite orientations use different vertex and UV winding. */
        const bool z_plane = metadata <= 3;
        const bool forward = metadata == 2 || metadata == 5;
        const int32_t across = z_plane ? x : z;
        const int32_t normal = z_plane ? z : x;
        const float plane = metadata == 2 || metadata == 4
                                ? at_fsub((float)mc_java_i32_add(normal, 1), .05f)
                                : at_fadd((float)normal, .05f);
        const double u[2] = {u0, u1}, v[2] = {v0, v1};
        static const unsigned int sites[4][4] = {
            {578, 612, 646, 680}, {442, 474, 506, 538}, {302, 336, 370, 404}, {166, 198, 230, 262}};
        const size_t orientation = (size_t)(metadata - 2);
        for (size_t i = 0; i < 4; ++i) {
            const int32_t across_offset = i < 2 ? 1 : 0;
            const bool top = forward ? i == 0 || i == 3 : i == 1 || i == 2;
            const float edge = across_offset != 0 ? at_fadd((float)mc_java_i32_add(across, 1), 0)
                                                  : at_fsub((float)mc_java_i32_add(across, 0), 0);
            const float height = top ? at_fadd((float)mc_java_i32_add(y, 1), 0)
                                     : at_fsub((float)mc_java_i32_add(y, 0), 0);
            const size_t u_index = forward ? (size_t)(i >= 2) : (size_t)(i < 2);
            const size_t v_index =
                forward ? (size_t)(i == 1 || i == 2) : (size_t)(i == 0 || i == 3);
            if (!mc_cv_vertex(c, z_plane ? (double)edge : (double)plane, (double)height,
                              z_plane ? (double)plane : (double)edge, u[u_index], v[v_index],
                              sites[orientation][i]))
                return false;
        }
    }
    mc_cv_pc(c, 684);
    *out = true;
    return true;
}
