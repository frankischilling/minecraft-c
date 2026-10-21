#include "mc/block/material/material.h"

#include <stddef.h>
#include <stdlib.h>

/* Preserve actual subclass dispatch independently of each instance's flags. */
typedef enum mc_material_class {
    MC_MATERIAL_CLASS_LN_HJ,
    MC_MATERIAL_CLASS_LU_HO,
    MC_MATERIAL_CLASS_SG_LK,
    MC_MATERIAL_CLASS_AQ_AC,
    MC_MATERIAL_CLASS_OU_JG
} mc_material_class;

struct mc_material_palette {
    int32_t p;
    int32_t q;
};

struct mc_material {
    mc_material_class kind;
    const mc_material_palette *C;
    bool D;
    bool E;
    bool F;
    bool G;
    int32_t H;
};

struct mc_materials {
    mc_material_palette palettes[MC_PALETTE_COUNT];
    const mc_material_palette *palette_table[MC_MATERIAL_PALETTE_SLOTS];
    mc_material materials[MC_MATERIAL_COUNT];
};

static mc_material *mc_material_m(mc_material *material) {
    material->F = true;
    return material;
}

static mc_material *mc_material_n(mc_material *material) {
    material->G = false;
    return material;
}

static mc_material *mc_material_o(mc_material *material) {
    material->D = true;
    return material;
}

static mc_material *mc_material_k(mc_material *material) {
    material->H = 1;
    return material;
}

static mc_material *mc_material_l(mc_material *material) {
    material->H = 2;
    return material;
}

static void mc_material_palette_construct(mc_materials *owner, mc_material_palette_id id,
                                          int32_t rgb) {
    mc_material_palette *palette = &owner->palettes[id];
    /* dx/cm constructor: q=index at PC6, p=RGB at PC11, a[index]=this at PC19. */
    palette->q = (int32_t)id;
    palette->p = rgb;
    owner->palette_table[id] = palette;
}

static mc_material *mc_material_construct(mc_materials *owner, mc_material_id id,
                                          mc_material_class kind, mc_material_palette_id palette) {
    mc_material *material = &owner->materials[id];
    /* Java zero defaults, then the base constructor's G=true and final C. */
    *material = (mc_material){
        .kind = kind,
        .C = &owner->palettes[palette],
        .D = false,
        .E = false,
        .F = false,
        .G = true,
        .H = 0,
    };
    if (kind == MC_MATERIAL_CLASS_LU_HO || kind == MC_MATERIAL_CLASS_SG_LK) {
        mc_material_f(material);
    }
    if (kind == MC_MATERIAL_CLASS_SG_LK) {
        mc_material_k(material);
    }
    return material;
}

