#include "../../../entity/player/tick_math_internal.h"
#include "internal.h"

/* Six actual cv face methods. The enclosing wrapper owns the CV frame; each
 * body captures the real NW singleton and uses live canonical Block bounds. */

static bool face_a(mc_cv_context *c, const mc_block *block, double d2, double d3, double d4,
                   int32_t n2) {
    mc_client_render_blocks *f = c->fields;
    if (!mc_cv_nw(c, 0))
        return false;
    if (f->d >= 0) {
        n2 = f->d;
    }
    int32_t n3 = (n2 & 0xF) << 4;
    int32_t n4 = n2 & 0xF0;
    const mc_block_fields *b = NULL;
    if (!mc_cv_fields(c, block, 39, &b) || !mc_cv_fp(c, 45))
        return false;
    double d5 = at_div(at_add((double)n3, at_mul(b->bs, 16.0)), 256.0);
    double d6 = at_div(at_sub(at_add((double)n3, at_mul(b->bv, 16.0)), 0.01), 256.0);
    double d7 = at_div(at_add((double)n4, at_mul(b->bu, 16.0)), 256.0);
    double d8 = at_div(at_sub(at_add((double)n4, at_mul(b->bx, 16.0)), 0.01), 256.0);
    if (b->bs < 0.0 || b->bv > 1.0) {
        d5 = at_widen(at_fdiv(at_fadd((float)n3, 0.0f), 256.0f));
        d6 = at_widen(at_fdiv(at_fadd((float)n3, 15.99f), 256.0f));
    }
    if (b->bu < 0.0 || b->bx > 1.0) {
        d7 = at_widen(at_fdiv(at_fadd((float)n4, 0.0f), 256.0f));
        d8 = at_widen(at_fdiv(at_fadd((float)n4, 15.99f), 256.0f));
    }
    double d9 = d6;
    double d10 = d5;
    double d11 = d7;
    double d12 = d8;
    if (f->l == 2) {
        d5 = at_div(at_add((double)n3, at_mul(b->bu, 16.0)), 256.0);
        d7 = at_div(at_sub((double)(n4 + 16), at_mul(b->bv, 16.0)), 256.0);
        d6 = at_div(at_add((double)n3, at_mul(b->bx, 16.0)), 256.0);
        d8 = at_div(at_sub((double)(n4 + 16), at_mul(b->bs, 16.0)), 256.0);
        d9 = d6;
        d10 = d5;
        d11 = d7;
        d12 = d8;
        d9 = d5;
        d10 = d6;
        d7 = d8;
        d8 = d11;
    } else if (f->l == 1) {
        d5 = at_div(at_sub((double)(n3 + 16), at_mul(b->bx, 16.0)), 256.0);
        d7 = at_div(at_add((double)n4, at_mul(b->bs, 16.0)), 256.0);
        d6 = at_div(at_sub((double)(n3 + 16), at_mul(b->bu, 16.0)), 256.0);
        d8 = at_div(at_add((double)n4, at_mul(b->bv, 16.0)), 256.0);
        d9 = d6;
        d10 = d5;
        d11 = d7;
        d12 = d8;
        d5 = d9;
        d6 = d10;
        d11 = d8;
        d12 = d7;
    } else if (f->l == 3) {
        d5 = at_div(at_sub((double)(n3 + 16), at_mul(b->bs, 16.0)), 256.0);
        d6 = at_div(at_sub(at_sub((double)(n3 + 16), at_mul(b->bv, 16.0)), 0.01), 256.0);
        d7 = at_div(at_sub((double)(n4 + 16), at_mul(b->bu, 16.0)), 256.0);
        d8 = at_div(at_sub(at_sub((double)(n4 + 16), at_mul(b->bx, 16.0)), 0.01), 256.0);
        d9 = d6;
        d10 = d5;
        d11 = d7;
        d12 = d8;
    }
    double d13 = at_add(d2, b->bs);
    double d14 = at_add(d2, b->bv);
    double d15 = at_add(d3, b->bt);
    double d16 = at_add(d4, b->bu);
    double d17 = at_add(d4, b->bx);
    if (f->m) {
        if (!mc_cv_color(c, f->P_to_aa[0], f->P_to_aa[4], f->P_to_aa[8], 635))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d17, d10, d12, 650))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[1], f->P_to_aa[5], f->P_to_aa[9], 667))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d16, d5, d7, 682))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[2], f->P_to_aa[6], f->P_to_aa[10], 699))
            return false;
        if (!mc_cv_vertex(c, d14, d15, d16, d9, d11, 714))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[3], f->P_to_aa[7], f->P_to_aa[11], 731))
            return false;
        if (!mc_cv_vertex(c, d14, d15, d17, d6, d8, 746))
            return false;
    } else {
        if (!mc_cv_vertex(c, d13, d15, d17, d10, d12, 764))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d16, d5, d7, 779))
            return false;
        if (!mc_cv_vertex(c, d14, d15, d16, d9, d11, 794))
            return false;
        if (!mc_cv_vertex(c, d14, d15, d17, d6, d8, 809))
            return false;
    }
    return true;
}

