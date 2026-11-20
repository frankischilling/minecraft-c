#ifndef MC_CLIENT_WORLD_RENDERER_H
#define MC_CLIENT_WORLD_RENDERER_H

#include "display.h"
#include "mc/client/world/first_person.h"
#include "vertex.h"

typedef struct mc_client_world_renderer mc_client_world_renderer;
typedef struct mc_client_world_mesh mc_client_world_mesh;

typedef enum mc_client_world_fog_kind {
    MC_CLIENT_WORLD_FOG_NONE,
    MC_CLIENT_WORLD_FOG_LINEAR,
    MC_CLIENT_WORLD_FOG_EXPONENTIAL
} mc_client_world_fog_kind;

typedef struct mc_client_world_camera {
    double eye_x, eye_y, eye_z;
    float yaw, pitch, vertical_fov, near_clip, far_clip;
} mc_client_world_camera;

typedef struct mc_client_world_fog {
    mc_client_world_fog_kind kind;
    float red, green, blue, start, end, density;
    bool radial;
} mc_client_world_fog;

typedef struct mc_client_world_renderer_limits {
    size_t max_meshes, max_vertices_per_mesh, max_vertex_bytes;
} mc_client_world_renderer_limits;

/* Real OpenGL3.3 terrain pipeline. Borrow Display until retirement; textures
 * remain Display-owned. Input is an actual decoded terrain atlas. This owner
 * draws supplied geometry, without querying or mutating a World. Its native
 * camera/fog interface does not qualify original EntityRenderer correspondence. */
bool mc_client_world_renderer_create(mc_client_display *display, const mc_image_view *terrain,
                                     bool anaglyph, const mc_client_world_renderer_limits *limits,
                                     mc_client_world_renderer **out,
                                     mc_client_display_error *error);
mc_client_world_renderer_limits mc_client_world_renderer_default_limits(void);

/* Upload triangle vertices relative to a retained integer section origin.
 * Bounds and finite geometry are checked before GL allocation. Output remains
 * unchanged on failure. The renderer owns live meshes and releases them at
 * retirement; explicit mesh retirement consumes/nulls the caller pointer.
 * Mesh views expire at renderer retirement. No CPU vertex pointer is retained. */
bool mc_client_world_mesh_create(mc_client_world_renderer *renderer, int32_t origin_x,
                                 int32_t origin_y, int32_t origin_z,
                                 const mc_client_world_vertex *vertices, size_t vertex_count,
                                 mc_client_world_mesh **out, mc_client_display_error *error);
bool mc_client_world_mesh_retire(mc_client_world_mesh **mesh, mc_client_display_error *error);

/* Begin clears color/depth and derives perspective from current drawable size.
 * A zero drawable returns NOT_DRAWABLE and invalidates the prior World frame.
 * Draw uses camera-relative origins, nearest atlas sampling, depth LEQUAL and
 * source-alpha blending when selected. Alpha testing discards final alpha<=.1.
 * Callers submit opaque geometry before their ordered translucent geometry.
 * A later GUI begin establishes its own independent overlay state/depth. */
bool mc_client_world_renderer_begin(mc_client_world_renderer *renderer,
                                    const mc_client_world_camera *camera,
                                    const mc_client_world_fog *fog, mc_client_display_error *error);
/* Consume a completed selected source camera frame on this actual renderer.
 * Dimensions must be the current drawable dimensions used to build the frame;
 * caller rebuilds after resize. Preserve raw render origin separately from the
 * source modelview's height translation. No EntityRenderer constructor, fog,
 * texture registration, render tick or scene scheduling is synthesized. */
bool mc_client_world_renderer_begin_first_person(mc_client_world_renderer *renderer,
                                                 const mc_client_camera_frame *frame,
                                                 const mc_client_world_fog *fog,
                                                 mc_client_display_error *error);
bool mc_client_world_renderer_draw(mc_client_world_renderer *renderer,
                                   const mc_client_world_mesh *mesh, bool blend, bool alpha_test,
                                   bool depth_write, mc_client_display_error *error);
bool mc_client_world_renderer_retire(mc_client_world_renderer **renderer,
                                     mc_client_display_error *error);

/* SDL main/video thread and exclusive ownership throughout. Native geometry
 * limits cover submitted vertex bytes, not private driver allocation. Camera
 * values must be finite, fov in(0,180), and0<near<far. Fog parameters are finite;
 * linear end>start and exponential density>=0. Colors are not silently clamped. */

#endif
