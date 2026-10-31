#ifndef MC_WORLD_STORAGE_SOURCE_CHUNK_LOADER_H
#define MC_WORLD_STORAGE_SOURCE_CHUNK_LOADER_H
#include "mc/block/registry/registry.h"
#include "mc/world/storage/source_regions.h"
typedef struct mc_world_store_game_stage mc_world_store_game_stage;
typedef struct mc_world_runtime mc_world_runtime;

typedef struct mc_world_source_chunk_error {
    mc_world_source_error source;
    mc_world_source_region_error region;
    mc_gzip_error compression;
    mc_nbt_stream_error input;
    mc_world_storage_error storage;
    mc_registry_error registry;
} mc_world_source_chunk_error;

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
const mc_world_source_chunk_error *
mc_world_source_chunk_loader_last_error(const mc_world_source_chunk_loader *);
mc_world_storage_status
mc_world_source_chunk_loader_prepare_normalizer(mc_world_source_chunk_loader *, mc_world_runtime *,
                                                mc_world_storage_error *);
#endif
