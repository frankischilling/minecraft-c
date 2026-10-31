#include "mc/client/saves/save_format.h"
#include "mc/world/storage/source_handler.h"
#include "platform/time/millis.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
static mc_world_store *observed_store;
static mc_world_store_source_classes *observed_classes;
static mc_world_source_error *callback_error;
static bool armed, close_fault, reentry_ok = true;
static size_t actual_closes, actual_writes, reentries;

static void reentry(void) {
    ++reentries;
    mc_world_store *store = observed_store;
    mc_world_store_source_classes *classes = observed_classes;
    if (mc_world_store_source_retain(store, callback_error) != MC_WORLD_SOURCE_BUSY ||
        mc_world_store_source_release(&store, callback_error) != MC_WORLD_SOURCE_BUSY ||
        store != observed_store ||
        mc_world_store_source_classes_release(&classes, callback_error) != MC_WORLD_SOURCE_BUSY ||
        classes != observed_classes)
        reentry_ok = false;
}

/* Exact CO native primitive TU is linked once under these declared controls.
 * Real stdio runs FIRST. No stream/game child or successful IO is fabricated. */
size_t mc_test_output_write(const void *data, size_t size, size_t count, FILE *file) {
    ++actual_writes;
    const size_t written = fwrite(data, size, count, file);
    if (armed)
        reentry();
    return written;
}

int mc_test_output_ferror(FILE *file) {
    return ferror(file);
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

#define REQUIRE(value)                                                                             \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "handler check%u line%d: %s\n", checks, __LINE__, #value);             \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static mc_world_info_name *text(const char *ascii) {
    const size_t length = strlen(ascii);
    uint16_t *units = malloc((length == 0 ? 1 : length) * sizeof(*units));
    if (units == NULL)
        return NULL;
    for (size_t i = 0; i < length; ++i) {
        if ((unsigned char)ascii[i] >= 128) {
            free(units);
            return NULL;
        }
        units[i] = (unsigned char)ascii[i];
    }
    mc_world_info_name *name = NULL;
    mc_world_info_status s =
        mc_world_info_name_copy((mc_nbt_string){units, length}, 1048576u, &name);
    free(units);
    return s == MC_WORLD_INFO_OK ? name : NULL;
}

