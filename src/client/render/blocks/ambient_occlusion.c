#include "internal.h"

#include "../../../entity/player/tick_math_internal.h"
#include "mc/core/java/java.h"
#include <string.h>

/* Actual CV storage offsets, in declaration order. The immutable programs
 * below describe source calls/stores; all mutable light/color scratch remains
 * in the SAME entered CV object, never a detached mesher lighting object. */
enum {
    L_n,
    L_o,
    L_p,
    L_q,
    L_r,
    L_s,
    L_t,
    L_u,
    L_v,
    L_w,
    L_x,
    L_y,
    L_z,
    L_A,
    L_B,
    L_C,
    L_D,
    L_E,
    L_F,
    L_G,
    L_H,
    L_I,
    L_J,
    L_K,
    L_L,
    L_M,
    L_N
};

enum { F_ab, F_ac, F_ad, F_ae, F_af, F_ag, F_ah, F_ai, F_aj, F_ak, F_al, F_am };

typedef struct ao_sample {
    unsigned int slot;
    int32_t dx, dy, dz;
    int flag_a, flag_b;
    unsigned int fallback, pc;
} ao_sample;

typedef struct ao_face {
    int32_t dx, dy, dz;
    unsigned int side_pc, axial;
    ao_sample samples[8];
    unsigned int average[4][4], average_order[4], average_pc;
    float shade;
    unsigned int base_pc[3], shade_pc, multiply_pc;
    unsigned int texture_pc, reread_pc, face_pc, overlay_pc, overlay_store_pc, overlay_face_pc;
} ao_face;

/* The first four samples are unconditional edges. The remaining samples use
 * the original OR of CV flags, or copy the specified live fallback field. */