static bool face_b(mc_cv_context *c, const mc_block *block, double d2, double d3, double d4,
                   int32_t n2) {
    mc_client_render_blocks *f = c->fields;
    if (!mc_cv_nw(c, 0))
        return false;
    if (f->d >= 0) {
        n2 = f->d;
    }
    int32_t n3 = (n2 & 0xF) << 4;
    int32_t n4 = n2 & 0xF0;
    const mc_block_fields *b = NULL;
    if (!mc_cv_fields(c, block, 39, &b) || !mc_cv_fp(c, 45))
        return false;
    double d5 = at_div(at_add((double)n3, at_mul(b->bs, 16.0)), 256.0);
    double d6 = at_div(at_sub(at_add((double)n3, at_mul(b->bv, 16.0)), 0.01), 256.0);
    double d7 = at_div(at_add((double)n4, at_mul(b->bu, 16.0)), 256.0);
    double d8 = at_div(at_sub(at_add((double)n4, at_mul(b->bx, 16.0)), 0.01), 256.0);
    if (b->bs < 0.0 || b->bv > 1.0) {
        d5 = at_widen(at_fdiv(at_fadd((float)n3, 0.0f), 256.0f));
        d6 = at_widen(at_fdiv(at_fadd((float)n3, 15.99f), 256.0f));
    }
    if (b->bu < 0.0 || b->bx > 1.0) {
        d7 = at_widen(at_fdiv(at_fadd((float)n4, 0.0f), 256.0f));
        d8 = at_widen(at_fdiv(at_fadd((float)n4, 15.99f), 256.0f));
    }
    double d9 = d6;
    double d10 = d5;
    double d11 = d7;
    double d12 = d8;
    if (f->k == 1) {
        d5 = at_div(at_add((double)n3, at_mul(b->bu, 16.0)), 256.0);
        d7 = at_div(at_sub((double)(n4 + 16), at_mul(b->bv, 16.0)), 256.0);
        d6 = at_div(at_add((double)n3, at_mul(b->bx, 16.0)), 256.0);
        d8 = at_div(at_sub((double)(n4 + 16), at_mul(b->bs, 16.0)), 256.0);
        d9 = d6;
        d10 = d5;
        d11 = d7;
        d12 = d8;
        d9 = d5;
        d10 = d6;
        d7 = d8;
        d8 = d11;
    } else if (f->k == 2) {
        d5 = at_div(at_sub((double)(n3 + 16), at_mul(b->bx, 16.0)), 256.0);
        d7 = at_div(at_add((double)n4, at_mul(b->bs, 16.0)), 256.0);
        d6 = at_div(at_sub((double)(n3 + 16), at_mul(b->bu, 16.0)), 256.0);
        d8 = at_div(at_add((double)n4, at_mul(b->bv, 16.0)), 256.0);
        d9 = d6;
        d10 = d5;
        d11 = d7;
        d12 = d8;
        d5 = d9;
        d6 = d10;
        d11 = d8;
        d12 = d7;
    } else if (f->k == 3) {
        d5 = at_div(at_sub((double)(n3 + 16), at_mul(b->bs, 16.0)), 256.0);
        d6 = at_div(at_sub(at_sub((double)(n3 + 16), at_mul(b->bv, 16.0)), 0.01), 256.0);
        d7 = at_div(at_sub((double)(n4 + 16), at_mul(b->bu, 16.0)), 256.0);
        d8 = at_div(at_sub(at_sub((double)(n4 + 16), at_mul(b->bx, 16.0)), 0.01), 256.0);
        d9 = d6;
        d10 = d5;
        d11 = d7;
        d12 = d8;
    }
    double d13 = at_add(d2, b->bs);
    double d14 = at_add(d2, b->bv);
    double d15 = at_add(d3, b->bw);
    double d16 = at_add(d4, b->bu);
    double d17 = at_add(d4, b->bx);
    if (f->m) {
        if (!mc_cv_color(c, f->P_to_aa[0], f->P_to_aa[4], f->P_to_aa[8], 635))
            return false;
        if (!mc_cv_vertex(c, d14, d15, d17, d6, d8, 650))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[1], f->P_to_aa[5], f->P_to_aa[9], 667))
            return false;
        if (!mc_cv_vertex(c, d14, d15, d16, d9, d11, 682))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[2], f->P_to_aa[6], f->P_to_aa[10], 699))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d16, d5, d7, 714))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[3], f->P_to_aa[7], f->P_to_aa[11], 731))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d17, d10, d12, 746))
            return false;
    } else {
        if (!mc_cv_vertex(c, d14, d15, d17, d6, d8, 764))
            return false;
        if (!mc_cv_vertex(c, d14, d15, d16, d9, d11, 779))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d16, d5, d7, 794))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d17, d10, d12, 809))
            return false;
    }
    return true;
}

