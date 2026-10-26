#ifndef MC_CLIENT_APPLICATION_STATE_H
#define MC_CLIENT_APPLICATION_STATE_H
#include "application.h"
#include "application_access.h"
#include "display/display.h"
#include "mc/client/gui/scaled_resolution.h"
#include "mc/client/timer/timer.h"
#include "mc/resources/files/files.h"
#include "mc/world/runtime/entities.h"
#include "nanotime.h"
#include "resources/assets.h"

typedef struct mc_gui_main_menu mc_gui_main_menu;
typedef struct mc_client_game_directory mc_client_game_directory;
typedef struct mc_client_world_session mc_client_world_session;
typedef struct mc_client_world_view mc_client_world_view;
typedef struct mc_player_session_classes mc_player_session_classes;
typedef struct mc_player_session mc_player_session;
typedef struct mc_client_stats_writer mc_client_stats_writer;
typedef struct mc_client_water_texture_effect mc_client_water_texture_effect;
typedef struct mc_client_effect_renderer mc_client_effect_renderer;
typedef struct mc_client_render_engine mc_client_render_engine;

/* The actual application storage, extracted without a shadow representation.
 * Existing fields retain their order; notice/class lifetime fields are explicit. */
typedef struct client_button {
    int32_t id, x, y, width, height;
    mc_font_text label;
} client_button;

struct client_application {
    mc_client_application_options options;
    mc_client_display *display;
    mc_client_display_error display_error;
    mc_platform_nanotime clock;
    mc_timer *timer;
    mc_resource_files *files;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *registry;
    mc_entity_classes *entities;
    mc_world_runtime_classes *world_classes;
    mc_math_trig *trig;
    mc_client_render_classes *render_classes;
    mc_client_achievement_notice *achievement_notice;
    mc_client_world_session *world_session;
    mc_client_world_view *world_view;
    mc_client_assets *assets;
    mc_client_game_directory *game_directory;
    mc_game_settings *settings;
    mc_gui_main_menu *main_menu;
    mc_java_random title_random;
    mc_client_string splash;
    mc_scaled_resolution resolution;
    client_button buttons[5];
    int display_width, display_height, input_window_width, input_window_height;
    bool running, captured;
    uint32_t frames;
    /* Actual k/I fields on this complete application. Native retention is
     * separate so an allocated/failed xi is never published as completed I. */
    mc_player_session_classes *session_classes;
    mc_player_session *session;
    mc_client_stats_writer *stats_writer;
    mc_client_stats_writer *stats_writer_retained;
    bool statistics_active_use_completed;
    bool session_constructing, stats_writer_constructing, statistics_retiring, statistics_retired;
    mc_client_water_texture_effect *water_effect; /* actual ad, published at122 */
    mc_client_water_texture_effect *water_effect_retained;
    bool water_effect_constructing;
    /* Actual f/p/j slots. World and manager native holds are explicit; p is
     * borrowed from its concrete GL resource owner. No World is inferred from
     * a Player or the separate inspection-session wrapper. */
    mc_world_runtime *active_world;
    mc_client_render_engine *render_engine;
    mc_client_effect_renderer *effect_renderer, *effect_renderer_retained;
    bool effect_renderer_constructing;
};

#endif
