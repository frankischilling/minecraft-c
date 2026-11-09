#ifndef MC_WORLD_RUNTIME_ENTITY_INSERTION_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_ENTITY_INSERTION_EXECUTION_INTERNAL_H

#include "../chunk/entity_insertion_execution_internal.h"
#include "entity_publication_detailed_execution_internal.h"

/* Stable private caller frame. All pointers are explicit borrows and every
 * diagnostic/result/token observation is VALUE storage, never a copied owner.
 * Actual execution/Math, defining client FD/LM/LI code/Registry/Entity graph,
 * exact evaluated nullable World scope and complete external provider/Store /
 * import/producer graph stay live/exclusive through native unwind/retirement.
 * The caller owns that World BEFORE later Entity/constructor/argument work.
 * code passes unchanged to the kernel; receiver_site is that actual defining
 * caller's captured load site, not current execution/World/Chunk.d inference.
 * One real IN namespace in execution->math defines this joined client FD/LM
 * code; mutable World/Chunk fields never select another Math implementation.
 * Foreign code graphs remain an explicit unjoined external caller contract.
 *
 * Fresh/disjoint enclosing storage only; never reinitialize a live result/frame.
 * acquisition/chunk are the actual stable lower frames BEFORE projection.
 * get/chunk/observer_returned means only the actual selected C entry returned;
 * it is not proof that Source ran or that raw OK authorizes continuation.
 * Native/Math-only stops may leave compact Source status OK. Preserve exact
 * bool plus runtime result, active lower frame and actual Math cause separately.
 */
typedef struct mc_world_entity_insert_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
    mc_entity_tick_execution_site receiver_site;
    mc_world_runtime_execution_result *owned;
    mc_world_runtime_acquisition_error acquisition;
    mc_world_chunk_entity_error chunk;
    mc_world_storage_status chunk_status;
    bool entered, body_entered, body_completed, completed;
    bool get_returned, chunk_returned, observer_returned;
} mc_world_entity_insert_execution;

void mc_world_entity_insert_execution_init(mc_world_entity_insert_execution *,
                                           mc_entity_tick_execution *, mc_world_runtime_classes *,
                                           mc_world_runtime_execution_code,
                                           mc_entity_tick_execution_site,
                                           mc_world_runtime_execution_result *);

/* SAME complete actual World.b(Entity)Z. True means Source/native continuation
 * completed; *out is the genuine Source boolean and may be false on nonplayer
 * absence. false preserves out and all reached q/membership/List/callback effects.
 * Required fresh stable error/status and frame/result are disjoint from owners.
 * Native preflight notes result only, preserving out/status/error; no missing
 * context selects legacy. Exact current IDLE admission precedes Source fields.
 * FD8/20, presence45, Player d70/y77, ONE actual GET83, Blank/normal Chunk87,
 * World List95 and genuine already-owned observer103 stay in original order.
 * Normal LM13/25/81 uses the SAME Math-aware insertion body, with q2 first.
 * Full actual GET/selection/pending/close result is captured before compact
 * projection. Only completed successful NONE with quiet continuations clears.
 * Actual OK/native refusal stops BEFORE Chunk87/List95/observer103/out.
 * No batch substitution, after-return receipt retain, loader/source replay,
 * actor/world-field repair, scope pop, cancellation or context/Math retirement.
 * Caller owns exact checked unwind and clears typed borrows before retirement.
 * Existing wrong-location diagnostic and concrete unsupported cases remain real
 * refusals. Complete drop/scatter/death/bed callers remain separate OPEN joins.
 */
bool mc_world_runtime_insert_entity_in_execution(mc_world_entity_insert_execution *,
                                                 mc_world_runtime *, mc_entity *, bool *,
                                                 mc_world_entity_error *,
                                                 mc_world_storage_status *);

#endif
