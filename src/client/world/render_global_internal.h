#ifndef MC_CLIENT_WORLD_RENDER_GLOBAL_INTERNAL_H
#define MC_CLIENT_WORLD_RENDER_GLOBAL_INTERNAL_H

#include "../display/occlusion.h"
#include "../render/cx.h"
#include "../render_engine/ge.h"
#include "../render_engine/nw.h"
#include "mc/client/world/render_global.h"
#include "mc/core/java/java.h"

struct mc_client_render_global_error {
    mc_client_render_global_status status;
    const char *operation, *source_class, *source_method, *source_descriptor;
    unsigned int source_pc, enclosing_pc;
    bool native_policy, source_returned;
    mc_client_render_engine_status classes;
    mc_client_graphics_error graphics, cleanup_graphics;
    mc_client_cx_error cx;
    mc_client_display_error display, cleanup_display;
};

struct mc_client_render_batch {
    mc_client_render_batch_state state;
};

struct mc_client_render_global {
    mc_client_render_global_phase phase;
    size_t holds;
    bool active, scene_guard;
    mc_client_render_engine_classes *classes;
    mc_client_render_arena arena;
    mc_client_render_global_fields fields;
    mc_client_render_batch **pending_T;
    mc_client_render_batch *pending_batch;
    unsigned int constructed_batches, published_batches;
    mc_client_display *display; /* Borrowed through retained class/query context leases. */
    mc_client_display_query_names *queries;
    mc_client_tessellator *tessellator; /* Borrowed actual singleton. */
    mc_java_random *star_random;
    unsigned int last_store_pc, last_constructor_pc, star_pc;
    unsigned int star_attempts, accepted_stars, star_vertices, top_quads, bottom_quads;
    int32_t open_list;
    bool matrix_pushed, source_returned, queries_issued;
    mc_client_render_global_error failure, retirement;
};

mc_client_render_global_status mc_render_global_fail(mc_client_render_global_error *,
                                                     mc_client_render_global_status, const char *,
                                                     const char *, unsigned int,
                                                     bool native_policy);
bool mc_render_global_graphics(mc_client_render_global *, unsigned int enclosing_pc);
bool mc_render_global_display(mc_client_render_global *, unsigned int source_pc);
bool mc_render_global_fp(mc_client_render_global *, const char *method, unsigned int pc);
bool mc_render_global_stars(mc_client_render_global *);
bool mc_render_global_sky(mc_client_render_global *);
bool mc_render_batch_construct(mc_client_render_global *, mc_client_render_batch *,
                               unsigned int enclosing_pc);

#endif
