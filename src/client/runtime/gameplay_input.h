#ifndef MC_CLIENT_RUNTIME_GAMEPLAY_INPUT_H
#define MC_CLIENT_RUNTIME_GAMEPLAY_INPUT_H

#include "../entity/player_message.h"
#include "../gui/display_screen.h"
#include "../gui/source_screen_events.h"
#include "../settings/source_input.h"
#include "mc/client/controller/attack.h"
#include "mc/client/controller/mining.h"
#include "mc/client/controller/singleplayer_tick.h"
#include "mc/client/controller/use.h"
#include "mc/client/particles/effect_renderer.h"
#include "mc/client/settings/game_option.h"
#include "mc/client/world/entity_renderer.h"
#include "mc/entity/player/drop.h"
#include "mc/entity/player/input.h"

typedef struct mc_client_gameplay_input {
    bool active, returned, failed;
    mc_client_screen_transitions *transitions;
    unsigned int source_pc;
    const char *operation;
    mc_client_screen_site site;
    size_t mouse_events, keyboard_events, stale_mouse_events, hotbar_stores;
    size_t left_clicks, swing_calls, click_timer_stores, mining_starts, mining_progress_calls;
    size_t mined_blocks, hit_particles_constructed, hit_particles_appended;
    size_t right_clicks, block_uses, air_uses, placements_admitted, placed_blocks;
    size_t placement_sounds, used_statistics, stack_count_stores;
    size_t selected_slot_clears, equip_resets;
    size_t drop_calls, drop_publications, dropped_items;
    size_t attack_calls, attack_hits;
    mc_player_attack_call *attack_call;
    mc_player_attack_state attack_state;
    mc_player_attack_error attack_error, attack_cleanup;
    mc_player_drop_call *drop_call;
    mc_player_drop_state drop_state;
    mc_player_drop_error drop_error, drop_cleanup;
    mc_client_sdk_input_error input;
    mc_player_input_error player;
    mc_inventory_selection_error inventory;
    mc_game_settings_error settings;
    mc_game_settings_source_input_wheel_report wheel;
    mc_game_settings_option_action_report option;
    mc_singleplayer_tick_error controller;
    mc_singleplayer_mining_call *mining_call;
    mc_singleplayer_mining_state mining_state;
    mc_singleplayer_mining_error mining_error, mining_cleanup;
    mc_singleplayer_use_call *use_call;
    mc_singleplayer_use_state use_state;
    mc_singleplayer_use_error use_error, use_cleanup;
    mc_client_player_message_call *message_call;
    mc_client_player_message_state message_state;
    mc_client_player_message_error message_error, message_cleanup;
    size_t player_message_calls, player_message_returns;
    mc_item_stack *click_block_stack, *click_air_stack;
    mc_inventory_current_error current;
    mc_inventory_error slot;
    mc_client_entity_renderer_error equip;
    mc_player_mining_error swing;
    mc_client_effect_renderer *hit_renderer;
    bool hit_block_work; /* This caller obtained a newly allocated DN block receipt. */
    mc_effect_renderer_error hit_effect, hit_cleanup;
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
 * Empty-hand mining executes on the actual controller/World, including its
 * hit-particle call, plus actual Block/Stack use and the Dirt placement path.
 * Distinct tool/Block overrides, combat, inventory/chat
 * screens, fullscreen and sound reload retain their reached dependencies.
 * This segment alone does not establish a complete tick or playable frame. */
bool mc_client_gameplay_input_run(client_application *, mc_client_screen_transitions *,
                                  mc_client_gameplay_input *);
/* Native cleanup only; releases retained focus/monitor prefixes, never input
 * events or source field effects. Value diagnostics survive cleanup; owned
 * focus/monitor views end when released. Zero the retired record before
 * reusing it after a failure. A refusal preserves the remaining record. */
bool mc_client_gameplay_input_retire(mc_client_gameplay_input *);

#endif
