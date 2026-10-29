#ifndef MC_WORLD_STORAGE_SOURCE_HANDLER_INTERNAL_H
#define MC_WORLD_STORAGE_SOURCE_HANDLER_INTERNAL_H
#include "../../platform/time/millis.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/storage/source_chunk_loader.h"
#include "mc/world/storage/source_handler.h"

struct mc_world_store_source_classes {
    size_t holds, handlers, literal_allocations;
    bool active;
    mc_world_source_phase phase;
    int last_store_pc;
    mc_java_files *files;
    mc_java_logging *logging;
    mc_world_source_regions *regions;
    uint8_t block_map[256];
    bool block_map_ready;
    bool block_map_failed;
    mc_world_source_error block_map_failure;
    mc_registry_error block_map_registry;
    mc_java_logger *source_a, *pending_logger;
    mc_world_info_name *minecraft, *players, *data, *session_lock;
    mc_world_info_name *level_name, *old_name, *data_key;
    mc_world_source_options options;
    mc_world_source_error failure, cleanup, work;
};

typedef struct mc_world_store_source {
    mc_world_source_state state;
    mc_world_store_source_classes *classes;
    mc_world_source_error failure, cleanup, work;
    mc_world_source_input_state *input;
    mc_java_file *data_file;
    mc_world_info_name *data_name;
} mc_world_store_source;

static mc_world_source_status source_input_clear(mc_world_store_source *);
static mc_world_storage_status store_source_legacy(mc_world_storage_error *error);
static mc_world_storage_status store_source_native_admit(const mc_world_store *,
                                                         mc_world_storage_error *);
static mc_world_storage_status store_source_save_frontier(mc_world_store *,
                                                          mc_world_storage_error *);
struct store_chunk;
static mc_stored_chunk_status store_source_normalize(mc_world_store *, struct store_chunk *,
                                                     mc_world_storage_error *);
static mc_world_storage_status store_source_checked_free(mc_world_store **slot,
                                                         mc_world_storage_error *error);
#endif