static int file_root(mc_java_files *files, const char *path, mc_java_file **out) {
    mc_java_file_error e;
    mc_world_info_name *name = text(path);
    REQUIRE(name != NULL);
    REQUIRE(mc_java_file_allocate(files, out, &e) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_construct_string(*out, name, &e) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(name);
    return 0;
}

static int verify_token(mc_java_files *files, const mc_world_source_state *state) {
    mc_java_file_error e;
    mc_java_file_stream *input = NULL;
    REQUIRE(mc_java_file_stream_allocate(files, &input, &e) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_stream_construct(input, state->session_file, &e) == MC_JAVA_FILE_OK);
    uint8_t bytes[8] = {0};
    size_t used = 0;
    while (used != sizeof(bytes)) {
        mc_java_file_read read;
        REQUIRE(mc_java_file_stream_read(input, bytes + used, sizeof(bytes) - used, &read, &e) ==
                MC_JAVA_FILE_OK);
        REQUIRE(read.count != 0 && read.count <= sizeof(bytes) - used);
        used += read.count;
    }
    const uint64_t bits = (uint64_t)state->source_e;
    for (size_t i = 0; i < sizeof(bytes); ++i)
        REQUIRE(bytes[i] == (uint8_t)(bits >> (56u - 8u * i)));
    int64_t length = 0;
    REQUIRE(mc_java_file_length(state->session_file, &length, &e) == MC_JAVA_FILE_OK &&
            length == 8);
    REQUIRE(mc_java_file_stream_source_close(input, &e) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_stream_release(&input, &e) == MC_JAVA_FILE_OK && input == NULL);
    return 0;
}

static int normal(mc_world_store_source_classes *classes, mc_java_files *files, mc_java_file *root,
                  bool players, mc_world_store **out, mc_world_source_error *e) {
    mc_world_info_name *name = text(players ? "with-players" : "without-players");
    REQUIRE(name != NULL);
    REQUIRE(mc_world_store_source_allocate(classes, out, e) == MC_WORLD_SOURCE_OK);
    mc_world_source_state before;
    REQUIRE(mc_world_store_source_inspect(*out, &before));
    REQUIRE(before.phase == MC_WORLD_SOURCE_RAW && before.source_e == 0 && !before.e_stored &&
            before.source_b == NULL && before.source_c == NULL && before.source_d == NULL &&
            before.pending_file == NULL && before.pending_fos == NULL &&
            before.pending_dos == NULL);
    int64_t first = 0, last = 0;
    REQUIRE(mc_platform_current_time_millis(&first) == MC_PLATFORM_MILLIS_OK);
    observed_store = *out;
    observed_classes = classes;
    armed = true;
    REQUIRE(mc_world_store_source_construct(*out, root, name, players, e) == MC_WORLD_SOURCE_OK);
    armed = false;
    REQUIRE(reentry_ok && reentries != 0 && actual_writes != 0);
    REQUIRE(mc_platform_current_time_millis(&last) == MC_PLATFORM_MILLIS_OK);
    mc_world_info_name_release(name);
    mc_world_source_state state;
    REQUIRE(mc_world_store_source_inspect(*out, &state));
    REQUIRE(state.phase == MC_WORLD_SOURCE_READY && state.e_stored && state.last_store_pc == 63 &&
            state.source_b != NULL && state.source_c != NULL && state.source_d != NULL &&
            state.source_b != root && state.source_b != state.source_c &&
            state.source_b != state.source_d && state.source_c != state.source_d &&
            state.session_file != state.source_b && state.session_file_stored &&
            state.output_stored && state.write_returned && state.close_returned &&
            state.last_close_pc == 39 && state.d_returned && state.fm_returned &&
            state.mx_returned);
    /* No equality between independent clocks. A backward wall-clock movement
     * is reported rather than hidden by broad arithmetic normalization. */
    REQUIRE(first <= last && state.source_e >= first && state.source_e <= last);
    int64_t canonical = 0;
    REQUIRE(mc_world_store_session_token_get(*out, &canonical) == MC_WORLD_STORAGE_OK &&
            canonical == state.source_e);
    REQUIRE(mc_java_file_namespace(state.source_b) == files &&
            mc_java_file_namespace(state.source_c) == files &&
            mc_java_file_namespace(state.source_d) == files &&
            mc_java_file_namespace(state.session_file) == files);
    mc_java_file_state b, c, d;
    mc_java_file_error fe;
    REQUIRE(mc_java_file_read_state(state.source_b, &b, &fe) == MC_JAVA_FILE_OK &&
            b.mkdir_calls == 1);
    REQUIRE(mc_java_file_read_state(state.source_c, &c, &fe) == MC_JAVA_FILE_OK &&
            c.mkdir_calls == (players ? 1u : 0u));
    REQUIRE(mc_java_file_read_state(state.source_d, &d, &fe) == MC_JAVA_FILE_OK &&
            d.mkdir_calls == 1);
    bool exists = false;
    REQUIRE(mc_java_file_exists(state.source_c, &exists, &fe) == MC_JAVA_FILE_OK &&
            exists == players);
    REQUIRE(mc_java_file_exists(state.source_d, &exists, &fe) == MC_JAVA_FILE_OK && exists);
    mc_java_file_output_state fos;
    mc_java_data_output_state dos;
    REQUIRE(mc_java_file_output_inspect(state.pending_fos, &fos) &&
            mc_java_data_output_inspect(state.pending_dos, &dos));
    REQUIRE(fos.argument == state.session_file &&
            fos.path == mc_java_file_path(state.session_file) && fos.bytes_committed == 8 &&
            !fos.native_open && fos.source_closed && fos.source_close_calls == 1 &&
            dos.out == state.pending_fos && dos.written == 8 && dos.closed &&
            dos.source_close_calls == 1 && dos.write_buffer != NULL && dos.bytearr == NULL &&
            dos.close_lock != fos.close_lock);
    REQUIRE(verify_token(files, &state) == 0);
    mc_world_storage_error legacy;
    mc_world_info *info = NULL;
    REQUIRE(mc_world_store_load_world_info(*out, MC_WORLD_INFO_CLIENT, &info, &legacy) ==
                MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
            info == NULL);
    REQUIRE(mc_world_store_check_session(*out, &legacy) == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    REQUIRE(mc_world_store_source_read_info(*out, &info, e) == MC_WORLD_SOURCE_OK && info == NULL);
    mc_world_source_input_state input;
    REQUIRE(mc_world_store_source_input_inspect(*out, &input) &&
            input.call == MC_WORLD_SOURCE_INPUT_METADATA && input.old_file && !input.exists &&
            input.source_returned && input.output_transferred && input.last_pc == 109 &&
            input.stream == NULL && input.reader == NULL && input.info == NULL);
    REQUIRE(mc_world_store_source_check_session(*out, e) == MC_WORLD_SOURCE_OK);
    REQUIRE(mc_world_store_source_input_inspect(*out, &input) &&
            input.call == MC_WORLD_SOURCE_INPUT_SESSION && input.token_read &&
            input.token_matches && input.observed_token == state.source_e && input.closed &&
            input.close_pc == 53 && input.source_returned && input.last_pc == 80 &&
            input.reader != NULL && input.stream != NULL);
    mc_nbt_stream_state reader;
    REQUIRE(mc_nbt_stream_state_read(input.reader, &reader, &e->input) == MC_NBT_OK &&
            reader.source_in == input.stream && reader.close_calls == 1 && reader.position == 8 &&
            reader.candidate == NULL);
    REQUIRE(mc_world_store_source_input_clear(*out, e) == MC_WORLD_SOURCE_OK);
    REQUIRE(!mc_world_store_source_input_inspect(*out, &input));
    /* This fixture deliberately has no shared rj namespace. The genuine mx
     * factory must reject that native binding before allocating an ld, rather
     * than returning the old source-loader implementation frontier. */
    mc_world_source_chunk_loader *loader = NULL;
    REQUIRE(mc_world_store_source_regions(*out) == NULL);
    REQUIRE(mc_world_store_source_loader(*out, NULL, &loader, e) == MC_WORLD_SOURCE_STATE &&
            loader == NULL && e->native_policy && !e->source_exception && !e->diagnostic_frontier &&
            e->frame_count == 0 && e->operation != NULL &&
            strcmp(e->operation, "mx.a shared rj namespace binding") == 0);
    mc_world_source_state after_loader;
    REQUIRE(mc_world_store_source_inspect(*out, &after_loader) &&
            after_loader.phase == MC_WORLD_SOURCE_READY && after_loader.holds == state.holds &&
            after_loader.source_b == state.source_b && after_loader.source_c == state.source_c &&
            after_loader.source_d == state.source_d && after_loader.source_e == state.source_e &&
            !after_loader.active && !after_loader.pending_file);
    return 0;
}

static int native_bound(mc_java_files *files, mc_java_logging *logging, mc_java_file *root,
                        mc_world_source_error *e) {
    mc_world_source_options limits = mc_world_store_source_default_options();
    limits.output.max_written_bytes = 4;
    mc_world_store_source_classes *classes = NULL;
    REQUIRE(mc_world_store_source_classes_create(files, logging, &limits, &classes, e) ==
            MC_WORLD_SOURCE_OK);
    mc_world_store *store = NULL;
    REQUIRE(mc_world_store_source_allocate(classes, &store, e) == MC_WORLD_SOURCE_OK);
    mc_world_info_name *name = text("limited");
    REQUIRE(name != NULL);
    REQUIRE(mc_world_store_source_construct(store, root, name, false, e) == MC_WORLD_SOURCE_LIMIT &&
            e->native_policy && !e->source_exception && !e->caught_io && !e->diagnostic_frontier);
    mc_world_info_name_release(name);
    mc_world_source_state state;
    REQUIRE(mc_world_store_source_inspect(store, &state) && state.phase == MC_WORLD_SOURCE_FAILED &&
            state.output_stored && !state.write_returned && !state.close_returned &&
            state.last_close_pc == -1);
    mc_java_data_output_state dos;
    mc_java_file_output_state fos;
    REQUIRE(mc_java_data_output_inspect(state.pending_dos, &dos) && dos.written == 0 &&
            dos.source_close_calls == 0 && !dos.closed && dos.write_buffer != NULL);
    REQUIRE(mc_java_file_output_inspect(state.pending_fos, &fos) && fos.bytes_committed == 0 &&
            fos.native_open && fos.source_close_calls == 0);
    for (size_t i = 0; i < 8; ++i)
        REQUIRE(dos.write_buffer[i] == (uint8_t)((uint64_t)state.source_e >> (56u - 8u * i)));
    mc_java_file *session = state.session_file;
    mc_java_file_error fe;
    REQUIRE(mc_java_file_retain(session, &fe) == MC_JAVA_FILE_OK);
    observed_store = store;
    observed_classes = classes;
    armed = close_fault = true;
    const size_t before = actual_closes;
    REQUIRE(mc_world_store_source_release(&store, e) == MC_WORLD_SOURCE_OUTPUT && store != NULL);
    armed = close_fault = false;
    REQUIRE(actual_closes == before + 1 && reentry_ok);
    REQUIRE(mc_world_store_source_inspect(store, &state) &&
            state.phase == MC_WORLD_SOURCE_RETIRING && state.pending_dos == NULL &&
            state.pending_fos != NULL && state.cleanup->native_policy &&
            !state.cleanup->source_exception);
    REQUIRE(mc_java_file_output_inspect(state.pending_fos, &fos) && !fos.native_open &&
            fos.source_close_calls == 0 && fos.native_close_calls == 1);
    REQUIRE(mc_world_store_source_retain(store, e) == MC_WORLD_SOURCE_BUSY);
    REQUIRE(mc_world_store_source_release(&store, e) == MC_WORLD_SOURCE_OK && store == NULL &&
            actual_closes == before + 1);
    int64_t length = -1;
    REQUIRE(mc_java_file_length(session, &length, &fe) == MC_JAVA_FILE_OK && length == 0);
    REQUIRE(mc_java_file_release(&session, &fe) == MC_JAVA_FILE_OK && session == NULL);
    REQUIRE(mc_world_store_source_classes_retire(&classes, e) == MC_WORLD_SOURCE_OK &&
            classes == NULL);
    return 0;
}

static int actual_io_frontier(mc_world_store_source_classes *classes, mc_java_files *files,
                              mc_java_file *root, mc_world_source_error *e) {
    mc_world_info_name *name = text("obstacle");
    REQUIRE(name != NULL);
    mc_java_file_error fe;
    mc_java_file *obstacle = NULL;
    REQUIRE(mc_java_file_allocate(files, &obstacle, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_construct_child(obstacle, root, name, &fe) == MC_JAVA_FILE_OK);
    mc_java_file_output *fos = NULL;
    mc_java_data_output *dos = NULL;
    mc_java_output_error *oe = calloc(1, sizeof(*oe));
    REQUIRE(oe != NULL);
    REQUIRE(mc_java_file_output_allocate(files, NULL, &fos, oe) == MC_JAVA_OUTPUT_OK);
    REQUIRE(mc_java_file_output_construct(fos, obstacle, oe) == MC_JAVA_OUTPUT_OK);
    REQUIRE(mc_java_data_output_allocate(NULL, &dos, oe) == MC_JAVA_OUTPUT_OK);
    REQUIRE(mc_java_data_output_construct(dos, fos, oe) == MC_JAVA_OUTPUT_OK);
    REQUIRE(mc_java_data_output_write_long(dos, INT64_C(0x0102030405060708), oe) ==
            MC_JAVA_OUTPUT_OK);
    REQUIRE(mc_java_data_output_source_close(dos, oe) == MC_JAVA_OUTPUT_OK);
    REQUIRE(mc_java_data_output_release(&dos, oe) == MC_JAVA_OUTPUT_OK);
    REQUIRE(mc_java_file_output_release(&fos, oe) == MC_JAVA_OUTPUT_OK);
    free(oe);
    mc_world_store *store = NULL;
    REQUIRE(mc_world_store_source_allocate(classes, &store, e) == MC_WORLD_SOURCE_OK);
    REQUIRE(mc_world_store_source_construct(store, root, name, true, e) ==
            MC_WORLD_SOURCE_UNIMPLEMENTED);
    REQUIRE(e->caught_io && e->diagnostic_frontier && e->source_exception &&
            e->output.cause.source_exception && e->cause_site.pc == 23 && e->frames[0].pc == 4 &&
            e->frames[1].pc == 87 && e->frames[2].pc == 57 &&
            strcmp(e->operation, "java/io/IOException.printStackTrace()V") == 0);
    mc_world_source_state state;
    REQUIRE(mc_world_store_source_inspect(store, &state));
    REQUIRE(state.phase == MC_WORLD_SOURCE_FAILED && state.last_store_pc == 63 && state.e_stored &&
            state.source_b != NULL && state.source_c != NULL && state.source_d != NULL &&
            state.session_file_stored && !state.output_stored && !state.d_returned &&
            state.pending_fos != NULL && state.pending_dos != NULL);
    REQUIRE(mc_world_store_source_construct(store, root, name, true, e) == MC_WORLD_SOURCE_STATE);
    REQUIRE(mc_world_store_source_release(&store, e) == MC_WORLD_SOURCE_OK && store == NULL);
    int64_t length = 0;
    REQUIRE(mc_java_file_length(obstacle, &length, &fe) == MC_JAVA_FILE_OK && length == 8);
    REQUIRE(mc_java_file_release(&obstacle, &fe) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(name);
    return 0;
}

int main(int argc, char **argv) {
    REQUIRE(argc == 2);
    mc_world_source_error *e = calloc(1, sizeof(*e));
    callback_error = calloc(1, sizeof(*callback_error));
    mc_client_save_error *se = calloc(1, sizeof(*se));
    REQUIRE(e != NULL && se != NULL && callback_error != NULL);
    mc_java_file_error fe;
    mc_java_files *files = NULL;
    REQUIRE(mc_java_files_create(NULL, &files, &fe) == MC_JAVA_FILE_OK);
    mc_java_file *root = NULL;
    REQUIRE(file_root(files, argv[1], &root) == 0);
    mc_java_logging *logging = NULL;
    mc_java_logging_error le;
    REQUIRE(mc_java_logging_create(NULL, &logging, &le) == MC_JAVA_LOGGING_OK);
    mc_world_store_source_classes *classes = NULL;
    REQUIRE(mc_world_store_source_classes_create(files, logging, NULL, &classes, e) ==
            MC_WORLD_SOURCE_OK);
    mc_world_source_class_state cold;
    mc_java_logging_state logger_state;
    REQUIRE(mc_world_store_source_classes_inspect(classes, &cold) &&
            cold.phase == MC_WORLD_SOURCE_RAW && cold.source_a == NULL && cold.minecraft == NULL &&
            cold.literal_allocations == 0);
    REQUIRE(mc_java_logging_inspect(logging, &logger_state, &le) == MC_JAVA_LOGGING_OK &&
            logger_state.phase == MC_JAVA_LOGGING_COLD);
    mc_world_store *raw = NULL;
    REQUIRE(mc_world_store_source_allocate(classes, &raw, e) == MC_WORLD_SOURCE_OK);
    mc_world_source_class_state active;
    REQUIRE(mc_world_store_source_classes_inspect(classes, &active) &&
            active.phase == MC_WORLD_SOURCE_READY && active.last_store_pc == 5 &&
            active.source_a != NULL && active.pending_logger == NULL);
    REQUIRE(mc_java_logging_inspect(logging, &logger_state, &le) == MC_JAVA_LOGGING_OK &&
            active.source_a == logger_state.minecraft && logger_state.registry_count == 3 &&
            !logger_state.console_constructed);
    mc_world_source_state state;
    REQUIRE(mc_world_store_source_inspect(raw, &state) && !state.e_stored && state.source_e == 0);
    mc_world_store_free(raw); /* Legacy void API cannot bypass counted source ownership. */
    REQUIRE(mc_world_store_source_inspect(raw, &state) && state.phase == MC_WORLD_SOURCE_RAW);
    REQUIRE(mc_world_store_source_retire(&raw, e) == MC_WORLD_SOURCE_OK && raw == NULL);
    mc_world_store *with = NULL, *without = NULL;
    REQUIRE(normal(classes, files, root, true, &with, e) == 0);
    REQUIRE(normal(classes, files, root, false, &without, e) == 0);
    REQUIRE(with != without);
    mc_world_store_source_classes *same = classes;
    REQUIRE(mc_world_store_source_classes_retire(&same, e) == MC_WORLD_SOURCE_BUSY &&
            same == classes);
    mc_world_store *alias = with;
    REQUIRE(mc_world_store_source_retain(with, e) == MC_WORLD_SOURCE_OK);
    REQUIRE(mc_world_store_source_retire(&with, e) == MC_WORLD_SOURCE_BUSY && with == alias);
    mc_world_storage_error legacy;
    REQUIRE(mc_world_store_free_checked(&alias, &legacy) == MC_WORLD_STORAGE_OK && alias == NULL);
    REQUIRE(mc_world_store_source_inspect(with, &state) && state.holds == 1);
    mc_java_data_output *external_dos = state.pending_dos;
    mc_java_file *external_file = state.session_file;
    mc_java_output_error *oe = calloc(1, sizeof(*oe));
    REQUIRE(oe != NULL);
    REQUIRE(mc_java_data_output_retain(external_dos, oe) == MC_JAVA_OUTPUT_OK);
    REQUIRE(mc_java_file_retain(external_file, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_world_store_source_retire(&with, e) == MC_WORLD_SOURCE_OK && with == NULL);
    mc_java_data_output_state surviving;
    REQUIRE(mc_java_data_output_inspect(external_dos, &surviving) && surviving.closed &&
            surviving.written == 8);
    REQUIRE(mc_java_data_output_release(&external_dos, oe) == MC_JAVA_OUTPUT_OK &&
            external_dos == NULL);
    REQUIRE(mc_java_file_release(&external_file, &fe) == MC_JAVA_FILE_OK && external_file == NULL);
    free(oe);
    REQUIRE(actual_io_frontier(classes, files, root, e) == 0);
    REQUIRE(native_bound(files, logging, root, e) == 0);

    mc_client_save_formats *formats = NULL;
    mc_client_save_format *format = NULL;
    REQUIRE(mc_client_save_formats_create(files, NULL, NULL, &formats, se) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_formats_bind_handlers(formats, classes, se) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_allocate(formats, &format, se) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_construct(format, root, se) == MC_CLIENT_SAVE_OK);
    mc_world_store *out = without; /* Borrowed failure sentinel; without owns its occurrence. */
    REQUIRE(mc_client_save_format_open(format, NULL, false, &out, se) ==
                MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE &&
            out == without && se->frames[0].pc == 10 && se->handler != NULL &&
            se->handler->file.status == MC_JAVA_FILE_SOURCE_NULL_STRING);
    mc_client_save_format_state failed;
    REQUIRE(mc_client_save_format_read_state(format, &failed, se) == MC_CLIENT_SAVE_OK &&
            failed.pending_handler != NULL && failed.pending_handler != without);
    REQUIRE(mc_world_store_source_inspect(failed.pending_handler, &state) && state.e_stored &&
            state.last_store_pc == 8 && state.source_b == NULL && state.pending_file != NULL);
    REQUIRE(mc_client_save_format_open(format, NULL, false, &out, se) == MC_CLIENT_SAVE_BUSY &&
            out == without);
    REQUIRE(mc_client_save_format_clear_failure(format, se) == MC_CLIENT_SAVE_OK);
    mc_world_info_name *name = text("from-hi");
    REQUIRE(name != NULL);
    out = NULL;
    REQUIRE(mc_client_save_format_open(format, name, false, &out, se) == MC_CLIENT_SAVE_OK &&
            out != NULL);
    REQUIRE(mc_world_store_source_inspect(out, &state) && state.mx_returned && state.d_returned);
    REQUIRE(verify_token(files, &state) == 0);
    mc_world_info_name_release(name);
    REQUIRE(mc_client_save_format_release(&format, se) == MC_CLIENT_SAVE_OK && format == NULL);
    REQUIRE(mc_client_save_formats_release(&formats, se) == MC_CLIENT_SAVE_OK && formats == NULL);
    REQUIRE(mc_world_store_source_release(&out, e) == MC_WORLD_SOURCE_OK && out == NULL);
    REQUIRE(mc_world_store_source_release(&without, e) == MC_WORLD_SOURCE_OK && without == NULL);

    mc_world_source_options bounded = mc_world_store_source_default_options();
    bounded.max_allocations = 0;
    mc_world_store_source_classes *limited = NULL;
    REQUIRE(mc_world_store_source_classes_create(files, logging, &bounded, &limited, e) ==
            MC_WORLD_SOURCE_OK);
    REQUIRE(mc_world_store_source_allocate(limited, &out, e) == MC_WORLD_SOURCE_LIMIT &&
            out == NULL && e->frames[0].pc == 0 && strcmp(e->frames[0].method, "<clinit>") == 0);
    REQUIRE(mc_world_store_source_classes_inspect(limited, &active) &&
            active.phase == MC_WORLD_SOURCE_FAILED && active.source_a == NULL &&
            active.minecraft == NULL);
    REQUIRE(mc_world_store_source_allocate(limited, &out, e) == MC_WORLD_SOURCE_LATCHED &&
            out == NULL && e->cause_status == MC_WORLD_SOURCE_LIMIT);
    REQUIRE(mc_world_store_source_classes_retire(&limited, e) == MC_WORLD_SOURCE_OK &&
            limited == NULL);
    /* Class activation precedes native NEW storage admission. The same real
     * CS registry returns its same Logger; no per-Store fake service exists. */
    bounded.max_allocations = 1;
    REQUIRE(mc_world_store_source_classes_create(files, logging, &bounded, &limited, e) ==
            MC_WORLD_SOURCE_OK);
    REQUIRE(mc_world_store_source_allocate(limited, &out, e) == MC_WORLD_SOURCE_LIMIT &&
            out == NULL && e->native_policy && e->frame_count == 0);
    REQUIRE(mc_world_store_source_classes_inspect(limited, &active) &&
            active.phase == MC_WORLD_SOURCE_READY && active.source_a == logger_state.minecraft &&
            active.last_store_pc == 5 && active.handlers == 0);
    REQUIRE(mc_world_store_source_classes_retire(&limited, e) == MC_WORLD_SOURCE_OK &&
            limited == NULL);
    REQUIRE(mc_world_store_source_classes_retire(&classes, e) == MC_WORLD_SOURCE_OK &&
            classes == NULL);
    REQUIRE(mc_java_logging_retire(&logging, &le) == MC_JAVA_LOGGING_OK && logging == NULL);
    REQUIRE(mc_java_file_release(&root, &fe) == MC_JAVA_FILE_OK && root == NULL);
    REQUIRE(mc_java_files_release(&files, &fe) == MC_JAVA_FILE_OK && files == NULL);
    free(se);
    free(e);
    free(callback_error);
    printf("Actual source mx/fm authored contracts: %u checks\n", checks);
    return 0;
}
