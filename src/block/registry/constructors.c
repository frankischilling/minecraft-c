#include "registry_internal.h"

#include "mc/core/java/java.h"

#include <string.h>

/* Independently authored source-order constructor bodies. Class initialization
 * and RAW allocation precede argument evaluation in the registration caller.
 * Base publication and every reached child allocation survive later failure. */
#define STAGE(c, s, pc) mc_registry_block_stage(block, (c), (s), (pc))
#define TRY(expression)                                                                            \
    do {                                                                                           \
        mc_registry_status status = (expression);                                                  \
        if (status != MC_REGISTRY_OK)                                                              \
            return status;                                                                         \
    } while (0)

static mc_material *material(mc_registry_classes *classes, mc_material_id id) {
    return mc_materials_get(classes->options.materials, id);
}

/* The common texture-constructor prefix loads its material at3 and invokes
 * its superclass at6. The next source instruction is9; callers own any tail.
 * Names remain the actual declaring class. */
static mc_registry_status direct_texture(mc_registry_classes *classes, mc_block *block, int32_t id,
                                         int32_t texture, mc_material_id selector,
                                         const char *client, const char *server) {
    STAGE(client, server, 0);
    STAGE(client, server, 3);
    mc_material *selected = material(classes, selector);
    STAGE(client, server, 6);
    TRY(mc_registry_block_base_texture(classes, block, id, texture, selected));
    STAGE(client, server, 9);
    return MC_REGISTRY_OK;
}

static mc_registry_status nr_construct(mc_registry_classes *classes, mc_block *block, int32_t id,
                                       int32_t texture, mc_material *selected, bool flag) {
    STAGE("nr", "in", 0);
    STAGE("nr", "in", 4);
    TRY(mc_registry_block_base_texture(classes, block, id, texture, selected));
    STAGE("nr", "in", 10);
    block->nr_b = flag;
    STAGE("nr", "in", 13);
    return MC_REGISTRY_OK;
}

static mc_registry_status jp_construct(mc_registry_classes *classes, mc_block *block, int32_t id,
                                       int32_t texture, mc_material *selected, bool flag) {
    STAGE("jp", "gh", 0);
    STAGE("jp", "gh", 4);
    TRY(mc_registry_block_base_texture(classes, block, id, texture, selected));
    STAGE("jp", "gh", 10);
    block->jp_a = flag;
    STAGE("jp", "gh", 13);
    return MC_REGISTRY_OK;
}

static mc_registry_status rp_construct(mc_registry_classes *classes, mc_block *block, int32_t id,
                                       mc_material *selected) {
    STAGE("rp", "kv", 0);
    STAGE("rp", "kv", 3);
    mc_material *lava = material(classes, MC_MATERIAL_h);
    int32_t texture = selected == lava ? 14 : 12;
    texture = mc_java_i32_mul(texture, 16);
    texture = mc_java_i32_add(texture, 13);
    STAGE("rp", "kv", 23);
    TRY(mc_registry_block_base_texture(classes, block, id, texture, selected));
    float vertical = 0.0f;
    float horizontal = 0.0f;
    float x0 = 0.0f + horizontal;
    float y0 = 0.0f + vertical;
    float z0 = 0.0f + horizontal;
    float x1 = 1.0f + horizontal;
    float y1 = 1.0f + vertical;
    float z1 = 1.0f + horizontal;
    STAGE("rp", "kv", 54);
    mc_registry_block_bounds(block, x0, y0, z0, x1, y1, z1);
    STAGE("rp", "kv", 59);
    TRY(mc_registry_block_tick(classes, block, true));
    STAGE("rp", "kv", 63);
    return MC_REGISTRY_OK;
}

static mc_registry_status tile_flag(mc_registry_classes *classes, mc_block *block, int32_t id,
                                    int32_t pc) {
    STAGE("rw", "lb", pc);
    if (classes->blocks.p == NULL)
        return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "tile block array", block);
    if (id < 0 || id >= (int32_t)MC_REGISTRY_BLOCK_SLOTS)
        return mc_registry_fail(classes, MC_REGISTRY_INDEX_OUT_OF_RANGE, "tile block index", block);
    classes->blocks.p[(size_t)id] = true;
    return MC_REGISTRY_OK;
}

