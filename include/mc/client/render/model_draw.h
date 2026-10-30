#ifndef MC_CLIENT_RENDER_MODEL_DRAW_H
#define MC_CLIENT_RENDER_MODEL_DRAW_H
#include "client/render_engine/source.h"
#include "mc/client/render/model.h"
#include "mc/client/render_engine/render_engine.h"

typedef struct mc_client_model_draw_error {
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy, returned, compiled, pending_list;
    int32_t owned_list;
    unsigned int faces, vertices, source_pc;
    mc_client_graphics_error graphics;
    mc_client_display_error display;
    mc_entity_vector_error vector;
} mc_client_model_draw_error;

/* Actual ps.a(F), lazy ps.d(F) and tz.a(NW,F) for genuine box parts. The
 * complete model graph, graphics and Entity namespaces stay live/exclusive.
 * Source list, pool and matrix effects survive failure. Native cleanup aborts
 * only this call's unfinished definition; it never acknowledges glEndList. */
bool mc_client_model_part_draw(mc_client_model_part *, mc_client_render_engine_classes *,
                               mc_entity_classes *, mc_client_display *, float,
                               mc_client_model_draw_error *);
#endif
