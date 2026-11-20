#ifndef MC_INVENTORY_RUNTIME_INSERTION_H
#define MC_INVENTORY_RUNTIME_INSERTION_H

#include "mc/entity/entity.h"
#include "mc/inventory/player.h"

typedef struct mc_inventory_insert_limits {
    size_t max_loop_work;
} mc_inventory_insert_limits;

#define MC_INVENTORY_INSERT_DEFAULT_LOOP_WORK ((size_t)1048576)

typedef struct mc_inventory_insert_error {
    mc_entity_status status;
    const char *operation, *source_method, *stack_method;
    unsigned int bytecode, add_bytecode, merge_bytecode, find_bytecode, stack_bytecode;
    int32_t slot;
    size_t loop_work, max_loop_work;
    mc_registry_error registry;
    mc_inventory_error inventory;
    mc_item_stack_status stack;
} mc_inventory_insert_error;

void mc_inventory_insert_error_init(mc_inventory_insert_error *);
/* Actual client ix.a(iz)Z and its concrete d/e/j helpers on the genuine main
 * array, using real iz property bodies and Item virtuals. The Registry is a
 * borrowed client code namespace, retained without active use at admission.
 * Native Inventory/Stack lifetimes are protected through source calls; arrays,
 * selected cells, Item statics and Stack fields stay live and are reread.
 * No copied inventory, count clamp, READY gate on source field reads, implicit
 * changed flag, source Player/World lookup or generic Item callback is added.
 * Caller keeps all owners/dependencies exclusive and live; outputs/errors/
 * limits are disjoint. A pending-loop ceiling fails at the reached operation,
 * never returns shortened success. Boolean out is published only on success.
 * This completes insertion, independently of the later dc pickup particle. */
mc_entity_status mc_inventory_player_insert(mc_inventory_player *, mc_item_stack *,
                                            mc_registry_classes *,
                                            const mc_inventory_insert_limits *, bool *,
                                            mc_inventory_insert_error *);

#endif
