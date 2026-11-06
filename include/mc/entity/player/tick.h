#ifndef MC_ENTITY_PLAYER_TICK_H
#define MC_ENTITY_PLAYER_TICK_H
#include "mc/entity/action/action.h"
#include "mc/entity/player/sleep.h"
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
    MC_PLAYER_TICK_CHILD_ACTION,
    MC_PLAYER_TICK_CHILD_WAKE
} mc_player_tick_child;

typedef struct mc_player_tick_wake {
    mc_player_sleep_call *call;
    mc_player_sleep_error error;
} mc_player_tick_wake;

typedef enum mc_player_tick_cleanup_child {
    MC_PLAYER_TICK_CLEANUP_NONE,
    MC_PLAYER_TICK_CLEANUP_WAKE,
    MC_PLAYER_TICK_CLEANUP_REGISTRY,
    MC_PLAYER_TICK_CLEANUP_EFFECT
} mc_player_tick_cleanup_child;

typedef enum mc_player_tick_effect_child {
    MC_PLAYER_TICK_EFFECT_NONE,
    MC_PLAYER_TICK_EFFECT_ENVIRONMENT,
    MC_PLAYER_TICK_EFFECT_TRAVEL,
    MC_PLAYER_TICK_EFFECT_MOVEMENT
} mc_player_tick_effect_child;

typedef struct mc_player_tick_cleanup {
    mc_entity_status status;
    const char *operation;
    mc_player_tick_cleanup_child child;
    mc_player_tick_effect_child effect;
    mc_player_sleep_cleanup wake;
    mc_registry_error registry;
} mc_player_tick_cleanup;

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
        mc_player_tick_wake wake;
    } child_error;

    mc_player_statistic_error statistic;
    mc_client_stats_error writer;
    mc_client_notice_error notice;
    mc_player_close_error close_error;
    mc_movement_input_error input;
    mc_world_storage_error world;
    mc_world_solid_support_error solid;
    /* Sole independently owned entry Registry occurrence on an interrupted
     * wake or refused entry-hold cleanup. Separate from the child Registry. */
    mc_registry_classes *retained_registry;
    mc_player_tick_cleanup cleanup;
} mc_player_tick_error;

/* Initialization is passive and must not discard an owned previous result. */
void mc_player_tick_error_init(mc_player_tick_error *);
/* A WAKE call and retained_registry transfer exactly once into this result.
 * While either is nonnull it is noncopyable: keep the actual graph live and
 * retire before reusing/clearing the record. Retry performs only native checked
 * cleanup, preserving the first source status/site and every completed effect.
 * Successful child retirement clears its expired borrowed pointers before a
 * later Registry refusal. Nested environment and active travel/movement effect
 * receipts borrow actor-owned work; retirement checks those actual tagged
 * children before releasing the parent Registry. An EFFECT refusal identifies
 * its exact nested receipt through cleanup.effect and leaves its first source
 * cause intact. Inspect the current cleanup discriminator only. */
bool mc_player_tick_error_retire(mc_player_tick_error *);
/* Genuine canonical READY dc only. The action entry is dc.o, living names
 * invokespecial ls.w_, and player executes inherited gs.w_. Internal virtual
 * descendants remain genuine dc. Context/owners/limits stay live/exclusive
 * through synchronous reentry; errors/outputs are disjoint. No World tick,
 * riding s_, outer Minecraft.k writer service, implicit pool reset, source
 * save, successful truncated work or empty unsupported descendant is added.
 * Every reached failure retains the completed source prefix and child frames.
 * GS.w_53/73 invoke one actual retained wake child with the exact source flags;
 * successful wake is checked-retired before Living111. A nonnull error output
 * owns any failed child. With NULL output a reached wake stops before invocation
 * with an explicit native dependency, after its genuine preceding prefix;
 * ordinary non-waking NULL-output paths keep their existing behavior. */
mc_entity_status mc_local_player_action_tick(mc_entity *, const mc_player_action_context *,
                                             const mc_player_tick_limits *, mc_player_tick_error *);
mc_entity_status mc_living_tick_body(mc_entity *, const mc_player_action_context *,
                                     const mc_player_tick_limits *, mc_player_tick_error *);
mc_entity_status mc_player_tick(mc_entity *, const mc_player_action_context *,
                                const mc_player_tick_limits *, mc_player_tick_error *);
#endif
