#ifndef MC_CLIENT_WORLD_WORLD_FRAME_H
#define MC_CLIENT_WORLD_WORLD_FRAME_H

#include "mc/client/particles/render.h"
#include "mc/client/world/first_person_hand.h"
#include "mc/client/world/frustum.h"
#include "mc/client/world/frustum_culling.h"
#include "mc/client/world/picking.h"
#include "mc/client/world/precipitation.h"
#include "mc/client/world/render_global_clouds.h"
#include "mc/client/world/render_global_entities.h"
#include "mc/client/world/render_global_sky.h"
#include "mc/client/world/render_global_submit.h"
#include "mc/client/world/render_global_update.h"
#include "mc/client/world/renderer_camera.h"
#include "mc/client/world/renderer_fog.h"
#include "mc/client/world/renderer_fog_state.h"
#include "mc/core/math/classes.h"
#include "mc/entity/hit_result.h"
#include "mc/entity/movement/material.h"
#include "mc/inventory/player_current.h"
#include "nanotime.h"

typedef struct mc_client_world_frame_call mc_client_world_frame_call;
typedef struct mc_client_texture_lookup_error mc_client_texture_lookup_error;
typedef struct mc_client_lighting_error mc_client_lighting_error;

typedef enum mc_client_world_frame_status {
    MC_CLIENT_WORLD_FRAME_OK,
    MC_CLIENT_WORLD_FRAME_ARGUMENT,
    MC_CLIENT_WORLD_FRAME_BUSY,
    MC_CLIENT_WORLD_FRAME_ALLOCATION,
    MC_CLIENT_WORLD_FRAME_SOURCE_NULL,
    MC_CLIENT_WORLD_FRAME_NATIVE_FAILED,
    MC_CLIENT_WORLD_FRAME_CHILD_FAILED,
    MC_CLIENT_WORLD_FRAME_UNSUPPORTED_FP,
    MC_CLIENT_WORLD_FRAME_DEPENDENCY
} mc_client_world_frame_status;

typedef struct mc_client_world_frame_error {
    mc_client_world_frame_status status;
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy;
    const mc_client_entity_renderer_pick_error *picking;
    const mc_client_entity_renderer_error *owner;
    const mc_client_renderer_fog_error *fog;
    const mc_client_renderer_camera_error *camera;
    const mc_client_renderer_fog_state_error *fog_setup;
    const mc_client_render_global_sky_error *sky;
    const mc_client_render_global_clouds_error *clouds;
    const mc_client_culling_error *culling;
    const mc_client_render_global_error *update;
    const mc_client_render_global_error *submission;
    const mc_client_render_global_error *entities;
    const mc_effect_draw_error *particles;
    const mc_client_precipitation_error *precipitation;
    const mc_client_first_person_hand_error *hand;
    const mc_client_texture_lookup_error *texture;
    const mc_client_graphics_error *lighting;
    const mc_client_lighting_error *lighting_enable;
    const mc_entity_view_error *view;
    mc_platform_nanotime_status clock;
    mc_client_display_error display;
    mc_entity_status entity;
    mc_effect_renderer_status effects;
    mc_entity_material_error material;
    mc_registry_error registry;
    mc_inventory_current_error inventory;
    mc_entity_hit_error hit;
    mc_item_stack_status stack;
    mc_client_frustum_error frustum;
} mc_client_world_frame_error;

