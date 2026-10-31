#include "path_internal.h"

bool pf_candidate(mc_path_finder *w, int32_t x, int32_t y, int32_t z, mc_path_point *extent,
                  int32_t step, mc_path_point **out) {
    mc_path_point *point = NULL;
    int32_t result;
    pf_at(w, "a", "(Lsn;IIILd;I)Ld;", 11);
    if (!pf_clearance(w, x, y, z, extent, &result))
        return false;
    if (result == 1) {
        pf_at(w, "a", "(Lsn;IIILd;I)Ld;", 23);
        point = pf_intern(w, x, y, z);
        if (point == NULL)
            return false;
    }
    if (point == NULL && step > 0) {
        pf_at(w, "a", "(Lsn;IIILd;I)Ld;", 49);
        if (!pf_clearance(w, x, mc_java_i32_add(y, step), z, extent, &result))
            return false;
        if (result == 1) {
            pf_at(w, "a", "(Lsn;IIILd;I)Ld;", 64);
            point = pf_intern(w, x, mc_java_i32_add(y, step), z);
            if (point == NULL)
                return false;
            y = mc_java_i32_add(y, step);
        }
    }
    if (point != NULL) {
        int32_t drops = 0;
        result = 0;
        while (y > 0) {
            pf_at(w, "a", "(Lsn;IIILd;I)Ld;", 99);
            if (!pf_clearance(w, x, mc_java_i32_sub(y, 1), z, extent, &result))
                return false;
            if (result != 1)
                break;
            drops = mc_java_i32_add(drops, 1);
            if (drops >= 4) {
                *out = NULL;
                return true;
            }
            y = mc_java_i32_sub(y, 1);
            if (y > 0) {
                pf_at(w, "a", "(Lsn;IIILd;I)Ld;", 132);
                point = pf_intern(w, x, y, z);
                if (point == NULL)
                    return false;
            }
        }
        if (result == -2)
            point = NULL;
    }
    pf_at(w, "a", "(Lsn;IIILd;I)Ld;", 151);
    *out = point;
    return true;
}

bool pf_neighbors(mc_path_finder *w, mc_path_point *current, mc_path_point *extent,
                  mc_path_point *goal, float range, size_t *out) {
    int32_t clearance;
    pf_at(w, "b", "(Lsn;Ld;Ld;Ld;F)I", 23);
    if (!pf_clearance(w, current->a, mc_java_i32_add(current->b, 1), current->c, extent,
                      &clearance))
        return false;
    const int32_t step = clearance == 1 ? 1 : 0;
    mc_path_point *choices[4];
    static const int32_t pcs[] = {52, 76, 100, 124};
    for (size_t i = 0; i < 4; ++i) {
        const int32_t x = i == 1   ? mc_java_i32_sub(current->a, 1)
                          : i == 2 ? mc_java_i32_add(current->a, 1)
                                   : current->a;
        const int32_t z = i == 0   ? mc_java_i32_add(current->c, 1)
                          : i == 3 ? mc_java_i32_sub(current->c, 1)
                                   : current->c;
        pf_at(w, "b", "(Lsn;Ld;Ld;Ld;F)I", pcs[i]);
        if (!pf_candidate(w, x, current->b, z, extent, step, &choices[i]))
            return false;
    }
    size_t count = 0;
    static const int32_t distances[] = {146, 184, 222, 260};
    for (size_t i = 0; i < 4; ++i) {
        mc_path_point *point = choices[i];
        if (point != NULL && !point->i) {
            float distance;
            pf_at(w, "b", "(Lsn;Ld;Ld;Ld;F)I", distances[i]);
            if (!pf_distance(w, point, goal, &distance))
                return false;
            if (distance < range)
                w->d[count++] = point;
        }
    }
    pf_at(w, "b", "(Lsn;Ld;Ld;Ld;F)I", 283);
    *out = count;
    return true;
}
