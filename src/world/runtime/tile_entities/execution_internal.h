#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_EXECUTION_INTERNAL_H

#include "../runtime_execution_internal.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

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
 * Unsupported concrete factory/Block behavior keeps its existing real refusal.
 * Returned Tile owns the actual ordinary result hold; failure preserves out.
 * Other public set/Block/deferred callers still use their separate legacy route. */
mc_world_storage_status mc_live_tile_entities_get_in_execution(
    mc_entity_tick_execution *, mc_world_runtime *, mc_world_runtime_execution_code,
    mc_registry_classes *defining_registry, mc_world_runtime_classes *factory_classes,
    mc_entity_tick_execution_site lazy_receiver_site, int32_t, int32_t, int32_t, mc_tile_entity **,
    mc_live_tile_entity_error *, mc_world_runtime_execution_result *);

#endif
