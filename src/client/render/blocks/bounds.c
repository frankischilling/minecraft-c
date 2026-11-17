#include "internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../../../entity/player/tick_math_internal.h"
#include "../../../world/runtime/tile_entities/piston_math_internal.h"
#include <string.h>

static bool bounds(mc_cv_context *c, mc_block *b, float x0, float y0, float z0, float x1, float y1,
                   float z1, unsigned int pc) {
    if (mc_block_render_bounds(b, x0, y0, z0, x1, y1, z1) != MC_REGISTRY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "actual Block bounds storage",
                          pc, true);
    return true;
}

static bool moving_registry(mc_cv_context *c, mc_registry_status status, unsigned int pc) {
    if (status == MC_REGISTRY_OK)
        return true;
    const bool native =
        c->failure.registry.initializer_pc >= 0 ||
        (status != MC_REGISTRY_NULL_REFERENCE && status != MC_REGISTRY_INDEX_OUT_OF_RANGE);
    if (!native)
        c->exception_class = status == MC_REGISTRY_NULL_REFERENCE
                                 ? "java/lang/NullPointerException"
                                 : "java/lang/ArrayIndexOutOfBoundsException";
    return mc_cv_fail(c,
                      status == MC_REGISTRY_NULL_REFERENCE && !native
                          ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                          : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                      c->failure.registry.message, pc, native);
}

static bool moving_tile(mc_cv_context *c, const void *xp, int32_t x, int32_t y, int32_t z,
                        mc_tile_entity **out) {
    mc_cv_pc(c, 6);
    if (!mc_cv_push_source(c, "ut", "c", "(Lxp;III)Luk;"))
        return false;
    mc_tile_entity *tile = NULL;
    if (!mc_cv_tile_bound(c, xp, x, y, z, 5, &tile))
        return false;
    if (tile == NULL) {
        mc_cv_pc(c, 32);
        mc_cv_pop(c);
        *out = NULL;
        return true;
    }
    /* The Source frame extent also bounds simultaneously live UT locals.
     * Ordinary successful cells release their occurrence immediately. */
    if (c->piston_tile_count == sizeof(c->piston_tiles) / sizeof(c->piston_tiles[0])) {
        mc_tile_entity_release(tile);
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "UT native local occurrence extent", 19,
                          true);
    }
    c->piston_tiles[c->piston_tile_count++] = tile;
    if (c->tile_classes == NULL &&
        mc_tile_entity_classes_borrow(c->code_classes, &c->tile_classes) != MC_TILE_ENTITY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                          "UT defining Tile Class namespace", 19, true);
    const mc_tile_entity_class_literal *actual = NULL, *piston = NULL;
    if (mc_tile_entity_get_class(tile, &actual) != MC_TILE_ENTITY_OK ||
        mc_tile_entity_class_literal_get(c->tile_classes, MC_TILE_ENTITY_CLASS_KIND_PISTON,
                                         &piston) != MC_TILE_ENTITY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                          "UT actual instanceof Class identity", 19, true);
    if (actual != piston) {
        mc_tile_entity_release(c->piston_tiles[--c->piston_tile_count]);
        c->piston_tiles[c->piston_tile_count] = NULL;
        tile = NULL;
    }
    mc_cv_pc(c, tile == NULL ? 32 : 30);
    mc_cv_pop(c);
    *out = tile;
    return true;
}

