#include "mc/world/storage/source_region_output.h"
#include "platform/time/millis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mc_test_region_raf_fault_once(void);
size_t mc_test_region_raf_close_calls(void);
size_t mc_test_region_raf_close_faults(void);

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "source region output line%d check%u: %s\n", __LINE__, checks, #x);    \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    const char *root_path;
    mc_java_files *files;
    mc_java_file *root;
    mc_world_source_regions *regions;
    mc_world_source_region_output_error output_error;
    mc_world_source_region_error region_error;
} fixture;

static mc_java_file *path_file(mc_java_files *files, const char *text) {
    const size_t count = strlen(text);
    uint16_t *units = malloc((count + 1) * sizeof(*units));
    if (units == NULL)
        return NULL;
    for (size_t i = 0; i < count; ++i)
        units[i] = (unsigned char)text[i];
    mc_world_info_name *name = NULL;
    mc_java_file *file = NULL;
    mc_java_file_error e;
    if (mc_world_info_name_copy((mc_nbt_string){units, count}, 1048576, &name) !=
            MC_WORLD_INFO_OK ||
        mc_java_file_allocate(files, &file, &e) != MC_JAVA_FILE_OK ||
        mc_java_file_construct_string(file, name, &e) != MC_JAVA_FILE_OK) {
        (void)mc_java_file_release(&file, NULL);
    }
    mc_world_info_name_release(name);
    free(units);
    return file;
}

static int start(fixture *f, const char *root) {
    f->root_path = root;
    CHECK(mc_java_files_create(NULL, &f->files, NULL) == MC_JAVA_FILE_OK);
    f->root = path_file(f->files, root);
    CHECK(f->root != NULL);
    CHECK(mc_world_source_regions_create(f->files, NULL, &f->regions, &f->region_error) ==
          MC_WORLD_SOURCE_OK);
    return 0;
}

static int finish(fixture *f) {
    CHECK(mc_world_source_regions_retire(&f->regions, &f->region_error) == MC_WORLD_SOURCE_OK &&
          f->regions == NULL);
    CHECK(mc_java_file_release(&f->root, NULL) == MC_JAVA_FILE_OK);
    CHECK(mc_java_files_release(&f->files, NULL) == MC_JAVA_FILE_OK && f->files == NULL);
    return 0;
}

static uint8_t *payload(size_t count) {
    uint8_t *bytes = malloc(count == 0 ? 1 : count);
    uint32_t state = UINT32_C(0x12345678);
    if (bytes == NULL)
        return NULL;
    for (size_t i = 0; i < count; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        bytes[i] = (uint8_t)state;
    }
    return bytes;
}

static int write_payload(fixture *f, mc_world_source_region_output *out, size_t count,
                         uint8_t **borrowed_until_close) {
    static const uint16_t name[] = {'P', 'a', 'y', 'l', 'o', 'a', 'd'};
    *borrowed_until_close = payload(count);
    CHECK(*borrowed_until_close != NULL);
    const mc_nbt_tag array = {.type = MC_NBT_BYTE_ARRAY,
                              .name = {name, 7},
                              .value.byte_array = {*borrowed_until_close, count}};
    const mc_nbt_tag *items[] = {&array};
    const mc_nbt_tag root = {.type = MC_NBT_COMPOUND, .value.compound = {items, 1}};
    CHECK(mc_world_source_region_output_write_compound(out, &root, NULL, &f->output_error) ==
          MC_WORLD_SOURCE_OK);
    return 0;
}

static int opened(fixture *f, int32_t x, int32_t z, mc_world_source_region_output **out) {
    CHECK(mc_world_source_regions_output(f->regions, f->root, x, z, NULL, out, &f->output_error) ==
          MC_WORLD_SOURCE_OK);
    mc_world_source_region_output_state s;
    mc_java_random_access_state raf;
    CHECK(mc_world_source_region_output_inspect(*out, &s) && s.phase == MC_JAVA_OUTPUT_READY &&
          !s.active && !s.closed && !s.finished && !s.deflater_ended && !s.commit_entered &&
          s.buffer_capacity == 8096 && s.compressed_length == 0 && s.written == 0 &&
          s.source_close_calls == 0 && s.region_holds >= 2 && s.regions == f->regions &&
          s.region_identity != NULL && s.region_file != NULL && s.x == (x & 31) && s.z == (z & 31));
    CHECK(mc_java_random_access_inspect(s.random_access, &raf) && raf.native_open &&
          !raf.close_reached);
    return 0;
}

