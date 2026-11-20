#ifndef MC_CLIENT_WORLD_PICKING_H
#define MC_CLIENT_WORLD_PICKING_H

#include "mc/client/world/entity_renderer.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_math_classes mc_math_classes;
typedef struct mc_entity_vector mc_entity_vector;
typedef struct mc_entity_hit_result mc_entity_hit_result;
typedef struct mc_entity_view_error mc_entity_view_error;
typedef struct mc_entity_living_ray_state mc_entity_living_ray_state;
typedef struct mc_entity_geometry_error mc_entity_geometry_error;
typedef struct mc_entity_vector_error mc_entity_vector_error;
typedef struct mc_entity_bounds_error mc_entity_bounds_error;
typedef struct mc_entity_bounds_intercept_view mc_entity_bounds_intercept_view;
typedef struct mc_entity_hit_error mc_entity_hit_error;
typedef struct mc_math_class_error mc_math_class_error;
typedef struct mc_singleplayer_controller_error mc_singleplayer_controller_error;
typedef struct mc_world_nearby_error mc_world_nearby_error;
typedef struct mc_world_source_chunk_error mc_world_source_chunk_error;
typedef struct mc_client_entity_renderer_pick_call mc_client_entity_renderer_pick_call;

typedef enum mc_client_pick_status {
    MC_CLIENT_PICK_OK,
    MC_CLIENT_PICK_ARGUMENT,
    MC_CLIENT_PICK_STATE,
    MC_CLIENT_PICK_BUSY,
    MC_CLIENT_PICK_LIMIT,
    MC_CLIENT_PICK_ALLOCATION,
    MC_CLIENT_PICK_SOURCE_NULL_REFERENCE,
    MC_CLIENT_PICK_CHILD_FAILED,
    MC_CLIENT_PICK_UNIMPLEMENTED,
    MC_CLIENT_PICK_CLEANUP_FAILED
} mc_client_pick_status;

typedef struct mc_client_entity_renderer_pick_error {
    mc_client_pick_status status;
    const char *operation, *exception_class;
    bool native_policy;
    mc_client_graphics_site site, child;
    const mc_client_entity_renderer_error *owner;
    const mc_math_class_error *math;
    const mc_singleplayer_controller_error *controller;
    const mc_entity_error *entity;
    const mc_entity_view_error *view;
    const mc_entity_living_ray_state *ray;
    const mc_entity_geometry_error *geometry;
    const mc_entity_vector_error *vector;
    const mc_entity_bounds_error *bounds;
    const mc_entity_bounds_intercept_view *intercept;
    const mc_entity_hit_error *hit;
    const mc_world_nearby_error *nearby;
    const mc_world_source_chunk_error *source_chunk;
} mc_client_entity_renderer_pick_error;

typedef struct mc_client_entity_renderer_pick_state {
    bool active, retiring, source_returned;
    bool block_published, target_cleared, query_returned, entity_published;
    unsigned int source_pc;
    float partial;
    double block_reach, entity_reach, nearest_distance;
    int32_t index;
    size_t entities_visited;
    const mc_client_source_px *owner;
    const client_application *app;
    mc_entity_vector *origin, *look, *endpoint;
    const mc_entity *candidate, *selected;
    const mc_entity_hit_result *block_result, *result;
    const mc_client_entity_renderer_pick_error *failure, *cleanup;
} mc_client_entity_renderer_pick_state;

/* Actual px.a(F), synchronously once. The caller keeps the actual completed
 * App/PX graph exclusive and live; MathHelper is its SAME namespace. The call
 * retains native operands and every unfinished child. A published failed call
 * is inspected/retired, never replayed. A successful NULL result is distinct
 * from a failed source or native operation. Source m/App.y stores survive.
 * Out must start NULL and remain disjoint from all owner/error storage. */
mc_client_pick_status
mc_client_entity_renderer_pick_begin(mc_client_source_px *, mc_math_classes *, float partial,
                                     mc_client_entity_renderer_pick_call **out);
bool mc_client_entity_renderer_pick_inspect(const mc_client_entity_renderer_pick_call *,
                                            mc_client_entity_renderer_pick_state *out);
mc_client_pick_status
mc_client_entity_renderer_pick_retire(mc_client_entity_renderer_pick_call **owned);

/* Native terminal coordination, with no source picking/World/save replay.
 * End only this PX's owned m and App.y occurrences before World retirement.
 * Active calls/foreign PX holds or replaced source fields preserve remaining
 * slots and report BUSY. The actual fields are not rolled back. */
mc_client_pick_status
mc_client_entity_renderer_pick_detach(mc_client_source_px *,
                                      mc_client_entity_renderer_pick_error *error);

#endif
