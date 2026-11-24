#include "path_internal.h"
#include <math.h>
#include <string.h>

static bool up(mc_path_finder *w, int32_t index) {
    mc_path_heap *h = w->b;
    pf_child(w, "lf", "a", "(I)V", 5);
    if (h->a == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path LF up null array");
    if (index < 0 || (size_t)(uint32_t)index >= h->capacity)
        return pf_source(w, "java/lang/ArrayIndexOutOfBoundsException", "Path LF up array index");
    mc_path_point *point = h->a[index];
    pf_child(w, "lf", "a", "(I)V", 8);
    if (point == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path LF up null Point");
    const float priority = point->g;
    w->state.heap_work_point = point;
    w->state.heap_work_index = index;
    while (index > 0) {
        pf_child(w, "lf", "a", "(I)V", 29);
        if (!pf_work(w))
            return false;
        const int32_t parent = (index - 1) >> 1;
        mc_path_point *above = h->a[parent];
        if (!(priority < above->g))
            break;
        h->a[index] = above;
        above->d = index;
        index = parent;
        w->state.heap_work_index = index;
    }
    h->a[index] = point;
    point->d = index;
    return true;
}

static bool down(mc_path_finder *w, int32_t index) {
    mc_path_heap *h = w->b;
    pf_child(w, "lf", "b", "(I)V", 5);
    if (h->a == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path LF down null array");
    if (index < 0 || (size_t)(uint32_t)index >= h->capacity)
        return pf_source(w, "java/lang/ArrayIndexOutOfBoundsException", "Path LF down array index");
    mc_path_point *point = h->a[index];
    pf_child(w, "lf", "b", "(I)V", 8);
    if (point == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path LF down null Point");
    const float priority = point->g;
    w->state.heap_work_point = point;
    w->state.heap_work_index = index;
    for (;;) {
        const int32_t left = mc_java_i32_add(1, mc_java_i32_shl(index, 1));
        const int32_t right = mc_java_i32_add(left, 1);
        if (left >= h->b)
            break;
        pf_child(w, "lf", "b", "(I)V", 43);
        if (!pf_work(w))
            return false;
        mc_path_point *a = h->a[left], *b = right < h->b ? h->a[right] : NULL;
        const float bg = b == NULL ? INFINITY : b->g;
        if (a->g < bg) {
            if (!(a->g < priority))
                break;
            h->a[index] = a;
            a->d = index;
            index = left;
            w->state.heap_work_index = index;
        } else {
            if (!(bg < priority))
                break;
            h->a[index] = b;
            b->d = index;
            index = right;
            w->state.heap_work_index = index;
        }
    }
    h->a[index] = point;
    point->d = index;
    return true;
}

void pf_heap_clear(mc_path_finder *w) {
    pf_child(w, "lf", "a", "()V", 2);
    w->b->b = 0;
}

bool pf_heap_add(mc_path_finder *w, mc_path_point *point) {
    mc_path_heap *h = w->b;
    pf_child(w, "lf", "a", "(Ld;)Ld;", 1);
    if (point == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path LF null Point");
    if (point->d >= 0) {
        pf_child(w, "lf", "a", "(Ld;)Ld;", 16);
        return pf_source(w, "java/lang/IllegalStateException", "Path LF already open Point");
    }
    if (h->a == NULL) {
        pf_child(w, "lf", "a", "(Ld;)Ld;", 25);
        return pf_source(w, "java/lang/NullPointerException", "Path LF add null array");
    }
    if ((size_t)(uint32_t)h->b == h->capacity) {
        pf_child(w, "lf", "a", "(Ld;)Ld;", 35);
        if (h->capacity > (size_t)INT32_MAX / 2 || h->capacity * 2 > SIZE_MAX / sizeof(*h->a))
            return pf_fail(w, MC_PATH_LIMIT, "Path LF backing extent");
        const size_t capacity = h->capacity * 2;
        mc_path_point **next = pf_alloc(w, capacity * sizeof(*next), false);
        if (next == NULL)
            return false;
        memcpy(next, h->a, (size_t)(uint32_t)h->b * sizeof(*next));
        mc_path_point **old = h->a;
        h->a = next;
        h->capacity = capacity;
        pf_free(w, old);
    }
    if (h->b < 0 || (size_t)(uint32_t)h->b >= h->capacity) {
        pf_child(w, "lf", "a", "(Ld;)Ld;", 67);
        return pf_source(w, "java/lang/ArrayIndexOutOfBoundsException", "Path LF add array index");
    }
    h->a[h->b] = point;
    point->d = h->b;
    const int32_t index = h->b;
    h->b = mc_java_i32_add(h->b, 1);
    return up(w, index);
}

bool pf_heap_pop(mc_path_finder *w, mc_path_point **out) {
    mc_path_heap *h = w->b;
    pf_child(w, "lf", "b", "()Ld;", 5);
    if (h->a == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path LF pop null array");
    if (h->capacity == 0)
        return pf_source(w, "java/lang/ArrayIndexOutOfBoundsException", "Path LF pop root index");
    mc_path_point *point = h->a[0];
    h->b = mc_java_i32_sub(h->b, 1);
    if (h->b < 0 || (size_t)(uint32_t)h->b >= h->capacity) {
        pf_child(w, "lf", "b", "()Ld;", 27);
        return pf_source(w, "java/lang/ArrayIndexOutOfBoundsException", "Path LF empty pop");
    }
    h->a[0] = h->a[h->b];
    h->a[h->b] = NULL;
    if (h->b > 0 && !down(w, 0))
        return false;
    if (point == NULL) {
        pf_child(w, "lf", "b", "()Ld;", 53);
        return pf_source(w, "java/lang/NullPointerException", "Path LF null popped Point");
    }
    point->d = -1;
    *out = point;
    return true;
}

bool pf_heap_change(mc_path_finder *w, mc_path_point *point, float priority) {
    pf_child(w, "lf", "a", "(Ld;F)V", 1);
    if (point == NULL)
        return pf_source(w, "java/lang/NullPointerException", "Path LF change null Point");
    const float old = point->g;
    point->g = priority;
    return priority < old ? up(w, point->d) : down(w, point->d);
}