static int copy_region(fixture *f, const char *label, const mc_java_file *file, size_t *extent) {
    const mc_nbt_string path = mc_world_info_name_text(mc_java_file_path(file));
    char source[4096], destination[4096];
    CHECK(path.length < sizeof(source));
    for (size_t i = 0; i < path.length; ++i) {
        CHECK(path.units[i] > 0 && path.units[i] <= 127);
        source[i] = (char)path.units[i];
    }
    source[path.length] = 0;
    const int n =
        snprintf(destination, sizeof(destination), "%s/snapshots/%s.mcr", f->root_path, label);
    CHECK(n > 0 && (size_t)n < sizeof(destination));
    FILE *in = fopen(source, "rb"), *out = fopen(destination, "wb");
    CHECK(in != NULL && out != NULL);
    uint8_t bytes[4096];
    size_t total = 0, count;
    while ((count = fread(bytes, 1, sizeof(bytes), in)) != 0) {
        CHECK(total <= 1048576 && count <= 1048576 - total);
        CHECK(fwrite(bytes, 1, count, out) == count);
        total += count;
    }
    CHECK(!ferror(in) && fclose(in) == 0 && fclose(out) == 0);
    *extent = total;
    return 0;
}

/* Each row snapshots actual bytes independently from the public passive state.
 * No offset, clock, free-list, owner or codec field is changed by this helper. */
static int emit(fixture *f, const char *label, mc_world_source_region_output *out, int64_t first,
                int64_t last, int32_t growth1, int32_t growth2) {
    mc_world_source_region_output_state s;
    mc_world_source_regions_state r;
    mc_java_random_access_state raf;
    size_t extent = 0;
    CHECK(mc_world_source_region_output_inspect(out, &s) && s.region_file != NULL);
    CHECK(mc_world_source_regions_inspect((mc_world_source_regions *)s.regions, &r));
    CHECK(mc_java_random_access_inspect(s.random_access, &raf));
    CHECK(copy_region(f, label, s.region_file, &extent) == 0);
    printf("{\"event\":\"state\",\"step\":\"%s\",\"output_id\":%llu,\"qid\":%llu,\"raf_id\":%llu,"
           "\"phase\":%d,\"closed\":%d,\"finished\":%d,\"deflater_ended\":%d,\"commit_entered\":%d,"
           "\"commit_returned\":%d,\"oversized\":%d,\"x\":%d,\"z\":%d,\"written\":%d,\"raw_bytes\":"
           "%llu,"
           "\"compressed_length\":%zu,\"capacity\":%zu,\"owner_bytes\":%zu,\"peak_bytes\":%zu,"
           "\"sector\":%d,\"required\":%d,\"timestamp\":%d,\"sectors\":%zu,\"appended\":%zu,\"free_"
           "updates\":%zu,"
           "\"growth\":%d,\"growth1\":%d,\"growth2\":%d,\"first\":%lld,\"last\":%lld,"
           "\"qholds\":%zu,\"rholds\":%zu,\"entries\":%zu,\"source_closes\":%zu,\"raf_open\":%d,"
           "\"raf_source_close\":%d,\"raf_reads\":%zu,\"raf_writes\":%zu,\"raf_seeks\":%zu,\"raf_"
           "bytes\":%llu,"
           "\"native_closes\":%zu,\"extent\":%zu,\"failure_status\":%d,\"diagnostic\":%d,\"cause_"
           "pc\":%d,\"prior_pc\":%d,"
           "\"offsets\":[",
           label, (unsigned long long)(uintptr_t)out,
           (unsigned long long)(uintptr_t)s.region_identity,
           (unsigned long long)(uintptr_t)s.random_access, (int)s.phase, s.closed, s.finished,
           s.deflater_ended, s.commit_entered, s.commit_returned, s.oversized, s.x, s.z, s.written,
           (unsigned long long)s.raw_bytes, s.compressed_length, s.buffer_capacity, s.owner_bytes,
           s.peak_owner_bytes, s.sector, s.required, s.timestamp, s.sectors, s.sectors_appended,
           s.free_updates, s.growth, growth1, growth2, (long long)first, (long long)last,
           s.region_holds, r.holds, r.entries, s.source_close_calls, raf.native_open,
           raf.close_reached, raf.read_calls, raf.write_calls, raf.seek_calls,
           (unsigned long long)raf.bytes_written, mc_test_region_raf_close_calls(), extent,
           (int)s.failure->region.source.status, s.failure->region.source.diagnostic_frontier,
           s.failure->region.source.cause_site.pc, s.failure->region.source.prior_site.pc);
    for (size_t i = 0; i < 1024; ++i)
        printf("%s%d", i == 0 ? "" : ",", s.offsets[i]);
    printf("],\"timestamps\":[");
    for (size_t i = 0; i < 1024; ++i)
        printf("%s%d", i == 0 ? "" : ",", s.timestamps[i]);
    printf("],\"free\":[");
    for (size_t i = 0; i < s.sectors; ++i)
        printf("%s%d", i == 0 ? "" : ",", s.free_sectors[i]);
    printf("]}\n");
    return 0;
}

