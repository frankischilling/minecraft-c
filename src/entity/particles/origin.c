#include "mc/entity/particles/origin.h"

#include "../classes/classes_internal.h"
#include "../classes/memory_internal.h"

#include <string.h>

mc_entity_status mc_entity_particle_origin_store(mc_entity_classes *classes, unsigned int axis,
                                                 double value, mc_entity_error *error) {
    if (classes == NULL || classes->source != MC_ENTITY_CLIENT || axis > 2)
        return mc_entity_error_set(error, MC_ENTITY_INVALID_ARGUMENT,
                                   "particle camera-origin store arguments", 0);
    const mc_entity_status status = mc_entity_base_class_initialize(classes, error);
    if (status != MC_ENTITY_OK)
        return status;
    double *cell = axis == 0   ? &classes->particle_origin.l
                   : axis == 1 ? &classes->particle_origin.m
                               : &classes->particle_origin.n;
    memcpy(cell, &value, sizeof(value));
    return mc_entity_error_set(error, MC_ENTITY_OK, NULL, 0);
}

mc_entity_status mc_entity_particle_origin_read(const mc_entity_classes *classes,
                                                mc_particle_origin *out, mc_entity_error *error) {
    if (classes == NULL || classes->source != MC_ENTITY_CLIENT || out == NULL)
        return mc_entity_error_set(error, MC_ENTITY_INVALID_ARGUMENT,
                                   "particle camera-origin read arguments", 0);
    memcpy(out, &classes->particle_origin, sizeof(*out));
    return mc_entity_error_set(error, MC_ENTITY_OK, NULL, 0);
}
