#include "internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../entity/player/tick_math_internal.h"
#include "../../application_state.h"
#include "../model_graph_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/entity/vector/vector.h"

static bool fluid_material(mc_cv_context *c, mc_material_id id, unsigned int pc,
                           mc_material **out) {
    mc_cv_pc(c, pc);
    const mc_registry_status status =
        mc_registry_material_get(c->registry, id, out, &c->failure.registry);
    return status == MC_REGISTRY_OK ||
           mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                      "fluid actual Material defining-class active use", pc, true);
}

static bool fluid_texture(mc_cv_context *c, const mc_block *b, int32_t side, int32_t metadata,
                          bool metadata_argument, unsigned int pc, int32_t *out) {
    mc_cv_pc(c, pc);
    if (b == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "fluid actual texture Block receiver", pc, false);
    if (metadata_argument) {
        if (!mc_cv_push_source(c, "uu", "a", "(II)I"))
            return false;
        mc_cv_pc(c, 2);
    }
    const bool rp = b->kind == MC_BLOCK_FLOWING_FLUID || b->kind == MC_BLOCK_STILL_FLUID;
    if (!mc_cv_push_source(c, rp ? "rp" : "uu", "a", "(I)I"))
        return false;
    const mc_registry_status status = mc_block_render_texture_metadata(b, side, metadata, out);
    if (status != MC_REGISTRY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                          "fluid actual texture virtual body", 0, true);
    mc_cv_pop(c);
    if (metadata_argument)
        mc_cv_pop(c);
    return true;
}

/* Actual cv.a(III,Material)F. The four samples are ordered and each repeated
 * Material/metadata read dispatches through this SAME XP receiver. */
static bool fluid_height(mc_cv_context *c, int32_t x, int32_t y, int32_t z, mc_material *material,
                         unsigned int caller_pc, float *out) {
    mc_cv_pc(c, caller_pc);
    if (!mc_cv_push(c, "a", "(IIILln;)F"))
        return false;
    int32_t count = 0;
    float sum = 0;
    for (int32_t i = 0; i < 4; ++i) {
        const int32_t nx = mc_java_i32_sub(x, i & 1);
        const int32_t nz = mc_java_i32_sub(z, (i >> 1) & 1);
        mc_material *m;
        if (!mc_cv_material(c, nx, mc_java_i32_add(y, 1), nz, 48, &m))
            return false;
        if (m == material) {
            *out = 1;
            mc_cv_pc(c, 59);
            mc_cv_pop(c);
            return true;
        }
        if (!mc_cv_material(c, nx, y, nz, 70, &m))
            return false;
        if (m == material) {
            int32_t metadata;
            if (!mc_cv_metadata(c, nx, y, nz, 94, &metadata))
                return false;
            if (metadata >= 8 || metadata == 0) {
                mc_cv_pc(c, 117);
                if (!mc_cv_push_source(c, "rp", "d", "(I)F") || !mc_cv_fp(c, 14))
                    return false;
                const float value =
                    at_fdiv((float)mc_java_i32_add(metadata >= 8 ? 0 : metadata, 1), 9);
                mc_cv_pop(c);
                sum = at_fadd(sum, at_fmul(value, 10));
                count = mc_java_i32_add(count, 10);
            }
            mc_cv_pc(c, 133);
            if (!mc_cv_push_source(c, "rp", "d", "(I)F") || !mc_cv_fp(c, 14))
                return false;
            const float value = at_fdiv((float)mc_java_i32_add(metadata >= 8 ? 0 : metadata, 1), 9);
            mc_cv_pop(c);
            sum = at_fadd(sum, value);
            count = mc_java_i32_add(count, 1);
        } else {
            mc_cv_pc(c, 147);
            if (m == NULL)
                return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                  "fluid height actual Material.a receiver", 147, false);
            if (!mc_material_a(m)) {
                if (!mc_cv_fp(c, 156))
                    return false;
                sum = at_fadd(sum, 1);
                count = mc_java_i32_add(count, 1);
            }
        }
    }
    if (!mc_cv_fp(c, 174))
        return false;
    *out = at_fsub(1, at_fdiv(sum, (float)count));
    mc_cv_pc(c, 176);
    mc_cv_pop(c);
    return true;
}

