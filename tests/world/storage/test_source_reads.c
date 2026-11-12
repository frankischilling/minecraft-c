#include "mc/world/storage/source_handler.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef MC_SOURCE_READ_CLOSE_CONTROL
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#endif

static unsigned checks;
static mc_world_source_error *error, *reentry_error;
#ifdef MC_SOURCE_READ_CLOSE_CONTROL
static mc_world_store *observed;
static bool close_fault, reentry_ok = true;
static size_t closes, reentries;
#endif
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "source reads check%u line%d: %s\n", checks, __LINE__, #x);            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

#ifdef MC_SOURCE_READ_CLOSE_CONTROL
/* Included real FIS native owner, with a balanced descriptor close FIRST.
 * The following declared failure tests consume/error ordering, not a fake FD. */
int mc_test_save_close(int fd) {
#ifdef _WIN32
    int result = _close(fd);
#else
    int result = close(fd);
#endif
    ++closes;
    if (observed != NULL) {
        ++reentries;
        mc_world_info *out = NULL;
        mc_world_store *same = observed;
        if (mc_world_store_source_read_info(observed, &out, reentry_error) !=
                MC_WORLD_SOURCE_BUSY ||
            out != NULL ||
            mc_world_store_source_check_session(observed, reentry_error) != MC_WORLD_SOURCE_BUSY ||
            mc_world_store_source_input_clear(observed, reentry_error) != MC_WORLD_SOURCE_BUSY ||
            mc_world_store_source_retire(&same, reentry_error) != MC_WORLD_SOURCE_BUSY ||
            same != observed)
            reentry_ok = false;
    }
    if (close_fault) {
        errno = EIO;
        return -1;
    }
    return result;
}
#endif

static mc_world_info_name *text(const char *ascii) {
    size_t n = strlen(ascii);
    uint16_t *units = malloc((n == 0 ? 1 : n) * sizeof(*units));
    if (units == NULL)
        return NULL;
    for (size_t i = 0; i < n; ++i) {
        if ((unsigned char)ascii[i] >= 128) {
            free(units);
            return NULL;
        }
        units[i] = (unsigned char)ascii[i];
    }
    mc_world_info_name *name = NULL;
    mc_world_info_status s = mc_world_info_name_copy((mc_nbt_string){units, n}, 1048576u, &name);
    free(units);
    return s == MC_WORLD_INFO_OK ? name : NULL;
}

static int make_store(mc_world_store_source_classes *classes, mc_java_file *root,
                      const char *folder, mc_world_store **out) {
    mc_world_info_name *name = text(folder);
    CHECK(name != NULL);
    CHECK(mc_world_store_source_allocate(classes, out, error) == MC_WORLD_SOURCE_OK);
    CHECK(mc_world_store_source_construct(*out, root, name, false, error) == MC_WORLD_SOURCE_OK);
    mc_world_info_name_release(name);
    return 0;
}

