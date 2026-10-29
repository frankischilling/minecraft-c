#ifndef MC_CLIENT_GUI_DISPLAY_SCREEN_H
#define MC_CLIENT_GUI_DISPLAY_SCREEN_H

#include "../display/sdk_update.h"
#include "../input/mouse_helper.h"
#include "loading_screen.h"
#include "mc/client/gui/gui_ingame.h"
#include "mc/client/gui/scaled_resolution.h"
#include "mc/client/statistics/writer.h"
#include "source_screen_initialize.h"

typedef enum mc_client_display_screen_status {
    MC_CLIENT_DISPLAY_SCREEN_OK,
    MC_CLIENT_DISPLAY_SCREEN_ARGUMENT,
    MC_CLIENT_DISPLAY_SCREEN_BUSY,
    MC_CLIENT_DISPLAY_SCREEN_ALLOCATION,
    MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE,
    MC_CLIENT_DISPLAY_SCREEN_CHILD_FAILED,
    MC_CLIENT_DISPLAY_SCREEN_UNIMPLEMENTED
} mc_client_display_screen_status;

typedef struct mc_client_display_screen_call {
    bool active, source_returned, old_closed, stats_b_returned, stats_c_returned;
    bool messages_cleared, r_stored, focus_released, resolution_returned, initialized, w_stored;
    bool health_read, display_active, focus_stored, mouse_grabbed, focus_returned;
    int32_t player_health;
    unsigned int focus_pc;
    client_application *app; /* Stable caller borrow through retirement. */
    mc_client_source_da *argument, *target, *old_screen; /* Stable caller-owned source operands. */
    mc_client_source_fu *fallback; /* Actual new fu57, if the null/no-World branch is reached. */
    struct mc_client_display_screen_call *focus_display; /* Actual recursive a(NULL)29. */
    mc_client_display_screen_status status;
    const char *operation;
    mc_client_screen_site site, child;
    mc_client_stats_writer *stats_receiver;
    mc_client_stats_error statistics;
    mc_client_stats_report statistics_report;
    mc_client_screen_error screen;
    mc_client_screen_init_error initialization;
    mc_client_gui_ingame_error gui;
    mc_client_mouse_helper_error mouse;
    mc_client_sdk_update_error update;
    mc_client_loading_error resolution_error;
    mc_scaled_resolution resolution;
} mc_client_display_screen_call;

/* Minecraft.a(da), dispatched over the implemented da/fu classes. The caller
 * serializes App use and keeps App, argument, old r and the class namespace
 * live. A failed invocation retains its actual stores and child prefixes;
 * retirement performs no source close, save, initialization or field rollback.
 * Screen owners, including source initialization records, retire before App.
 * Healthy null-screen focus uses the actual Player/SDK and retained recursive
 * display call. Death-screen construction and Player key release still report
 * their actual reached dependencies. */
mc_client_display_screen_status mc_client_display_screen_run(client_application *,
                                                             mc_client_screen_classes *,
                                                             mc_client_source_da *,
                                                             mc_client_display_screen_call **);
/* Actual Minecraft.g(), starting with Display.isActive0. Uses the SAME focus
 * body as a(NULL), without an enclosing display call. Direct entry errors use
 * g0/24/29 and returns use g6/14/51; the retained recursive a(NULL)29 and its
 * children remain inspectable through focus_display. The caller keeps App and
 * this exact class namespace live and exclusive through checked retirement.
 * Out must start NULL. Retire with mc_client_display_screen_retire. */
mc_client_display_screen_status mc_client_display_focus_run(client_application *,
                                                            mc_client_screen_classes *,
                                                            mc_client_display_screen_call **);
mc_client_display_screen_status mc_client_display_screen_retire(mc_client_display_screen_call **);

#endif
