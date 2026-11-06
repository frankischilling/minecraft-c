#ifndef MC_CLIENT_WORLD_RENDERER_CAMERA_H
#define MC_CLIENT_WORLD_RENDERER_CAMERA_H
#include "mc/client/world/entity_renderer.h"
#include "mc/core/math/classes.h"
#include "mc/entity/hit_result.h"
#include "mc/entity/movement/material.h"
#include "mc/entity/player/input.h"
#include "mc/world/runtime/raytrace.h"

typedef struct mc_client_renderer_camera_call mc_client_renderer_camera_call;

typedef enum mc_client_renderer_camera_status {
    MC_CLIENT_RENDERER_CAMERA_OK,
    MC_CLIENT_RENDERER_CAMERA_ARGUMENT,
    MC_CLIENT_RENDERER_CAMERA_BUSY,
    MC_CLIENT_RENDERER_CAMERA_ALLOCATION,
    MC_CLIENT_RENDERER_CAMERA_SOURCE_NULL,
    MC_CLIENT_RENDERER_CAMERA_NATIVE_FAILED,
    MC_CLIENT_RENDERER_CAMERA_CHILD_FAILED,
    MC_CLIENT_RENDERER_CAMERA_UNSUPPORTED_FP,
    MC_CLIENT_RENDERER_CAMERA_DEPENDENCY
} mc_client_renderer_camera_status;

typedef struct mc_client_renderer_camera_error {
    mc_client_renderer_camera_status status;
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy;
    mc_client_display_error display;
    mc_math_class_error math;
    mc_registry_error registry;
    mc_entity_status entity;
    mc_entity_material_error material;
    mc_player_error player;
    mc_world_storage_error world;
    mc_entity_geometry_error geometry;
    mc_entity_vector_error vector;
    mc_entity_hit_error hit;
    const mc_world_raytrace_error *ray;
} mc_client_renderer_camera_error;

typedef struct mc_client_renderer_camera_state {
    bool active, returned, k_stored, projection_reset, modelview_reset;
    bool fov_returned, hurt_returned, bob_returned, orient_returned, D_stored;
    bool zoomed, portal_applied, third_person, sleeping;
    unsigned int source_pc, transforms, ray_calls;
    float partial, fov, far_distance, eye_offset, portal;
    int32_t eye, width, height;
    double distance, cloud_x, cloud_y, cloud_z;
    mc_client_source_px *owner;
    const mc_client_renderer_camera_error *error;
} mc_client_renderer_camera_state;

bool mc_client_entity_renderer_camera_inspect(const mc_client_renderer_camera_call *,
                                              mc_client_renderer_camera_state *);
/* Checked native cleanup. Parent PX and complete App graph remain live and
 * exclusive until this child retires. Source/GL stores survive cleanup. */
mc_client_renderer_camera_status
mc_client_entity_renderer_camera_retire(mc_client_renderer_camera_call **,
                                        mc_client_renderer_camera_error *);
#endif
