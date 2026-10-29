#ifndef MC_CLIENT_GUI_SOURCE_SCREEN_EVENTS_H
#define MC_CLIENT_GUI_SOURCE_SCREEN_EVENTS_H

#include "../display/sdk_input.h"
#include "../render_engine/texture_lookup.h"
#include "mc/client/sound/sound_manager.h"
#include "source_screen.h"

typedef struct mc_client_screen_transitions mc_client_screen_transitions;

typedef enum mc_client_screen_action {
    MC_CLIENT_SCREEN_ACTION_NONE,
    MC_CLIENT_SCREEN_ACTION_OPTIONS,
    MC_CLIENT_SCREEN_ACTION_SINGLEPLAYER,
    MC_CLIENT_SCREEN_ACTION_MULTIPLAYER,
    MC_CLIENT_SCREEN_ACTION_TEXTURE_PACKS,
    MC_CLIENT_SCREEN_ACTION_QUIT,
    MC_CLIENT_SCREEN_ACTION_FULLSCREEN,
    MC_CLIENT_SCREEN_ACTION_CLOSE,
    MC_CLIENT_SCREEN_ACTION_CANCEL,
    MC_CLIENT_SCREEN_ACTION_SELECT_WORLD,
    MC_CLIENT_SCREEN_ACTION_CREATE_WORLD,
    MC_CLIENT_SCREEN_ACTION_RENAME_WORLD,
    MC_CLIENT_SCREEN_ACTION_DELETE_WORLD
} mc_client_screen_action;

typedef struct mc_client_screen_event_report {
    size_t mouse_events, keyboard_events, presses, releases, click_calls, actions;
    mc_client_screen_action action;
    const mc_client_source_ke *button; /* Borrowed through the screen's next mutation. */
    int32_t button_id, mouse_x, mouse_y, key;
    uint16_t character;
    mc_client_screen_site pending;
    bool source_returned, quit_returned;
} mc_client_screen_event_report;

typedef struct mc_client_screen_event_error {
    mc_client_screen_error screen;
    mc_client_sdk_input_error input;
    mc_sound_error sound;
    mc_sound_manager_report sound_report;
    int application_status;
    mc_client_texture_lookup_error lookup;
    mc_client_display_error display;
    mc_client_source_da *pending_receiver; /* Native monitor-recovery occurrence only. */
    bool monitor_lost;
} mc_client_screen_event_error;

/* Actual virtual a()V: fu increments its canonical i by binary32 +1; da is
 * the literal no-op. Input drain is the distinct da.e()V entry below. */
mc_client_screen_status mc_client_screen_tick(mc_client_source_da *,
                                              mc_client_screen_event_error *);
/* Actual du.a()V completes its empty List loop. A reached kw update remains
 * explicit. Native Minecraft.k order is da.e293, fresh r/h -> du.a310, then
 * fresh r -> da.a317. Screen tick and input drain are never interchangeable. */
mc_client_screen_status mc_client_screen_particles_tick(mc_client_source_du *,
                                                        mc_client_screen_event_error *);
/* Actual du.a(F)V performs lookup16 then ji.b(int) bind19 before its List loop,
 * including an empty List. Nonempty kw geometry remains an explicit frontier. */
mc_client_screen_status mc_client_screen_particles_draw(mc_client_source_du *, float partial_tick,
                                                        mc_client_screen_event_error *);
/* Uses the same SDK's Mouse.next until empty, then Keyboard.next until empty.
 * No SDL pump/poll or replacement input queue is introduced. On a reached
 * failure/dependency, later events remain unread. Report/error are required,
 * disjoint, reset on entry and borrowed views expire on owner mutation. */
mc_client_screen_status mc_client_screen_handle_input(mc_client_source_da *, mc_client_sdk_input *,
                                                      mc_client_screen_event_report *,
                                                      mc_client_screen_event_error *);
mc_client_screen_status mc_client_screen_handle_input_with_transitions(
    mc_client_source_da *, mc_client_sdk_input *, mc_client_screen_transitions *,
    mc_client_screen_event_report *, mc_client_screen_event_error *);
/* Complete native call cleanup after a monitor reacquisition failure. Reuse
 * error storage/retire the graph only after this consumes pending_receiver.
 * This performs no source input, action, initialization or field rollback. */
mc_client_screen_status mc_client_screen_event_retire(mc_client_screen_event_error *);
/* Prepared-current-event controls and source callees. They do not claim SDK
 * next/poll completion. Mouse scaling reads actual da.c/d and App.d/e, with
 * Java wrapping multiplication, integer division and inverted Y. */
mc_client_screen_status mc_client_screen_mouse_event(mc_client_source_da *,
                                                     const mc_client_sdk_mouse_event *,
                                                     mc_client_screen_event_report *,
                                                     mc_client_screen_event_error *);
mc_client_screen_status mc_client_screen_key_event(mc_client_source_da *,
                                                   const mc_client_sdk_key_event *,
                                                   mc_client_screen_event_report *,
                                                   mc_client_screen_event_error *);
/* Actual ke.c(Minecraft,II)Z ignores visibility h and tests enabled g with
 * source integer-add wrap at right/bottom bounds. App is not read by ke.c. */
mc_client_screen_status mc_client_screen_button_hit(mc_client_source_ke *, int32_t x, int32_t y,
                                                    bool *, mc_client_screen_error *);

/* Main-menu routes for options/world selection/server selection/texture packs
 * retain their exact unfinished source allocation site and return a pending
 * action, never a fabricated screen or World. Quit executes actual App.f(). */

#endif
