#ifndef MC_ENTITY_MINECART_CLASSES_H
#define MC_ENTITY_MINECART_CLASSES_H

#include "mc/entity/classes/classes.h"

typedef struct mc_minecart_shape_array mc_minecart_shape_array;
typedef struct mc_minecart_row_array mc_minecart_row_array;
typedef struct mc_minecart_int_array mc_minecart_int_array;

/* Actual yl/pr.j graph. Identities are borrowed, values are passive copies.
 * COLD or failed-before-publication has j NULL and zero graph fields. */
typedef struct mc_minecart_class_snapshot {
    mc_entity_source source;
    mc_entity_class_phase phase;
    uint16_t bytecode;       /* Last completed initializer instruction. */
    mc_entity_error failure; /* First full initializer failure; neutral otherwise. */
    const mc_minecart_shape_array *j;
    size_t length;
    const mc_minecart_row_array *rows[10];
    size_t row_lengths[10];
    const mc_minecart_int_array *endpoints[10][2];
    size_t endpoint_lengths[10][2];
    int32_t values[10][2][3];
    size_t native_array_count, native_allocation_bytes;
} mc_minecart_class_snapshot;

/* Requires a live, exclusive class owner. No initialization, allocation,
 * profile access, mutation or hold transfer. Failure preserves out. */
mc_entity_status mc_minecart_class_get_snapshot(const mc_entity_classes *classes,
                                                mc_minecart_class_snapshot *out);

#endif