static bool moving_bounds(mc_cv_context *c, mc_block *block, const void *xp, int32_t x, int32_t y,
                          int32_t z) {
    mc_tile_entity *tile;
    if (!moving_tile(c, xp, x, y, z, &tile))
        return false;
    if (tile == NULL) {
        mc_cv_pc(c, 204);
        return true;
    }
    mc_registry_block_m_view array;
    if (!moving_registry(c, mc_registry_block_m_get(block->classes, &array, &c->failure.registry),
                         16))
        return false;
    int32_t id;
    if (mc_tile_entity_piston_block_id(tile, &id) != MC_TILE_ENTITY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "UT actual UK.a receiver", 21,
                          true);
    const mc_block *carried = NULL;
    if (!moving_registry(c, mc_registry_block_m_at(&array, id, &carried, &c->failure.registry), 24))
        return false;
    if (carried != NULL && carried != block) {
        if (!mc_cv_update_bounds_bound(c, carried, xp, x, y, z, 46))
            return false;
        uint32_t bits;
        mc_cv_pc(c, 52);
        if (mc_tile_entity_piston_progress(tile, 0, &bits, &c->piston_error) != MC_TILE_ENTITY_OK)
            return mc_cv_fail(c,
                              c->piston_error.status == MC_TILE_ENTITY_UNSUPPORTED_FP
                                  ? MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
                                  : MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                              "UT actual UK.a(F)F", 52, true);
        float progress;
        memcpy(&progress, &bits, sizeof(progress));
        bool extending;
        if (mc_tile_entity_piston_extending(tile, &extending) != MC_TILE_ENTITY_OK)
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "UT actual UK.b receiver",
                              59, true);
        if (extending) {
            if (!mc_cv_fp(c, 68))
                return false;
            progress = mc_piston_float_subtract(1.0f, progress);
        }
        int32_t direction;
        if (mc_tile_entity_piston_facing(tile, &direction) != MC_TILE_ENTITY_OK)
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE, "UT actual UK.d receiver",
                              73, true);
        const double *source[] = {&carried->fields.bs, &carried->fields.bt, &carried->fields.bu,
                                  &carried->fields.bv, &carried->fields.bw, &carried->fields.bx};
        double *target[] = {&block->fields.bs, &block->fields.bt, &block->fields.bu,
                            &block->fields.bv, &block->fields.bw, &block->fields.bx};
        for (size_t i = 0; i < 6; ++i) {
            /* Capture each live carried bound BEFORE that independent WJ
             * active use, exactly as the corresponding getfield does. */
            const double coordinate = *source[i];
            const unsigned int get_pc = 84u + (unsigned int)i * 21u;
            mc_registry_direction_view axis;
            if (!moving_registry(
                    c,
                    mc_registry_direction_get(
                        block->classes,
                        (mc_registry_direction_field)(MC_REGISTRY_DIRECTION_B + (int)(i % 3)),
                        &axis, &c->failure.registry),
                    get_pc))
                return false;
            int32_t step;
            if (!moving_registry(
                    c, mc_registry_direction_at(&axis, direction, &step, &c->failure.registry),
                    get_pc + 5) ||
                !mc_cv_fp(c, get_pc + 6))
                return false;
            const float value = _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), step));
            *target[i] = mc_piston_coordinate_subtract(coordinate,
                                                       mc_piston_float_multiply(value, progress));
            mc_cv_store(c, get_pc + 12);
        }
    }
    mc_tile_entity_release(c->piston_tiles[--c->piston_tile_count]);
    c->piston_tiles[c->piston_tile_count] = NULL;
    mc_cv_pc(c, carried == NULL || carried == block ? 38 : 204);
    return true;
}

bool mc_cv_update_bounds(mc_cv_context *c, const mc_block *receiver, int32_t x, int32_t y,
                         int32_t z, unsigned int pc) {
    return mc_cv_update_bounds_bound(c, receiver, c->fields->c, x, y, z, pc);
}

