#ifndef MC_ENTITY_MOB_DAMAGE_EXECUTION_INTERNAL_H
#define MC_ENTITY_MOB_DAMAGE_EXECUTION_INTERNAL_H

#include "../../world/runtime/runtime_execution_internal.h"
#include "mc/entity/movement/effects.h"

typedef enum mc_living_damage_operation {
    MC_LIVING_DAMAGE_NO_OPERATION,
    MC_LIVING_DAMAGE_SOUND,
    MC_LIVING_DAMAGE_KNOCKBACK
} mc_living_damage_operation;

/* Stable caller frame, borrowed pointers + ordinary VALUE observations only.
 * Initialize once with a fresh/disjoint result for one actual leaf invocation;
 * never overwrite a live frame/result or re-enter Source after a false return.
 * Caller owns actual held READY canonical Living actor, defining Registry /
 * runtime / Entity / IN graph and every external callback/resource dependency.
 * code_classes is actual defining code, independent of Entity.world/Chunk.d;
 * execution->math is real stable storage whose declared IN may remain NULL
 * until reached LS.a(SN,IDD)9. No Math/Registry ensure happens at admission.
 * All inputs/error/frame/result and actual owners are exclusive/disjoint.
 *
 * world_returned means the one actual owned C sound entry returned, not that
 * Source completed. Its raw status and error are not continuation witnesses.
 * A false result may leave Source status and runtime ownership status OK when
 * the independent actual Math channel refuses. Preserve all current typed
 * frames; legacy native_policy is not proof of an arbitrary Source classifier.
 *
 * scope_open describes ONLY this actual created token; it never means the
 * entire context is quiescent. On false retain this frame/result/typed views
 * until the parent's exact native unwind and genuine context/Math retirement.
 * The Source-return path attempts its close once. Explicit Native-only close
 * below uses its recorded operand and exact token generation/phase VALUE.
 * No ambient scope selection or receipt inference.
 */
typedef struct mc_living_damage_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_result *owned;
    mc_entity_tick_execution_world_token world;
    mc_entity_tick_execution_site receiver_site;
    mc_living_damage_operation operation;
    mc_world_storage_status world_status;
    mc_world_storage_status initial_close_status, native_close_status;
    bool entered, completed, scope_created, scope_open, close_attempted, world_returned;
    bool close_locals_drained, close_failed, close_consumed, native_close_attempted;
} mc_living_damage_execution;

void mc_living_damage_execution_init(mc_living_damage_execution *, mc_entity_tick_execution *,
                                     mc_world_runtime_classes *,
                                     mc_world_runtime_execution_result *);

/* SAME actual existing sound/knockback bodies, not the whole victim dispatch.
 * Sound captures fresh nullable World at LS279/327 before virtual name/volume
 * and both Random draws; reaches one real owned World sound311/359. It pairs
 * only the actual created scope after the child's ordinary calls/holds drain.
 * Knockback uses the actual shared Math require at9 and original arithmetic /
 * velocity-store order; unused source/amount operands remain unused.
 * Bool preserves exact body plus checked Native continuation independently.
 * No scope/Math retirement, fake leaf result, Source fallback or replay.
 * Source error scratch is required initialized stable enclosing-frame storage.
 * Public effects/mob/Player interfaces and their Source domain remain unchanged.
 * Full damage, wake/death/drop/publication and parent cleanup/classification
 * joins remain OPEN; these definitions do not promise those missing rights.
 */
bool mc_living_damage_sound_in_execution(mc_entity *, mc_registry_classes *, bool fatal,
                                         mc_living_damage_execution *, mc_entity_effect_error *);
bool mc_living_damage_knockback_in_execution(mc_entity *, double, double,
                                             mc_living_damage_execution *,
                                             mc_entity_effect_error *);

/* Native-only closure of ONLY this recorded sound-created World token. The
 * SAME initial close records close_locals_drained and its exact close_failed
 * operand immediately before the actual pop; neither is inferred from raw
 * world_status, completed, owned/Math status or an ambient current error.
 * initial_close_status is assigned ONLY to that actual initial pop return.
 * Native cleanup never changes it, world_status, completed, operation or any
 * Source/Math diagnostic. native_close_status changes ONLY after an actual
 * Native cleanup pop; admission/no-op returns do not fabricate such an event.
 *
 * Require the original stable/exclusive frame, shared execution/result and all
 * retained owners live. A real matching OPEN token must be the checked current
 * token; unrelated descendants and any pending/close records refuse untouched.
 * Caller first resolves their genuine explicit continuations. No scan of raw
 * receipt/loader diagnostics, inferred owner, pop of another token, capture,
 * producer release, result reset, Math/World retirement or Source work occurs.
 * A selected completed loader view alone is not a pending continuation and is
 * neither cleared nor rewritten here. First owned Native failure stays latched.
 *
 * Each eligible invocation makes at most ONE real checked pop with the exact
 * stored operand. Preflight/still-OPEN refusal preserves that right. Positive same-generation
 * FREE/RETAINED phase proves consumption; a consumed non-OK close is never
 * popped again, even after slot reuse. A generation mismatch alone does NOT
 * prove consumption. Native OK means ONLY this token is already/now closed;
 * it never changes Source completion/result or claims whole-result quiescence.
 * Retained leases/loader copies remain with the shared owner until the outer
 * module expires typed borrows and performs its existing checked final cleanup.
 */
mc_world_storage_status mc_living_damage_sound_native_close(mc_living_damage_execution *);

#endif