static bool face_c(mc_cv_context *c, const mc_block *block, double d2, double d3, double d4,
                   int32_t n2) {
    mc_client_render_blocks *f = c->fields;
    double d5;
    if (!mc_cv_nw(c, 0))
        return false;
    if (f->d >= 0) {
        n2 = f->d;
    }
    int32_t n3 = (n2 & 0xF) << 4;
    int32_t n4 = n2 & 0xF0;
    const mc_block_fields *b = NULL;
    if (!mc_cv_fields(c, block, 39, &b) || !mc_cv_fp(c, 45))
        return false;
    double d6 = at_div(at_add((double)n3, at_mul(b->bs, 16.0)), 256.0);
    double d7 = at_div(at_sub(at_add((double)n3, at_mul(b->bv, 16.0)), 0.01), 256.0);
    double d8 = at_div(at_sub((double)(n4 + 16), at_mul(b->bw, 16.0)), 256.0);
    double d9 = at_div(at_sub(at_sub((double)(n4 + 16), at_mul(b->bt, 16.0)), 0.01), 256.0);
    if (f->e) {
        d5 = d6;
        d6 = d7;
        d7 = d5;
    }
    if (b->bs < 0.0 || b->bv > 1.0) {
        d6 = at_widen(at_fdiv(at_fadd((float)n3, 0.0f), 256.0f));
        d7 = at_widen(at_fdiv(at_fadd((float)n3, 15.99f), 256.0f));
    }
    if (b->bt < 0.0 || b->bw > 1.0) {
        d8 = at_widen(at_fdiv(at_fadd((float)n4, 0.0f), 256.0f));
        d9 = at_widen(at_fdiv(at_fadd((float)n4, 15.99f), 256.0f));
    }
    d5 = d7;
    double d10 = d6;
    double d11 = d8;
    double d12 = d9;
    if (f->g == 2) {
        d6 = at_div(at_add((double)n3, at_mul(b->bt, 16.0)), 256.0);
        d8 = at_div(at_sub((double)(n4 + 16), at_mul(b->bs, 16.0)), 256.0);
        d7 = at_div(at_add((double)n3, at_mul(b->bw, 16.0)), 256.0);
        d9 = at_div(at_sub((double)(n4 + 16), at_mul(b->bv, 16.0)), 256.0);
        d5 = d7;
        d10 = d6;
        d11 = d8;
        d12 = d9;
        d5 = d6;
        d10 = d7;
        d8 = d9;
        d9 = d11;
    } else if (f->g == 1) {
        d6 = at_div(at_sub((double)(n3 + 16), at_mul(b->bw, 16.0)), 256.0);
        d8 = at_div(at_add((double)n4, at_mul(b->bv, 16.0)), 256.0);
        d7 = at_div(at_sub((double)(n3 + 16), at_mul(b->bt, 16.0)), 256.0);
        d9 = at_div(at_add((double)n4, at_mul(b->bs, 16.0)), 256.0);
        d5 = d7;
        d10 = d6;
        d11 = d8;
        d12 = d9;
        d6 = d5;
        d7 = d10;
        d11 = d9;
        d12 = d8;
    } else if (f->g == 3) {
        d6 = at_div(at_sub((double)(n3 + 16), at_mul(b->bs, 16.0)), 256.0);
        d7 = at_div(at_sub(at_sub((double)(n3 + 16), at_mul(b->bv, 16.0)), 0.01), 256.0);
        d8 = at_div(at_add((double)n4, at_mul(b->bw, 16.0)), 256.0);
        d9 = at_div(at_sub(at_add((double)n4, at_mul(b->bt, 16.0)), 0.01), 256.0);
        d5 = d7;
        d10 = d6;
        d11 = d8;
        d12 = d9;
    }
    double d13 = at_add(d2, b->bs);
    double d14 = at_add(d2, b->bv);
    double d15 = at_add(d3, b->bt);
    double d16 = at_add(d3, b->bw);
    double d17 = at_add(d4, b->bu);
    if (f->m) {
        if (!mc_cv_color(c, f->P_to_aa[0], f->P_to_aa[4], f->P_to_aa[8], 654))
            return false;
        if (!mc_cv_vertex(c, d13, d16, d17, d5, d11, 669))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[1], f->P_to_aa[5], f->P_to_aa[9], 686))
            return false;
        if (!mc_cv_vertex(c, d14, d16, d17, d6, d8, 701))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[2], f->P_to_aa[6], f->P_to_aa[10], 718))
            return false;
        if (!mc_cv_vertex(c, d14, d15, d17, d10, d12, 733))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[3], f->P_to_aa[7], f->P_to_aa[11], 750))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d17, d7, d9, 765))
            return false;
    } else {
        if (!mc_cv_vertex(c, d13, d16, d17, d5, d11, 783))
            return false;
        if (!mc_cv_vertex(c, d14, d16, d17, d6, d8, 798))
            return false;
        if (!mc_cv_vertex(c, d14, d15, d17, d10, d12, 813))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d17, d7, d9, 828))
            return false;
    }
    return true;
}

