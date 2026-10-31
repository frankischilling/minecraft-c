#ifndef MC_CLIENT_RENDER_LIVING_DRAW_INTERNAL_H
#define MC_CLIENT_RENDER_LIVING_DRAW_INTERNAL_H
#include "client/render_engine/image_download.h"
#include "client/render_engine/texture_lookup.h"
#include "client/startup/application_owner.h"
#include "entity/render/internal.h"
#include "mc/client/render/manager/draw.h"
#include "mc/world/runtime/brightness.h"
#include "mc/world/runtime/queries.h"
#include "model_runtime_internal.h"

typedef struct mc_client_render_manager_draw_call mc_client_render_manager_draw_call;

typedef struct mc_client_draw_scope {
    mc_client_render_manager_draw_call *owner;
    mc_client_model_runtime_context model;
    mc_client_application_classes *application_classes;
    size_t work, max_work;
    size_t shadow_cells, max_shadow_cells;
    size_t shadow_quads, max_shadow_quads;
} mc_client_draw_scope;

/* Native lifetime captures only; no source query, class initialization,
 * source store or replacement service. NULL captures are no-ops. */
bool mc_client_draw_capture_math(mc_client_draw_scope *, mc_client_manager_error *);
bool mc_client_draw_capture_graphics(mc_client_draw_scope *, mc_client_manager_error *);
bool mc_client_draw_capture_world(mc_client_draw_scope *, mc_world_runtime *,
                                  mc_client_manager_error *);
/* Consumes/nulls a genuine existing occurrence only after native ledger publication.
 * Failure preserves the slot; retired Worlds need no new source admission. */
bool mc_client_draw_adopt_world(mc_client_draw_scope *, mc_world_runtime **owned_slot,
                                mc_client_manager_error *);
bool mc_client_draw_capture_engine(mc_client_draw_scope *, mc_client_render_engine *,
                                   mc_client_manager_error *);
bool mc_client_draw_capture_string(mc_client_draw_scope *, const mc_player_string *,
                                   mc_client_manager_error *);
bool mc_client_draw_capture_settings(mc_client_draw_scope *, mc_game_settings *,
                                     mc_client_manager_error *);
bool mc_client_draw_pending_lookup(mc_client_draw_scope *, mc_client_render_engine *,
                                   bool pending_on_entry, mc_client_manager_error *);
bool mc_client_draw_pending_list(mc_client_draw_scope *, const mc_client_model_draw_error *,
                                 mc_client_manager_error *);
/* Passive native receipt admission, before the actual direct invocation. */
bool mc_client_draw_query_begin(mc_client_draw_scope *, const mc_client_manager_draw_query *,
                                size_t *index, mc_client_manager_error *);
void mc_client_draw_query_enter(mc_client_draw_scope *, size_t index);
void mc_client_draw_query_end(mc_client_draw_scope *, size_t index, bool invoked, bool returned,
                              uint32_t result_bits, bool native_policy, bool source_exception,
                              const char *exception_class);
bool mc_client_draw_entity_queries(mc_client_draw_scope *, mc_entity_render_error *,
                                   mc_client_manager_error *);
/* Actual canonical Minecraft.w; Root captures reached App/Settings owners. */
bool mc_client_draw_debug_flag(mc_client_draw_scope *, bool *, mc_client_manager_error *);

typedef enum mc_client_living_draw_status {
    MC_CLIENT_LIVING_DRAW_OK,
    MC_CLIENT_LIVING_DRAW_ARGUMENT,
    MC_CLIENT_LIVING_DRAW_STATE,
    MC_CLIENT_LIVING_DRAW_BUSY,
    MC_CLIENT_LIVING_DRAW_LIMIT,
    MC_CLIENT_LIVING_DRAW_SOURCE_EXCEPTION,
    MC_CLIENT_LIVING_DRAW_SOURCE_ERROR,
    MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
    MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED
} mc_client_living_draw_status;

typedef enum mc_client_living_draw_throwable {
    MC_CLIENT_LIVING_DRAW_NO_THROWABLE,
    MC_CLIENT_LIVING_DRAW_EXCEPTION,
    MC_CLIENT_LIVING_DRAW_ERROR,
    MC_CLIENT_LIVING_DRAW_UNCLASSIFIED_THROWABLE
} mc_client_living_draw_throwable;

typedef enum mc_client_living_draw_child {
    MC_CLIENT_LIVING_DRAW_CHILD_NONE,
    MC_CLIENT_LIVING_DRAW_CHILD_CAPTURE,
    MC_CLIENT_LIVING_DRAW_CHILD_ENTITY,
    MC_CLIENT_LIVING_DRAW_CHILD_MATH,
    MC_CLIENT_LIVING_DRAW_CHILD_MODEL,
    MC_CLIENT_LIVING_DRAW_CHILD_DISPLAY,
    MC_CLIENT_LIVING_DRAW_CHILD_GRAPHICS,
    MC_CLIENT_LIVING_DRAW_CHILD_SETTINGS,
    MC_CLIENT_LIVING_DRAW_CHILD_LOOKUP,
    MC_CLIENT_LIVING_DRAW_CHILD_DOWNLOAD,
    MC_CLIENT_LIVING_DRAW_CHILD_QUERY,
    MC_CLIENT_LIVING_DRAW_CHILD_BLOCK,
    MC_CLIENT_LIVING_DRAW_CHILD_BRIGHTNESS,
    MC_CLIENT_LIVING_DRAW_CHILD_REGISTRY
} mc_client_living_draw_child;