static int normal(fixture *f, const char *label, int32_t x, int32_t z, size_t length, int sector,
                  int required, int appended, int growth) {
    mc_world_source_region_output *out = NULL;
    CHECK(opened(f, x, z, &out) == 0);
    char step[96];
    snprintf(step, sizeof(step), "%s-open", label);
    CHECK(emit(f, step, out, 0, 0, -1, -1) == 0);
    mc_world_source_region_output_state before, after;
    mc_java_random_access_state raf_before, raf_after;
    CHECK(mc_world_source_region_output_inspect(out, &before));
    CHECK(mc_java_random_access_inspect(before.random_access, &raf_before));
    uint8_t *bytes = NULL;
    CHECK(write_payload(f, out, length, &bytes) == 0);
    CHECK(mc_world_source_region_output_inspect(out, &after) &&
          after.written == (int32_t)(length + 18) && after.raw_bytes == length + 18 &&
          !after.closed && !after.commit_entered);
    CHECK(mc_java_random_access_inspect(after.random_access, &raf_after) &&
          raf_after.write_calls == raf_before.write_calls &&
          raf_after.seek_calls == raf_before.seek_calls &&
          raf_after.bytes_written == raf_before.bytes_written);
    snprintf(step, sizeof(step), "%s-written", label);
    CHECK(emit(f, step, out, 0, 0, -1, -1) == 0);
    int64_t first, last;
    CHECK(mc_platform_current_time_millis(&first) == MC_PLATFORM_MILLIS_OK);
    CHECK(mc_world_source_region_output_source_close(out, &f->output_error) == MC_WORLD_SOURCE_OK);
    CHECK(mc_platform_current_time_millis(&last) == MC_PLATFORM_MILLIS_OK);
    CHECK(mc_world_source_region_output_inspect(out, &after) && after.closed && after.finished &&
          after.deflater_ended && after.commit_entered && after.commit_returned &&
          !after.oversized && after.source_close_calls == 1 && after.sector == sector &&
          after.required == required && after.sectors_appended == (size_t)appended &&
          after.growth == growth && after.region_identity == before.region_identity &&
          after.random_access == before.random_access);
    snprintf(step, sizeof(step), "%s-closed", label);
    CHECK(emit(f, step, out, first, last, -1, -1) == 0);
    const mc_world_source_region_output_state completed = after;
    CHECK(mc_java_random_access_inspect(after.random_access, &raf_before));
    CHECK(mc_world_source_region_output_source_close(out, &f->output_error) == MC_WORLD_SOURCE_OK);
    CHECK(mc_world_source_region_output_inspect(out, &after) && after.source_close_calls == 2 &&
          after.compressed_length == completed.compressed_length &&
          after.growth == completed.growth);
    CHECK(mc_java_random_access_inspect(after.random_access, &raf_after) &&
          raf_after.write_calls == raf_before.write_calls &&
          raf_after.seek_calls == raf_before.seek_calls &&
          raf_after.bytes_written == raf_before.bytes_written && raf_after.native_open &&
          !raf_after.close_reached);
    int32_t growth1 = -1, growth2 = -1;
    CHECK(mc_world_source_regions_consume_growth(f->regions, f->root, x, z, &growth1,
                                                 &f->region_error) == MC_WORLD_SOURCE_OK &&
          growth1 == growth);
    CHECK(mc_world_source_regions_consume_growth(f->regions, f->root, x, z, &growth2,
                                                 &f->region_error) == MC_WORLD_SOURCE_OK &&
          growth2 == 0);
    snprintf(step, sizeof(step), "%s-growth", label);
    CHECK(emit(f, step, out, 0, 0, growth1, growth2) == 0);
    mc_world_source_region_record *record = NULL;
    CHECK(mc_world_source_regions_read(f->regions, f->root, x, z, &record, &f->region_error) ==
              MC_WORLD_SOURCE_OK &&
          record != NULL && record->compression == 2 && record->length == after.compressed_length &&
          memcmp(record->bytes, after.compressed_bytes, record->length) == 0);
    mc_world_source_region_record_free(record);
    CHECK(mc_world_source_region_output_release(&out, &f->output_error) == MC_WORLD_SOURCE_OK &&
          out == NULL);
    free(bytes);
    return 0;
}

