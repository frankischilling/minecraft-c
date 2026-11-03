#ifndef MC_ENTITY_PARTICLES_DIGGING_COLOR_INTERNAL_H
#define MC_ENTITY_PARTICLES_DIGGING_COLOR_INTERNAL_H

#include "../../client/render_engine/colorizer.h"
#include "mc/entity/particles/particle.h"
#include "mc/world/dimension/world_chunk_manager.h"
#include "mc/world/runtime/blocks.h"

typedef enum mc_digging_color_child {
    MC_DIGGING_COLOR_NONE,
    MC_DIGGING_COLOR_METADATA,
    MC_DIGGING_COLOR_MANAGER,
    MC_DIGGING_COLOR_CLIMATE,
    MC_DIGGING_COLOR_CHANNEL,
    MC_DIGGING_COLOR_COLORIZER
} mc_digging_color_child;

/* One retained DN child, reused by its 64 sequential QM invocations. Only the
 * selected reached member is diagnostic; no successful large-frame reset or
 * per-particle allocation occurs. Nothing here owns an array/World/namespace.
 * The enclosing DN captures failed acquisition receipts before returning. */
typedef struct mc_digging_color_error {
    mc_digging_color_child child;
    bool reached, failed, acquisition_reached;
    unsigned int block_pc;
    int32_t value;
    mc_client_graphics_site site;
    mc_world_storage_error world;
    mc_world_runtime_acquisition_error acquisition;
    mc_world_manager_error manager;
    mc_world_chunk_manager_error climate;
    mc_client_colorizer_error colorizer;
} mc_digging_color_error;

mc_entity_status mc_digging_particle_tint_entered(mc_entity *, struct mc_client_render_engine *,
                                                  int32_t, int32_t, int32_t,
                                                  mc_digging_color_error *, mc_particle_error *);

#endif
