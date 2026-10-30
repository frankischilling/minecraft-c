#ifndef MC_CLIENT_WORLD_PRECIPITATION_H
#define MC_CLIENT_WORLD_PRECIPITATION_H

#include "mc/client/world/entity_renderer.h"
#include "mc/core/math/classes.h"
#include "mc/world/dimension/world_chunk_manager.h"
#include "mc/world/runtime/brightness.h"

typedef struct mc_client_precipitation_call mc_client_precipitation_call;

typedef enum mc_client_precipitation_status {
    MC_CLIENT_PRECIPITATION_OK,
    MC_CLIENT_PRECIPITATION_ARGUMENT,
    MC_CLIENT_PRECIPITATION_BUSY,
    MC_CLIENT_PRECIPITATION_ALLOCATION,
    MC_CLIENT_PRECIPITATION_SOURCE_NULL,
    MC_CLIENT_PRECIPITATION_CHILD_FAILED,
    MC_CLIENT_PRECIPITATION_UNSUPPORTED_FP
} mc_client_precipitation_status;

typedef struct mc_client_precipitation_error {
    mc_client_precipitation_status status;
    const char *operation;
    unsigned int source_pc;
    bool native_policy;
    mc_client_graphics_error graphics;
    mc_client_display_error display;
    mc_math_class_error math;
    mc_world_storage_error world;
    mc_world_query_error query;
    mc_world_brightness_error brightness;
    mc_world_chunk_manager_error biome;
    mc_entity_status entity;
    const struct mc_client_texture_lookup_error *texture;
} mc_client_precipitation_error;

typedef struct mc_client_precipitation_state {
    bool active, returned, strength_read, camera_captured, world_captured;
    bool snow_texture_bound, rain_texture_bound, biomes_returned, state_restored;
    unsigned int source_pc;
    float partial, strength;
    int32_t camera_x, camera_y, camera_z, eye_y, radius;
    int32_t x, z, lower, upper, brightness_y;
    size_t snow_cells, rain_cells, snow_draws, rain_draws;
    double render_x, render_y, render_z;
    mc_client_source_px *owner;
    mc_entity *camera;
    mc_world_runtime *world;
    const mc_biome_reference_array *biomes;
    const mc_client_precipitation_error *failure;
} mc_client_precipitation_state;

/* Actual px.c(F)V, including both precipitation traversals. Reads the current
 * App World for strength, then separately captures camera and World. Strength
 * <=0 returns18 before camera, Math, NW, Settings or Display work. NaN draws.
 * Calls use the genuine PX.J, biome array, World queries, NW and Display.
 * Keep the entire graph live/exclusive through checked retirement. Outputs
 * start NULL; an allocated failed call remains inspectable and terminal.
 * Retirement releases native rights only; source RNG/GL/NW effects survive. */
mc_client_precipitation_status mc_client_entity_renderer_precipitation_begin(
    mc_client_source_px *, mc_math_classes *, float partial,
    mc_client_precipitation_call **owned_out, mc_client_precipitation_error *);
bool mc_client_entity_renderer_precipitation_inspect(const mc_client_precipitation_call *,
                                                     mc_client_precipitation_state *);
mc_client_precipitation_status
mc_client_entity_renderer_precipitation_retire(mc_client_precipitation_call **,
                                               mc_client_precipitation_error *);

#endif
