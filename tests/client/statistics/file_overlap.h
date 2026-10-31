#ifndef MC_STATS_FILE_OVERLAP_H
#define MC_STATS_FILE_OVERLAP_H
#include "mc/world/storage/java_file.h"
#include <stdatomic.h>
#include <threads.h>

extern atomic_bool stats_overlap_armed, stats_overlap_allocated, stats_overlap_cache,
    stats_overlap_read, stats_overlap_failed, stats_overlap_reentry;
extern thrd_t stats_overlap_foreground;
extern mc_java_files *stats_overlap_files, *stats_overlap_occurrence;
bool stats_overlap_wait(const atomic_bool *flag);
#endif
