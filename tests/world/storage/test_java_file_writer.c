/* Genuine concrete FOS/typed Writer bodies once. Native fault controls forward
 * real byte writes/closes and invalidate only this process's own descriptor.
 * No game constructor, charset provider or encoder result is replaced. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "../../../src/platform/files/byte_output.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

static bool partial_write, fail_native_close, fault_failed;
static unsigned int forwarded_writes, forwarded_closes;

static bool invalidate_handle(FILE *file) {
#ifdef _WIN32
    /* Keep CRT descriptor bookkeeping live so real WriteFile/fclose observe
     * ERROR_INVALID_HANDLE; do not invoke CRT invalid-parameter machinery. */
    const intptr_t handle = _get_osfhandle(_fileno(file));
    return handle != -1 && CloseHandle((HANDLE)handle) != 0;
#else
    return close(fileno(file)) == 0;
#endif
}

static bool actual_write(FILE *file, const uint8_t *bytes, size_t length, size_t *committed,
                         mc_byte_output_error *error) {
    ++forwarded_writes;
    if (partial_write && length > 1) {
        partial_write = false;
        const bool ok = mc_byte_output_write(file, bytes, 1, committed, error);
        if (ok && *committed == 1 && !invalidate_handle(file))
            fault_failed = true;
        return ok;
    }
    return mc_byte_output_write(file, bytes, length, committed, error);
}

static bool actual_close(FILE **slot, mc_byte_output_error *error) {
    ++forwarded_closes;
    if (fail_native_close && *slot != NULL) {
        fail_native_close = false;
        if (!invalidate_handle(*slot))
            fault_failed = true;
    }
    return mc_byte_output_close(slot, error);
}

#define mc_byte_output_write actual_write
#define mc_byte_output_close actual_close
#define finish included_fos_finish
#include "../../../src/world/storage/java_output.c"
#undef finish
#undef mc_byte_output_close
#undef mc_byte_output_write

static bool busy_source_close;

/* Arrange an actual FOS native admission flag only at the reached close call.
 * Always forward the real canonical source-close result unchanged. */
static mc_java_output_status observed_source_close(mc_java_file_output *output,
                                                   mc_java_output_error *error) {
    if (busy_source_close) {
        busy_source_close = false;
        output->active = true;
    }
    return mc_java_file_output_source_close(output, error);
}

#define mc_java_file_output_source_close observed_source_close
#include "../../../src/world/storage/java_file_writer.c"
#undef mc_java_file_output_source_close

static unsigned int checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "typed Writer check%u line%d: %s\n", checks, __LINE__, #value);        \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static mc_java_files *files;
static mc_java_file *parent;
static const mc_java_writer_profile profile = {MC_JAVA_READER_UTF8, MC_JAVA_WRITER_PRINT,
                                               MC_JAVA_WRITER_LF};

static bool name_file(const char *ascii, mc_java_file **out) {
    const size_t length = strlen(ascii);
    uint16_t *units = malloc(length * sizeof(*units));
    CHECK(units != NULL);
    for (size_t n = 0; n < length; ++n) {
        CHECK((unsigned char)ascii[n] < 128);
        units[n] = (unsigned char)ascii[n];
    }
    mc_world_info_name *name = NULL;
    mc_java_file_error error = {0};
    mc_java_file *captured_parent = parent;
    CHECK(mc_world_info_name_copy((mc_nbt_string){units, length}, SIZE_MAX, &name) ==
          MC_WORLD_INFO_OK);
    free(units);
    CHECK(mc_java_file_allocate(files, out, &error) == MC_JAVA_FILE_OK);
    CHECK(captured_parent == NULL
              ? mc_java_file_construct_string(*out, name, &error) == MC_JAVA_FILE_OK
              : mc_java_file_construct_child(*out, captured_parent, name, &error) ==
                    MC_JAVA_FILE_OK);
    mc_world_info_name_release(name);
    return true;
}

