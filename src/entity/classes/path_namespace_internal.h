#ifndef MC_ENTITY_CLASSES_PATH_NAMESPACE_INTERNAL_H
#define MC_ENTITY_CLASSES_PATH_NAMESPACE_INTERNAL_H

#include "mc/entity/classes/classes.h"

/* Native lifetime bookkeeping only. The actual class-memory allocator charges
 * every arena/finder request separately. Neither operation initializes source
 * classes, consumes entropy or owns a World. Valid close is infallible. */
mc_entity_status mc_entity_path_arena_open(mc_entity_classes *, mc_entity_error *);
void mc_entity_path_arena_close(mc_entity_classes *);

#endif
