#include "internal.h"

#include "../../../entity/player/tick_math_internal.h"
#include "../model_graph_internal.h"
#include "mc/client/world/entity_renderer.h"

/* Actual cv.a(Block,int,double,double,double). It appends to its caller's
 * existing NW batch; it neither starts nor finishes that batch. */
bool mc_cv_crossed_plant_geometry(mc_cv_context *c, const mc_block *block, int32_t metadata,
                                  double x, double y, double z) {
    if (!mc_cv_push(c, "a", "(Luu;IDDD)V") || !mc_cv_nw(c, 0))
        return false;
    if (block == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "crossed plant actual Block texture receiver", 8, false);
    int32_t texture;
    const mc_registry_status status =
        mc_block_render_texture_metadata(block, 0, metadata, &texture);
    if (status != MC_REGISTRY_OK) {
        c->failure.child = (mc_client_graphics_site){"uu", "a", "(II)I", 0};
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                          "crossed plant actual Block metadata texture", 8, true);
    }
    if (c->fields->d >= 0)
        texture = c->fields->d;
    if (!mc_cv_fp(c, 48))
        return false;
    const int32_t tx = (texture & 15) << 4, ty = texture & 240;
    const double u0 = at_widen(at_fdiv((float)tx, 256.0f));
    const double u1 = at_widen(at_fdiv(at_fadd((float)tx, 15.99f), 256.0f));
    const double v0 = at_widen(at_fdiv((float)ty, 256.0f));
    const double v1 = at_widen(at_fdiv(at_fadd((float)ty, 15.99f), 256.0f));
    const double x0 = at_sub(at_add(x, 0.5), 0.44999998807907104);
    const double x1 = at_add(at_add(x, 0.5), 0.44999998807907104);
    const double z0 = at_sub(at_add(z, 0.5), 0.44999998807907104);
    const double z1 = at_add(at_add(z, 0.5), 0.44999998807907104);
    const double top = at_add(y, 1.0), bottom = at_add(y, 0.0);
    const double vertices[16][5] = {
        {x0, top, z0, u0, v0},    {x0, bottom, z0, u0, v1}, {x1, bottom, z1, u1, v1},
        {x1, top, z1, u1, v0},    {x1, top, z1, u0, v0},    {x1, bottom, z1, u0, v1},
        {x0, bottom, z0, u1, v1}, {x0, top, z0, u1, v0},    {x0, top, z1, u0, v0},
        {x0, bottom, z1, u0, v1}, {x1, bottom, z0, u1, v1}, {x1, top, z0, u1, v0},
        {x1, top, z0, u0, v0},    {x1, bottom, z0, u0, v1}, {x0, bottom, z1, u1, v1},
        {x0, top, z1, u1, v0}};
    static const unsigned int pc[16] = {145, 162, 179, 196, 213, 230, 247, 264,
                                        281, 298, 315, 332, 349, 366, 383, 400};
    for (size_t i = 0; i < 16; ++i)
        if (!mc_cv_vertex(c, vertices[i][0], vertices[i][1], vertices[i][2], vertices[i][3],
                          vertices[i][4], pc[i]))
            return false;
    mc_cv_pc(c, 403);
    mc_cv_pop(c);
    return true;
}

/* Actual cv.i(Block,int,int,int). The dispatcher owns this method's source
 * frame, just as it does for the standard flat and ambient-occlusion bodies. */
bool mc_cv_crossed_plant(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                         bool *out) {
    if (!mc_cv_nw(c, 0))
        return false;
    float brightness;
    int32_t color;
    if (!mc_cv_brightness(c, block, x, y, z, 14, &brightness) ||
        !mc_cv_block_color(c, block, x, y, z, 28, &color) || !mc_cv_fp(c, 45))
        return false;
    float red = at_fdiv((float)(((uint32_t)color >> 16u) & 255u), 255.0f);
    float green = at_fdiv((float)(((uint32_t)color >> 8u) & 255u), 255.0f);
    float blue = at_fdiv((float)((uint32_t)color & 255u), 255.0f);
    mc_cv_pc(c, 75);
    if (mc_client_entity_renderer_class_active_use(c->models, NULL) != MC_CLIENT_ENTITY_RENDERER_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                          "crossed plant actual PX defining class active use", 75, true);
    if (c->models->px_a) {
        const float r =
            at_fdiv(at_fadd(at_fadd(at_fmul(red, 30), at_fmul(green, 59)), at_fmul(blue, 11)), 100);
        const float g = at_fdiv(at_fadd(at_fmul(red, 30), at_fmul(green, 70)), 100);
        const float b = at_fdiv(at_fadd(at_fmul(red, 30), at_fmul(blue, 70)), 100);
        red = r;
        green = g;
        blue = b;
    }
    if (!mc_cv_color(c, at_fmul(brightness, red), at_fmul(brightness, green),
                     at_fmul(brightness, blue), 164))
        return false;
    double dx = x, dy = y, dz = z;
    const mc_block *tall_grass = NULL;
    mc_cv_pc(c, 181);
    if (mc_registry_block_static_get(c->registry, "Y", &tall_grass, &c->failure.registry) !=
        MC_REGISTRY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, c->failure.registry.message,
                          181, true);
    if (block == tall_grass) {
        int64_t hash = (int64_t)mc_java_i32_mul(x, 3129871) ^
                       mc_java_i64_mul((int64_t)z, 116129781) ^ (int64_t)y;
        hash = mc_java_i64_add(mc_java_i64_mul(mc_java_i64_mul(hash, hash), 42317861),
                               mc_java_i64_mul(hash, 11));
        dx = at_add(
            dx,
            at_mul(at_sub(at_widen(at_fdiv((float)(mc_java_i64_shr(hash, 16) & 15), 15.0f)), 0.5),
                   0.5));
        dy = at_add(
            dy,
            at_mul(at_sub(at_widen(at_fdiv((float)(mc_java_i64_shr(hash, 20) & 15), 15.0f)), 1.0),
                   0.2));
        dz = at_add(
            dz,
            at_mul(at_sub(at_widen(at_fdiv((float)(mc_java_i64_shr(hash, 24) & 15), 15.0f)), 0.5),
                   0.5));
    }
    int32_t metadata;
    if (!mc_cv_metadata(c, x, y, z, 312, &metadata))
        return false;
    mc_cv_pc(c, 323);
    if (!mc_cv_crossed_plant_geometry(c, block, metadata, dx, dy, dz))
        return false;
    mc_cv_pc(c, 327);
    *out = true;
    return true;
}