static bool read_exact(mc_java_file *file, const uint8_t *expected, size_t length) {
    mc_java_file_stream *stream = NULL;
    mc_java_file_error error = {0};
    CHECK(mc_java_file_stream_allocate(files, &stream, &error) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_stream_construct(stream, file, &error) == MC_JAVA_FILE_OK);
    uint8_t bytes[128];
    size_t used = 0;
    bool eof = false;
    while (!eof && used < sizeof(bytes)) {
        mc_java_file_read read;
        CHECK(mc_java_file_stream_read(stream, bytes + used, sizeof(bytes) - used, &read, &error) ==
              MC_JAVA_FILE_OK);
        CHECK(read.count != 0 || read.eof);
        used += read.count;
        eof = read.eof;
    }
    CHECK(eof && used == length && (length == 0 || memcmp(bytes, expected, length) == 0));
    CHECK(mc_java_file_stream_source_close(stream, &error) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_stream_release(&stream, &error) == MC_JAVA_FILE_OK);
    return true;
}

static bool seed(mc_java_file *file) {
    static const uint8_t old[] = "PREVIOUS LONG CONTENT";
    mc_java_file_output *output = NULL;
    mc_java_output_error error = {0};
    CHECK(mc_java_file_output_allocate(files, NULL, &output, &error) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_file_output_construct(output, file, &error) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_file_output_write_span(output, old, sizeof(old) - 1, &error) ==
          MC_JAVA_OUTPUT_OK);
    CHECK(error.committed == sizeof(old) - 1);
    CHECK(mc_java_file_output_source_close(output, &error) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_file_output_release(&output, &error) == MC_JAVA_OUTPUT_OK);
    return true;
}

