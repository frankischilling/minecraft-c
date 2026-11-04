#include "internal.h"

#include "../../../entity/player/tick_math_internal.h"

/* Actual cv.b(Block,int,DDD). The enclosing caller owns the existing NW
 * batch. Each of four vertical planes has both source windings. */
bool mc_cv_crops_geometry(mc_cv_context *c, const mc_block *block, int32_t metadata, double x,
                          double y, double z) {
    if (!mc_cv_push(c, "b", "(Luu;IDDD)V") || !mc_cv_nw(c, 0))
        return false;
    mc_cv_pc(c, 8);
    if (block == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "Crops actual Block metadata texture receiver", 8, false);
    int32_t texture;
    if (mc_block_render_texture_metadata(block, 0, metadata, &texture) != MC_REGISTRY_OK) {
        c->failure.child = (mc_client_graphics_site){"ni", "a", "(II)I", 0};
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                          "Crops actual Block metadata texture", 8, true);
    }
    /* The virtual texture getter is reached even when d replaces its result. */
    if (c->fields->d >= 0)
        texture = c->fields->d;
    if (!mc_cv_fp(c, 48))
        return false;
    const int32_t tx = mc_java_i32_shl(texture & 15, 4), ty = texture & 240;
    const double u[2] = {at_widen(at_fdiv((float)tx, 256)),
                         at_widen(at_fdiv(at_fadd((float)tx, 15.99f), 256))};
    const double v[2] = {at_widen(at_fdiv((float)ty, 256)),
                         at_widen(at_fdiv(at_fadd((float)ty, 15.99f), 256))};
    static const unsigned char across[16] = {0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1};
    for (size_t phase = 0; phase < 2; ++phase) {
        const double normal = phase == 0 ? x : z, edge = phase == 0 ? z : x;
        const double plane[2] = {at_sub(at_add(normal, .5), .25), at_add(at_add(normal, .5), .25)};
        const double span[2] = {at_sub(at_add(edge, .5), .5), at_add(at_add(edge, .5), .5)};
        for (size_t i = 0; i < 16; ++i) {
            const size_t corner = i % 4;
            const double height = at_add(y, corner == 0 || corner == 3 ? 1.0 : 0.0);
            const double a = span[across[i]], n = plane[i / 8];
            if (!mc_cv_vertex(c, phase == 0 ? n : a, height, phase == 0 ? a : n, u[corner >= 2],
                              v[corner == 1 || corner == 2],
                              (unsigned int)((phase == 0 ? 145 : 463) + i * 17)))
                return false;
        }
    }
    mc_cv_pc(c, 721);
    mc_cv_pop(c);
    return true;
}

/* Actual cv.j(Block,III): brightness/color precede metadata and texture. */
bool mc_cv_crops(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                 bool *out) {
    if (!mc_cv_nw(c, 0))
        return false;
    float brightness;
    if (!mc_cv_brightness(c, block, x, y, z, 14, &brightness) ||
        !mc_cv_color(c, brightness, brightness, brightness, 27))
        return false;
    int32_t metadata;
    if (!mc_cv_metadata(c, x, y, z, 40, &metadata) || !mc_cv_fp(c, 48))
        return false;
    /* JVM i2f/fsub precede f2d; large y must not be subtracted as binary64. */
    const double origin_y = at_widen(at_fsub((float)y, .0625f));
    mc_cv_pc(c, 56);
    if (!mc_cv_crops_geometry(c, block, metadata, (double)x, origin_y, (double)z))
        return false;
    mc_cv_pc(c, 60);
    *out = true;
    return true;
}
