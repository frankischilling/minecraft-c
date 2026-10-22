#ifndef MC_ENTITY_MINECART_CLASSES_INTERNAL_H
#define MC_ENTITY_MINECART_CLASSES_INTERNAL_H

#include "mc/entity/minecart/classes.h"

/* Each source newarray/anewarray is a distinct native owner. No interning or
 * flattened replacement of the source's 31 identities. */
struct mc_minecart_int_array {
    size_t length;
    int32_t elements[];
};

struct mc_minecart_row_array {
    size_t length;
    mc_minecart_int_array *elements[];
};

struct mc_minecart_shape_array {
    size_t length;
    mc_minecart_row_array *elements[];
};

/* mc_entity_classes library, not simulation. Initializes actual superclass
 * first, then every own allocation/store in source order, publishes j at439,
 * returns442. Failure latches once; only unpublished unreachable temporaries
 * retire. No Entity header, ID consumption, World/profile call or Math draw. */
mc_entity_status mc_minecart_class_initialize(mc_entity_classes *classes, mc_entity_error *error);

/* Sole caller is classes_free after its unchanged BUSY admission. No active
 * use, callback, initialization or live borrower/entity remains. */
void mc_minecart_class_dispose(mc_entity_classes *classes);

#endif