static int metadata_ok(mc_world_store_source_classes *classes, mc_java_file *root,
                       const char *folder, int64_t seed, bool old, bool empty,
                       mc_world_info **keep) {
    mc_world_store *store = NULL;
    CHECK(make_store(classes, root, folder, &store) == 0);
    mc_world_info *info = NULL;
    CHECK(mc_world_store_source_read_info(store, &info, error) == MC_WORLD_SOURCE_OK &&
          info != NULL);
    mc_world_source_input_state input;
    CHECK(mc_world_store_source_input_inspect(store, &input) &&
          input.call == MC_WORLD_SOURCE_INPUT_METADATA && input.old_file == old && input.parsed &&
          input.closed && input.source_returned && input.output_transferred && input.info == NULL &&
          input.last_pc == (old ? 102 : 48));
    mc_world_info_fields fields;
    CHECK(mc_world_info_fields_get(info, &fields) == MC_WORLD_INFO_OK && fields.value.seed == seed);
    CHECK(fields.name == mc_world_info_data_name(input.data));
    if (empty)
        CHECK(fields.name == mc_world_info_name_empty());
    const mc_nbt_document *document = mc_world_info_data_document(input.data);
    CHECK(document != NULL);
    mc_nbt_stream_state reader;
    CHECK(mc_nbt_stream_state_read(input.reader, &reader, &error->input) == MC_NBT_OK &&
          reader.source_in == input.gzip && reader.candidate == document &&
          reader.close_calls == 1);
    mc_java_file_stream_state fis;
    CHECK(mc_java_file_stream_read_state(input.stream, &fis, &error->file) == MC_JAVA_FILE_OK &&
          !fis.native_open && fis.source_close_reached && fis.source_close_succeeded);
    /* Source Info/Data aliases survive clearing the actual reader/Store graph. */
    CHECK(mc_world_store_source_input_clear(store, error) == MC_WORLD_SOURCE_OK);
    CHECK(!mc_world_store_source_input_inspect(store, &input));
    CHECK(mc_world_info_fields_get(info, &fields) == MC_WORLD_INFO_OK && fields.value.seed == seed);
    CHECK(mc_world_store_source_retire(&store, error) == MC_WORLD_SOURCE_OK && store == NULL);
    if (keep != NULL)
        *keep = info;
    else
        mc_world_info_release(info);
    return 0;
}

static int metadata_failure(mc_world_store_source_classes *classes, mc_java_file *root,
                            const char *folder, bool old, int call_pc, bool closed, bool parsed,
                            bool failed_info, mc_world_info *sentinel) {
    mc_world_store *store = NULL;
    CHECK(make_store(classes, root, folder, &store) == 0);
    mc_world_info *out = sentinel; /* Borrowed failure sentinel, not a new hold. */
    CHECK(mc_world_store_source_read_info(store, &out, error) == MC_WORLD_SOURCE_UNIMPLEMENTED &&
          out == sentinel);
    CHECK(error->frame_count == 1 && error->frames[0].pc == (old ? 105 : 51) &&
          error->cause_site.pc == call_pc && error->caught_exception && error->diagnostic_frontier);
    mc_world_source_input_state input;
    CHECK(mc_world_store_source_input_inspect(store, &input) && input.old_file == old &&
          !input.source_returned && !input.output_transferred && input.closed == closed &&
          input.parsed == parsed);
    CHECK((input.info != NULL) == failed_info);
    if (failed_info) {
        mc_world_info_state state;
        CHECK(mc_world_info_state_get(input.info, &state) == MC_WORLD_INFO_OK &&
              state.phase == MC_WORLD_INFO_FAILED_CONSTRUCTION && state.last_store_pc == 81);
    }
    mc_java_file_stream *external = NULL;
    if (!closed) {
        external = input.stream;
        CHECK(external != NULL &&
              mc_java_file_stream_retain(external, &error->file) == MC_JAVA_FILE_OK);
    }
    CHECK(mc_world_store_source_input_clear(store, error) == MC_WORLD_SOURCE_OK);
    if (external != NULL) {
        mc_java_file_stream_state state;
        CHECK(mc_java_file_stream_read_state(external, &state, &error->file) == MC_JAVA_FILE_OK &&
              state.native_open && !state.source_close_reached && state.holds == 1);
        CHECK(mc_java_file_stream_release(&external, &error->file) == MC_JAVA_FILE_OK &&
              external == NULL);
    }
    CHECK(mc_world_store_source_retire(&store, error) == MC_WORLD_SOURCE_OK && store == NULL);
    return 0;
}

/* Deliberately authored external input to an already-owned test session.lock.
 * This is not a game FileOutputStream body or an acknowledgement of one. */
