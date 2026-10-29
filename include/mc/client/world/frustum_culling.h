#ifndef MC_CLIENT_WORLD_FRUSTUM_CULLING_H
#define MC_CLIENT_WORLD_FRUSTUM_CULLING_H

#include "mc/client/world/frustum.h"
#include "mc/client/world/render_global.h"
#include "mc/entity/bounds/bounds.h"

typedef struct mc_client_source_frustum mc_client_source_frustum;
typedef struct mc_client_culling_call mc_client_culling_call;

typedef struct mc_client_culling_error {
    mc_client_frustum_status status;
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy;
    mc_client_frustum_error frustum;
    mc_client_model_status models;
    const mc_client_render_global_error *owner;
} mc_client_culling_error;

typedef struct mc_client_source_frustum_state {
    bool active, constructed;
    size_t holds;
    unsigned int source_pc;
    const mc_client_source_dr *a;
    double b, c, d;
    const mc_client_culling_error *failure;
} mc_client_source_frustum_state;

/* Actual sr constructor: invoke the SAME w.a at5, then publish returned dr at8.
 * The wrapper independently holds Models, keeping its static W arrays live.
 * A published failed constructor remains inspectable and retires without replay.
 * Out starts NULL; the complete graph is live/exclusive on the video thread. */
mc_client_frustum_status mc_client_source_frustum_begin(mc_client_render_classes *,
                                                        mc_client_render_engine_classes *,
                                                        mc_client_display *, mc_math_classes *,
                                                        mc_client_source_frustum **,
                                                        mc_client_culling_error *);
bool mc_client_source_frustum_inspect(const mc_client_source_frustum *,
                                      mc_client_source_frustum_state *);
/* Actual sr.a(DDD): raw ordered b2/c7/d13 stores, including nonfinite values. */
mc_client_frustum_status mc_client_source_frustum_recenter(mc_client_source_frustum *, double,
                                                           double, double,
                                                           mc_client_culling_error *);
/* Actual sr.a(DDDDDD) and sr.a(eq). Subtract the SAME live b/c/d values in
 * source order, then call actual dr.a. Null bounds fail at sr.a(eq):2.
 * All writable outputs/errors are disjoint; failure preserves ordinary out. */
mc_client_frustum_status mc_client_source_frustum_box(mc_client_source_frustum *, double, double,
                                                      double, double, double, double, bool *,
                                                      mc_client_culling_error *);
mc_client_frustum_status mc_client_source_frustum_bounds(mc_client_source_frustum *,
                                                         const mc_entity_bounds *, bool *,
                                                         mc_client_culling_error *);
mc_client_frustum_status mc_client_source_frustum_retire(mc_client_source_frustum **,
                                                         mc_client_culling_error *);

typedef struct mc_client_culling_state {
    bool active, returned;
    unsigned int source_pc;
    int32_t index, initial_j, final_j;
    size_t visited, empty_skips, throttle_skips, tested, stores;
    mc_client_render_global *owner;
    mc_client_source_frustum *frustum;
    const mc_client_source_dk *chunk;
    const mc_client_culling_error *failure;
} mc_client_culling_state;

/* Whole actual n.a(yn,F): reread live o at each source array access, execute
 * real dk.e E/p short circuit, throttle already-visible cells by (index+j)&15,
 * call sr.a on actual DK.v and only then store DK.o11. Wrap j++70 after the
 * whole loop returns. Partial effects survive errors; partial is unused.
 * The retained call independently holds N and sr. The caller keeps App and
 * dependencies live/exclusive through retirement. NULL sr is a real nullable
 * source argument and fails only if dk.a reaches its invokeinterface6.
 * Out starts NULL; every allocated terminal call is inspected/retired once. */
mc_client_frustum_status mc_client_render_global_cull_begin(mc_client_render_global *,
                                                            mc_client_source_frustum *, float,
                                                            mc_client_culling_call **,
                                                            mc_client_culling_error *);
bool mc_client_render_global_cull_inspect(const mc_client_culling_call *,
                                          mc_client_culling_state *);
mc_client_frustum_status mc_client_render_global_cull_retire(mc_client_culling_call **,
                                                             mc_client_culling_error *);

#endif
