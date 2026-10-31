/* Authored real-file controls, unrun. The separate native TU includes the
 * exact byte-output implementation once and forwards real stdio first. */
#include "../../../src/world/storage/java_file_native.h"
#include "../../../src/world/storage/java_output_internal.h"
#include "mc/world/storage/java_output.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static mc_java_file_output *observed_fos;
static mc_java_data_output *observed_dos;
static bool armed, partial, close_fault, callback_ok = true;
static size_t actual_closes, actual_writes;

static void reentry(void) {
    mc_java_output_error e;
    mc_java_file_output *f = observed_fos;
    mc_java_data_output *d = observed_dos;
    if (f != NULL &&
        (mc_java_file_output_retain(f, &e) != MC_JAVA_OUTPUT_BUSY ||
         mc_java_file_output_release(&f, &e) != MC_JAVA_OUTPUT_BUSY || f != observed_fos))
        callback_ok = false;
    if (d != NULL &&
        (mc_java_data_output_write_long(d, 7, &e) != MC_JAVA_OUTPUT_BUSY ||
         mc_java_data_output_release(&d, &e) != MC_JAVA_OUTPUT_BUSY || d != observed_dos))
        callback_ok = false;
}

size_t mc_test_output_write(const void *p, size_t size, size_t count, FILE *file) {
    ++actual_writes;
    const size_t n = fwrite(p, size, armed && partial && count > 3 ? 3 : count, file);
    if (armed)
        reentry();
    if (armed && partial)
        errno = EIO;
    return n;
}

int mc_test_output_ferror(FILE *file) {
    const int result = ferror(file);
    return armed && partial ? 1 : result;
}

int mc_test_output_close(FILE *file) {
    ++actual_closes;
    const int result = fclose(file);
    if (armed)
        reentry();
    if (armed && close_fault) {
        errno = EIO;
        return EOF;
    }
    return result;
}

static bool name_ascii(const char *s, mc_world_info_name **out) {
    uint16_t units[2048];
    const size_t n = strlen(s);
    if (n > sizeof(units) / sizeof(units[0]))
        return false;
    for (size_t i = 0; i < n; ++i) {
        if ((uint8_t)s[i] > 127)
            return false;
        units[i] = (uint8_t)s[i];
    }
    return mc_world_info_name_copy((mc_nbt_string){units, n}, 8192, out) == MC_WORLD_INFO_OK;
}

static bool read_bytes(mc_java_file *file, uint8_t *out, size_t n) {
    mc_file_native_char *path = NULL;
    mc_java_file_error e;
    if (mc_java_file_native_path_copy(file, &path, &e) != MC_JAVA_FILE_OK)
        return false;
#ifdef _WIN32
    FILE *in = _wfopen(path, L"rb");
#else
    FILE *in = fopen(path, "rb");
#endif
    free(path);
    if (in == NULL)
        return false;
    const bool ok = fread(out, 1, n, in) == n && fgetc(in) == EOF && !ferror(in);
    return fclose(in) == 0 && ok;
}

