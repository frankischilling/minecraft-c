#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_EXECUTION_INTERNAL_H

#include "../runtime_execution_internal.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

typedef struct mc_live_tile_entity_query_execution_call mc_live_tile_entity_query_execution_call;
struct mc_world_mutation_error;

/* Actual lazy Chunk.d67 receiver and reached virtual Block.c child. This is
 * stable caller-owned work, separate from copyable canonical result VALUES.
 * The caller captures the exact current producer and drains actual pending
 * outcomes before native close. Close/retire never resumes a Source body.
 * Failed close retains its exact scope and typed child for an idempotent retry.
 * A successful lookup retires this owner before its Map reread98. A failed
 * lookup leaves it until checked parent retirement, even if its scope closed. */
bool mc_live_tile_entity_query_execution_owns_world(
    const mc_live_tile_entity_query_execution_call *, const mc_entity_tick_execution_world_token *);
mc_world_storage_status
mc_live_tile_entity_query_execution_native_close(mc_live_tile_entity_query_execution_call *);
mc_world_storage_status
mc_live_tile_entity_query_execution_retire(mc_live_tile_entity_query_execution_call **);
const struct mc_world_mutation_error *
mc_live_tile_entity_query_execution_failure(const mc_live_tile_entity_query_execution_call *);
/* Passive copied defining capture site from an actual retained invocation,
 * including after its checked pop. No new Source call or owning hold. */
bool mc_live_tile_entity_query_execution_capture_site(
    const mc_live_tile_entity_query_execution_call *, mc_entity_tick_execution_site *);
bool mc_live_tile_entity_query_execution_source_null(
    const mc_live_tile_entity_query_execution_call *, mc_entity_tick_execution_site *);

/* Caller already captured nullable B at the actual Chunk.d receiver load and
 * holds its entered World call. Run the SAME complete Map registration body
 * under that exact receiver/Store identity, never a reread of mutable Chunk.d
 * for cleanup. No acquisition occurs. Required fresh/disjoint result is empty
 * unless native admission/ownership refuses. Legacy uses NULL internally. */
mc_world_storage_status mc_live_tile_entities_chunk_register_in_execution(
    mc_entity_tick_execution *, mc_world_runtime *, mc_world_runtime_execution_code,
    mc_world_chunk *, mc_live_tile_entity_error *, mc_world_runtime_execution_result *);

/* Already-open exact evaluated World B; the SAME World.b(III,Tile)V body still
 * owns its ordinary active call and temporary Tile/WA occurrences. Its one
 * optional acquisition uses the ONE owned kernel route. Full current loader
 * selection/pending ledger is retained in result BEFORE public Tile error
 * projection. Actual callback55 moves its actual local WA right through the
 * lease before either ordinary temporary release; no post-return retain.
 * This child neither closes B nor retires execution/Math. NULL error is legal;
 * NULL/invalid context or result refuses natively, never legacy fallback. */
mc_world_storage_status mc_world_runtime_tile_entity_dirty_in_execution(
    mc_entity_tick_execution *, mc_world_runtime *, mc_world_runtime_execution_code, int32_t,
    int32_t, int32_t, mc_tile_entity *, mc_live_tile_entity_error *,
    mc_world_runtime_execution_result *);

/* Actual inherited ow.y_/jh.i body. Preserve d1 null guard, then capture fresh
 * d8 before x12/y16/z20 and invoke24. actual_receiver_site comes from that
 * defining code's caller, not mutable Tile.world; code passes unchanged into
 * nested acquisition/activation. Every pushed B gets an exact checked pop.
 * Refused structural unwind retains B and every VALUE pending outcome; caller
 * corrects native fault and resumes native-only, never another getter/Source
 * call. Other inventory/get/set/Block/deferred caller threading remains OPEN.
 * All arguments/owners/results/errors must be live, exclusive and disjoint. */
