#include "internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../entity/player/tick_math_internal.h"

static bool bounds(mc_cv_context *c, mc_block *b, float x0, float y0, float z0, float x1, float y1,
                   float z1, unsigned int pc) {
    if (mc_block_render_bounds(b, x0, y0, z0, x1, y1, z1) != MC_REGISTRY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "actual Block bounds storage",
                          pc, true);
    return true;
}

bool mc_cv_update_bounds(mc_cv_context *c, const mc_block *receiver, int32_t x, int32_t y,
                         int32_t z, unsigned int pc) {
    mc_cv_pc(c, pc);
    if (receiver == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "actual Block bound-update receiver", pc, false);
    mc_block *b = (mc_block *)receiver;
    const char *name = b->kind == MC_BLOCK_STAIRS                                      ? "ss"
                       : b->kind == MC_BLOCK_PORTAL                                    ? "ak"
                       : b->kind == MC_BLOCK_SNOW_LAYER                                ? "jr"
                       : b->kind == MC_BLOCK_PRESSURE_PLATE                            ? "bv"
                       : b->kind == MC_BLOCK_BUTTON                                    ? "oi"
                       : b->kind == MC_BLOCK_CAKE                                      ? "um"
                       : b->kind == MC_BLOCK_TRAPDOOR                                  ? "oq"
                       : b->kind == MC_BLOCK_RAIL || b->kind == MC_BLOCK_DETECTOR_RAIL ? "pc"
                       : b->kind == MC_BLOCK_BED                                       ? "ve"
                       : b->kind == MC_BLOCK_DOOR                                      ? "le"
                       : b->kind == MC_BLOCK_LEVER                                     ? "xr"
                       : b->kind == MC_BLOCK_SIGN                                      ? "uj"
                       : b->kind == MC_BLOCK_MOVING_PISTON                             ? "ut"
                       : b->kind == MC_BLOCK_PISTON_BASE                               ? "jq"
                       : b->kind == MC_BLOCK_PISTON_HEAD                               ? "h"
                                                                                       : "uu";
    if (!mc_cv_push_source(c, name, "a", "(Lxp;III)V"))
        return false;
    int32_t m;
    switch (b->kind) {
        case MC_BLOCK_STAIRS:
            if (!bounds(c, b, 0, 0, 0, 1, 1, 1, 7))
                return false;
            break;
        case MC_BLOCK_RAIL:
        case MC_BLOCK_DETECTOR_RAIL:
            if (!mc_cv_metadata(c, x, y, z, 5, &m) ||
                !bounds(c, b, 0, 0, 0, 1, m >= 2 && m <= 5 ? .625f : .125f, 1,
                        m >= 2 && m <= 5 ? 32 : 46))
                return false;
            break;
        case MC_BLOCK_BED:
            mc_cv_pc(c, 1);
            if (!mc_cv_push_source(c, "ve", "r", "()V") || !bounds(c, b, 0, 0, 0, 1, .5625f, 1, 8))
                return false;
            mc_cv_pop(c);
            break;
        case MC_BLOCK_PORTAL: {
            int32_t id;
            if (!mc_cv_block(c, mc_java_i32_sub(x, 1), y, z, 7, &id))
                return false;
            bool along_x = id == b->fields.bn;
            if (!along_x) {
                if (!mc_cv_block(c, mc_java_i32_add(x, 1), y, z, 26, &id))
                    return false;
                along_x = id == b->fields.bn;
            }
            if (along_x) {
                if (!bounds(c, b, 0, 0, .375f, 1, 1, .625f, 69))
                    return false;
            } else if (!bounds(c, b, .375f, 0, 0, .625f, 1, 1, 106))
                return false;
            break;
        }
        case MC_BLOCK_SNOW_LAYER:
            if (!mc_cv_metadata(c, x, y, z, 5, &m) || !mc_cv_fp(c, 24))
                return false;
            if (!bounds(c, b, 0, 0, 0, 1, at_fdiv((float)(2 * (1 + (m & 7))), 16), 1, 35))
                return false;
            break;
        case MC_BLOCK_PRESSURE_PLATE:
            if (!mc_cv_metadata(c, x, y, z, 5, &m))
                return false;
            if (!bounds(c, b, .0625f, 0, .0625f, .9375f, m == 1 ? .03125f : .0625f, .9375f,
                        m == 1 ? 46 : 68))
                return false;
            break;
        case MC_BLOCK_BUTTON: {
            if (!mc_cv_metadata(c, x, y, z, 5, &m))
                return false;
            const float thickness = (m & 8) != 0 ? .0625f : .125f;
            switch (m & 7) {
                case 1:
                    if (!bounds(c, b, 0, .375f, .3125f, thickness, .625f, .6875f, 83))
                        return false;
                    break;
                case 2:
                    if (!bounds(c, b, 1 - thickness, .375f, .3125f, 1, .625f, .6875f, 115))
                        return false;
                    break;
                case 3:
                    if (!bounds(c, b, .3125f, .375f, 0, .6875f, .625f, thickness, 145))
                        return false;
                    break;
                case 4:
                    if (!bounds(c, b, .3125f, .375f, 1 - thickness, .6875f, .625f, 1, 177))
                        return false;
                    break;
                default:
                    break;
            }
            break;
        }
        case MC_BLOCK_CAKE:
            if (!mc_cv_metadata(c, x, y, z, 5, &m) || !mc_cv_fp(c, 25))
                return false;
            if (!bounds(c, b, at_fdiv((float)mc_java_i32_add(1, mc_java_i32_mul(m, 2)), 16), 0,
                        .0625f, .9375f, .5f, .9375f, 48))
                return false;
            break;
        case MC_BLOCK_TRAPDOOR:
            if (!mc_cv_metadata(c, x, y, z, 6, &m))
                return false;
            mc_cv_pc(c, 11);
            if (!mc_cv_push_source(c, "oq", "d", "(I)V") ||
                !bounds(c, b, 0, 0, 0, 1, .1875f, 1, 10))
                return false;
            if ((m & 4) != 0) {
                switch (m & 3) {
                    case 0:
                        if (!bounds(c, b, 0, 0, .8125f, 1, 1, 1, 35))
                            return false;
                        break;
                    case 1:
                        if (!bounds(c, b, 0, 0, 0, 1, 1, .1875f, 52))
                            return false;
                        break;
                    case 2:
                        if (!bounds(c, b, .8125f, 0, 0, 1, 1, 1, 71))
                            return false;
                        break;
                    case 3:
                        if (!bounds(c, b, 0, 0, 0, .1875f, 1, 1, 88))
                            return false;
                        break;
                }
            }
            mc_cv_pop(c);
            break;
        case MC_BLOCK_DOOR:
        case MC_BLOCK_LEVER:
        case MC_BLOCK_SIGN:
        case MC_BLOCK_MOVING_PISTON:
        case MC_BLOCK_PISTON_BASE:
        case MC_BLOCK_PISTON_HEAD:
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                              "actual specialized Block bound-update child", 0, true);
        default:
            break;
    }
    mc_cv_pop(c);
    return true;
}
