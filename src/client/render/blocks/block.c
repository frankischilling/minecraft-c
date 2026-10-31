#include "internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../entity/player/tick_math_internal.h"
#include "mc/block/registry/runtime.h"

static bool enter_block(mc_cv_context *c, const mc_block *b, const char *class_name,
                        const char *method, const char *descriptor, unsigned int pc) {
    mc_cv_pc(c, pc);
    if (b == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "actual Block virtual receiver", pc, false);
    return mc_cv_push_source(c, class_name, method, descriptor);
}

bool mc_cv_brightness(mc_cv_context *c, const mc_block *b, int32_t x, int32_t y, int32_t z,
                      unsigned int pc, float *out) {
    const char *name =
        b != NULL && b->kind == MC_BLOCK_STAIRS ? "ss"
        : b != NULL && (b->kind == MC_BLOCK_FLOWING_FLUID || b->kind == MC_BLOCK_STILL_FLUID)
            ? "rp"
            : "uu";
    if (!enter_block(c, b, name, "d", "(Lxp;III)F", pc))
        return false;
    float value;
    if (b->kind == MC_BLOCK_STAIRS) {
        if (!mc_cv_brightness(c, b->subtype.ss.a, x, y, z, 9, &value))
            return false;
    } else if (b->kind == MC_BLOCK_FLOWING_FLUID || b->kind == MC_BLOCK_STILL_FLUID) {
        float below, above;
        if (!mc_cv_cache_brightness(c, x, y, z, 5, &below) ||
            !mc_cv_cache_brightness(c, x, mc_java_i32_add(y, 1), z, 19, &above) || !mc_cv_fp(c, 30))
            return false;
        value = below > above ? below : above;
    } else {
        mc_registry_block_view view;
        if (!mc_cv_registry(c, 5, &view))
            return false;
        const int32_t *s = view.s;
        if (s == NULL)
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                              "actual uu.s brightness array", 12, false);
        const int32_t id = b->fields.bn;
        if (id < 0 || id >= 256) {
            c->failure.registry =
                (mc_registry_error){.status = MC_REGISTRY_INDEX_OUT_OF_RANGE, .block_id = id};
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                              "actual uu.s brightness index", 12, false);
        }
        if (!mc_cv_min_brightness(c, x, y, z, s[id], 13, &value))
            return false;
    }
    mc_cv_pop(c);
    *out = value;
    return true;
}

static bool base_side(mc_cv_context *c, const mc_block *b, int32_t x, int32_t y, int32_t z,
                      int32_t side, unsigned int pc, bool *out) {
    static const unsigned int comparison_pc[6] = {10, 28, 44, 61, 78, 95};
    if (!enter_block(c, b, "uu", "b", "(Lxp;IIII)Z", pc))
        return false;
    if (side >= 0 && side < 6 && !mc_cv_fp(c, comparison_pc[side]))
        return false;
    const mc_block_fields *f = &b->fields;
    bool value = true;
    if (!((side == 0 && f->bt > 0.0) || (side == 1 && f->bw < 1.0) || (side == 2 && f->bu > 0.0) ||
          (side == 3 && f->bx < 1.0) || (side == 4 && f->bs > 0.0) || (side == 5 && f->bv < 1.0))) {
        if (!mc_cv_opaque(c, x, y, z, 106, &value))
            return false;
        value = !value;
    }
    mc_cv_pop(c);
    *out = value;
    return true;
}

static bool portal_edge(mc_cv_context *c, const mc_block *b, int32_t x, int32_t y, int32_t z,
                        int32_t dx, int32_t dz, unsigned int first_pc, unsigned int second_pc,
                        bool *out) {
    int32_t id;
    if (!mc_cv_block(c, mc_java_i32_add(x, dx), y, mc_java_i32_add(z, dz), first_pc, &id))
        return false;
    if (id != b->fields.bn) {
        *out = false;
        return true;
    }
    if (!mc_cv_block(c, mc_java_i32_add(x, mc_java_i32_mul(dx, 2)), y,
                     mc_java_i32_add(z, mc_java_i32_mul(dz, 2)), second_pc, &id))
        return false;
    *out = id != b->fields.bn;
    return true;
}

