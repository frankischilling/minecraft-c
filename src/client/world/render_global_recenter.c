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

static bool axis_position(int32_t slot, uint32_t camera, int32_t width, int32_t half, int32_t *out,
                          unsigned int pc, mc_client_render_global_error *e) {
    const uint32_t origin = (uint32_t)slot * 16u;
    uint32_t delta = origin + (uint32_t)half - camera;
    if (delta > INT32_MAX)
        delta -= (uint32_t)width - 1u;
    int32_t wraps;
    if (!quotient(mc_java_i32_narrow(delta), width, &wraps, pc, e))
        return false;
    *out = mc_java_i32_narrow(origin - (uint32_t)wraps * (uint32_t)width);
    return true;
}

static bool chunk(mc_client_render_global *n, uint32_t index, mc_client_source_dk **out,
                  mc_client_render_global_error *e) {
    if (n->fields.o == NULL)
        return fail(e, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual recenter spatial DK array",
                    309, false);
    if (index > INT32_MAX || (size_t)index >= n->o_length) {
        e->graphics.status = MC_CLIENT_GRAPHICS_ARRAY_INDEX;
        e->graphics.actual_index = mc_java_i32_narrow(index);
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
    const uint32_t camera_x = (uint32_t)x - 8u;
    const uint32_t camera_z = (uint32_t)z - 8u;
    (void)y; /* The vertical grid uses its section slots. */
    n->fields.B = INT32_MAX;
    n->fields.C = INT32_MAX;
    n->fields.D = INT32_MAX;
    n->fields.E = INT32_MIN;
    n->fields.F = INT32_MIN;
    n->fields.G = INT32_MIN;
    const int32_t width = mc_java_i32_mul(n->fields.p, 16);
    const int32_t half = width / 2;
    const uint32_t row_stride = (uint32_t)n->fields.p;
    const uint32_t column_stride = row_stride * (uint32_t)n->fields.q;

    enum { cached_z_limit = 64 };

    int32_t z_positions[cached_z_limit];
    const bool cache_z = n->fields.r <= cached_z_limit;
    for (int32_t ix = 0; ix < n->fields.p; ++ix) {
        int32_t px;
        if (!axis_position(ix, camera_x, width, half, &px, 106, e))
            return false;
        if (px < n->fields.B)
            n->fields.B = px;
        if (px > n->fields.E)
            n->fields.E = px;
        for (int32_t iz = 0; iz < n->fields.r; ++iz) {
            int32_t pz;
            /* Keep the first column's traversal order, including any early failure. */
            if (cache_z && ix != 0) {
                pz = z_positions[iz];
            } else {
                if (!axis_position(iz, camera_z, width, half, &pz, 195, e))
                    return false;
                if (cache_z)
                    z_positions[iz] = pz;
            }
            if (pz < n->fields.D)
                n->fields.D = pz;
            if (pz > n->fields.G)
                n->fields.G = pz;
            uint32_t index = column_stride * (uint32_t)iz + (uint32_t)ix;
            for (int32_t iy = 0; iy < n->fields.q; ++iy, index += row_stride) {
                const uint32_t vertical = (uint32_t)iy * 16u;
                const int32_t py =
                    vertical <= INT32_MAX ? (int32_t)vertical : mc_java_i32_narrow(vertical);
                if (py < n->fields.C)
                    n->fields.C = py;
                if (py > n->fields.F)
                    n->fields.F = py;
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
