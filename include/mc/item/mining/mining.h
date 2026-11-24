#ifndef MC_ITEM_MINING_MINING_H
#define MC_ITEM_MINING_MINING_H

#include "mc/inventory/errors.h"
#include "mc/item/registry/runtime.h"

/* Actual client GM/DQ/QD/BL speed and GM/WC/AU/QD/BL harvest dispatch across
 * all represented Item Classes. Source fields and nullable unused arguments
 * keep their original semantics; no constructor/READY gate is added.
 * The actual client Registry and all owners remain live and exclusive.
 * Outputs are required/disjoint and preserved on failure. Errors are optional
 * values with reached source frames; no allocation or mutation occurs. */
mc_inventory_status mc_item_mining_speed(mc_registry_classes *, const mc_item *,
                                         const mc_item_stack *, const mc_block *, float *,
                                         mc_inventory_error *);
mc_inventory_status mc_item_mining_harvest(mc_registry_classes *, const mc_item *, const mc_block *,
                                           bool *, mc_inventory_error *);

/* IZ.a(Block) and IZ.b(Block) preserve their separate Item-array/Stack.id
 * reads. Speed first enters IZ.a(); harvest reads GM.c directly. */
mc_inventory_status mc_item_stack_mining_speed(mc_registry_classes *, const mc_item_stack *,
                                               const mc_block *, float *, mc_inventory_error *);
mc_inventory_status mc_item_stack_mining_harvest(mc_registry_classes *, const mc_item_stack *,
                                                 const mc_block *, bool *, mc_inventory_error *);

#endif
