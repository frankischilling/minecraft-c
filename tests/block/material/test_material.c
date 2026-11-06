#include "mc/block/material/material.h"

#include <stddef.h>
#include <stdio.h>

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        bool check_passed = (condition);                                                           \
        if (!check_passed) {                                                                       \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct material_case {
    mc_material_id id;
    mc_material_palette_id palette;
    bool a, b, c, d, e, g, h, i;
    int32_t j;
} material_case;

/* Literal successful-initializer results from paired ln/hj and their four
 * actual subclasses. Columns are a,b,c,d,e,g,h,i,j, not inferred block traits.
 * Original class/bytecode locators are in docs/reference/materials.md. */
static const material_case material_cases[] = {
    {MC_MATERIAL_a, MC_PALETTE_b, false, false, false, false, false, true, false, true, 0},
    {MC_MATERIAL_b, MC_PALETTE_c, true, true, true, false, false, false, true, true, 0},
    {MC_MATERIAL_c, MC_PALETTE_l, true, true, true, false, false, false, true, true, 0},
    {MC_MATERIAL_d, MC_PALETTE_o, true, true, true, false, true, false, true, true, 0},
    {MC_MATERIAL_e, MC_PALETTE_m, true, true, true, false, false, false, true, false, 0},
    {MC_MATERIAL_f, MC_PALETTE_h, true, true, true, false, false, false, true, false, 0},
    {MC_MATERIAL_g, MC_PALETTE_n, false, true, false, true, false, true, false, true, 1},
    {MC_MATERIAL_h, MC_PALETTE_f, false, true, false, true, false, true, false, true, 1},
    {MC_MATERIAL_i, MC_PALETTE_i, true, true, true, false, true, false, false, true, 1},
    {MC_MATERIAL_j, MC_PALETTE_i, false, false, false, false, false, false, false, true, 1},
    {MC_MATERIAL_k, MC_PALETTE_e, true, true, true, false, false, false, true, true, 0},
    {MC_MATERIAL_l, MC_PALETTE_e, true, true, true, false, true, false, true, true, 0},
    {MC_MATERIAL_m, MC_PALETTE_b, false, false, false, false, false, true, false, true, 1},
    {MC_MATERIAL_n, MC_PALETTE_d, true, true, true, false, false, false, true, true, 0},
    {MC_MATERIAL_o, MC_PALETTE_b, false, false, false, false, false, false, false, true, 1},
    {MC_MATERIAL_p, MC_PALETTE_b, true, true, true, false, false, false, false, true, 0},
    {MC_MATERIAL_q, MC_PALETTE_f, true, true, true, false, true, false, false, true, 0},
    {MC_MATERIAL_r, MC_PALETTE_i, true, true, true, false, false, false, true, true, 1},
    {MC_MATERIAL_s, MC_PALETTE_g, true, true, true, false, false, false, false, true, 0},
    {MC_MATERIAL_t, MC_PALETTE_j, false, false, false, false, false, true, false, false, 1},
    {MC_MATERIAL_u, MC_PALETTE_j, true, true, true, false, false, false, true, false, 0},
    {MC_MATERIAL_v, MC_PALETTE_i, true, true, true, false, false, false, false, true, 1},
    {MC_MATERIAL_w, MC_PALETTE_k, true, true, true, false, false, false, true, true, 0},
    {MC_MATERIAL_x, MC_PALETTE_i, true, true, true, false, false, false, true, true, 1},
    {MC_MATERIAL_y, MC_PALETTE_b, false, false, false, false, false, false, false, true, 2},
    {MC_MATERIAL_z, MC_PALETTE_b, true, true, true, false, false, false, true, true, 1},
    {MC_MATERIAL_A, MC_PALETTE_e, true, true, true, false, false, false, true, false, 1},
    {MC_MATERIAL_B, MC_PALETTE_m, true, true, true, false, false, false, true, true, 2},
};

typedef struct palette_case {
    mc_material_palette_id id;
    int32_t index;
    int32_t rgb;
} palette_case;

/* Actual dx/cm constructor arguments, b..o. e and h deliberately share RGB. */
static const palette_case palette_cases[] = {
    {MC_PALETTE_b, 0, 0},        {MC_PALETTE_c, 1, 8368696},   {MC_PALETTE_d, 2, 16247203},
    {MC_PALETTE_e, 3, 10987431}, {MC_PALETTE_f, 4, 16711680},  {MC_PALETTE_g, 5, 10526975},
    {MC_PALETTE_h, 6, 10987431}, {MC_PALETTE_i, 7, 31744},     {MC_PALETTE_j, 8, 16777215},
    {MC_PALETTE_k, 9, 10791096}, {MC_PALETTE_l, 10, 12020271}, {MC_PALETTE_m, 11, 7368816},
    {MC_PALETTE_n, 12, 4210943}, {MC_PALETTE_o, 13, 6837042},
};

#define MATERIAL_CASES (sizeof(material_cases) / sizeof(material_cases[0]))
#define PALETTE_CASES (sizeof(palette_cases) / sizeof(palette_cases[0]))

_Static_assert(MATERIAL_CASES == 28, "all original material fields required");
_Static_assert(PALETTE_CASES == 14, "all original palette fields required");

static int check_material(const mc_material *material, const material_case *expected,
                          bool expected_g, const mc_material_palette *palette) {
    int result = 1;
    CHECK(material != NULL);
    CHECK(mc_material_a(material) == expected->a);
    CHECK(mc_material_b(material) == expected->b);
    CHECK(mc_material_c(material) == expected->c);
    CHECK(mc_material_d(material) == expected->d);
    CHECK(mc_material_e(material) == expected->e);
    CHECK(mc_material_g(material) == expected_g);
    CHECK(mc_material_h(material) == expected->h);
    CHECK(mc_material_i(material) == expected->i);
    CHECK(mc_material_j(material) == expected->j);
    CHECK(mc_material_palette_of(material) == palette);
    result = 0;
cleanup:
    return result;
}

static int check_palette(const mc_material_palette *palette, const palette_case *expected) {
    int result = 1;
    CHECK(palette != NULL);
    CHECK(mc_material_palette_index(palette) == expected->index);
    CHECK(mc_material_palette_rgb(palette) == expected->rgb);
    result = 0;
cleanup:
    return result;
}

static int test_material_owners(void) {
    int result = 1;
    mc_materials *owner = NULL, *other = NULL;
    mc_material *materials[MATERIAL_CASES];
    mc_material *other_materials[MATERIAL_CASES];
    const mc_material_palette *palettes[PALETTE_CASES];
    const mc_material_palette *other_palettes[PALETTE_CASES];
    const mc_material_palette *const *table = NULL;
    const mc_material_palette *const *other_table = NULL;
    bool changed[MATERIAL_CASES] = {false};

    CHECK(MC_MATERIAL_COUNT == 28);
    CHECK(MC_PALETTE_COUNT == 14);
    CHECK(MC_MATERIAL_PALETTE_SLOTS == 16u);
    CHECK(!mc_materials_create(NULL));
    mc_materials_destroy(NULL);
    CHECK(mc_materials_create(&owner));
    CHECK(owner != NULL);
    CHECK(mc_materials_create(&other));
    CHECK(other != NULL && other != owner);

    table = mc_materials_palette_table(owner);
    other_table = mc_materials_palette_table(other);
    CHECK(table != NULL && other_table != NULL && table != other_table);
    CHECK(mc_materials_palette_table(owner) == table);
    CHECK(mc_materials_palette_table(other) == other_table);
    for (size_t i = 0; i < PALETTE_CASES; ++i) {
        const palette_case *expected = &palette_cases[i];
        palettes[i] = mc_materials_palette(owner, expected->id);
        CHECK(check_palette(palettes[i], expected) == 0);
        CHECK(mc_materials_palette(owner, expected->id) == palettes[i]);
        CHECK(table[expected->index] == palettes[i]);
        CHECK(mc_materials_palette_at(owner, expected->index) == palettes[i]);
        other_palettes[i] = mc_materials_palette(other, expected->id);
        CHECK(check_palette(other_palettes[i], expected) == 0);
        CHECK(mc_materials_palette(other, expected->id) == other_palettes[i]);
        CHECK(other_table[expected->index] == other_palettes[i]);
        CHECK(mc_materials_palette_at(other, expected->index) == other_table[expected->index]);
        for (size_t j = 0; j < i; ++j) {
            CHECK(palettes[i] != palettes[j]);
            CHECK(other_palettes[i] != other_palettes[j]);
        }
        for (size_t j = 0; j < PALETTE_CASES; ++j) {
            CHECK(palettes[i] != mc_materials_palette(other, palette_cases[j].id));
        }
    }
    CHECK(table[14] == NULL && table[15] == NULL);
    CHECK(other_table[14] == NULL && other_table[15] == NULL);
    CHECK(mc_materials_palette_at(owner, 14) == NULL);
    CHECK(mc_materials_palette_at(owner, 15) == NULL);
    CHECK(mc_materials_palette_at(other, 14) == NULL);
    CHECK(mc_materials_palette_at(other, 15) == NULL);
    CHECK(mc_materials_palette(owner, MC_PALETTE_e) != mc_materials_palette(owner, MC_PALETTE_h));
    CHECK(mc_material_palette_rgb(mc_materials_palette(owner, MC_PALETTE_e)) == 10987431);
    CHECK(mc_material_palette_rgb(mc_materials_palette(owner, MC_PALETTE_h)) == 10987431);

    for (size_t i = 0; i < MATERIAL_CASES; ++i) {
        const material_case *expected = &material_cases[i];
        const mc_material_palette *palette = mc_materials_palette(owner, expected->palette);
        materials[i] = mc_materials_get(owner, expected->id);
        CHECK(check_material(materials[i], expected, expected->g, palette) == 0);
        CHECK(mc_materials_get(owner, expected->id) == materials[i]);
        other_materials[i] = mc_materials_get(other, expected->id);
        CHECK(check_material(other_materials[i], expected, expected->g,
                             mc_materials_palette(other, expected->palette)) == 0);
        CHECK(mc_materials_get(other, expected->id) == other_materials[i]);
        for (size_t j = 0; j < i; ++j) {
            CHECK(materials[i] != materials[j]);
            CHECK(other_materials[i] != other_materials[j]);
        }
        for (size_t j = 0; j < MATERIAL_CASES; ++j) {
            CHECK(materials[i] != mc_materials_get(other, material_cases[j].id));
        }
    }

    /* Aliases stand for different borrowers of ONE owner, not new world sets.
     * f sets only E; compare all other methods/identities with literal rows. */
    for (size_t i = 0; i < MATERIAL_CASES; ++i) {
        mc_material *first_alias = mc_materials_get(owner, material_cases[i].id);
        mc_material *second_alias = mc_materials_get(owner, material_cases[i].id);
        CHECK(first_alias == materials[i] && second_alias == first_alias);
        CHECK(mc_material_f(first_alias) == first_alias);
        changed[i] = true;
        CHECK(mc_material_g(second_alias));
        CHECK(mc_material_f(second_alias) == first_alias);
        for (size_t j = 0; j < MATERIAL_CASES; ++j) {
            const material_case *expected = &material_cases[j];
            CHECK(mc_materials_get(owner, expected->id) == materials[j]);
            CHECK(mc_materials_get(other, expected->id) == other_materials[j]);
            CHECK(check_material(materials[j], expected, changed[j] || expected->g,
                                 mc_materials_palette(owner, expected->palette)) == 0);
            CHECK(check_material(mc_materials_get(other, expected->id), expected, expected->g,
                                 mc_materials_palette(other, expected->palette)) == 0);
        }
        CHECK(mc_materials_palette_table(owner) == table);
        CHECK(mc_materials_palette_table(other) == other_table);
        for (size_t j = 0; j < PALETTE_CASES; ++j) {
            const palette_case *expected = &palette_cases[j];
            CHECK(mc_materials_palette(owner, expected->id) == palettes[j]);
            CHECK(table[expected->index] == palettes[j]);
            CHECK(check_palette(palettes[j], expected) == 0);
            CHECK(mc_materials_palette(other, expected->id) == other_palettes[j]);
            CHECK(other_table[expected->index] == other_palettes[j]);
            CHECK(check_palette(other_table[expected->index], expected) == 0);
        }
        CHECK(table[14] == NULL && table[15] == NULL);
        CHECK(other_table[14] == NULL && other_table[15] == NULL);
    }

    /* An independent owner remains valid after the first owner's retirement.
     * No retired material, palette or table view is dereferenced afterward. */
    mc_materials_destroy(owner);
    owner = NULL;
    for (size_t i = 0; i < MATERIAL_CASES; ++i) {
        const material_case *expected = &material_cases[i];
        CHECK(mc_materials_get(other, expected->id) == other_materials[i]);
        CHECK(check_material(mc_materials_get(other, expected->id), expected, expected->g,
                             mc_materials_palette(other, expected->palette)) == 0);
    }
    for (size_t i = 0; i < PALETTE_CASES; ++i) {
        const palette_case *expected = &palette_cases[i];
        CHECK(mc_materials_palette(other, expected->id) == other_palettes[i]);
        CHECK(mc_materials_palette_at(other, expected->index) == other_table[expected->index]);
        CHECK(check_palette(other_table[expected->index], expected) == 0);
    }
    CHECK(other_table[14] == NULL && other_table[15] == NULL);
    result = 0;
cleanup:
    if (other != owner) {
        mc_materials_destroy(other);
    }
    mc_materials_destroy(owner);
    return result;
}

int main(void) {
    if (test_material_owners() != 0) {
        return 1;
    }
    printf("PASS: %u material checks\n", checks);
    return 0;
}
