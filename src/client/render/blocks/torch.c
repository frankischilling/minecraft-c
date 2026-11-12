#include "internal.h"

#include "../../../entity/player/tick_math_internal.h"

/* Actual cv.a(Block,DDDDD). The texture virtual runs even with an override.
 * Geometry ignores bounds, AO scratch, rotations and the side-flip flag. */
bool mc_cv_torch_geometry(mc_cv_context *c, const mc_block *block, double x, double y, double z,
                          double tilt_x, double tilt_z) {
    if (!mc_cv_push(c, "a", "(Luu;DDDDD)V") || !mc_cv_nw(c, 0))
        return false;
    int32_t texture;
    if (!mc_cv_side_texture(c, block, 0, 7, &texture))
        return false;
    if (c->fields->d >= 0)
        texture = c->fields->d;
    if (!mc_cv_fp(c, 47))
        return false;
    const int32_t tx = (texture & 15) << 4, ty = texture & 240;
    const double u0 = at_widen(at_fdiv((float)tx, 256.0f));
    const double u1 = at_widen(at_fdiv(at_fadd((float)tx, 15.99f), 256.0f));
    const double v0 = at_widen(at_fdiv((float)ty, 256.0f));
    const double v1 = at_widen(at_fdiv(at_fadd((float)ty, 15.99f), 256.0f));
    const double cap_u0 = at_add(u0, .02734375), cap_v0 = at_add(v0, .0234375);
    const double cap_u1 = at_add(u0, .03515625), cap_v1 = at_add(v0, .03125);
    x = at_add(x, .5);
    z = at_add(z, .5);
    const double x0 = at_sub(x, .5), x1 = at_add(x, .5);
    const double z0 = at_sub(z, .5), z1 = at_add(z, .5);
    const double radius = .0625, cap = .625;
    const double cap_x = at_add(x, at_mul(tilt_x, at_sub(1, cap)));
    const double cap_z = at_add(z, at_mul(tilt_z, at_sub(1, cap)));
    const double cap_y = at_add(y, cap), top = at_add(y, 1), bottom = at_add(y, 0);
    /* Addition order differs on opposite sides. Keep the source grouping,
     * including each explicit y+0 rather than replacing it with y. */
    const double vertices[20][5] = {
        {at_sub(cap_x, radius), cap_y, at_sub(cap_z, radius), cap_u0, cap_v0},
        {at_sub(cap_x, radius), cap_y, at_add(cap_z, radius), cap_u0, cap_v1},
        {at_add(cap_x, radius), cap_y, at_add(cap_z, radius), cap_u1, cap_v1},
        {at_add(cap_x, radius), cap_y, at_sub(cap_z, radius), cap_u1, cap_v0},
        {at_sub(x, radius), top, z0, u0, v0},
        {at_add(at_sub(x, radius), tilt_x), bottom, at_add(z0, tilt_z), u0, v1},
        {at_add(at_sub(x, radius), tilt_x), bottom, at_add(z1, tilt_z), u1, v1},
        {at_sub(x, radius), top, z1, u1, v0},
        {at_add(x, radius), top, z1, u0, v0},
        {at_add(at_add(x, tilt_x), radius), bottom, at_add(z1, tilt_z), u0, v1},
        {at_add(at_add(x, tilt_x), radius), bottom, at_add(z0, tilt_z), u1, v1},
        {at_add(x, radius), top, z0, u1, v0},
        {x0, top, at_add(z, radius), u0, v0},
        {at_add(x0, tilt_x), bottom, at_add(at_add(z, radius), tilt_z), u0, v1},
        {at_add(x1, tilt_x), bottom, at_add(at_add(z, radius), tilt_z), u1, v1},
        {x1, top, at_add(z, radius), u1, v0},
        {x1, top, at_sub(z, radius), u0, v0},
        {at_add(x1, tilt_x), bottom, at_add(at_sub(z, radius), tilt_z), u0, v1},
        {at_add(x0, tilt_x), bottom, at_add(at_sub(z, radius), tilt_z), u1, v1},
        {x0, top, at_sub(z, radius), u1, v0}};
    static const unsigned int vertex_pc[20] = {206, 245, 284, 323, 344, 371, 398, 419, 440, 467,
                                               494, 515, 537, 565, 593, 615, 637, 665, 693, 715};
    for (size_t i = 0; i < 20; ++i)
        if (!mc_cv_vertex(c, vertices[i][0], vertices[i][1], vertices[i][2], vertices[i][3],
                          vertices[i][4], vertex_pc[i]))
            return false;
    mc_cv_pc(c, 718);
    mc_cv_pop(c);
    return true;
}

/* Actual cv.c(Block,III). Metadata precedes NW capture and brightness. The
 * separate emission-array read after brightness must not reuse its capture. */
bool mc_cv_torch(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                 bool *out) {
    int32_t metadata;
    if (!mc_cv_metadata(c, x, y, z, 8, &metadata) || !mc_cv_nw(c, 15))
        return false;
    float brightness;
    if (!mc_cv_brightness(c, block, x, y, z, 29, &brightness))
        return false;
    mc_registry_block_view view;
    const mc_block_fields *fields;
    if (!mc_cv_registry(c, 34, &view) || !mc_cv_fields(c, block, 38, &fields))
        return false;
    if (view.s == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "actual Torch uu.s emission array", 41, false);
    if (fields->bn < 0 || fields->bn >= 256) {
        c->failure.registry =
            (mc_registry_error){.status = MC_REGISTRY_INDEX_OUT_OF_RANGE, .block_id = fields->bn};
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                          "actual Torch uu.s emission index", 41, false);
    }
    if (view.s[fields->bn] > 0)
        brightness = 1;
    if (!mc_cv_color(c, brightness, brightness, brightness, 56) || !mc_cv_fp(c, 69))
        return false;
    const double tilt = 0.4000000059604645, offset = at_sub(.5, tilt);
    const double lift = 0.20000000298023224;
    double dx = x, dy = y, dz = z, tilt_x = 0, tilt_z = 0;
    unsigned int call_pc = 210;
    if (metadata >= 1 && metadata <= 4) {
        dy = at_add(dy, lift);
        if (metadata == 1) {
            dx = at_sub(dx, offset);
            tilt_x = -tilt;
            call_pc = 102;
        } else if (metadata == 2) {
            dx = at_add(dx, offset);
            tilt_x = tilt;
            call_pc = 132;
        } else if (metadata == 3) {
            dz = at_sub(dz, offset);
            tilt_z = -tilt;
            call_pc = 163;
        } else {
            dz = at_add(dz, offset);
            tilt_z = tilt;
            call_pc = 193;
        }
    }
    mc_cv_pc(c, call_pc);
    if (!mc_cv_torch_geometry(c, block, dx, dy, dz, tilt_x, tilt_z))
        return false;
    mc_cv_pc(c, 214);
    *out = true;
    return true;
}
