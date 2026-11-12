#include "mc/world/saved_data/source_storage.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef MC_SAVED_SOURCE_CLOSE_CONTROL
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#endif

static unsigned checks;
static mc_world_saved_data_source_error *error, *reentry;
static mc_world_source_error *handler;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "source SavedData check%u line%d: %s\n", checks, __LINE__,             \
                    #condition);                                                                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

/* Authored interchange input: duplicate Short d, later Int x replacing Short,
 * nonnull empty name, and modified-UTF8 NUL/unpaired-surrogate names. The two
 * trailing bytes are not consumed by the source raw-compound reader. */
static const uint8_t fixture[] = {10,   0,    1,    'I',  2,    0,    1,    'a',  0x7f, 0xff, 1,
                                  0,    1,    'b',  0xff, 2,    0,    1,    'd',  0,    1,    2,
                                  0,    1,    'd',  0xff, 0xfe, 2,    0,    1,    'x',  0,    9,
                                  3,    0,    1,    'x',  0,    0,    0,    7,    2,    0,    0,
                                  0xff, 0xff, 2,    0,    12,   0xc0, 0x80, 0xed, 0xa0, 0x80, 'n',
                                  0xed, 0xb0, 0x80, 0xef, 0xbf, 0xbf, 0x80, 0,    0,    0xde, 0xad};
static const uint8_t malformed[] = {10, 0, 0, 2, 0, 1, 'a', 0};

#ifdef MC_SAVED_SOURCE_CLOSE_CONTROL
static mc_world_saved_data *observed;
static bool close_fault, reentry_ok = true;
static size_t closes, reentries;

/* Close the real descriptor FIRST, then report the declared test fault. This
 * controls native close/error ordering without inventing a completed input. */
