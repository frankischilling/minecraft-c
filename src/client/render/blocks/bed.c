#include "internal.h"

#include "../../../block/registry/bed_render_tables_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../../../entity/player/tick_math_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"

static bool registry_result(mc_cv_context *c, mc_registry_status status, unsigned int pc) {
    if (status == MC_REGISTRY_OK)
        return true;
    return mc_cv_fail(
        c,
        status == MC_REGISTRY_NULL_REFERENCE      ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
        : status == MC_REGISTRY_LIMIT             ? MC_CLIENT_RENDER_GLOBAL_LIMIT
        : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
        : status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY
            ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
            : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
        c->failure.registry.message, pc,
        status != MC_REGISTRY_NULL_REFERENCE && status != MC_REGISTRY_INDEX_OUT_OF_RANGE);
}

static bool bed_use(mc_cv_context *c, unsigned int pc) {
    mc_cv_pc(c, pc);
    return registry_result(c, mc_registry_bed_active_use(c->registry, &c->failure.registry), pc);
}

/* Entered genuine VE.a(II)I, called by UU.a(XP,IIII). */
bool mc_cv_bed_texture_metadata(mc_cv_context *c, const mc_block *block, int32_t side,
                                int32_t metadata, int32_t *out) {
    if (side == 0) {
        const mc_block *planks;
        const mc_block_fields *fields;
        mc_cv_pc(c, 4);
        if (!registry_result(
                c, mc_registry_block_static_get(c->registry, "y", &planks, &c->failure.registry),
                4) ||
            !mc_cv_fields(c, planks, 7, &fields))
            return false;
        *out = fields->bm;
        return true;
    }
    if (!bed_use(c, 12))
        return false;
    const int32_t direction = metadata & 3;
    mc_registry_bed_render_matrix matrix;
    mc_registry_bed_render_array row;
    int32_t mapped;
    mc_cv_pc(c, 16);
    if (!registry_result(
            c, mc_registry_bed_render_matrix_get(c->registry, &matrix, &c->failure.registry), 16))
        return false;
    mc_cv_pc(c, 20);
    if (!registry_result(
            c, mc_registry_bed_render_matrix_at(&matrix, direction, &row, &c->failure.registry),
            20))
        return false;
    mc_cv_pc(c, 22);
    if (!registry_result(
            c, mc_registry_bed_render_array_at(&row, side, &mapped, &c->failure.registry), 22) ||
        !bed_use(c, 26))
        return false;
    const bool head = (metadata & 8) != 0;
    const mc_block_fields *fields;
    const unsigned int field_pc = head ? (mapped == 2                  ? 39u
                                          : mapped == 5 || mapped == 4 ? 61u
                                                                       : 71u)
                                       : (mapped == 3                  ? 84u
                                          : mapped == 5 || mapped == 4 ? 106u
                                                                       : 114u);
    if (!mc_cv_fields(c, block, field_pc, &fields))
        return false;
    *out = head ? mc_java_i32_add(fields->bm, mapped == 2                  ? 18
                                              : mapped == 5 || mapped == 4 ? 17
                                                                           : 1)
                : mc_java_i32_add(fields->bm, mapped == 3                  ? 15
                                              : mapped == 5 || mapped == 4 ? 16
                                                                           : 0);
    return true;
}

static bool color(mc_cv_context *c, mc_client_tessellator *nw, float shade, float light,
                  unsigned int multiply_pc, unsigned int call_pc) {
    if (!mc_cv_fp(c, multiply_pc))
        return false;
    c->tessellator = nw;
    return mc_cv_color(c, at_fmul(shade, light), at_fmul(shade, light), at_fmul(shade, light),
                       call_pc);
}

static void uv(int32_t texture, double *u0, double *u1, double *v0, double *v1) {
    const int32_t tx = (texture & 15) << 4, ty = texture & 240;
    *u0 = at_widen(at_fdiv((float)tx, 256));
    *u1 = at_div(at_sub((double)(tx + 16), .01), 256);
    *v0 = at_widen(at_fdiv((float)ty, 256));
    *v1 = at_div(at_sub((double)(ty + 16), .01), 256);
}

/* Actual CV.q(Block,III). The begun NW batch and every mutable CV field remain
 * owned by its caller. Bottom/top are direct quads; only side faces enter the
 * ordinary face children, so override/m/rotation scratch has its source scope. */
