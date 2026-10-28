#ifndef MC_WORLD_STORAGE_SOURCE_HANDLER_INTERNAL_H
#define MC_WORLD_STORAGE_SOURCE_HANDLER_INTERNAL_H
#include "../../platform/time/millis.h"
#include "mc/world/storage/source_handler.h"

struct mc_world_store_source_classes {
    size_t holds, handlers, literal_allocations;
    bool active;
    mc_world_source_phase phase;
    int last_store_pc;
    mc_java_files *files;
    mc_java_logging *logging;
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
} mc_world_store_source;

static mc_world_source_status source_input_clear(mc_world_store_source *);
static mc_world_storage_status store_source_legacy(mc_world_storage_error *error);
static mc_world_storage_status store_source_checked_free(mc_world_store **slot,
                                                         mc_world_storage_error *error);
#endif
