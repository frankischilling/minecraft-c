#include "path_internal.h"
#include <string.h>

static uint32_t mixed(int32_t key) {
    uint32_t h = (uint32_t)key;
    h ^= (h >> 20) ^ (h >> 12);
    return h ^ (h >> 7) ^ (h >> 4);
}

mc_path_point *pf_map_get(mc_path_finder *w, int32_t key) {
    mc_path_map *map = w->c;
    if (map->a == NULL) {
        pf_child(w, "jx", "a", "(I)Ljava/lang/Object;", 14);
        pf_source(w, "java/lang/NullPointerException", "Path JX get null array");
        return NULL;
    }
    pf_child(w, "jx", "a", "(I)Ljava/lang/Object;", 18);
    for (mc_path_map_entry *e = map->a[mixed(key) & (map->capacity - 1)]; e != NULL; e = e->c) {
        pf_child(w, "jx", "a", "(I)Ljava/lang/Object;", 25);
        if (!pf_work(w))
            return NULL;
        if (e->a == key)
            return e->b;
    }
    return NULL;
}

static bool grow(mc_path_finder *w) {
    mc_path_map *m = w->c;
    if (m->capacity == UINT32_C(1073741824)) {
        m->c = INT32_MAX;
        return true;
    }
    pf_child(w, "jx", "f", "(I)V", 22);
    if (m->capacity > (size_t)INT32_MAX / 2 || m->capacity * 2 > SIZE_MAX / sizeof(*m->a))
        return pf_fail(w, MC_PATH_LIMIT, "Path JX bucket extent");
    const size_t capacity = m->capacity * 2;
    mc_path_map_entry **next = pf_alloc(w, capacity * sizeof(*next), false);
    if (next == NULL)
        return false;
    w->state.pending_map_array = (const mc_path_map_entry *const *)next;
    w->state.pending_map_capacity = capacity;
    for (size_t i = 0; i < m->capacity; ++i) {
        pf_child(w, "jx", "a", "([Lwm;)V", 21);
        if (!pf_work(w))
            return false;
        mc_path_map_entry *e = m->a[i];
        if (e != NULL) {
            m->a[i] = NULL;
            do {
                w->state.map_transfer_entry = e;
                pf_child(w, "jx", "a", "([Lwm;)V", 38);
                if (!pf_work(w))
                    return false;
                mc_path_map_entry *following = e->c;
                const size_t at = (uint32_t)e->d & (capacity - 1);
                e->c = next[at];
                next[at] = e;
                e = following;
            } while (e != NULL);
        }
    }
    mc_path_map_entry **old = m->a;
    m->a = next;
    m->capacity = capacity;
    m->c = mc_java_float_to_i32(at_fmul((float)capacity, m->d));
    w->state.pending_map_array = NULL;
    w->state.pending_map_capacity = 0;
    w->state.map_transfer_entry = NULL;
    pf_free(w, old);
    return true;
}

bool pf_map_put(mc_path_finder *w, int32_t key, mc_path_point *point) {
    mc_path_map *m = w->c;
    if (m->a == NULL) {
        pf_child(w, "jx", "a", "(ILjava/lang/Object;)V", 10);
        return pf_source(w, "java/lang/NullPointerException", "Path JX put null array");
    }
    const uint32_t hash = mixed(key);
    const size_t at = hash & (m->capacity - 1);
    for (mc_path_map_entry *e = m->a[at]; e != NULL; e = e->c) {
        pf_child(w, "jx", "a", "(ILjava/lang/Object;)V", 32);
        if (!pf_work(w))
            return false;
        if (e->a == key)
            e->b = point;
    }
    m->e = mc_java_i32_add(m->e, 1);
    pf_child(w, "jx", "a", "(IILjava/lang/Object;I)V", 15);
    mc_path_map_entry *e = pf_alloc(w, sizeof(*e), false);
    if (e == NULL)
        return false;
    e->a = key;
    e->b = point;
    e->c = m->a[at];
    e->d = mc_java_i32_narrow((int64_t)hash);
    m->a[at] = e;
    const int32_t previous = m->b;
    m->b = mc_java_i32_add(previous, 1);
    if (previous >= m->c)
        return grow(w);
    return true;
}

bool pf_map_clear(mc_path_finder *w) {
    mc_path_map *m = w->c;
    m->e = mc_java_i32_add(m->e, 1);
    if (m->a == NULL) {
        pf_child(w, "jx", "a", "()V", 19);
        return pf_source(w, "java/lang/NullPointerException", "Path JX clear null array");
    }
    for (size_t i = 0; i < m->capacity; ++i) {
        pf_child(w, "jx", "a", "()V", 26);
        if (!pf_work(w))
            return false;
        m->a[i] = NULL;
    }
    m->b = 0;
    return true;
}

bool mc_path_map_entry_inspect(const mc_path_map_entry *e, mc_path_map_entry_state *out) {
    if (e == NULL || out == NULL)
        return false;
    *out = (mc_path_map_entry_state){e->d, e->a, e->b, e->c};
    return true;
}
