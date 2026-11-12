#ifndef MC_CLIENT_HELD_BOW_GEOMETRY_INTERNAL_H
#define MC_CLIENT_HELD_BOW_GEOMETRY_INTERNAL_H

#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/render_engine/nw.h"
#include "mc/client/render/manager/manager.h"

typedef struct mc_client_draw_scope mc_client_draw_scope;

/* Parent owns the entered RA and every context
 * occurrence. This fragment never obtains an icon or reports RA returned. */
typedef enum mc_held_bow_status {
    MC_HELD_BOW_OK,
    MC_HELD_BOW_ARGUMENT,
    MC_HELD_BOW_FP,
    MC_HELD_BOW_NULL_NW,
    MC_HELD_BOW_GRAPHICS,
    MC_HELD_BOW_DISPLAY,
    MC_HELD_BOW_CAPTURE_FAILED
} mc_held_bow_status;

/* Actual completed or failed NW draw boundaries. Fixed extents cover this
 * body's six groups, including quad conversion and inherited reserved words.
 * Source receivers/buffers remain borrowed under the retained Parent. */
typedef struct mc_held_bow_batch_receipt {
    bool attempted, returned, captured;
    mc_client_nw_fields before, after;
    size_t word_count;
    int32_t words[768];
} mc_held_bow_batch_receipt;

typedef struct mc_held_bow_state {
    mc_client_tessellator *captured_nw;
    mc_client_render_engine_classes *graphics;
    mc_client_display *display;
    int32_t icon, remainder16, quotient16, column_pixels, row_pixels;
    float u0, u1, v0, v1, span, thickness, fraction, strip_uv, strip_position;
    unsigned int source_pc, batch; /* batch is zero based. */
    int32_t strip;
    size_t begin_attempts, begin_returns, normal_attempts, normal_returns;
    size_t vertex_attempts, vertex_returns, draw_attempts, draw_returns;
    double last_vertex[5]; /* Actual evaluated x/y/z/u/v before NW invocation. */
    bool rescale_enabled, rescale_disabled, pop_returned, fragment_completed;
} mc_held_bow_state;

typedef struct mc_held_bow_error {
    mc_held_bow_status status;
    const char *operation;
    mc_client_graphics_site site; /* Actual RA caller operation. */
    mc_held_bow_state state;
    mc_client_graphics_error graphics;
    mc_client_display_error display;
    mc_client_manager_error capture_failure;
    mc_client_draw_scope *native_scope;
    mc_held_bow_batch_receipt batches[6];
    /* This is the only Source Throwable introduced by the fragment itself.
     * ARGUMENT/FP are native. GRAPHICS/DISPLAY keep their complete producer
     * classification: parent must project that selected child, not infer a
     * Java Throwable from the geometry status or its operation text. */
    bool source_null_invocation;
} mc_held_bow_error;

/* Caller has REALLY completed RA.a(LS,IZ)'s icon child130 and stored its int.
 * Real items atlas is already looked up/bound; the actual RA push0 remains.
 * captured_nw is the real earlier local124 (NULL is delayed to invocation268).
 * All nonnull contexts/owners are genuine, live, exclusive, same namespace,
 * and held by the parent through diagnostics and checked native retirement.
 * Error is fresh/disjoint/caller-owned (prefer retained parent storage).
 * No preflight reads the current NW singleton or substitutes it for the local.
 *
 * Success completes only this fragment through pop967. Parent may THEN reach
 * RA return970; its RA/App/Stack/Item/lookup/guard chain remains parent-owned.
 * Native work scope may be NULL for the independently retained first-person
 * RA parent; the same fixed six groups still execute without a TH call or
 * fabricated manager work. Nonnull scopes keep their exact reached quota.
 * Failure leaves real matrices, rescale, NW batch/backing/color/normal exactly
 * as the selected child left them. There is no abort/reset/pop/cleanup API. */
bool entity_renderer_held_bow_draw(mc_client_draw_scope *, int32_t actual_icon,
                                   mc_client_tessellator *captured_nw,
                                   mc_client_render_engine_classes *actual_graphics,
                                   mc_client_display *actual_display, mc_held_bow_error *);

/* Entered CPU geometry component of the same production groups. A genuine
 * graphics namespace and its real captured NW remain caller-owned; NULL NW
 * fails only at that selected group's begin invocation. This computes the
 * original icon coordinates and emits all vertices/normals of batch0..5 using
 * the SAME body as the complete GL route. It executes no GL, NW.draw, matrix
 * operation or source cleanup and leaves the actual NW batch open. Success
 * certifies only this entered component, never the surrounding RA return. */
bool entity_renderer_held_item_geometry_batch(int32_t actual_icon, unsigned int batch,
                                              mc_client_tessellator *captured_nw,
                                              mc_client_render_engine_classes *,
                                              mc_held_bow_error *);

#endif
