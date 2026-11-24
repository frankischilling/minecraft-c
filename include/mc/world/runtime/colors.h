#ifndef MC_WORLD_RUNTIME_COLORS_H
#define MC_WORLD_RUNTIME_COLORS_H

#include "mc/core/math/classes.h"
#include "mc/entity/entity.h"
#include "mc/entity/vector/vector.h"
#include "mc/world/biome/sky_color.h"
#include "mc/world/dimension/world_chunk_manager.h"
#include "mc/world/runtime/environment.h"

typedef struct mc_world_color_site {
    const char *class_name, *method, *descriptor;
    unsigned int bytecode;
} mc_world_color_site;

typedef struct mc_world_color_error {
    mc_world_storage_status status;
    const char *operation;
    bool native_policy;
    mc_world_color_site site, child;
    mc_world_storage_error cause;
    mc_math_class_error math;
    mc_world_info_status info;
    mc_world_provider_error provider;
    mc_world_chunk_manager_error biome;
    mc_biome_sky_color_error sky;
    mc_entity_vector_error vector;
    mc_entity_status entity;
} mc_world_color_error;

/* Actual client fd.a(sn,F), d(F), c(F): sky, Dimension fog and cloud colors.
 * Read the actual World Info/Dimension, camera coordinates, fresh biome
 * managers, weather and lightning fields in source order. Math active use
 * occurs only where reached and must join the SAME World-bound trig table.
 * The sky temperature is the raw xv query, not its clamped Biome channel.
 * Source floating operations round individually; no detached RGB/time cache,
 * independent World/biome owner or replacement camera is created.
 *
 * Success transfers one native hold on a pooled Vec3 from this World's actual
 * Entity classes. Release through that same classes owner. Pool cursor/reset
 * behavior stays with the caller; a held result can still be overwritten by
 * later pool reuse. Out is unchanged on failure; reached class/biome/pool
 * effects and full child diagnostics remain. NULL source receivers and native
 * policy failures are distinct. Calls admit live client GAME World prefixes;
 * keep the complete actual graph live/exclusive and outputs/errors disjoint.
 * No renderer, weather tick, resource import or World construction is implied. */
mc_world_storage_status mc_world_runtime_sky_color(mc_world_runtime *, mc_entity *camera,
                                                   float partial_tick, mc_math_classes *,
                                                   mc_entity_vector **out, mc_world_color_error *);
mc_world_storage_status mc_world_runtime_fog_color(mc_world_runtime *, float partial_tick,
                                                   mc_math_classes *, mc_entity_vector **out,
                                                   mc_world_color_error *);
mc_world_storage_status mc_world_runtime_cloud_color(mc_world_runtime *, float partial_tick,
                                                     mc_math_classes *, mc_entity_vector **out,
                                                     mc_world_color_error *);

#endif
