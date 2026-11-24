#ifndef MC_WORLD_RUNTIME_COLLISION_ENTITIES_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_COLLISION_ENTITIES_EXECUTION_INTERNAL_H

#include "../runtime_execution_internal.h"
#include "mc/world/runtime/collision/entities.h"

/* ONE complete FD.b(Entity,AABB) query, provider GET and actual normal/Blank
 * gather body, shared with the old public/held NULL-descriptor entries.
 * Caller already owns this exact evaluated nullable World BEFORE the later
 * arguments and keeps actual bounds/code/provider/Store/import/resource graph
 * live and exclusive. code is unchanged actual defining Chunk code VALUE.
 * This initial owned domain is actual client FD/LM/LI code; execution->math is
 * its explicit actual defining IN context, borrowed live until reached use,
 * not World.trig or inferred from mutable World/Chunk.d. Other code namespaces
 * and arbitrary foreign-Store graph joins remain OPEN. No eager Math use,
 * READY gate, source field repair or extra World/Entity ownership is added.
 *
 * execution/result/status/error/out are required, fresh, stable, exclusive and
 * disjoint enclosing-frame storage (out has no prior List lease). The actual
 * error is used directly through lower calls, so synchronous real callbacks
 * can observe its current source prefix. Native preflight initializes a native
 * diagnostic; it preserves out, source fields and actual existing rights.
 * Missing/invalid ownership never falls back to legacy. Exact current IDLE node
 * admission precedes call_enter and all Source reads; HELD/ARMED refuses.
 * The World call and temporary excluded occurrence pair only their own rights.
 *
 * Return true ONLY on complete Source return with one actual mutable R lease
 * in out. false preserves out and reached Source effects. *status is this C
 * body's raw storage channel, NOT a continuation/Throwable witness: a reached
 * Math or owned-completion stop can leave it OK. result retains participating
 * native-first, pending2 and close8 continuations independently; the explicit
 * Math context retains its own first/current/cleanup. Inspect bool FIRST, then
 * actual typed channels. Never classify a Java Throwable from numeric status.
 *
 * R.clear before bounds/FP; FD argument arithmetic before IN21/37/54/71;
 * normal LM arithmetic before IN12/29, Blank literal return before normal
 * bounds/Math. Presence102, one reached actual GET113, current R119, gather122
 * and returned R138 remain ordered. Every occurrence gets its own real hold;
 * duplicates/partial lists remain and no source-dead/phase/World filter appears.
 *
 * Kernel alone reserves/finishes each actual GET. A failed completed explicit
 * selection rewires source_chunk to the stable selected view (NONE stays NULL).
 * Refused selection read suppresses that unsafe outward borrow and preserves
 * Source cause/token/native error. Pending direct views remain protected by
 * their exact unfinished actual World plus the existing external producer graph.
 * Actual OK with native refusal stops before gather and R119; only completed
 * successful NONE with no pending/close records is cleared before another GET.
 * A later Math/List failure cannot inherit an active old failed selection.
 *
 * Parent owns checked unwind of actual published capsules/pending attempts and
 * its own scope, clears typed borrows before lease/World retirement, releases
 * successful List lease and independently retires Math. This child does none
 * of those, never resumes Source after failure, cancels a loader, retries a
 * getter or uses an ambient error/owner. Full SL and unjoined import/generation/
 * population/captured-provider lifetime closure remain separate OPEN work.
 */
bool mc_world_runtime_nearby_entities_in_execution(
    mc_entity_tick_execution *, mc_world_runtime *, mc_world_runtime_execution_code, mc_entity *,
    const mc_entity_bounds *, mc_world_entity_list_view *, mc_world_nearby_error *,
    mc_world_storage_status *, mc_world_runtime_execution_result *);

/* Shared reached actual acquiring getter for the distinct Entity queries.
 * Caller holds/enters exact NONNULL World and keeps the actual state/code graph
 * live. frame is the same initialized caller-stable prefix; result was freshly
 * initialized by the outer operation and MUST be quiet before this getter.
 * This helper does NOT initialize/discard result, clear R, floor bounds, probe
 * presence, gather, enter/leave the World call or close any scope. Native exact
 * current IDLE admission precedes provider fields even when called directly.
 * true means actual GET and native completion both returned normally. false
 * preserves actual typed failure/result; raw frame status may still be OK.
 * Same one acquire body underlies the legacy held helper and both query routes.
 */
bool mc_world_entity_query_acquire_in_execution(mc_entity_tick_execution *, mc_world_runtime *,
                                                mc_world_runtime_execution_code, int32_t, int32_t,
                                                mc_chunk_state **, mc_world_nearby_error *,
                                                mc_world_runtime_execution_result *);

#endif