static int lock_bytes(const char *root, int64_t value, size_t count) {
    char path[4096];
    int n = snprintf(path, sizeof(path), "%s/session/session.lock", root);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    uint64_t bits = (uint64_t)value;
    uint8_t bytes[9];
    for (size_t i = 0; i < 8; ++i)
        bytes[i] = (uint8_t)(bits >> (56u - 8u * i));
    bytes[8] = 0x5a;
    FILE *f = NULL;
#ifdef _MSC_VER
    CHECK(fopen_s(&f, path, "wb") == 0 && f != NULL);
#else
    f = fopen(path, "wb");
    CHECK(f != NULL);
#endif
    CHECK(fwrite(bytes, 1, count, f) == count && fclose(f) == 0);
    return 0;
}

static int sessions(mc_world_store_source_classes *classes, mc_java_file *root, const char *path) {
    mc_world_store *store = NULL;
    CHECK(make_store(classes, root, "session", &store) == 0);
    mc_world_source_state original;
    CHECK(mc_world_store_source_inspect(store, &original));
    CHECK(mc_world_store_source_check_session(store, error) == MC_WORLD_SOURCE_OK);
    mc_world_source_input_state input;
    CHECK(mc_world_store_source_input_inspect(store, &input) &&
          input.call == MC_WORLD_SOURCE_INPUT_SESSION && input.token_read && input.token_matches &&
          input.observed_token == original.source_e && input.closed && input.close_pc == 53 &&
          input.last_pc == 80 && input.source_returned);
    mc_nbt_stream_state reader;
    CHECK(mc_nbt_stream_state_read(input.reader, &reader, &error->input) == MC_NBT_OK &&
          reader.position == 8 && reader.source_in == input.stream && reader.candidate == NULL &&
          reader.close_calls == 1);
    mc_java_file_state file;
    CHECK(mc_java_file_read_state(input.file, &file, &error->file) == MC_JAVA_FILE_OK &&
          file.exists_calls == 0);
    mc_java_file *retained = input.file;
    CHECK(mc_java_file_retain(retained, &error->file) == MC_JAVA_FILE_OK);
    CHECK(mc_world_store_source_check_session(store, error) == MC_WORLD_SOURCE_OK);
    CHECK(mc_world_store_source_input_inspect(store, &input) && input.file != retained);
    CHECK(mc_java_file_release(&retained, &error->file) == MC_JAVA_FILE_OK && retained == NULL);
    static const int64_t values[] = {0, 1, INT64_MIN, INT64_MAX, -1};
    for (size_t i = 0; i < sizeof(values) / sizeof(*values); ++i) {
        CHECK(values[i] != original.source_e);
        CHECK(lock_bytes(path, values[i], 8) == 0);
        CHECK(mc_world_store_source_check_session(store, error) == MC_WORLD_SOURCE_UNIMPLEMENTED &&
              error->frames[0].pc == 42 && error->session_mismatch && !error->caught_io);
        CHECK(mc_world_store_source_input_inspect(store, &input) && input.token_read &&
              !input.token_matches && input.observed_token == values[i] && !input.source_returned &&
              !input.closed && input.close_pc == -1);
        CHECK(mc_nbt_stream_state_read(input.reader, &reader, &error->input) == MC_NBT_OK &&
              reader.position == 8);
        CHECK(mc_world_store_source_input_clear(store, error) == MC_WORLD_SOURCE_OK);
    }
    for (size_t n = 0; n < 8; ++n) {
        CHECK(lock_bytes(path, original.source_e, n) == 0);
        CHECK(mc_world_store_source_check_session(store, error) == MC_WORLD_SOURCE_UNIMPLEMENTED &&
              error->frames[0].pc == 70 && error->caught_io &&
              error->input.source_fault == MC_NBT_SOURCE_EOF && error->cause_site.pc == 31 &&
              !error->session_mismatch);
        CHECK(mc_world_store_source_input_inspect(store, &input) && !input.token_read &&
              input.close_pc == 61 && input.closed);
        CHECK(mc_nbt_stream_state_read(input.reader, &reader, &error->input) == MC_NBT_OK &&
              reader.position == n && reader.close_calls == 1 && reader.bytearr != NULL &&
              reader.read_buffer != NULL);
    }
    CHECK(lock_bytes(path, original.source_e, 9) == 0);
    CHECK(mc_world_store_source_check_session(store, error) == MC_WORLD_SOURCE_OK);
    CHECK(mc_world_store_source_input_inspect(store, &input));
    mc_java_file_stream_state fis;
    CHECK(mc_java_file_stream_read_state(input.stream, &fis, &error->file) == MC_JAVA_FILE_OK &&
          fis.native_bytes_read == 8 && !fis.native_open && fis.source_close_succeeded);
    CHECK(mc_world_store_source_input_clear(store, error) == MC_WORLD_SOURCE_OK);
    char lock[4096];
    int made = snprintf(lock, sizeof(lock), "%s/session/session.lock", path);
    CHECK(made > 0 && (size_t)made < sizeof(lock) && remove(lock) == 0);
    CHECK(mc_world_store_source_check_session(store, error) == MC_WORLD_SOURCE_UNIMPLEMENTED &&
          error->frames[0].pc == 70 && error->cause_site.pc == 23 && error->caught_io);
    CHECK(mc_world_store_source_input_inspect(store, &input) && !input.input_constructed &&
          input.reader != NULL && input.stream != NULL && input.close_pc == -1);
    CHECK(mc_world_store_source_retire(&store, error) == MC_WORLD_SOURCE_OK && store == NULL);
    return 0;
}

