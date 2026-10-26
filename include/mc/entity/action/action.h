#ifndef MC_ENTITY_ACTION_ACTION_H
#define MC_ENTITY_ACTION_ACTION_H
#include "mc/entity/item/pickup.h"
#include "mc/entity/living/travel.h"
#include "mc/entity/player/input.h"
#include "mc/inventory/runtime/tick.h"

typedef struct mc_player_action_context {
    mc_player_travel_context travel;
    const mc_inventory_tick_limits *inventory_limits;
} mc_player_action_context;

typedef struct mc_player_action_limits {
    size_t max_loop_work;
} mc_player_action_limits;

#define MC_PLAYER_ACTION_DEFAULT_LOOP_WORK ((size_t)1048576)

typedef enum mc_player_action_child_kind {
    MC_PLAYER_ACTION_CHILD_NONE = 0,
    MC_PLAYER_ACTION_CHILD_TRAVEL,
    MC_PLAYER_ACTION_CHILD_PICKUP
} mc_player_action_child_kind;

typedef struct mc_player_action_error {
    mc_entity_status status;
    const char *operation, *source_class, *source_method;
    unsigned int player_bytecode, living_bytecode, input_bytecode, swing_bytecode;
    unsigned int push_bytecode, pickup_bytecode, contact_bytecode, position_bytecode,
        angle_bytecode;
    size_t loop_work, max_loop_work, nearby_index;
    mc_entity_error entity;
    mc_entity_bounds_error bounds;
    mc_registry_error registry;
    mc_item_stack_status stack;
    mc_inventory_tick_error inventory;
    /* A failed jump/travel returns before pickup. Pickup is reached only after
     * completed travel returned its fresh empty child. Keep the full active
     * failed child without adding another large automatic frame to this chain.
     * NONE/fresh storage has the legacy fresh travel sentinels. Read pickup
     * only for PICKUP; read travel only for NONE/TRAVEL. Independent meaningful
     * Inventory/Registry/bounds/nearby siblings and caller PCs remain separate. */
    mc_player_action_child_kind child_kind;

    union {
        mc_player_travel_error travel;
        mc_item_pickup_error pickup;
    };

    mc_entity_environment_error environment;
    mc_world_collision_error collision;
    mc_world_nearby_error nearby;
} mc_player_action_error;

void mc_player_action_error_init(mc_player_action_error *error);
/* Canonical held READY dc only. These entries name gs.o, its invoked ls.o
 * body, and actual dc.f_ (including gs.f_). They do not execute dc.o's earlier
 * tutorial/portal/Input.update/push-out prefix or either w_ wrapper/World tick.
 * Contexts, owners, limits and dependencies remain live/exclusive across
 * synchronous calls. Lists and source fields stay live; no copied inventory,
 * collision snapshot, implicit pool reset or fabricated empty descendant.
 * Reached failures retain all completed effects and detailed child frames.
 * Outputs/errors/limits are disjoint from owners and each other. The optional
 * work ceiling bounds pending angle/List iterations without truncation success. */
mc_entity_status mc_player_action(mc_entity *, const mc_player_action_context *,
                                  const mc_player_action_limits *, mc_player_action_error *);
mc_entity_status mc_living_action_body(mc_entity *, const mc_player_action_context *,
                                       const mc_player_action_limits *, mc_player_action_error *);
mc_entity_status mc_local_player_action_input(mc_entity *, mc_player_action_error *);
#endif