#define NEED(x)                                                                                    \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "binary output line%d: %s\n", __LINE__, #x);                           \
            goto done;                                                                             \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    if (argc != 2) {
        fputs("fresh exclusively owned ASCII qualification directory required\n", stderr);
        return 2;
    }
    int result = 1;
    mc_java_files *files = NULL;
    mc_java_file *parent = NULL, *file = NULL, *invalid = NULL;
    mc_world_info_name *parent_name = NULL, *leaf = NULL, *bad = NULL;
    mc_java_file_output *f = NULL, *alias = NULL, *raw = NULL;
    mc_java_data_output *d = NULL, *da = NULL, *null_out = NULL, *limited = NULL;
    mc_java_file_error fe;
    mc_java_output_error e;
    mc_java_file_output_state fs;
    mc_java_data_output_state ds;
    NEED(mc_java_files_create(NULL, &files, &fe) == MC_JAVA_FILE_OK);
    NEED(name_ascii(argv[1], &parent_name) && name_ascii("binary-owner.bin", &leaf));
    NEED(mc_java_file_allocate(files, &parent, &fe) == MC_JAVA_FILE_OK);
    NEED(mc_java_file_construct_string(parent, parent_name, &fe) == MC_JAVA_FILE_OK);
    bool made = false;
    NEED(mc_java_file_mkdirs(parent, &made, &fe) == MC_JAVA_FILE_OK);
    NEED(mc_java_file_allocate(files, &file, &fe) == MC_JAVA_FILE_OK);
    NEED(mc_java_file_construct_child(file, parent, leaf, &fe) == MC_JAVA_FILE_OK);
    NEED(mc_java_file_output_allocate(files, NULL, &raw, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_file_output_inspect(raw, &fs) && fs.phase == MC_JAVA_OUTPUT_RAW &&
         fs.argument == NULL && fs.path == NULL && fs.descriptor == NULL && fs.close_lock == NULL &&
         !fs.native_open && !fs.source_closed && fs.last_store_pc == -1);
    const size_t closes_before = actual_closes;
    NEED(mc_java_file_output_release(&raw, &e) == MC_JAVA_OUTPUT_OK && raw == NULL &&
         actual_closes == closes_before);
    NEED(mc_java_file_output_allocate(files, NULL, &f, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_file_output_construct(f, file, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_file_output_inspect(f, &fs) && fs.argument == file &&
         fs.path == mc_java_file_path(file) && fs.descriptor_parent == f && fs.native_open &&
         fs.cleanable_registered && fs.last_store_pc == 94);
    NEED(mc_java_file_output_retain(f, &e) == MC_JAVA_OUTPUT_OK);
    alias = f;
    NEED(mc_java_file_output_release(&alias, &e) == MC_JAVA_OUTPUT_OK && alias == NULL);
    NEED(mc_java_data_output_allocate(NULL, &d, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_data_output_construct(d, f, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_data_output_inspect(d, &ds) && ds.out == f && ds.write_buffer != NULL &&
         ds.bytearr == NULL && ds.written == 0 && !ds.closed && ds.last_store_pc == 15);
    const int64_t values[] = {0, -1, INT64_MIN, INT64_MAX, INT64_C(0x0102030405060708)};
    const uint8_t expected[] = {0,   0,   0,   0,   0, 0, 0, 0, 255, 255, 255, 255, 255, 255,
                                255, 255, 128, 0,   0, 0, 0, 0, 0,   0,   127, 255, 255, 255,
                                255, 255, 255, 255, 1, 2, 3, 4, 5,   6,   7,   8};
    uint8_t actual[sizeof(expected)];
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        NEED(mc_java_data_output_write_long(d, values[i], &e) == MC_JAVA_OUTPUT_OK);
    }
    NEED(read_bytes(file, actual, sizeof(actual)) && memcmp(actual, expected, sizeof(actual)) == 0);
    NEED(mc_java_data_output_inspect(d, &ds) && ds.written == (int32_t)sizeof(expected));
    /* A real semantic field input tests Java incCount saturation, not a
     * captured platform counter. The next actual write still happens. */
    d->written = INT32_MAX - 3;
    NEED(mc_java_data_output_write_long(d, 0, &e) == MC_JAVA_OUTPUT_OK && d->written == INT32_MAX);
    NEED(mc_java_data_output_retain(d, &e) == MC_JAVA_OUTPUT_OK);
    da = d;
    NEED(mc_java_data_output_release(&da, &e) == MC_JAVA_OUTPUT_OK && da == NULL);
    observed_fos = f;
    observed_dos = d;
    armed = true;
    close_fault = true;
    const size_t before_close = actual_closes;
    NEED(mc_java_data_output_source_close(d, &e) == MC_JAVA_OUTPUT_SOURCE_IO && e.cause.consumed &&
         e.cause.source_exception && callback_ok);
    armed = false;
    close_fault = false;
    NEED(mc_java_file_output_inspect(f, &fs) && !fs.native_open && fs.source_closed &&
         actual_closes == before_close + 1);
    NEED(mc_java_data_output_source_close(d, &e) == MC_JAVA_OUTPUT_OK &&
         actual_closes == before_close + 1);
    NEED(mc_java_data_output_write_long(d, 0, &e) == MC_JAVA_OUTPUT_SOURCE_IO &&
         d->written == INT32_MAX); /* Buffer stores precede closed FOS failure. */
    NEED(mc_java_data_output_flush(d, &e) == MC_JAVA_OUTPUT_OK); /* Actual no-op after close. */
    NEED(mc_java_data_output_release(&d, &e) == MC_JAVA_OUTPUT_OK && d == NULL);
    NEED(mc_java_file_output_release(&f, &e) == MC_JAVA_OUTPUT_OK && f == NULL &&
         actual_closes == before_close + 1);

    /* Real reopen truncates the same owned File, with a different FOS identity. */
    NEED(mc_java_file_output_allocate(files, NULL, &f, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_file_output_construct(f, file, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_data_output_allocate(NULL, &d, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_data_output_construct(d, f, &e) == MC_JAVA_OUTPUT_OK);
    observed_fos = f;
    observed_dos = d;
    armed = true;
    partial = true;
    NEED(mc_java_data_output_write_long(d, INT64_C(0x1122334455667788), &e) ==
             MC_JAVA_OUTPUT_SOURCE_IO &&
         e.committed == 3 && d->written == 0 && callback_ok);
    armed = false;
    partial = false;
    uint8_t prefix[3];
    NEED(read_bytes(file, prefix, 3) && prefix[0] == 0x11 && prefix[1] == 0x22 &&
         prefix[2] == 0x33);
    NEED(f->committed == 3 && d->write_buffer[7] == 0x88);
    /* Dropping an unclosed DOS does NOT source-close/flush the external FOS. */
    NEED(mc_java_data_output_release(&d, &e) == MC_JAVA_OUTPUT_OK && d == NULL);
    NEED(mc_java_file_output_inspect(f, &fs) && fs.native_open && !fs.source_close_reached);
    observed_dos = NULL;
    observed_fos = f;
    armed = true;
    close_fault = true;
    const size_t retire_closes = actual_closes;
    NEED(mc_java_file_output_release(&f, &e) == MC_JAVA_OUTPUT_NATIVE_FAILURE && f != NULL &&
         e.cause.consumed && callback_ok);
    armed = false;
    close_fault = false;
    NEED(mc_java_file_output_inspect(f, &fs) && fs.phase == MC_JAVA_OUTPUT_RETIRING &&
         !fs.native_open && !fs.source_close_reached && actual_closes == retire_closes + 1);
    NEED(mc_java_file_output_retain(f, &e) == MC_JAVA_OUTPUT_STATE);
    NEED(mc_java_file_output_release(&f, &e) == MC_JAVA_OUTPUT_OK && f == NULL &&
         actual_closes == retire_closes + 1);

    NEED(mc_java_data_output_allocate(NULL, &null_out, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_data_output_construct(null_out, NULL, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_data_output_write_long(null_out, -1, &e) == MC_JAVA_OUTPUT_SOURCE_NULL &&
         e.cause.site.pc == 108 && null_out->written == 0 && null_out->write_buffer[7] == 255);
    NEED(mc_java_data_output_source_close(null_out, &e) == MC_JAVA_OUTPUT_SOURCE_NULL &&
         e.cause.site.pc == 131 && e.suppressed.site.pc == 4 && e.suppressed_caller_count == 1 &&
         e.suppressed_callers[0].pc == 43 && null_out->closed);
    NEED(mc_java_data_output_source_close(null_out, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_data_output_release(&null_out, &e) == MC_JAVA_OUTPUT_OK);

    NEED(mc_java_file_output_allocate(files, NULL, &raw, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_file_output_construct(raw, NULL, &e) == MC_JAVA_OUTPUT_SOURCE_NULL &&
         e.cause.site.pc == 51 && raw->last_store == 12 && raw->fd == NULL);
    NEED(mc_java_file_output_construct(raw, file, &e) == MC_JAVA_OUTPUT_STATE);
    NEED(mc_java_file_output_release(&raw, &e) == MC_JAVA_OUTPUT_OK);
    mc_java_output_options o = mc_java_output_default_options();
    o.max_owner_bytes = sizeof(mc_java_data_output) + sizeof(mc_output_lock) + 7;
    NEED(mc_java_data_output_allocate(&o, &limited, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_data_output_construct(limited, NULL, &e) == MC_JAVA_OUTPUT_LIMIT &&
         e.cause.site.pc == 13 && limited->last_store == 7 && limited->write_buffer == NULL);
    NEED(mc_java_data_output_release(&limited, &e) == MC_JAVA_OUTPUT_OK);
    const uint16_t nul_name[] = {'x', 0, 'y'};
    NEED(mc_world_info_name_copy((mc_nbt_string){nul_name, 3}, 1024, &bad) == MC_WORLD_INFO_OK);
    NEED(mc_java_file_allocate(files, &invalid, &fe) == MC_JAVA_FILE_OK);
    NEED(mc_java_file_construct_child(invalid, parent, bad, &fe) == MC_JAVA_FILE_OK);
    NEED(mc_java_file_output_allocate(files, NULL, &raw, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_file_output_construct(raw, invalid, &e) == MC_JAVA_OUTPUT_SOURCE_IO &&
         e.cause.site.pc == 69 && raw->fd == NULL && raw->last_store == 12);
    NEED(mc_java_file_output_release(&raw, &e) == MC_JAVA_OUTPUT_OK);
    /* An actual directory reaches real open failure after fd/path stores. */
    NEED(mc_java_file_output_allocate(files, NULL, &raw, &e) == MC_JAVA_OUTPUT_OK);
    NEED(mc_java_file_output_construct(raw, parent, &e) == MC_JAVA_OUTPUT_SOURCE_IO &&
         raw->last_store == 94 && raw->fd != NULL && raw->fd->file == NULL &&
         strcmp(e.cause.exception_class, "java/io/FileNotFoundException") == 0 &&
         e.caller_count == 1 && e.callers[0].pc == 3);
    NEED(mc_java_file_output_release(&raw, &e) == MC_JAVA_OUTPUT_OK);
    result = 0;
done:
    armed = false;
    partial = false;
    close_fault = false;
    observed_fos = NULL;
    observed_dos = NULL;
#define CLEAN(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "binary cleanup line%d\n", __LINE__);                                  \
            result = 1;                                                                            \
        }                                                                                          \
    } while (0)
    CLEAN(mc_java_data_output_release(&limited, &e) == MC_JAVA_OUTPUT_OK);
    CLEAN(mc_java_data_output_release(&null_out, &e) == MC_JAVA_OUTPUT_OK);
    CLEAN(mc_java_data_output_release(&da, &e) == MC_JAVA_OUTPUT_OK);
    CLEAN(mc_java_data_output_release(&d, &e) == MC_JAVA_OUTPUT_OK);
    CLEAN(mc_java_file_output_release(&alias, &e) == MC_JAVA_OUTPUT_OK);
    CLEAN(mc_java_file_output_release(&raw, &e) == MC_JAVA_OUTPUT_OK);
    CLEAN(mc_java_file_output_release(&f, &e) == MC_JAVA_OUTPUT_OK);
    if (d != NULL || f != NULL || raw != NULL)
        return 1;
    CLEAN(mc_java_file_release(&invalid, &fe) == MC_JAVA_FILE_OK);
    CLEAN(mc_java_file_release(&file, &fe) == MC_JAVA_FILE_OK);
    CLEAN(mc_java_file_release(&parent, &fe) == MC_JAVA_FILE_OK);
    CLEAN(mc_java_files_release(&files, &fe) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(bad);
    mc_world_info_name_release(leaf);
    mc_world_info_name_release(parent_name);
    if (result == 0)
        printf("JAVA_BINARY_OUTPUT_OK checks=%u writes=%zu closes=%zu\n", checks, actual_writes,
               actual_closes);
    return result;
}
