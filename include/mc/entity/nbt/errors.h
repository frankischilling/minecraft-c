#ifndef MC_ENTITY_NBT_ERRORS_H
#define MC_ENTITY_NBT_ERRORS_H

#include "mc/entity/errors.h"
#include "mc/entity/minecart/inventory_errors.h"
#include "mc/entity/registry/registry.h"
#include "mc/inventory/runtime/nbt.h"
#include "mc/storage/nbt/nbt.h"

#include <stddef.h>
#include <stdint.h>

typedef enum mc_entity_nbt_status {
    MC_ENTITY_NBT_OK = 0,
    MC_ENTITY_NBT_INVALID_ARGUMENT,
    MC_ENTITY_NBT_NULL_REFERENCE,
    MC_ENTITY_NBT_WRONG_TAG,
    MC_ENTITY_NBT_INDEX_OUT_OF_BOUNDS,
    MC_ENTITY_NBT_LIMIT,
    MC_ENTITY_NBT_ALLOCATION_FAILED,
    MC_ENTITY_NBT_ENTITY_FAILED,
    MC_ENTITY_NBT_STACK_FAILED,
    MC_ENTITY_NBT_NBT_FAILED,
    MC_ENTITY_NBT_UNSUPPORTED_FP,
    MC_ENTITY_NBT_UNSUPPORTED_TYPE,
    MC_ENTITY_NBT_REGISTRY_FAILED
} mc_entity_nbt_status;

typedef enum mc_entity_nbt_late_hook {
    MC_ENTITY_NBT_LATE_NONE = 0,
    MC_ENTITY_NBT_LATE_CART,
    MC_ENTITY_NBT_LATE_PLAYER
} mc_entity_nbt_late_hook;

/* Passive per-attempt frames. Zero PC normally denotes native admission or
 * unreached work; initializer0 is also the genuine first namespace allocation.
 * Use status/operation and the whole Registry cause to distinguish that site.
 * nested source calls retain their enclosing and child PCs independently.
 * index is SIZE_MAX unless a vector/List occurrence is selected. field and
 * operation are static ASCII strings, never borrowed input-tree storage.
 * The construction, Stack, namespace and codec causes remain whole values.
 * name1 denotes the final class-name wrapper's active use; name7 is reached
 * only after namespace initialization and the actual Class Map lookup. */
typedef struct mc_entity_nbt_error {
    mc_entity_nbt_status status;
    mc_entity_nbt_late_hook late_hook;
    const char *operation, *field;
    size_t index;
    unsigned int wrapper_bytecode, name_bytecode, factory_bytecode;
    unsigned int base_bytecode, item_bytecode, falling_bytecode, stack_bytecode;
    /* Actual dc -> gs -> ls hook frames, independent of the base wrapper. */
    unsigned int local_player_bytecode, player_bytecode;
    unsigned int initializer_bytecode, registration_bytecode;
    mc_entity_error construction;
    mc_item_stack_error stack;
    mc_nbt_error nbt;
    mc_entity_registry_error registry;

    /* Concrete late hook selections; index is the Items list occurrence,
     * while inventory.slot is meaningful only when slot_selected is true.
     * Complete selected lower errors reuse construction/stack above. */
    /* NONE exposes the legacy fresh Cart defaults. CART and PLAYER expose
     * only their complete selected arm; check late_hook before reading it.
     * Common/base/namespace/Stack/codec fields above remain independent. */
    union {
        struct {
            mc_minecart_site minecart_read, minecart_write;
            mc_minecart_inventory_context inventory;
        };

        struct {
            unsigned int living_bytecode;
            mc_inventory_nbt_error player_inventory;
        };
    };
} mc_entity_nbt_error;

/* Passive in-place initialization; NULL is a no-op. */
void mc_entity_nbt_error_init(mc_entity_nbt_error *error);
/* Passive concrete-detail selection. Repeated selection preserves the active
 * child; changing arms initializes that entire selected payload. Common
 * diagnostics are never reset. NULL is a no-op. No source method is entered. */
void mc_entity_nbt_select_cart_detail(mc_entity_nbt_error *error);
void mc_entity_nbt_select_player_detail(mc_entity_nbt_error *error);
mc_entity_nbt_error mc_entity_nbt_empty_error(void);
const char *mc_entity_nbt_status_string(mc_entity_nbt_status status);

#endif