typedef struct mc_client_world_frame_state {
    bool active, returned, cull_enabled, depth_enabled, view_stored, picking_returned;
    bool provider_read, anaglyph_read, anaglyph, eye_stored, color_masked, viewport_set;
    bool fog_world_captured, fog_camera_captured, fog_distance_read, fog_blend_computed;
    bool fog_returned, buffers_cleared, post_fog_cull_enabled;
    bool camera_returned;
    bool frustum_returned, fog_distance_branch_read;
    bool sky_fog_returned, sky_returned, fog_enabled, terrain_fog_returned;
    bool ambient_occlusion_read, ambient_occlusion, smooth_shade_set;
    bool frustum_constructed, frustum_recentered, culling_returned;
    bool updates_returned, draw_fog_returned, terrain_texture_bound, lighting_disabled;
    bool terrain_returned, lighting_enabled, eye_position_returned, entities_returned;
    bool effects_captured, lit_particles_returned, particle_lighting_disabled;
    bool particle_fog_returned, sprite_particles_returned;
    bool first_hit_read, first_hit_present, first_water_read, first_water, first_player_read;
    bool first_player, translucent_fog_returned, translucent_texture_bound;
    bool fancy_graphics_read, fancy_graphics, translucent_ambient_read, translucent_ambient;
    bool translucent_color_masked, translucent_returned, translucent_anaglyph_read;
    bool translucent_anaglyph, translucent_color_restored, translucent_state_restored;
    bool translucent_replay_returned;
    bool selection_zoom_read, selection_player_read, selection_player, selection_hit_read;
    bool selection_hit_present, selection_water_read, selection_water;
    bool selection_alpha_disabled, selection_current_returned;
    bool precipitation_returned, weather_fog_disabled, cloud_fog_returned, cloud_fog_enabled;
    bool clouds_returned, post_cloud_fog_disabled, post_cloud_fog_returned;
    bool hand_zoom_read, hand_depth_cleared, hand_returned, tail_anaglyph_read, tail_anaglyph;
    int32_t terrain_count;
    int32_t translucent_count;
    size_t update_calls;
    int64_t last_clock, deadline_remaining;
    int32_t post_camera_render_distance;
    mc_client_frustum_state frustum;
    unsigned int source_pc, eye;
    float partial, fog_distance_blend;
    int32_t fog_render_distance, width, height;
    int64_t deadline;
    double render_x, render_y, render_z;
    mc_client_source_px *owner;
    client_application *app;
    mc_entity *camera;
    mc_world_runtime *world;
    struct mc_client_render_global *render_global;
    struct mc_client_effect_renderer *effects;
    const mc_client_world_frame_error *failure;
    const mc_client_entity_renderer_pick_state *picking;
    const mc_client_renderer_fog_state *fog;
    const mc_client_renderer_camera_state *camera_setup;
    const mc_client_renderer_fog_state_view *sky_fog, *terrain_fog;
    const mc_client_render_global_sky_state *sky;
    const mc_client_source_frustum_state *culler;
    const mc_client_culling_state *culling;
    const mc_client_render_global_update_state *update;
    const mc_client_render_global_submit_state *submission;
    const mc_client_render_global_entities_state *entities;
    const mc_effect_draw_state *lit_particles, *sprite_particles;
    const mc_client_render_global_submit_state *translucent_submission;
    const mc_client_render_global_submit_state *translucent_replay;
    const mc_client_lighting_error *standard_lighting;
    const mc_entity_vector *entity_eye;
    const mc_client_renderer_fog_state_view *draw_fog, *particle_fog, *translucent_fog;
    const mc_entity_hit_result *selection_hit;
    const mc_item_stack *selection_stack;
    const mc_client_precipitation_state *precipitation;
    const mc_client_renderer_fog_state_view *cloud_fog;
    const mc_client_renderer_fog_state_view *post_cloud_fog;
    const mc_client_render_global_clouds_state *clouds;
    const mc_client_first_person_hand_state *hand;
} mc_client_world_frame_state;

/* Actual px.a(FJ) World draw entry, invoked from px.b's World branch. Uses the
 * same Display, source view, picker, provider and PX class namespace; it never
 * invokes the separate inspection renderer. It retains reached source/GL
 * effects and exact child failures. Actual h(F) supplies World sky/fog colors,
 * canonical PX RGB stores and GL clear color before the reached buffer clear
 * and camera invoke. The canonical frustum then extracts those matrices. The
 * actual N captured at57 has its own native occurrence through cleanup; later
 * culling/update calls still read fresh App.g. Entity rendering receives that
 * captured N, the actual pooled eye and the actual SR. The FX captured at66
 * has an independent occurrence through both real DN draw entries. Completed
 * native children retire before a later same-owner call, retaining scalar
 * receipts and ending their borrowed views. A returning particle prefix
 * continues through translucent terrain, weather, clouds and the first-person
 * hand. A dry empty-hand nonanaglyph World draw can return; this does not
 * complete the enclosing PX frame's delay/HUD composition.
 *
 * App and its complete graph stay live/exclusive on the video thread through
 * retirement. Out starts NULL; an allocated terminal call is returned even on
 * failure. A failed call is inspected/retired, never replayed to resume work. */
mc_client_world_frame_status mc_client_entity_renderer_world_frame_begin(
    mc_client_source_px *, mc_math_classes *, float partial, int64_t deadline,
    mc_client_world_frame_call **owned_out, mc_client_world_frame_error *admission_error);
bool mc_client_entity_renderer_world_frame_inspect(const mc_client_world_frame_call *,
                                                   mc_client_world_frame_state *);
/* Native-only cleanup; no field rollback, GL restoration or source replay. */
mc_client_world_frame_status
mc_client_entity_renderer_world_frame_retire(mc_client_world_frame_call **,
                                             mc_client_world_frame_error *cleanup_error);

#endif