static const ao_face faces[6] = {
    {.dx = 0,
     .dy = -1,
     .dz = 0,
     .side_pc = 538,
     .axial = L_p,
     .samples = {{L_v, -1, -1, 0, -1, -1, 0, 566},
                 {L_x, 0, -1, -1, -1, -1, 0, 584},
                 {L_y, 0, -1, 1, -1, -1, 0, 602},
                 {L_A, 1, -1, 0, -1, -1, 0, 620},
                 {L_u, -1, -1, -1, F_aj, F_al, L_v, 654},
                 {L_w, -1, -1, 1, F_am, F_al, L_v, 699},
                 {L_z, 1, -1, -1, F_aj, F_ak, L_A, 744},
                 {L_B, 1, -1, 1, F_am, F_ak, L_A, 789}},
     .average =
         {{L_w, L_v, L_y, L_p}, {L_v, L_u, L_p, L_x}, {L_p, L_x, L_A, L_z}, {L_y, L_p, L_B, L_A}},
     .average_order = {0, 3, 2, 1},
     .average_pc = 817,
     .shade = 0.5f,
     .base_pc = {942, 975, 1008},
     .shade_pc = 940,
     .multiply_pc = 1030,
     .texture_pc = 1173,
     .face_pc = 1176},
    {.dx = 0,
     .dy = 1,
     .dz = 0,
     .side_pc = 1201,
     .axial = L_s,
     .samples = {{L_D, -1, 1, 0, -1, -1, 0, 1229},
                 {L_H, 1, 1, 0, -1, -1, 0, 1247},
                 {L_F, 0, 1, -1, -1, -1, 0, 1265},
                 {L_I, 0, 1, 1, -1, -1, 0, 1283},
                 {L_C, -1, 1, -1, F_ab, F_ad, L_D, 1317},
                 {L_G, 1, 1, -1, F_ab, F_ac, L_H, 1362},
                 {L_E, -1, 1, 1, F_ae, F_ad, L_D, 1407},
                 {L_J, 1, 1, 1, F_ae, F_ac, L_H, 1452}},
     .average =
         {{L_I, L_s, L_J, L_H}, {L_s, L_F, L_H, L_G}, {L_D, L_C, L_s, L_F}, {L_E, L_D, L_I, L_s}},
     .average_order = {3, 0, 1, 2},
     .average_pc = 1480,
     .shade = 1.0f,
     .base_pc = {1602, 1632, 1662},
     .shade_pc = 0,
     .multiply_pc = 1684,
     .texture_pc = 1827,
     .face_pc = 1830},
    {.dx = 0,
     .dy = 0,
     .dz = -1,
     .side_pc = 1855,
     .axial = L_q,
     .samples = {{L_K, -1, 0, -1, -1, -1, 0, 1883},
                 {L_x, 0, -1, -1, -1, -1, 0, 1901},
                 {L_F, 0, 1, -1, -1, -1, 0, 1919},
                 {L_L, 1, 0, -1, -1, -1, 0, 1937},
                 {L_u, -1, -1, -1, F_af, F_aj, L_K, 1971},
                 {L_C, -1, 1, -1, F_af, F_ab, L_K, 2016},
                 {L_z, 1, -1, -1, F_ai, F_aj, L_L, 2061},
                 {L_G, 1, 1, -1, F_ai, F_ab, L_L, 2106}},
     .average =
         {{L_K, L_C, L_q, L_F}, {L_q, L_F, L_L, L_G}, {L_x, L_q, L_z, L_L}, {L_u, L_K, L_x, L_q}},
     .average_order = {0, 1, 2, 3},
     .average_pc = 2134,
     .shade = 0.8f,
     .base_pc = {2259, 2292, 2325},
     .shade_pc = 2257,
     .multiply_pc = 2347,
     .texture_pc = 2481,
     .face_pc = 2497,
     .overlay_pc = 2500,
     .overlay_store_pc = 2527,
     .overlay_face_pc = 2662},
    {.dx = 0,
     .dy = 0,
     .dz = 1,
     .side_pc = 2687,
     .axial = L_t,
     .samples = {{L_M, -1, 0, 1, -1, -1, 0, 2715},
                 {L_N, 1, 0, 1, -1, -1, 0, 2733},
                 {L_y, 0, -1, 1, -1, -1, 0, 2751},
                 {L_I, 0, 1, 1, -1, -1, 0, 2769},
                 {L_w, -1, -1, 1, F_ah, F_am, L_M, 2803},
                 {L_E, -1, 1, 1, F_ah, F_ae, L_M, 2848},
                 {L_B, 1, -1, 1, F_ag, F_am, L_N, 2893},
                 {L_J, 1, 1, 1, F_ag, F_ae, L_N, 2938}},
     .average =
         {{L_M, L_E, L_t, L_I}, {L_w, L_M, L_y, L_t}, {L_y, L_t, L_B, L_N}, {L_t, L_I, L_N, L_J}},
     .average_order = {0, 3, 2, 1},
     .average_pc = 2966,
     .shade = 0.8f,
     .base_pc = {3091, 3124, 3157},
     .shade_pc = 3089,
     .multiply_pc = 3179,
     .texture_pc = 3313,
     .reread_pc = 3337,
     .face_pc = 3340,
     .overlay_pc = 3343,
     .overlay_store_pc = 3370,
     .overlay_face_pc = 3505},
    {.dx = -1,
     .dy = 0,
     .dz = 0,
     .side_pc = 3530,
     .axial = L_o,
     .samples = {{L_v, -1, -1, 0, -1, -1, 0, 3558},
                 {L_K, -1, 0, -1, -1, -1, 0, 3576},
                 {L_M, -1, 0, 1, -1, -1, 0, 3594},
                 {L_D, -1, 1, 0, -1, -1, 0, 3612},
                 {L_u, -1, -1, -1, F_af, F_al, L_K, 3646},
                 {L_w, -1, -1, 1, F_ah, F_al, L_M, 3691},
                 {L_C, -1, 1, -1, F_af, F_ad, L_K, 3736},
                 {L_E, -1, 1, 1, F_ah, F_ad, L_M, 3781}},
     .average =
         {{L_o, L_M, L_D, L_E}, {L_K, L_o, L_C, L_D}, {L_u, L_v, L_K, L_o}, {L_v, L_w, L_o, L_M}},
     .average_order = {3, 0, 1, 2},
     .average_pc = 3809,
     .shade = 0.6f,
     .base_pc = {3934, 3967, 4000},
     .shade_pc = 3932,
     .multiply_pc = 4022,
     .texture_pc = 4156,
     .face_pc = 4172,
     .overlay_pc = 4175,
     .overlay_store_pc = 4202,
     .overlay_face_pc = 4337},
    {.dx = 1,
     .dy = 0,
     .dz = 0,
     .side_pc = 4362,
     .axial = L_r,
     .samples = {{L_A, 1, -1, 0, -1, -1, 0, 4390},
                 {L_L, 1, 0, -1, -1, -1, 0, 4408},
                 {L_N, 1, 0, 1, -1, -1, 0, 4426},
                 {L_H, 1, 1, 0, -1, -1, 0, 4444},
                 {L_z, 1, -1, -1, F_ak, F_ai, L_L, 4478},
                 {L_B, 1, -1, 1, F_ak, F_ag, L_N, 4523},
                 {L_G, 1, 1, -1, F_ac, F_ai, L_L, 4568},
                 {L_J, 1, 1, 1, F_ac, F_ag, L_N, 4613}},
     .average =
         {{L_A, L_B, L_r, L_N}, {L_z, L_A, L_L, L_r}, {L_L, L_r, L_G, L_H}, {L_r, L_N, L_H, L_J}},
     .average_order = {0, 3, 2, 1},
     .average_pc = 4641,
     .shade = 0.6f,
     .base_pc = {4766, 4799, 4832},
     .shade_pc = 4764,
     .multiply_pc = 4854,
     .texture_pc = 4988,
     .face_pc = 5004,
     .overlay_pc = 5007,
     .overlay_store_pc = 5034,
     .overlay_face_pc = 5169}};

