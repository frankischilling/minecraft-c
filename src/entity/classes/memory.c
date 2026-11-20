#include "classes_internal.h"
#include "memory_internal.h"

#include <stdlib.h>

mc_entity_status mc_entity_error_set(mc_entity_error *error, mc_entity_status status,
                                     const char *operation, uint16_t bytecode) {
    if (error != NULL)
        *error = (mc_entity_error){.status = status, .operation = operation, .bytecode = bytecode};
    return status;
}

void *mc_entity_class_memory_allocate(mc_entity_classes *classes, size_t bytes,
                                      mc_entity_error *error, const char *operation,
                                      uint16_t bytecode) {
    if (classes->allocated_bytes > classes->limits.max_allocation_bytes ||
        bytes > classes->limits.max_allocation_bytes - classes->allocated_bytes) {
        mc_entity_error_set(error, MC_ENTITY_LIMIT, operation, bytecode);
        return NULL;
    }
    void *storage = malloc(bytes);
    if (storage == NULL) {
        mc_entity_error_set(error, MC_ENTITY_ALLOCATION_FAILED, operation, bytecode);
        return NULL;
    }
    classes->allocated_bytes += bytes;
    return storage;
}

void mc_entity_class_memory_retire(mc_entity_classes *classes, void *storage, size_t bytes) {
    if (storage != NULL) {
        free(storage);
        classes->allocated_bytes -= bytes;
    }
}