static bool ordinary(void) {
    mc_java_file *file = NULL;
    mc_java_typed_file_writer *writer = NULL;
    mc_java_typed_file_writer_error error = {0};
    mc_java_typed_file_writer_state state;
    CHECK(name_file("ordinary.txt", &file) && seed(file));
    CHECK(mc_java_typed_file_writer_allocate(files, NULL, &writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_inspect(writer, &state));
    CHECK(state.phase == MC_JAVA_TYPED_FILE_WRITER_RAW && state.file_output == NULL &&
          state.writer == NULL && !state.construct_entered);
    static const uint8_t previous[] = "PREVIOUS LONG CONTENT";
    CHECK(read_exact(file, previous, sizeof(previous) - 1));
    CHECK(mc_java_typed_file_writer_construct(writer, file, &profile, &error) == MC_JAVA_WRITER_OK);
    CHECK(read_exact(file, NULL, 0));
    CHECK(mc_java_typed_file_writer_inspect(writer, &state));
    CHECK(state.file_output_returned && state.writer_returned &&
          state.output_state.argument == file && state.output_state.native_open &&
          state.output_state.source_close_calls == 0);
    static const uint16_t a[] = {'A'}, b[] = {'B'};
    const mc_java_properties_string_view first = {a, 1}, second = {b, 1};
    CHECK(mc_java_typed_file_writer_println(writer, &first, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_println(writer, &second, &error) == MC_JAVA_WRITER_OK);
    const unsigned int closes_before_source_close = forwarded_closes;
    CHECK(mc_java_typed_file_writer_source_close(writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_inspect(writer, &state));
    CHECK(state.output_state.source_close_calls == 1 &&
          state.output_state.native_close_calls == 1 && state.output_state.source_close_succeeded &&
          !state.output_state.native_open && forwarded_closes == closes_before_source_close + 1 &&
          state.writer_state.encoder_closed && state.writer_state.source_close_reached &&
          !state.writer_state.trouble && !state.writer_state.print_writer_out_present);
    static const uint8_t both[] = "A\nB\n";
    CHECK(read_exact(file, both, sizeof(both) - 1));
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) == MC_JAVA_WRITER_OK && writer == NULL);
    CHECK(error.retirement.source_close_reached && !error.retirement.additional_sink_close &&
          forwarded_closes == closes_before_source_close + 1);
    CHECK(mc_java_typed_file_writer_allocate(files, NULL, &writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_construct(writer, file, &profile, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_println(writer, &first, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_source_close(writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) == MC_JAVA_WRITER_OK);
    static const uint8_t shorter[] = "A\n";
    CHECK(read_exact(file, shorter, sizeof(shorter) - 1));
    CHECK(mc_java_file_release(&file, NULL) == MC_JAVA_FILE_OK);
    return true;
}

static bool constructor_failures(void) {
    mc_java_file *file = NULL;
    mc_java_typed_file_writer *writer = NULL;
    mc_java_typed_file_writer_error error = {0};
    mc_java_typed_file_writer_state state;
    CHECK(mc_java_typed_file_writer_allocate(files, NULL, &writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_construct(writer, NULL, &profile, &error) != MC_JAVA_WRITER_OK);
    CHECK(error.source_exception && error.output.status == MC_JAVA_OUTPUT_SOURCE_NULL &&
          strcmp(error.output.cause.exception_class, "java/lang/NullPointerException") == 0);
    CHECK(mc_java_typed_file_writer_inspect(writer, &state));
    CHECK(state.phase == MC_JAVA_TYPED_FILE_WRITER_FAILED && state.file_output != NULL &&
          !state.file_output_returned && !state.writer_returned && state.writer == NULL);
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(name_file("charset.txt", &file) && seed(file));
    CHECK(mc_java_typed_file_writer_allocate(files, NULL, &writer, &error) == MC_JAVA_WRITER_OK);
    mc_java_writer_profile bad = profile;
    bad.charset =
        (mc_java_reader_charset)99; /* Actual native profile rejection, not fake locale. */
    CHECK(mc_java_typed_file_writer_construct(writer, file, &bad, &error) ==
          MC_JAVA_WRITER_INVALID_ARGUMENT);
    CHECK(!error.source_exception && error.writer.status == MC_JAVA_WRITER_INVALID_ARGUMENT);
    CHECK(read_exact(file, NULL, 0));
    CHECK(mc_java_typed_file_writer_inspect(writer, &state));
    CHECK(state.phase == MC_JAVA_TYPED_FILE_WRITER_FAILED && state.file_output_returned &&
          !state.writer_returned && state.output_state.native_open &&
          state.output_state.argument == file && !state.output_state.source_close_reached);
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(!error.retirement.source_close_reached);
    CHECK(mc_java_file_release(&file, NULL) == MC_JAVA_FILE_OK);
    return true;
}

static bool spans_and_legacy(void) {
    mc_java_file *file = NULL;
    mc_java_file_output *output = NULL;
    mc_java_data_output *data = NULL;
    mc_java_output_error error = {0};
    mc_java_file_output_state state;
    CHECK(name_file("spans.txt", &file));
    CHECK(mc_java_file_output_allocate(files, NULL, &output, &error) == MC_JAVA_OUTPUT_OK);
    static const uint8_t prefix[] = {'x', 'y'};
    CHECK(mc_java_file_output_write_span(output, prefix, 0, &error) == MC_JAVA_OUTPUT_STATE);
    CHECK(mc_java_file_output_construct(output, file, &error) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_file_output_write_span(output, prefix, 2, &error) == MC_JAVA_OUTPUT_OK &&
          error.committed == 2);
    CHECK(mc_java_data_output_allocate(NULL, &data, &error) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_data_output_construct(data, output, &error) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_data_output_write_long(data, INT64_C(0x1122334455667788), &error) ==
          MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_data_output_source_close(data, &error) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_file_output_inspect(output, &state));
    const size_t writes = state.native_write_calls;
    CHECK(state.source_close_calls == 1 && state.source_closed);
    CHECK(mc_java_file_output_write_span(output, prefix, 0, &error) == MC_JAVA_OUTPUT_OK &&
          error.committed == 0);
    CHECK(mc_java_file_output_inspect(output, &state) && state.native_write_calls == writes);
    CHECK(mc_java_file_output_write_span(output, prefix, 1, &error) == MC_JAVA_OUTPUT_SOURCE_IO &&
          error.cause.source_exception && error.cause.site.pc == 16);
    CHECK(mc_java_file_output_write_span(output, NULL, 0, &error) == MC_JAVA_OUTPUT_ARGUMENT &&
          !error.cause.source_exception);
    CHECK(mc_java_data_output_release(&data, &error) == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_file_output_release(&output, &error) == MC_JAVA_OUTPUT_OK);
    static const uint8_t expected[] = {'x', 'y', 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    CHECK(read_exact(file, expected, sizeof(expected)));
    CHECK(mc_java_file_release(&file, NULL) == MC_JAVA_FILE_OK);
    return true;
}

static bool native_cleanup(void) {
    mc_java_file *file = NULL;
    mc_java_typed_file_writer *writer = NULL;
    mc_java_typed_file_writer_error error = {0};
    mc_java_typed_file_writer_state state;
    static const uint16_t high[] = {0xd800};
    const mc_java_properties_string_view pending = {high, 1};
    CHECK(name_file("native.txt", &file));
    CHECK(mc_java_typed_file_writer_allocate(files, NULL, &writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_construct(writer, file, &profile, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_print(writer, &pending, &error) == MC_JAVA_WRITER_OK);
    writer->active = true; /* Actual native admission flag; no Source field mutation. */
    mc_java_typed_file_writer *slot = writer;
    CHECK(mc_java_typed_file_writer_retire(&slot, &error) == MC_JAVA_WRITER_BUSY && slot == writer);
    writer->active = false;
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) == MC_JAVA_WRITER_OK && writer == NULL);
    CHECK(!error.retirement.source_close_reached && error.retirement.additional_sink_close);
    CHECK(read_exact(file, NULL, 0)); /* No flush or final replacement from native cleanup. */
    CHECK(mc_java_typed_file_writer_allocate(files, NULL, &writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_construct(writer, file, &profile, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_print(writer, &pending, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_source_close(writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) == MC_JAVA_WRITER_OK);
    static const uint8_t replacement[] = {'?'};
    CHECK(read_exact(file, replacement, 1));
    CHECK(mc_java_file_release(&file, NULL) == MC_JAVA_FILE_OK);

    CHECK(name_file("busy-child.txt", &file));
    CHECK(mc_java_typed_file_writer_allocate(files, NULL, &writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_construct(writer, file, &profile, &error) == MC_JAVA_WRITER_OK);
    mc_java_file_output *child = writer->output;
    child->active = true; /* Arrange an actual child-owner BUSY admission. */
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) == MC_JAVA_WRITER_BUSY &&
          writer != NULL);
    CHECK(mc_java_typed_file_writer_inspect(writer, &state));
    CHECK(state.phase == MC_JAVA_TYPED_FILE_WRITER_RETIRING && state.writer == NULL &&
          state.file_output == child && state.writer_state.sink_close_attempted &&
          !state.writer_state.source_close_reached && !state.writer_state.encoder_closed);
    child->active = false;
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) == MC_JAVA_WRITER_OK && writer == NULL);
    CHECK(mc_java_file_release(&file, NULL) == MC_JAVA_FILE_OK);
    return true;
}

static bool io_and_policy(void) {
    mc_java_file *file = NULL;
    mc_java_typed_file_writer *writer = NULL;
    mc_java_typed_file_writer_error error = {0};
    mc_java_typed_file_writer_state state;
    static const uint16_t text[] = {'P', 'a', 'r', 't'};
    const mc_java_properties_string_view view = {text, 4};
    CHECK(name_file("partial.txt", &file));
    CHECK(mc_java_typed_file_writer_allocate(files, NULL, &writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_construct(writer, file, &profile, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_print(writer, &view, &error) == MC_JAVA_WRITER_OK);
    partial_write = true;
    CHECK(mc_java_typed_file_writer_flush(writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(!fault_failed && error.output.status == MC_JAVA_OUTPUT_SOURCE_IO &&
          error.output.committed == 1 && !error.source_exception);
    CHECK(mc_java_typed_file_writer_inspect(writer, &state));
    CHECK(state.writer_state.trouble && state.output_state.bytes_committed == 1);
    CHECK(mc_java_typed_file_writer_source_close(writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_inspect(writer, &state) &&
          state.first_output_failure->status == MC_JAVA_OUTPUT_SOURCE_IO &&
          state.first_output_failure->committed == 1);
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) == MC_JAVA_WRITER_OK);
    static const uint8_t partial[] = {'P'};
    CHECK(read_exact(file, partial, 1));
    CHECK(mc_java_file_release(&file, NULL) == MC_JAVA_FILE_OK);

    CHECK(name_file("policy.txt", &file));
    mc_java_typed_file_writer_options options = mc_java_typed_file_writer_default_options();
    options.file_output.max_written_bytes = 1;
    CHECK(mc_java_typed_file_writer_allocate(files, &options, &writer, &error) ==
          MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_construct(writer, file, &profile, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_print(writer, &view, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_flush(writer, &error) == MC_JAVA_WRITER_LIMIT);
    CHECK(error.output.status == MC_JAVA_OUTPUT_LIMIT && !error.source_exception);
    CHECK(mc_java_typed_file_writer_inspect(writer, &state) && !state.writer_state.trouble &&
          state.output_state.bytes_committed == 0);
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(read_exact(file, NULL, 0));
    CHECK(mc_java_file_release(&file, NULL) == MC_JAVA_FILE_OK);

    CHECK(name_file("close-retry.txt", &file));
    CHECK(mc_java_typed_file_writer_allocate(files, NULL, &writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_construct(writer, file, &profile, &error) == MC_JAVA_WRITER_OK);
    fail_native_close = true;
    const unsigned int prior_closes = forwarded_closes;
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) != MC_JAVA_WRITER_OK && writer != NULL);
    CHECK(!fault_failed && !error.source_exception &&
          error.output.status == MC_JAVA_OUTPUT_NATIVE_FAILURE);
    CHECK(mc_java_typed_file_writer_inspect(writer, &state));
    CHECK(state.writer == NULL && state.file_output != NULL &&
          state.phase == MC_JAVA_TYPED_FILE_WRITER_RETIRING &&
          state.writer_state.sink_close_attempted && !state.writer_state.source_close_reached &&
          !state.output_state.native_open && state.output_state.source_close_calls == 0 &&
          state.output_state.native_close_calls == 1);
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) == MC_JAVA_WRITER_OK && writer == NULL);
    CHECK(forwarded_closes == prior_closes + 1); /* No reused-descriptor second close. */
    CHECK(mc_java_file_release(&file, NULL) == MC_JAVA_FILE_OK);
    return true;
}

static bool callback_status_regressions(void) {
    mc_java_file *file = NULL;
    mc_java_typed_file_writer *writer = NULL;
    mc_java_typed_file_writer_error error = {0};
    mc_java_typed_file_writer_state state;
    static const uint16_t text[] = {'S', 'a', 'v', 'e'};
    const mc_java_properties_string_view view = {text, 4}, invalid = {NULL, 1};
    mc_java_typed_file_writer_options options = mc_java_typed_file_writer_default_options();
    options.file_output.max_written_bytes = 1;
    CHECK(name_file("callback-stale.txt", &file));
    CHECK(mc_java_typed_file_writer_allocate(files, &options, &writer, &error) ==
          MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_construct(writer, file, &profile, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_print(writer, &view, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_flush(writer, &error) == MC_JAVA_WRITER_LIMIT);
    CHECK(error.output.status == MC_JAVA_OUTPUT_LIMIT && !error.source_exception &&
          writer->restored_native_failure);
    CHECK(mc_java_typed_file_writer_print(writer, &invalid, &error) ==
          MC_JAVA_WRITER_INVALID_ARGUMENT);
    CHECK(error.writer.operation != NULL &&
          strcmp(error.writer.operation, "PrintWriter String input") == 0 &&
          writer->call_first_failure.status == MC_JAVA_OUTPUT_OK &&
          writer->call_native_failure.status == MC_JAVA_OUTPUT_OK &&
          !writer->restored_native_failure);
    /* The failed drain kept position0/limit4. A flush cannot resend those
     * bytes. This fresh Source print supplies new bytes through the SAME
     * owners; historical LIMIT cannot replace this operation's success. */
    writer->output->options.max_written_bytes = UINT64_MAX;
    CHECK(mc_java_typed_file_writer_print(writer, &view, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_flush(writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(!writer->restored_native_failure &&
          writer->call_native_failure.status == MC_JAVA_OUTPUT_OK);
    CHECK(mc_java_typed_file_writer_source_close(writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) == MC_JAVA_WRITER_OK && writer == NULL);
    static const uint8_t expected[] = {'S', 'a', 'v', 'e'};
    CHECK(read_exact(file, expected, sizeof(expected)));
    CHECK(mc_java_file_release(&file, NULL) == MC_JAVA_FILE_OK);

    CHECK(name_file("callback-primary-io.txt", &file));
    CHECK(mc_java_typed_file_writer_allocate(files, NULL, &writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_construct(writer, file, &profile, &error) == MC_JAVA_WRITER_OK);
    CHECK(mc_java_typed_file_writer_print(writer, &view, &error) == MC_JAVA_WRITER_OK);
    mc_java_file_output *child = writer->output;
    const unsigned int closes = forwarded_closes;
    partial_write = true;
    busy_source_close = true;
    CHECK(mc_java_typed_file_writer_source_close(writer, &error) == MC_JAVA_WRITER_OK);
    CHECK(!fault_failed && !busy_source_close && !writer->restored_native_failure &&
          writer->call_first_failure.status == MC_JAVA_OUTPUT_SOURCE_IO &&
          writer->call_native_failure.status == MC_JAVA_OUTPUT_BUSY &&
          error.output.status == MC_JAVA_OUTPUT_BUSY && !error.source_exception);
    CHECK(mc_java_typed_file_writer_inspect(writer, &state));
    CHECK(state.writer_state.trouble && state.writer_state.source_close_reached &&
          state.writer_state.encoder_closed && state.output_state.active &&
          state.output_state.bytes_committed == 1 && state.output_state.source_close_calls == 0 &&
          state.output_state.native_close_calls == 0 &&
          state.first_output_failure->status == MC_JAVA_OUTPUT_SOURCE_IO &&
          state.writer_state.secondary_close_error.status == MC_JAVA_WRITER_INVALID_ARGUMENT &&
          forwarded_closes == closes);
    /* The earlier selected IO was swallowed; the later native BUSY remains a
     * full retained secondary cause, then genuine Native cleanup must retry. */
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) == MC_JAVA_WRITER_BUSY &&
          writer != NULL);
    CHECK(mc_java_typed_file_writer_inspect(writer, &state) && state.writer == NULL &&
          state.file_output == child && state.phase == MC_JAVA_TYPED_FILE_WRITER_RETIRING);
    child->active = false;
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) != MC_JAVA_WRITER_OK && writer != NULL);
    CHECK(error.output.status == MC_JAVA_OUTPUT_NATIVE_FAILURE && !error.source_exception);
    CHECK(mc_java_typed_file_writer_retire(&writer, &error) == MC_JAVA_WRITER_OK && writer == NULL);
    CHECK(forwarded_closes == closes + 1 && !fault_failed);
    static const uint8_t prefix[] = {'S'};
    CHECK(read_exact(file, prefix, 1));
    CHECK(mc_java_file_release(&file, NULL) == MC_JAVA_FILE_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2 || mc_java_files_create(NULL, &files, NULL) != MC_JAVA_FILE_OK)
        return 2;
    if (!name_file(argv[1], &parent) || !ordinary() || !constructor_failures() ||
        !spans_and_legacy() || !native_cleanup() || !io_and_policy() ||
        !callback_status_regressions())
        return 1;
    if (mc_java_file_release(&parent, NULL) != MC_JAVA_FILE_OK ||
        mc_java_files_release(&files, NULL) != MC_JAVA_FILE_OK)
        return 1;
    printf("Actual typed FileWriter controls: %u checks\n", checks);
    return 0;
}
