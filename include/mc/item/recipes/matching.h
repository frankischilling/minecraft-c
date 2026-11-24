#ifndef MC_ITEM_RECIPES_MATCHING_H
#define MC_ITEM_RECIPES_MATCHING_H

#include "mc/inventory/crafting.h"
#include "mc/item/recipes/recipes.h"

#define MC_CRAFTING_MAX_INGREDIENTS 65536u
#define MC_CRAFTING_MAX_RECIPES 65536u
#define MC_CRAFTING_MAX_WORK_STEPS 1048576u

typedef mc_inventory_status mc_crafting_status;
typedef mc_inventory_error mc_crafting_error;

#define MC_CRAFTING_OK MC_INVENTORY_OK
#define MC_CRAFTING_INVALID_ARGUMENT MC_INVENTORY_INVALID_ARGUMENT
#define MC_CRAFTING_NULL_REFERENCE MC_INVENTORY_NULL_REFERENCE
#define MC_CRAFTING_INDEX_OUT_OF_BOUNDS MC_INVENTORY_INDEX_OUT_OF_BOUNDS
#define MC_CRAFTING_LIMIT MC_INVENTORY_LIMIT
#define MC_CRAFTING_ALLOCATION_FAILED MC_INVENTORY_ALLOCATION_FAILED
#define MC_CRAFTING_STACK_FAILED MC_INVENTORY_STACK_FAILED
#define MC_CRAFTING_REGISTRY_FAILED MC_INVENTORY_REGISTRY_FAILED
#define MC_CRAFTING_UNIMPLEMENTED_DEPENDENCY MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY

/* Execute shaped/shapeless methods over the actual concrete grid. side selects
 * original diagnostic names. Borrowed inputs remain unchanged. The ordinary
 * profile is the retained real catalog and a 2x2/3x3 grid. Larger shape offset
 * bounds, scratch copies and work above the declared ceilings return native
 * LIMIT. Manager scans share one work budget across all their recipe calls.
 * Nonnull Stack inputs require READY phase. Source null/index failures retain
 * their caller frames; native owner/phase/allocation failures stay distinct.
 * Outputs are unchanged on failure. Calls require exclusive owner access and
 * valid disjoint output storage. */
mc_crafting_status mc_recipe_matches(mc_registry_side side, const mc_recipe *recipe,
                                     const mc_inventory_crafting *grid, bool *out,
                                     mc_crafting_error *error);

/* Fresh Stack hold, id/count/damage copied, animation0 and no native storage
 * provenance. This method ignores grid, including NULL, as the source does.
 * Shaped output allocation precedes retained output field reads. Caller owns
 * every successful result and must release an old result before reusing out. */
mc_crafting_status mc_recipe_craft_result(mc_registry_side side, const mc_recipe *recipe,
                                          const mc_inventory_crafting *grid, mc_item_stack **out,
                                          mc_crafting_error *error);

/* Scan the actual manager's retained sorted list, returning its first match's
 * fresh output or NULL on successful no-match. No class initialization or
 * substitute manager is added. classes/manager, recipes, grid and input Stacks
 * stay live through the synchronous call; returned output can outlive them. */
mc_crafting_status mc_crafting_manager_find_matching(mc_registry_side side,
                                                     const mc_crafting_manager *manager,
                                                     const mc_inventory_crafting *grid,
                                                     mc_item_stack **out, mc_crafting_error *error);

#endif
