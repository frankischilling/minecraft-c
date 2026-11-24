#ifndef MC_WORLD_PLANT_SUBSTRATE_INTERNAL_H
#define MC_WORLD_PLANT_SUBSTRATE_INTERNAL_H

#include "mc/block/registry/runtime.h"

typedef struct mc_world_plant_substrate_error {
    mc_registry_error registry;
    const char *class_name, *method, *operation;
    unsigned int pc;
} mc_world_plant_substrate_error;

/* Entered WB/NW virtual substrate body. The caller retains the actual Block
 * and its code Registry. No World, class-phase, ID or future static preflight
 * occurs here. Each static/array operation is reached in source order; failure
 * preserves out and retains its complete Registry cause and concrete site. */
mc_registry_status mc_world_plant_substrate(const mc_block *, int32_t id, bool *out,
                                            mc_world_plant_substrate_error *);

#endif
