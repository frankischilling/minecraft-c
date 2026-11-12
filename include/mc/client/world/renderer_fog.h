#ifndef MC_CLIENT_WORLD_RENDERER_FOG_H
#define MC_CLIENT_WORLD_RENDERER_FOG_H

#include "mc/client/world/entity_renderer.h"
#include "mc/entity/movement/material.h"
#include "mc/entity/vector/vector.h"
#include "mc/world/runtime/colors.h"

typedef struct mc_client_renderer_fog_call mc_client_renderer_fog_call;

typedef enum mc_client_renderer_fog_status {
    MC_CLIENT_RENDERER_FOG_OK,
    MC_CLIENT_RENDERER_FOG_ARGUMENT,
    MC_CLIENT_RENDERER_FOG_BUSY,
    MC_CLIENT_RENDERER_FOG_ALLOCATION,
    MC_CLIENT_RENDERER_FOG_SOURCE_NULL,
    MC_CLIENT_RENDERER_FOG_NATIVE_FAILED,
    MC_CLIENT_RENDERER_FOG_CHILD_FAILED,
    MC_CLIENT_RENDERER_FOG_UNSUPPORTED_FP
} mc_client_renderer_fog_status;

typedef struct mc_client_renderer_fog_error {
    mc_client_renderer_fog_status status;
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy;
    mc_world_color_error colors;
    mc_world_storage_error weather;
    mc_entity_material_error material;
    mc_registry_error registry;
    mc_client_display_error display;
    mc_entity_status entity;
} mc_client_renderer_fog_error;

typedef struct mc_client_renderer_fog_state {
    bool active, returned, world_captured, camera_captured, distance_read, blend_computed;
    bool sky_returned, fog_returned, rain_returned, thunder_returned, cloud_returned;
    bool water_tested, in_water, lava_tested, in_lava, brightness_applied;
    bool anaglyph_read, anaglyph, clear_color_set;
    unsigned int source_pc, last_store_pc;
    int32_t render_distance;
    float partial, distance_blend, sky_red, sky_green, sky_blue, rain, thunder, brightness;
    mc_client_source_px *owner;
    mc_world_runtime *world;
    mc_entity *camera, *sky_camera;
    const mc_entity_vector *sky, *fog, *cloud;
    const mc_client_renderer_fog_error *failure;
} mc_client_renderer_fog_state;

/* Passive views of actual px.h(F), owned by its retained enclosing World
 * frame. Pooled Vec holds do not prevent a later pool acquisition from
 * overwriting the same source object; no pool operation is inserted here. */
bool mc_client_entity_renderer_fog_inspect(const mc_client_renderer_fog_call *,
                                           mc_client_renderer_fog_state *);
mc_client_renderer_fog_status
mc_client_entity_renderer_fog_retire(mc_client_renderer_fog_call **,
                                     mc_client_renderer_fog_error *cleanup_error);

#endif
