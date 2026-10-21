#ifndef MC_ENTITY_BOUNDS_INTERNAL_H
#define MC_ENTITY_BOUNDS_INTERNAL_H

#include "../classes/classes_internal.h"
#include "mc/entity/bounds/bounds.h"

/* A pool reference and native holds are independent. Clearing the source List
 * may detach an object while actual source descendants still retain it. */
typedef struct mc_entity_bounds_object {
    mc_entity_bounds value;
    size_t holds;
    bool pooled;
    struct mc_entity_bounds_object *next;
} mc_entity_bounds_object;

/* Shared constructor active use, retaining its existing diagnostic prefix. */
mc_entity_status mc_entity_bounds_initialize(mc_entity_classes *classes, mc_entity_error *error);
/* Called only after classes_free has admitted zero native bounds holds. */
void mc_entity_bounds_dispose(mc_entity_classes *classes);

#endif