static bool face_d(mc_cv_context *c, const mc_block *block, double d2, double d3, double d4,
                   int32_t n2) {
    mc_client_render_blocks *f = c->fields;
    double d5;
    if (!mc_cv_nw(c, 0))
        return false;
    if (f->d >= 0) {
        n2 = f->d;
    }
    int32_t n3 = (n2 & 0xF) << 4;
    int32_t n4 = n2 & 0xF0;
    const mc_block_fields *b = NULL;
    if (!mc_cv_fields(c, block, 39, &b) || !mc_cv_fp(c, 45))
        return false;
    double d6 = at_div(at_add((double)n3, at_mul(b->bs, 16.0)), 256.0);
    double d7 = at_div(at_sub(at_add((double)n3, at_mul(b->bv, 16.0)), 0.01), 256.0);
    double d8 = at_div(at_sub((double)(n4 + 16), at_mul(b->bw, 16.0)), 256.0);
    double d9 = at_div(at_sub(at_sub((double)(n4 + 16), at_mul(b->bt, 16.0)), 0.01), 256.0);
    if (f->e) {
        d5 = d6;
        d6 = d7;
        d7 = d5;
    }
    if (b->bs < 0.0 || b->bv > 1.0) {
        d6 = at_widen(at_fdiv(at_fadd((float)n3, 0.0f), 256.0f));
        d7 = at_widen(at_fdiv(at_fadd((float)n3, 15.99f), 256.0f));
    }
    if (b->bt < 0.0 || b->bw > 1.0) {
        d8 = at_widen(at_fdiv(at_fadd((float)n4, 0.0f), 256.0f));
        d9 = at_widen(at_fdiv(at_fadd((float)n4, 15.99f), 256.0f));
    }
    d5 = d7;
    double d10 = d6;
    double d11 = d8;
    double d12 = d9;
    if (f->h == 1) {
        d6 = at_div(at_add((double)n3, at_mul(b->bt, 16.0)), 256.0);
        d9 = at_div(at_sub((double)(n4 + 16), at_mul(b->bs, 16.0)), 256.0);
        d7 = at_div(at_add((double)n3, at_mul(b->bw, 16.0)), 256.0);
        d8 = at_div(at_sub((double)(n4 + 16), at_mul(b->bv, 16.0)), 256.0);
        d5 = d7;
        d10 = d6;
        d11 = d8;
        d12 = d9;
        d5 = d6;
        d10 = d7;
        d8 = d9;
        d9 = d11;
    } else if (f->h == 2) {
        d6 = at_div(at_sub((double)(n3 + 16), at_mul(b->bw, 16.0)), 256.0);
        d8 = at_div(at_add((double)n4, at_mul(b->bs, 16.0)), 256.0);
        d7 = at_div(at_sub((double)(n3 + 16), at_mul(b->bt, 16.0)), 256.0);
        d9 = at_div(at_add((double)n4, at_mul(b->bv, 16.0)), 256.0);
        d5 = d7;
        d10 = d6;
        d11 = d8;
        d12 = d9;
        d6 = d5;
        d7 = d10;
        d11 = d9;
        d12 = d8;
    } else if (f->h == 3) {
        d6 = at_div(at_sub((double)(n3 + 16), at_mul(b->bs, 16.0)), 256.0);
        d7 = at_div(at_sub(at_sub((double)(n3 + 16), at_mul(b->bv, 16.0)), 0.01), 256.0);
        d8 = at_div(at_add((double)n4, at_mul(b->bw, 16.0)), 256.0);
        d9 = at_div(at_sub(at_add((double)n4, at_mul(b->bt, 16.0)), 0.01), 256.0);
        d5 = d7;
        d10 = d6;
        d11 = d8;
        d12 = d9;
    }
    double d13 = at_add(d2, b->bs);
    double d14 = at_add(d2, b->bv);
    double d15 = at_add(d3, b->bt);
    double d16 = at_add(d3, b->bw);
    double d17 = at_add(d4, b->bx);
    if (f->m) {
        if (!mc_cv_color(c, f->P_to_aa[0], f->P_to_aa[4], f->P_to_aa[8], 654))
            return false;
        if (!mc_cv_vertex(c, d13, d16, d17, d6, d8, 669))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[1], f->P_to_aa[5], f->P_to_aa[9], 686))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d17, d10, d12, 701))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[2], f->P_to_aa[6], f->P_to_aa[10], 718))
            return false;
        if (!mc_cv_vertex(c, d14, d15, d17, d7, d9, 733))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[3], f->P_to_aa[7], f->P_to_aa[11], 750))
            return false;
        if (!mc_cv_vertex(c, d14, d16, d17, d5, d11, 765))
            return false;
    } else {
        if (!mc_cv_vertex(c, d13, d16, d17, d6, d8, 783))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d17, d10, d12, 798))
            return false;
        if (!mc_cv_vertex(c, d14, d15, d17, d7, d9, 813))
            return false;
        if (!mc_cv_vertex(c, d14, d16, d17, d5, d11, 828))
            return false;
    }
    return true;
}

