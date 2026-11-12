#ifndef MC_CLIENT_WORLD_RENDERER_RAIN_H
#define MC_CLIENT_WORLD_RENDERER_RAIN_H

#include "mc/client/world/entity_renderer.h"
#include "mc/world/dimension/world_chunk_manager.h"
#include "mc/world/runtime/queries.h"

typedef struct mc_client_renderer_rain_call mc_client_renderer_rain_call;

typedef enum mc_client_renderer_rain_status {
    MC_CLIENT_RENDERER_RAIN_OK,
    MC_CLIENT_RENDERER_RAIN_ARGUMENT,
    MC_CLIENT_RENDERER_RAIN_BUSY,
    MC_CLIENT_RENDERER_RAIN_PHASE,
    MC_CLIENT_RENDERER_RAIN_ALLOCATION,
    MC_CLIENT_RENDERER_RAIN_SOURCE_NULL,
    MC_CLIENT_RENDERER_RAIN_CHILD_FAILED,
    MC_CLIENT_RENDERER_RAIN_UNSUPPORTED_FP,
    MC_CLIENT_RENDERER_RAIN_PARTICLE_DEPENDENCY,
    MC_CLIENT_RENDERER_RAIN_CLEANUP_FAILED
} mc_client_renderer_rain_status;

typedef struct mc_client_renderer_rain_error {
    mc_client_renderer_rain_status status;
    const char *operation;
    unsigned int source_pc;
    bool native_policy;
    const mc_world_storage_error *world;
    const mc_world_query_error *query;
    const mc_world_chunk_manager_error *biome;
    const mc_registry_error *registry;
    const mc_client_entity_renderer_error *owner;
    mc_entity_status entity;
} mc_client_renderer_rain_error;

typedef struct mc_client_renderer_rain_state {
    bool active, returned, strength_read, fancy_read, fancy, reseeded;
    unsigned int source_pc;
    float strength, adjusted_strength;
    int64_t seed;
    int32_t camera_x, camera_y, camera_z, loop_limit, iteration, completed_iterations;
    int32_t x, z, top, block_id, reservoir_count;
    float offset_x, offset_z;
    double sound_x, sound_y, sound_z;
    mc_client_source_px *owner;
    client_application *app;
    mc_entity *camera;
    mc_world_runtime *world;
    const mc_client_renderer_rain_error *failure;
} mc_client_renderer_rain_state;

/* Actual private px.c()V, not precipitation drawing c(F). The real PX.J is
 * reseeded and consumed in source order; the actual World, camera and biome
 * owners supply queries. Zero strength returns35 without reading J/camera.
 * Missing xn/xd particle allocation remains explicit at NEW280/387 after its
 * completed RNG/query/reservoir prefix. No artificial rainy success is supplied.
 * The owner and its complete graph remain live/exclusive through retirement.
 * A NULL output value is required; once allocated the call is returned even
 * on failure. Failed calls are terminal, inspected and retired, never replayed.
 * RAW/failed source prefixes are admitted; construction and retirement reject
 * native reentry. A normal runtime still uses its genuine constructed PX.
 * Native errors are not fabricated Java Throwables. Outputs are disjoint. */
mc_client_renderer_rain_status
mc_client_entity_renderer_rain_tick_begin(mc_client_source_px *,
                                          mc_client_renderer_rain_call **owned_out,
                                          mc_client_renderer_rain_error *admission_error);
bool mc_client_entity_renderer_rain_tick_inspect(const mc_client_renderer_rain_call *,
                                                 mc_client_renderer_rain_state *);
/* Native-only checked cleanup; it never restores J/K or source fields. */
mc_client_renderer_rain_status
mc_client_entity_renderer_rain_tick_retire(mc_client_renderer_rain_call **,
                                           mc_client_renderer_rain_error *cleanup_error);

#endif
