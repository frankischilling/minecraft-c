#ifndef MC_CLIENT_RENDER_ENGINE_NW_INTERNAL_H
#define MC_CLIENT_RENDER_ENGINE_NW_INTERNAL_H
#include "nw.h"

enum { MC_NW_BUFFER_WORDS = 2097152 };

typedef struct mc_client_nw_namespace {
    mc_client_render_engine_phase phase;
    bool b, c;
    mc_client_tessellator *a; /* Actual mutable Source static field. */
    /* Immutable native successful allocation occurrence, independent of a.
     * The class namespace owns its lifetime, without a counted owner cycle. */
    mc_client_tessellator *published;
    mc_client_tessellator *candidate; /* Only a failed/in-progress constructor. */
    mc_client_render_engine_options options;
    mc_client_graphics_error failure;
} mc_client_nw_namespace;

struct mc_client_tessellator {
    mc_client_render_engine_classes *classes; /* borrowed from sole class owner */
    mc_client_render_arena arena;
    mc_client_nw_fields fields;
    int32_t last_store_pc;
    bool active;
};

/* The caller must hold the exact scene's class owner for the duration of this
 * call. The scene lease prevents context cleanup during these no-draw calls. */
bool mc_client_nw_color_float_in_scene(mc_client_tessellator *o, float r, float g, float b,
                                       const mc_client_render_engine_classes *scene_classes,
                                       mc_client_graphics_error *e);
bool mc_client_nw_vertex_uv_in_scene(mc_client_tessellator *o, double x, double y, double z,
                                     double u, double v,
                                     const mc_client_render_engine_classes *scene_classes,
                                     mc_client_graphics_error *e);
#endif