static mc_registry_status rw_construct(mc_registry_classes *classes, mc_block *block, int32_t id,
                                       mc_material *selected) {
    STAGE("rw", "lb", 0);
    STAGE("rw", "lb", 3);
    TRY(mc_registry_block_base(classes, block, id, selected));
    TRY(tile_flag(classes, block, id, 11));
    STAGE("rw", "lb", 12);
    return MC_REGISTRY_OK;
}

static mc_registry_status rw_texture_construct(mc_registry_classes *classes, mc_block *block,
                                               int32_t id, int32_t texture, mc_material *selected) {
    STAGE("rw", "lb", 0);
    STAGE("rw", "lb", 4);
    TRY(mc_registry_block_base_texture(classes, block, id, texture, selected));
    TRY(tile_flag(classes, block, id, 12));
    STAGE("rw", "lb", 13);
    return MC_REGISTRY_OK;
}

static mc_registry_status wb_construct(mc_registry_classes *classes, mc_block *block, int32_t id,
                                       int32_t texture) {
    STAGE("wb", "nw", 0);
    mc_material *selected = material(classes, MC_MATERIAL_j);
    STAGE("wb", "nw", 5);
    TRY(mc_registry_block_base(classes, block, id, selected));
    STAGE("wb", "nw", 10);
    block->fields.bm = texture;
    STAGE("wb", "nw", 15);
    TRY(mc_registry_block_tick(classes, block, true));
    float radius = 0.2f;
    float x0 = 0.5f - radius;
    float z0 = 0.5f - radius;
    float x1 = 0.5f + radius;
    float y1 = radius * 3.0f;
    float z1 = 0.5f + radius;
    STAGE("wb", "nw", 44);
    mc_registry_block_bounds(block, x0, 0.0f, z0, x1, y1, z1);
    STAGE("wb", "nw", 47);
    return MC_REGISTRY_OK;
}

static mc_registry_status pc_construct(mc_registry_classes *classes, mc_block *block, int32_t id,
                                       int32_t texture, bool flag) {
    TRY(direct_texture(classes, block, id, texture, MC_MATERIAL_o, "pc", "jj"));
    STAGE("pc", "jj", 11);
    block->subtype.pc.a = flag;
    STAGE("pc", "jj", 22);
    mc_registry_block_bounds(block, 0.0f, 0.0f, 0.0f, 1.0f, 0.125f, 1.0f);
    STAGE("pc", "jj", 25);
    return MC_REGISTRY_OK;
}

static mc_registry_status vm_construct(mc_registry_classes *classes, mc_block *block, int32_t id,
                                       int32_t texture) {
    TRY(direct_texture(classes, block, id, texture, MC_MATERIAL_o, "vm", "np"));
    STAGE("vm", "np", 11);
    TRY(mc_registry_block_tick(classes, block, true));
    STAGE("vm", "np", 15);
    return MC_REGISTRY_OK;
}

/* These I constructors load a material at2, call B5 and store bm11. */
static mc_registry_status fixed_texture(mc_registry_classes *classes, mc_block *block, int32_t id,
                                        mc_material_id selector, int32_t texture,
                                        const char *client, const char *server) {
    STAGE(client, server, 0);
    STAGE(client, server, 2);
    mc_material *selected = material(classes, selector);
    STAGE(client, server, 5);
    TRY(mc_registry_block_base(classes, block, id, selected));
    STAGE(client, server, 11);
    block->fields.bm = texture;
    STAGE(client, server, 14);
    return MC_REGISTRY_OK;
}

static void ve_bounds(mc_block *block) {
    STAGE("ve", "nj", 0);
    STAGE("ve", "nj", 8);
    mc_registry_block_bounds(block, 0.0f, 0.0f, 0.0f, 1.0f, 0.5625f, 1.0f);
    STAGE("ve", "nj", 11);
}

