#ifndef MC_CLIENT_WORLD_RENDERER_FOG_STATE_H
#define MC_CLIENT_WORLD_RENDERER_FOG_STATE_H

#include "mc/client/world/entity_renderer.h"
#include "mc/entity/movement/material.h"
#include "mc/world/dimension/world_provider.h"

typedef struct mc_client_renderer_fog_state_call mc_client_renderer_fog_state_call;

typedef enum mc_client_renderer_fog_state_status {
    MC_CLIENT_RENDERER_FOG_STATE_OK,
    MC_CLIENT_RENDERER_FOG_STATE_ARGUMENT,
    MC_CLIENT_RENDERER_FOG_STATE_BUSY,
    MC_CLIENT_RENDERER_FOG_STATE_ALLOCATION,
    MC_CLIENT_RENDERER_FOG_STATE_SOURCE_NULL,
    MC_CLIENT_RENDERER_FOG_STATE_NATIVE_FAILED,
    MC_CLIENT_RENDERER_FOG_STATE_CHILD_FAILED,
    MC_CLIENT_RENDERER_FOG_STATE_UNSUPPORTED_FP
} mc_client_renderer_fog_state_status;

typedef struct mc_client_renderer_fog_state_error {
    mc_client_renderer_fog_state_status status;
    const char *operation;
    mc_client_graphics_site site, parent, child;
    bool native_policy;
    mc_client_graphics_error graphics;
    mc_client_display_error display;
    mc_entity_material_error material;
    mc_registry_error registry;
    mc_entity_status entity;
    mc_world_storage_error world;
    mc_world_provider_status dimension;
} mc_client_renderer_fog_state_error;

typedef struct mc_client_renderer_fog_state_view {
    bool active, returned, camera_captured, color_buffer_returned;
    bool color_set, normal_set, white_set, mode_set, density_set;
    bool water_tested, in_water, lava_tested, in_lava, cloud;
    bool anaglyph_read, anaglyph, unused_rgb_computed;
    bool start_set, end_set, negative_pass_applied;
    bool capability_read, nv_fog_distance, radial_set;
    bool world_captured, dimension_read, dimension_override;
    bool color_material_enabled, color_material_selected;
    unsigned int source_pc, helper_pc, buffer_puts;
    int32_t pass, mode;
    float partial; /* Actual source argument is never loaded by a(IF). */
    float rgba[4], unused_rgb[3], density, start, end;
    mc_client_source_px *owner;
    mc_entity *camera;
    mc_world_runtime *world;
    const struct mc_client_direct_float_buffer *buffer;
    mc_client_direct_buffer_state buffer_state;
    const mc_client_renderer_fog_state_error *failure;
} mc_client_renderer_fog_state_view;

/* Passive views of the retained actual PX.a(IF) invocation. Its same parent
 * PX, App/class Registry/Settings/Display graph remains live and exclusive
 * through checked retirement. Earlier buffer and GL state effects survive
 * every failure; no source operation is replayed or rolled back. */
bool mc_client_entity_renderer_fog_state_inspect(const mc_client_renderer_fog_state_call *,
                                                 mc_client_renderer_fog_state_view *);
mc_client_renderer_fog_state_status
mc_client_entity_renderer_fog_state_retire(mc_client_renderer_fog_state_call **,
                                           mc_client_renderer_fog_state_error *cleanup_error);

#endif
