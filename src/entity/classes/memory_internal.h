#ifndef MC_ENTITY_CLASSES_MEMORY_INTERNAL_H
#define MC_ENTITY_CLASSES_MEMORY_INTERNAL_H

#include "mc/entity/classes/classes.h"

/* Shared error writer and class-ledger request/retire kernel. No Entity
 * layout, World/profile call or subtype callback. Caller supplies the site. */
mc_entity_status mc_entity_error_set(mc_entity_error *error, mc_entity_status status,
                                     const char *operation, uint16_t bytecode);
void *mc_entity_class_memory_allocate(mc_entity_classes *classes, size_t bytes,
                                      mc_entity_error *error, const char *operation,
                                      uint16_t bytecode);
void mc_entity_class_memory_retire(mc_entity_classes *classes, void *allocation, size_t bytes);

/* Entity counter active use, independent of Minecart's own initializer. */
mc_entity_status mc_entity_base_class_initialize(mc_entity_classes *classes,
                                                 mc_entity_error *error);

#endif