bool mc_cv_bed(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
               bool *out) {
    if (!mc_cv_nw(c, 0))
        return false;
    mc_client_tessellator *const nw = c->tessellator;
    int32_t metadata;
    if (!mc_cv_metadata(c, x, y, z, 13, &metadata) || !bed_use(c, 22))
        return false;
    const int32_t direction = metadata & 3;
    if (!bed_use(c, 29))
        return false;
    const bool head = (metadata & 8) != 0;
    float center;
    int32_t texture;
    if (!mc_cv_brightness(c, block, x, y, z, 106, &center) ||
        !color(c, nw, .5f, center, 117, 128) ||
        !mc_cv_texture(c, block, x, y, z, 0, 141, &texture) || !mc_cv_fp(c, 168))
        return false;
    double u0, u1, v0, v1;
    uv(texture, &u0, &u1, &v0, &v1);
    const mc_block_fields *b;
    if (!mc_cv_fields(c, block, 216, &b))
        return false;
    double x0 = at_add((double)x, b->bs), x1 = at_add((double)x, b->bv);
    double yy = at_add(at_add((double)y, b->bt), .1875);
    double z0 = at_add((double)z, b->bu), z1 = at_add((double)z, b->bx);
    if (!mc_cv_vertex(c, x0, yy, z1, u0, v1, 276) || !mc_cv_vertex(c, x0, yy, z0, u0, v0, 291) ||
        !mc_cv_vertex(c, x1, yy, z0, u1, v0, 306) || !mc_cv_vertex(c, x1, yy, z1, u1, v1, 321))
        return false;
    float light;
    if (!mc_cv_brightness(c, block, x, mc_java_i32_add(y, 1), z, 335, &light) ||
        !color(c, nw, 1, light, 346, 357) || !mc_cv_texture(c, block, x, y, z, 1, 370, &texture) ||
        !mc_cv_fp(c, 397))
        return false;
    uv(texture, &u0, &u1, &v0, &v1);
    double ua = u0, ub = u1, va = v0, vb = v0, uc = u0, ud = u1, vc = v1, vd = v1;
    if (direction == 0 || direction == 3) {
        ub = u0;
        va = v1;
        uc = u1;
        vd = v0;
    }
    if (direction == 2 || direction == 3) {
        ua = u1;
        vb = v1;
        ud = u0;
        vc = v0;
    }
    if (!mc_cv_fields(c, block, 564, &b))
        return false;
    x0 = at_add((double)x, b->bs);
    x1 = at_add((double)x, b->bv);
    yy = at_add((double)y, b->bw);
    z0 = at_add((double)z, b->bu);
    z1 = at_add((double)z, b->bx);
    if (!mc_cv_vertex(c, x1, yy, z1, uc, vc, 620) || !mc_cv_vertex(c, x1, yy, z0, ua, va, 635) ||
        !mc_cv_vertex(c, x0, yy, z0, ub, vb, 650) || !mc_cv_vertex(c, x0, yy, z1, ud, vd, 665))
        return false;
    mc_registry_bed_render_array a, opposite;
    int32_t excluded;
    mc_cv_pc(c, 668);
    if (!registry_result(
            c, mc_registry_bed_render_array_get(c->registry, false, &a, &c->failure.registry), 668))
        return false;
    mc_cv_pc(c, 673);
    if (!registry_result(
            c, mc_registry_bed_render_array_at(&a, direction, &excluded, &c->failure.registry),
            673))
        return false;
    if (head) {
        mc_cv_pc(c, 681);
        if (!registry_result(
                c, mc_registry_bed_render_array_get(c->registry, false, &a, &c->failure.registry),
                681))
            return false;
        mc_cv_pc(c, 684);
        if (!registry_result(c,
                             mc_registry_bed_render_array_get(c->registry, true, &opposite,
                                                              &c->failure.registry),
                             684))
            return false;
        int32_t index;
        mc_cv_pc(c, 689);
        if (!registry_result(
                c,
                mc_registry_bed_render_array_at(&opposite, direction, &index, &c->failure.registry),
                689))
            return false;
        mc_cv_pc(c, 690);
        if (!registry_result(
                c, mc_registry_bed_render_array_at(&a, index, &excluded, &c->failure.registry),
                690))
            return false;
    }
    static const int32_t flipped_side[4] = {5, 3, 4, 2};
    static const int32_t neighbors[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    for (int32_t side = 2; side <= 5; ++side) {
        const unsigned int offset = (unsigned int)(side - 2) * 120u;
        if (side == excluded)
            continue;
        const int32_t nx = mc_java_i32_add(x, neighbors[side - 2][0]);
        const int32_t nz = mc_java_i32_add(z, neighbors[side - 2][1]);
        if (!c->fields->f) {
            bool visible;
            if (!mc_cv_side(c, block, nx, y, nz, side, 771 + offset, &visible))
                return false;
            if (!visible)
                continue;
        }
        if (!mc_cv_brightness(c, block, nx, y, nz, 788 + offset, &light) ||
            !mc_cv_fields(c, block, 794 + offset, &b) || !mc_cv_fp(c, 798 + offset))
            return false;
        if ((side == 2 && b->bu > 0) || (side == 3 && b->bx < 1) || (side == 4 && b->bs > 0) ||
            (side == 5 && b->bv < 1))
            light = center;
        if (!color(c, nw, side < 4 ? .8f : .6f, light, 812 + offset, 823 + offset))
            return false;
        c->fields->e = flipped_side[direction] == side;
        mc_cv_store(c, 838 + offset);
        if (!mc_cv_texture(c, block, x, y, z, side, 860 + offset, &texture) ||
            !mc_cv_face(c, side, block, (double)x, (double)y, (double)z, texture, 863 + offset))
            return false;
    }
    c->fields->e = false;
    mc_cv_store(c, 1228);
    mc_cv_pc(c, 1232);
    *out = true;
    return true;
}
