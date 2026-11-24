#include "internal.h"
#include "inventory_color.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../entity/player/tick_math_internal.h"
#include "../../display/source_surface.h"

bool mc_cv_inventory_type_3d(int32_t type) {
    return type == 0 || type == 13 || type == 10 || type == 11 || type == 16;
}

static bool inventory_gl(mc_cv_context *c, bool ok, unsigned int pc) {
    mc_cv_pc(c, pc);
    return ok || mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                            c->failure.display.operation, pc, true);
}

static bool inventory_nw(mc_cv_context *c, bool ok, unsigned int pc) {
    mc_cv_pc(c, pc);
    if (ok)
        return true;
    c->failure.child = c->failure.graphics.site;
    return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, c->failure.graphics.operation, pc,
                      c->failure.graphics.native_policy);
}

bool mc_cv_leaf_inventory_color(mc_cv_context *c, int32_t metadata, int32_t *out) {
    mc_cv_pc(c, 14);
    if (!mc_cv_push_source(c, "bk", "b", "(I)I"))
        return false;
    const bool pine = ((uint32_t)metadata & 1u) == 1u;
    const bool birch = !pine && ((uint32_t)metadata & 2u) == 2u;
    const unsigned int invoke = pine ? 7u : birch ? 18u : 22u;
    const mc_client_colorizer_read_kind read = pine    ? MC_CLIENT_COLORIZER_PINE
                                               : birch ? MC_CLIENT_COLORIZER_BIRCH
                                                       : MC_CLIENT_COLORIZER_DEFAULT_FOLIAGE;
    mc_cv_pc(c, invoke);
    if (mc_client_colorizer_initialize(c->graphics, MC_CLIENT_COLORIZER_FOLIAGE, NULL,
                                       &c->colorizer_error) != MC_CLIENT_COLORIZER_OK) {
        const mc_client_colorizer_error *e = &c->colorizer_error;
        c->failure.child = e->site;
        return mc_cv_fail(c,
                          e->status == MC_CLIENT_COLORIZER_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                          : e->status == MC_CLIENT_COLORIZER_BUSY
                              ? MC_CLIENT_RENDER_GLOBAL_BUSY
                              : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                          e->operation, invoke, e->native_policy);
    }
    if (!mc_cv_push_source(c, "jh", pine ? "a" : birch ? "b" : "c", "()I"))
        return false;
    if (mc_client_colorizer_read_ready(c->graphics, MC_CLIENT_COLORIZER_FOLIAGE, read, 0.0, 0.0,
                                       out, &c->colorizer_error) != MC_CLIENT_COLORIZER_OK) {
        const mc_client_colorizer_error *e = &c->colorizer_error;
        c->failure.child = e->site;
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, e->operation, 0,
                          e->native_policy);
    }
    mc_cv_pc(c, 2);
    mc_cv_pop(c);
    mc_cv_pc(c, pine ? 10u : birch ? 21u : 25u);
    mc_cv_pop(c);
    return true;
}

typedef struct inventory_face_sites {
    unsigned int begin[6], normal[6], texture[6], face[6], draw[6];
} inventory_face_sites;

static const inventory_face_sites cube_sites = {{119, 152, 184, 217, 249, 282},
                                                {128, 160, 193, 225, 258, 290},
                                                {139, 171, 204, 236, 269, 301},
                                                {142, 174, 207, 239, 272, 304},
                                                {147, 179, 212, 244, 277, 309}};
static const inventory_face_sites cactus_sites = {{393, 425, 456, 507, 557, 608},
                                                  {402, 433, 465, 515, 566, 616},
                                                  {412, 443, 484, 535, 585, 636},
                                                  {415, 446, 487, 538, 588, 639},
                                                  {420, 451, 502, 552, 603, 653}};
static const inventory_face_sites stairs_sites = {{817, 849, 880, 912, 943, 975},
                                                  {826, 857, 889, 920, 952, 983},
                                                  {836, 867, 899, 930, 962, 993},
                                                  {839, 870, 902, 933, 965, 996},
                                                  {844, 875, 907, 938, 970, 1001}};