bool mc_cv_side(mc_cv_context *c, const mc_block *b, int32_t x, int32_t y, int32_t z, int32_t side,
                unsigned int pc, bool *out) {
    if (b == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "actual Block side virtual receiver", pc, false);
    const char *name = b->kind == MC_BLOCK_LEAVES       ? "nr"
                       : b->kind == MC_BLOCK_GLASS      ? "jp"
                       : b->kind == MC_BLOCK_ICE        ? "nk"
                       : b->kind == MC_BLOCK_PORTAL     ? "ak"
                       : b->kind == MC_BLOCK_SLAB       ? "ys"
                       : b->kind == MC_BLOCK_SNOW_LAYER ? "jr"
                       : b->kind == MC_BLOCK_REPEATER   ? "wo"
                       : b->kind == MC_BLOCK_STAIRS     ? "ss"
                       : b->kind == MC_BLOCK_FLOWING_FLUID || b->kind == MC_BLOCK_STILL_FLUID
                           ? "rp"
                           : "uu";
    if (name[0] == 'u')
        return base_side(c, b, x, y, z, side, pc, out);
    if (!enter_block(c, b, name, "b", "(Lxp;IIII)Z", pc))
        return false;
    bool value;
    if (b->kind == MC_BLOCK_PORTAL) {
        int32_t id;
        if (!mc_cv_block(c, x, y, z, 5, &id))
            return false;
        value = false;
        if (id != b->fields.bn) {
            bool west, east, north, south;
            if (!portal_edge(c, b, x, y, z, -1, 0, 26, 45, &west) ||
                !portal_edge(c, b, x, y, z, 1, 0, 71, 90, &east) ||
                !portal_edge(c, b, x, y, z, 0, -1, 116, 135, &north) ||
                !portal_edge(c, b, x, y, z, 0, 1, 161, 180, &south))
                return false;
            value = ((west || east) && (side == 4 || side == 5)) ||
                    ((north || south) && (side == 2 || side == 3));
        }
    } else if (b->kind == MC_BLOCK_SLAB) {
        const mc_block *half = NULL;
        if (mc_registry_block_static_get(c->registry, "al", &half, &c->failure.registry) !=
            MC_REGISTRY_OK)
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                              c->failure.registry.message, 1, true);
        /* The first base call's boolean is discarded, but its reads/effects
         * still happen for a receiver distinct from the real static al. */
        if (b != half && !base_side(c, b, x, y, z, side, 15, &value))
            return false;
        if (side == 1)
            value = true;
        else {
            if (!base_side(c, b, x, y, z, side, 35, &value))
                return false;
            if (value && side != 0) {
                int32_t id;
                if (!mc_cv_block(c, x, y, z, 55, &id))
                    return false;
                value = id != b->fields.bn;
            }
        }
    } else if (b->kind == MC_BLOCK_REPEATER) {
        value = side != 0 && side != 1;
    } else if (b->kind == MC_BLOCK_SNOW_LAYER && side == 1) {
        value = true;
    } else if (b->kind == MC_BLOCK_FLOWING_FLUID || b->kind == MC_BLOCK_STILL_FLUID) {
        mc_material *material;
        if (!mc_cv_material(c, x, y, z, 5, &material))
            return false;
        if (material == b->fields.bA ||
            material == mc_materials_get(b->classes->options.materials, MC_MATERIAL_s))
            value = false;
        else if (side == 1)
            value = true;
        else if (!base_side(c, b, x, y, z, side, 49, &value))
            return false;
    } else {
        unsigned int base_pc = b->kind == MC_BLOCK_STAIRS ? 8 : 16;
        if (b->kind == MC_BLOCK_LEAVES || b->kind == MC_BLOCK_GLASS || b->kind == MC_BLOCK_ICE) {
            if (b->kind == MC_BLOCK_ICE) {
                mc_cv_pc(c, 10);
                if (!mc_cv_push_source(c, "jp", "b", "(Lxp;IIII)Z"))
                    return false;
                side = mc_java_i32_sub(1, side);
            }
            /* NR/JP always execute xp.a, including fancy/glass-all profiles. */
            int32_t id;
            if (!mc_cv_block(c, x, y, z, 5, &id))
                return false;
            const bool all = b->kind == MC_BLOCK_LEAVES ? b->nr_b : b->jp_a;
            if (!all && id == b->fields.bn) {
                value = false;
                if (b->kind == MC_BLOCK_ICE)
                    mc_cv_pop(c);
                goto returned;
            }
            base_pc = 38;
        }
        if (!base_side(c, b, x, y, z, side, base_pc, &value))
            return false;
        if (b->kind == MC_BLOCK_ICE)
            mc_cv_pop(c);
    }
