#ifndef MC_CLIENT_WORLD_LOAD_WORLD_H
#define MC_CLIENT_WORLD_LOAD_WORLD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct client_application client_application;
typedef struct mc_world_info_name mc_world_info_name;
typedef struct mc_world_store mc_world_store;
typedef struct mc_world_runtime mc_world_runtime;
typedef struct mc_entity mc_entity;
typedef struct mc_movement_input mc_movement_input;
typedef struct mc_client_world_load_classes mc_client_world_load_classes;
typedef struct mc_client_world_load_call mc_client_world_load_call;
typedef struct mc_client_world_prepare_call mc_client_world_prepare_call;
typedef struct mc_client_world_prepare_state mc_client_world_prepare_state;
typedef struct mc_client_save_error mc_client_save_error;
typedef struct mc_client_stats_error mc_client_stats_error;
typedef struct mc_client_stats_report mc_client_stats_report;
typedef struct mc_client_stats_file_error mc_client_stats_file_error;
typedef struct mc_client_loading_error mc_client_loading_error;
typedef struct mc_sound_error mc_sound_error;
typedef struct mc_world_storage_error mc_world_storage_error;
typedef struct mc_world_source_error mc_world_source_error;
typedef struct mc_world_saved_data_source_error mc_world_saved_data_source_error;
typedef struct mc_singleplayer_controller_error mc_singleplayer_controller_error;
typedef struct mc_player_spawn_error mc_player_spawn_error;
typedef struct mc_player_input_error mc_player_input_error;
typedef struct mc_movement_input_error mc_movement_input_error;
typedef struct mc_registry_error mc_registry_error;
typedef struct mc_client_render_global_world_state mc_client_render_global_world_state;
typedef struct mc_client_render_global_error mc_client_render_global_error;
typedef struct mc_client_entity_renderer_pick_error mc_client_entity_renderer_pick_error;
typedef struct mc_effect_renderer_error mc_effect_renderer_error;
typedef struct mc_world_player_load_error mc_world_player_load_error;

typedef enum mc_client_world_load_status {
    MC_CLIENT_WORLD_LOAD_OK,
    MC_CLIENT_WORLD_LOAD_ARGUMENT,
    MC_CLIENT_WORLD_LOAD_STATE,
    MC_CLIENT_WORLD_LOAD_BUSY,
    MC_CLIENT_WORLD_LOAD_LIMIT,
    MC_CLIENT_WORLD_LOAD_ALLOCATION,
    MC_CLIENT_WORLD_LOAD_NULL_REFERENCE,
    MC_CLIENT_WORLD_LOAD_CHILD_FAILED,
    MC_CLIENT_WORLD_LOAD_UNIMPLEMENTED,
    MC_CLIENT_WORLD_LOAD_CLEANUP_FAILED
} mc_client_world_load_status;

typedef struct mc_client_world_load_site {
    const char *class_name, *method, *descriptor;
    int pc;
} mc_client_world_load_site;

/* Full typed records are owned by this retained call or its retained child.
 * Child views remain borrowed; inspect the current state after a cleanup
 * attempt, since successful child retirement can consume an earlier view. */
typedef struct mc_client_world_load_error {
    mc_client_world_load_status status;
    const char *operation;
    mc_client_world_load_site site, caller, child;
    bool native_policy;
    const mc_client_save_error *save;
    const mc_client_stats_error *statistics;
    const mc_client_stats_report *statistics_report;
    const mc_client_stats_file_error *statistics_file;
    const mc_client_loading_error *loading;
    const mc_sound_error *sound;
    const mc_world_storage_error *world;
    const mc_world_source_error *source_store;
    const mc_world_saved_data_source_error *saved_data;
    const mc_singleplayer_controller_error *controller;
    const mc_player_spawn_error *spawn;
    const mc_player_input_error *player_input;
    const mc_movement_input_error *input;
    const mc_registry_error *registry;
    const mc_client_world_prepare_state *terrain;
    const mc_client_render_global_world_state *renderer;
    const mc_client_render_global_error *render_global;
    const mc_client_entity_renderer_pick_error *picking;
    const mc_effect_renderer_error *effects;
    const mc_world_player_load_error *player_load;
} mc_client_world_load_error;

typedef struct mc_client_world_load_state {
    bool active, retiring, source_returned;
    bool initial_detach_returned, conversion_checked, needs_conversion;
    bool handler_opened, world_constructed, world_published, player_published, input_published;
    bool initial_i_cleared, initial_h_cleared, initial_P_stored;
    bool statistics_first_returned, statistics_second_returned;
    bool renderer_returned, effects_returned, player_load_returned, view_published;
    size_t gc_requests, terrain_samples;
    int last_outer_pc, last_change_pc;
    int64_t seed;
    client_application *app;
    mc_world_info_name *folder, *display;
    mc_world_store *handler;
    mc_world_runtime *world;
    mc_entity *player;
    mc_entity *failed_player; /* Borrowed SAME candidate retained by the captured controller. */
    mc_movement_input *input;
    const mc_client_world_prepare_call *prepare;
    const mc_client_world_load_error *failure, *cleanup;
} mc_client_world_load_state;

/* One native association per completed App runtime. Bind its SAME GUI/loading
 * namespace. Text literals resolve lazily in the SAME actual MC class pool;
 * no source literal allocation, load, World,
 * Player, clock, File, GL or statistic operation occurs. Keep the App's startup
 * graph exclusive/live. Classes own an App screen-domain borrow until final
 * retirement, and cannot retire while an invocation still exists. */
mc_client_world_load_status
mc_client_world_load_classes_create(client_application *actual_completed_app,
                                    mc_client_world_load_classes **out);
mc_client_world_load_status
mc_client_world_load_classes_retire(mc_client_world_load_classes **slot);

/* Actual selected Minecraft.a(String,String,J), once. Arguments retain their
 * SAME nullable UTF16 Name identities, with no filesystem/text round trip.
 * Output receives the allocated retained call even when source/child work
 * fails; allocation/admission failure before a call exists preserves output.
 * The admitted selected-entry closure has no prior World/Player occurrences;
 * other reached branches stop explicitly instead of releasing unknown borrows.
 * A failure is terminal: inspect and retire, never replay its source effects.
 * Existing saved Worlds reach renderer/effect attachment, Player insertion and
 * view publication. New-World saving and centered providers remain explicit
 * reached dependencies. */
mc_client_world_load_status mc_client_world_load_begin(mc_client_world_load_classes *,
                                                       mc_world_info_name *folder,
                                                       mc_world_info_name *display, int64_t seed,
                                                       mc_client_world_load_call **out);
bool mc_client_world_load_inspect(const mc_client_world_load_call *,
                                  mc_client_world_load_state *out);

/* Checked native cleanup without source save/unload or rollback. Drain actual
 * retained loading/terrain calls, detach owned renderer/particle World users
 * and end owned App.h/i/f occurrences and failed controller candidates before
 * World/handler release. The input owner outlives World/Chunk Player memberships.
 * BUSY/error keeps
 * the call and every remaining slot; successful earlier consumption is final.
 * Keep App/controller/settings/Registry/Files/GUI dependencies alive throughout
 * retries. Native GC requests are advisory counters, not JVM execution. */
mc_client_world_load_status mc_client_world_load_retire(mc_client_world_load_call **slot);

#endif
