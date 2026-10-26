#ifndef MC_ENTITY_PLAYER_TICK_H
#define MC_ENTITY_PLAYER_TICK_H
#include "mc/entity/action/action.h"
#include "mc/entity/player/statistics.h"
#include "mc/entity/tick/environment.h"

typedef struct mc_player_tick_limits {
    size_t max_animation_work;
    mc_player_action_limits action;
    mc_entity_tick_limits environment;
} mc_player_tick_limits;

#define MC_PLAYER_TICK_DEFAULT_ANIMATION_WORK ((size_t)1048576)

typedef enum mc_player_tick_child {
    MC_PLAYER_TICK_CHILD_NONE,
    MC_PLAYER_TICK_CHILD_ENVIRONMENT,
    MC_PLAYER_TICK_CHILD_ACTION
} mc_player_tick_child;

typedef struct mc_player_tick_error {
    mc_entity_status status;
    const char *operation, *source_class, *source_method;
    unsigned int player_bytecode, living_bytecode, local_bytecode;
    unsigned int pushout_bytecode, pushout_call_bytecode, solid_bytecode;
    unsigned int bed_bytecode, container_bytecode, close_bytecode;
    unsigned int dismount_bytecode, position_bytecode;
    size_t animation_work, max_animation_work;
    mc_registry_error registry;
    /* Only the tagged last reached large child is active. Completed success
     * is empty; an interrupted child retains its full original diagnostics. */
    mc_player_tick_child child;

    union {
        mc_entity_tick_error environment;
        mc_player_action_error action;
    } child_error;

    mc_player_statistic_error statistic;
    mc_client_stats_error writer;
    mc_client_notice_error notice;
    mc_movement_input_error input;
    mc_world_storage_error world;
    mc_world_solid_support_error solid;
} mc_player_tick_error;

void mc_player_tick_error_init(mc_player_tick_error *);
/* Genuine canonical READY dc only. The action entry is dc.o, living names
 * invokespecial ls.w_, and player executes inherited gs.w_. Internal virtual
 * descendants remain genuine dc. Context/owners/limits stay live/exclusive
 * through synchronous reentry; errors/outputs are disjoint. No World tick,
 * riding s_, outer Minecraft.k writer service, implicit pool reset, source
 * save, successful truncated work or empty unsupported descendant is added.
 * Every reached failure retains the completed source prefix and child frames. */
mc_entity_status mc_local_player_action_tick(mc_entity *, const mc_player_action_context *,
                                             const mc_player_tick_limits *, mc_player_tick_error *);
mc_entity_status mc_living_tick_body(mc_entity *, const mc_player_action_context *,
                                     const mc_player_tick_limits *, mc_player_tick_error *);
mc_entity_status mc_player_tick(mc_entity *, const mc_player_action_context *,
                                const mc_player_tick_limits *, mc_player_tick_error *);
#endif
