#include "render_global_recenter.h"

#include "source_chunk_renderer_internal.h"

#include <limits.h>

static bool fail(mc_client_render_global_error *e, mc_client_render_global_status status,
                 const char *operation, unsigned int pc, bool native) {
    mc_render_global_source_fail(e, status, operation, "n", "b", "(III)V", pc, native);
    return false;
}

static bool quotient(int32_t left, int32_t right, int32_t *out, unsigned int pc,
                     mc_client_render_global_error *e) {
    if (mc_java_i32_div(left, right, out))
        return true;
    e->child = (mc_client_graphics_site){"java/lang/ArithmeticException", NULL, NULL, 0};
    return fail(e, MC_CLIENT_RENDER_GLOBAL_SOURCE_ARITHMETIC,
                "actual renderer-grid integer division by zero", pc, false);
}

static bool chunk(mc_client_render_global *n, int32_t index, mc_client_source_dk **out,
                  mc_client_render_global_error *e) {
    if (n->fields.o == NULL)
        return fail(e, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual recenter spatial DK array",
                    309, false);
    if (index < 0 || (size_t)index >= n->o_length) {
        e->graphics.status = MC_CLIENT_GRAPHICS_ARRAY_INDEX;
        e->graphics.actual_index = index;
        e->graphics.site = (mc_client_graphics_site){"n", "b", "(III)V", 309};
        return fail(e, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                    "actual recenter spatial DK array index", 309, false);
    }
    *out = n->fields.o[index];
    if (*out == NULL)
        return fail(e, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual recenter old DK.u receiver",
                    314, false);
    return true;
}

bool mc_render_global_recenter_entered(mc_client_render_global *n, int32_t x, int32_t y, int32_t z,
                                       mc_client_render_global_error *e) {
    if (e == NULL)
        return false;
    if (n == NULL || !n->active)
        return fail(e, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                    "recenter requires the admitted actual N guard", 0, true);
    x = mc_java_i32_sub(x, 8);
    y = mc_java_i32_sub(y, 8);
    z = mc_java_i32_sub(z, 8);
    (void)y; /* Source decrements it but uses each Y-loop coordinate below. */
    n->fields.B = INT32_MAX;
    n->fields.C = INT32_MAX;
    n->fields.D = INT32_MAX;
    n->fields.E = INT32_MIN;
    n->fields.F = INT32_MIN;
    n->fields.G = INT32_MIN;
    const int32_t width = mc_java_i32_mul(n->fields.p, 16);
    int32_t half;
    if (!quotient(width, 2, &half, 57, e))
        return false;
    for (int32_t ix = 0; ix < n->fields.p; ix = mc_java_i32_add(ix, 1)) {
        int32_t px = mc_java_i32_mul(ix, 16);
        int32_t shift = mc_java_i32_sub(mc_java_i32_add(px, half), x);
        if (shift < 0)
            shift = mc_java_i32_sub(shift, mc_java_i32_sub(width, 1));
        if (!quotient(shift, width, &shift, 106, e))
            return false;
        px = mc_java_i32_sub(px, mc_java_i32_mul(shift, width));
        if (px < n->fields.B)
            n->fields.B = px;
        if (px > n->fields.E)
            n->fields.E = px;
        for (int32_t iz = 0; iz < n->fields.r; iz = mc_java_i32_add(iz, 1)) {
            int32_t pz = mc_java_i32_mul(iz, 16);
            shift = mc_java_i32_sub(mc_java_i32_add(pz, half), z);
            if (shift < 0)
                shift = mc_java_i32_sub(shift, mc_java_i32_sub(width, 1));
            if (!quotient(shift, width, &shift, 195, e))
                return false;
            pz = mc_java_i32_sub(pz, mc_java_i32_mul(shift, width));
            if (pz < n->fields.D)
                n->fields.D = pz;
            if (pz > n->fields.G)
                n->fields.G = pz;
            for (int32_t iy = 0; iy < n->fields.q; iy = mc_java_i32_add(iy, 1)) {
                const int32_t py = mc_java_i32_mul(iy, 16);
                if (py < n->fields.C)
                    n->fields.C = py;
                if (py > n->fields.F)
                    n->fields.F = py;
                const int32_t index = mc_java_i32_add(
                    mc_java_i32_mul(mc_java_i32_add(mc_java_i32_mul(iz, n->fields.q), iy),
                                    n->fields.p),
                    ix);
                mc_client_source_dk *d = NULL;
                if (!chunk(n, index, &d, e))
                    return false;
                const bool dirty = d->fields.u;
                if (d->owner != n)
                    return fail(e, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                                "recenter DK belongs to another native owner", 327, true);
                if (!mc_source_dk_position(d, px, py, pz, e)) {
                    const mc_client_graphics_site caller = {"n", "b", "(III)V", 327};
                    if (e->parent.class_name != NULL)
                        e->ancestor = caller;
                    else
                        e->parent = caller;
                    return false;
                }
                if (!dirty && d->fields.u &&
                    !mc_render_global_pointer_append(
                        n, n->fields.m, d, (mc_client_graphics_site){"n", "b", "(III)V", 349}, e))
                    return false;
            }
        }
    }
    return true;
}
