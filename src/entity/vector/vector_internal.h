#ifndef MC_ENTITY_VECTOR_INTERNAL_H
#define MC_ENTITY_VECTOR_INTERNAL_H

#include "../classes/classes_internal.h"
#include "mc/entity/vector/vector.h"

/* A pool reference and native holds are independent. Clearing the source List
 * may detach an object while actual source descendants still retain it. */
typedef struct mc_entity_vector_object {
    mc_entity_vector value;
    size_t holds;
    bool pooled;
    struct mc_entity_vector_object *next;
} mc_entity_vector_object;

/* Shared constructor active use, retaining its existing diagnostic prefix. */
mc_entity_status mc_entity_vector_initialize(mc_entity_classes *classes, mc_entity_error *error);
/* Called only after classes_free has admitted zero native vector holds. */
void mc_entity_vector_dispose(mc_entity_classes *classes);

/* Passive native provenance/hold query. Never dereferences an unrecognized
 * vector pointer and performs no active use, allocation or hold mutation. */
size_t mc_entity_vector_native_holds(const mc_entity_classes *classes,
                                     const mc_entity_vector *vector);

#endif
