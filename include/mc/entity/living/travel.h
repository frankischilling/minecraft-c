#ifndef MC_ENTITY_LIVING_TRAVEL_H
#define MC_ENTITY_LIVING_TRAVEL_H

#include "mc/entity/movement/environment.h"
#include "mc/entity/movement/material.h"
#include "mc/entity/movement/movement.h"
#include "mc/statistics/registry/registry.h"

typedef struct mc_player_travel_context {
    mc_registry_classes *code_registry;
    const mc_math_trig *trig;
    const mc_entity_move_limits *movement_limits;
    const mc_world_material_movement_limits *material_limits;
} mc_player_travel_context;

typedef enum mc_player_travel_cause {
    MC_PLAYER_TRAVEL_NONE = 0,
    MC_PLAYER_TRAVEL_OWNER,
    MC_PLAYER_TRAVEL_NULL_REFERENCE,
    MC_PLAYER_TRAVEL_NAMESPACE,
    MC_PLAYER_TRAVEL_FP,
    MC_PLAYER_TRAVEL_REGISTRY,
    MC_PLAYER_TRAVEL_WORLD,
    MC_PLAYER_TRAVEL_ENVIRONMENT,
    MC_PLAYER_TRAVEL_MOVEMENT,
    MC_PLAYER_TRAVEL_PENDING_STATISTIC, /* Retained enum value for source compatibility. */
    MC_PLAYER_TRAVEL_STATISTIC
} mc_player_travel_cause;

typedef struct mc_player_travel_error {
    mc_entity_status status;
    mc_player_travel_cause cause;
    const char *operation, *source_class, *source_method, *player_method;
    /* source_* names the requested body. living/ladder name ls.a_/p;
     * player_bytecode belongs to player_method (gs.a_, R or private i).
     * statistic_bytecode belongs to the reached canonical dc.a bridge. */
    unsigned int living_bytecode, player_bytecode, ladder_bytecode, statistic_bytecode;
    const mc_statistic *statistic;
    const char *statistic_field;
    int32_t statistic_amount;
    mc_registry_error registry;
    mc_world_storage_error world;
    mc_entity_environment_error environment;
    mc_entity_material_error immersion;
    mc_entity_move_error movement;
    mc_entity_effect_error effect;
    mc_player_statistic_error statistic_error;
} mc_player_travel_error;

/* Passive initialization avoids extra automatic copies of the full descendant
 * frame. NULL is a no-op; no owners or source state are accessed. */
void mc_player_travel_error_init(mc_player_travel_error *error);
mc_player_travel_error mc_player_travel_empty_error(void);

/* Client-first actual ancestor bodies on a held READY canonical dc owner.
 * living_travel is precisely ls.a_(FF)V; player_travel is gs.a_(FF)V including
 * its saved coordinates and post-move i(DDD)V. No other Living subtype, active
 * tick, input update, gravity shortcut, world surrogate or pool reset.
 * All current and previous fields belong to the same existing Player owner.
 * Water uses cached bx; lava queries its real bounds. Ground friction and its
 * live Block.bB are reread at both source sites; ladder and dc sneaking retain
 * their actual descendants. The context-aware mover must be the genuine
 * completed movement/effects closure, including reached failures.
 *
 * Contexts borrow the actual Registry namespace and initialized immutable trig
 * table, independently of nullable Entity.world. A supplied Registry is held
 * through the call without class active use at native admission. Required
 * code statics fail at their reached site when context is absent; no synthetic
 * World is created. Callers keep all contexts/dependencies/limits exclusive,
 * stable and live. Outputs/errors must be disjoint from owners/dependencies.
 * Native failures preserve all completed field, World, pool and child prefixes;
 * constructor diagnostics remain unchanged. Strict x86-64 FP is required only
 * at reached arithmetic. Successful errors are fresh and empty.
 *
 * Reached dc.a(Statistic,int) calls use the actual application/StatsWriter/
 * notice owners borrowed through this same Player. The descriptor and child
 * error preserve source order, amount and caller PC. No callback, empty base
 * body, synthetic World or completed damage/close descendant is supplied.
 * Keep the Registry and actual application live through calls and diagnostics.
 * Successful travel is still not an active Player or World tick. */
mc_entity_status mc_living_travel(mc_entity *player, const mc_player_travel_context *context,
                                  float strafe, float forward, mc_player_travel_error *error);
mc_entity_status mc_player_travel(mc_entity *player, const mc_player_travel_context *context,
                                  float strafe, float forward, mc_player_travel_error *error);
mc_entity_status mc_living_on_ladder(mc_entity *player, const mc_player_travel_context *context,
                                     bool *out, mc_player_travel_error *error);
mc_entity_status mc_living_jump(mc_entity *player, mc_player_travel_error *error);
mc_entity_status mc_player_jump(mc_entity *player, const mc_player_travel_context *context,
                                mc_player_travel_error *error);
mc_entity_status mc_player_movement_statistics(mc_entity *player,
                                               const mc_player_travel_context *context, double dx,
                                               double dy, double dz, mc_player_travel_error *error);

#endif
