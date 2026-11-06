#ifndef MC_INVENTORY_ERRORS_H
#define MC_INVENTORY_ERRORS_H

#include "mc/item/recipes/recipes.h"

typedef enum mc_inventory_status {
    MC_INVENTORY_OK = 0,
    MC_INVENTORY_INVALID_ARGUMENT,
    MC_INVENTORY_NULL_REFERENCE,
    MC_INVENTORY_INDEX_OUT_OF_BOUNDS,
    MC_INVENTORY_NEGATIVE_ARRAY_SIZE,
    MC_INVENTORY_LIMIT,
    MC_INVENTORY_ALLOCATION_FAILED,
    MC_INVENTORY_BUSY,
    MC_INVENTORY_STACK_FAILED,
    MC_INVENTORY_REGISTRY_FAILED,
    MC_INVENTORY_MATCH_FAILED,
    MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY
} mc_inventory_status;

typedef struct mc_inventory_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_inventory_site;

#define MC_INVENTORY_ERROR_FRAMES 8u

/* Reached source frames, outermost first, with complete native Stack/registry
 * causes. Slots and source fields already stored survive descendant failure. */
typedef struct mc_inventory_error {
    mc_inventory_status status;
    const char *operation;
    mc_inventory_site frames[MC_INVENTORY_ERROR_FRAMES];
    size_t frame_count;
    bool mutation_reached, notification_reached;
    mc_item_stack_status stack_status;
    mc_item_stack_error stack;
    mc_registry_status registry_status;
    mc_registry_error registry;
    int32_t matching_status;
} mc_inventory_error;

typedef struct mc_inventory_limits {
    size_t allocation_bytes, max_slots;
} mc_inventory_limits;

mc_inventory_limits mc_inventory_default_limits(void);

#endif