#ifdef MC_SOURCE_READ_CLOSE_CONTROL
static int close_failures(mc_world_store_source_classes *classes, mc_java_file *root,
                          const char *path) {
    mc_world_store *store = NULL;
    CHECK(make_store(classes, root, "session", &store) == 0);
    observed = store;
    close_fault = true;
    size_t before = closes;
    CHECK(mc_world_store_source_check_session(store, error) == MC_WORLD_SOURCE_UNIMPLEMENTED &&
          error->frames[0].pc == 70 && error->cause_site.pc == 53 && error->caught_io);
    close_fault = false;
    observed = NULL;
    CHECK(closes == before + 1 && reentry_ok && reentries != 0);
    mc_world_source_input_state input;
    CHECK(mc_world_store_source_input_inspect(store, &input) && input.token_matches &&
          !input.closed);
    mc_java_file_stream_state fis;
    CHECK(mc_java_file_stream_read_state(input.stream, &fis, &error->file) == MC_JAVA_FILE_OK &&
          !fis.native_open && fis.source_close_reached && !fis.source_close_succeeded);
    CHECK(mc_world_store_source_input_clear(store, error) == MC_WORLD_SOURCE_OK &&
          closes == before + 1);
    mc_world_source_state state;
    CHECK(mc_world_store_source_inspect(store, &state));
    CHECK(lock_bytes(path, state.source_e, 3) == 0);
    observed = store;
    close_fault = true;
    before = closes;
    CHECK(mc_world_store_source_check_session(store, error) == MC_WORLD_SOURCE_UNIMPLEMENTED &&
          error->frames[0].pc == 70 && error->cause_site.pc == 31 && error->prior_site.pc == 61 &&
          error->input.source_fault == MC_NBT_SOURCE_EOF &&
          error->input_close.source_fault == MC_NBT_SOURCE_INPUT);
    observed = NULL;
    close_fault = false;
    CHECK(mc_world_store_source_input_clear(store, error) == MC_WORLD_SOURCE_OK &&
          closes == before + 1);
    CHECK(mc_world_store_source_retire(&store, error) == MC_WORLD_SOURCE_OK && store == NULL);
    CHECK(make_store(classes, root, "primary", &store) == 0);
    observed = store;
    close_fault = true;
    before = closes;
    mc_world_info *out = NULL;
    CHECK(mc_world_store_source_read_info(store, &out, error) == MC_WORLD_SOURCE_UNIMPLEMENTED &&
          out == NULL && error->frames[0].pc == 51 && error->cause_site.pc == 29 &&
          error->prior_site.pc == 22 && error->caught_exception);
    observed = NULL;
    close_fault = false;
    CHECK(mc_world_store_source_input_inspect(store, &input) && input.parsed && !input.closed &&
          input.info == NULL && !input.source_returned);
    CHECK(mc_world_store_source_input_clear(store, error) == MC_WORLD_SOURCE_OK &&
          closes == before + 1 && reentry_ok);
    CHECK(mc_world_store_source_retire(&store, error) == MC_WORLD_SOURCE_OK && store == NULL);
    return 0;
}
#endif

