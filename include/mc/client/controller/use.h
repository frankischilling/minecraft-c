#ifndef MC_CLIENT_CONTROLLER_USE_H
#define MC_CLIENT_CONTROLLER_USE_H

#include "mc/block/registry/runtime.h"
#include "mc/client/controller/singleplayer.h"
#include "mc/inventory/player.h"
#include "mc/world/runtime/runtime.h"

typedef struct mc_singleplayer_use_call mc_singleplayer_use_call;
typedef struct mc_item_use_state mc_item_use_state;
typedef struct mc_item_use_error mc_item_use_error;
typedef struct mc_world_placement_error mc_world_placement_error;
typedef struct mc_world_block_activation_context mc_world_block_activation_context;
typedef struct mc_world_runtime_acquisition_error mc_world_runtime_acquisition_error;

typedef struct mc_math_classes mc_math_classes;

/* Actual client class dependencies and reached Player virtual children.
 * begin copies this record and the activation callbacks; keep their supplying
 * graph live/exclusive through retirement. Math is not retained or activated
 * until a reached Item child needs that exact namespace. */
typedef struct mc_singleplayer_use_context {
    const mc_world_block_activation_context *activation;
    mc_math_classes *math;
} mc_singleplayer_use_context;

typedef enum mc_singleplayer_use_status {
    MC_SINGLEPLAYER_USE_OK,
    MC_SINGLEPLAYER_USE_ARGUMENT,
    MC_SINGLEPLAYER_USE_BUSY,
    MC_SINGLEPLAYER_USE_LIMIT,
    MC_SINGLEPLAYER_USE_ALLOCATION,
    MC_SINGLEPLAYER_USE_SOURCE_NULL,
    MC_SINGLEPLAYER_USE_SOURCE_INDEX,
    MC_SINGLEPLAYER_USE_REGISTRY,
    MC_SINGLEPLAYER_USE_WORLD,
    MC_SINGLEPLAYER_USE_PLAYER,
    MC_SINGLEPLAYER_USE_STACK,
    MC_SINGLEPLAYER_USE_ITEM,
    MC_SINGLEPLAYER_USE_INVENTORY,
    MC_SINGLEPLAYER_USE_DEPENDENCY,
    MC_SINGLEPLAYER_USE_CLEANUP
} mc_singleplayer_use_status;

typedef struct mc_singleplayer_use_error {
    mc_singleplayer_use_status status;
    const char *operation, *exception_class;
    mc_player_site site, child;
    bool native_policy, source_exception, source_error;
    mc_registry_error registry;
    mc_world_storage_error world;
    mc_entity_status player;
    mc_item_stack_status stack;
    mc_inventory_error inventory;
    const mc_world_runtime_acquisition_error *acquisition;
    const mc_world_placement_error *activation;
    const mc_item_use_error *item;
} mc_singleplayer_use_error;

typedef struct mc_singleplayer_use_state {
    mc_client_singleplayer_controller *controller;
    const mc_singleplayer_use_error *error, *cleanup;
    const mc_item_use_state *item;
    mc_entity *player;
    mc_world_runtime *world;
    mc_item_stack *stack, *result_stack;
    unsigned int source_pc;
    bool active, returned, result, on_block;
    bool block_read, activation_entered, activation_returned, activated;
    bool item_entered, item_returned, count_read, count_compared, same_stack;
    bool selected_slot_stored, zero_slot_cleared;
    bool placement_admitted, setter_returned, placed, sound_returned, count_stored;
    bool statistic_returned;
    int32_t clicked_x, clicked_y, clicked_z, clicked_id;
    int32_t x, y, z, face, block_id, metadata, count_before, count_after, selected;
} mc_singleplayer_use_state;

/* Actual inherited OB methods on the canonical OS owner, with the supplied
 * source arguments. No App/World/Stack substitute is selected. On-block ID7
 * and activation31 precede nullable Stack44/use56. In-air count4/use9 and
 * returned identity/count precede fresh Player Inventory stores51/75.
 * Each admitted call is returned even on failure and owns reached native
 * occurrences and children until checked retirement. Keep the enclosing App
 * namespace live/exclusive; no failure is replayed or source effect undone.
 * Borrowed child views expire as their checked native retirement succeeds. */
mc_singleplayer_use_status
mc_client_singleplayer_use_on_block_begin(mc_client_singleplayer_controller *, mc_entity *player,
                                          mc_world_runtime *, mc_item_stack *, int32_t x, int32_t y,
                                          int32_t z, int32_t face, mc_singleplayer_use_call **,
                                          mc_singleplayer_use_error *);
/* Same OB body, with its actual synchronous Player display child supplied by
 * the enclosing client caller. Context lifetime includes checked retirement. */
mc_singleplayer_use_status mc_client_singleplayer_use_on_block_with_context_begin(
    mc_client_singleplayer_controller *, mc_entity *player, mc_world_runtime *, mc_item_stack *,
    int32_t x, int32_t y, int32_t z, int32_t face, const mc_world_block_activation_context *,
    mc_singleplayer_use_call **, mc_singleplayer_use_error *);
mc_singleplayer_use_status mc_client_singleplayer_use_on_block_with_sources_begin(
    mc_client_singleplayer_controller *, mc_entity *player, mc_world_runtime *, mc_item_stack *,
    int32_t x, int32_t y, int32_t z, int32_t face, const mc_singleplayer_use_context *,
    mc_singleplayer_use_call **, mc_singleplayer_use_error *);
mc_singleplayer_use_status
mc_client_singleplayer_use_in_air_begin(mc_client_singleplayer_controller *, mc_entity *player,
                                        mc_world_runtime *, mc_item_stack *,
                                        mc_singleplayer_use_call **, mc_singleplayer_use_error *);
bool mc_client_singleplayer_use_inspect(const mc_singleplayer_use_call *,
                                        mc_singleplayer_use_state *);
mc_singleplayer_use_status mc_client_singleplayer_use_retire(mc_singleplayer_use_call **,
                                                             mc_singleplayer_use_error *);

#endif
