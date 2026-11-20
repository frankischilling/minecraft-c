#ifndef MC_CLIENT_WORLD_RENDERER_FOCUS_H
#define MC_CLIENT_WORLD_RENDERER_FOCUS_H

#include "client/input/mouse_helper.h"
#include "mc/client/world/entity_renderer.h"
#include "mc/entity/movement/turn.h"

typedef struct mc_client_entity_renderer_focus_call mc_client_entity_renderer_focus_call;

typedef enum mc_client_renderer_focus_status {
    MC_CLIENT_RENDERER_FOCUS_OK,
    MC_CLIENT_RENDERER_FOCUS_ARGUMENT,
    MC_CLIENT_RENDERER_FOCUS_BUSY,
    MC_CLIENT_RENDERER_FOCUS_ALLOCATION,
    MC_CLIENT_RENDERER_FOCUS_SOURCE_NULL,
    MC_CLIENT_RENDERER_FOCUS_NATIVE_FAILED,
    MC_CLIENT_RENDERER_FOCUS_CHILD_FAILED,
    MC_CLIENT_RENDERER_FOCUS_UNSUPPORTED_FP
} mc_client_renderer_focus_status;

typedef struct mc_client_renderer_focus_error {
    mc_client_renderer_focus_status status;
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy;
    mc_client_entity_renderer_error owner;
    mc_client_mouse_helper_error mouse;
    mc_entity_status entity;
} mc_client_renderer_focus_error;

typedef struct mc_client_renderer_focus_state {
    bool active, segment_completed, focused, motion_called, motion_returned;
    bool sensitivity_read, invert_read, smooth_read, inverted, smooth;
    bool n_returned, o_returned, turn_called, turn_returned;
    unsigned int source_pc;
    int32_t dx, dy;
    float sensitivity, scale, gain, yaw_delta, pitch_delta;
    mc_client_source_px *owner;
    const mc_client_renderer_focus_error *failure;
} mc_client_renderer_focus_state;

/* Exact focused-input segment px.b(F)38..194. The enclosing frame performs
 * inactivity handling first and continues at194 only after OK. This segment
 * consumes actual vy.c() motion through the SAME SDK input, then uses the
 * canonical Settings, CU n/o and Player inherited sn.d(FF)V. It never pumps
 * an extra event, resets filters, scales by frame time or turns App.i instead
 * of App.h. A false App.N reads no input, Settings, filters or Player.
 *
 * Output starts NULL; owner/errors/output are disjoint. The full actual App
 * graph remains live and exclusive through retirement. Allocated failed calls
 * are terminal records: consumed deltas and completed CU/angle stores survive.
 * A retained PX occurrence and pending-call guard prevent replay on this PX
 * until checked native retirement. No complete px.b return is claimed. */
mc_client_renderer_focus_status
mc_client_entity_renderer_focus_begin(mc_client_source_px *, mc_client_sdk_input *,
                                      mc_client_entity_renderer_focus_call **owned_out,
                                      mc_client_renderer_focus_error *);
bool mc_client_entity_renderer_focus_inspect(const mc_client_entity_renderer_focus_call *,
                                             mc_client_renderer_focus_state *);
mc_client_renderer_focus_status
mc_client_entity_renderer_focus_retire(mc_client_entity_renderer_focus_call **,
                                       mc_client_renderer_focus_error *);

#endif