mc_tile_entity_status
mc_tile_entity_dirty_in_execution(mc_entity_tick_execution *, mc_tile_entity *,
                                  mc_world_runtime_execution_code,
                                  mc_entity_tick_execution_site actual_receiver_site,
                                  mc_live_tile_entity_error *, mc_world_runtime_execution_result *);

/* Full existing supported World lookup and lazy Block/factory/set chain, not
 * an already-loaded-only route. Required caller-held defining Registry/factory
 * classes are used ONLY at reached static42/53 and constructor15 respectively.
 * No early ensure/READY/side/equality test masks Map/id/argument failures.
 * Actual Chunk.d67 is captured before j71/k81 coordinates and Block.c89;
 * lazy_receiver_site names that defining lm.d/hi.d code, never mutable d.
 * The same kernel code bundle and complete result ledger pass unchanged into
 * each actually reached acquisition; no extra get or Source retry is run.
 * code_classes supplies the explicit retained mutation/World code graph;
 * it is independent of the lookup Registry and virtual factory graph. The
 * required lazy_call slot starts NULL. A loaded result allocates no owner.
 * Reentry with a live owner refuses without changing out/error/owned; only
 * checked native close/retire can consume that preceding failed invocation.
 * Unsupported concrete factory/Block behavior keeps its existing real refusal.
 * Returned Tile owns the actual ordinary result hold; failure preserves out.
 * Other public set/Block/deferred callers still use their separate legacy route. */
mc_world_storage_status mc_live_tile_entities_get_in_execution(
    mc_entity_tick_execution *, mc_world_runtime *, mc_world_runtime_execution_code,
    mc_world_runtime_classes *code_classes, mc_registry_classes *defining_registry,
    mc_world_runtime_classes *factory_classes, mc_entity_tick_execution_site lazy_receiver_site,
    int32_t, int32_t, int32_t, mc_tile_entity **, mc_live_tile_entity_error *,
    mc_world_runtime_execution_result *, mc_live_tile_entity_query_execution_call **);

/* Actual RW.c World argument is already owned by the caller's exact current B.
 * Reuse the SAME base6 / virtual factory15 / World install18 body. Defining
 * Registry, factory classes and kernel code remain caller values; none comes
 * from mutable Chunk.d. The stable fresh child error and initialized canonical
 * result belong to that caller through checked native unwind. Do not reset the
 * result, retry Source, close B or fall back to the legacy route here. NULL B
 * still reaches the actual factory15 before its invocation failure18. The
 * required source_null output is true only for that reached NULL receiver,
 * never for native admission, factory or acquisition failure. */
mc_world_storage_status mc_live_tile_entities_block_added_in_execution(
    mc_entity_tick_execution *, mc_world_runtime *, mc_world_runtime_execution_code,
    mc_registry_classes *defining_registry, mc_world_runtime_classes *factory_classes,
    mc_entity_tick_execution_site receiver_site, const mc_block *, int32_t, int32_t, int32_t,
    mc_live_tile_entity_error *, mc_world_runtime_execution_result *, bool *source_null);

/* Actual inherited Sign RW.b: literal base6, then World.p invocation14.
 * Both World acquisitions use the same caller-owned result and defining
 * code/Registry/factory operands. A failed lazy lookup keeps its real owner
 * until native close/retirement; this body never retries Source or resets the
 * enclosing ledger. The exact evaluated World token is already open. */
mc_world_storage_status mc_live_tile_entities_sign_removed_in_execution(
    mc_entity_tick_execution *, mc_world_runtime *, mc_world_runtime_execution_code,
    mc_world_runtime_classes *code_classes, mc_registry_classes *defining_registry,
    mc_world_runtime_classes *factory_classes, mc_entity_tick_execution_site, int32_t, int32_t,
    int32_t, mc_live_tile_entity_error *, mc_world_runtime_execution_result *,
    mc_live_tile_entity_query_execution_call **, bool *source_null);

#endif