int mc_test_save_close(int fd) {
#ifdef _WIN32
    int result = _close(fd);
#else
    int result = close(fd);
#endif
    ++closes;
    if (observed != NULL) {
        ++reentries;
        mc_world_saved_data *slot = observed;
        mc_world_saved_data_source_state state;
        mc_world_storage_error detail;
        int32_t untouched = 13579;
        if (mc_world_saved_data_retire(&slot, reentry) != MC_WORLD_STORAGE_BUSY ||
            slot != observed ||
            mc_world_saved_data_source_construct(observed, NULL, reentry) !=
                MC_WORLD_STORAGE_BUSY ||
            mc_world_saved_data_source_inspect(observed, &state) ||
            mc_world_saved_data_save(observed, &detail) != MC_WORLD_STORAGE_BUSY ||
            mc_world_saved_data_next_id(observed, NULL, &untouched, &detail) !=
                MC_WORLD_STORAGE_BUSY ||
            untouched != 13579)
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
    size_t count = strlen(ascii);
    uint16_t *units = malloc((count == 0 ? 1 : count) * sizeof(*units));
    if (units == NULL)
        return NULL;
    for (size_t i = 0; i < count; ++i) {
        if ((unsigned char)ascii[i] >= 128) {
            free(units);
            return NULL;
        }
        units[i] = (unsigned char)ascii[i];
    }
    mc_world_info_name *name = NULL;
    mc_world_info_status status =
        mc_world_info_name_copy((mc_nbt_string){units, count}, 1048576, &name);
    free(units);
    return status == MC_WORLD_INFO_OK ? name : NULL;
}

static int child(mc_java_files *files, mc_java_file *parent, const char *ascii,
                 mc_java_file **out) {
    mc_world_info_name *name = text(ascii);
    CHECK(name != NULL);
    CHECK(mc_java_file_allocate(files, out, &error->file) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_construct_child(*out, parent, name, &error->file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(name);
    return 0;
}

static int make_store(mc_world_store_source_classes *classes, mc_java_file *root,
                      const char *folder, mc_world_store **out) {
    mc_world_info_name *name = text(folder);
    CHECK(name != NULL);
    CHECK(mc_world_store_source_allocate(classes, out, handler) == MC_WORLD_SOURCE_OK);
    CHECK(mc_world_store_source_construct(*out, root, name, false, handler) == MC_WORLD_SOURCE_OK);
    mc_world_info_name_release(name);
    CHECK(mc_world_store_root(*out) == NULL);
    return 0;
}

static int fixture_write(const char *root, const char *folder, const uint8_t *bytes,
                         size_t length) {
    char path[4096];
    int n = snprintf(path, sizeof(path), "%s/%s/data/idcounts.dat", root, folder);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    FILE *file = NULL;
#ifdef _MSC_VER
    CHECK(fopen_s(&file, path, "wbx") == 0 && file != NULL);
#else
    file = fopen(path, "wbx");
    CHECK(file != NULL);
#endif
    CHECK(fwrite(bytes, 1, length, file) == length && fclose(file) == 0);
    return 0;
}

static int fixture_unchanged(const char *root, const char *folder, const uint8_t *bytes,
                             size_t length) {
    char path[4096];
    int n = snprintf(path, sizeof(path), "%s/%s/data/idcounts.dat", root, folder);
    CHECK(n > 0 && (size_t)n < sizeof(path) && length < 1024);
    FILE *file = NULL;
#ifdef _MSC_VER
    CHECK(fopen_s(&file, path, "rb") == 0 && file != NULL);
#else
    file = fopen(path, "rb");
    CHECK(file != NULL);
#endif
    uint8_t actual[1024];
    CHECK(fread(actual, 1, sizeof(actual), file) == length && feof(file) && !ferror(file));
    CHECK(fclose(file) == 0 && memcmp(actual, bytes, length) == 0);
    return 0;
}

static int null_stores(void) {
    for (int side = MC_WORLD_INFO_CLIENT; side <= MC_WORLD_INFO_SERVER; ++side) {
        mc_world_saved_data *owner = NULL;
        CHECK(mc_world_saved_data_source_allocate((mc_world_info_side)side, NULL, &owner, error) ==
              MC_WORLD_STORAGE_OK);
        mc_world_saved_data_source_state state;
        CHECK(mc_world_saved_data_source_inspect(owner, &state) &&
              state.phase == MC_WORLD_SAVED_DATA_SOURCE_RAW && state.source_a == NULL &&
              state.source_b == NULL && state.source_c == NULL && state.source_d == NULL &&
              !state.a_stored && !state.constructor_returned);
        mc_world_storage_error detail;
        int32_t out = 321;
        CHECK(mc_world_saved_data_next_id(owner, NULL, &out, &detail) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              out == 321);
        CHECK(mc_world_saved_data_source_construct(owner, NULL, error) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_saved_data_source_inspect(owner, &state) &&
              state.side == (mc_world_info_side)side &&
              state.phase == MC_WORLD_SAVED_DATA_SOURCE_READY && state.b_stored && state.c_stored &&
              state.d_stored && state.a_stored && state.source_a == NULL && state.map_cleared &&
              state.last_store_pc == 39 && state.last_body_pc == 16 && state.constructor_returned &&
              state.load_returned && state.reader == NULL && state.file == NULL);
        CHECK(state.source_b != state.source_c && state.source_b != state.source_d &&
              state.source_c != state.source_d);
        CHECK(mc_world_saved_data_save(owner, &detail) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_saved_data_next_id(owner, NULL, &out, &detail) == MC_WORLD_STORAGE_OK &&
              out == 0);
        CHECK(mc_world_saved_data_next_id(owner, NULL, &out, &detail) == MC_WORLD_STORAGE_OK &&
              out == 1);
        /* Void legacy free cannot silently discard a source owner. */
        mc_world_saved_data_free(owner);
        CHECK(mc_world_saved_data_source_inspect(owner, &state) && state.constructor_returned);
        CHECK(mc_world_saved_data_source_construct(owner, NULL, error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE);
        CHECK(mc_world_saved_data_retire(&owner, error) == MC_WORLD_STORAGE_OK && owner == NULL);
        CHECK(mc_world_saved_data_retire(&owner, error) == MC_WORLD_STORAGE_OK);
    }
    mc_world_saved_data_limits limits = mc_world_saved_data_default_limits();
    limits.max_allocation_bytes = 0;
    mc_world_saved_data *out = NULL;
    CHECK(mc_world_saved_data_source_allocate(MC_WORLD_INFO_CLIENT, &limits, &out, error) ==
              MC_WORLD_STORAGE_LIMIT &&
          out == NULL);
    CHECK(mc_world_saved_data_source_allocate((mc_world_info_side)99, NULL, &out, error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          out == NULL);
    CHECK(mc_world_saved_data_source_allocate(MC_WORLD_INFO_CLIENT, NULL, &out, error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_retire(&out, error) == MC_WORLD_STORAGE_OK && out == NULL);
    return 0;
}

static int missing(mc_world_store_source_classes *classes, mc_java_file *root) {
    mc_world_store *store = NULL;
    CHECK(make_store(classes, root, "missing", &store) == 0);
    mc_world_saved_data *owner = NULL;
    mc_world_storage_error detail;
    CHECK(mc_world_saved_data_create(store, NULL, &owner, &detail) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          owner == NULL);
    CHECK(mc_world_saved_data_source_allocate(MC_WORLD_INFO_CLIENT, NULL, &owner, error) ==
          MC_WORLD_STORAGE_OK);
    mc_world_source_state source;
    CHECK(mc_world_store_source_inspect(store, &source) && source.holds == 1);
    CHECK(mc_world_saved_data_source_construct(owner, store, error) == MC_WORLD_STORAGE_OK);
    mc_world_saved_data_source_state state;
    CHECK(mc_world_saved_data_source_inspect(owner, &state) && state.constructor_returned &&
          state.source_a == store && state.file != NULL && state.file_stored && !state.exists &&
          state.reader == NULL && state.stream == NULL && state.last_body_pc == 155);
    mc_java_file_state file;
    CHECK(mc_java_file_read_state(state.file, &file, &error->file) == MC_JAVA_FILE_OK &&
          file.exists_calls == 1);
    CHECK(mc_world_store_source_inspect(store, &source) && source.holds == 2);
    CHECK(mc_world_store_source_retire(&store, handler) == MC_WORLD_SOURCE_BUSY &&
          store == state.source_a);
    CHECK(mc_world_saved_data_count(owner) == 0 &&
          mc_world_saved_data_save(owner, &detail) == MC_WORLD_STORAGE_OK);
    int32_t id = 654;
    CHECK(mc_world_saved_data_next_id(owner, NULL, &id, &detail) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          id == 654 && mc_world_saved_data_count(owner) == 0);
    mc_saved_data_value *value = NULL;
    CHECK(mc_world_saved_data_get(owner, NULL, NULL, &value, &detail) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          value == NULL);
    mc_saved_data_error value_error;
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(MC_WORLD_INFO_CLIENT), NULL, 1048576,
                                   &value, &value_error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_put(owner, NULL, value, &detail) == MC_WORLD_STORAGE_OK);
    mc_saved_data_value *cached = NULL;
    CHECK(mc_world_saved_data_get(owner, NULL, NULL, &cached, &detail) == MC_WORLD_STORAGE_OK &&
          cached == value);
    mc_saved_data_value_release(cached);
    CHECK(mc_world_saved_data_save(owner, &detail) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_value_mark_dirty(value) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_save(owner, &detail) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          mc_saved_data_value_dirty(value));
    CHECK(mc_saved_data_value_dirty_set(value, false) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_save(owner, &detail) == MC_WORLD_STORAGE_OK);
    mc_saved_data_value_release(value);
    bool exists = true;
    CHECK(mc_java_file_exists(state.file, &exists, &error->file) == MC_JAVA_FILE_OK && !exists);
    CHECK(mc_world_store_root(store) == NULL);
    CHECK(mc_world_saved_data_retire(&owner, error) == MC_WORLD_STORAGE_OK && owner == NULL);
    CHECK(mc_world_store_source_inspect(store, &source) && source.holds == 1);
    CHECK(mc_world_store_source_retire(&store, handler) == MC_WORLD_SOURCE_OK && store == NULL);
    return 0;
}

static int server_profile(mc_world_store_source_classes *classes, mc_java_file *root) {
    mc_world_store *store = NULL;
    CHECK(make_store(classes, root, "server-profile", &store) == 0);
    mc_world_saved_data *owner = NULL;
    CHECK(mc_world_saved_data_source_allocate(MC_WORLD_INFO_SERVER, NULL, &owner, error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_source_construct(owner, store, error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          strcmp(error->site.class_name, "es") == 0 && error->site.pc == 23 &&
          error->native_policy && !error->source_exception);
    mc_world_saved_data_source_state state;
    CHECK(mc_world_saved_data_source_inspect(owner, &state) && state.side == MC_WORLD_INFO_SERVER &&
          state.a_stored && state.source_a == store && state.last_store_pc == 39 &&
          !state.constructor_returned && state.file == NULL);
    CHECK(mc_world_saved_data_retire(&owner, error) == MC_WORLD_STORAGE_OK && owner == NULL);
    CHECK(mc_world_store_source_retire(&store, handler) == MC_WORLD_SOURCE_OK && store == NULL);
    return 0;
}

static bool same_name(mc_nbt_string name, const uint16_t *units, size_t count) {
    return name.length == count &&
           (count == 0 || memcmp(name.units, units, count * sizeof(*units)) == 0);
}

static int valid(mc_world_store_source_classes *classes, mc_java_file *root, const char *path) {
    mc_world_store *store = NULL;
    CHECK(make_store(classes, root, "valid", &store) == 0);
    CHECK(fixture_write(path, "valid", fixture, sizeof(fixture)) == 0);
    mc_world_saved_data *owner = NULL;
    CHECK(mc_world_saved_data_source_allocate(MC_WORLD_INFO_CLIENT, NULL, &owner, error) ==
          MC_WORLD_STORAGE_OK);
#ifdef MC_SAVED_SOURCE_CLOSE_CONTROL
    observed = owner;
#endif
    CHECK(mc_world_saved_data_source_construct(owner, store, error) == MC_WORLD_STORAGE_OK);
#ifdef MC_SAVED_SOURCE_CLOSE_CONTROL
    observed = NULL;
    CHECK(reentry_ok && reentries != 0);
#endif
    mc_world_saved_data_source_state state;
    CHECK(mc_world_saved_data_source_inspect(owner, &state) && state.source_a == store &&
          state.parsed && state.close_returned && state.close_pc == 62 &&
          state.last_body_pc == 155 && state.constructor_returned && state.imported_shorts == 4 &&
          state.document != NULL);
    mc_nbt_stream_state input;
    CHECK(mc_nbt_stream_state_read(state.reader, &input, &error->input) == MC_NBT_OK &&
          input.source_in == state.stream && input.close_calls == 1 &&
          input.position == sizeof(fixture) - 2 && input.candidate == state.document);
    mc_java_file_stream_state stream;
    CHECK(mc_java_file_stream_read_state(state.stream, &stream, &error->file) == MC_JAVA_FILE_OK &&
          !stream.native_open && stream.source_close_reached && stream.source_close_succeeded &&
          stream.native_bytes_read > 0);
    CHECK(mc_world_saved_data_count(owner) == 4);
    static const uint16_t unicode[] = {0, 0xd800, 'n', 0xdc00, 0xffff};
    bool a = false, d = false, empty = false, wide = false;
    for (size_t i = 0; i < mc_world_saved_data_count(owner); ++i) {
        const mc_world_saved_id *id = mc_world_saved_data_id_at(owner, i);
        CHECK(id != NULL && !id->null_name);
        if (same_name(id->name, (const uint16_t[]){'a'}, 1))
            a = id->value == 32767;
        else if (same_name(id->name, (const uint16_t[]){'d'}, 1))
            d = id->value == -2;
        else if (id->name.length == 0)
            empty = id->value == -1;
        else if (same_name(id->name, unicode, sizeof(unicode) / sizeof(unicode[0])))
            wide = id->value == -32768;
        else
            CHECK(false);
    }
    CHECK(a && d && empty && wide);
    mc_world_storage_error detail;
    CHECK(mc_world_saved_data_save(owner, &detail) == MC_WORLD_STORAGE_OK);
    CHECK(fixture_unchanged(path, "valid", fixture, sizeof(fixture)) == 0);
    /* Release the original local: HC's a39 is now the remaining Store hold. */
    mc_world_store *retained = store;
    CHECK(mc_world_store_source_release(&store, handler) == MC_WORLD_SOURCE_OK && store == NULL);
    mc_world_source_state store_state;
    CHECK(mc_world_store_source_inspect(retained, &store_state) && store_state.holds == 1);
    CHECK(mc_world_saved_data_retire(&owner, error) == MC_WORLD_STORAGE_OK && owner == NULL);
    return 0;
}

static int input_failure(mc_world_store_source_classes *classes, mc_java_file *root,
                         const char *path, const char *folder, const uint8_t *bytes, size_t length,
                         bool native_limit) {
    mc_world_store *store = NULL;
    CHECK(make_store(classes, root, folder, &store) == 0);
    CHECK(fixture_write(path, folder, bytes, length) == 0);
    mc_world_saved_data_limits limits = mc_world_saved_data_default_limits();
    if (native_limit)
        limits.nbt.max_input_bytes = 2;
    mc_world_saved_data *owner = NULL;
    CHECK(mc_world_saved_data_source_allocate(MC_WORLD_INFO_CLIENT, &limits, &owner, error) ==
          MC_WORLD_STORAGE_OK);
    mc_world_storage_status expected =
        native_limit ? MC_WORLD_STORAGE_LIMIT : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    CHECK(mc_world_saved_data_source_construct(owner, store, error) == expected);
    CHECK(error->site.pc == (native_limit ? 57 : 152) && error->source_exception == !native_limit &&
          error->native_policy == native_limit && error->caught_exception == !native_limit &&
          error->diagnostic_frontier == !native_limit);
    if (!native_limit)
        CHECK(error->cause_site.pc == 57);
    mc_world_saved_data_source_state state;
    CHECK(mc_world_saved_data_source_inspect(owner, &state) &&
          state.phase == MC_WORLD_SAVED_DATA_SOURCE_FAILED && !state.constructor_returned &&
          !state.parsed && !state.close_returned && state.close_pc == -1 &&
          state.input_constructed && state.document == NULL &&
          mc_world_saved_data_count(owner) == 0);
    mc_nbt_stream_state reader;
    CHECK(mc_nbt_stream_state_read(state.reader, &reader, &error->input) == MC_NBT_OK &&
          reader.close_calls == 0);
    mc_java_file_stream_state stream;
    CHECK(mc_java_file_stream_read_state(state.stream, &stream, &error->file) == MC_JAVA_FILE_OK &&
          stream.native_open && !stream.source_close_reached);
    size_t calls = stream.read_calls;
    CHECK(mc_world_saved_data_source_construct(owner, store, error) == expected);
    CHECK(mc_java_file_stream_read_state(state.stream, &stream, &error->file) == MC_JAVA_FILE_OK &&
          stream.read_calls == calls);
    CHECK(fixture_unchanged(path, folder, bytes, length) == 0);
    /* An independent FIS occurrence survives containing graph cleanup. Native
     * retirement must not synthesize the unreached source close62. */
    mc_java_file_stream *external = state.stream;
    CHECK(mc_java_file_stream_retain(external, &error->file) == MC_JAVA_FILE_OK);
    CHECK(mc_world_saved_data_retire(&owner, error) == MC_WORLD_STORAGE_OK && owner == NULL);
    CHECK(mc_java_file_stream_read_state(external, &stream, &error->file) == MC_JAVA_FILE_OK &&
          stream.holds == 1 && stream.native_open && !stream.source_close_reached);
    CHECK(mc_java_file_stream_release(&external, &error->file) == MC_JAVA_FILE_OK &&
          external == NULL);
    CHECK(mc_world_store_source_retire(&store, handler) == MC_WORLD_SOURCE_OK && store == NULL);
    return 0;
}

static int partial_map(mc_world_store_source_classes *classes, mc_java_file *root,
                       const char *path) {
    mc_world_store *store = NULL;
    CHECK(make_store(classes, root, "partial", &store) == 0);
    CHECK(fixture_write(path, "partial", fixture, sizeof(fixture)) == 0);
    mc_world_saved_data_limits limits = mc_world_saved_data_default_limits();
    limits.max_ids = 1;
    mc_world_saved_data *owner = NULL;
    CHECK(mc_world_saved_data_source_allocate(MC_WORLD_INFO_CLIENT, &limits, &owner, error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_source_construct(owner, store, error) == MC_WORLD_STORAGE_LIMIT &&
          error->site.pc == 138 && error->native_policy && !error->source_exception &&
          !error->caught_exception);
    mc_world_saved_data_source_state state;
    CHECK(mc_world_saved_data_source_inspect(owner, &state) && state.parsed &&
          state.close_returned && !state.constructor_returned && state.imported_shorts == 1 &&
          mc_world_saved_data_count(owner) == 1);
    CHECK(mc_world_saved_data_id_at(owner, 0)->value == 32767);
    mc_world_storage_error detail;
    CHECK(mc_world_saved_data_save(owner, &detail) == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    CHECK(mc_world_saved_data_retire(&owner, error) == MC_WORLD_STORAGE_OK && owner == NULL);
    CHECK(mc_world_store_source_retire(&store, handler) == MC_WORLD_SOURCE_OK && store == NULL);
    CHECK(fixture_unchanged(path, "partial", fixture, sizeof(fixture)) == 0);
    return 0;
}

static int directory_input(mc_world_store_source_classes *classes, mc_java_files *files,
                           mc_java_file *root) {
    mc_world_store *store = NULL;
    CHECK(make_store(classes, root, "directory", &store) == 0);
    mc_world_source_state store_state;
    CHECK(mc_world_store_source_inspect(store, &store_state));
    mc_java_file *directory = NULL;
    CHECK(child(files, store_state.source_d, "idcounts.dat", &directory) == 0);
    bool made = false;
    CHECK(mc_java_file_mkdir(directory, &made, &error->file) == MC_JAVA_FILE_OK && made);
    CHECK(mc_java_file_release(&directory, &error->file) == MC_JAVA_FILE_OK);
    mc_world_saved_data *owner = NULL;
    CHECK(mc_world_saved_data_source_allocate(MC_WORLD_INFO_CLIENT, NULL, &owner, error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_source_construct(owner, store, error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error->site.pc == 152 && error->cause_site.pc == 49 && error->caught_exception);
    mc_world_saved_data_source_state state;
    CHECK(mc_world_saved_data_source_inspect(owner, &state) && state.reader != NULL &&
          state.stream != NULL && !state.input_constructed && !state.parsed &&
          !state.close_returned);
    mc_nbt_stream_state reader;
    CHECK(mc_nbt_stream_state_read(state.reader, &reader, &error->input) == MC_NBT_OK &&
          reader.phase == MC_NBT_STREAM_RAW && reader.source_in == NULL && reader.close_calls == 0);
    CHECK(mc_world_saved_data_retire(&owner, error) == MC_WORLD_STORAGE_OK && owner == NULL);
    CHECK(mc_world_store_source_retire(&store, handler) == MC_WORLD_SOURCE_OK && store == NULL);
    return 0;
}

#ifdef MC_SAVED_SOURCE_CLOSE_CONTROL
static int close_failures(mc_world_store_source_classes *classes, mc_java_file *root,
                          const char *path) {
    mc_world_store *store = NULL;
    mc_world_saved_data *owner = NULL;
    CHECK(make_store(classes, root, "source-close", &store) == 0);
    CHECK(fixture_write(path, "source-close", fixture, sizeof(fixture)) == 0);
    CHECK(mc_world_saved_data_source_allocate(MC_WORLD_INFO_CLIENT, NULL, &owner, error) ==
          MC_WORLD_STORAGE_OK);
    size_t before = closes;
    close_fault = true;
    observed = owner;
    CHECK(mc_world_saved_data_source_construct(owner, store, error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error->site.pc == 152 && error->cause_site.pc == 62 && error->source_exception);
    observed = NULL;
    close_fault = false;
    mc_world_saved_data_source_state state;
    CHECK(mc_world_saved_data_source_inspect(owner, &state) && state.parsed &&
          !state.close_returned && !state.constructor_returned && state.close_pc == 62 &&
          state.imported_shorts == 0 && mc_world_saved_data_count(owner) == 0 &&
          closes == before + 1);
    CHECK(mc_world_saved_data_retire(&owner, error) == MC_WORLD_STORAGE_OK && owner == NULL &&
          closes == before + 1);
    CHECK(mc_world_store_source_retire(&store, handler) == MC_WORLD_SOURCE_OK && store == NULL);

    CHECK(make_store(classes, root, "retire-close", &store) == 0);
    CHECK(fixture_write(path, "retire-close", malformed, sizeof(malformed)) == 0);
    CHECK(mc_world_saved_data_source_allocate(MC_WORLD_INFO_CLIENT, NULL, &owner, error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_source_construct(owner, store, error) ==
          MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    mc_world_saved_data *same = owner;
    before = closes;
    observed = owner;
    close_fault = true;
    CHECK(mc_world_saved_data_retire(&owner, error) == MC_WORLD_STORAGE_IO_ERROR && owner == same &&
          error->native_policy && !error->source_exception && closes == before + 1);
    observed = NULL;
    close_fault = false;
    CHECK(mc_world_saved_data_source_inspect(owner, &state) &&
          state.phase == MC_WORLD_SAVED_DATA_SOURCE_RETIRING && state.reader == NULL &&
          state.document == NULL && state.stream != NULL && state.file != NULL &&
          state.source_a == store && state.failure->site.pc == 152 &&
          state.failure->cause_site.pc == 57 &&
          state.cleanup->status == MC_WORLD_STORAGE_IO_ERROR && state.cleanup->native_policy);
    mc_java_file_stream_state stream;
    CHECK(mc_java_file_stream_read_state(state.stream, &stream, &error->file) == MC_JAVA_FILE_OK &&
          !stream.native_open && !stream.source_close_reached);
    mc_world_source_state store_state;
    CHECK(mc_world_store_source_inspect(store, &store_state) && store_state.holds == 2);
    CHECK(mc_world_saved_data_retire(&owner, error) == MC_WORLD_STORAGE_OK && owner == NULL &&
          closes == before + 1);
    CHECK(mc_world_store_source_retire(&store, handler) == MC_WORLD_SOURCE_OK && store == NULL &&
          reentry_ok);
    return 0;
}
#endif

int main(int argc, char **argv) {
    CHECK(argc == 2);
    error = calloc(1, sizeof(*error));
    reentry = calloc(1, sizeof(*reentry));
    handler = calloc(1, sizeof(*handler));
    CHECK(error != NULL && reentry != NULL && handler != NULL);
    CHECK(null_stores() == 0);
    mc_java_files *files = NULL;
    mc_java_logging *logging = NULL;
    mc_world_store_source_classes *classes = NULL;
    CHECK(mc_java_files_create(NULL, &files, &error->file) == MC_JAVA_FILE_OK);
    CHECK(mc_java_logging_create(NULL, &logging, &handler->logging) == MC_JAVA_LOGGING_OK);
    CHECK(mc_world_store_source_classes_create(files, logging, NULL, &classes, handler) ==
          MC_WORLD_SOURCE_OK);
    mc_world_info_name *base_name = text(argv[1]);
    CHECK(base_name != NULL);
    mc_java_file *base = NULL;
    CHECK(mc_java_file_allocate(files, &base, &error->file) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_construct_string(base, base_name, &error->file) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(base_name);
    bool made = false;
    CHECK(mc_java_file_mkdirs(base, &made, &error->file) == MC_JAVA_FILE_OK);
    mc_java_file *root = NULL;
    char suffix[40];
    for (unsigned i = 0; i < 1024; ++i) {
        int n = snprintf(suffix, sizeof(suffix), "owned-%u", i);
        CHECK(n > 0 && (size_t)n < sizeof(suffix));
        CHECK(child(files, base, suffix, &root) == 0);
        CHECK(mc_java_file_mkdir(root, &made, &error->file) == MC_JAVA_FILE_OK);
        if (made)
            break;
        CHECK(mc_java_file_release(&root, &error->file) == MC_JAVA_FILE_OK);
    }
    CHECK(root != NULL && made);
    char path[4096];
    int n = snprintf(path, sizeof(path), "%s/%s", argv[1], suffix);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    CHECK(missing(classes, root) == 0);
    CHECK(server_profile(classes, root) == 0);
    CHECK(valid(classes, root, path) == 0);
    CHECK(input_failure(classes, root, path, "malformed", malformed, sizeof(malformed), false) ==
          0);
    static const uint8_t wrong_root[] = {1, 0, 0, 7};
    CHECK(input_failure(classes, root, path, "wrong-root", wrong_root, sizeof(wrong_root), false) ==
          0);
    static const uint8_t compressed_header[] = {0x1f, 0x8b, 8, 0, 0};
    CHECK(input_failure(classes, root, path, "compressed", compressed_header,
                        sizeof(compressed_header), false) == 0);
    CHECK(input_failure(classes, root, path, "input-limit", fixture, sizeof(fixture), true) == 0);
    CHECK(partial_map(classes, root, path) == 0);
    CHECK(directory_input(classes, files, root) == 0);
#ifdef MC_SAVED_SOURCE_CLOSE_CONTROL
    CHECK(close_failures(classes, root, path) == 0 && reentry_ok);
#endif
    CHECK(mc_java_file_release(&root, &error->file) == MC_JAVA_FILE_OK && root == NULL);
    CHECK(mc_java_file_release(&base, &error->file) == MC_JAVA_FILE_OK && base == NULL);
    CHECK(mc_world_store_source_classes_retire(&classes, handler) == MC_WORLD_SOURCE_OK &&
          classes == NULL);
    CHECK(mc_java_logging_release(&logging, &handler->logging) == MC_JAVA_LOGGING_OK &&
          logging == NULL);
    CHECK(mc_java_files_release(&files, &error->file) == MC_JAVA_FILE_OK && files == NULL);
    free(handler);
    free(reentry);
    free(error);
    printf("Source SavedData authored contracts: %u checks\n", checks);
    return 0;
}