bool mc_cv_update_bounds_bound(mc_cv_context *c, const mc_block *receiver, const void *xp,
                               int32_t x, int32_t y, int32_t z, unsigned int pc) {
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
            if (!mc_cv_metadata_bound(c, xp, x, y, z, 5, &m) ||
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
            if (!mc_cv_block_bound(c, xp, mc_java_i32_sub(x, 1), y, z, 7, &id))
                return false;
            bool along_x = id == b->fields.bn;
            if (!along_x) {
                if (!mc_cv_block_bound(c, xp, mc_java_i32_add(x, 1), y, z, 26, &id))
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
            if (!mc_cv_metadata_bound(c, xp, x, y, z, 5, &m) || !mc_cv_fp(c, 24))
                return false;
            if (!bounds(c, b, 0, 0, 0, 1, at_fdiv((float)(2 * (1 + (m & 7))), 16), 1, 35))
                return false;
            break;
        case MC_BLOCK_PRESSURE_PLATE:
            if (!mc_cv_metadata_bound(c, xp, x, y, z, 5, &m))
                return false;
            if (!bounds(c, b, .0625f, 0, .0625f, .9375f, m == 1 ? .03125f : .0625f, .9375f,
                        m == 1 ? 46 : 68))
                return false;
            break;
        case MC_BLOCK_BUTTON: {
            if (!mc_cv_metadata_bound(c, xp, x, y, z, 5, &m))
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
            if (!mc_cv_metadata_bound(c, xp, x, y, z, 5, &m) || !mc_cv_fp(c, 25))
                return false;
            if (!bounds(c, b, at_fdiv((float)mc_java_i32_add(1, mc_java_i32_mul(m, 2)), 16), 0,
                        .0625f, .9375f, .5f, .9375f, 48))
                return false;
            break;
        case MC_BLOCK_TRAPDOOR:
            if (!mc_cv_metadata_bound(c, xp, x, y, z, 6, &m))
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
        case MC_BLOCK_LEVER:
            if (!mc_cv_metadata_bound(c, xp, x, y, z, 5, &m))
                return false;
            m &= 7;
            if (m == 1) {
                if (!bounds(c, b, 0, .2f, .3125f, .375f, .8f, .6875f, 45))
                    return false;
            } else if (m == 2) {
                if (!bounds(c, b, .625f, .2f, .3125f, 1, .8f, .6875f, 79))
                    return false;
            } else if (m == 3) {
                if (!bounds(c, b, .3125f, .2f, 0, .6875f, .8f, .375f, 111))
                    return false;
            } else if (m == 4) {
                if (!bounds(c, b, .3125f, .2f, .625f, .6875f, .8f, 1, 145))
                    return false;
            } else if (!bounds(c, b, .25f, 0, .25f, .75f, .6f, .75f, 179))
                return false;
            break;
        case MC_BLOCK_PISTON_BASE:
        case MC_BLOCK_PISTON_HEAD: {
            if (!mc_cv_metadata_bound(c, xp, x, y, z, 5, &m))
                return false;
            static const float base[6][6] = {{0, .25f, 0, 1, 1, 1}, {0, 0, 0, 1, .75f, 1},
                                             {0, 0, .25f, 1, 1, 1}, {0, 0, 0, 1, 1, .75f},
                                             {.25f, 0, 0, 1, 1, 1}, {0, 0, 0, .75f, 1, 1}};
            static const float head[6][6] = {{0, 0, 0, 1, .25f, 1}, {0, .75f, 0, 1, 1, 1},
                                             {0, 0, 0, 1, 1, .25f}, {0, 0, .75f, 1, 1, 1},
                                             {0, 0, 0, .25f, 1, 1}, {.75f, 0, 0, 1, 1, 1}};
            if (b->kind == MC_BLOCK_PISTON_BASE && (m & 8) == 0) {
                if (!bounds(c, b, 0, 0, 0, 1, 1, 1, 155))
                    return false;
            } else {
                const int32_t direction = m & 7;
                if (direction < 6) {
                    const float *v =
                        b->kind == MC_BLOCK_PISTON_BASE ? base[direction] : head[direction];
                    if (!bounds(c, b, v[0], v[1], v[2], v[3], v[4], v[5],
                                (b->kind == MC_BLOCK_PISTON_BASE ? 72u : 64u) +
                                    (unsigned int)direction * 14u))
                        return false;
                }
            }
            break;
        }
        case MC_BLOCK_MOVING_PISTON:
            if (!moving_bounds(c, b, xp, x, y, z))
                return false;
            break;
        case MC_BLOCK_DOOR:
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                              "actual specialized Block bound-update child", 0, true);
        case MC_BLOCK_SIGN:
            /* Standing UJ returns without reading XP or replacing the retained
             * bounds. Wall UJ resets first, including unrecognized metadata. */
            mc_cv_pc(c, 1);
            if (b->subtype.uj.b)
                break;
            if (!mc_cv_metadata_bound(c, xp, x, y, z, 13, &m) ||
                !bounds(c, b, 0, 0, 0, 1, 1, 1, 45))
                return false;
            if (m == 2 && !bounds(c, b, 0, .28125f, .875f, 1, .78125f, 1, 68))
                return false;
            if (m == 3 && !bounds(c, b, 0, .28125f, 0, 1, .78125f, .125f, 89))
                return false;
            if (m == 4 && !bounds(c, b, .875f, .28125f, 0, 1, .78125f, 1, 112))
                return false;
            if (m == 5 && !bounds(c, b, 0, .28125f, 0, .125f, .78125f, 1, 133))
                return false;
            break;
        default:
            break;
    }
    mc_cv_pop(c);
    return true;
}
