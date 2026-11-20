#ifndef MC_CLIENT_GUI_SOURCE_WORLD_SELECTION_DRAWING_H
#define MC_CLIENT_GUI_SOURCE_WORLD_SELECTION_DRAWING_H
#include "source_world_list.h"

typedef struct mc_client_world_selection_draw_error {
    mc_client_world_list_error list;
    size_t buttons_drawn;
    bool source_returned;
    mc_client_source_da *pending_receiver; /* Native monitor-recovery occurrence only. */
    bool monitor_lost;
} mc_client_world_selection_draw_error;

/* Actual rq.a(IIF): MG draw7 (including same SDK mouse query and synchronous
 * selection), title29, then canonical DA List button drawing36. The shared
 * GL3.3 canvas supplies projection/dirt/quad/glyph kernels only; it adds no FU
 * logo, splash, version or copyright. Source rows clip through the actual
 * visibility test and later dirt overlays, with no invented scissor/wheel.
 *
 * Caller/router retains this RQ and its same App/Font/resources through the
 * entire call, including synchronous screen changes. A brief native active
 * occurrence blocks retirement/reentry, without holding the screen monitor
 * across clocks, SDK/GL or concrete transition calls. Error is heap-owned,
 * mandatory, disjoint from all borrowed owners and callbacks. After a lost
 * monitor, retain this error and all dependencies until draw_retire succeeds. */
bool mc_client_source_world_selection_draw(mc_client_source_rq *, mc_gui_main_menu *,
                                           const mc_scaled_resolution *, mc_gui_mouse_position,
                                           float partial_tick, mc_client_sdk_input *,
                                           mc_gui_main_menu_millis_fn, void *clock_context,
                                           mc_client_screen_transitions *,
                                           mc_client_world_selection_draw_error *);
/* Retry native monitor recovery only; never replay drawing or a source call.
 * An executing draw refuses recovery until its final reacquisition has failed. */
mc_client_screen_status
mc_client_source_world_selection_draw_retire(mc_client_world_selection_draw_error *);
#endif