bool mc_materials_create(mc_materials **out) {
    if (out == NULL) {
        return false;
    }
    mc_materials *owner = malloc(sizeof(*owner));
    if (owner == NULL) {
        return false;
    }

    for (size_t index = 0; index < MC_MATERIAL_PALETTE_SLOTS; ++index) {
        owner->palette_table[index] = NULL;
    }
    /* Complete dx/cm initializer order. Equal RGB does not merge identities. */
    mc_material_palette_construct(owner, MC_PALETTE_b, 0);
    mc_material_palette_construct(owner, MC_PALETTE_c, 8368696);
    mc_material_palette_construct(owner, MC_PALETTE_d, 16247203);
    mc_material_palette_construct(owner, MC_PALETTE_e, 10987431);
    mc_material_palette_construct(owner, MC_PALETTE_f, 16711680);
    mc_material_palette_construct(owner, MC_PALETTE_g, 10526975);
    mc_material_palette_construct(owner, MC_PALETTE_h, 10987431);
    mc_material_palette_construct(owner, MC_PALETTE_i, 31744);
    mc_material_palette_construct(owner, MC_PALETTE_j, 16777215);
    mc_material_palette_construct(owner, MC_PALETTE_k, 10791096);
    mc_material_palette_construct(owner, MC_PALETTE_l, 12020271);
    mc_material_palette_construct(owner, MC_PALETTE_m, 7368816);
    mc_material_palette_construct(owner, MC_PALETTE_n, 4210943);
    mc_material_palette_construct(owner, MC_PALETTE_o, 6837042);

    /* Complete ln/hj initializer order and fluent-call order. Subclass
     * constructors execute f and/or k inside construct before these chains. */
    mc_material_construct(owner, MC_MATERIAL_a, MC_MATERIAL_CLASS_LU_HO, MC_PALETTE_b);
    mc_material_construct(owner, MC_MATERIAL_b, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_c);
    mc_material_construct(owner, MC_MATERIAL_c, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_l);
    mc_material_o(
        mc_material_construct(owner, MC_MATERIAL_d, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_o));
    mc_material_n(
        mc_material_construct(owner, MC_MATERIAL_e, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_m));
    mc_material_n(
        mc_material_construct(owner, MC_MATERIAL_f, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_h));
    mc_material_k(
        mc_material_construct(owner, MC_MATERIAL_g, MC_MATERIAL_CLASS_SG_LK, MC_PALETTE_n));
    mc_material_k(
        mc_material_construct(owner, MC_MATERIAL_h, MC_MATERIAL_CLASS_SG_LK, MC_PALETTE_f));
    mc_material_k(mc_material_m(mc_material_o(
        mc_material_construct(owner, MC_MATERIAL_i, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_i))));
    mc_material_k(
        mc_material_construct(owner, MC_MATERIAL_j, MC_MATERIAL_CLASS_AQ_AC, MC_PALETTE_i));
    mc_material_construct(owner, MC_MATERIAL_k, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_e);
    mc_material_o(
        mc_material_construct(owner, MC_MATERIAL_l, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_e));
    mc_material_k(
        mc_material_construct(owner, MC_MATERIAL_m, MC_MATERIAL_CLASS_LU_HO, MC_PALETTE_b));
    mc_material_construct(owner, MC_MATERIAL_n, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_d);
    mc_material_k(
        mc_material_construct(owner, MC_MATERIAL_o, MC_MATERIAL_CLASS_AQ_AC, MC_PALETTE_b));
    mc_material_m(
        mc_material_construct(owner, MC_MATERIAL_p, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_b));
    mc_material_m(mc_material_o(
        mc_material_construct(owner, MC_MATERIAL_q, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_f)));
    mc_material_k(
        mc_material_construct(owner, MC_MATERIAL_r, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_i));
    mc_material_m(
        mc_material_construct(owner, MC_MATERIAL_s, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_g));
    mc_material_k(mc_material_n(mc_material_m(mc_material_f(
        mc_material_construct(owner, MC_MATERIAL_t, MC_MATERIAL_CLASS_AQ_AC, MC_PALETTE_j)))));
    mc_material_n(
        mc_material_construct(owner, MC_MATERIAL_u, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_j));
    mc_material_k(mc_material_m(
        mc_material_construct(owner, MC_MATERIAL_v, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_i)));
    mc_material_construct(owner, MC_MATERIAL_w, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_k);
    mc_material_k(
        mc_material_construct(owner, MC_MATERIAL_x, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_i));
    mc_material_l(
        mc_material_construct(owner, MC_MATERIAL_y, MC_MATERIAL_CLASS_OU_JG, MC_PALETTE_b));
    mc_material_k(
        mc_material_construct(owner, MC_MATERIAL_z, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_b));
    mc_material_k(mc_material_n(
        mc_material_construct(owner, MC_MATERIAL_A, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_e)));
    mc_material_l(
        mc_material_construct(owner, MC_MATERIAL_B, MC_MATERIAL_CLASS_LN_HJ, MC_PALETTE_m));

    *out = owner;
    return true;
}

void mc_materials_destroy(mc_materials *owner) {
    free(owner);
}

mc_material *mc_materials_get(mc_materials *owner, mc_material_id id) {
    return &owner->materials[id];
}

const mc_material_palette *mc_materials_palette(const mc_materials *owner,
                                                mc_material_palette_id id) {
    return &owner->palettes[id];
}

const mc_material_palette *const *mc_materials_palette_table(const mc_materials *owner) {
    return owner->palette_table;
}

const mc_material_palette *mc_materials_palette_at(const mc_materials *owner, int32_t index) {
    return owner->palette_table[index];
}

const mc_material_palette *mc_material_palette_of(const mc_material *material) {
    return material->C;
}

int32_t mc_material_palette_index(const mc_material_palette *palette) {
    return palette->q;
}

int32_t mc_material_palette_rgb(const mc_material_palette *palette) {
    return palette->p;
}

bool mc_material_a(const mc_material *material) {
    return material->kind == MC_MATERIAL_CLASS_LN_HJ;
}

bool mc_material_b(const mc_material *material) {
    /* sg/lk inherits the base b()=true while overriding a/c=false and d=true. */
    return material->kind == MC_MATERIAL_CLASS_LN_HJ || material->kind == MC_MATERIAL_CLASS_SG_LK;
}

bool mc_material_c(const mc_material *material) {
    return material->kind == MC_MATERIAL_CLASS_LN_HJ;
}

bool mc_material_d(const mc_material *material) {
    return material->kind == MC_MATERIAL_CLASS_SG_LK;
}

bool mc_material_e(const mc_material *material) {
    return material->D;
}

mc_material *mc_material_f(mc_material *material) {
    material->E = true;
    return material;
}

bool mc_material_g(const mc_material *material) {
    return material->E;
}

bool mc_material_h(const mc_material *material) {
    if (material->F) {
        return false;
    }
    return mc_material_c(material);
}

bool mc_material_i(const mc_material *material) {
    return material->G;
}

int32_t mc_material_j(const mc_material *material) {
    return material->H;
}
