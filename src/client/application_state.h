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
};

#endif
