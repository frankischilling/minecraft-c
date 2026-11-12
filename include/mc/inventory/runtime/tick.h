#ifndef MC_INVENTORY_RUNTIME_TICK_H
#define MC_INVENTORY_RUNTIME_TICK_H

#include "mc/entity/entity.h"
#include "mc/inventory/player.h"
#include "mc/item/registry/registry.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/saved_data/value.h"

typedef struct mc_inventory_tick_limits {
    size_t max_name_allocation_bytes, max_map_allocation_bytes;
} mc_inventory_tick_limits;

typedef enum mc_inventory_tick_status {
    MC_INVENTORY_TICK_OK = 0,
    MC_INVENTORY_TICK_INVALID_ARGUMENT,
    MC_INVENTORY_TICK_NULL_REFERENCE,
    MC_INVENTORY_TICK_CLASS_CAST,
    MC_INVENTORY_TICK_OWNER,
    MC_INVENTORY_TICK_NAMESPACE,
    MC_INVENTORY_TICK_STACK_FAILED,
    MC_INVENTORY_TICK_REGISTRY_FAILED,
    MC_INVENTORY_TICK_WORLD_FAILED,
    MC_INVENTORY_TICK_LIMIT,
    MC_INVENTORY_TICK_ALLOCATION_FAILED,
    MC_INVENTORY_TICK_BUSY,
    MC_INVENTORY_TICK_UNIMPLEMENTED_DEPENDENCY
} mc_inventory_tick_status;

typedef struct mc_inventory_tick_map_prefix {
    bool created, published, dirty;
    int32_t x_center, z_center;
    int8_t scale, dimension;
} mc_inventory_tick_map_prefix;

typedef struct mc_inventory_tick_error {
    mc_inventory_tick_status status;
    const char *operation;
    unsigned int inventory_bytecode, stack_bytecode, item_bytecode, map_lookup_bytecode;
    unsigned int tracker_bytecode, terrain_bytecode;
    int32_t slot;
    size_t completed_slots;
    bool animation_stored;
    const mc_item *item;
    /* Borrowed only after successful lookup, when the actual World manager
     * owns it. Keep that same manager/World live after this call to inspect. */
    const mc_saved_data_value *pending_map;
    mc_inventory_tick_map_prefix map;
    mc_item_stack_status stack;
    mc_entity_status entity;
    mc_registry_error registry;
    mc_world_storage_error world;
    mc_saved_data_error saved_data;
    mc_world_info_status info;
    mc_world_provider_status provider;
} mc_inventory_tick_error;

/* Reuse the existing SavedData default's 4MiB bound independently for a new
 * name and new fixed-size MapData owner. Existing manager/NBT budgets apply
 * separately. Zero caller limits are valid and preserve reached prefixes. */
mc_inventory_tick_limits mc_inventory_tick_default_limits(void);
mc_inventory_tick_error mc_inventory_tick_empty_error(void);

/* Actual client ix.e()V. Visit the SAME main array in source order, rereading
 * field/cell/player/world/selected at their original sites. Armor/cursor are
 * not visited. No array copy, replacement inventory or general callback.
 * A null player is source-valid until a nonnull main slot reads gs.aI at30.
 * Native active state prevents retirement/reentry; it is not a source flag.
 * Actual stack/Registry/Entity/World owners remain exclusive and live, with
 * source aliases retained across descendants. Code Registry is an independent
 * loaded namespace, nullable until a reached getstatic; it is borrowed without
 * class active use at entry. No World is fabricated to obtain that namespace.
 * Errors/limits must be disjoint from all owners/dependencies. Constructor
 * diagnostics and all earlier field/storage prefixes remain unchanged on
 * failure; success returns a fresh empty error. No active Player tick. */
mc_inventory_tick_status mc_inventory_player_tick(mc_inventory_player *inventory,
                                                  mc_registry_classes *code_registry,
                                                  const mc_inventory_tick_limits *limits,
                                                  mc_inventory_tick_error *error);

/* Actual iz.a(World,Entity,int,bool), Code35: decrement positive animation
 * BEFORE Item.c active use, then read current id, captured-array aaload and
 * actual virtual Item update. Nullable World/Entity are not dereferenced by
 * the proven inherited base Item body. Source Stack phases/provenance remain
 * unchanged. No item/count/damage clamp or empty Map substitution. */
mc_inventory_tick_status mc_item_stack_inventory_update(mc_item_stack *stack,
                                                        mc_registry_classes *code_registry,
                                                        mc_world_runtime *world, mc_entity *entity,
                                                        int32_t slot, bool selected,
                                                        const mc_inventory_tick_limits *limits,
                                                        mc_inventory_tick_error *error);

/* Actual concrete gm/wr.a(Stack,World,Entity,int,bool), no preceding animation
 * or Item.c load. The receiver's real Registry owner supplies the namespace.
 * All currently represented source subtypes are exhaustively resolved: base
 * return0 is genuine; Map reads real World.B and performs real lookup/create.
 * Player tracking (iu.a) and selected terrain updates remain explicit reached
 * dependencies. Their source calls never return an invented empty success. */
mc_inventory_tick_status mc_item_inventory_update(const mc_item *item, mc_item_stack *stack,
                                                  mc_world_runtime *world, mc_entity *entity,
                                                  int32_t slot, bool selected,
                                                  const mc_inventory_tick_limits *limits,
                                                  mc_inventory_tick_error *error);

/* Actual wr.a(Stack,World) lookup/create body. Successful nonnull output is
 * an owned real MapData hold independent of the manager's cache/list holds.
 * Failure preserves out and earlier counter, Stack damage and manager effects.
 * This entry performs neither Player tracking nor terrain/pixel updates. */
mc_inventory_tick_status mc_item_map_data_get(const mc_item *map_item, mc_item_stack *stack,
                                              mc_world_runtime *world,
                                              const mc_inventory_tick_limits *limits,
                                              mc_saved_data_value **out,
                                              mc_inventory_tick_error *error);

#endif