static bool fluid_decay(mc_cv_context *c, const mc_block *b, int32_t x, int32_t y, int32_t z,
                        unsigned int caller_pc, int32_t *out) {
    mc_cv_pc(c, caller_pc);
    if (!mc_cv_push_source(c, "rp", "c", "(Lxp;III)I"))
        return false;
    mc_material *m;
    if (!mc_cv_material(c, x, y, z, 5, &m))
        return false;
    if (m != b->fields.bA)
        *out = -1;
    else {
        if (!mc_cv_metadata(c, x, y, z, 24, out))
            return false;
        if (*out >= 8)
            *out = 0;
    }
    mc_cv_pop(c);
    return true;
}

static bool fluid_solid(mc_cv_context *c, const mc_block *b, int32_t x, int32_t y, int32_t z,
                        int32_t side, unsigned int caller_pc, bool *out) {
    mc_cv_pc(c, caller_pc);
    if (!mc_cv_push_source(c, "rp", "d", "(Lxp;IIII)Z"))
        return false;
    mc_material *m, *ice;
    if (!mc_cv_material(c, x, y, z, 5, &m))
        return false;
    if (m == b->fields.bA)
        *out = false;
    else {
        if (!fluid_material(c, MC_MATERIAL_s, 25, &ice))
            return false;
        if (m == ice)
            *out = false;
        else if (side == 1)
            *out = true;
        else {
            mc_cv_pc(c, 49);
            if (!mc_cv_push_source(c, "uu", "d", "(Lxp;IIII)Z") ||
                !mc_cv_material(c, x, y, z, 5, &m))
                return false;
            if (m == NULL)
                return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                  "fluid actual super Material.a receiver", 10, false);
            *out = mc_material_a(m);
            mc_cv_pop(c);
        }
    }
    mc_cv_pop(c);
    return true;
}

static bool fluid_vector_result(mc_cv_context *c, mc_entity_status status, unsigned int pc,
                                const char *method, const char *descriptor) {
    if (status == MC_ENTITY_OK)
        return true;
    const mc_entity_vector_error *error = &c->fluid_vector_error;
    c->failure.child = (mc_client_graphics_site){"bt", method, descriptor, error->bytecode};
    return mc_cv_fail(c,
                      status == MC_ENTITY_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                      : status == MC_ENTITY_ALLOCATION_FAILED
                          ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                      : status == MC_ENTITY_BUSY ? MC_CLIENT_RENDER_GLOBAL_BUSY
                                                 : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                      error->operation, pc, true);
}

static bool fluid_vector_replace(mc_cv_context *c, mc_entity_vector **value, bool normalize,
                                 double x, double y, double z, unsigned int pc) {
    mc_entity_classes *classes = c->models->entities;
    mc_entity_vector *next = NULL;
    mc_cv_pc(c, pc);
    const mc_entity_status status =
        normalize ? mc_entity_vector_normalize(classes, *value, &next, &c->fluid_vector_error)
                  : mc_entity_vector_add(classes, *value, x, y, z, &next, &c->fluid_vector_error);
    if (!fluid_vector_result(c, status, pc, "c", normalize ? "()Lbt;" : "(DDD)Lbt;"))
        return false;
    mc_entity_vector_release(classes, *value);
    *value = next;
    return true;
}

