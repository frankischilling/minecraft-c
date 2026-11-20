#ifndef MC_INVENTORY_RUNTIME_ARMOR_H
#define MC_INVENTORY_RUNTIME_ARMOR_H

#include "mc/inventory/player.h"
#include "mc/item/combat/combat.h"

typedef enum mc_inventory_armor_status {
    MC_INVENTORY_ARMOR_OK = 0,
    MC_INVENTORY_ARMOR_INVALID_ARGUMENT,
    MC_INVENTORY_ARMOR_BUSY,
    MC_INVENTORY_ARMOR_UNIMPLEMENTED_DEPENDENCY,
    MC_INVENTORY_ARMOR_NULL_REFERENCE,
    MC_INVENTORY_ARMOR_CLASS_CAST,
    MC_INVENTORY_ARMOR_OWNER,
    MC_INVENTORY_ARMOR_STACK_FAILED,
    MC_INVENTORY_ARMOR_REGISTRY_FAILED,
    MC_INVENTORY_ARMOR_COMBAT_FAILED,
    MC_INVENTORY_ARMOR_LIMIT,
    MC_INVENTORY_ARMOR_ALLOCATION_FAILED
} mc_inventory_armor_status;

typedef struct mc_inventory_armor_error {
    mc_inventory_armor_status status;
    const char *operation, *exception_class;
    mc_inventory_site site, child, dependency;
    bool native_policy, source_exception, source_error;
    bool source_entered, source_returned;
    int32_t slot, defense, remaining, maximum;
    size_t completed_slots, wear_calls, empty_hooks, cleared_slots;
    bool durability_entered, durability_returned, empty_hook_entered, empty_hook_returned;
    /* The source field/hold operations return this complete scalar cause;
     * they invoke no NBT operation or constructor/READY phase check. */
    mc_item_stack_status stack;
    mc_registry_error registry;
    mc_item_combat_error combat;
} mc_inventory_armor_error;

void mc_inventory_armor_error_init(mc_inventory_armor_error *);

/* Actual client IX.f()I and IX.e(I)V over the constructor-owned armor4 array.
 * Reload the actual array/cell/Item/maximum/damage/Player at their Source sites.
 * Nullable code Registry and Player are unused until the selected body reaches
 * them. Server methods remain explicit unavailable dependencies. No replacement
 * arrays, callbacks, phase gates, clamping or changed notification are added.
 *
 * The Inventory and reached Registry/Stack/Player owners stay live/exclusive;
 * native active state prevents Inventory retirement/reentry. Each owning slot
 * retains its Stack independently, including aliases in other slots. Earlier
 * durability/statistic/slot mutations survive failure. No pending child is
 * returned. Error and value outputs are disjoint from all owners and arguments;
 * error is optional, value is required and unchanged on failure. Receipts retain
 * actual completed prefixes on success and failure. A null Inventory's Java
 * invocation PC belongs to its caller, so native refusal has no invented IX PC.
 */
mc_inventory_armor_status mc_inventory_player_armor_value(mc_inventory_player *,
                                                          mc_registry_classes *code_registry,
                                                          int32_t *out, mc_inventory_armor_error *);
mc_inventory_armor_status mc_inventory_player_armor_wear(mc_inventory_player *,
                                                         mc_registry_classes *code_registry,
                                                         int32_t amount,
                                                         mc_inventory_armor_error *);

#endif
