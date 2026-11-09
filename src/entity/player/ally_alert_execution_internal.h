#ifndef MC_ENTITY_PLAYER_ALLY_ALERT_EXECUTION_INTERNAL_H
#define MC_ENTITY_PLAYER_ALLY_ALERT_EXECUTION_INTERNAL_H

#include "../../world/runtime/collision/class_entities_execution_internal.h"
#include "mc/entity/player/ally_alert.h"

/* Private observation of the ONE actual retained invocation. VALUE token only;
 * never an inferred World, owner copy, or authority to close some other scope.
 * Context/result and actual defining runtime/Registry/Entity/IN graph are real
 * stable caller borrows through this invocation's checked retirement. */
typedef struct mc_player_ally_alert_execution_view {
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_result *owned;
    mc_entity_tick_execution_world_token world;
    bool scope_created, scope_open, query_returned, completed;
    mc_world_storage_status query_status;
} mc_player_ally_alert_execution_view;

/* SAME GS.a(Living,Z), including its actual early returns and Wolf List loop.
 * Defining code_classes is supplied independently of Player.world/Chunk.d.
 * Required fresh/disjoint stable result/status/output, existing fresh call slot.
 * Captures actual nullable World65 BEFORE Class/pooled bounds. The owned class
 * query captures each full result before projection; no after-return retain or
 * loader copy is performed in this owned path. Bool preserves exact body/close
 * continuation, independent of raw Source/status slots; inspect owned first.
 * Native preflight initializes owned only; out/status are unchanged until the
 * actual shared admission. No eager Math/Registry use or READY Source gate.
 * Normal call closes its created scope once; refusal retains actual token and
 * every result record. Parent must finish exact native unwind before retiring
 * the call. This package never retires execution/Math or retries Source.
 * Returned call keeps ordinary existing Player/target/World/bounds/List holds.
 * Existing external provider/Store/import graph contract remains caller-owned.
 * Unproved query Source classification remains explicitly unresolved; the exact
 * active nearby/Math/result frames remain available, not relabeled Native.
 */
bool mc_player_ally_alert_begin_in_execution(mc_entity_tick_execution *, mc_world_runtime_classes *,
                                             mc_world_runtime_execution_code, mc_entity *,
                                             mc_entity *, bool attack, mc_player_ally_alert_call **,
                                             mc_entity_status *,
                                             mc_world_runtime_execution_result *);
bool mc_player_ally_alert_execution_inspect(const mc_player_ally_alert_call *,
                                            mc_player_ally_alert_execution_view *);

#endif
