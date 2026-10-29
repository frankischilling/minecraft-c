#ifndef MC_CLIENT_APPLICATION_STARTUP_INTERNAL_H
#define MC_CLIENT_APPLICATION_STARTUP_INTERNAL_H

#include "../application_state.h"
#include "application_startup.h"

typedef struct application_directory_builder {
    uint16_t *units;
    size_t length, capacity;
} application_directory_builder;

struct mc_client_application_startup {
    client_application *app;
    mc_client_startup_services services;
    mc_client_startup_options options;
    mc_client_startup_phase phase;
    bool active, run_J_store_completed, display_created;
    bool files_held, formats_held, inputs_held, settings_classes_held, icons_held;
    bool Z_stored, aa_stored, z_stored, D_stored, p_stored, ji_active_use_completed,
        appdata_present;
    int32_t last_startup_store, last_directory_pc;
    mc_client_display *display;
    mc_client_display_mode *mode;
    mc_java_file *directory_candidate, *saves_candidate;
    mc_client_save_format *format_candidate;
    mc_game_settings *settings_candidate;
    mc_client_texture_pack_list *pack_candidate;
    mc_client_render_engine *engine_candidate;
    mc_client_render_engine_classes *engine_classes; /* borrowed SAME App.s namespace */
    mc_client_default_font *default_font;
    mc_client_bitmap_font
        *font_candidate; /* Borrowed identity guard; actual App retained slot owns. */
    mc_client_loading_classes *font_classes; /* SAME actual App.s GUI namespace, borrowed. */
    mc_player_string
        *font_resource; /* Borrowed lazy MC literal occurrence, no extra source field. */
    bool q_stored;
    mc_client_source_px *entity_renderer_candidate;
    bool t_stored;
    mc_client_render_manager_classes *manager_classes;
    mc_client_render_manager *manager_capture;
    mc_client_manager_consumer manager_consumer;
    mc_client_held_item *manager_hand_candidate;
    bool manager_f_stored;
    int32_t last_manager_pc;
    mc_client_color_array *color_argument; /* One actual returned/incoming occurrence. */
    mc_client_color_array *color_results[MC_CLIENT_COLORIZER_COUNT]; /* Borrowed Engine.c traces. */
    mc_player_string *color_literals[MC_CLIENT_COLORIZER_COUNT]; /* Borrowed actual MC literals. */
    /* Successful native call completions; source return can precede cleanup refusal. */
    unsigned int color_lookups_completed, color_setters_completed;
    unsigned int last_color_lookup_pc, last_color_setter_pc;
    bool color_lookup_source_returned; /* Actual last lookup source progress, distinct native
                                          success. */
    bool colors_entered, color_metadata_retired, color_cleanup_latched;
    mc_client_color_lookup_error
        color_cleanup; /* First real native cleanup cause, no owner views. */
    mc_world_info_name *home, *appdata, *directory_child;
    mc_world_info_name *settings_error_line; /* Native diagnostic copy; never a source field. */
    mc_java_file *published_Z; /* Native identity guard, not another source field/hold. */
    application_directory_builder *builder;
    mc_client_startup_error failure, retirement;
    mc_client_startup_logo_call *logo;
    mc_client_startup_stats_call *statistics;
    mc_client_startup_input_call *input;
    mc_client_startup_graphics_call *graphics;
    mc_client_startup_effects_call *effects;
    mc_client_startup_rendering_call *rendering;
    struct mc_client_startup_resources_call *resources;
};

mc_client_startup_status application_startup_fail(mc_client_startup_error *,
                                                  mc_client_startup_status, const char *,
                                                  const char *, const char *, const char *,
                                                  unsigned int);
mc_client_startup_status application_startup_manager(mc_client_application_startup *);
mc_client_startup_status application_startup_entity_renderer(mc_client_application_startup *);
mc_client_startup_status application_startup_colors(mc_client_application_startup *);
/* Actual x()V callee over these real already-constructed owners. A direct
 * prepared-receiver control does not credit intervening a324..354 calls.
 * One retained call record; no source a749/run32 completion or event pump. */
mc_client_startup_status application_startup_logo(mc_client_application_startup *);
mc_client_startup_status application_startup_statistics(mc_client_application_startup *);
mc_client_startup_status application_startup_statistics_retire(mc_client_application_startup *);
mc_client_startup_status application_startup_input(mc_client_application_startup *);
mc_client_startup_status application_startup_input_retire(mc_client_application_startup *);
mc_client_startup_status application_startup_graphics(mc_client_application_startup *);
mc_client_startup_status application_startup_graphics_retire(mc_client_application_startup *);
mc_client_startup_status application_startup_effects(mc_client_application_startup *);
mc_client_startup_status application_startup_effects_retire(mc_client_application_startup *);
mc_client_startup_status application_startup_rendering(mc_client_application_startup *);
mc_client_startup_status application_startup_rendering_retire(mc_client_application_startup *);
mc_client_startup_status application_startup_resources(mc_client_application_startup *);
mc_client_startup_status application_startup_resources_retire(mc_client_application_startup *);
mc_client_startup_status application_startup_font(mc_client_application_startup *);
mc_client_startup_status application_startup_render_engine(mc_client_application_startup *);
mc_client_startup_status application_startup_pack(mc_client_application_startup *);
mc_client_startup_status application_startup_directory(mc_client_application_startup *);
mc_client_startup_status application_startup_title(mc_client_application_classes *,
                                                   mc_player_string **, mc_client_startup_error *);
void application_directory_builder_destroy(application_directory_builder **);
mc_client_startup_status application_startup_retain_settings_error(mc_client_application_startup *,
                                                                   mc_client_startup_error *);

#endif
