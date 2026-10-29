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
#include "startup/application_owner.h"

#include <stdatomic.h>

/* Source-only references whose later method bodies remain separate. */
typedef struct mc_client_singleplayer_controller mc_client_singleplayer_controller;
typedef struct mc_client_source_cx mc_client_source_cx;
typedef struct mc_client_render_global mc_client_render_global; /* actual n */
typedef struct mc_client_source_canvas mc_client_source_canvas;
typedef struct mc_client_source_applet mc_client_source_applet;
typedef struct mc_client_source_da mc_client_source_da;
typedef struct mc_client_source_px mc_client_source_px;
typedef struct mc_client_source_cz mc_client_source_cz;
typedef struct mc_client_source_uq mc_client_source_uq;
typedef struct mc_client_source_vf mc_client_source_vf;
typedef struct mc_client_source_vy mc_client_source_vy;
typedef struct mc_client_texture_pack_list mc_client_texture_pack_list;
typedef struct mc_client_source_nl mc_client_source_nl;
typedef struct mc_sound_manager mc_sound_manager;
typedef struct mc_client_lava_texture_effect mc_client_lava_texture_effect;
typedef struct mc_client_biped_owner mc_client_biped_owner;
typedef struct mc_client_application_construction mc_client_application_construction;

typedef struct mc_gui_main_menu mc_gui_main_menu;
typedef struct mc_client_game_directory mc_client_game_directory;
typedef struct mc_java_file mc_java_file;
typedef struct mc_client_application_startup mc_client_application_startup;
typedef struct mc_client_world_session mc_client_world_session;
typedef struct mc_client_world_view mc_client_world_view;
typedef struct mc_player_session_classes mc_player_session_classes;
typedef struct mc_player_session mc_player_session;
typedef struct mc_client_stats_writer mc_client_stats_writer;
typedef struct mc_client_water_texture_effect mc_client_water_texture_effect;
typedef struct mc_client_effect_renderer mc_client_effect_renderer;
typedef struct mc_client_render_engine mc_client_render_engine;
typedef struct mc_client_loading_screen mc_client_loading_screen;
typedef struct mc_client_bitmap_font mc_client_bitmap_font;

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
    mc_client_game_directory *diagnostic_game_directory; /* native frontend path policy */
    mc_java_file *game_directory;                        /* canonical actual Z:Ljava/io/File; */
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
    /* Actual s/q fields. Failed candidates are separate native holds;
     * no constructor-completion or J store is supplied by these slots. */
    mc_client_loading_screen *loading_screen, *loading_screen_retained;
    mc_client_bitmap_font *font_renderer, *font_renderer_retained;
    bool loading_screen_constructing, font_renderer_constructing;
    /* Actual source volatile J/o storage. Existing running/captured above
     * remain native component-loop/capture flags; they are not source mirrors.
     * Register this unique App lifetime before any atomic access. Native worker
     * holds require the caller to keep this actual storage live through join. */
    atomic_bool J, o;
    atomic_size_t timer_hack_holds;
    atomic_size_t source_static_holds;
    atomic_size_t callback_holds; /* Native Registry-owned kh borrows, not a Registry hold. */
    atomic_size_t effect_borrows; /* Native compass/watch App storage borrows. */
    atomic_size_t resource_thread_borrows; /* Native cz borrows through join and cleanup. */
    atomic_size_t screen_borrows; /* Native da call records and independently retained du. */
    mc_client_application_lifetime_phase native_lifetime_phase;
    /* Remaining actual Minecraft instance fields. Canonical fields above are
     * their sole T/f/j/k/p/q/s/u/z/Z/I/ad storage. Native display/options/loop
     * values never mirror these source fields. All start at actual defaults. */
    /* Actual c: the canonical implemented ob/os owner. Other concrete ob
     * classes require their own source dispatch; no diagnostic controller is adopted. */
    mc_client_singleplayer_controller *controller;
    bool Q, R;
    int32_t d, e;
    mc_client_source_cx *S;
    mc_client_render_global *g;
    mc_entity *h; /* dc LocalPlayer; distinct from i, published only later */
    mc_entity *i; /* ls LivingEntity view; no World/inspection inference */
    mc_player_string *l;
    mc_client_source_canvas *m;
    bool n;
    mc_client_source_da *r;
    mc_client_source_px *t;
    mc_client_source_cz *U;
    int32_t V, W, X, Y;
    mc_client_source_uq *v;
    bool w;
    mc_client_biped_owner *x;
    mc_client_source_vf *y;
    mc_client_source_applet *A;
    mc_sound_manager *B;
    mc_client_source_vy *C;
    mc_client_texture_pack_list *D;
    mc_client_source_nl *aa;
    mc_player_string *ab;
    int32_t ac;
    mc_client_lava_texture_effect *ae;
    mc_player_string *K;
    bool L;
    int64_t M;
    bool N;
    int32_t ag;
    bool O;
    int64_t P;
    int32_t ah;
    /* Native heap-owned attempt/diagnostics/services. This never replaces or
     * mirrors source field storage. Source views end on RETIRING. */
    mc_client_application_construction *construction;
    mc_client_application_constructor_phase constructor_phase;
    mc_client_application_classes *source_namespace;
    bool source_namespace_borrowed;
    /* Native startup-call lifetime; borrows this actual App until retirement. */
    mc_client_application_startup *startup;
    struct mc_client_screen_transitions
        *screen_transitions; /* Native graph lease, not a source field. */
};

#endif
