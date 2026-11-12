#ifndef MC_WORLD_RUNTIME_SOURCE_CONSTRUCTION_H
#define MC_WORLD_RUNTIME_SOURCE_CONSTRUCTION_H
#include "mc/world/runtime/construction.h"
#include "mc/world/saved_data/source_storage.h"
#include "mc/world/storage/source_chunk_loader.h"

/* Passive World-owned source-call diagnostics. Each typed error is retained by
 * value in this SAME World, independently of later Store work slots. Child
 * owner views remain borrowed and expire when consumed during native cleanup;
 * copied errors remain live until the World is actually freed. No getter enters
 * source construction, performs IO, retains an occurrence or creates a World. */
typedef struct mc_world_runtime_source_construction_state {
    const mc_world_store *store;
    const mc_world_saved_data *pending_saved_data;
    const mc_world_source_chunk_loader *loader;
    bool store_held, saved_data_published, metadata_called, metadata_returned;
    bool loaded_info_was_null, retiring;
    const mc_world_source_error *retention, *metadata, *loader_failure;
    const mc_world_source_error *store_cleanup, *loader_cleanup;
    const mc_world_saved_data_source_error *saved_data_failure, *saved_data_cleanup;
} mc_world_runtime_source_construction_state;

/* False for a legacy/composition World or before source ownership allocation;
 * output stays unchanged. Successful cleanup can consume z/pending HC/ld
 * before a later child blocks: consumed views are NULL, remaining owners and
 * all errors stay inspectable for retry. No source store/clear is credited. */
bool mc_world_runtime_source_construction_inspect(const mc_world_runtime *,
                                                  mc_world_runtime_source_construction_state *);
/* Last reached source metadata error, otherwise the source Store-retain cause.
 * Use the state above for the separate HC/ld and native cleanup diagnostics. */
const mc_world_source_error *mc_world_runtime_last_source_store_error(const mc_world_runtime *);
#endif
