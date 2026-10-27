#include "../../../src/client/saves/internal.h"
#include "../../../src/world/storage/java_file_internal.h"
#include "mc/client/saves/save_format.h"
#include "mc/storage/compression/compression.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#ifdef _WIN32
#include <wchar.h>
#endif

static size_t checks;
#define REQUIRE(test)                                                                              \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(test)) {                                                                             \
            fprintf(stderr, "SaveFormat check:%d: %s\n", __LINE__, #test);                         \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static mc_world_info_name *name(const char *ascii) {
    size_t n = strlen(ascii);
    uint16_t *u = malloc((n == 0 ? 1u : n) * sizeof(*u));
    REQUIRE(u != NULL);
    for (size_t i = 0; i < n; ++i) {
        REQUIRE((unsigned char)ascii[i] < 128u);
        u[i] = (uint16_t)(unsigned char)ascii[i];
    }
    mc_world_info_name *s = NULL;
    REQUIRE(mc_world_info_name_copy((mc_nbt_string){u, n}, SIZE_MAX, &s) == MC_WORLD_INFO_OK);
    free(u);
    return s;
}

static mc_java_file *file(mc_java_files *files, mc_java_file *parent, mc_world_info_name *s,
                          bool child) {
    mc_java_file *f = NULL;
    mc_java_file_error e;
    REQUIRE(mc_java_file_allocate(files, &f, &e) == MC_JAVA_FILE_OK);
    REQUIRE((child ? mc_java_file_construct_child(f, parent, s, &e)
                   : mc_java_file_construct_string(f, s, &e)) == MC_JAVA_FILE_OK);
    return f;
}

static void equal_text(mc_world_info_name *a, mc_world_info_name *b) {
    mc_nbt_string x = mc_world_info_name_text(a), y = mc_world_info_name_text(b);
    REQUIRE(x.length == y.length);
    REQUIRE(x.length == 0 || memcmp(x.units, y.units, x.length * sizeof(*x.units)) == 0);
}

