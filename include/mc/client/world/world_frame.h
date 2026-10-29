#ifndef MC_CLIENT_WORLD_WORLD_FRAME_H
#define MC_CLIENT_WORLD_WORLD_FRAME_H

#include "mc/client/world/picking.h"
#include "mc/core/math/classes.h"

typedef struct mc_client_world_frame_call mc_client_world_frame_call;

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
    mc_client_display_error display;
    mc_entity_status entity;
} mc_client_world_frame_error;

typedef struct mc_client_world_frame_state {
    bool active, returned, cull_enabled, depth_enabled, view_stored, picking_returned;
    bool provider_read, anaglyph_read, anaglyph, eye_stored, color_masked, viewport_set;
    bool fog_world_captured, fog_camera_captured, fog_distance_read, fog_blend_computed;
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
} mc_client_world_frame_state;

/* Actual px.a(FJ) World draw entry, invoked from px.b's World branch. Uses the
 * same Display, source view, picker, provider and PX class namespace; it never
 * invokes the separate inspection renderer. It retains reached source/GL
 * effects and exact child failures. The unfinished h(F) sky-color descendant
 * remains explicit; camera/frustum/terrain/HUD are not silently skipped.
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
