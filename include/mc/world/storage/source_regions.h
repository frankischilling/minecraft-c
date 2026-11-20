#ifndef MC_WORLD_STORAGE_SOURCE_REGIONS_H
#define MC_WORLD_STORAGE_SOURCE_REGIONS_H
#include "mc/world/storage/java_random_access.h"
#include "mc/world/storage/source_handler.h"

typedef struct mc_world_source_regions_options {
    size_t max_region_bytes, max_compressed_bytes, max_path_bytes;
} mc_world_source_regions_options;

typedef struct mc_world_source_region_error {
    mc_world_source_error source;
    mc_java_file_error random_access;
    int region_pc;
    bool caught_io, source_null;
} mc_world_source_region_error;

typedef struct mc_world_source_region_record {
    uint8_t *bytes;
    size_t length;
    uint8_t compression;
} mc_world_source_region_record;

typedef struct mc_world_source_regions_state {
    size_t holds, entries;
    bool active, activated, retiring;
    const mc_java_files *files;
    const mc_world_source_region_error *failure, *cleanup;
    const mc_java_file *pending_directory, *pending_file;
    const mc_java_random_access *pending_random_access;
} mc_world_source_regions_state;

mc_world_source_regions_options mc_world_source_regions_default_options(void);
mc_world_source_status mc_world_source_regions_create(mc_java_files *,
                                                      const mc_world_source_regions_options *,
                                                      mc_world_source_regions **,
                                                      mc_world_source_region_error *);
mc_world_source_status mc_world_source_regions_retain(mc_world_source_regions *,
                                                      mc_world_source_region_error *);
mc_world_source_status mc_world_source_regions_release(mc_world_source_regions **,
                                                       mc_world_source_region_error *);
mc_world_source_status mc_world_source_regions_retire(mc_world_source_regions **,
                                                      mc_world_source_region_error *);
/* The actual shared rj.a()/hi.c(), including qj.b descriptor closes before
 * Map.clear. A failed close's printStackTrace is a retained dependency. */
mc_world_source_status mc_world_source_regions_flush(mc_world_source_regions *,
                                                     mc_world_source_region_error *);
mc_world_source_status mc_world_source_regions_read(mc_world_source_regions *, mc_java_file *,
                                                    int32_t, int32_t,
                                                    mc_world_source_region_record **,
                                                    mc_world_source_region_error *);
void mc_world_source_region_record_free(mc_world_source_region_record *);
bool mc_world_source_regions_inspect(const mc_world_source_regions *,
                                     mc_world_source_regions_state *);
#endif