static void models(void) {
    mc_java_files *files = NULL;
    mc_java_file_error fe;
    REQUIRE(mc_java_files_create(NULL, &files, &fe) == MC_JAVA_FILE_OK);
    mc_world_info_name *plain = name("plain"), *empty = name("");
    mc_java_file *a = file(files, NULL, plain, false), *b = file(files, NULL, plain, true);
    REQUIRE(a != b);
    REQUIRE(mc_java_file_path(a) == plain);
    REQUIRE(mc_java_file_path(b) == plain);
    mc_world_info_name *got = NULL;
    REQUIRE(mc_java_file_get_name(a, &got, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(got == plain);
    mc_world_info_name_release(got);
    mc_java_file *alias = a;
    REQUIRE(mc_java_file_retain(a, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_release(&a, &fe) == MC_JAVA_FILE_OK && a == NULL);
    mc_java_file_state state;
    REQUIRE(mc_java_file_read_state(alias, &state, &fe) == MC_JAVA_FILE_OK && state.holds == 1);
    mc_java_file *e = file(files, NULL, empty, false);
    got = NULL;
    REQUIRE(mc_java_file_get_name(e, &got, &fe) == MC_JAVA_FILE_OK && got == empty);
    mc_world_info_name_release(got);
    mc_java_file *child = file(files, e, plain, true);
    REQUIRE(child != e && mc_java_file_path(child) != plain);
    mc_nbt_string joined = mc_world_info_name_text(mc_java_file_path(child));
    REQUIRE(joined.length == 6);
#ifdef _WIN32
    REQUIRE(joined.units[0] == '\\');
#else
    REQUIRE(joined.units[0] == '/');
#endif
    mc_java_file *bad = NULL;
    REQUIRE(mc_java_file_allocate(files, &bad, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_construct_string(bad, NULL, &fe) == MC_JAVA_FILE_SOURCE_NULL_STRING);
    REQUIRE(mc_java_file_read_state(bad, &state, &fe) == MC_JAVA_FILE_OK &&
            state.phase == MC_JAVA_FILE_FAILED && state.path == NULL);
    REQUIRE(mc_java_file_construct_string(bad, plain, &fe) == MC_JAVA_FILE_STATE_ERROR);
    uint16_t nul_units[] = {'a', 0, 'b'}, lone_units[] = {0xd800u};
    mc_world_info_name *nul = NULL, *lone = NULL;
    REQUIRE(mc_world_info_name_copy((mc_nbt_string){nul_units, 3}, SIZE_MAX, &nul) ==
            MC_WORLD_INFO_OK);
    REQUIRE(mc_world_info_name_copy((mc_nbt_string){lone_units, 1}, SIZE_MAX, &lone) ==
            MC_WORLD_INFO_OK);
    mc_java_file *n = file(files, NULL, nul, false), *l = file(files, NULL, lone, false);
    bool yes = true;
    REQUIRE(mc_java_file_exists(n, &yes, &fe) == MC_JAVA_FILE_OK && !yes);
    yes = true;
    REQUIRE(mc_java_file_mkdirs(n, &yes, &fe) == MC_JAVA_FILE_OK && !yes);
    mc_java_file_array *array = NULL;
    REQUIRE(mc_java_file_list(n, &array, &fe) == MC_JAVA_FILE_OK && array == NULL);
    REQUIRE(mc_java_file_exists(l, &yes, &fe) == MC_JAVA_FILE_UNSUPPORTED_PROVIDER);
    REQUIRE(mc_java_file_path(l) == lone);
    equal_text(mc_java_file_path(n), nul);
    mc_client_save_formats *fc = NULL;
    mc_client_save_format *fm = NULL, *failed_format = NULL;
    mc_client_save_error ce;
    REQUIRE(mc_client_save_formats_create(files, NULL, NULL, &fc, &ce) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_allocate(fc, &fm, &ce) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_construct(fm, n, &ce) == MC_CLIENT_SAVE_OK);
    mc_client_save_format_state cstate;
    REQUIRE(mc_client_save_format_read_state(fm, &cstate, &ce) == MC_CLIENT_SAVE_OK &&
            cstate.source_a == n && cstate.last_store_pc == 18);
    REQUIRE(mc_client_save_format_allocate(fc, &failed_format, &ce) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_construct(failed_format, NULL, &ce) ==
            MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE);
    REQUIRE(ce.frame_count == 2 && ce.frames[0].pc == 2 && ce.frames[1].pc == 5);
    REQUIRE(mc_client_save_format_read_state(failed_format, &cstate, &ce) == MC_CLIENT_SAVE_OK &&
            cstate.source_a == NULL && cstate.last_store_pc == 0);
    REQUIRE(mc_client_save_format_release(&failed_format, &ce) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_release(&fm, &ce) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_formats_release(&fc, &ce) == MC_CLIENT_SAVE_OK);
    mc_world_info *raw_info = NULL;
    REQUIRE(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &raw_info) == MC_WORLD_INFO_OK);
    int32_t version = 123;
    int64_t number = 456;
    mc_world_info_name *missing = plain;
    REQUIRE(mc_world_info_version_get(raw_info, &version) == MC_WORLD_INFO_OK && version == 0);
    REQUIRE(mc_world_info_name_get(raw_info, &missing) == MC_WORLD_INFO_OK && missing == NULL);
    REQUIRE(mc_world_info_last_played_get(raw_info, &number) == MC_WORLD_INFO_OK && number == 0);
    REQUIRE(mc_world_info_size_get(NULL, &number) == MC_WORLD_INFO_INVALID_ARGUMENT && number == 0);
    mc_world_info_release(raw_info);
    mc_client_save_selection *one = NULL, *two = NULL;
    mc_client_save_error se;
    REQUIRE(mc_client_save_selection_allocate(SIZE_MAX, &one, &se) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_selection_allocate(SIZE_MAX, &two, &se) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_selection_construct(one, NULL, plain, INT64_MIN, INT64_MAX, true, &se) ==
            MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_selection_construct(two, NULL, NULL, INT64_MAX, INT64_MIN, false, &se) ==
            MC_CLIENT_SAVE_OK);
    int32_t order = 0;
    REQUIRE(mc_client_save_selection_compare(one, two, &order, &se) == MC_CLIENT_SAVE_OK &&
            order == 1);
    REQUIRE(mc_client_save_selection_compare(two, one, &order, &se) == MC_CLIENT_SAVE_OK &&
            order == -1);
    REQUIRE(mc_client_save_selection_compare(one, one, &order, &se) ==
                MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE &&
            se.frames[0].pc == 36);
    mc_client_save_selection_fields fields;
    REQUIRE(mc_client_save_selection_read(one, &fields, &se) == MC_CLIENT_SAVE_OK &&
            fields.filename == NULL && fields.display_name == plain && fields.last_store_pc == 28);
    mc_client_save_selection *one_alias = one;
    REQUIRE(mc_client_save_selection_retain(one, &se) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_selection_release(&one, &se) == MC_CLIENT_SAVE_OK && one == NULL);
    REQUIRE(mc_client_save_selection_release(&one_alias, &se) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_selection_release(&two, &se) == MC_CLIENT_SAVE_OK);
    uint16_t high_units[] = {0xffffu, 0}, low_units[] = {0xd800u, 0};
    mc_world_info_name *high = NULL, *low = NULL;
    REQUIRE(mc_world_info_name_copy((mc_nbt_string){high_units, 2}, SIZE_MAX, &high) ==
            MC_WORLD_INFO_OK);
    REQUIRE(mc_world_info_name_copy((mc_nbt_string){low_units, 2}, SIZE_MAX, &low) ==
            MC_WORLD_INFO_OK);
    REQUIRE(mc_client_save_selection_allocate(SIZE_MAX, &one, &se) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_selection_allocate(SIZE_MAX, &two, &se) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_selection_construct(one, high, NULL, 7, 0, false, &se) ==
            MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_selection_construct(two, low, NULL, 7, 0, false, &se) ==
            MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_selection_compare(one, two, &order, &se) == MC_CLIENT_SAVE_OK &&
            order == 0x27ff);
    REQUIRE(mc_client_save_selection_release(&one, &se) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_selection_release(&two, &se) == MC_CLIENT_SAVE_OK);
    mc_world_info_name_release(high);
    mc_world_info_name_release(low);
    mc_math_classes *math = NULL;
    mc_math_class_error me;
    REQUIRE(mc_math_classes_create(SIZE_MAX, &math, &me) == MC_MATH_CLASS_OK);
    mc_math_class_state ms;
    REQUIRE(mc_math_classes_state(math, &ms, &me) == MC_MATH_CLASS_OK &&
            ms.phase == MC_MATH_CLASS_COLD && ms.source_a == NULL);
    unsigned original = _mm_getcsr();
    REQUIRE((original & 0xffc0u) == 0x1f80u);
    _mm_setcsr(original | 0x2000u);
    mc_math_class_status failed = mc_math_classes_ensure(math, &me);
    _mm_setcsr(original);
    REQUIRE(failed == MC_MATH_CLASS_UNSUPPORTED_FP && me.source_pc == 20);
    REQUIRE(mc_math_classes_state(math, &ms, &me) == MC_MATH_CLASS_OK &&
            ms.phase == MC_MATH_CLASS_FAILED && ms.last_store_pc == 4 && ms.source_a != NULL);
    const unsigned char *zero = (const unsigned char *)ms.source_a;
    for (size_t i = 0; i < sizeof(*ms.source_a); ++i)
        REQUIRE(zero[i] == 0);
    REQUIRE(mc_math_classes_ensure(math, &me) == MC_MATH_CLASS_INITIALIZATION_FAILED &&
            me.initial_cause == MC_MATH_CLASS_UNSUPPORTED_FP);
    REQUIRE(mc_math_classes_release(&math, &me) == MC_MATH_CLASS_OK && math == NULL);
    REQUIRE(mc_math_classes_create(SIZE_MAX, &math, &me) == MC_MATH_CLASS_OK);
    _mm_setcsr(original | 0x3fu);
    mc_math_class_status ready = mc_math_classes_ensure(math, &me);
    _mm_setcsr(original);
    REQUIRE(ready == MC_MATH_CLASS_OK);
    REQUIRE(mc_math_classes_state(math, &ms, &me) == MC_MATH_CLASS_OK &&
            ms.phase == MC_MATH_CLASS_READY && ms.source_a == mc_math_classes_table(math));
    mc_math_classes *ma = math;
    REQUIRE(mc_math_classes_retain(math, &me) == MC_MATH_CLASS_OK);
    REQUIRE(mc_math_classes_release(&math, &me) == MC_MATH_CLASS_OK && math == NULL);
    REQUIRE(mc_math_classes_table(ma) == ms.source_a);
    REQUIRE(mc_math_classes_release(&ma, &me) == MC_MATH_CLASS_OK);
    REQUIRE(mc_math_classes_create(1024u, &math, &me) == MC_MATH_CLASS_OK);
    REQUIRE(mc_math_classes_ensure(math, &me) == MC_MATH_CLASS_LIMIT && me.source_pc == 2);
    REQUIRE(mc_math_classes_state(math, &ms, &me) == MC_MATH_CLASS_OK &&
            ms.phase == MC_MATH_CLASS_FAILED && ms.source_a == NULL && ms.last_store_pc == 0);
    REQUIRE(mc_math_classes_release(&math, &me) == MC_MATH_CLASS_OK);
    for (unsigned i = 0; i < MC_CLIENT_SAVE_METHOD_COUNT; ++i) {
        mc_client_save_method_state method =
            mc_client_save_format_method_state((mc_client_save_method)i);
        REQUIRE(method.descriptor != NULL && method.owner != NULL);
    }
    REQUIRE(mc_java_file_release(&bad, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_release(&b, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_release(&alias, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_release(&child, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_release(&e, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_release(&n, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_release(&l, &fe) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(plain);
    mc_world_info_name_release(empty);
    mc_world_info_name_release(nul);
    mc_world_info_name_release(lone);
    REQUIRE(mc_java_files_release(&files, &fe) == MC_JAVA_FILE_OK && files == NULL);
}
#ifdef MC_SAVE_FILE_CLOSE_CONTROL
static mc_client_save_format **watched;
static bool fail_close;
static size_t actual_closes;

int mc_test_save_close(int descriptor) {
    ++actual_closes;
    if (watched != NULL) {
        mc_client_save_format *before = *watched;
        mc_client_save_error e;
        mc_java_file_error fe;
        mc_java_file_stream *stream = before->state.pending_stream;
        REQUIRE(mc_client_save_format_release(watched, &e) == MC_CLIENT_SAVE_BUSY &&
                *watched == before);
        REQUIRE(mc_client_save_format_retain(before, &e) == MC_CLIENT_SAVE_BUSY);
        REQUIRE(mc_java_file_stream_release(&before->state.pending_stream, &fe) ==
                    MC_JAVA_FILE_BUSY &&
                before->state.pending_stream == stream);
        REQUIRE(mc_java_file_stream_source_close(stream, &fe) == MC_JAVA_FILE_BUSY);
    }
#ifdef _WIN32
    int rc = _close(descriptor);
#else
    int rc = close(descriptor);
#endif
    if (fail_close && rc == 0) {
        fail_close = false;
        errno = EIO;
        return -1;
    }
    return rc;
}
#endif
static void authored_bytes(mc_java_file *f, const uint8_t *bytes, size_t count) {
    mc_java_file_error e;
    mc_file_native_char *path = NULL;
    REQUIRE(mc_file_native(f, &path, &e) == MC_JAVA_FILE_OK);
#ifdef _WIN32
    FILE *out = _wfopen(path, L"wb");
#else
    FILE *out = fopen(path, "wb");
#endif
    free(path);
    REQUIRE(out != NULL);
    REQUIRE(fwrite(bytes, 1, count, out) == count);
    REQUIRE(fclose(out) == 0);
}

static void authored_file(mc_java_file *f) {
    const uint8_t bytes[] = {'a', 'u', 't', 'h', 'o', 'r', 'e', 'd'};
    authored_bytes(f, bytes, sizeof(bytes));
}

static void authored_gzip(mc_java_file *f, const uint8_t *bytes, size_t count, bool bad_crc) {
    mc_compression_buffer output = {0};
    REQUIRE(mc_compression_encode(MC_COMPRESSION_GZIP, bytes, count, NULL, &output) ==
            MC_COMPRESSION_OK);
    REQUIRE(output.length >= 18);
    if (bad_crc)
        output.data[output.length - 8] ^= 1;
    authored_bytes(f, output.data, output.length);
    mc_compression_buffer_free(&output);
}

static void metadata(mc_client_save_format *format, mc_java_file *meta,
                     mc_world_info_name *folder) {
    const uint8_t normal[] = {10,  0,   0,   10,  0,   4,   'D', 'a', 't', 'a', 8,   0, 9, 'L',
                              'e', 'v', 'e', 'l', 'N', 'a', 'm', 'e', 0,   1,   'N', 0, 0};
    mc_client_save_error e;
    mc_world_info *info = NULL;
    mc_java_file_error fe;
    mc_client_save_format_state state;
    mc_java_file_stream_state stream;
    /* Actual tq/as returns a complete root without forcing GZIP EOF/CRC. */
    authored_gzip(meta, normal, sizeof(normal), true);
    REQUIRE(mc_client_save_format_read_info(format, folder, &info, &e) == MC_CLIENT_SAVE_OK &&
            info != NULL);
    const mc_nbt_tag *data = mc_world_info_source(info);
    REQUIRE(data != NULL && data->type == MC_NBT_COMPOUND && data->value.compound.length == 1);
    mc_world_info_name *label = NULL;
    REQUIRE(mc_world_info_name_get(info, &label) == MC_WORLD_INFO_OK);
    REQUIRE(mc_world_info_name_text(label).units ==
                data->value.compound.items[0]->value.string.units &&
            mc_world_info_name_text(label).units[0] == 'N');
    REQUIRE(mc_client_save_format_read_state(format, &state, &e) == MC_CLIENT_SAVE_OK &&
            state.pending_reader == NULL && state.pending_gzip == NULL &&
            state.pending_stream == NULL);
    mc_world_info_release(info);
    info = NULL;
    const uint8_t wrong_data[] = {10, 0, 0, 1, 0, 4, 'D', 'a', 't', 'a', 1, 0};
    authored_gzip(meta, wrong_data, sizeof(wrong_data), false);
    REQUIRE(mc_client_save_format_read_info(format, folder, &info, &e) ==
                MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY &&
            info == NULL && e.caught_exception && e.frames[0].pc == 76 &&
            e.cause_frames[0].pc == 57);
    REQUIRE(mc_client_save_format_read_state(format, &state, &e) == MC_CLIENT_SAVE_OK &&
            state.pending_document != NULL && state.pending_data == NULL &&
            state.pending_info == NULL);
    REQUIRE(mc_java_file_stream_read_state(state.pending_stream, &stream, &fe) == MC_JAVA_FILE_OK &&
            stream.source_close_succeeded && !stream.native_open);
    REQUIRE(mc_client_save_format_clear_failure(format, &e) == MC_CLIENT_SAVE_OK);
    const uint8_t wrong_name[] = {10,  0,   0,   10,  0,   4,   'D', 'a', 't', 'a', 1, 0, 9,
                                  'L', 'e', 'v', 'e', 'l', 'N', 'a', 'm', 'e', 1,   0, 0};
    authored_gzip(meta, wrong_name, sizeof(wrong_name), false);
    REQUIRE(mc_client_save_format_read_info(format, folder, &info, &e) ==
                MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY &&
            info == NULL && e.frames[0].pc == 76 && e.cause_frames[0].pc == 68 &&
            strcmp(e.info.field, "LevelName") == 0);
    REQUIRE(mc_client_save_format_read_state(format, &state, &e) == MC_CLIENT_SAVE_OK &&
            state.pending_document == NULL && state.pending_data != NULL &&
            state.pending_info != NULL);
    mc_world_info_state is;
    REQUIRE(mc_world_info_state_get(state.pending_info, &is) == MC_WORLD_INFO_OK &&
            is.phase == MC_WORLD_INFO_FAILED_CONSTRUCTION);
    REQUIRE(mc_client_save_format_clear_failure(format, &e) == MC_CLIENT_SAVE_OK);
#ifdef MC_SAVE_FILE_CLOSE_CONTROL
    authored_gzip(meta, normal, sizeof(normal), false);
    watched = &format;
    fail_close = true;
    size_t before = actual_closes;
    REQUIRE(mc_client_save_format_read_info(format, folder, &info, &e) ==
                MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY &&
            info == NULL && e.frames[0].pc == 76 && e.cause_frames[1].pc == 22 &&
            e.close.source_fault == MC_NBT_SOURCE_INPUT &&
            e.gzip_close.status == MC_GZIP_SOURCE_IO);
    REQUIRE(actual_closes == before + 1);
    REQUIRE(mc_client_save_format_read_state(format, &state, &e) == MC_CLIENT_SAVE_OK &&
            state.pending_document != NULL && state.pending_info == NULL);
    REQUIRE(mc_java_file_stream_read_state(state.pending_stream, &stream, &fe) == MC_JAVA_FILE_OK &&
            stream.source_close_reached && !stream.source_close_succeeded && !stream.native_open);
    REQUIRE(mc_client_save_format_clear_failure(format, &e) == MC_CLIENT_SAVE_OK &&
            actual_closes == before + 1);
    watched = NULL;
    const uint8_t partial[] = {10, 0, 0, 3, 0, 1, 'x', 0};
    authored_gzip(meta, partial, sizeof(partial), false);
    watched = &format;
    fail_close = true;
    before = actual_closes;
    REQUIRE(
        mc_client_save_format_read_info(format, folder, &info, &e) ==
            MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY &&
        info == NULL && e.cause_frames[1].pc == 29 && e.stream.source_fault == MC_NBT_SOURCE_EOF &&
        e.close.source_fault == MC_NBT_SOURCE_INPUT && e.gzip_close.status == MC_GZIP_SOURCE_IO);
    REQUIRE(actual_closes == before + 1);
    REQUIRE(mc_client_save_format_clear_failure(format, &e) == MC_CLIENT_SAVE_OK &&
            actual_closes == before + 1);
    watched = NULL;
#endif
}

static void fixture(mc_nbt_string root_path) {
    mc_java_files *files = NULL;
    mc_java_file_error fe;
    REQUIRE(mc_java_files_create(NULL, &files, &fe) == MC_JAVA_FILE_OK);
    mc_world_info_name *path = NULL;
    REQUIRE(mc_world_info_name_copy(root_path, SIZE_MAX, &path) == MC_WORLD_INFO_OK);
    mc_world_info_name *world_name = name("world"), *ordinary_name = name("ordinary"),
                       *meta_name = name("level.dat"), *lock_name = name("session.lock");
    mc_java_file *root = file(files, NULL, path, false);
    bool exists = true;
    REQUIRE(mc_java_file_exists(root, &exists, &fe) == MC_JAVA_FILE_OK && !exists);
    mc_math_classes *math = NULL;
    mc_math_class_error me;
    REQUIRE(mc_math_classes_create(SIZE_MAX, &math, &me) == MC_MATH_CLASS_OK);
    mc_client_save_formats *classes = NULL;
    mc_client_save_format *format = NULL, *other = NULL;
    mc_client_save_error e;
    REQUIRE(mc_client_save_formats_create(files, math, NULL, &classes, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_allocate(classes, &format, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_construct(format, root, &e) == MC_CLIENT_SAVE_OK);
    mc_client_save_format_state fs;
    REQUIRE(mc_client_save_format_read_state(format, &fs, &e) == MC_CLIENT_SAVE_OK &&
            fs.source_a == root && fs.last_store_pc == 18);
    mc_java_file *world = file(files, root, world_name, true);
    bool made = false;
    REQUIRE(mc_java_file_mkdirs(world, &made, &fe) == MC_JAVA_FILE_OK && made);
    mc_world_info *info = NULL;
    REQUIRE(mc_client_save_format_read_info(format, world_name, &info, &e) == MC_CLIENT_SAVE_OK &&
            info == NULL);
    mc_client_save_list *list = NULL;
    REQUIRE(mc_client_save_format_list(format, &list, &e) == MC_CLIENT_SAVE_OK && list != NULL &&
            mc_client_save_list_size(list) == 0);
    mc_client_save_list *la = list;
    REQUIRE(mc_client_save_list_retain(list, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_list_release(&list, &e) == MC_CLIENT_SAVE_OK && list == NULL);
    REQUIRE(mc_client_save_list_release(&la, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_read_info(format, NULL, &info, &e) ==
                MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE &&
            e.frames[0].pc == 9);
    REQUIRE(mc_client_save_format_clear_failure(format, &e) == MC_CLIENT_SAVE_OK);
    mc_java_file *meta = file(files, world, meta_name, true);
    const uint8_t listed[] = {10,  0,   0,   10,  0,   4,   'D', 'a', 't', 'a', 8,   0, 9, 'L',
                              'e', 'v', 'e', 'l', 'N', 'a', 'm', 'e', 0,   1,   'N', 0, 0};
    authored_gzip(meta, listed, sizeof(listed), false);
    /* The real list reaches in.a(String) only after actual file/as/ei work. */
    mc_math_classes *failed_math = NULL;
    REQUIRE(mc_math_classes_create(SIZE_MAX, &failed_math, &me) == MC_MATH_CLASS_OK);
    mc_client_save_formats *failed_classes = NULL;
    mc_client_save_format *failed_format = NULL;
    REQUIRE(mc_client_save_formats_create(files, failed_math, NULL, &failed_classes, &e) ==
            MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_allocate(failed_classes, &failed_format, &e) ==
            MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_construct(failed_format, root, &e) == MC_CLIENT_SAVE_OK);
    unsigned csr = _mm_getcsr();
    REQUIRE((csr & 0xffc0u) == 0x1f80u);
    _mm_setcsr(csr | 0x2000u);
    mc_client_save_status math_failure = mc_client_save_format_list(failed_format, &list, &e);
    _mm_setcsr(csr);
    REQUIRE(math_failure == MC_CLIENT_SAVE_MATH_ERROR && list == NULL && e.frames[0].pc == 101 &&
            e.frames[1].pc == 20);
    REQUIRE(mc_client_save_format_read_state(failed_format, &fs, &e) == MC_CLIENT_SAVE_OK &&
            fs.pending_info != NULL && fs.pending_files != NULL && fs.pending_list != NULL &&
            fs.pending_selection == NULL);
    mc_math_class_state failed_state;
    REQUIRE(mc_math_classes_state(failed_math, &failed_state, &me) == MC_MATH_CLASS_OK &&
            failed_state.phase == MC_MATH_CLASS_FAILED && failed_state.last_store_pc == 4 &&
            failed_state.source_a != NULL);
    REQUIRE(mc_client_save_format_release(&failed_format, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_formats_release(&failed_classes, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_math_classes_release(&failed_math, &me) == MC_MATH_CLASS_OK);
    REQUIRE(mc_client_save_format_list(format, &list, &e) == MC_CLIENT_SAVE_OK &&
            mc_client_save_list_size(list) == 1);
    mc_client_save_selection *row = mc_client_save_list_at(list, 0);
    mc_client_save_selection_fields row_fields;
    REQUIRE(mc_client_save_selection_read(row, &row_fields, &e) == MC_CLIENT_SAVE_OK &&
            row_fields.requires_conversion && row_fields.last_store_pc == 28 &&
            row_fields.last_played == 0 && row_fields.size_on_disk == 0);
    REQUIRE(mc_world_info_name_text(row_fields.display_name).units[0] == 'N');
    equal_text(row_fields.filename, world_name);
    REQUIRE(mc_client_save_selection_retain(row, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_list_release(&list, &e) == MC_CLIENT_SAVE_OK && list == NULL);
    REQUIRE(mc_client_save_selection_read(row, &row_fields, &e) == MC_CLIENT_SAVE_OK &&
            mc_world_info_name_text(row_fields.display_name).units[0] == 'N');
    REQUIRE(mc_client_save_selection_release(&row, &e) == MC_CLIENT_SAVE_OK);
    bool needs = false;
    REQUIRE(mc_client_save_format_needs_conversion(format, world_name, &needs, &e) ==
                MC_CLIENT_SAVE_OK &&
            needs);
    mc_world_info_name *old_folder_name = name("old-world"),
                       *old_metadata_name = name("level.dat_old");
    mc_java_file *old_world = file(files, root, old_folder_name, true),
                 *old_metadata = file(files, old_world, old_metadata_name, true);
    made = false;
    REQUIRE(mc_java_file_mkdirs(old_world, &made, &fe) == MC_JAVA_FILE_OK && made);
    authored_gzip(old_metadata, listed, sizeof(listed), false);
    REQUIRE(mc_client_save_format_read_info(format, old_folder_name, &info, &e) ==
                MC_CLIENT_SAVE_OK &&
            info != NULL);
    mc_world_info_release(info);
    info = NULL;
    authored_file(old_metadata);
    REQUIRE(mc_client_save_format_read_info(format, old_folder_name, &info, &e) ==
                MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY &&
            info == NULL && e.frames[0].pc == 133 && e.cause_frames[0].pc == 105 &&
            e.cause_frames[1].pc == 9);
    REQUIRE(mc_client_save_format_clear_failure(format, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_java_file_release(&old_metadata, &fe) == MC_JAVA_FILE_OK &&
            mc_java_file_release(&old_world, &fe) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(old_folder_name);
    mc_world_info_name_release(old_metadata_name);
    mc_java_file *ordinary = file(files, root, ordinary_name, true);
    authored_file(ordinary);
    REQUIRE(mc_client_save_format_allocate(classes, &other, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_construct(other, ordinary, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_list(other, &list, &e) == MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE &&
            e.frames[0].pc == 19 && list == NULL);
    REQUIRE(mc_client_save_format_clear_failure(other, &e) == MC_CLIENT_SAVE_OK);
    mc_world_info_name *blocked_name = name("blocked"),
                       *directory_name = name("directory-metadata");
    mc_java_file *blocked = file(files, ordinary, blocked_name, true);
    mc_client_save_format *ignored_false = NULL;
    REQUIRE(mc_client_save_format_allocate(classes, &ignored_false, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_construct(ignored_false, blocked, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_read_state(ignored_false, &fs, &e) == MC_CLIENT_SAVE_OK &&
            fs.source_a == blocked && fs.last_store_pc == 18);
    exists = true;
    REQUIRE(mc_java_file_exists(blocked, &exists, &fe) == MC_JAVA_FILE_OK && !exists);
    mc_java_file *directory_world = file(files, root, directory_name, true);
    made = false;
    REQUIRE(mc_java_file_mkdirs(directory_world, &made, &fe) == MC_JAVA_FILE_OK && made);
    mc_java_file *directory_meta = file(files, directory_world, meta_name, true);
    made = false;
    REQUIRE(mc_java_file_mkdirs(directory_meta, &made, &fe) == MC_JAVA_FILE_OK && made);
    REQUIRE(mc_client_save_format_read_info(format, directory_name, &info, &e) ==
                MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY &&
            info == NULL);
    REQUIRE(e.frame_count == 1 && e.frames[0].pc == 76 && e.caught_exception &&
            e.file.status == MC_JAVA_FILE_IO_ERROR && e.cause_frame_count == 1 &&
            e.cause_frames[0].pc == 45);
    REQUIRE(mc_client_save_format_read_state(format, &fs, &e) == MC_CLIENT_SAVE_OK &&
            fs.pending_stream != NULL && fs.pending_gzip == NULL && fs.pending_reader == NULL);
    mc_java_file_stream_state rejected_stream;
    REQUIRE(mc_java_file_stream_read_state(fs.pending_stream, &rejected_stream, &fe) ==
                MC_JAVA_FILE_OK &&
            !rejected_stream.native_open && !rejected_stream.source_close_reached);
    REQUIRE(mc_client_save_format_clear_failure(format, &e) == MC_CLIENT_SAVE_OK);
    mc_world_info_name *label = NULL, *label2 = NULL;
    REQUIRE(mc_client_save_format_name(format, &label, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_name(other, &label2, &e) == MC_CLIENT_SAVE_OK && label == label2);
    mc_world_info_name_release(label);
    mc_world_info_name_release(label2);
    metadata(format, meta, world_name);
    authored_file(meta);
    REQUIRE(mc_client_save_format_read_info(format, world_name, &info, &e) ==
                MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY &&
            info == NULL);
    REQUIRE(e.frame_count == 1 && e.frames[0].pc == 76 && e.caught_exception);
    REQUIRE(e.cause_frame_count == 2 && e.cause_frames[0].pc == 48 && e.cause_frames[1].pc == 9 &&
            e.gzip.status == MC_GZIP_SOURCE_FORMAT);
    REQUIRE(mc_client_save_format_read_state(format, &fs, &e) == MC_CLIENT_SAVE_OK &&
            fs.pending_stream != NULL && fs.pending_world != world);
    equal_text(mc_java_file_path(fs.pending_world), mc_java_file_path(world));
    mc_java_file_stream_state stream;
    REQUIRE(mc_java_file_stream_read_state(fs.pending_stream, &stream, &fe) == MC_JAVA_FILE_OK &&
            stream.native_open && !stream.source_close_reached);
    REQUIRE(stream.native_bytes_read == 2 && stream.byte_read_calls == 2);
    size_t available = 0;
    REQUIRE(mc_java_file_stream_available(fs.pending_stream, &available, &fe) == MC_JAVA_FILE_OK &&
            available == 6);
    mc_gzip_state gzip;
    mc_gzip_error ge;
    REQUIRE(mc_gzip_input_state_read(fs.pending_gzip, &gzip, &ge) == MC_GZIP_OK &&
            gzip.phase == MC_GZIP_FAILED && gzip.source_in == fs.pending_stream &&
            gzip.crc != NULL && gzip.uses_default_inflater);
    mc_nbt_stream_state reader;
    mc_nbt_stream_error ne;
    REQUIRE(mc_nbt_stream_state_read(fs.pending_reader, &reader, &ne) == MC_NBT_OK &&
            reader.phase == MC_NBT_STREAM_RAW && reader.source_in == NULL &&
            reader.bytearr == NULL);
#ifdef MC_SAVE_FILE_CLOSE_CONTROL
    watched = &format;
    fail_close = true;
    size_t before = actual_closes;
    REQUIRE(mc_client_save_format_clear_failure(format, &e) == MC_CLIENT_SAVE_FILE_ERROR);
    REQUIRE(actual_closes == before + 1 && format->state.pending_stream != NULL);
    REQUIRE(mc_java_file_stream_read_state(format->state.pending_stream, &stream, &fe) ==
                MC_JAVA_FILE_OK &&
            !stream.native_open && !stream.source_close_reached);
    REQUIRE(mc_client_save_format_clear_failure(format, &e) == MC_CLIENT_SAVE_OK &&
            actual_closes == before + 1);
    watched = NULL;
#else
    REQUIRE(mc_client_save_format_clear_failure(format, &e) == MC_CLIENT_SAVE_OK);
#endif
    mc_java_file *lock = file(files, world, lock_name, true);
    exists = true;
    REQUIRE(mc_java_file_exists(lock, &exists, &fe) == MC_JAVA_FILE_OK && !exists);
    mc_world_store *store = NULL;
    REQUIRE(mc_client_save_format_open(format, world_name, true, &store, &e) ==
                MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY &&
            store == NULL && e.frames[0].pc == 0);
    mc_math_class_state ms;
    REQUIRE(mc_math_classes_state(math, &ms, &me) == MC_MATH_CLASS_OK &&
            ms.phase == MC_MATH_CLASS_READY && ms.source_a == mc_math_classes_table(math));
    REQUIRE(mc_client_save_format_release(&ignored_false, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_release(&other, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_format_release(&format, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_client_save_formats_release(&classes, &e) == MC_CLIENT_SAVE_OK);
    REQUIRE(mc_java_file_release(&lock, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_release(&meta, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_release(&ordinary, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_release(&world, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_release(&root, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_release(&blocked, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_release(&directory_meta, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_java_file_release(&directory_world, &fe) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(path);
    mc_world_info_name_release(world_name);
    mc_world_info_name_release(ordinary_name);
    mc_world_info_name_release(meta_name);
    mc_world_info_name_release(lock_name);
    mc_world_info_name_release(blocked_name);
    mc_world_info_name_release(directory_name);
    REQUIRE(mc_java_files_release(&files, &fe) == MC_JAVA_FILE_OK);
    REQUIRE(mc_math_classes_release(&math, &me) == MC_MATH_CLASS_OK);
}

static int arguments(void) {
    fputs("SaveFormat fixture input: expected absent owned --fixture-root path\n", stderr);
    return 2;
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    _Static_assert(sizeof(wchar_t) == sizeof(uint16_t), "Windows argv UTF16 code units");
    if (argc == 1)
        models();
    else if (argc == 3 && wcscmp(argv[1], L"--fixture-root") == 0)
        fixture((mc_nbt_string){(const uint16_t *)argv[2], wcslen(argv[2])});
    else
        return arguments();
    printf("SaveFormat source models: %zu checks;streaming body and explicit diagnostic/handler "
           "frontiers\n",
           checks);
    return 0;
}
#else
/* Authored argv transport only, not the production File decoder/provider. */
static bool fixture_units(const char *text, uint16_t **out, size_t *length) {
    size_t n = strlen(text);
    if (n > SIZE_MAX / sizeof(uint16_t))
        return false;
    uint16_t *u = malloc((n == 0 ? 1u : n) * sizeof(*u));
    if (u == NULL)
        return false;
    size_t count = 0;
    for (size_t i = 0; i < n;) {
        uint32_t c = (unsigned char)text[i++], minimum = 0;
        unsigned need = 0;
        if (c < 0x80u) {
        } else if (c >= 0xc2u && c <= 0xdfu) {
            c &= 31u;
            need = 1;
            minimum = 0x80u;
        } else if (c >= 0xe0u && c <= 0xefu) {
            c &= 15u;
            need = 2;
            minimum = 0x800u;
        } else if (c >= 0xf0u && c <= 0xf4u) {
            c &= 7u;
            need = 3;
            minimum = 0x10000u;
        } else
            goto invalid;
        if (need > n - i)
            goto invalid;
        for (unsigned j = 0; j < need; ++j) {
            unsigned next = (unsigned char)text[i++];
            if ((next & 0xc0u) != 0x80u)
                goto invalid;
            c = (c << 6u) | (next & 63u);
        }
        if (c < minimum || c > 0x10ffffu || (c >= 0xd800u && c <= 0xdfffu))
            goto invalid;
        if (c < 0x10000u)
            u[count++] = (uint16_t)c;
        else {
            c -= 0x10000u;
            u[count++] = (uint16_t)(0xd800u + (c >> 10u));
            u[count++] = (uint16_t)(0xdc00u + (c & 1023u));
        }
    }
    *out = u;
    *length = count;
    return true;
invalid:
    free(u);
    return false;
}

int main(int argc, char **argv) {
    if (argc == 1)
        models();
    else if (argc == 3 && strcmp(argv[1], "--fixture-root") == 0) {
        uint16_t *u = NULL;
        size_t length = 0;
        if (!fixture_units(argv[2], &u, &length))
            return arguments();
        fixture((mc_nbt_string){u, length});
        free(u);
    } else
        return arguments();
    printf("SaveFormat source models: %zu checks;streaming body and explicit diagnostic/handler "
           "frontiers\n",
           checks);
    return 0;
}
#endif
