#ifndef MC_INVENTORY_RUNTIME_NBT_H
#define MC_INVENTORY_RUNTIME_NBT_H

#include "mc/inventory/player.h"
#include "mc/storage/nbt/tree.h"

typedef enum mc_inventory_nbt_status {
    MC_INVENTORY_NBT_OK,
    MC_INVENTORY_NBT_INVALID_ARGUMENT,
    MC_INVENTORY_NBT_NULL_REFERENCE,
    MC_INVENTORY_NBT_WRONG_TAG,
    MC_INVENTORY_NBT_INDEX_OUT_OF_BOUNDS,
    MC_INVENTORY_NBT_LIMIT,
    MC_INVENTORY_NBT_ALLOCATION_FAILED,
    MC_INVENTORY_NBT_BUSY,
    MC_INVENTORY_NBT_STACK_FAILED,
    MC_INVENTORY_NBT_REGISTRY_FAILED,
    MC_INVENTORY_NBT_CODEC_FAILED
} mc_inventory_nbt_status;

typedef struct mc_inventory_nbt_error {
    mc_inventory_nbt_status status;
    const char *operation, *field;
    unsigned int bytecode, stack_bytecode;
    size_t index;
    int32_t slot;
    mc_item_stack_error stack;
    mc_registry_error registry;
    mc_nbt_error nbt;
} mc_inventory_nbt_error;

void mc_inventory_nbt_error_init(mc_inventory_nbt_error *);

/* Actual client ix.b(sp): replace main then armor arrays, then read each real
 * List occurrence in order. Prior replacements/slot stores survive failures.
 * A missing caller List is a reached NULL receiver after both replacements;
 * the Player's absent Inventory getter supplies a real empty List view.
 * Registry is the borrowed actual code namespace, not inferred from World.
 * Its Item active use occurs only at iz.a0, before the id read/array lookup.
 * NULL Registry is allowed until that reached operation. No copied arrays,
 * selected/cursor/changed update, notification, source preservation or drop.
 *
 * Keep Inventory, its owning Player, Registry and immutable input spans live
 * and exclusive. Borrowed array views expire at their corresponding source
 * replacement. max_elements is a per-attempt reached iteration ceiling;
 * exceeding it fails before that pending List.get without truncating success.
 * Inventory array allocation uses its existing budget, including old/new
 * overlap. Caller-owned input/Stack owners are separately charged. */
mc_inventory_nbt_status mc_inventory_player_read_nbt(mc_inventory_player *, mc_registry_classes *,
                                                     const mc_nbt_tag *, const mc_nbt_limits *,
                                                     mc_inventory_nbt_error *);

/* Actual ix.a(new sp), with fresh output rather than a caller-mutated List.
 * The independent compound has one Inventory List containing Slot/Stack fields
 * in source main-then-armor order. No unknown Stack source tags are overlaid.
 * RAW/failed live Stack field values remain valid writer receivers.
 * max_allocation_bytes covers fixed heap scratch plus the final tree/codec
 * peak; borrowed Inventory/Stack owners and native allocator overhead are
 * excluded. out is unchanged on failure. No Java temporary/OOME or supplied
 * mutable-List partial-update equivalence is claimed. Free with tree_free. */
mc_inventory_nbt_status mc_inventory_player_build_nbt(mc_inventory_player *, const mc_nbt_limits *,
                                                      mc_nbt_tree **, mc_inventory_nbt_error *);

#endif