static bool face_e(mc_cv_context *c, const mc_block *block, double d2, double d3, double d4,
                   int32_t n2) {
    mc_client_render_blocks *f = c->fields;
    double d5;
    if (!mc_cv_nw(c, 0))
        return false;
    if (f->d >= 0) {
        n2 = f->d;
    }
    int32_t n3 = (n2 & 0xF) << 4;
    int32_t n4 = n2 & 0xF0;
    const mc_block_fields *b = NULL;
    if (!mc_cv_fields(c, block, 39, &b) || !mc_cv_fp(c, 45))
        return false;
    double d6 = at_div(at_add((double)n3, at_mul(b->bu, 16.0)), 256.0);
    double d7 = at_div(at_sub(at_add((double)n3, at_mul(b->bx, 16.0)), 0.01), 256.0);
    double d8 = at_div(at_sub((double)(n4 + 16), at_mul(b->bw, 16.0)), 256.0);
    double d9 = at_div(at_sub(at_sub((double)(n4 + 16), at_mul(b->bt, 16.0)), 0.01), 256.0);
    if (f->e) {
        d5 = d6;
        d6 = d7;
        d7 = d5;
    }
    if (b->bu < 0.0 || b->bx > 1.0) {
        d6 = at_widen(at_fdiv(at_fadd((float)n3, 0.0f), 256.0f));
        d7 = at_widen(at_fdiv(at_fadd((float)n3, 15.99f), 256.0f));
    }
    if (b->bt < 0.0 || b->bw > 1.0) {
        d8 = at_widen(at_fdiv(at_fadd((float)n4, 0.0f), 256.0f));
        d9 = at_widen(at_fdiv(at_fadd((float)n4, 15.99f), 256.0f));
    }
    d5 = d7;
    double d10 = d6;
    double d11 = d8;
    double d12 = d9;
    if (f->j == 1) {
        d6 = at_div(at_add((double)n3, at_mul(b->bt, 16.0)), 256.0);
        d8 = at_div(at_sub((double)(n4 + 16), at_mul(b->bx, 16.0)), 256.0);
        d7 = at_div(at_add((double)n3, at_mul(b->bw, 16.0)), 256.0);
        d9 = at_div(at_sub((double)(n4 + 16), at_mul(b->bu, 16.0)), 256.0);
        d5 = d7;
        d10 = d6;
        d11 = d8;
        d12 = d9;
        d5 = d6;
        d10 = d7;
        d8 = d9;
        d9 = d11;
    } else if (f->j == 2) {
        d6 = at_div(at_sub((double)(n3 + 16), at_mul(b->bw, 16.0)), 256.0);
        d8 = at_div(at_add((double)n4, at_mul(b->bu, 16.0)), 256.0);
        d7 = at_div(at_sub((double)(n3 + 16), at_mul(b->bt, 16.0)), 256.0);
        d9 = at_div(at_add((double)n4, at_mul(b->bx, 16.0)), 256.0);
        d5 = d7;
        d10 = d6;
        d11 = d8;
        d12 = d9;
        d6 = d5;
        d7 = d10;
        d11 = d9;
        d12 = d8;
    } else if (f->j == 3) {
        d6 = at_div(at_sub((double)(n3 + 16), at_mul(b->bu, 16.0)), 256.0);
        d7 = at_div(at_sub(at_sub((double)(n3 + 16), at_mul(b->bx, 16.0)), 0.01), 256.0);
        d8 = at_div(at_add((double)n4, at_mul(b->bw, 16.0)), 256.0);
        d9 = at_div(at_sub(at_add((double)n4, at_mul(b->bt, 16.0)), 0.01), 256.0);
        d5 = d7;
        d10 = d6;
        d11 = d8;
        d12 = d9;
    }
    double d13 = at_add(d2, b->bs);
    double d14 = at_add(d3, b->bt);
    double d15 = at_add(d3, b->bw);
    double d16 = at_add(d4, b->bu);
    double d17 = at_add(d4, b->bx);
    if (f->m) {
        if (!mc_cv_color(c, f->P_to_aa[0], f->P_to_aa[4], f->P_to_aa[8], 655))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d17, d5, d11, 670))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[1], f->P_to_aa[5], f->P_to_aa[9], 687))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d16, d6, d8, 702))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[2], f->P_to_aa[6], f->P_to_aa[10], 719))
            return false;
        if (!mc_cv_vertex(c, d13, d14, d16, d10, d12, 734))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[3], f->P_to_aa[7], f->P_to_aa[11], 751))
            return false;
        if (!mc_cv_vertex(c, d13, d14, d17, d7, d9, 766))
            return false;
    } else {
        if (!mc_cv_vertex(c, d13, d15, d17, d5, d11, 784))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d16, d6, d8, 799))
            return false;
        if (!mc_cv_vertex(c, d13, d14, d16, d10, d12, 814))
            return false;
        if (!mc_cv_vertex(c, d13, d14, d17, d7, d9, 829))
            return false;
    }
    return true;
}