static const inventory_face_sites fence_sites = {{1198, 1230, 1261, 1293, 1324, 1356},
                                                 {1207, 1238, 1270, 1301, 1333, 1364},
                                                 {1217, 1248, 1280, 1311, 1343, 1374},
                                                 {1220, 1251, 1283, 1314, 1346, 1377},
                                                 {1225, 1256, 1288, 1319, 1351, 1382}};

static bool inventory_faces(mc_cv_context *c, const mc_block *block, int32_t metadata,
                            const inventory_face_sites *sites, bool side_texture, bool cactus) {
    static const float normals[6][3] = {{0, -1, 0}, {0, 1, 0},  {0, 0, -1},
                                        {0, 0, 1},  {-1, 0, 0}, {1, 0, 0}};
    /* Every zero operand is the source's positive fconst_0, including undo.
     * Negating whole triples would change zero signs and live NW offsets. */
    static const float offset[4][2][3] = {{{0, 0, .0625f}, {0, 0, -.0625f}},
                                          {{0, 0, -.0625f}, {0, 0, .0625f}},
                                          {{.0625f, 0, 0}, {-.0625f, 0, 0}},
                                          {{-.0625f, 0, 0}, {.0625f, 0, 0}}};
    static const unsigned int offset_pc[4][2] = {{474, 497}, {525, 547}, {575, 598}, {626, 648}};
    for (int32_t side = 0; side < 6; ++side) {
        if (!inventory_nw(c, mc_client_nw_begin_quads(c->tessellator, &c->failure.graphics),
                          sites->begin[side]) ||
            !inventory_nw(c,
                          mc_client_nw_normal(c->tessellator, normals[side][0], normals[side][1],
                                              normals[side][2], &c->failure.graphics),
                          sites->normal[side]))
            return false;
        if (cactus && side >= 2) {
            const float *delta = offset[side - 2][0];
            if (!inventory_nw(c,
                              mc_client_nw_translation_add(c->tessellator, delta[0], delta[1],
                                                           delta[2], &c->failure.graphics),
                              offset_pc[side - 2][0]))
                return false;
        }
        int32_t icon;
        if (side_texture) {
            if (!mc_cv_side_texture(c, block, side, sites->texture[side], &icon))
                return false;
        } else if (mc_block_render_texture_metadata(block, side, metadata, &icon) !=
                   MC_REGISTRY_OK) {
            c->failure.child = (mc_client_graphics_site){"uu", "a", "(II)I", 0};
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                              "inventory actual Block face texture", sites->texture[side], true);
        }
        if (!mc_cv_face(c, side, block, 0.0, 0.0, 0.0, icon, sites->face[side]))
            return false;
        if (cactus && side >= 2) {
            const float *delta = offset[side - 2][1];
            if (!inventory_nw(c,
                              mc_client_nw_translation_add(c->tessellator, delta[0], delta[1],
                                                           delta[2], &c->failure.graphics),
                              offset_pc[side - 2][1]))
                return false;
        }
        if (!inventory_nw(c, mc_client_nw_draw(c->tessellator, c->display, &c->failure.graphics),
                          sites->draw[side]))
            return false;
        ++c->inventory_faces;
    }
    return true;
}

static bool inventory_translate(mc_cv_context *c, float delta, unsigned int pc) {
    return inventory_gl(
        c, mc_client_display_source_translate(c->display, delta, delta, delta, &c->failure.display),
        pc);
}

static bool inventory_shape(mc_cv_context *c, const mc_block *block, unsigned int pc) {
    /* Registry lookup exposes a const view; the actual Source Block receiver
     * remains mutable and its virtual g() may replace its live bounds. */
    if (mc_block_render_inventory_shape((mc_block *)(void *)block) == MC_REGISTRY_OK)
        return true;
    c->failure.child = (mc_client_graphics_site){"uu", "g", "()V", 0};
    return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                      "inventory actual Block item-bound hook", pc, true);
}

