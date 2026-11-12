#ifndef MC_WORLD_STORAGE_SOURCE_CHUNK_LOADER_H
#define MC_WORLD_STORAGE_SOURCE_CHUNK_LOADER_H
#include "mc/block/registry/registry.h"
#include "mc/storage/nbt/tree.h"
#include "mc/world/chunk/object.h"
#include "mc/world/storage/source_region_output.h"
typedef struct mc_world_store_game_stage mc_world_store_game_stage;
typedef struct mc_world_runtime mc_world_runtime;

typedef struct mc_world_source_chunk_error {
    mc_world_source_error source;
    mc_world_source_region_error region;
    mc_gzip_error compression;
    mc_nbt_stream_error input;
    mc_world_storage_error storage;
    mc_registry_error registry;
    mc_world_source_region_output_error output;
    mc_nbt_error output_tree;
    int serializer_pc, save_pc;
} mc_world_source_chunk_error;

typedef struct mc_world_source_chunk_save_state {
    bool root_constructed, level_constructed, level_attached, prefix_returned;
    bool entities_attached, tiles_attached, nbt_written, closed, source_returned, failed;
    int last_pc, serializer_pc;
    int32_t growth;
    const mc_world_source_region_output *output;
    const mc_nbt_tree *document, *entity_records, *tile_records;
} mc_world_source_chunk_save_state;

typedef struct mc_world_source_chunk_loader_state {
    mc_world_source_phase phase;
    size_t holds;
    bool active, retiring;
    mc_world_store *store;
    mc_java_file *source_a, *pending_file;
    mc_world_source_regions *regions;
    const mc_world_source_region_record *record;
    const mc_nbt_stream *reader;
    const mc_nbt_document *document;
    const mc_world_source_chunk_error *failure, *cleanup;
    int last_pc;
    mc_world_source_chunk_save_state save;
} mc_world_source_chunk_loader_state;

/* Native candidate allocation holds Store and the SAME shared rj. Actual
 * ld(File) only stores its File. No region/file open during construction. */
mc_world_source_status mc_world_source_chunk_loader_allocate(mc_world_store *,
                                                             mc_world_source_regions *,
                                                             mc_world_source_chunk_loader **,
                                                             mc_world_source_error *);
mc_world_source_status mc_world_source_chunk_loader_construct(mc_world_source_chunk_loader *,
                                                              mc_java_file *actual_world_directory,
                                                              bool nether, mc_world_source_error *);
mc_world_source_status mc_world_source_chunk_loader_retain(mc_world_source_chunk_loader *,
                                                           mc_world_source_error *);
mc_world_source_status mc_world_source_chunk_loader_release(mc_world_source_chunk_loader **,
                                                            mc_world_source_error *);
mc_world_source_status mc_world_source_chunk_loader_retire(mc_world_source_chunk_loader **,
                                                           mc_world_source_error *);
bool mc_world_source_chunk_loader_inspect(const mc_world_source_chunk_loader *,
                                          mc_world_source_chunk_loader_state *);
/* Actual source transport/parse into canonical Store GAME stage. Imports and
 * final Chunk.i stay in the existing runtime loader. NULL stage is source
 * absence. Errors retain the full real transport/parse prefix inside ld. */
mc_world_storage_status mc_world_source_chunk_loader_begin(mc_world_source_chunk_loader *,
                                                           mc_world_runtime *, int32_t dimension,
                                                           int32_t x, int32_t z, size_t,
                                                           mc_world_store_game_stage **,
                                                           mc_world_storage_error *);
/* Actual ld.a save body after its OUTSIDE-catch World.r at1. Begin opens the
 * concrete rj.d stream at16 BEFORE fresh root24/Level33/attach44. Prefix runs
 * the actual second session check and snapshots genuine to.a fields before q105.
 * Native allocation/shape policies are separate from Java mutable-NBT/OOM timing.
 * A failed/incomplete attempt remains owned and blocks another save; terminal
 * native release never finishes/commits it. A returning previous attempt may be
 * natively cleared by the next begin. All calls are serial/exclusive. */
mc_world_storage_status mc_world_source_chunk_loader_save_begin(mc_world_source_chunk_loader *,
                                                                mc_world_chunk *,
                                                                const mc_java_output_options *,
                                                                mc_world_source_chunk_error *);
mc_world_storage_status mc_world_source_chunk_loader_save_prefix(mc_world_source_chunk_loader *,
                                                                 mc_world_runtime *,
                                                                 mc_world_chunk *,
                                                                 mc_world_source_chunk_error *);
/* Transfer an actual independently owned semantic list tree at to.a208/289.
 * Success consumes/nulls the input slot. Failure preserves it. This retains
 * partial fresh composition safely if a later writer fails. */
mc_world_storage_status mc_world_source_chunk_loader_save_entities(mc_world_source_chunk_loader *,
                                                                   mc_nbt_tree **,
                                                                   mc_world_source_chunk_error *);
mc_world_storage_status mc_world_source_chunk_loader_save_tiles(mc_world_source_chunk_loader *,
                                                                mc_nbt_tree **,
                                                                mc_world_source_chunk_error *);
/* Both transferred list trees are fresh semantic results, with no preserved opaque
 * records. Deep-copy the actual fresh root/Level, plain NBT57, explicit close61,
 * actual Info65/size74, late rj.b89 and signed growth addition94. No source
 * finally, early timestamp, legacy writer or native Info fallback is supplied.
 * Failed streaming input remains owned by this loader through checked cleanup. */
mc_world_storage_status mc_world_source_chunk_loader_save_write(mc_world_source_chunk_loader *,
                                                                mc_world_runtime *,
                                                                mc_world_chunk *,
                                                                const mc_nbt_limits *, uint32_t *,
                                                                mc_world_source_chunk_error *);
/* Coordinator failure publication only: retains a completed typed failure,
 * disables this incomplete attempt and future loader source work, and performs
 * no close/finish/commit. Repeated publication preserves the first failure. */
mc_world_storage_status mc_world_source_chunk_loader_save_fail(mc_world_source_chunk_loader *,
                                                               const mc_world_source_chunk_error *);
const mc_world_source_chunk_error *
mc_world_source_chunk_loader_last_error(const mc_world_source_chunk_loader *);
mc_world_storage_status
mc_world_source_chunk_loader_prepare_normalizer(mc_world_source_chunk_loader *, mc_world_runtime *,
                                                mc_world_storage_error *);
#endif