static int flow(fixture *f) {
    CHECK(normal(f, "fresh", 0, 0, 32, 2, 1, 1, 12288) == 0);
    CHECK(normal(f, "rewrite", 0, 0, 32, 2, 1, 0, 0) == 0);
    CHECK(normal(f, "grow", 0, 0, 20000, 3, 5, 5, 20480) == 0);
    CHECK(normal(f, "shrink", 0, 0, 32, 2, 1, 0, 0) == 0);
    CHECK(normal(f, "reuse", 1, 0, 12000, 3, 3, 0, 0) == 0);
    CHECK(normal(f, "negative", -1, -33, 32, 2, 1, 1, 12288) == 0);
    mc_world_source_region_output *abandoned = NULL;
    CHECK(opened(f, 2, 0, &abandoned) == 0);
    uint8_t *abandoned_bytes = NULL;
    CHECK(write_payload(f, abandoned, 12000, &abandoned_bytes) == 0);
    CHECK(emit(f, "abandon-written", abandoned, 0, 0, -1, -1) == 0);
    const size_t closes = mc_test_region_raf_close_calls();
    CHECK(mc_world_source_region_output_release(&abandoned, &f->output_error) ==
              MC_WORLD_SOURCE_OK &&
          abandoned == NULL && mc_test_region_raf_close_calls() == closes);
    free(abandoned_bytes);
    mc_world_source_region_record *missing = NULL;
    CHECK(mc_world_source_regions_read(f->regions, f->root, 2, 0, &missing, &f->region_error) ==
              MC_WORLD_SOURCE_OK &&
          missing == NULL);
    mc_world_source_region_output *old = NULL, *replacement = NULL;
    CHECK(opened(f, 0, 0, &old) == 0);
    uint8_t *old_bytes = NULL, *new_bytes = NULL;
    CHECK(write_payload(f, old, 32, &old_bytes) == 0);
    CHECK(emit(f, "flush-before", old, 0, 0, -1, -1) == 0);
    mc_world_source_region_output_state a, b;
    CHECK(mc_world_source_region_output_inspect(old, &a));
    CHECK(mc_world_source_regions_flush(f->regions, &f->region_error) == MC_WORLD_SOURCE_OK);
    CHECK(emit(f, "flush-old", old, 0, 0, -1, -1) == 0);
    CHECK(opened(f, 4, 0, &replacement) == 0);
    CHECK(write_payload(f, replacement, 32, &new_bytes) == 0);
    CHECK(mc_world_source_region_output_inspect(replacement, &b) &&
          a.region_identity != b.region_identity && a.random_access != b.random_access &&
          b.growth == 0);
    CHECK(emit(f, "flush-replacement", replacement, 0, 0, -1, -1) == 0);
    CHECK(mc_world_source_region_output_source_close(old, &f->output_error) ==
              MC_WORLD_SOURCE_UNIMPLEMENTED &&
          f->output_error.region.source.diagnostic_frontier && f->output_error.region.caught_io &&
          f->output_error.region.source.source_exception &&
          !f->output_error.region.source.native_policy &&
          f->output_error.region.source.frames[0].pc == 441 &&
          f->output_error.region.source.cause_site.pc == 33 &&
          f->output_error.region.source.prior_site.pc == 74);
    CHECK(mc_world_source_region_output_inspect(old, &a) && a.closed && a.finished &&
          a.deflater_ended && a.commit_entered && !a.commit_returned && a.region_holds == 1);
    CHECK(emit(f, "flush-old-failed", old, 0, 0, -1, -1) == 0);
    CHECK(emit(f, "flush-new-uncommitted", replacement, 0, 0, -1, -1) == 0);
    /* This explicit native frontier is the end of the common source prefix.
     * Native retirement does not finish or commit the replacement. */
    CHECK(mc_world_source_region_output_release(&old, &f->output_error) == MC_WORLD_SOURCE_OK);
    CHECK(mc_world_source_region_output_release(&replacement, &f->output_error) ==
          MC_WORLD_SOURCE_OK);
    free(old_bytes);
    free(new_bytes);
    CHECK(finish(f) == 0);
    return 0;
}