returned:
    mc_cv_pop(c);
    *out = value;
    return true;
}

static bool registry_result(mc_cv_context *c, mc_registry_status status, unsigned int pc) {
    if (status == MC_REGISTRY_OK)
        return true;
    return mc_cv_fail(c,
                      status == MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY
                          ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                          : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                      "actual Block texture virtual child", pc, true);
}

bool mc_cv_texture(mc_cv_context *c, const mc_block *b, int32_t x, int32_t y, int32_t z,
                   int32_t side, unsigned int pc, int32_t *out) {
    const char *name = b != NULL && b->kind == MC_BLOCK_GRASS          ? "wp"
                       : b != NULL && b->kind == MC_BLOCK_FURNACE      ? "tc"
                       : b != NULL && b->kind == MC_BLOCK_STAIRS       ? "ss"
                       : b != NULL && b->kind == MC_BLOCK_DISPENSER    ? "xq"
                       : b != NULL && b->kind == MC_BLOCK_CHEST        ? "e"
                       : b != NULL && b->kind == MC_BLOCK_LOCKED_CHEST ? "ks"
                                                                       : "uu";
    if (!enter_block(c, b, name, "a", "(Lxp;IIII)I", pc))
        return false;
    int32_t value;
    if (b->kind == MC_BLOCK_STAIRS) {
        if (!mc_cv_texture(c, b->subtype.ss.a, x, y, z, side, 11, &value))
            return false;
    } else if (b->kind == MC_BLOCK_GRASS) {
        if (side == 1)
            value = 0;
        else if (side == 0)
            value = 2;
        else {
            mc_material *above;
            if (!mc_cv_material(c, x, mc_java_i32_add(y, 1), z, 22, &above))
                return false;
            mc_materials *materials = b->classes->options.materials;
            value = above == mc_materials_get(materials, MC_MATERIAL_t) ||
                            above == mc_materials_get(materials, MC_MATERIAL_u)
                        ? 68
                        : 3;
        }
    } else if (b->kind == MC_BLOCK_FURNACE || b->kind == MC_BLOCK_DISPENSER) {
        if (side == 0 || side == 1)
            value = mc_java_i32_add(b->fields.bm, 17);
        else {
            int32_t metadata;
            if (!mc_cv_metadata(c, x, y, z, 32, &metadata))
                return false;
            value = side != metadata
                        ? b->fields.bm
                        : mc_java_i32_add(b->fields.bm, b->kind == MC_BLOCK_DISPENSER ? 1
                                                        : b->subtype.tc.b             ? 16
                                                                                      : -1);
        }
    } else if (b->kind == MC_BLOCK_CHEST || b->kind == MC_BLOCK_LOCKED_CHEST) {
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                          "actual Chest texture adjacency child", 0, true);
    } else {
        int32_t metadata;
        if (!mc_cv_metadata(c, x, y, z, 7, &metadata) ||
            !registry_result(c, mc_block_render_texture_metadata(b, side, metadata, &value), 12))
            return false;
    }
    mc_cv_pop(c);
    *out = value;
    return true;
}