int main(int argc, char **argv) {
    CHECK(argc == 2);
    error = calloc(1, sizeof(*error));
    reentry_error = calloc(1, sizeof(*reentry_error));
    CHECK(error != NULL && reentry_error != NULL);
    mc_java_files *files = NULL;
    mc_java_logging *logging = NULL;
    mc_world_store_source_classes *classes = NULL;
    CHECK(mc_java_files_create(NULL, &files, &error->file) == MC_JAVA_FILE_OK);
    CHECK(mc_java_logging_create(NULL, &logging, &error->logging) == MC_JAVA_LOGGING_OK);
    CHECK(mc_world_store_source_classes_create(files, logging, NULL, &classes, error) ==
          MC_WORLD_SOURCE_OK);
    mc_java_file *root = NULL;
    mc_world_info_name *path = text(argv[1]);
    CHECK(path != NULL);
    CHECK(mc_java_file_allocate(files, &root, &error->file) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_construct_string(root, path, &error->file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(path);
    mc_world_info *sentinel = NULL;
    CHECK(metadata_ok(classes, root, "primary", 42, false, false, &sentinel) == 0);
    CHECK(metadata_ok(classes, root, "old-only", 84, true, false, NULL) == 0);
    CHECK(metadata_ok(classes, root, "missing-data", 0, false, true, NULL) == 0);
    mc_world_store *missing = NULL;
    CHECK(make_store(classes, root, "absent", &missing) == 0);
    mc_world_info *none = sentinel;
    CHECK(mc_world_store_source_read_info(missing, &none, error) == MC_WORLD_SOURCE_OK &&
          none == NULL);
    mc_world_source_input_state input;
    CHECK(mc_world_store_source_input_inspect(missing, &input) && input.old_file && !input.exists &&
          input.source_returned && input.last_pc == 109 && input.stream == NULL);
    CHECK(mc_world_store_source_retire(&missing, error) == MC_WORLD_SOURCE_OK && missing == NULL);
    CHECK(metadata_failure(classes, root, "bad-primary", false, 29, false, false, false,
                           sentinel) == 0);
    CHECK(metadata_failure(classes, root, "bad-old", true, 83, false, false, false, sentinel) == 0);
    CHECK(metadata_failure(classes, root, "wrong-root", false, 29, true, false, false, sentinel) ==
          0);
    CHECK(metadata_failure(classes, root, "wrong-data", false, 36, true, true, false, sentinel) ==
          0);
    CHECK(metadata_failure(classes, root, "wrong-version", false, 45, true, true, true, sentinel) ==
          0);
    CHECK(sessions(classes, root, argv[1]) == 0);
#ifdef MC_SOURCE_READ_CLOSE_CONTROL
    CHECK(close_failures(classes, root, argv[1]) == 0);
#endif
    mc_world_info_release(sentinel);
    CHECK(mc_java_file_release(&root, &error->file) == MC_JAVA_FILE_OK && root == NULL);
    CHECK(mc_world_store_source_classes_retire(&classes, error) == MC_WORLD_SOURCE_OK &&
          classes == NULL);
    CHECK(mc_java_logging_retire(&logging, &error->logging) == MC_JAVA_LOGGING_OK &&
          logging == NULL);
    CHECK(mc_java_files_release(&files, &error->file) == MC_JAVA_FILE_OK && files == NULL);
    free(error);
    free(reentry_error);
    printf("Source metadata/session authored contracts: %u checks\n", checks);
    return 0;
}
