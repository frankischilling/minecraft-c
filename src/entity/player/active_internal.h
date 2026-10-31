#ifndef MC_ENTITY_PLAYER_ACTIVE_INTERNAL_H
#define MC_ENTITY_PLAYER_ACTIVE_INTERNAL_H
#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../client/application_access.h"
#include "../../client/input/movement_input_internal.h"
#include "../../world/runtime/construction_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/entity/player/tick.h"
#include "mc/inventory/container.h"
#include "player_internal.h"
#include "tick_math_internal.h"

typedef struct mc_active_scope {
    mc_entity *player;
    const mc_player_action_context *context;
    const mc_player_tick_limits *limits;
    mc_registry_classes *registry;
    bool registry_held;
} mc_active_scope;

bool mc_active_fail(mc_player_tick_error *, mc_entity_status, const char *);
bool mc_active_fp(mc_player_tick_error *);
bool mc_active_work(mc_player_tick_error *);
bool mc_active_registry(mc_active_scope *, mc_player_tick_error *);
bool mc_active_registry_result(mc_player_tick_error *, mc_registry_status);
bool mc_active_world_result(mc_player_tick_error *, mc_world_storage_status, const char *);
bool mc_active_local(mc_active_scope *, mc_player_tick_error *);
bool mc_active_living(mc_active_scope *, mc_player_tick_error *);
bool mc_active_player(mc_active_scope *, mc_player_tick_error *);
bool mc_active_pushout(mc_active_scope *, double, double, double, mc_player_tick_error *);
bool mc_active_container(mc_active_scope *, mc_container *, bool *, mc_player_tick_error *);
#endif