static int owners(fixture *f, bool fail_cleanup) {
    mc_world_source_region_output *first = NULL, *second = NULL;
    CHECK(opened(f, 0, 0, &first) == 0);
    CHECK(opened(f, 1, 0, &second) == 0);
    mc_world_source_region_output_state a, b;
    CHECK(mc_world_source_region_output_inspect(first, &a) &&
          mc_world_source_region_output_inspect(second, &b) &&
          a.region_identity == b.region_identity && a.random_access == b.random_access &&
          a.region_holds == 3);
    CHECK(mc_world_source_regions_retire(&f->regions, &f->region_error) == MC_WORLD_SOURCE_BUSY &&
          f->regions != NULL);
    CHECK(mc_world_source_region_output_release(&first, &f->output_error) == MC_WORLD_SOURCE_OK &&
          first == NULL);
    CHECK(mc_world_source_region_output_inspect(second, &b) && b.region_holds == 2 && !b.closed);
    uint8_t *bytes = NULL;
    CHECK(write_payload(f, second, 12000, &bytes) == 0);
    CHECK(emit(f, "owners-uncommitted", second, 0, 0, -1, -1) == 0);
    mc_world_source_region_output_state before_cleanup;
    mc_java_random_access_state before_cleanup_raf;
    CHECK(mc_world_source_region_output_inspect(second, &before_cleanup) &&
          mc_java_random_access_inspect(before_cleanup.random_access, &before_cleanup_raf));
    for (size_t i = 0; i < 1024; ++i)
        CHECK(before_cleanup.offsets[i] == 0 && before_cleanup.timestamps[i] == 0);
    mc_java_file *external_file = (mc_java_file *)b.region_file;
    mc_java_files *external_namespace = f->files;
    CHECK(mc_java_file_retain(external_file, NULL) == MC_JAVA_FILE_OK &&
          mc_java_files_retain(external_namespace, NULL) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_release(&f->root, NULL) == MC_JAVA_FILE_OK &&
          mc_java_files_release(&f->files, NULL) == MC_JAVA_FILE_OK);
    mc_world_source_regions *remaining_regions = f->regions;
    const mc_java_random_access *remaining_raf = b.random_access;
    CHECK(mc_world_source_regions_release(&f->regions, &f->region_error) == MC_WORLD_SOURCE_OK &&
          f->regions == NULL);
    const size_t closes = mc_test_region_raf_close_calls();
    if (fail_cleanup) {
        mc_test_region_raf_fault_once();
        CHECK(mc_world_source_region_output_release(&second, &f->output_error) ==
                  MC_WORLD_SOURCE_FILE &&
              second != NULL);
        CHECK(mc_test_region_raf_close_faults() == 1 &&
              mc_test_region_raf_close_calls() == closes + 1);
        CHECK(f->output_error.region.source.native_policy &&
              !f->output_error.region.source.source_exception &&
              !f->output_error.region.source.caught_io &&
              !f->output_error.region.source.diagnostic_frontier &&
              f->output_error.region.source.exception_class == NULL &&
              f->output_error.region.source.cause_status == MC_WORLD_SOURCE_FILE &&
              f->output_error.region.source.file.status == MC_JAVA_FILE_IO_ERROR &&
              f->output_error.region.random_access.status == MC_JAVA_FILE_IO_ERROR &&
              f->output_error.region.random_access.system_error != 0 &&
              f->output_error.region.source.file.system_error ==
                  f->output_error.region.random_access.system_error &&
              f->output_error.region.random_access.operation != NULL &&
              strcmp(f->output_error.region.random_access.operation, "RandomAccessFile close") ==
                  0);
        mc_world_source_regions_state r;
        mc_java_random_access_state raf;
        CHECK(mc_world_source_region_output_inspect(second, &b) &&
              b.phase == MC_JAVA_OUTPUT_RETIRING && !b.active && b.region_identity == NULL &&
              b.random_access == NULL && !b.closed && !b.finished && !b.commit_entered &&
              b.deflater_ended && b.source_close_calls == 0 && b.regions == remaining_regions &&
              b.raw_bytes == before_cleanup.raw_bytes &&
              b.compressed_length == before_cleanup.compressed_length);
        CHECK(mc_world_source_regions_inspect(remaining_regions, &r) && r.retiring && !r.active &&
              r.entries == 1 && r.cleanup->source.native_policy &&
              !r.cleanup->source.source_exception && !r.cleanup->source.caught_io &&
              !r.cleanup->source.diagnostic_frontier && r.cleanup->source.exception_class == NULL &&
              r.cleanup->source.file.status == MC_JAVA_FILE_IO_ERROR &&
              r.cleanup->random_access.status == MC_JAVA_FILE_IO_ERROR &&
              b.failure->region.source.native_policy && !b.failure->region.source.source_exception);
        CHECK(mc_java_random_access_inspect(remaining_raf, &raf) &&
              raf.phase == MC_JAVA_FILE_READY && !raf.active && !raf.native_open &&
              !raf.close_reached && !raf.close_succeeded &&
              raf.write_calls == before_cleanup_raf.write_calls &&
              raf.seek_calls == before_cleanup_raf.seek_calls &&
              raf.bytes_written == before_cleanup_raf.bytes_written);
        size_t ignored;
        CHECK(copy_region(f, "cleanup-quarantine", external_file, &ignored) == 0);
        printf("{\"event\":\"cleanup\",\"retiring\":%d,\"inactive\":%d,\"native_policy\":%d,"
               "\"source_exception\":%d,\"physical_closes\":%zu,\"consumed_faults\":%zu,"
               "\"source_closes\":%zu,\"committed\":%d,\"status\":%d,\"child_status\":%d,"
               "\"child_system_error\":%d,\"raf_open\":%d,\"raf_source_close\":%d}\n",
               r.retiring, !r.active && !b.active && !raf.active,
               f->output_error.region.source.native_policy,
               f->output_error.region.source.source_exception, mc_test_region_raf_close_calls(),
               mc_test_region_raf_close_faults(), b.source_close_calls, b.commit_entered,
               (int)f->output_error.region.source.status,
               (int)f->output_error.region.random_access.status,
               f->output_error.region.random_access.system_error, raf.native_open,
               raf.close_reached);
    }
    CHECK(mc_world_source_region_output_release(&second, &f->output_error) == MC_WORLD_SOURCE_OK &&
          second == NULL && mc_test_region_raf_close_calls() == closes + 1);
    free(bytes);
    mc_java_file_state state;
    CHECK(mc_java_file_read_state(external_file, &state, NULL) == MC_JAVA_FILE_OK &&
          state.phase == MC_JAVA_FILE_READY && state.holds == 1 &&
          mc_java_file_namespace(external_file) == external_namespace);
    size_t ignored;
    CHECK(copy_region(f, "owners-retired", external_file, &ignored) == 0);
    CHECK(mc_java_file_release(&external_file, NULL) == MC_JAVA_FILE_OK && external_file == NULL);
    mc_java_file *probe = path_file(external_namespace, f->root_path);
    CHECK(probe != NULL && mc_java_file_release(&probe, NULL) == MC_JAVA_FILE_OK);
    CHECK(mc_java_files_release(&external_namespace, NULL) == MC_JAVA_FILE_OK &&
          external_namespace == NULL);
    return 0;
}

