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
    bool close_attempted, close_locals_drained, close_failed, close_consumed;
    bool native_close_attempted;
    mc_world_storage_status initial_close_status, native_close_status;
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

/* Close ONLY this retained call's actual recorded World65 token, without any
 * Source execution or retained-call retirement. Require the original stable,
 * exclusive execution-owned call/result/code/owner graph until completion.
 * Early-return/unreached-World calls have no such record: retire them normally,
 * do not invent a close or require World/Math just to clean up that path.
 *
 * The actual initial ally_close saves locals-drained and its exact failed
 * operand immediately before pop, after the SAME body returned. This proves
 * only its Source/query/Math/World temporary calls ended, not that retained
 * Player/target/World/bounds/List/producer rights were released. No active call
 * may use this operation. initial_close_status is that actual initial return.
 * native_close_status is assigned only after an actual Native cleanup pop;
 * admission/no-op does not fabricate an event. Original query_status,
 * query_returned, completed, Source state/error/Bool, attack and Math remain
 * independent and unchanged. Native OK never promotes a previous false begin.
 *
 * Matching OPEN must be the exact checked current token. An unrelated current
 * descendant or any explicit pending/root/child-close record refuses; caller
 * resolves only its real recorded continuations first. A completed selection
 * is retained, neither reset nor interpreted as pending. At most ONE actual
 * pop occurs per eligible call, always with the saved original failed operand.
 * Immediate Native return is noted without clearing the first owned failure.
 * Positive same-generation FREE/RETAINED proves consumption; consumed non-OK
 * is never popped again, including after slot reuse. A generation mismatch by
 * itself proves nothing. Native OK means only this own token is already/now
 * closed, not that Source/result/Math/shared execution is quiescent.
 *
 * No raw diagnostic/ambient admission inference, other-scope pop, capture,
 * producer release, owner release, result reset, Source retry/resume, field
 * repair, Math/execution retirement or automatic effect cleanup occurs. The
 * actual retained call and all typed borrows stay live; only after closure may
 * the caller use its existing checked ally_alert_retire and later outer
 * producer/Math/context retirement. This is a future victim dependency seam,
 * not full damage dispatch or current flight runtime qualification.
 */
mc_world_storage_status mc_player_ally_alert_native_close(mc_player_ally_alert_call *);

#endif