static bool fluid_vector(mc_cv_context *c, const mc_block *b, int32_t x, int32_t y, int32_t z,
                         unsigned int caller_pc, mc_entity_vector **out) {
    mc_cv_pc(c, caller_pc);
    if (b == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "fluid actual flow receiver",
                          caller_pc, false);
    if (b->kind != MC_BLOCK_FLOWING_FLUID && b->kind != MC_BLOCK_STILL_FLUID)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                          "fluid actual static RP receiver cast", caller_pc, false);
    if (!mc_cv_push_source(c, "rp", "e", "(Lxp;III)Lbt;"))
        return false;
    if (c->models == NULL || c->models->entities == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                          "fluid actual Entity class namespace", 3, true);
    mc_entity_classes *classes = c->models->entities;
    mc_entity_vector *value = NULL;
    mc_cv_pc(c, 3);
    if (!fluid_vector_result(
            c, mc_entity_vector_pool_get(classes, 0, 0, 0, &value, &c->fluid_vector_error), 3, "b",
            "(DDD)Lbt;"))
        return false;
    int32_t level;
    if (!fluid_decay(c, b, x, y, z, 14, &level))
        goto failed;
    for (int32_t d = 0; d < 4; ++d) {
        const int32_t nx = mc_java_i32_add(x, d == 0 ? -1 : d == 2 ? 1 : 0);
        const int32_t nz = mc_java_i32_add(z, d == 1 ? -1 : d == 3 ? 1 : 0);
        int32_t neighbor, delta;
        if (!fluid_decay(c, b, nx, y, nz, 81, &neighbor))
            goto failed;
        unsigned int add_pc = 213;
        if (neighbor < 0) {
            mc_material *m;
            if (!mc_cv_material(c, nx, y, nz, 98, &m))
                goto failed;
            if (m == NULL) {
                mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                           "fluid actual flow Material.c receiver", 103, false);
                goto failed;
            }
            if (mc_material_c(m))
                continue;
            if (!fluid_decay(c, b, nx, mc_java_i32_sub(y, 1), nz, 119, &neighbor))
                goto failed;
            if (neighbor < 0)
                continue;
            delta = mc_java_i32_sub(neighbor, mc_java_i32_sub(level, 8));
            add_pc = 166;
        } else
            delta = mc_java_i32_sub(neighbor, level);
        if (!fluid_vector_replace(c, &value, false,
                                  (double)mc_java_i32_mul(mc_java_i32_sub(nx, x), delta), 0,
                                  (double)mc_java_i32_mul(mc_java_i32_sub(nz, z), delta), add_pc))
            goto failed;
    }
    int32_t metadata;
    if (!mc_cv_metadata(c, x, y, z, 229, &metadata))
        goto failed;
    if (metadata >= 8) {
        static const int8_t offsets[8][4] = {{0, 0, -1, 2}, {0, 0, 1, 3},  {-1, 0, 0, 4},
                                             {1, 0, 0, 5},  {0, 1, -1, 2}, {0, 1, 1, 3},
                                             {-1, 1, 0, 4}, {1, 1, 0, 5}};
        static const unsigned int pcs[] = {256, 279, 302, 325, 350, 375, 400, 425};
        bool solid = false;
        for (size_t i = 0; i < 8 && !solid; ++i)
            if (!fluid_solid(c, b, mc_java_i32_add(x, offsets[i][0]),
                             mc_java_i32_add(y, offsets[i][1]), mc_java_i32_add(z, offsets[i][2]),
                             offsets[i][3], pcs[i], &solid))
                goto failed;
        if (solid && (!fluid_vector_replace(c, &value, true, 0, 0, 0, 441) ||
                      !fluid_vector_replace(c, &value, false, 0, -6, 0, 449)))
            goto failed;
    }
    if (!fluid_vector_replace(c, &value, true, 0, 0, 0, 456))
        goto failed;
    *out = value;
    mc_cv_pc(c, 463);
    mc_cv_pop(c);
    return true;
failed:
    mc_entity_vector_release(classes, value);
    return false;
}

static bool fluid_angle(mc_cv_context *c, int32_t x, int32_t y, int32_t z, mc_material *material,
                        float *out) {
    mc_cv_pc(c, 351);
    if (!mc_cv_push_source(c, "rp", "a", "(Lxp;IIILln;)D"))
        return false;
    mc_entity_vector *value = NULL;
    mc_material *water, *lava;
    if (!fluid_material(c, MC_MATERIAL_g, 5, &water))
        return false;
    if (material == water) {
        mc_registry_block_view view;
        if (!mc_cv_registry(c, 11, &view) ||
            !fluid_vector(c, c->registry->blocks.named[7], x, y, z, 21, &value))
            return false;
    }
    if (!fluid_material(c, MC_MATERIAL_h, 28, &lava))
        goto failed;
    if (material == lava) {
        mc_registry_block_view view;
        if (!mc_cv_registry(c, 34, &view))
            goto failed;
        mc_entity_vector *next = NULL;
        if (!fluid_vector(c, c->registry->blocks.named[9], x, y, z, 44, &next))
            goto failed;
        if (value != NULL)
            mc_entity_vector_release(c->models->entities, value);
        value = next;
    }
    if (value == NULL) {
        mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                   "fluid actual flow-vector field receiver", 51, false);
        return false;
    }
    if (!mc_cv_fp(c, 55))
        goto failed;
    double angle = -1000;
    if (value->x != 0 || value->z != 0) {
        mc_cv_pc(c, 83);
        angle = at_sub(mc_player_tick_atan2(value->z, value->x), 1.5707963267948966);
    }
    mc_entity_vector_release(c->models->entities, value);
    mc_cv_pop(c);
    *out = at_narrow(angle);
    return true;
