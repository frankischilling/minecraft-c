#include "../../../src/world/storage/java_file_internal.h"
#include "file_overlap.h"
#include "source_fixture.h"

atomic_bool stats_overlap_armed, stats_overlap_allocated, stats_overlap_cache, stats_overlap_read,
    stats_overlap_failed, stats_overlap_reentry;
thrd_t stats_overlap_foreground;
mc_java_files *stats_overlap_files, *stats_overlap_occurrence;

bool stats_overlap_wait(const atomic_bool *flag) {
    struct timespec begin = {0}, current = {0};
    if (timespec_get(&begin, TIME_UTC) != TIME_UTC)
        return false;
    const struct timespec pause = {0, 1000000};
    while (!atomic_load_explicit(flag, memory_order_acquire)) {
        if (timespec_get(&current, TIME_UTC) != TIME_UTC)
            return false;
        if (current.tv_sec - begin.tv_sec >= 10)
            return false;
        (void)thrd_sleep(&pause, NULL);
    }
    return true;
}

static bool overlap(source_fixture *f) {
    SOURCE_CHECK(source_directory("overlap") && source_directory("overlap/stats"));
    SOURCE_CHECK(source_map_file(f, "overlap/stats/stats_player123.dat", 9));
    mc_java_file *parent = NULL;
    SOURCE_CHECK(source_file(f, "overlap", &parent));
    mc_client_stats_writer *writer = NULL;
    SOURCE_CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
                 MC_CLIENT_STATS_OK);
    stats_overlap_files = f->files;
    stats_overlap_foreground = thrd_current();
    SOURCE_CHECK(mc_java_files_retain(f->files, NULL) == MC_JAVA_FILE_OK);
    stats_overlap_occurrence = f->files;
    atomic_store_explicit(&stats_overlap_armed, true, memory_order_release);
    SOURCE_CHECK(mc_client_stats_writer_construct_file(writer, f->classes, f->session, f->files,
                                                       parent, NULL, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(stats_overlap_wait(&stats_overlap_allocated));
    /* Allocate the foreground File before entering its actual lazy cache.
     * A valid held File and namespace occurrence protect every entrant. */
    mc_java_file *cache_file = NULL;
    mc_world_info_name *empty = mc_world_info_name_empty(), *leaf = NULL;
    static const uint16_t units[] = {'l', 'e', 'a', 'f'};
    SOURCE_CHECK(mc_world_info_name_copy((mc_nbt_string){units, 4}, SIZE_MAX, &leaf) ==
                 MC_WORLD_INFO_OK);
    SOURCE_CHECK(mc_java_file_allocate(f->files, &cache_file, NULL) == MC_JAVA_FILE_OK);
    SOURCE_CHECK(mc_java_file_construct_strings(cache_file, empty, leaf, NULL) == MC_JAVA_FILE_OK);
    atomic_store_explicit(&stats_overlap_armed, false, memory_order_release);
    SOURCE_CHECK(!atomic_load_explicit(&stats_overlap_failed, memory_order_acquire));
    SOURCE_CHECK(atomic_load_explicit(&stats_overlap_cache, memory_order_acquire) &&
                 atomic_load_explicit(&stats_overlap_read, memory_order_acquire) &&
                 atomic_load_explicit(&stats_overlap_reentry, memory_order_acquire));
    SOURCE_CHECK(mc_client_stats_writer_join(writer, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_service(writer, NULL, NULL) == MC_CLIENT_STATS_OK);
    int32_t value = 0;
    SOURCE_CHECK(mc_client_stats_writer_value(writer, f->statistic, &value, NULL) ==
                     MC_CLIENT_STATS_OK &&
                 value == 9);
    SOURCE_CHECK(mc_java_files_release(&stats_overlap_occurrence, NULL) == MC_JAVA_FILE_OK &&
                 stats_overlap_occurrence == NULL);
    mc_world_info_name_release(leaf);
    SOURCE_CHECK(mc_java_file_release(&cache_file, NULL) == MC_JAVA_FILE_OK);
    SOURCE_CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_java_file_release(&parent, NULL) == MC_JAVA_FILE_OK);
    return true;
}

static bool namespace_limits(void) {
    mc_java_files *files = NULL;
    SOURCE_CHECK(mc_java_files_create(NULL, &files, NULL) == MC_JAVA_FILE_OK);
    mc_java_files *alias = files;
    SOURCE_CHECK(mc_java_files_retain(files, NULL) == MC_JAVA_FILE_OK);
    SOURCE_CHECK(mc_java_files_release(&alias, NULL) == MC_JAVA_FILE_OK && alias == NULL);
    /* Controlled scalar-policy injection into a live native owner, not an
     * unheld stale-pointer race or a fabricated namespace. */
    atomic_store_explicit(&files->holds, SIZE_MAX, memory_order_release);
    SOURCE_CHECK(mc_java_files_retain(files, NULL) == MC_JAVA_FILE_LIMIT);
    atomic_store_explicit(&files->holds, 1, memory_order_release);
    atomic_store_explicit(&files->native_calls, 1, memory_order_release);
    SOURCE_CHECK(mc_java_files_release(&files, NULL) == MC_JAVA_FILE_BUSY && files != NULL);
    SOURCE_CHECK(atomic_load_explicit(&files->holds, memory_order_acquire) == 1 &&
                 !atomic_load_explicit(&files->retiring, memory_order_acquire));
    atomic_store_explicit(&files->native_calls, 0, memory_order_release);
    atomic_store_explicit(&files->retiring, true, memory_order_release);
    SOURCE_CHECK(mc_java_files_retain(files, NULL) == MC_JAVA_FILE_BUSY);
    SOURCE_CHECK(mc_java_files_release(&files, NULL) == MC_JAVA_FILE_BUSY && files != NULL);
    atomic_store_explicit(&files->retiring, false, memory_order_release);
    SOURCE_CHECK(mc_java_files_release(&files, NULL) == MC_JAVA_FILE_OK && files == NULL);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || SOURCE_CD(argv[2]) != 0) {
        fputs("Stats overlap input: resources work-directory\n", stderr);
        return 2;
    }
    source_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    if (!source_setup(f, argv[1]) || !overlap(f) || !source_teardown(f) || !namespace_limits())
        return 1;
    free(f);
    printf("Stats File overlap: %zu checks\n", source_checks);
    return 0;
}
