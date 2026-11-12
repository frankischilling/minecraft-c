#include "bounds_draw.h"

static mc_client_graphics_site bounds_site(int32_t pc) {
    return (mc_client_graphics_site){"bw", "a", "(Leq;)V", pc};
}

static bool bounds_child(mc_client_graphics_error *error, int32_t pc) {
    error->entry = bounds_site(0);
    error->parent = bounds_site(pc);
    return false;
}

bool mc_client_render_bounds_draw(mc_client_render_engine_classes *classes,
                                  mc_client_display *display, const mc_entity_bounds *bounds,
                                  mc_client_graphics_error *out) {
    mc_client_graphics_error scratch = {0};
    mc_client_graphics_error *error = out == NULL ? &scratch : out;
    mc_client_tessellator *nw = NULL;
    if (!mc_client_nw_active_use(classes, bounds_site(0), &nw, error))
        return false; /* The class initializer retains its own nested frame. */
    if (!mc_client_nw_begin_quads(nw, error))
        return bounds_child(error, 5);
    if (bounds == NULL) {
        mc_client_graphics_fail(error, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                                "actual bw bounds receiver", bounds_site(10), false);
        error->entry = bounds_site(0);
        return false;
    }

#define BOUNDS_VERTEX(pc, x, y, z)                                                                 \
    do {                                                                                           \
        if (!mc_client_nw_vertex(nw, bounds->x, bounds->y, bounds->z, error))                      \
            return bounds_child(error, pc);                                                        \
    } while (0)

    BOUNDS_VERTEX(21, min_x, max_y, min_z);
    BOUNDS_VERTEX(37, max_x, max_y, min_z);
    BOUNDS_VERTEX(53, max_x, min_y, min_z);
    BOUNDS_VERTEX(69, min_x, min_y, min_z);
    BOUNDS_VERTEX(85, min_x, min_y, max_z);
    BOUNDS_VERTEX(101, max_x, min_y, max_z);
    BOUNDS_VERTEX(117, max_x, max_y, max_z);
    BOUNDS_VERTEX(133, min_x, max_y, max_z);
    BOUNDS_VERTEX(149, min_x, min_y, min_z);
    BOUNDS_VERTEX(165, max_x, min_y, min_z);
    BOUNDS_VERTEX(181, max_x, min_y, max_z);
    BOUNDS_VERTEX(197, min_x, min_y, max_z);
    BOUNDS_VERTEX(213, min_x, max_y, max_z);
    BOUNDS_VERTEX(229, max_x, max_y, max_z);
    BOUNDS_VERTEX(245, max_x, max_y, min_z);
    BOUNDS_VERTEX(261, min_x, max_y, min_z);
    BOUNDS_VERTEX(277, min_x, min_y, max_z);
    BOUNDS_VERTEX(293, min_x, max_y, max_z);
    BOUNDS_VERTEX(309, min_x, max_y, min_z);
    BOUNDS_VERTEX(325, min_x, min_y, min_z);
    BOUNDS_VERTEX(341, max_x, min_y, min_z);
    BOUNDS_VERTEX(357, max_x, max_y, min_z);
    BOUNDS_VERTEX(373, max_x, max_y, max_z);
    BOUNDS_VERTEX(389, max_x, min_y, max_z);

#undef BOUNDS_VERTEX

    if (!mc_client_nw_draw(nw, display, error))
        return bounds_child(error, 393);
    return true;
}
