#ifndef MC_ENTITY_PLAYER_DAMAGE_ALERT_EXECUTION_INTERNAL_H
#define MC_ENTITY_PLAYER_DAMAGE_ALERT_EXECUTION_INTERNAL_H
#include "ally_alert_execution_internal.h"
#include "mc/entity/movement/effects.h"

/* SAME retained GS144 wrapper, genuine Player/target passed unchanged. Caller
 * owns stable execution/code/result through actual pending victim cleanup.
 * Required raw status is separate from bool; pending native-only failure may
 * leave it OK. Use ally_alert_execution_inspect on the real pending invocation
 * for its exact created token; no scope is guessed/closed by this wrapper.
 * Exact Source query classification remains a separate parent join. */
bool mc_player_damage_alert_entered_in_execution(mc_entity_tick_execution *,
                                                 mc_world_runtime_classes *,
                                                 mc_world_runtime_execution_code, mc_entity *,
                                                 mc_entity *, mc_entity_status *,
                                                 mc_entity_effect_error *,
                                                 mc_world_runtime_execution_result *);
#endif
