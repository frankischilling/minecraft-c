#ifndef MC_INVENTORY_ERRORS_H
#define MC_INVENTORY_ERRORS_H

#include "mc/item/recipes/recipes.h"

typedef struct mc_live_tile_entity_error mc_live_tile_entity_error;

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
    /* Borrowed retained Chest adapter failure; valid until that adapter's final
     * native release. Includes dirty/WorldAccess prefix ownership. */
    const mc_live_tile_entity_error *chest;
    const mc_live_tile_entity_error *furnace;
    /* Actual AZ failure ledger borrow. Explicit end_calls or the end of the
     * owning adapter occurrence requires typed expiry in diagnostic copies;
     * receipt-derived exception_class has the same lifetime. */
    const mc_live_tile_entity_error *dispenser;
    /* Value witness for the actual adapter diagnostic ledger, never an object
     * reference or ownership token. Copies acquire no producer occurrence. */
    uintptr_t dispenser_diagnostic_owner_id;
    /* True only when an actual WorldAccess receipt supplied exception_class.
     * Static null/index names remain false and survive diagnostic expiry. */
    bool exception_class_from_dispenser;
    bool source_exception;
    const char *exception_class;
} mc_inventory_error;

typedef struct mc_inventory_limits {
    size_t allocation_bytes, max_slots;
} mc_inventory_limits;

mc_inventory_limits mc_inventory_default_limits(void);

#endif