static mc_registry_status constructor_body(mc_registry_classes *classes, mc_block *block,
                                           const mc_block_arguments *a) {
    mc_material *selected;
    switch (block->kind) {
        case MC_BLOCK_UU_NA:
            return mc_registry_block_base_texture(classes, block, a->id, a->texture, a->material);
        case MC_BLOCK_EO_CX:
            return direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_e, "eo", "cx");
        case MC_BLOCK_WP_OJ:
            STAGE("wp", "oj", 0);
            selected = material(classes, MC_MATERIAL_b);
            STAGE("wp", "oj", 5);
            TRY(mc_registry_block_base(classes, block, a->id, selected));
            STAGE("wp", "oj", 10);
            block->fields.bm = 3;
            STAGE("wp", "oj", 15);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("wp", "oj", 19);
            return MC_REGISTRY_OK;
        case MC_BLOCK_OT_JF:
            return direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_c, "ot", "jf");
        case MC_BLOCK_HE_EU: {
            STAGE("he", "eu", 0);
            STAGE("he", "eu", 3);
            TRY(wb_construct(classes, block, a->id, a->texture));
            float radius = 0.4f;
            float x0 = 0.5f - radius;
            float z0 = 0.5f - radius;
            float x1 = 0.5f + radius;
            float y1 = radius * 2.0f;
            float z1 = 0.5f + radius;
            STAGE("he", "eu", 30);
            mc_registry_block_bounds(block, x0, 0.0f, z0, x1, y1, z1);
            STAGE("he", "eu", 33);
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_OM_JC: {
            STAGE("om", "jc", 0);
            STAGE("om", "jc", 3);
            TRY(rp_construct(classes, block, a->id, a->material));
            STAGE("om", "jc", 8);
            block->subtype.om.a = 0;
            STAGE("om", "jc", 13);
            bool *flow = mc_registry_allocate(classes, 4u * sizeof(*flow));
            if (flow == NULL)
                return classes->last_error.status;
            memset(flow, 0, 4u * sizeof(*flow));
            STAGE("om", "jc", 15);
            block->subtype.om.b = flow;
            STAGE("om", "jc", 20);
            int32_t *cost = mc_registry_allocate(classes, 4u * sizeof(*cost));
            if (cost == NULL)
                return classes->last_error.status;
            memset(cost, 0, 4u * sizeof(*cost));
            STAGE("om", "jc", 22);
            block->subtype.om.c = cost;
            STAGE("om", "jc", 25);
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_NX_IS:
            STAGE("nx", "is", 0);
            STAGE("nx", "is", 3);
            TRY(rp_construct(classes, block, a->id, a->material));
            STAGE("nx", "is", 8);
            TRY(mc_registry_block_tick(classes, block, false));
            STAGE("nx", "is", 13);
            selected = material(classes, MC_MATERIAL_h);
            if (a->material == selected) {
                STAGE("nx", "is", 21);
                TRY(mc_registry_block_tick(classes, block, true));
            }
            STAGE("nx", "is", 25);
            return MC_REGISTRY_OK;
        case MC_BLOCK_GK_EI:
            return direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_n, "gk", "ei");
        case MC_BLOCK_NE_IG:
            STAGE("ne", "ig", 0);
            STAGE("ne", "ig", 3);
            TRY(direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_n, "gk", "ei"));
            STAGE("ne", "ig", 6);
            return MC_REGISTRY_OK;
        case MC_BLOCK_MT_IC:
            return direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_e, "mt", "ic");
        case MC_BLOCK_VG_NK:
            return fixed_texture(classes, block, a->id, MC_MATERIAL_d, 20, "vg", "nk");
        case MC_BLOCK_BK_AR:
            STAGE("bk", "ar", 0);
            selected = material(classes, MC_MATERIAL_i);
            STAGE("bk", "ar", 7);
            TRY(nr_construct(classes, block, a->id, a->texture, selected, false));
            STAGE("bk", "ar", 12);
            block->subtype.bk.c = a->texture;
            STAGE("bk", "ar", 17);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("bk", "ar", 21);
            return MC_REGISTRY_OK;
        case MC_BLOCK_XF_OV:
            return fixed_texture(classes, block, a->id, MC_MATERIAL_k, 48, "xf", "ov");
        case MC_BLOCK_FK_DO:
            STAGE("fk", "do", 0);
            STAGE("fk", "do", 6);
            TRY(jp_construct(classes, block, a->id, a->texture, a->material, a->flag));
            STAGE("fk", "do", 9);
            return MC_REGISTRY_OK;
        case MC_BLOCK_XQ_PA:
            STAGE("xq", "pa", 0);
            selected = material(classes, MC_MATERIAL_e);
            STAGE("xq", "pa", 5);
            TRY(rw_construct(classes, block, a->id, selected));
            STAGE("xq", "pa", 13);
            TRY(mc_registry_block_random(classes, block, &block->subtype.xq.a));
            STAGE("xq", "pa", 16);
            STAGE("xq", "pa", 22);
            block->fields.bm = 45;
            STAGE("xq", "pa", 25);
            return MC_REGISTRY_OK;
        case MC_BLOCK_RD_KL:
            STAGE("rd", "kl", 0);
            selected = material(classes, MC_MATERIAL_e);
            STAGE("rd", "kl", 8);
            TRY(mc_registry_block_base_texture(classes, block, a->id, 192, selected));
            STAGE("rd", "kl", 11);
            return MC_REGISTRY_OK;
        case MC_BLOCK_PT_JR:
            STAGE("pt", "jr", 0);
            selected = material(classes, MC_MATERIAL_d);
            STAGE("pt", "jr", 7);
            TRY(rw_texture_construct(classes, block, a->id, 74, selected));
            STAGE("pt", "jr", 10);
            return MC_REGISTRY_OK;
        case MC_BLOCK_VE_NJ:
            STAGE("ve", "nj", 0);
            selected = material(classes, MC_MATERIAL_l);
            STAGE("ve", "nj", 8);
            TRY(mc_registry_block_base_texture(classes, block, a->id, 134, selected));
            STAGE("ve", "nj", 12);
            ve_bounds(block);
            STAGE("ve", "nj", 15);
            return MC_REGISTRY_OK;
        case MC_BLOCK_PC_JJ:
            return pc_construct(classes, block, a->id, a->texture, a->flag);
        case MC_BLOCK_PH_JN:
            STAGE("ph", "jn", 0);
            STAGE("ph", "jn", 4);
            TRY(pc_construct(classes, block, a->id, a->texture, true));
            STAGE("ph", "jn", 9);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("ph", "jn", 13);
            return MC_REGISTRY_OK;
        case MC_BLOCK_JQ_GI: {
            TRY(direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_B, "jq", "gi"));
            STAGE("jq", "gi", 11);
            block->subtype.jq.a = a->flag;
            STAGE("jq", "gi", 15);
            const mc_block_sound *piston_sound = classes->blocks.h;
            STAGE("jq", "gi", 18);
            mc_registry_block_sound(block, piston_sound);
            STAGE("jq", "gi", 25);
            mc_registry_block_hardness(block, 0.5f);
            STAGE("jq", "gi", 29);
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_RN_KU:
            return direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_A, "rn", "ku");
        case MC_BLOCK_RU_KZ:
        case MC_BLOCK_JB_FZ: {
            const char *client = block->kind == MC_BLOCK_RU_KZ ? "ru" : "jb";
            const char *server = block->kind == MC_BLOCK_RU_KZ ? "kz" : "fz";
            STAGE(client, server, 0);
            STAGE(client, server, 3);
            TRY(wb_construct(classes, block, a->id, a->texture));
            float radius = 0.4f;
            float x0 = 0.5f - radius;
            float z0 = 0.5f - radius;
            float x1 = 0.5f + radius;
            float z1 = 0.5f + radius;
            STAGE(client, server, 29);
            mc_registry_block_bounds(block, x0, 0.0f, z0, x1, 0.8f, z1);
            STAGE(client, server, 32);
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_H_G: {
            TRY(direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_B, "h", "g"));
            STAGE("h", "g", 11);
            block->subtype.h.a = -1;
            STAGE("h", "g", 15);
            const mc_block_sound *extension_sound = classes->blocks.h;
            STAGE("h", "g", 18);
            mc_registry_block_sound(block, extension_sound);
            STAGE("h", "g", 25);
            mc_registry_block_hardness(block, 0.5f);
            STAGE("h", "g", 29);
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_EE_CQ:
            STAGE("ee", "cq", 0);
            STAGE("ee", "cq", 1);
            block->requested_id = 35;
            selected = material(classes, MC_MATERIAL_l);
            STAGE("ee", "cq", 8);
            TRY(mc_registry_block_base_texture(classes, block, 35, 64, selected));
            STAGE("ee", "cq", 11);
            return MC_REGISTRY_OK;
        case MC_BLOCK_UT_MZ:
            STAGE("ut", "mz", 0);
            selected = material(classes, MC_MATERIAL_B);
            STAGE("ut", "mz", 5);
            TRY(rw_construct(classes, block, a->id, selected));
            STAGE("ut", "mz", 11);
            mc_registry_block_hardness(block, -1.0f);
            STAGE("ut", "mz", 15);
            return MC_REGISTRY_OK;
        case MC_BLOCK_WB_NW:
            return wb_construct(classes, block, a->id, a->texture);
        case MC_BLOCK_TL_MD: {
            STAGE("tl", "md", 0);
            STAGE("tl", "md", 3);
            TRY(wb_construct(classes, block, a->id, a->texture));
            float radius = 0.2f;
            float x0 = 0.5f - radius;
            float z0 = 0.5f - radius;
            float x1 = 0.5f + radius;
            float y1 = radius * 2.0f;
            float z1 = 0.5f + radius;
            STAGE("tl", "md", 30);
            mc_registry_block_bounds(block, x0, 0.0f, z0, x1, y1, z1);
            STAGE("tl", "md", 35);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("tl", "md", 39);
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_L_J:
            STAGE("l", "j", 0);
            selected = material(classes, MC_MATERIAL_f);
            STAGE("l", "j", 5);
            TRY(mc_registry_block_base(classes, block, a->id, selected));
            STAGE("l", "j", 10);
            block->fields.bm = a->texture;
            STAGE("l", "j", 13);
            return MC_REGISTRY_OK;
        case MC_BLOCK_YS_PW:
            STAGE("ys", "pw", 0);
            selected = material(classes, MC_MATERIAL_e);
            STAGE("ys", "pw", 7);
            TRY(mc_registry_block_base_texture(classes, block, a->id, 6, selected));
            STAGE("ys", "pw", 12);
            block->subtype.ys.b = a->flag;
            if (!a->flag) {
                STAGE("ys", "pw", 27);
                mc_registry_block_bounds(block, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 1.0f);
            }
            STAGE("ys", "pw", 34);
            TRY(mc_registry_block_opacity(classes, block, 255));
            STAGE("ys", "pw", 38);
            return MC_REGISTRY_OK;
        case MC_BLOCK_AH_T:
            return direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_q, "ah", "t");
        case MC_BLOCK_HB_ER:
            return direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_d, "hb", "er");
        case MC_BLOCK_FB_DH:
            STAGE("fb", "dh", 0);
            STAGE("fb", "dh", 3);
            TRY(direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_e, "eo", "cx"));
            STAGE("fb", "dh", 6);
            return MC_REGISTRY_OK;
        case MC_BLOCK_VM_NP:
            return vm_construct(classes, block, a->id, a->texture);
        case MC_BLOCK_YQ_PT: {
            TRY(direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_m, "yq", "pt"));
            STAGE("yq", "pt", 13);
            int32_t *encouragement = mc_registry_allocate(classes, 256u * sizeof(*encouragement));
            if (encouragement == NULL)
                return classes->last_error.status;
            memset(encouragement, 0, 256u * sizeof(*encouragement));
            STAGE("yq", "pt", 15);
            block->subtype.yq.a = encouragement;
            STAGE("yq", "pt", 22);
            int32_t *flammability = mc_registry_allocate(classes, 256u * sizeof(*flammability));
            if (flammability == NULL)
                return classes->last_error.status;
            memset(flammability, 0, 256u * sizeof(*flammability));
            STAGE("yq", "pt", 24);
            block->subtype.yq.b = flammability;
            STAGE("yq", "pt", 29);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("yq", "pt", 33);
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_DD_BZ:
            STAGE("dd", "bz", 0);
            selected = material(classes, MC_MATERIAL_e);
            STAGE("dd", "bz", 6);
            TRY(rw_texture_construct(classes, block, a->id, a->texture, selected));
            STAGE("dd", "bz", 9);
            return MC_REGISTRY_OK;
        case MC_BLOCK_SS_LU: {
            STAGE("ss", "lu", 0);
            STAGE("ss", "lu", 3);
            mc_block *source = a->source_block;
            if (source == NULL)
                return mc_registry_fail(classes, MC_REGISTRY_NULL_REFERENCE, "stair source block",
                                        block);
            int32_t texture = source->fields.bm;
            STAGE("ss", "lu", 7);
            mc_material *source_material = source->fields.bA;
            STAGE("ss", "lu", 10);
            TRY(mc_registry_block_base_texture(classes, block, a->id, texture, source_material));
            STAGE("ss", "lu", 15);
            block->subtype.ss.a = source;
            STAGE("ss", "lu", 20);
            float hardness = source->fields.bo;
            STAGE("ss", "lu", 23);
            mc_registry_block_hardness(block, hardness);
            STAGE("ss", "lu", 29);
            float resistance = source->fields.bp;
            resistance = resistance / 3.0f;
            STAGE("ss", "lu", 35);
            mc_registry_block_resistance(block, resistance);
            STAGE("ss", "lu", 41);
            const mc_block_sound *sound = source->fields.by;
            STAGE("ss", "lu", 44);
            mc_registry_block_sound(block, sound);
            STAGE("ss", "lu", 52);
            TRY(mc_registry_block_opacity(classes, block, 255));
            STAGE("ss", "lu", 56);
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_E_E:
            STAGE("e", "e", 0);
            selected = material(classes, MC_MATERIAL_d);
            STAGE("e", "e", 5);
            TRY(rw_construct(classes, block, a->id, selected));
            STAGE("e", "e", 13);
            TRY(mc_registry_block_random(classes, block, &block->subtype.e.a));
            STAGE("e", "e", 16);
            STAGE("e", "e", 22);
            block->fields.bm = 26;
            STAGE("e", "e", 25);
            return MC_REGISTRY_OK;
        case MC_BLOCK_SM_LO: {
            TRY(direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_o, "sm", "lo"));
            STAGE("sm", "lo", 11);
            block->subtype.sm.a = true;
            STAGE("sm", "lo", 19);
            mc_registry_wire_set *set = NULL;
            TRY(mc_registry_wire_set_create(classes, &set));
            STAGE("sm", "lo", 22);
            block->subtype.sm.b = set;
            STAGE("sm", "lo", 33);
            mc_registry_block_bounds(block, 0.0f, 0.0f, 0.0f, 1.0f, 0.0625f, 1.0f);
            STAGE("sm", "lo", 36);
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_FI_DN:
            return fixed_texture(classes, block, a->id, MC_MATERIAL_d, 59, "fi", "dn");
        case MC_BLOCK_NI_IJ: {
            STAGE("ni", "ij", 0);
            STAGE("ni", "ij", 3);
            TRY(wb_construct(classes, block, a->id, a->texture));
            STAGE("ni", "ij", 8);
            block->fields.bm = a->texture;
            STAGE("ni", "ij", 13);
            TRY(mc_registry_block_tick(classes, block, true));
            float radius = 0.5f;
            float x0 = 0.5f - radius;
            float z0 = 0.5f - radius;
            float x1 = 0.5f + radius;
            float z1 = 0.5f + radius;
            STAGE("ni", "ij", 40);
            mc_registry_block_bounds(block, x0, 0.0f, z0, x1, 0.25f, z1);
            STAGE("ni", "ij", 43);
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_VL_NO:
            TRY(fixed_texture(classes, block, a->id, MC_MATERIAL_c, 87, "vl", "no"));
            STAGE("vl", "no", 16);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("vl", "no", 28);
            mc_registry_block_bounds(block, 0.0f, 0.0f, 0.0f, 1.0f, 0.9375f, 1.0f);
            STAGE("vl", "no", 35);
            TRY(mc_registry_block_opacity(classes, block, 255));
            STAGE("vl", "no", 39);
            return MC_REGISTRY_OK;
        case MC_BLOCK_TC_LZ:
            STAGE("tc", "lz", 0);
            selected = material(classes, MC_MATERIAL_e);
            STAGE("tc", "lz", 5);
            TRY(rw_construct(classes, block, a->id, selected));
            STAGE("tc", "lz", 13);
            TRY(mc_registry_block_random(classes, block, &block->subtype.tc.a));
            STAGE("tc", "lz", 16);
            STAGE("tc", "lz", 21);
            block->subtype.tc.b = a->flag;
            STAGE("tc", "lz", 27);
            block->fields.bm = 45;
            STAGE("tc", "lz", 30);
            return MC_REGISTRY_OK;
        case MC_BLOCK_UJ_MT: {
            STAGE("uj", "mt", 0);
            selected = material(classes, MC_MATERIAL_d);
            STAGE("uj", "mt", 5);
            TRY(rw_construct(classes, block, a->id, selected));
            STAGE("uj", "mt", 10);
            block->subtype.uj.b = a->flag;
            STAGE("uj", "mt", 15);
            block->fields.bm = 4;
            STAGE("uj", "mt", 20);
            block->subtype.uj.a = a->tile_class;
            float radius = 0.25f;
            float height = 1.0f;
            float x0 = 0.5f - radius;
            float z0 = 0.5f - radius;
            float x1 = 0.5f + radius;
            float z1 = 0.5f + radius;
            STAGE("uj", "mt", 54);
            mc_registry_block_bounds(block, x0, 0.0f, z0, x1, height, z1);
            STAGE("uj", "mt", 57);
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_LE_HC:
        case MC_BLOCK_OQ_JD: {
            bool door = block->kind == MC_BLOCK_LE_HC;
            const char *client = door ? "le" : "oq";
            const char *server = door ? "hc" : "jd";
            STAGE(client, server, 0);
            STAGE(client, server, 3);
            TRY(mc_registry_block_base(classes, block, a->id, a->material));
            STAGE(client, server, 9);
            block->fields.bm = door ? 97 : 84;
            STAGE(client, server, 13);
            selected = material(classes, MC_MATERIAL_f);
            if (a->material == selected) {
                int32_t texture = block->fields.bm;
                texture = mc_java_i32_add(texture, 1);
                STAGE(client, server, 26);
                block->fields.bm = texture;
            }
            float radius = 0.5f;
            float height = 1.0f;
            float x0 = 0.5f - radius;
            float z0 = 0.5f - radius;
            float x1 = 0.5f + radius;
            float z1 = 0.5f + radius;
            STAGE(client, server, 55);
            mc_registry_block_bounds(block, x0, 0.0f, z0, x1, height, z1);
            STAGE(client, server, 58);
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_DP_CG:
            return direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_o, "dp", "cg");
        case MC_BLOCK_XR_PD:
            return direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_o, "xr", "pd");
        case MC_BLOCK_BV_BC: {
            STAGE("bv", "bc", 0);
            STAGE("bv", "bc", 5);
            TRY(mc_registry_block_base_texture(classes, block, a->id, a->texture, a->material));
            STAGE("bv", "bc", 10);
            block->subtype.bv.a = a->pressure;
            STAGE("bv", "bc", 15);
            TRY(mc_registry_block_tick(classes, block, true));
            float edge = 0.0625f;
            float x1 = 1.0f - edge;
            float z1 = 1.0f - edge;
            STAGE("bv", "bc", 39);
            mc_registry_block_bounds(block, edge, 0.0f, edge, x1, 0.03125f, z1);
            STAGE("bv", "bc", 42);
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_BS_AZ:
            TRY(direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_e, "bs", "az"));
            if (a->flag) {
                STAGE("bs", "az", 15);
                TRY(mc_registry_block_tick(classes, block, true));
            }
            STAGE("bs", "az", 21);
            block->subtype.bs.a = a->flag;
            STAGE("bs", "az", 24);
            return MC_REGISTRY_OK;
        case MC_BLOCK_DB_BY:
            STAGE("db", "by", 0);
            STAGE("db", "by", 3);
            TRY(vm_construct(classes, block, a->id, a->texture));
            STAGE("db", "by", 8);
            block->subtype.db.a = false;
            STAGE("db", "by", 13);
            block->subtype.db.a = a->flag;
            STAGE("db", "by", 18);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("db", "by", 22);
            return MC_REGISTRY_OK;
        case MC_BLOCK_OI_IZ:
            TRY(direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_o, "oi", "iz"));
            STAGE("oi", "iz", 11);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("oi", "iz", 15);
            return MC_REGISTRY_OK;
        case MC_BLOCK_JR_GJ:
            TRY(direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_t, "jr", "gj"));
            STAGE("jr", "gj", 17);
            mc_registry_block_bounds(block, 0.0f, 0.0f, 0.0f, 1.0f, 0.125f, 1.0f);
            STAGE("jr", "gj", 22);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("jr", "gj", 26);
            return MC_REGISTRY_OK;
        case MC_BLOCK_NK_IK:
            STAGE("nk", "ik", 0);
            selected = material(classes, MC_MATERIAL_s);
            STAGE("nk", "ik", 7);
            TRY(jp_construct(classes, block, a->id, a->texture, selected, false));
            STAGE("nk", "ik", 13);
            block->fields.bB = 0.98f;
            STAGE("nk", "ik", 18);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("nk", "ik", 22);
            return MC_REGISTRY_OK;
        case MC_BLOCK_AC_P:
            TRY(direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_u, "ac", "p"));
            STAGE("ac", "p", 11);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("ac", "p", 15);
            return MC_REGISTRY_OK;
        case MC_BLOCK_OR_JE:
            TRY(direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_v, "or", "je"));
            STAGE("or", "je", 11);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("or", "je", 15);
            return MC_REGISTRY_OK;
        case MC_BLOCK_RZ_LF:
            return direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_w, "rz", "lf");
        case MC_BLOCK_RI_KO: {
            STAGE("ri", "ko", 0);
            selected = material(classes, MC_MATERIAL_j);
            STAGE("ri", "ko", 5);
            TRY(mc_registry_block_base(classes, block, a->id, selected));
            STAGE("ri", "ko", 10);
            block->fields.bm = a->texture;
            float radius = 0.375f;
            float x0 = 0.5f - radius;
            float z0 = 0.5f - radius;
            float x1 = 0.5f + radius;
            float z1 = 0.5f + radius;
            STAGE("ri", "ko", 35);
            mc_registry_block_bounds(block, x0, 0.0f, z0, x1, 1.0f, z1);
            STAGE("ri", "ko", 40);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("ri", "ko", 44);
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_FO_DT:
            STAGE("fo", "dt", 0);
            selected = material(classes, MC_MATERIAL_d);
            STAGE("fo", "dt", 6);
            TRY(rw_texture_construct(classes, block, a->id, a->texture, selected));
            STAGE("fo", "dt", 9);
            return MC_REGISTRY_OK;
        case MC_BLOCK_JW_GN:
            return direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_d, "jw", "gn");
        case MC_BLOCK_FC_DI:
            STAGE("fc", "di", 0);
            selected = material(classes, MC_MATERIAL_x);
            STAGE("fc", "di", 5);
            TRY(mc_registry_block_base(classes, block, a->id, selected));
            STAGE("fc", "di", 10);
            block->fields.bm = a->texture;
            STAGE("fc", "di", 15);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("fc", "di", 21);
            block->subtype.fc.a = a->flag;
            STAGE("fc", "di", 24);
            return MC_REGISTRY_OK;
        case MC_BLOCK_YI_PO:
            return direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_e, "yi", "po");
        case MC_BLOCK_OA_IU:
            return direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_n, "oa", "iu");
        case MC_BLOCK_LY_HR:
            STAGE("ly", "hr", 0);
            STAGE("ly", "hr", 4);
            TRY(mc_registry_block_base_texture(classes, block, a->id, a->texture, a->material));
            STAGE("ly", "hr", 7);
            return MC_REGISTRY_OK;
        case MC_BLOCK_AK_Z:
            STAGE("ak", "z", 0);
            selected = material(classes, MC_MATERIAL_y);
            STAGE("ak", "z", 7);
            TRY(jp_construct(classes, block, a->id, a->texture, selected, false));
            STAGE("ak", "z", 10);
            return MC_REGISTRY_OK;
        case MC_BLOCK_UM_MW:
            TRY(direct_texture(classes, block, a->id, a->texture, MC_MATERIAL_z, "um", "mw"));
            STAGE("um", "mw", 11);
            TRY(mc_registry_block_tick(classes, block, true));
            STAGE("um", "mw", 15);
            return MC_REGISTRY_OK;
        case MC_BLOCK_WO_OI:
            STAGE("wo", "oi", 0);
            selected = material(classes, MC_MATERIAL_o);
            STAGE("wo", "oi", 7);
            TRY(mc_registry_block_base_texture(classes, block, a->id, 6, selected));
            STAGE("wo", "oi", 12);
            block->subtype.wo.c = a->flag;
            STAGE("wo", "oi", 23);
            mc_registry_block_bounds(block, 0.0f, 0.0f, 0.0f, 1.0f, 0.125f, 1.0f);
            STAGE("wo", "oi", 26);
            return MC_REGISTRY_OK;
        case MC_BLOCK_KS_GV:
            return fixed_texture(classes, block, a->id, MC_MATERIAL_d, 26, "ks", "gv");
        case MC_BLOCK_KIND_COUNT:
            break;
    }
    return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT, "block constructor kind", block);
}

mc_registry_status mc_registry_block_construct(mc_registry_classes *classes, mc_block *block,
                                               const mc_block_arguments *arguments) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (block == NULL || arguments == NULL || block->classes != classes ||
        (unsigned int)block->kind >= (unsigned int)MC_BLOCK_KIND_COUNT)
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "block constructor arguments", block);
    if (block->phase != MC_BLOCK_RAW)
        return mc_registry_fail(classes, MC_REGISTRY_FAILED_INITIALIZATION,
                                "block constructor phase", block);
    block->requested_id = arguments->id;
    classes->current_block = block;
    block->phase = MC_BLOCK_CONSTRUCTING;
    mc_registry_status status = constructor_body(classes, block, arguments);
    block->phase = status == MC_REGISTRY_OK ? MC_BLOCK_CONSTRUCTED : MC_BLOCK_CONSTRUCTION_FAILED;
    return status;
}

#undef TRY
#undef STAGE