static bool inventory_parts(mc_cv_context *c, const mc_block *block, bool fence) {
    static const float stairs_bounds[2][6] = {{0, 0, 0, 1, 1, .5f}, {0, 0, .5f, 1, .5f, 1}};
    static const float fence_bounds[4][6] = {{.375f, 0, 0, .625f, 1, .25f},
                                             {.375f, 0, .75f, .625f, 1, 1},
                                             {.4375f, .8125f, -.125f, .5625f, .9375f, 1.125f},
                                             {.4375f, .3125f, -.125f, .5625f, .4375f, 1.125f}};
    static const unsigned int stairs_pc[2] = {785, 803};
    static const unsigned int fence_pc[4] = {1065, 1094, 1140, 1184};
    for (size_t part = 0; part < (fence ? 4u : 2u); ++part) {
        const float *bounds = fence ? fence_bounds[part] : stairs_bounds[part];
        mc_cv_pc(c, fence ? fence_pc[part] : stairs_pc[part]);
        mc_registry_block_bounds((mc_block *)(void *)block, bounds[0], bounds[1], bounds[2],
                                 bounds[3], bounds[4], bounds[5]);
        if (!inventory_translate(c, -.5f, fence ? 1193u : 812u) ||
            !inventory_faces(c, block, 0, fence ? &fence_sites : &stairs_sites, true, false) ||
            !inventory_translate(c, .5f, fence ? 1391u : 1010u))
            return false;
    }
    if (fence) {
        mc_cv_pc(c, 1407);
        mc_registry_block_bounds((mc_block *)(void *)block, 0, 0, 0, 1, 1, 1);
    }
    return true;
}

bool mc_cv_inventory(mc_cv_context *c, const mc_block *block, int32_t metadata, float brightness) {
    if (!mc_cv_push(c, "a", "(Luu;IF)V") || !mc_cv_nw(c, 0))
        return false;
    if (c->fields->b) {
        if (block == NULL)
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                              "inventory Block color null receiver", 14, false);
        int32_t color;
        if (block->kind == MC_BLOCK_LEAVES) {
            if (!mc_cv_leaf_inventory_color(c, metadata, &color))
                return false;
        } else if (mc_block_render_inventory_color(block, metadata, &color) != MC_REGISTRY_OK) {
            c->failure.child = (mc_client_graphics_site){"uu", "b", "(I)I", 0};
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                              "inventory actual Block metadata-color body", 14, true);
        }
        if (!mc_cv_fp(c, 31))
            return false;
        const float red = at_fdiv((float)(((uint32_t)color >> 16u) & 255u), 255.0f);
        const float green = at_fdiv((float)(((uint32_t)color >> 8u) & 255u), 255.0f);
        const float blue = at_fdiv((float)((uint32_t)color & 255u), 255.0f);
        if (!inventory_gl(c,
                          mc_client_display_source_color(
                              c->display, at_fmul(red, brightness), at_fmul(green, brightness),
                              at_fmul(blue, brightness), 1.0f, &c->failure.display),
                          74))
            return false;
    }
    if (block == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "inventory Block render-type null receiver", 78, false);
    mc_block_render_description description;
    if (mc_block_render_describe(block, &description) != MC_REGISTRY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                          "inventory actual Block render-type dispatch", 78, true);
    switch (description.type) {
        case 0:
        case 16:
            if (description.type == 16)
                metadata = 1;
            if (!inventory_shape(c, block, 105) || !inventory_translate(c, -.5f, 114) ||
                !inventory_faces(c, block, metadata, &cube_sites, false, false) ||
                !inventory_translate(c, .5f, 318))
                return false;
            break;
        case 13:
            if (!inventory_shape(c, block, 375) || !inventory_translate(c, -.5f, 384) ||
                !inventory_faces(c, block, metadata, &cactus_sites, true, true) ||
                !inventory_translate(c, .5f, 662))
                return false;
            break;
        case 10:
        case 11:
            if (!inventory_parts(c, block, description.type == 11))
                return false;
            break;
        default:
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                              "inventory actual specialized Block geometry", 324, true);
    }
    mc_cv_pc(c, 1410);
    c->returned = true;
    mc_cv_pop(c);
    return true;
}
