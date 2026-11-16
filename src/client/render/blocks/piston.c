#include "internal.h"

#include "../../../entity/player/tick_math_internal.h"
#include "mc/core/java/java.h"

/* Genuine CV Piston bodies. Their enclosing owner, not these children, owns
 * the NW begin/draw. Every shared Block/CV store is retained on failure. */
static bool piston_bounds(mc_cv_context *c, const mc_block *block, const float values[6],
                          unsigned int pc) {
    mc_cv_pc(c, pc);
    if (block == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "Piston actual Block bounds receiver", pc, false);
    if (mc_block_render_bounds((mc_block *)block, values[0], values[1], values[2], values[3],
                               values[4], values[5]) != MC_REGISTRY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                          "Piston actual Block bounds storage", pc, true);
    return true;
}

static void rotation(mc_cv_context *c, size_t field, int32_t value, unsigned int pc) {
    int32_t *const fields[] = {&c->fields->g, &c->fields->h, &c->fields->i,
                               &c->fields->j, &c->fields->k, &c->fields->l};
    *fields[field] = value;
    mc_cv_store(c, pc);
}

static void piston_rotations(mc_cv_context *c, int32_t direction, const unsigned int pcs[6]) {
    const unsigned int pc = direction >= 0 && direction < 6 ? pcs[direction] : 0;
    switch (direction) {
        case 0:
            for (size_t i = 0; i < 4; ++i)
                rotation(c, i, 3, pc + (unsigned int)i * 5u);
            break;
        case 2:
            rotation(c, 2, 1, pc);
            rotation(c, 3, 2, pc + 5);
            break;
        case 3:
            rotation(c, 2, 2, pc);
            rotation(c, 3, 1, pc + 5);
            rotation(c, 4, 3, pc + 10);
            rotation(c, 5, 3, pc + 15);
            break;
        case 4:
            rotation(c, 0, 1, pc);
            rotation(c, 1, 2, pc + 5);
            rotation(c, 4, 2, pc + 10);
            rotation(c, 5, 1, pc + 15);
            break;
        case 5:
            rotation(c, 0, 2, pc);
            rotation(c, 1, 1, pc + 5);
            rotation(c, 4, 1, pc + 10);
            rotation(c, 5, 2, pc + 15);
            break;
        default:
            break; /* Direction1 and invalid6/7 perform no rotation stores. */
    }
}

static void reset_rotations(mc_cv_context *c, unsigned int first_pc) {
    for (size_t i = 0; i < 6; ++i)
        rotation(c, i, 0, first_pc + (unsigned int)i * 5u);
}

static bool piston_standard(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y,
                            int32_t z, unsigned int pc) {
    mc_cv_pc(c, pc);
    if (!mc_cv_push(c, "l", "(Luu;III)Z"))
        return false;
    bool ignored;
    if (!mc_cv_standard(c, block, x, y, z, &ignored))
        return false;
    mc_cv_pop(c);
    return true;
}

static bool piston_direction(mc_cv_context *c, bool head, int32_t metadata, unsigned int pc,
                             int32_t *out) {
    mc_cv_pc(c, pc);
    if (mc_block_render_piston_direction(c->registry, head, metadata, out, &c->failure.registry) ==
        MC_REGISTRY_OK)
        return true;
    mc_item_render_throwable throwable = MC_ITEM_RENDER_NO_THROWABLE;
    mc_item_render_registry_classification(c->registry, &c->failure.registry, &throwable,
                                           &c->exception_class);
    return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, c->failure.registry.message, pc,
                      throwable == MC_ITEM_RENDER_NO_THROWABLE);
}

bool mc_cv_piston_base(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                       bool force_extended, bool *out) {
    if (!mc_cv_push(c, "b", "(Luu;IIIZ)Z"))
        return false;
    int32_t metadata;
    if (!mc_cv_metadata(c, x, y, z, 8, &metadata))
        return false;
    const bool extended = force_extended || ((uint32_t)metadata & 8u) != 0;
    int32_t direction;
    if (!piston_direction(c, false, metadata, 37, &direction))
        return false;
    static const unsigned int extended_rotations[6] = {90, 0, 138, 162, 196, 230};
    static const unsigned int normal_rotations[6] = {354, 0, 380, 393, 416, 439};
    static const float extended_bounds[6][6] = {{0, .25f, 0, 1, 1, 1}, {0, 0, 0, 1, .75f, 1},
                                                {0, 0, .25f, 1, 1, 1}, {0, 0, 0, 1, 1, .75f},
                                                {.25f, 0, 0, 1, 1, 1}, {0, 0, 0, .75f, 1, 1}};
    static const unsigned int bounds_pc[6] = {116, 130, 154, 188, 222, 256};
    piston_rotations(c, direction, extended ? extended_rotations : normal_rotations);
    if (extended && direction < 6 &&
        !piston_bounds(c, block, extended_bounds[direction], bounds_pc[direction]))
        return false;
    if (!piston_standard(c, block, x, y, z, extended ? 265 : 463))
        return false;
    reset_rotations(c, extended ? 271 : 469);
    static const float full[6] = {0, 0, 0, 1, 1, 1};
    if (extended && !piston_bounds(c, block, full, 306))
        return false;
    mc_cv_pc(c, 498);
    *out = true;
    mc_cv_pop(c);
    return true;
}

