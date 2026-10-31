#ifndef MC_CLIENT_WORLD_RENDERER_TICK_H
#define MC_CLIENT_WORLD_RENDERER_TICK_H

#include "mc/client/world/entity_renderer.h"
#include "mc/client/world/held_item_tick.h"
#include "mc/client/world/renderer_rain.h"
#include "mc/core/math/classes.h"
#include "mc/world/runtime/brightness.h"

typedef struct mc_client_entity_renderer_tick_call mc_client_entity_renderer_tick_call;

typedef enum mc_client_renderer_tick_status {
    MC_CLIENT_RENDERER_TICK_OK,
    MC_CLIENT_RENDERER_TICK_ARGUMENT,
    MC_CLIENT_RENDERER_TICK_BUSY,
    MC_CLIENT_RENDERER_TICK_ALLOCATION,
    MC_CLIENT_RENDERER_TICK_SOURCE_NULL,
    MC_CLIENT_RENDERER_TICK_NATIVE_FAILED,
    MC_CLIENT_RENDERER_TICK_CHILD_FAILED,
    MC_CLIENT_RENDERER_TICK_UNSUPPORTED_FP
} mc_client_renderer_tick_status;

typedef struct mc_client_renderer_tick_error {
    mc_client_renderer_tick_status status;
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy;
    const mc_client_entity_renderer_error *owner;
    const mc_math_class_error *math;
    const mc_world_brightness_error *brightness;
    const mc_client_held_item_tick_error *held_item;
    const mc_client_renderer_rain_error *rain;
    mc_entity_status entity;
} mc_client_renderer_tick_error;

typedef struct mc_client_renderer_tick_state {
    bool active, source_returned, view_stored, brightness_returned, M_stored, l_stored;
    bool held_item_called, held_item_returned, rain_called, rain_returned;
    unsigned int source_pc, interpolation_stores;
    mc_client_source_px *owner;
    client_application *app;
    int32_t x, y, z, render_distance;
    float brightness, distance_blend, target, before_M, after_M;
    const mc_client_renderer_tick_error *failure;
    const mc_client_held_item_tick_state *held_item;
    const mc_client_renderer_rain_state *rain;
} mc_client_renderer_tick_state;

/* Actual px.a()V, distinct from picking a(F). It mutates the SAME canonical
 * interpolation fields, App.i, brightness, counter, RA and rain Random state.
 * Native allocation/ordinary PX retain precede effects. Math active use occurs
 * at the first reached floor call. Failed allocated calls remain inspectable,
 * are terminal and must be retired before another tick on this PX. Out starts
 * NULL; errors/outputs are disjoint. Keep the complete App/World/Player/Settings
 * graph live and exclusive on the video thread through retirement. Source view
 * publication has an independent App-owned hold, shared with World loading.
 * No frame drawing, World simulation or source shutdown is implied. */
mc_client_renderer_tick_status
mc_client_entity_renderer_tick_begin(mc_client_source_px *, mc_math_classes *,
                                     mc_client_entity_renderer_tick_call **owned_out,
                                     mc_client_renderer_tick_error *admission_error);
bool mc_client_entity_renderer_tick_inspect(const mc_client_entity_renderer_tick_call *,
                                            mc_client_renderer_tick_state *);
/* Checked native cleanup only, children before their parent occurrence. A
 * refusal preserves the remaining owner/diagnostic graph for retry. Successful
 * child release ends its borrowed state view; copied value errors survive.
 * App.i and
 * other completed stores survive; App view cleanup belongs to its slot owner. */
mc_client_renderer_tick_status
mc_client_entity_renderer_tick_retire(mc_client_entity_renderer_tick_call **,
                                      mc_client_renderer_tick_error *cleanup_error);

#endif
