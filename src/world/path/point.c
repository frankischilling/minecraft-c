#include "path_internal.h"

int32_t pf_point_key(int32_t x, int32_t y, int32_t z) {
    return (y & 255) | mc_java_i32_shl(x & 32767, 8) | mc_java_i32_shl(z & 32767, 24) |
           (x < 0 ? INT32_MIN : 0) | (z < 0 ? 32768 : 0);
}

bool pf_point_equals(const mc_path_point *p, const mc_path_point *q) {
    return q != NULL && p->j == q->j && p->a == q->a && p->b == q->b && p->c == q->c;
}

mc_path_point *pf_point_new(mc_path_finder *w) {
    if (w->arena->points == w->limits.max_points) {
        pf_fail(w, MC_PATH_LIMIT, "Path pending D ceiling");
        return NULL;
    }
    mc_path_point *p = pf_alloc(w, sizeof(*p), true);
    if (p == NULL)
        return NULL;
    p->allocation_next = w->arena->point_list;
    w->arena->point_list = p;
    ++w->arena->points;
    w->state.pending_point = p;
    return p;
}

void pf_point_construct(mc_path_finder *w, mc_path_point *p, int32_t x, int32_t y, int32_t z) {
    pf_child(w, "d", "<init>", "(III)V", 6);
    p->d = -1;
    p->constructor_pc = 6;
    p->i = false;
    p->constructor_pc = 12;
    p->a = x;
    p->constructor_pc = 16;
    p->b = y;
    p->constructor_pc = 21;
    p->c = z;
    p->constructor_pc = 26;
    p->j = pf_point_key(x, y, z);
    p->constructor_pc = 36;
    p->constructed = true;
    pf_child(w, "d", "<init>", "(III)V", 39);
    w->state.pending_point = NULL;
}

mc_path_point *pf_intern(mc_path_finder *w, int32_t x, int32_t y, int32_t z) {
    pf_at(w, "a", "(III)Ld;", 3);
    const int32_t key = pf_point_key(x, y, z);
    pf_at(w, "a", "(III)Ld;", 14);
    mc_path_point *p = pf_map_get(w, key);
    w->state.intern_point = p;
    if (w->state.failed)
        return NULL;
    if (p == NULL) {
        pf_at(w, "a", "(III)Ld;", 27);
        p = pf_point_new(w);
        if (p == NULL)
            return NULL;
        w->state.intern_point = p;
        pf_at(w, "a", "(III)Ld;", 34);
        pf_point_construct(w, p, x, y, z);
        pf_at(w, "a", "(III)Ld;", 47);
        if (!pf_map_put(w, key, p))
            return NULL;
    }
    pf_at(w, "a", "(III)Ld;", 52);
    return p;
}

bool pf_distance(mc_path_finder *w, const mc_path_point *p, const mc_path_point *q, float *out) {
    pf_child(w, "d", "a", "(Ld;)F", 1);
    if (q == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path D distance null argument");
    if (!pf_fp(w))
        return false;
    const float x = (float)mc_java_i32_sub(q->a, p->a);
    const float y = (float)mc_java_i32_sub(q->b, p->b);
    const float z = (float)mc_java_i32_sub(q->c, p->c);
    const float sum = at_fadd(at_fadd(at_fmul(x, x), at_fmul(y, y)), at_fmul(z, z));
    pf_child(w, "d", "a", "(Ld;)F", 47);
    if (!pf_math(w))
        return false;
    *out = at_root(at_widen(sum));
    return true;
}
