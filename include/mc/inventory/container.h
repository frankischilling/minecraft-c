#ifndef MC_INVENTORY_CONTAINER_H
#define MC_INVENTORY_CONTAINER_H

#include "mc/inventory/crafting.h"
#include "mc/inventory/player.h"
#include "mc/inventory/result.h"

typedef struct mc_world_runtime mc_world_runtime;

typedef enum mc_container_kind { MC_CONTAINER_PLAYER, MC_CONTAINER_WORKBENCH } mc_container_kind;

typedef enum mc_slot_kind { MC_SLOT_NORMAL, MC_SLOT_CRAFTING, MC_SLOT_ARMOR } mc_slot_kind;

typedef enum mc_slot_inventory_kind {
    MC_SLOT_INVENTORY_CRAFTING,
    MC_SLOT_INVENTORY_RESULT,
    MC_SLOT_INVENTORY_PLAYER
} mc_slot_inventory_kind;

typedef struct mc_container_slot_state {
    mc_slot_kind kind;
    mc_slot_inventory_kind inventory_kind;
    const void *inventory;
    mc_entity *player;
    const mc_inventory_crafting *crafting_input;
    const mc_container *armor_container;
    int32_t container_index, inventory_index, x, y, armor_type;
} mc_container_slot_state;

typedef struct mc_container_state {
    mc_registry_side side;
    mc_container_kind kind;
    bool constructed, active, local;
    int32_t window_id;
    int16_t transaction;
    size_t slots, snapshots, listeners, blocked_players, allocation_bytes;
    const void *slot_list, *snapshot_list, *listener_list, *blocked_player_set;
    mc_inventory_crafting *crafting;
    mc_inventory_result *result;
    mc_inventory_player *player_inventory;
    mc_world_runtime *world;
    int32_t x, y, z;
} mc_container_state;

/* Actual base fields, distinct 2x2/3x3 grid/result owners, and45/46 Slots.
 * Classes and supplied Player inventory/World stay borrowed and live. Output
 * preview executes the existing real recipe singleton and catalog. No click,
 * output-pickup, Player stat, ItemMap, close/drop or eligibility body is added. */
mc_inventory_status mc_container_player_create(mc_registry_side side, mc_registry_classes *classes,
                                               mc_inventory_player *inventory, bool local,
                                               const mc_inventory_limits *limits,
                                               mc_container **out, mc_inventory_error *error);
mc_inventory_status mc_container_workbench_create(mc_registry_side side,
                                                  mc_registry_classes *classes,
                                                  mc_inventory_player *inventory,
                                                  mc_world_runtime *world, int32_t x, int32_t y,
                                                  int32_t z, const mc_inventory_limits *limits,
                                                  mc_container **out, mc_inventory_error *error);
mc_inventory_status mc_container_player_create_default(
    mc_registry_side side, mc_registry_classes *classes, mc_inventory_player *inventory,
    const mc_inventory_limits *limits, mc_container **out, mc_inventory_error *error);
mc_inventory_status mc_container_retire(mc_container **container, mc_inventory_error *error);
bool mc_container_read_state(const mc_container *container, mc_container_state *out);
mc_inventory_status mc_container_slot_read(const mc_container *container, int32_t slot,
                                           mc_container_slot_state *out, mc_inventory_error *error);
mc_inventory_status mc_container_inventory_changed(mc_container *container,
                                                   mc_inventory_error *error);

/* All owner operations require exclusive access. Borrowed state/slot/array
 * views require their owners to remain live. Child grid/result retirement is
 * BUSY while the concrete Container owns it. Native retirement releases its
 * resources; it does not invoke source close/pickup/drop/Player hooks. */

#endif
