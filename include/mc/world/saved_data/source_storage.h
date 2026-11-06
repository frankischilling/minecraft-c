#ifndef MC_WORLD_SAVED_DATA_SOURCE_STORAGE_H
#define MC_WORLD_SAVED_DATA_SOURCE_STORAGE_H

#include "mc/world/saved_data/storage.h"
#include "mc/world/storage/source_handler.h"

typedef enum mc_world_saved_data_source_phase {
    MC_WORLD_SAVED_DATA_SOURCE_RAW,
    MC_WORLD_SAVED_DATA_SOURCE_CONSTRUCTING,
    MC_WORLD_SAVED_DATA_SOURCE_READY,
    MC_WORLD_SAVED_DATA_SOURCE_FAILED,
    MC_WORLD_SAVED_DATA_SOURCE_RETIRING
} mc_world_saved_data_source_phase;

typedef struct mc_world_saved_data_source_error {
    mc_world_storage_status status, cause_status;
    const char *operation;
    mc_world_source_site site, cause_site;
    bool native_policy, source_exception, caught_exception, diagnostic_frontier;
    mc_world_source_error handler;
    mc_java_file_error file;
    mc_nbt_stream_error input;
    mc_world_storage_error storage;
    mc_world_info_status string_status;
} mc_world_saved_data_source_error;

/* Passive borrowed views. No copied slot supplies an owned occurrence. The
 * reader is unique: only state_read is admitted through this view. Native
 * retirement of failed input prefixes never credits the source close62. */
typedef struct mc_world_saved_data_source_state {
    mc_world_saved_data_source_phase phase;
    mc_world_info_side side;
    bool active, b_stored, c_stored, d_stored, a_stored;
    bool map_cleared, file_stored, exists, input_constructed, parsed;
    bool close_returned, load_returned, constructor_returned;
    int last_store_pc, last_body_pc, close_pc;
    size_t imported_shorts;
    mc_world_store *source_a;
    const void *source_b, *source_c, *source_d;
    mc_world_info_name *idcounts;
    mc_java_file *file;
    mc_java_file_stream *stream;
    mc_nbt_stream *reader;
    const mc_nbt_document *document;
    const mc_world_saved_data_source_error *failure, *cleanup;
} mc_world_saved_data_source_state;

/* Actual hc (client) / es (server) NEW identity, with all source fields still
 * null. The explicit side selects the original manager base-type domain and
 * bytecode sites. Constructor collections reuse this owner's canonical
 * descriptors; Java collection heap-allocation timing/bucket order is not
 * qualified. Bounds charge the owner and native source sidecar together;
 * input/document/name allocations retain their separate canonical bounds. */
mc_world_storage_status mc_world_saved_data_source_allocate(
    mc_world_info_side side, const mc_world_saved_data_limits *limits, mc_world_saved_data **out,
    mc_world_saved_data_source_error *error);

/* One constructor attempt. Publish b12/c23/d34, independently retain the SAME
 * nullable source Store at a39, then actual raw idcounts File/FIS/DIS/NBT path.
 * No legacy path copy or compressed input. Source DIS.close62 is reached only
 * after successful parsing. A caught Java exception stops honestly at the
 * unimplemented Exception.printStackTrace152 and retains its open prefix.
 * Native limits/allocations remain native failures. Neither failure replays.
 * Current nonnull source Store is the client mx/fm namespace; a server es
 * receiving it stops explicitly at the distinct om.b(String)23 dependency.
 * A null Store completes the actual no-file constructor for either side.
 * The Store and all arguments are live/exclusive throughout the synchronous
 * call; inspect/retire are the only operations admitted on a failed owner. */
mc_world_storage_status
mc_world_saved_data_source_construct(mc_world_saved_data *owner,
                                     mc_world_store *nullable_source_store,
                                     mc_world_saved_data_source_error *error);

bool mc_world_saved_data_source_inspect(const mc_world_saved_data *owner,
                                        mc_world_saved_data_source_state *out);

/* Checked native retirement, including RAW/failed source and legacy owners.
 * BUSY or lower cleanup failure preserves the remaining owner slot for retry;
 * consumed child occurrences stay null. Native close is not source close.
 * A null slot is an idempotent success. Calls and error/slot storage must be
 * exclusive, live and disjoint from the owner and its borrowed state views.
 * Legacy void free deliberately leaves source owners for this checked API. */
mc_world_storage_status mc_world_saved_data_retire(mc_world_saved_data **slot,
                                                   mc_world_saved_data_source_error *error);

#endif
