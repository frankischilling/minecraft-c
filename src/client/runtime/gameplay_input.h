#ifndef MC_CLIENT_RUNTIME_GAMEPLAY_INPUT_H
#define MC_CLIENT_RUNTIME_GAMEPLAY_INPUT_H

#include "../gui/display_screen.h"
#include "../gui/source_screen_events.h"
#include "../settings/source_input.h"
#include "mc/client/controller/singleplayer_tick.h"
#include "mc/client/settings/game_option.h"
#include "mc/entity/player/input.h"

typedef struct mc_client_gameplay_input {
    bool active, returned, failed;
    unsigned int source_pc;
    const char *operation;
    mc_client_screen_site site;
    size_t mouse_events, keyboard_events, stale_mouse_events, hotbar_stores;
    mc_client_sdk_input_error input;
    mc_player_input_error player;
    mc_inventory_selection_error inventory;
    mc_game_settings_error settings;
    mc_game_settings_source_input_wheel_report wheel;
    mc_game_settings_option_action_report option;
    mc_singleplayer_tick_error controller;
    mc_client_screen_event_report screen_report;
    mc_client_screen_event_error screen;
    mc_client_display_screen_call *focus;
} mc_client_gameplay_input;

/* Execute Minecraft.k337..1073 on the actual App and its existing SDK queues.
 * The caller has already evaluated r==NULL || r.f. No event pump, World tick,
 * clock substitution or Player/input copy is introduced. This record starts
 * zeroed; after a terminal failure inspect it and retire before reuse. Reached
 * effects survive failure and unread SDK records remain in the same queues.
 * App, transitions and their whole graph stay live/exclusive on the video
 * thread through retirement. Report storage must be disjoint from that graph.
 * Mining, click/use, pause/inventory/chat screens, fullscreen and sound reload
 * retain their reached dependencies. The non-mining controller reset executes.
 * This segment alone does not establish a complete tick or playable frame. */
bool mc_client_gameplay_input_run(client_application *, mc_client_screen_transitions *,
                                  mc_client_gameplay_input *);
/* Native cleanup only; releases retained focus/monitor prefixes, never input
 * events or source field effects. Value diagnostics survive cleanup; owned
 * focus/monitor views end when released. Zero the retired record before
 * reusing it after a failure. A refusal preserves the remaining record. */
bool mc_client_gameplay_input_retire(mc_client_gameplay_input *);

#endif