static void light_store(mc_cv_context *c, unsigned int slot, float value, unsigned int pc) {
    memcpy(&c->fields->n_to_N[slot], &value, sizeof(value));
    mc_cv_store(c, pc);
}

static bool sample(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                   const ao_sample *s) {
    if (s->flag_a >= 0 && !c->fields->ab_to_am[s->flag_a] && !c->fields->ab_to_am[s->flag_b]) {
        light_store(c, s->slot, c->fields->n_to_N[s->fallback], s->pc + 14u);
        return true;
    }
    float value;
    if (!mc_cv_brightness(c, block, mc_java_i32_add(x, s->dx), mc_java_i32_add(y, s->dy),
                          mc_java_i32_add(z, s->dz), s->pc, &value))
        return false;
    light_store(c, s->slot, value, s->pc + 3u);
    return true;
}

static bool averages(mc_cv_context *c, const ao_face *face, float corner[4]) {
    if (!mc_cv_fp(c, face->average_pc))
        return false;
    for (size_t step = 0; step < 4; ++step) {
        const unsigned int which = face->average_order[step];
        const unsigned int *s = face->average[which];
        const float *light = c->fields->n_to_N;
        const float first = at_fadd(light[s[0]], light[s[1]]);
        const float second = at_fadd(first, light[s[2]]);
        corner[which] = at_fdiv(at_fadd(second, light[s[3]]), 4.0f);
    }
    return true;
}

static void color_store(mc_cv_context *c, unsigned int slot, float value, unsigned int pc) {
    memcpy(&c->fields->P_to_aa[slot], &value, sizeof(value));
    mc_cv_store(c, pc);
}

static bool colors(mc_cv_context *c, const ao_face *face, bool tint, const float rgb[3],
                   const float corner[4]) {
    if (face->shade_pc != 0 && !mc_cv_fp(c, face->shade_pc))
        return false;
    for (unsigned int channel = 0; channel < 3; ++channel) {
        float value = tint ? rgb[channel] : 1.0f;
        if (face->shade_pc != 0)
            value = at_fmul(value, face->shade);
        const unsigned int first = channel * 4, pc = face->base_pc[channel];
        color_store(c, first + 3, value, pc);
        color_store(c, first + 2, value, pc + 4u);
        color_store(c, first + 1, value, pc + 8u);
        color_store(c, first, value, pc + 11u);
    }
    if (!mc_cv_fp(c, face->multiply_pc - 1u))
        return false;
    unsigned int pc = face->multiply_pc;
    for (unsigned int which = 0; which < 4; ++which)
        for (unsigned int channel = 0; channel < 3; ++channel, pc += 11u) {
            const unsigned int slot = channel * 4 + which;
            color_store(c, slot, at_fmul(c->fields->P_to_aa[slot], corner[which]), pc);
        }
    return true;
}

static bool overlay(mc_cv_context *c, const ao_face *face, const mc_block *block, int32_t side,
                    int32_t x, int32_t y, int32_t z, int32_t texture, const float rgb[3]) {
    if (face->overlay_pc == 0)
        return true;
    mc_cv_pc(c, face->overlay_pc);
    bool ready, enabled;
    if (!mc_client_render_blocks_static_get(c->models, &ready, &enabled) || !ready)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                          "actual CV overlay class namespace", face->overlay_pc, true);
    if (!enabled || texture != 3 || c->fields->d >= 0)
        return true;
    if (!mc_cv_fp(c, face->overlay_store_pc - 1u))
        return false;
    unsigned int pc = face->overlay_store_pc;
    for (unsigned int channel = 0; channel < 3; ++channel)
        for (unsigned int which = 0; which < 4; ++which, pc += 11u) {
            const unsigned int slot = channel * 4 + which;
            color_store(c, slot, at_fmul(c->fields->P_to_aa[slot], rgb[channel]), pc);
        }
    return mc_cv_face(c, side, block, (double)x, (double)y, (double)z, 38, face->overlay_face_pc);
}

