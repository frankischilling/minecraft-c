#ifndef MC_INVENTORY_CONTAINER_H
#define MC_INVENTORY_CONTAINER_H

#include "mc/inventory/chest.h"
#include "mc/inventory/crafting.h"
#include "mc/inventory/furnace.h"
#include "mc/inventory/player.h"
#include "mc/inventory/result.h"

typedef struct mc_world_runtime mc_world_runtime;

typedef enum mc_container_kind {
    MC_CONTAINER_PLAYER,
    MC_CONTAINER_WORKBENCH,
    MC_CONTAINER_CHEST,
    MC_CONTAINER_FURNACE
} mc_container_kind;

typedef enum mc_slot_kind {
    MC_SLOT_NORMAL,
    MC_SLOT_CRAFTING,
    MC_SLOT_ARMOR,
    MC_SLOT_FURNACE
} mc_slot_kind;

typedef enum mc_slot_inventory_kind {
    MC_SLOT_INVENTORY_CRAFTING,
    MC_SLOT_INVENTORY_RESULT,
    MC_SLOT_INVENTORY_PLAYER,
    MC_SLOT_INVENTORY_CHEST,
    MC_SLOT_INVENTORY_FURNACE
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
    bool constructor_entered, constructor_failed;
    int32_t constructor_pc;
    int32_t window_id;
    int16_t transaction;
    size_t slots, snapshots, listeners, blocked_players, allocation_bytes;
    const void *slot_list, *snapshot_list, *listener_list, *blocked_player_set;
    mc_inventory_crafting *crafting;
    mc_inventory_result *result;
    mc_inventory_player *player_inventory;
    mc_inventory_chest *chest_inventory;
    mc_inventory_furnace *furnace_inventory;
    int32_t furnace_cook, furnace_burn, furnace_fuel;
    int32_t chest_rows;
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
/* Actual NEW IT storage and one entered IT.<init> invocation. Allocation
 * performs no source constructor/class active use. out starts NULL. Source
 * construction accepts nullable Inventory/World unchanged, retains every
 * reached prefix on failure and cannot be retried/replayed. Inspect or hold
 * the same RAW/failed object, then retire it with the ordinary checked API.
 * Registry and argument owners remain live/exclusive through retirement.
 * The existing create factory still publishes only a completed Container
 * and frees its failed prefix; these source-lifetime entries do not weaken it. */
mc_inventory_status mc_container_workbench_allocate(mc_registry_side, mc_registry_classes *,
                                                    const mc_inventory_limits *, mc_container **,
                                                    mc_inventory_error *);
mc_inventory_status mc_container_workbench_construct(mc_container *, mc_inventory_player *,
                                                     mc_world_runtime *, int32_t, int32_t, int32_t,
                                                     mc_inventory_error *);
/* CD.<init>(LW,LW), retaining partial Slots/fields on constructor failure. The
 * supplied Player Inventory is a nullable LW argument; CD does not dereference
 * it while building ordinary GP Slots. CD owns the Chest and Player bindings.
 * Negative/partial rows use Java division and wrapping layout arithmetic. */
mc_inventory_status mc_container_chest_allocate(mc_registry_side, mc_registry_classes *,
                                                const mc_inventory_limits *, mc_container **,
                                                mc_inventory_error *);
mc_inventory_status mc_container_chest_construct(mc_container *, mc_inventory_player *,
                                                 mc_inventory_chest *, mc_inventory_error *);
mc_inventory_status mc_container_chest_create(mc_registry_side, mc_registry_classes *,
                                              mc_inventory_player *, mc_inventory_chest *,
                                              const mc_inventory_limits *, mc_container **,
                                              mc_inventory_error *);
/* Actual CD.b(GS) invokes its storage's LW.a_(GS), with original null and child
 * frames. This is source eligibility, independent of native retirement. */
mc_inventory_status mc_container_chest_usable(mc_container *, mc_entity *, bool *,
                                              mc_inventory_error *);
/* BM has two ordinary GP inputs, actual VQ output, and 36 Player Slots.
 * Nullable SK is stored without dereference; nullable IX fails at field64
 * after the two input Slots. Failed constructor prefixes remain inspectable. */
mc_inventory_status mc_container_furnace_allocate(mc_registry_side, mc_registry_classes *,
                                                  const mc_inventory_limits *, mc_container **,
                                                  mc_inventory_error *);
mc_inventory_status mc_container_furnace_construct(mc_container *, mc_inventory_player *,
                                                   mc_inventory_furnace *, mc_inventory_error *);
mc_inventory_status mc_container_furnace_create(mc_registry_side, mc_registry_classes *,
                                                mc_inventory_player *, mc_inventory_furnace *,
                                                const mc_inventory_limits *, mc_container **,
                                                mc_inventory_error *);
mc_inventory_status mc_container_furnace_usable(mc_container *, mc_entity *, bool *,
                                                mc_inventory_error *);
/* Actual BM.a()/DW snapshots and local empty-list progress-cache stores.
 * Reached unsupported EC callbacks refuse with the preceding snapshot intact. */
mc_inventory_status mc_container_furnace_detect_changes(mc_container *, mc_inventory_error *);
/* Direct actual BM.a(II) field stores; this does not simulate a packet/listener. */
mc_inventory_status mc_container_furnace_progress(mc_container *, int32_t, int32_t,
                                                  mc_inventory_error *);
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
