#ifndef MC_WORLD_RUNTIME_SOURCE_CONSTRUCTION_INTERNAL_H
#define MC_WORLD_RUNTIME_SOURCE_CONSTRUCTION_INTERNAL_H
#include "mc/world/runtime/source_construction.h"

typedef struct mc_world_runtime_source_construction {
    mc_world_store *store;
    mc_world_saved_data *pending_saved_data;
    mc_world_source_chunk_loader *loader;
    bool saved_data_published, metadata_called, metadata_returned;
    bool loaded_info_was_null, retiring;
    mc_world_source_error retention, metadata, loader_failure;
    mc_world_source_error store_cleanup, loader_cleanup;
    mc_world_saved_data_source_error saved_data_failure, saved_data_cleanup;
} mc_world_runtime_source_construction;

/* Exclusive same-World helpers. Legacy Stores retain their original borrowed
 * transport; actual source Stores acquire a distinct field occurrence before
 * w publication. No generic Store/root or source constructor is substituted. */
mc_world_storage_status mc_world_runtime_source_bind_store(mc_world_runtime *, mc_world_store *,
                                                           mc_world_storage_error *);
mc_world_storage_status mc_world_runtime_source_saved_data(mc_world_runtime *,
                                                           mc_world_storage_error *);
mc_world_storage_status mc_world_runtime_source_read_info(mc_world_runtime *, mc_world_info **,
                                                          mc_world_storage_error *);
mc_world_storage_status mc_world_runtime_source_create_loader(mc_world_runtime *,
                                                              mc_world_storage_error *);
mc_world_source_chunk_loader *mc_world_runtime_source_loader(mc_world_runtime *);
/* Call only after all existing World/cache/registration/active-call preflights.
 * HC/ld retirement and final source Store release may fail; no irreversible
 * generic World free may precede this stage. The existing provider destructor
 * only releases arrays/generator/Biome references and never uses ld or Store.
 * Clear each consumed owning slot immediately; failed slots remain for retry. */
mc_world_storage_status mc_world_runtime_source_prepare_free(mc_world_runtime *,
                                                             mc_world_storage_error *);
void mc_world_runtime_source_free(mc_world_runtime *);
#endif