bool mc_source_cv_ambient_occlusion(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y,
                                    int32_t z, float red, float green, float blue, bool *out) {
    c->fields->m = true;
    mc_cv_store(c, 2);
    bool rendered = false;
    /* Source local f5..f8 each read the old n before the fresh center store63. */
    float corner[4];
    for (size_t i = 0; i < 4; ++i)
        memcpy(&corner[i], &c->fields->n_to_N[L_n], sizeof(corner[i]));
    static const ao_sample initial[7] = {
        {L_n, 0, 0, 0, -1, -1, 0, 60},  {L_o, -1, 0, 0, -1, -1, 0, 78},
        {L_p, 0, -1, 0, -1, -1, 0, 96}, {L_q, 0, 0, -1, -1, -1, 0, 114},
        {L_r, 1, 0, 0, -1, -1, 0, 132}, {L_s, 0, 1, 0, -1, -1, 0, 150},
        {L_t, 0, 0, 1, -1, -1, 0, 168}};
    for (size_t i = 0; i < 7; ++i)
        if (!sample(c, block, x, y, z, &initial[i]))
            return false;

    static const struct {
        unsigned int slot;
        int32_t dx, dy, dz;
    } flags[12] = {{F_ac, 1, 1, 0},  {F_ak, 1, -1, 0},  {F_ag, 1, 0, 1},   {F_ai, 1, 0, -1},
                   {F_ad, -1, 1, 0}, {F_al, -1, -1, 0}, {F_af, -1, 0, -1}, {F_ah, -1, 0, 1},
                   {F_ae, 0, 1, 1},  {F_ab, 0, 1, -1},  {F_am, 0, -1, 1},  {F_aj, 0, -1, -1}};

    for (unsigned int i = 0; i < 12; ++i) {
        bool value;
        const unsigned int pc = 175u + i * 25u;
        if (!mc_cv_ao_flag(c, mc_java_i32_add(x, flags[i].dx), mc_java_i32_add(y, flags[i].dy),
                           mc_java_i32_add(z, flags[i].dz), pc, pc + 15u, pc + 20u, &value))
            return false;
        c->fields->ab_to_am[flags[i].slot] = value;
        mc_cv_store(c, pc + 21u);
    }
    const mc_block_fields *fields;
    if (!mc_cv_fields(c, block, 475, &fields))
        return false;
    const bool tinted = fields->bm != 3 && c->fields->d < 0;
    const float rgb[3] = {red, green, blue};
    for (int32_t side = 0; side < 6; ++side) {
        const ao_face *face = &faces[side];
        bool visible = c->fields->f;
        if (!visible &&
            !mc_cv_side(c, block, mc_java_i32_add(x, face->dx), mc_java_i32_add(y, face->dy),
                        mc_java_i32_add(z, face->dz), side, face->side_pc, &visible))
            return false;
        if (!visible)
            continue;
        if (c->fields->O > 0) {
            for (size_t i = 0; i < 8; ++i)
                if (!sample(c, block, x, y, z, &face->samples[i]))
                    return false;
            if (!averages(c, face, corner))
                return false;
        } else {
            corner[3] = c->fields->n_to_N[face->axial];
            corner[2] = corner[3];
            corner[1] = corner[3];
            corner[0] = corner[3];
        }
        if (!colors(c, face, side == 1 || tinted, rgb, corner))
            return false;
        int32_t texture;
        if (!mc_cv_texture(c, block, x, y, z, side, face->texture_pc, &texture))
            return false;
        int32_t base_texture = texture;
        /* South really evaluates the same virtual twice, and preserves the
         * first result for its later grass-overlay condition. */
        if (face->reread_pc != 0 &&
            !mc_cv_texture(c, block, x, y, z, side, face->reread_pc, &base_texture))
            return false;
        if (!mc_cv_face(c, side, block, (double)x, (double)y, (double)z, base_texture,
                        face->face_pc) ||
            !overlay(c, face, block, side, x, y, z, texture, rgb))
            return false;
        rendered = true;
    }
    c->fields->m = false;
    mc_cv_store(c, 5177);
    mc_cv_pc(c, 5182);
    *out = rendered;
    return true;
}