failed:
    if (value != NULL)
        mc_entity_vector_release(c->models->entities, value);
    return false;
}

bool mc_cv_fluid(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                 bool *out) {
    if (!mc_cv_nw(c, 0))
        return false;
    int32_t color;
    if (!mc_cv_block_color(c, block, x, y, z, 14, &color) || !mc_cv_fp(c, 31))
        return false;
    const float red = at_fdiv((float)(((uint32_t)color >> 16) & 255), 255);
    const float green = at_fdiv((float)(((uint32_t)color >> 8) & 255), 255);
    const float blue = at_fdiv((float)((uint32_t)color & 255), 255);
    bool top, bottom, side[4];
    if (!mc_cv_side(c, block, x, mc_java_i32_add(y, 1), z, 1, 73, &top) ||
        !mc_cv_side(c, block, x, mc_java_i32_sub(y, 1), z, 0, 90, &bottom) ||
        !mc_cv_side(c, block, x, y, mc_java_i32_sub(z, 1), 2, 115, &side[0]) ||
        !mc_cv_side(c, block, x, y, mc_java_i32_add(z, 1), 3, 134, &side[1]) ||
        !mc_cv_side(c, block, mc_java_i32_sub(x, 1), y, z, 4, 153, &side[2]) ||
        !mc_cv_side(c, block, mc_java_i32_add(x, 1), y, z, 5, 172, &side[3]))
        return false;
    if (!top && !bottom && !side[0] && !side[1] && !side[2] && !side[3]) {
        *out = false;
        mc_cv_pc(c, 215);
        return true;
    }
    const mc_block_fields *fields;
    if (!mc_cv_fields(c, block, 241, &fields))
        return false;
    mc_material *material = fields->bA;
    int32_t metadata;
    float height[4];
    if (!mc_cv_metadata(c, x, y, z, 254, &metadata) ||
        !fluid_height(c, x, y, z, material, 268, &height[0]) ||
        !fluid_height(c, x, y, mc_java_i32_add(z, 1), material, 282, &height[1]) ||
        !fluid_height(c, mc_java_i32_add(x, 1), y, mc_java_i32_add(z, 1), material, 298,
                      &height[2]) ||
        !fluid_height(c, mc_java_i32_add(x, 1), y, z, material, 312, &height[3]))
        return false;
    bool rendered = false;
    if (c->fields->f || top) {
        rendered = true;
        int32_t texture;
        float angle;
        if (!fluid_texture(c, block, 1, metadata, true, 336, &texture) ||
            !fluid_angle(c, x, y, z, material, &angle))
            return false;
        if (angle > -999 && !fluid_texture(c, block, 2, metadata, true, 369, &texture))
            return false;
        const int32_t tile_x = mc_java_i32_shl(texture & 15, 4), tile_y = texture & 240;
        double u = at_div(at_add((double)tile_x, 8), 256);
        double v = at_div(at_add((double)tile_y, 8), 256);
        if (angle < -999)
            angle = 0;
        else {
            u = at_widen(at_fdiv((float)mc_java_i32_add(tile_x, 16), 256));
            v = at_widen(at_fdiv((float)mc_java_i32_add(tile_y, 16), 256));
        }
        mc_cv_pc(c, 457);
        if (c->application == NULL || mc_math_classes_ensure(c->application->source_math_classes,
                                                             &c->failure.math) != MC_MATH_CLASS_OK)
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                              "fluid actual MathHelper active use", 457, true);
        const mc_math_trig *trig = mc_math_classes_table(c->application->source_math_classes);
        const float sine = at_fdiv(at_fmul(mc_math_sin(trig, angle), 8), 256);
        const float cosine = at_fdiv(at_fmul(mc_math_cos(trig, angle), 8), 256);
        float light;
        if (!mc_cv_brightness(c, block, x, y, z, 490, &light) ||
            !mc_cv_color(c, at_fmul(at_fmul(1, light), red), at_fmul(at_fmul(1, light), green),
                         at_fmul(at_fmul(1, light), blue), 521))
            return false;
        const double uu[4] = {at_sub(at_sub(u, cosine), sine), at_add(at_sub(u, cosine), sine),
                              at_add(at_add(u, cosine), sine), at_sub(at_add(u, cosine), sine)};
        const double vv[4] = {at_add(at_sub(v, cosine), sine), at_add(at_add(v, cosine), sine),
                              at_sub(at_add(v, cosine), sine), at_sub(at_sub(v, cosine), sine)};
        static const int8_t dx[] = {0, 0, 1, 1}, dz[] = {0, 1, 1, 0};
        static const unsigned int vertex_pc[] = {561, 601, 641, 681};
        for (size_t i = 0; i < 4; ++i)
            if (!mc_cv_vertex(c, (double)mc_java_i32_add(x, dx[i]),
                              at_widen(at_fadd((float)y, height[i])),
                              (double)mc_java_i32_add(z, dz[i]), uu[i], vv[i], vertex_pc[i]))
                return false;
    }
    if (c->fields->f || bottom) {
        float light;
        int32_t texture;
        if (!mc_cv_brightness(c, block, x, mc_java_i32_sub(y, 1), z, 707, &light))
            return false;
        light = at_fmul(.5f, light);
        if (!mc_cv_color(c, light, light, light, 729) ||
            !fluid_texture(c, block, 0, 0, false, 743, &texture) ||
            !mc_cv_face(c, 0, block, (double)x, (double)y, (double)z, texture, 746))
            return false;
        rendered = true;
    }
    for (int32_t i = 0; i < 4; ++i) {
        const int32_t nx = mc_java_i32_add(x, i == 2 ? -1 : i == 3 ? 1 : 0);
        const int32_t nz = mc_java_i32_add(z, i == 0 ? -1 : i == 1 ? 1 : 0);
        int32_t texture;
        if (!fluid_texture(c, block, i + 2, metadata, true, 813, &texture))
            return false;
        const int32_t tile_x = mc_java_i32_shl(texture & 15, 4), tile_y = texture & 240;
        if (!c->fields->f && !side[i])
            continue;
        rendered = true;
        const float h0 = height[i == 0 ? 0 : i == 1 ? 2 : i == 2 ? 1 : 3];
        const float h1 = height[i == 0 ? 3 : i == 1 ? 1 : i == 2 ? 0 : 2];
        const float x0 = (float)mc_java_i32_add(x, i == 1 || i == 3 ? 1 : 0);
        const float x1 = (float)mc_java_i32_add(x, i == 0 || i == 3 ? 1 : 0);
        const float z0 = (float)mc_java_i32_add(z, i == 1 || i == 2 ? 1 : 0);
        const float z1 = (float)mc_java_i32_add(z, i == 1 || i == 3 ? 1 : 0);
        const double u0 = at_widen(at_fdiv((float)tile_x, 256));
        const double u1 = at_div(at_sub((double)mc_java_i32_add(tile_x, 16), .01), 256);
        const double v0 =
            at_widen(at_fdiv(at_fadd((float)tile_y, at_fmul(at_fsub(1, h0), 16)), 256));
        const double v1 =
            at_widen(at_fdiv(at_fadd((float)tile_y, at_fmul(at_fsub(1, h1), 16)), 256));
        const double vb = at_div(at_sub((double)mc_java_i32_add(tile_y, 16), .01), 256);
        float light;
        if (!mc_cv_brightness(c, block, nx, y, nz, 1087, &light))
            return false;
        light = at_fmul(light, i < 2 ? .8f : .6f);
        if (!mc_cv_color(c, at_fmul(at_fmul(1, light), red), at_fmul(at_fmul(1, light), green),
                         at_fmul(at_fmul(1, light), blue), 1141) ||
            !mc_cv_vertex(c, x0, at_widen(at_fadd((float)y, h0)), z0, u0, v0, 1162) ||
            !mc_cv_vertex(c, x1, at_widen(at_fadd((float)y, h1)), z1, u1, v1, 1183) ||
            !mc_cv_vertex(c, x1, (double)y, z1, u1, vb, 1202) ||
            !mc_cv_vertex(c, x0, (double)y, z0, u0, vb, 1221))
            return false;
    }
    ((mc_block *)block)->fields.bt = 0;
    mc_cv_store(c, 1233);
    ((mc_block *)block)->fields.bw = 1;
    mc_cv_store(c, 1239);
    mc_cv_pc(c, 1244);
    *out = rendered;
    return true;
}
