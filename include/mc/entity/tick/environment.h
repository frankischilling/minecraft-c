#ifndef MC_ENTITY_TICK_ENVIRONMENT_H
#define MC_ENTITY_TICK_ENVIRONMENT_H
#include "mc/entity/movement/effects.h"
#include "mc/entity/movement/environment.h"
#include "mc/entity/movement/material.h"

typedef struct mc_entity_tick_limits {
    size_t max_particle_work;
    mc_world_material_movement_limits material;
} mc_entity_tick_limits;

#define MC_ENTITY_TICK_DEFAULT_PARTICLE_WORK ((size_t)1048576)

typedef struct mc_entity_tick_error {
    mc_entity_status status;
    const char *operation, *source_class, *source_method;
    unsigned int bytecode, base_bytecode, living_bytecode;
    unsigned int flag_bytecode, watcher_bytecode, suffocation_bytecode;
    unsigned int lava_bytecode, void_bytecode, ambient_bytecode;
    int32_t watcher_index;
    size_t particle_work, max_particle_work;
    mc_entity_error entity;
    mc_entity_environment_error environment;
    mc_entity_material_error material;
    mc_entity_effect_error effect;
    mc_world_movement_error world;
    mc_world_solid_support_error solid;
    mc_registry_error registry;
    bool native_policy, source_exception;
    const char *exception_class;
} mc_entity_tick_error;

void mc_entity_tick_error_init(mc_entity_tick_error *error);

/* Actual virtual U() on a held READY canonical client dc: ls.U invokes sn.U
 * then continues its real Living environment/counter sequence. This is not
 * gs/ls.w_, action/travel, a World update or a full Player tick. The separate
 * base-body entry names only the invokespecial sn.U operation.
 * Keep actual code Registry and all owners live/exclusive through callbacks.
 * Failure preserves every completed field/Random/pool/watcher/World effect.
 * Reached missing damage/close descendants retain their real failure prefixes.
 * Limits/error storage are disjoint from owners/dependencies. NULL limits
 * choose independent default particle/material ceilings. Exhaustion stops
 * before a pending iteration, never as successful truncated work. */
mc_entity_status mc_entity_tick_update_in_context(mc_entity *local_player,
                                                  mc_registry_classes *code_registry,
                                                  const mc_entity_tick_limits *limits,
                                                  mc_entity_tick_error *error);
mc_entity_status mc_entity_tick_base_body_in_context(mc_entity *local_player,
                                                     mc_registry_classes *code_registry,
                                                     const mc_entity_tick_limits *limits,
                                                     mc_entity_tick_error *error);

/* Actual sn.b(int,boolean): typed Byte read, masked Java bit operation and
 * separately reloaded watcher setter. Byte.equals controls record dirty then
 * watcher dirty stores. No generic Object, invented Box or serialization. */
mc_entity_status mc_entity_tick_flag_set(mc_entity *local_player, int32_t bit, bool value,
                                         mc_entity_tick_error *error);
mc_entity_status mc_entity_tick_suffocating(mc_entity *local_player, bool *out,
                                            mc_entity_tick_error *error);
#endif
