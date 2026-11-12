#ifndef MC_WORLD_RUNTIME_BLOCKS_EXECUTION_INTERNAL_H
#define MC_WORLD_RUNTIME_BLOCKS_EXECUTION_INTERNAL_H

#include "acquisition_internal.h"
#include "runtime_execution_internal.h"

/* One stable caller frame for the SAME World scalar bodies as blocks.h.
 * The caller owns the exact evaluated nonnull World before later arguments,
 * plus the real defining Chunk/provider/Store/import/resource graphs. code is
 * supplied by that defining caller, never inferred from World or Chunk.d.
 * Unjoined cold import/generation and foreign graph lifetimes retain their
 * explicit external contract; this descriptor does not adopt them.
 *
 * Fresh/disjoint frame, result, outputs and owners remain live/exclusive through
 * checked unwind. No World scope is begun/closed here. Only the reached GET
 * uses the kernel and captures its actual selection/pending/close records.
 * Native preflight preserves error/status/out. Raw status is assigned only
 * after the shared body returns and is NOT a continuation witness. An actual
 * OK GET with native refusal stops before payload reads, dirty/nibble stores
 * or out publication, without inventing a Source error. Retry never resumes
 * this body. Failed selections/typed acquisition views remain retained until
 * the caller restores the exact scope, drains its recorded native work and
 * clears expiring borrows before retiring owners.
 *
 * get_entered/get_returned describe the actual lower C invocation, not proof
 * that Source entered. acquisition is failure-only scratch: inspect it only
 * for a non-OK get_status; an OK getter leaves it untouched. source reports
 * the actual metadata read/write fault separately. Completed quiet success
 * alone permits frame reuse via fresh initialization; never reset pending work.
 */
typedef struct mc_world_block_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_code code;
    mc_entity_tick_execution_site receiver_site;
    mc_world_runtime_execution_result *owned;
    mc_world_runtime_acquisition_error acquisition;
    mc_world_metadata_source_error source;
    mc_chunk_acquire_status get_status;
    bool entered, get_entered, get_returned, native_stopped, completed;
} mc_world_block_execution;

void mc_world_block_execution_init(mc_world_block_execution *, mc_entity_tick_execution *,
                                   mc_world_runtime_execution_code, mc_entity_tick_execution_site,
                                   mc_world_runtime_execution_result *);

bool mc_world_runtime_block_get_in_execution(mc_world_block_execution *, mc_world_runtime *,
                                             int32_t, int32_t, int32_t, int32_t *,
                                             mc_world_storage_error *, mc_world_storage_status *);
bool mc_world_runtime_metadata_get_in_execution(mc_world_block_execution *, mc_world_runtime *,
                                                int32_t, int32_t, int32_t, int32_t *,
                                                mc_world_storage_error *,
                                                mc_world_storage_status *);
bool mc_world_runtime_metadata_set_quiet_in_execution(mc_world_block_execution *,
                                                      mc_world_runtime *, int32_t, int32_t, int32_t,
                                                      int32_t, bool *, mc_world_storage_error *,
                                                      mc_world_storage_status *);

#endif
