#ifndef MC_ENTITY_MINECART_INVENTORY_ERRORS_H
#define MC_ENTITY_MINECART_INVENTORY_ERRORS_H

#include "mc/entity/minecart/sites.h"
#include "mc/item/stack/stack.h"

typedef enum mc_minecart_inventory_status {
    MC_MINECART_INVENTORY_OK = 0,
    MC_MINECART_INVENTORY_INVALID_ARGUMENT,
    MC_MINECART_INVENTORY_NULL_REFERENCE,
    MC_MINECART_INVENTORY_INDEX_OUT_OF_BOUNDS,
    MC_MINECART_INVENTORY_LIMIT,
    MC_MINECART_INVENTORY_ALLOCATION_FAILED,
    MC_MINECART_INVENTORY_STACK_FAILED
} mc_minecart_inventory_status;

/* Explicit selection of the complete Stack cause. Native occurrence holds and
 * field accesses are not presented as entered Java Stack methods. */
typedef enum mc_minecart_inventory_stack_cause {
    MC_MINECART_INVENTORY_STACK_NONE = 0,
    MC_MINECART_INVENTORY_STACK_RETAIN,
    MC_MINECART_INVENTORY_STACK_FIELDS,
    MC_MINECART_INVENTORY_STACK_COUNT_STORE,
    MC_MINECART_INVENTORY_STACK_SPLIT,
    MC_MINECART_INVENTORY_STACK_NBT_CONSTRUCTOR
} mc_minecart_inventory_stack_cause;

/* Plain context can be carried by Entity NBT without duplicating its existing
 * full Stack/construction children. Slot is selected only after a source slot
 * is available; NBT's separate index remains the Items-list occurrence. */
typedef struct mc_minecart_inventory_context {
    mc_minecart_inventory_status status;
    const char *operation;
    bool slot_selected;
    int32_t slot;
    mc_minecart_site current;
    mc_minecart_inventory_stack_cause stack_cause;
    mc_minecart_site stack_split, stack_constructor, stack_read;
} mc_minecart_inventory_context;

typedef struct mc_minecart_inventory_error {
    mc_minecart_inventory_context context;
    mc_item_stack_error stack;
} mc_minecart_inventory_error;

const char *mc_minecart_inventory_status_string(mc_minecart_inventory_status status);

#endif