/* Exact CV.a/b/c(DDDDDDFD)V rod quads. The three Source methods differ
 * only in vertex-coordinate selection, not in UV/color setup or call order. */
static bool piston_rod(mc_cv_context *c, size_t axis, const double p[6], float shade, double span,
                       unsigned int caller_pc) {
    static const char *const method[3] = {"a", "b", "c"};
    mc_cv_pc(c, caller_pc);
    if (!mc_cv_push(c, method[axis], "(DDDDDDFD)V"))
        return false;
    const int32_t texture = c->fields->d >= 0 ? c->fields->d : 108;
    const int32_t tx = mc_java_i32_shl((int32_t)((uint32_t)texture & 15u), 4);
    const int32_t ty = (int32_t)((uint32_t)texture & 240u);
    if (!mc_cv_nw(c, 34) || !mc_cv_fp(c, 43))
        return false;
    const double u0 = at_widen(at_fdiv((float)mc_java_i32_add(tx, 0), 256.0f));
    const double v0 = at_widen(at_fdiv((float)mc_java_i32_add(ty, 0), 256.0f));
    const double u1 = at_div(at_sub(at_add((double)tx, span), .01), 256.0);
    const double v1 = at_div(at_sub(at_widen(at_fadd((float)ty, 4.0f)), .01), 256.0);
    if (!mc_cv_color(c, shade, shade, shade, 102))
        return false;
    static const size_t vertices[3][4][3] = {{{0, 3, 4}, {0, 2, 4}, {1, 2, 5}, {1, 3, 5}},
                                             {{0, 2, 5}, {0, 2, 4}, {1, 3, 4}, {1, 3, 5}},
                                             {{1, 2, 4}, {0, 2, 4}, {0, 3, 5}, {1, 3, 5}}};
    static const unsigned int pc[4] = {116, 130, 144, 158};
    const double uv[4][2] = {{u1, v0}, {u0, v0}, {u0, v1}, {u1, v1}};
    for (size_t i = 0; i < 4; ++i) {
        const size_t *v = vertices[axis][i];
        if (!mc_cv_vertex(c, p[v[0]], p[v[1]], p[v[2]], uv[i][0], uv[i][1], pc[i]))
            return false;
    }
    mc_cv_pc(c, 161);
    mc_cv_pop(c);
    return true;
}

static float coordinate(int32_t value) {
    return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), value));
}

static bool head_rods(mc_cv_context *c, int32_t direction, int32_t x, int32_t y, int32_t z,
                      float light, float length, double span) {
    static const unsigned int call_pc[6][4] = {{194, 246, 298, 350},     {430, 486, 542, 598},
                                               {684, 736, 788, 837},     {937, 993, 1049, 1102},
                                               {1198, 1247, 1299, 1351}, {1451, 1504, 1560, 1616}};
    static const unsigned int first_fp[6][4] = {{147, 199, 251, 303},     {379, 435, 491, 547},
                                                {637, 689, 741, 793},     {886, 942, 998, 1054},
                                                {1151, 1204, 1253, 1305}, {1400, 1457, 1510, 1566}};
    /* Arguments are Source float-coordinate expressions widened only after
     * their additions/subtractions. This is not double-coordinate meshing. */
    static const float cross[3][4][4] = {{{.375f, .625f, .625f, .625f},
                                          {.625f, .375f, .375f, .375f},
                                          {.375f, .375f, .375f, .625f},
                                          {.625f, .625f, .625f, .375f}},
                                         {{.375f, .375f, .625f, .375f},
                                          {.625f, .625f, .375f, .625f},
                                          {.375f, .625f, .375f, .375f},
                                          {.625f, .375f, .625f, .625f}},
                                         {{.375f, .375f, .625f, .375f},
                                          {.625f, .625f, .375f, .625f},
                                          {.375f, .625f, .375f, .375f},
                                          {.625f, .375f, .625f, .625f}}};
    static const float shades[3][4] = {
        {.8f, .8f, .6f, .6f}, {.6f, .6f, .5f, 1}, {.5f, 1, .6f, .6f}};
    const size_t axis = direction < 2 ? 0u : direction < 4 ? 1u : 2u;
    for (size_t quad = 0; quad < 4; ++quad) {
        if (!mc_cv_fp(c, first_fp[direction][quad]))
            return false;
        const float fx = coordinate(x), fy = coordinate(y), fz = coordinate(z);
        const float along = axis == 0 ? fy : axis == 1 ? fz : fx;
        float low, high;
        if ((direction & 1) == 0) {
            low = at_fadd(along, .25f);
            high = at_fadd(at_fadd(along, .25f), length);
        } else {
            high = at_fadd(at_fsub(along, .25f), 1.0f);
            low = at_fsub(at_fadd(at_fsub(along, .25f), 1.0f), length);
        }
        const float *v = cross[axis][quad];
        double args[6];
        if (axis == 0) {
            args[0] = at_widen(at_fadd(fx, v[0]));
            args[1] = at_widen(at_fadd(fx, v[1]));
            args[2] = at_widen(low);
            args[3] = at_widen(high);
            args[4] = at_widen(at_fadd(fz, v[2]));
            args[5] = at_widen(at_fadd(fz, v[3]));
        } else if (axis == 1) {
            args[0] = at_widen(at_fadd(fx, v[0]));
            args[1] = at_widen(at_fadd(fx, v[1]));
            args[2] = at_widen(at_fadd(fy, v[2]));
            args[3] = at_widen(at_fadd(fy, v[3]));
            args[4] = at_widen(low);
            args[5] = at_widen(high);
        } else {
            args[0] = at_widen(low);
            args[1] = at_widen(high);
            args[2] = at_widen(at_fadd(fy, v[0]));
            args[3] = at_widen(at_fadd(fy, v[1]));
            args[4] = at_widen(at_fadd(fz, v[2]));
            args[5] = at_widen(at_fadd(fz, v[3]));
        }
        const float shade = shades[axis][quad] == 1 ? light : at_fmul(light, shades[axis][quad]);
        if (!piston_rod(c, axis, args, shade, span, call_pc[direction][quad]))
            return false;
    }
    return true;
}

