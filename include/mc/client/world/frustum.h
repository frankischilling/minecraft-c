#ifndef MC_CLIENT_WORLD_FRUSTUM_H
#define MC_CLIENT_WORLD_FRUSTUM_H

#include "client/render_engine/source.h"
#include "mc/client/render/model.h"
#include "mc/client/render_engine/render_engine.h"
#include "mc/core/math/classes.h"

typedef struct mc_client_source_dr mc_client_source_dr;

typedef enum mc_client_frustum_status {
    MC_CLIENT_FRUSTUM_OK,
    MC_CLIENT_FRUSTUM_ARGUMENT,
    MC_CLIENT_FRUSTUM_BUSY,
    MC_CLIENT_FRUSTUM_LIMIT,
    MC_CLIENT_FRUSTUM_ALLOCATION,
    MC_CLIENT_FRUSTUM_CLASS_FAILED,
    MC_CLIENT_FRUSTUM_SOURCE_NULL,
    MC_CLIENT_FRUSTUM_CHILD_FAILED,
    MC_CLIENT_FRUSTUM_UNSUPPORTED_FP,
    MC_CLIENT_FRUSTUM_NATIVE_FAILED
} mc_client_frustum_status;

typedef struct mc_client_frustum_error {
    mc_client_frustum_status status;
    const char *operation;
    mc_client_graphics_site site, parent;
    bool native_policy;
    mc_client_graphics_error graphics;
    mc_math_class_error math;
} mc_client_frustum_error;

typedef struct mc_client_frustum_state {
    bool initialized, failed, active, static_published, returned;
    unsigned int last_store_pc, matrix_stores, plane_stores, normalized_planes;
    const char *last_store_method, *last_store_class;
    size_t bytes, allocations;
    const mc_client_source_dr *singleton, *candidate;
    const float *planes[16];
    const float *projection, *modelview, *combined;
    mc_client_direct_buffer_state buffers[3];
    const mc_client_frustum_error *failure;
} mc_client_frustum_state;

/* Actual w.a(): one canonical RenderModels static w.e, real dr arrays and
 * ge FloatBuffers, ordered matrix extraction/combination and six normalized
 * planes. The GL3.3 bridge reads the canonical Display camera ledger at the
 * two source GL matrix queries. No reset, frame work or diagnostic frustum
 * is substituted. Source buffer positions and all prior writes survive
 * failures. A successful returned dr is borrowed until namespace retirement;
 * its shared arrays change on the next successful or partial update.
 *
 * First active use retains the SAME graphics namespace for the static ge
 * buffers. Native singleton allocation uses the supplied graphics owner's
 * constructor limits in a separate explicit arena. Keep the same Display,
 * Models/Graphics/Math graph exclusive and live through each call. MathHelper
 * initialization is reached only at the first normalization35. Disjoint
 * outputs/errors are required; ordinary out is preserved on failure. */
mc_client_frustum_status mc_client_frustum_update(mc_client_render_classes *,
                                                  mc_client_render_engine_classes *,
                                                  mc_client_display *, mc_math_classes *,
                                                  const mc_client_source_dr **borrowed_out,
                                                  mc_client_frustum_error *);
bool mc_client_frustum_inspect(const mc_client_render_classes *, mc_client_frustum_state *);
/* Exact dr.a(DDDDDD): ordered eight-corner >0 tests for each of six planes. */
mc_client_frustum_status mc_client_frustum_box(const mc_client_source_dr *, double min_x,
                                               double min_y, double min_z, double max_x,
                                               double max_y, double max_z, bool *out,
                                               mc_client_frustum_error *);
/* Explicit terminal native cleanup after all Models owner occurrences end,
 * before final Graphics/Models namespace destruction. No source static reset
 * or class reinitialization; failed constructor arenas are reclaimed too. */
mc_client_frustum_status
mc_client_frustum_namespace_retire(mc_client_render_classes *,
                                   mc_client_frustum_error *nullable_error);

#endif
