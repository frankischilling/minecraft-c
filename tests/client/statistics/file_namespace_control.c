/* Test-only forwarder around the SAME namespace body, not a runtime allocator
 * callback. Link audit must prove java_file.c has exactly one implementation. */
#include "file_overlap.h"
#include "mc/world/info/info.h"

static mc_world_info_status overlap_name_copy(mc_nbt_string text, size_t maximum,
                                              mc_world_info_name **out) {
    const mc_world_info_status status = mc_world_info_name_copy(text, maximum, out);
    if (status == MC_WORLD_INFO_OK &&
        atomic_load_explicit(&stats_overlap_armed, memory_order_acquire) &&
        thrd_equal(thrd_current(), stats_overlap_foreground) && text.length == 1 &&
        (text.units[0] == '/' || text.units[0] == '\\')) {
        mc_java_file_error error = {0};
        const mc_java_file_status retained = mc_java_files_retain(stats_overlap_files, &error);
        const mc_java_file_status released =
            mc_java_files_release(&stats_overlap_occurrence, &error);
        atomic_store_explicit(&stats_overlap_reentry,
                              retained == MC_JAVA_FILE_BUSY && released == MC_JAVA_FILE_BUSY &&
                                  stats_overlap_occurrence == stats_overlap_files,
                              memory_order_release);
        atomic_store_explicit(&stats_overlap_cache, true, memory_order_release);
        if (!stats_overlap_wait(&stats_overlap_read))
            atomic_store_explicit(&stats_overlap_failed, true, memory_order_release);
    }
    return status;
}

#define mc_world_info_name_copy overlap_name_copy
#include "../../../src/world/storage/java_file.c"
#undef mc_world_info_name_copy