static int constructor_budget(fixture *f) {
    const int32_t x = 7, z = 11;
    mc_java_output_options options = mc_world_source_region_output_default_options();
    options.max_owner_bytes = 65536;
    mc_world_source_region_output *probe = NULL, *limited = NULL;
    CHECK(mc_world_source_regions_output(f->regions, f->root, x, z, &options, &probe,
                                         &f->output_error) == MC_WORLD_SOURCE_LIMIT &&
          probe != NULL);
    mc_world_source_region_output_state a, b;
    mc_java_random_access_state before, after;
    CHECK(mc_world_source_region_output_inspect(probe, &a) && a.phase == MC_JAVA_OUTPUT_FAILED &&
          !a.active && a.buffer_capacity == 8096 && a.owner_bytes >= a.buffer_capacity &&
          a.owner_bytes < options.max_owner_bytes &&
          a.peak_owner_bytes <= options.max_owner_bytes && a.x == x && a.z == z &&
          a.region_holds == 2 && a.region_identity != NULL && a.regions == f->regions &&
          a.growth == 8192 && !a.closed && !a.finished && !a.deflater_ended && !a.commit_entered &&
          !a.commit_returned && a.source_close_calls == 0 && a.raw_bytes == 0 &&
          a.compressed_length == 0 && a.written == 0);
    CHECK(f->output_error.region.source.native_policy &&
          !f->output_error.region.source.source_exception &&
          !f->output_error.region.source.diagnostic_frontier &&
          f->output_error.stream.output.status == MC_JAVA_OUTPUT_LIMIT &&
          f->output_error.stream.output.cause.operation != NULL &&
          strcmp(f->output_error.stream.output.cause.operation, "native region deflater") == 0);
    CHECK(mc_java_random_access_inspect(a.random_access, &before) && before.native_open &&
          !before.close_reached);
    for (size_t i = 0; i < 1024; ++i)
        CHECK(a.offsets[i] == 0 && a.timestamps[i] == 0);
    /* Failed deflateInit has freed its partial zlib allocations. The reached
     * owner now consists of the native base plus the actual Hz byte[8096]. */
    const size_t base = a.owner_bytes - a.buffer_capacity;
    CHECK(base > 0 && base <= SIZE_MAX - 8096);
    CHECK(emit(f, "constructor-deflater-limit", probe, 0, 0, -1, -1) == 0);
    const void *qid = a.region_identity;
    const mc_java_random_access *raf = a.random_access;
    mc_java_file *external_file = (mc_java_file *)a.region_file;
    CHECK(mc_java_file_retain(external_file, NULL) == MC_JAVA_FILE_OK);
    const size_t closes = mc_test_region_raf_close_calls();
    CHECK(mc_world_source_region_output_release(&probe, &f->output_error) == MC_WORLD_SOURCE_OK &&
          probe == NULL && mc_test_region_raf_close_calls() == closes);
    options.max_owner_bytes = base + 8096 - 1;
    CHECK(mc_world_source_regions_output(f->regions, f->root, x, z, &options, &limited,
                                         &f->output_error) == MC_WORLD_SOURCE_LIMIT &&
          limited != NULL);
    CHECK(mc_world_source_region_output_inspect(limited, &b) && b.phase == MC_JAVA_OUTPUT_FAILED &&
          !b.active && b.buffer_capacity == 0 && b.owner_bytes == base &&
          b.peak_owner_bytes == base && b.region_identity == qid && b.random_access == raf &&
          b.regions == f->regions && b.region_holds == 2 && b.growth == 8192 && !b.closed &&
          !b.finished && !b.deflater_ended && !b.commit_entered && !b.commit_returned &&
          b.source_close_calls == 0 && b.raw_bytes == 0 && b.compressed_length == 0 &&
          b.written == 0);
    CHECK(f->output_error.region.source.native_policy &&
          !f->output_error.region.source.source_exception &&
          !f->output_error.region.source.caught_io &&
          !f->output_error.region.source.diagnostic_frontier &&
          f->output_error.stream.output.status == MC_JAVA_OUTPUT_LIMIT &&
          f->output_error.stream.output.cause.operation != NULL &&
          strcmp(f->output_error.stream.output.cause.operation, "native Hz initial byte[8096]") ==
              0);
    CHECK(mc_java_random_access_inspect(b.random_access, &after) && after.native_open &&
          !after.close_reached && after.write_calls == before.write_calls &&
          after.seek_calls == before.seek_calls && after.read_calls == before.read_calls &&
          after.bytes_written == before.bytes_written && after.bytes_read == before.bytes_read);
    for (size_t i = 0; i < 1024; ++i)
        CHECK(b.offsets[i] == 0 && b.timestamps[i] == 0);
    CHECK(emit(f, "constructor-buffer-limit", limited, 0, 0, -1, -1) == 0);
    /* Retain the actual failed prefix above before this dedicated regression
     * assertion: Hz stores its coordinates only after super(8096) returns. */
    CHECK(fflush(stdout) == 0);
    CHECK(b.x == 0 && b.z == 0);
    CHECK(mc_world_source_region_output_release(&limited, &f->output_error) == MC_WORLD_SOURCE_OK &&
          limited == NULL && mc_test_region_raf_close_calls() == closes);
    CHECK(mc_java_random_access_inspect(raf, &after) && after.native_open && !after.close_reached &&
          after.write_calls == before.write_calls && after.seek_calls == before.seek_calls &&
          after.read_calls == before.read_calls && after.bytes_written == before.bytes_written &&
          after.bytes_read == before.bytes_read);
    size_t extent;
    CHECK(copy_region(f, "constructor-retired", external_file, &extent) == 0 && extent == 8192);
    CHECK(mc_java_file_release(&external_file, NULL) == MC_JAVA_FILE_OK && external_file == NULL);
    printf("{\"event\":\"constructor_budget\",\"base_owner_bytes\":%zu,\"deflater_budget\":65536,"
           "\"buffer_budget\":%zu,\"requested_x\":%d,\"requested_z\":%d,\"extra_raf_writes\":0}\n",
           base, options.max_owner_bytes, x, z);
    CHECK(finish(f) == 0 && mc_test_region_raf_close_calls() == closes + 1);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 3);
    fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL && start(f, argv[1]) == 0);
    const bool flow_mode = strcmp(argv[2], "flow") == 0;
    const bool cleanup_mode = strcmp(argv[2], "cleanup-failure") == 0;
    const bool constructor_mode = strcmp(argv[2], "constructor-budget") == 0;
    CHECK(flow_mode || cleanup_mode || constructor_mode || strcmp(argv[2], "owners") == 0);
    CHECK((constructor_mode ? constructor_budget(f)
           : flow_mode      ? flow(f)
                            : owners(f, cleanup_mode)) == 0);
    printf("{\"event\":\"complete\",\"mode\":\"%s\",\"checks\":%u,\"native_closes\":%zu,\"consumed_"
           "faults\":%zu}\n",
           argv[2], checks, mc_test_region_raf_close_calls(), mc_test_region_raf_close_faults());
    free(f);
    return 0;
}
