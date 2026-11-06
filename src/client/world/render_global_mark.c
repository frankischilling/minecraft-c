#include "../render_engine/internal.h"
#include "source_chunk_renderer_internal.h"
#include <string.h>

static mc_client_render_global_status mark(mc_client_render_global *o, int32_t x0, int32_t y0,
                                           int32_t z0, int32_t x1, int32_t y1, int32_t z1,
                                           mc_client_render_global_error *e) {
    if (o == NULL || e == NULL)
        return MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT;
    memset(e, 0, sizeof(*e));
    if (o->world_retiring)
        return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                            "terminal RenderGlobal source marking", NULL, NULL,
                                            NULL, 0, true);
    const int32_t first_x = mc_java_i32_shr(x0, 4), first_y = mc_java_i32_shr(y0, 4),
                  first_z = mc_java_i32_shr(z0, 4);
    const int32_t last_x = mc_java_i32_shr(x1, 4), last_y = mc_java_i32_shr(y1, 4),
                  last_z = mc_java_i32_shr(z1, 4);
    for (int32_t x = first_x; x <= last_x; x = mc_java_i32_add(x, 1)) {
        int32_t wrapped_x;
        if (!mc_java_i32_rem(x, o->fields.p, &wrapped_x))
            return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                                                "actual n.a range integer remainder by zero", "n",
                                                "a", "(IIIIII)V", 68, false);
        if (wrapped_x < 0)
            wrapped_x = mc_java_i32_add(wrapped_x, o->fields.p);
        for (int32_t y = first_y; y <= last_y; y = mc_java_i32_add(y, 1)) {
            int32_t wrapped_y;
            if (!mc_java_i32_rem(y, o->fields.q, &wrapped_y))
                return mc_render_global_source_fail(
                    e, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                    "actual n.a range integer remainder by zero", "n", "a", "(IIIIII)V", 102,
                    false);
            if (wrapped_y < 0)
                wrapped_y = mc_java_i32_add(wrapped_y, o->fields.q);
            for (int32_t z = first_z; z <= last_z; z = mc_java_i32_add(z, 1)) {
                int32_t wrapped_z;
                if (!mc_java_i32_rem(z, o->fields.r, &wrapped_z))
                    return mc_render_global_source_fail(
                        e, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                        "actual n.a range integer remainder by zero", "n", "a", "(IIIIII)V", 137,
                        false);
                if (wrapped_z < 0)
                    wrapped_z = mc_java_i32_add(wrapped_z, o->fields.r);
                const int32_t index = mc_java_i32_add(
                    mc_java_i32_mul(
                        mc_java_i32_add(mc_java_i32_mul(wrapped_z, o->fields.q), wrapped_y),
                        o->fields.p),
                    wrapped_x);
                if (o->fields.o == NULL)
                    return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                                        "actual n.o array receiver", "n", "a",
                                                        "(IIIIII)V", 179, false);
                if (index < 0 || (size_t)index >= o->o_length)
                    return mc_render_global_source_fail(
                        e, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                        "actual n.o array index", "n", "a", "(IIIIII)V", 179, false);
                mc_client_source_dk *chunk = o->fields.o[index];
                if (chunk == NULL)
                    return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                                        "actual DK.u range receiver", "n", "a",
                                                        "(IIIIII)V", 184, false);
                if (!chunk->fields.u) {
                    if (!mc_render_global_pointer_append(
                            o, o->fields.m, chunk,
                            (mc_client_graphics_site){"n", "a", "(IIIIII)V", 196}, e))
                        return e->status;
                    mc_source_dk_dirty(chunk);
                }
            }
        }
    }
    e->source_returned = true;
    return MC_CLIENT_RENDER_GLOBAL_OK;
}

mc_client_render_global_status mc_render_global_mark_cell(mc_client_render_global *o, int32_t x,
                                                          int32_t y, int32_t z,
                                                          mc_client_render_global_error *e) {
    const mc_client_render_global_status status =
        mark(o, mc_java_i32_sub(x, 1), mc_java_i32_sub(y, 1), mc_java_i32_sub(z, 1),
             mc_java_i32_add(x, 1), mc_java_i32_add(y, 1), mc_java_i32_add(z, 1), e);
    if (status != MC_CLIENT_RENDER_GLOBAL_OK &&
        status != MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT && e != NULL)
        e->parent = (mc_client_graphics_site){"n", "a", "(III)V", 19};
    return status;
}

mc_client_render_global_status mc_render_global_mark_range(mc_client_render_global *o, int32_t x0,
                                                           int32_t y0, int32_t z0, int32_t x1,
                                                           int32_t y1, int32_t z1,
                                                           mc_client_render_global_error *e) {
    const mc_client_render_global_status status =
        mark(o, mc_java_i32_sub(x0, 1), mc_java_i32_sub(y0, 1), mc_java_i32_sub(z0, 1),
             mc_java_i32_add(x1, 1), mc_java_i32_add(y1, 1), mc_java_i32_add(z1, 1), e);
    if (status != MC_CLIENT_RENDER_GLOBAL_OK &&
        status != MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT && e != NULL)
        e->parent = (mc_client_graphics_site){"n", "b", "(IIIIII)V", 19};
    return status;
}