bool mc_cv_piston_head(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                       bool full_rod, bool *out) {
    if (!mc_cv_push(c, "c", "(Luu;IIIZ)Z"))
        return false;
    int32_t metadata;
    if (!mc_cv_metadata(c, x, y, z, 8, &metadata))
        return false;
    int32_t direction;
    if (!piston_direction(c, true, metadata, 17, &direction))
        return false;
    float light;
    if (!mc_cv_brightness(c, block, x, y, z, 31, &light))
        return false;
    const float length = full_rod ? 1.0f : .5f;
    const double span = full_rod ? 16.0 : 8.0;
    static const unsigned int rotations[6] = {106, 0, 606, 845, 1110, 1359};
    static const float plate[6][6] = {{0, 0, 0, 1, .25f, 1}, {0, .75f, 0, 1, 1, 1},
                                      {0, 0, 0, 1, 1, .25f}, {0, 0, .75f, 1, 1, 1},
                                      {0, 0, 0, .25f, 1, 1}, {.75f, 0, 0, 1, 1, 1}};
    static const unsigned int bounds_pc[6] = {132, 364, 622, 871, 1136, 1385};
    static const unsigned int standard_pc[6] = {141, 373, 631, 880, 1145, 1394};
    if (direction < 6) {
        piston_rotations(c, direction, rotations);
        if (!piston_bounds(c, block, plate[direction], bounds_pc[direction]) ||
            !piston_standard(c, block, x, y, z, standard_pc[direction]) ||
            !head_rods(c, direction, x, y, z, light, length, span))
            return false;
    }
    reset_rotations(c, 1621);
    static const float full[6] = {0, 0, 0, 1, 1, 1};
    if (!piston_bounds(c, block, full, 1656))
        return false;
    mc_cv_pc(c, 1660);
    *out = true;
    mc_cv_pop(c);
    return true;
}

bool mc_cv_piston_base_all(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y,
                           int32_t z) {
    if (!mc_cv_push(c, "d", "(Luu;III)V"))
        return false;
    c->fields->f = true;
    mc_cv_store(c, 2);
    mc_cv_pc(c, 12);
    bool ignored;
    if (!mc_cv_piston_base(c, block, x, y, z, true, &ignored))
        return false;
    c->fields->f = false;
    mc_cv_store(c, 18);
    mc_cv_pc(c, 21);
    c->returned = true;
    mc_cv_pop(c);
    return true;
}

bool mc_cv_piston_head_all(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                           bool full_rod) {
    if (!mc_cv_push(c, "a", "(Luu;IIIZ)V"))
        return false;
    c->fields->f = true;
    mc_cv_store(c, 2);
    mc_cv_pc(c, 13);
    bool ignored;
    if (!mc_cv_piston_head(c, block, x, y, z, full_rod, &ignored))
        return false;
    c->fields->f = false;
    mc_cv_store(c, 19);
    mc_cv_pc(c, 22);
    c->returned = true;
    mc_cv_pop(c);
    return true;
}
