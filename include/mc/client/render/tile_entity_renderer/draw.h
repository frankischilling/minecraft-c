#ifndef MC_CLIENT_RENDER_TILE_ENTITY_RENDERER_DRAW_H
#define MC_CLIENT_RENDER_TILE_ENTITY_RENDERER_DRAW_H

#include "mc/client/render/manager/draw.h"
#include "mc/client/render/tile_entity_renderer/tile_entity_renderer.h"
#include "mc/entity/entity.h"
#include "mc/world/runtime/brightness.h"

typedef struct mc_client_tile_renderer_draw_call mc_client_tile_renderer_draw_call;
typedef struct mc_entity_factory_error mc_entity_factory_error;
typedef struct mc_entity_position_rotation_error mc_entity_position_rotation_error;

typedef struct mc_client_tile_renderer_draw_limits {
    size_t max_bytes;
    mc_client_manager_draw_limits manager;
} mc_client_tile_renderer_draw_limits;

typedef struct mc_client_tile_renderer_draw_error {
    mc_client_tile_renderer_status status;
    const char *operation, *exception_class;
    mc_client_graphics_site site, parent, child;
    bool native_policy, source_exception;
    mc_client_tile_renderer_error owner;
    mc_client_display_error display;
    /* Borrow the same retained call until checked retirement succeeds. */
    const mc_world_brightness_error *brightness;
    const mc_entity_factory_error *factory;
    const mc_entity_error *entity;
    const mc_entity_position_rotation_error *pose;
    const mc_client_manager_error *manager_owner;
    const mc_client_manager_draw_error *manager;
} mc_client_tile_renderer_draw_error;

typedef struct mc_client_tile_renderer_draw_state {
    bool active, returned, retiring, coordinates_only;
    bool distance_returned, distance_culled, brightness_returned, color_returned;
    bool lookup_returned, renderer_returned, bridge_cast_returned;
    bool matrix_pushed, matrix_popped, cache_hit, cache_miss;
    bool factory_returned, cache_put_returned, world_rebound, pose_returned;
    bool manager_returned, manager_retired;
    unsigned int source_pc, gl_calls, factory_calls;
    double distance, x, y, z;
    float partial, brightness, rotation;
    size_t cache_entries, cache_capacity, allocation_bytes;
    uintptr_t receiver_identity, tile_entity_identity, renderer_identity;
    uintptr_t preview_identity, preview_world_identity;
    const char *renderer_class;
    const mc_entity_class_literal *preview_class;
    /* PS returns occur every draw; faces/vertices describe new PS.d geometry
     * compilation only and can be zero when those same lists are reused. */
    unsigned int model_parts, model_faces, model_vertices, model_compiled_parts;
    const mc_client_tile_renderer_draw_error *failure, *cleanup;
} mc_client_tile_renderer_draw_state;

mc_client_tile_renderer_draw_limits mc_client_tile_renderer_draw_default_limits(void);
/* The ordinary LL.a(OW,F) entry includes actual OW distance, World brightness,
 * color, fresh coordinates/static origins and the coordinates overload. Services
 * are the existing TH dependencies, admitted only when reached. Caller keeps
 * their whole graph live/exclusive on the video thread through retirement.
 * A successful native admission publishes one retained call even on a later
 * source failure. No cleanup resumes Source or balances a source GL prefix. */
mc_client_tile_renderer_status mc_client_tile_renderer_draw_begin(
    mc_client_tile_renderer_dispatcher *, mc_tile_entity *nullable_tile_entity, float partial,
    const mc_client_manager_draw_services *, const mc_client_tile_renderer_draw_limits *,
    mc_client_tile_renderer_draw_call **, mc_client_tile_renderer_draw_error *);
/* Actual LL.a(OW,DDDF): no distance, brightness, color or origin subtraction. */
mc_client_tile_renderer_status mc_client_tile_renderer_coordinates_draw_begin(
    mc_client_tile_renderer_dispatcher *, mc_tile_entity *nullable_tile_entity, double x, double y,
    double z, float partial, const mc_client_manager_draw_services *,
    const mc_client_tile_renderer_draw_limits *, mc_client_tile_renderer_draw_call **,
    mc_client_tile_renderer_draw_error *);
bool mc_client_tile_renderer_draw_inspect(const mc_client_tile_renderer_draw_call *,
                                          mc_client_tile_renderer_draw_state *);
mc_client_tile_renderer_status
mc_client_tile_renderer_draw_retire(mc_client_tile_renderer_draw_call **,
                                    mc_client_tile_renderer_draw_error *);

#endif