typedef struct mc_client_living_draw_block_error {
    mc_world_storage_error cause;
    mc_world_runtime_acquisition_error acquisition;
} mc_client_living_draw_block_error;

typedef union mc_client_living_draw_detail {
    mc_client_manager_error capture;
    mc_entity_render_error entity;
    mc_math_class_error math;
    mc_client_model_runtime_error model;
    mc_client_display_error display;
    mc_client_graphics_error graphics;
    mc_game_settings_error settings;
    mc_client_texture_lookup_error lookup;
    mc_client_download_texture_error download;
    mc_world_query_error query;
    mc_client_living_draw_block_error block;
    mc_world_brightness_error brightness;
    mc_registry_error registry;
} mc_client_living_draw_detail;

typedef struct mc_client_living_draw_caught {
    bool active;
    const char *operation, *exception_class;
    mc_client_graphics_site site, child;
    mc_client_living_draw_child selected_child;
    mc_client_living_draw_detail detail;
} mc_client_living_draw_caught;

#define MC_CLIENT_LIVING_DRAW_TEXTURE_CALLS 8u
#define MC_CLIENT_LIVING_DRAW_TEXTURE_KEY_UNITS 64u

/* Passive values from actual direct JI lookup/bind calls. Identities cannot
 * be dereferenced; the copied key does not borrow a source String buffer.
 * Skin download/fallback has its existing distinct child diagnostics. */
typedef struct mc_client_living_draw_texture_call {
    uintptr_t engine_identity, key_identity;
    mc_client_graphics_site caller, lookup_site, bind_site, failure_site;
    size_t key_length, key_stored;
    uint16_t key_units[MC_CLIENT_LIVING_DRAW_TEXTURE_KEY_UNITS];
    int32_t texture_id;
    bool key_null, key_copied, attempted, lookup_entered, lookup_returned;
    bool bind_attempted, bind_returned, returned, failed;
    bool native_policy, source_exception;
    const char *exception_class;
} mc_client_living_draw_texture_call;

typedef struct mc_client_living_draw_state {
    bool returned, primary_event_returned, primary_render_returned;
    bool pass_attempted[4], pass_result[4], pass_render_returned[4];
    bool rescale_disabled, cull_enabled, matrix_popped, debug_checked, debug;
    bool shadow_settings, shadow_started, shadow_returned, fire_checked, burning;
    unsigned int source_pc;
    int32_t pass, shadow_x, shadow_y, shadow_z, shadow_block, shadow_light;
    float body_yaw, head_yaw, pitch, age, limb_swing, limb_amount, scale, shadow_alpha;
    size_t shadow_block_queries, shadow_light_queries, shadow_normal_queries;
    size_t shadow_brightness_queries, shadow_quads, shadow_vertices;
    uintptr_t shadow_world_identity, last_quad_world_identity;
    mc_client_model_runtime_state primary_event, primary_render, pass_render[4];
    size_t texture_call_count;
    mc_client_living_draw_texture_call texture_calls[MC_CLIENT_LIVING_DRAW_TEXTURE_CALLS];
    bool flap_arguments_read, flap_math_returned;
    float flap_first, flap_second;
} mc_client_living_draw_state;

typedef struct mc_client_living_draw_error {
    mc_client_living_draw_status status;
    const char *operation, *exception_class;
    mc_client_graphics_site caller, parent, site, child;
    bool native_policy, source_exception;
    mc_client_living_draw_throwable throwable;
    mc_client_living_draw_child selected_child;
    mc_client_living_draw_detail detail;
    /* Native ledger/adoption failure stays separate from its earlier child. */
    mc_client_manager_error capture_failure;
    mc_client_living_draw_caught caught;
    mc_client_living_draw_state state;
} mc_client_living_draw_error;

void mc_client_living_draw_error_init(mc_client_living_draw_error *);
bool mc_client_living_draw_entered(mc_client_draw_scope *, mc_client_managed_renderer *,
                                   mc_entity *, double x, double y, double z, float yaw,
                                   float partial, mc_client_graphics_site caller,
                                   mc_client_living_draw_error *);
bool mc_client_entity_post_draw_entered(mc_client_draw_scope *, mc_client_managed_renderer *,
                                        mc_entity *, double x, double y, double z, float yaw,
                                        float partial, mc_client_graphics_site caller,
                                        mc_client_living_draw_error *);
#endif