static bool face_f(mc_cv_context *c, const mc_block *block, double d2, double d3, double d4,
                   int32_t n2) {
    mc_client_render_blocks *f = c->fields;
    double d5;
    if (!mc_cv_nw(c, 0))
        return false;
    if (f->d >= 0) {
        n2 = f->d;
    }
    int32_t n3 = (n2 & 0xF) << 4;
    int32_t n4 = n2 & 0xF0;
    const mc_block_fields *b = NULL;
    if (!mc_cv_fields(c, block, 39, &b) || !mc_cv_fp(c, 45))
        return false;
    double d6 = at_div(at_add((double)n3, at_mul(b->bu, 16.0)), 256.0);
    double d7 = at_div(at_sub(at_add((double)n3, at_mul(b->bx, 16.0)), 0.01), 256.0);
    double d8 = at_div(at_sub((double)(n4 + 16), at_mul(b->bw, 16.0)), 256.0);
    double d9 = at_div(at_sub(at_sub((double)(n4 + 16), at_mul(b->bt, 16.0)), 0.01), 256.0);
    if (f->e) {
        d5 = d6;
        d6 = d7;
        d7 = d5;
    }
    if (b->bu < 0.0 || b->bx > 1.0) {
        d6 = at_widen(at_fdiv(at_fadd((float)n3, 0.0f), 256.0f));
        d7 = at_widen(at_fdiv(at_fadd((float)n3, 15.99f), 256.0f));
    }
    if (b->bt < 0.0 || b->bw > 1.0) {
        d8 = at_widen(at_fdiv(at_fadd((float)n4, 0.0f), 256.0f));
        d9 = at_widen(at_fdiv(at_fadd((float)n4, 15.99f), 256.0f));
    }
    d5 = d7;
    double d10 = d6;
    double d11 = d8;
    double d12 = d9;
    if (f->i == 2) {
        d6 = at_div(at_add((double)n3, at_mul(b->bt, 16.0)), 256.0);
        d8 = at_div(at_sub((double)(n4 + 16), at_mul(b->bu, 16.0)), 256.0);
        d7 = at_div(at_add((double)n3, at_mul(b->bw, 16.0)), 256.0);
        d9 = at_div(at_sub((double)(n4 + 16), at_mul(b->bx, 16.0)), 256.0);
        d5 = d7;
        d10 = d6;
        d11 = d8;
        d12 = d9;
        d5 = d6;
        d10 = d7;
        d8 = d9;
        d9 = d11;
    } else if (f->i == 1) {
        d6 = at_div(at_sub((double)(n3 + 16), at_mul(b->bw, 16.0)), 256.0);
        d8 = at_div(at_add((double)n4, at_mul(b->bx, 16.0)), 256.0);
        d7 = at_div(at_sub((double)(n3 + 16), at_mul(b->bt, 16.0)), 256.0);
        d9 = at_div(at_add((double)n4, at_mul(b->bu, 16.0)), 256.0);
        d5 = d7;
        d10 = d6;
        d11 = d8;
        d12 = d9;
        d6 = d5;
        d7 = d10;
        d11 = d9;
        d12 = d8;
    } else if (f->i == 3) {
        d6 = at_div(at_sub((double)(n3 + 16), at_mul(b->bu, 16.0)), 256.0);
        d7 = at_div(at_sub(at_sub((double)(n3 + 16), at_mul(b->bx, 16.0)), 0.01), 256.0);
        d8 = at_div(at_add((double)n4, at_mul(b->bw, 16.0)), 256.0);
        d9 = at_div(at_sub(at_add((double)n4, at_mul(b->bt, 16.0)), 0.01), 256.0);
        d5 = d7;
        d10 = d6;
        d11 = d8;
        d12 = d9;
    }
    double d13 = at_add(d2, b->bv);
    double d14 = at_add(d3, b->bt);
    double d15 = at_add(d3, b->bw);
    double d16 = at_add(d4, b->bu);
    double d17 = at_add(d4, b->bx);
    if (f->m) {
        if (!mc_cv_color(c, f->P_to_aa[0], f->P_to_aa[4], f->P_to_aa[8], 655))
            return false;
        if (!mc_cv_vertex(c, d13, d14, d17, d10, d12, 670))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[1], f->P_to_aa[5], f->P_to_aa[9], 687))
            return false;
        if (!mc_cv_vertex(c, d13, d14, d16, d7, d9, 702))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[2], f->P_to_aa[6], f->P_to_aa[10], 719))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d16, d5, d11, 734))
            return false;
        if (!mc_cv_color(c, f->P_to_aa[3], f->P_to_aa[7], f->P_to_aa[11], 751))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d17, d6, d8, 766))
            return false;
    } else {
        if (!mc_cv_vertex(c, d13, d14, d17, d10, d12, 784))
            return false;
        if (!mc_cv_vertex(c, d13, d14, d16, d7, d9, 799))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d16, d5, d11, 814))
            return false;
        if (!mc_cv_vertex(c, d13, d15, d17, d6, d8, 829))
            return false;
    }
    return true;
}

bool mc_cv_face_body(mc_cv_context *c, int32_t side, const mc_block *block, double x, double y,
                     double z, int32_t texture) {
    switch (side) {
        case 0:
            return face_a(c, block, x, y, z, texture);
        case 1:
            return face_b(c, block, x, y, z, texture);
        case 2:
            return face_c(c, block, x, y, z, texture);
        case 3:
            return face_d(c, block, x, y, z, texture);
        case 4:
            return face_e(c, block, x, y, z, texture);
        case 5:
            return face_f(c, block, x, y, z, texture);
        default:
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                              "native CV face selector", 0, true);
    }
}
