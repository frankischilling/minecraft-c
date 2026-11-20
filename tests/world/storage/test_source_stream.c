/* Authored controls. UNRUN in the source package. All nonnull byte input is
 * a real descriptor -> raw inflater -> GZIP owner. No completed-child port. */
#include "../../../src/client/startup/directory_inputs.h"
#include "../../../src/world/storage/java_file_internal.h"
#include "../../../src/world/storage/java_file_text.h"
#include "mc/storage/compression/compression.h"
#include "mc/world/info/info.h"
#include "mc/world/storage/gzip_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <wchar.h>
#endif
static size_t checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Source stream check:%d: %s\n", __LINE__, #x);                         \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static mc_world_info_name *units(mc_nbt_string text) {
    mc_world_info_name *n = NULL;
    CHECK(mc_world_info_name_copy(text, SIZE_MAX, &n) == MC_WORLD_INFO_OK);
    return n;
}

static mc_world_info_name *ascii(const char *s) {
    uint16_t u[128];
    size_t n = strlen(s);
    CHECK(n <= 128);
    for (size_t i = 0; i < n; ++i) {
        CHECK((unsigned char)s[i] < 128);
        u[i] = (uint16_t)(unsigned char)s[i];
    }
    return units((mc_nbt_string){u, n});
}

static mc_java_file *make_file(mc_java_files *files, mc_java_file *parent,
                               mc_world_info_name *name) {
    mc_java_file *f = NULL;
    mc_java_file_error e;
    CHECK(mc_java_file_allocate(files, &f, &e) == MC_JAVA_FILE_OK);
    CHECK((parent == NULL ? mc_java_file_construct_string(f, name, &e)
                          : mc_java_file_construct_child(f, parent, name, &e)) == MC_JAVA_FILE_OK);
    return f;
}

static void write_bytes(mc_java_file *f, const uint8_t *bytes, size_t n) {
    mc_file_native_char *path = NULL;
    mc_java_file_error e;
    CHECK(mc_file_native(f, &path, &e) == MC_JAVA_FILE_OK);
#ifdef _WIN32
    FILE *out = _wfopen(path, L"wb");
#else
    FILE *out = fopen(path, "wb");
#endif
    free(path);
    CHECK(out != NULL);
    CHECK(fwrite(bytes, 1, n, out) == n);
    CHECK(fclose(out) == 0);
}

typedef struct fixture {
    mc_java_file *file;
    mc_java_file_stream *input;
    mc_gzip_input *gzip;
    mc_nbt_stream *reader;
} fixture;

static fixture open_fixture(mc_java_files *files, mc_java_file *root, const uint8_t *bytes,
                            size_t n, bool corrupt_crc) {
    mc_world_info_name *name = ascii("current.gz");
    fixture f = {.file = make_file(files, root, name)};
    mc_world_info_name_release(name);
    mc_compression_buffer encoded = {0};
    CHECK(mc_compression_encode(MC_COMPRESSION_GZIP, bytes, n, NULL, &encoded) ==
          MC_COMPRESSION_OK);
    CHECK(encoded.length >= 18);
    if (corrupt_crc)
        encoded.data[encoded.length - 8] ^= 1;
    write_bytes(f.file, encoded.data, encoded.length);
    mc_compression_buffer_free(&encoded);
    mc_java_file_error fe;
    CHECK(mc_java_file_stream_allocate(files, &f.input, &fe) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_stream_construct(f.input, f.file, &fe) == MC_JAVA_FILE_OK);
    mc_gzip_error ge;
    CHECK(mc_gzip_input_allocate(NULL, &f.gzip, &ge) == MC_GZIP_OK);
    CHECK(mc_gzip_input_construct(f.gzip, f.input, &ge) == MC_GZIP_OK);
    return f;
}

static mc_nbt_status parse(fixture *f, bool compound, mc_nbt_document **out,
                           mc_nbt_stream_error *e) {
    CHECK(mc_nbt_stream_allocate(1048576, &f->reader, e) == MC_NBT_OK);
    mc_nbt_byte_input input = mc_gzip_input_byte_port(f->gzip);
    CHECK(mc_nbt_stream_construct(f->reader, &input, NULL, e) == MC_NBT_OK);
    return mc_nbt_stream_parse(f->reader, compound, out, e);
}

static void close_fixture(fixture *f) {
    mc_nbt_stream_error ne;
    mc_gzip_error ge;
    mc_java_file_error fe;
    CHECK(mc_nbt_stream_release(&f->reader, &ne) == MC_NBT_OK);
    CHECK(mc_gzip_input_release(&f->gzip, &ge) == MC_GZIP_OK);
    CHECK(mc_java_file_stream_release(&f->input, &fe) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_release(&f->file, &fe) == MC_JAVA_FILE_OK);
}

static const uint8_t empty_compound[] = {10, 0, 0, 0};

static void simple_and_crc(mc_java_files *files, mc_java_file *root) {
    fixture f = open_fixture(files, root, empty_compound, sizeof(empty_compound), true);
    mc_nbt_document *doc = NULL;
    mc_nbt_stream_error ne;
    CHECK(parse(&f, true, &doc, &ne) == MC_NBT_OK);
    CHECK(mc_nbt_document_root(doc)->type == MC_NBT_COMPOUND);
    mc_gzip_state gs;
    mc_gzip_error ge;
    CHECK(mc_gzip_input_state_read(f.gzip, &gs, &ge) == MC_GZIP_OK);
    CHECK(!gs.eos && !gs.reach_eof && gs.output_returned == 4 && gs.trailer_bytes_taken == 0);
    /* A completed root does not force EOF/CRC. The NEXT actual read reaches it. */
    int value = 123;
    CHECK(mc_gzip_input_read_byte(f.gzip, &value, &ge) == MC_GZIP_SOURCE_FORMAT);
    CHECK(value == 123 && ge.source_exception);
    CHECK(mc_gzip_input_state_read(f.gzip, &gs, &ge) == MC_GZIP_OK && gs.trailer_bytes_taken == 4);
    CHECK(mc_nbt_stream_source_close(f.reader, &ne) == MC_NBT_OK);
    mc_java_file_stream_state is;
    mc_java_file_error fe;
    CHECK(mc_java_file_stream_read_state(f.input, &is, &fe) == MC_JAVA_FILE_OK &&
          is.source_close_succeeded && !is.native_open);
    mc_nbt_document_free(doc);
    doc = NULL;
    close_fixture(&f);
    f = open_fixture(files, root, empty_compound, sizeof(empty_compound), false);
    CHECK(parse(&f, true, &doc, &ne) == MC_NBT_OK);
    value = 0;
    CHECK(mc_gzip_input_read_byte(f.gzip, &value, &ge) == MC_GZIP_OK && value == -1);
    CHECK(mc_gzip_input_state_read(f.gzip, &gs, &ge) == MC_GZIP_OK && gs.eos &&
          gs.trailer_bytes_taken == 8);
    CHECK(mc_nbt_stream_source_close(f.reader, &ne) == MC_NBT_OK);
    mc_nbt_document_free(doc);
    doc = NULL;
    close_fixture(&f);
}

static void primitive_prefixes(mc_java_files *files, mc_java_file *root) {
    const uint8_t i32[] = {3, 0, 0, 0x12, 0x34, 0x56, 0x78};
    mc_nbt_document *doc = NULL;
    mc_nbt_stream_error ne;
    mc_nbt_stream_state rs;
    mc_gzip_state gs;
    mc_gzip_error ge;
    fixture f = open_fixture(files, root, i32, sizeof(i32), false);
    CHECK(parse(&f, false, &doc, &ne) == MC_NBT_OK);
    CHECK(mc_nbt_document_root(doc)->value.int_value == 0x12345678);
    CHECK(mc_nbt_stream_state_read(f.reader, &rs, &ne) == MC_NBT_OK && rs.read_calls == 7 &&
          rs.position == 7 && rs.last_tag_store == 7);
    CHECK(rs.read_buffer[0] == 0);
    CHECK(mc_gzip_input_state_read(f.gzip, &gs, &ge) == MC_GZIP_OK && gs.single_byte[0] == 0x78);
    mc_nbt_document_free(doc);
    doc = NULL;
    close_fixture(&f);
    const uint8_t i64[] = {4, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8};
    f = open_fixture(files, root, i64, sizeof(i64), false);
    CHECK(parse(&f, false, &doc, &ne) == MC_NBT_OK);
    CHECK(mc_nbt_document_root(doc)->value.long_value == INT64_C(0x0102030405060708));
    CHECK(mc_nbt_stream_state_read(f.reader, &rs, &ne) == MC_NBT_OK && rs.read_calls == 4);
    CHECK(memcmp(rs.read_buffer, i64 + 3, 8) == 0);
    CHECK(mc_gzip_input_state_read(f.gzip, &gs, &ge) == MC_GZIP_OK && gs.single_byte[0] == 0);
    mc_nbt_document_free(doc);
    doc = NULL;
    close_fixture(&f);
    const uint8_t short_i32[] = {3, 0, 0, 0x12};
    f = open_fixture(files, root, short_i32, sizeof(short_i32), false);
    doc = NULL;
    CHECK(parse(&f, false, &doc, &ne) == MC_NBT_TRUNCATED && doc == NULL &&
          ne.source_fault == MC_NBT_SOURCE_EOF);
    CHECK(mc_nbt_stream_state_read(f.reader, &rs, &ne) == MC_NBT_OK && rs.read_calls == 7 &&
          rs.position == 4 && rs.last_tag_store == 31);
    close_fixture(&f);
    const uint8_t short_i64[] = {4, 0, 0, 1, 2, 3};
    f = open_fixture(files, root, short_i64, sizeof(short_i64), false);
    CHECK(parse(&f, false, &doc, &ne) == MC_NBT_TRUNCATED);
    CHECK(mc_nbt_stream_state_read(f.reader, &rs, &ne) == MC_NBT_OK && rs.position == 6 &&
          rs.last_tag_store == 31);
    CHECK(memcmp(rs.read_buffer, short_i64 + 3, 3) == 0);
    close_fixture(&f);
}

static void tag_prefixes(mc_java_files *files, mc_java_file *root) {
    const uint8_t unknown[] = {11, 0, 1, 'x'};
    fixture f = open_fixture(files, root, unknown, sizeof(unknown), false);
    mc_nbt_document *doc = NULL;
    mc_nbt_stream_error ne;
    mc_nbt_stream_state rs;
    CHECK(parse(&f, false, &doc, &ne) == MC_NBT_SOURCE_FAILURE &&
          ne.source_fault == MC_NBT_SOURCE_NULL_TAG && ne.source_pc == 31);
    CHECK(mc_nbt_stream_state_read(f.reader, &rs, &ne) == MC_NBT_OK && rs.position == 4 &&
          rs.last_tag == NULL);
    close_fixture(&f);
    const uint8_t negative_array[] = {7, 0, 0, 255, 255, 255, 255};
    f = open_fixture(files, root, negative_array, sizeof(negative_array), false);
    CHECK(parse(&f, false, &doc, &ne) == MC_NBT_INVALID_LENGTH &&
          ne.source_fault == MC_NBT_SOURCE_NEGATIVE_ARRAY && ne.source_pc == 9);
    CHECK(mc_nbt_stream_state_read(f.reader, &rs, &ne) == MC_NBT_OK && rs.last_array == NULL &&
          rs.last_tag_store == 31);
    close_fixture(&f);
    const uint8_t short_array[] = {7, 0, 0, 0, 0, 0, 3, 0x42};
    f = open_fixture(files, root, short_array, sizeof(short_array), false);
    CHECK(parse(&f, false, &doc, &ne) == MC_NBT_TRUNCATED && ne.source_pc == 19);
    CHECK(mc_nbt_stream_state_read(f.reader, &rs, &ne) == MC_NBT_OK && rs.last_array != NULL &&
          rs.last_array_length == 3 && rs.last_tag_store == 11);
    CHECK(((const uint8_t *)rs.last_array)[0] == 0x42);
    close_fixture(&f);
    const uint8_t negative_list[] = {9, 0, 0, 99, 255, 255, 255, 255};
    f = open_fixture(files, root, negative_list, sizeof(negative_list), false);
    CHECK(parse(&f, false, &doc, &ne) == MC_NBT_OK);
    CHECK(mc_nbt_document_root(doc)->value.list.length == 0 &&
          mc_nbt_document_root(doc)->value.list.element_type == 99);
    CHECK(mc_nbt_stream_state_read(f.reader, &rs, &ne) == MC_NBT_OK && rs.last_tag_store == 25 &&
          rs.last_collection != NULL);
    mc_nbt_document_free(doc);
    doc = NULL;
    close_fixture(&f);
    const uint8_t partial_compound[] = {10, 0, 0, 1, 0, 1, 'a', 7, 11, 0, 1, 'b'};
    f = open_fixture(files, root, partial_compound, sizeof(partial_compound), false);
    CHECK(parse(&f, true, &doc, &ne) == MC_NBT_SOURCE_FAILURE && ne.source_pc == 31);
    CHECK(mc_nbt_stream_state_read(f.reader, &rs, &ne) == MC_NBT_OK &&
          rs.last_collection_count == 1);
    CHECK(mc_nbt_document_root(rs.candidate)->value.compound.length == 1 &&
          mc_nbt_document_root(rs.candidate)->value.compound.items[0]->value.byte_value == 7);
    close_fixture(&f);
}

static void utf_prefixes(mc_java_files *files, mc_java_file *root) {
    const uint8_t accepts[] = {
        8,    0,    0,    0,    10,   0,    0xc0, 0x80,
        0xc1, 0x81, 0xe0, 0x80, 0x80, 0xed, 0xa0, 0x80}; /* corrected length below */
    uint8_t payload[sizeof(accepts)];
    memcpy(payload, accepts, sizeof(payload));
    payload[4] = 11;
    fixture f = open_fixture(files, root, payload, sizeof(payload), false);
    mc_nbt_document *doc = NULL;
    mc_nbt_stream_error ne;
    mc_nbt_stream_state rs;
    CHECK(parse(&f, false, &doc, &ne) == MC_NBT_OK);
    const mc_nbt_string s = mc_nbt_document_root(doc)->value.string;
    CHECK(s.length == 5 && s.units[0] == 0 && s.units[1] == 0 && s.units[2] == 65 &&
          s.units[3] == 0 && s.units[4] == 0xd800);
    mc_nbt_document_free(doc);
    doc = NULL;
    close_fixture(&f);
    const uint8_t malformed[] = {8, 0, 0, 0, 4, 'A', 0xe0, 0x80, 'X'};
    f = open_fixture(files, root, malformed, sizeof(malformed), false);
    CHECK(parse(&f, false, &doc, &ne) == MC_NBT_INVALID_UTF &&
          ne.source_fault == MC_NBT_SOURCE_UTF);
    CHECK(ne.utf_prefix_units == 1);
    CHECK(mc_nbt_stream_state_read(f.reader, &rs, &ne) == MC_NBT_OK &&
          rs.position == sizeof(malformed) && rs.chararr[0] == 'A' && rs.last_tag_store == 31);
    close_fixture(&f);
    const uint8_t truncated[] = {8, 0, 0, 0, 3, 'A', 0xe0};
    f = open_fixture(files, root, truncated, sizeof(truncated), false);
    CHECK(parse(&f, false, &doc, &ne) == MC_NBT_TRUNCATED && ne.source_fault == MC_NBT_SOURCE_EOF);
    CHECK(mc_nbt_stream_state_read(f.reader, &rs, &ne) == MC_NBT_OK && rs.chararr[0] == 0 &&
          rs.bytearr[0] == 'A' && rs.bytearr[1] == 0xe0);
    close_fixture(&f);
}

static void document_adoption(void) {
    /* Root/Data/LevelName actual decoded units and Player children. */
    const uint8_t bytes[] = {10,  0,   0,   10,  0,   4,   'D', 'a', 't', 'a', 8,   0,   9,   'L',
                             'e', 'v', 'e', 'l', 'N', 'a', 'm', 'e', 0,   1,   'N', 10,  0,   6,
                             'P', 'l', 'a', 'y', 'e', 'r', 3,   0,   9,   'D', 'i', 'm', 'e', 'n',
                             's', 'i', 'o', 'n', 0,   0,   0,   2,   0,   0,   0};
    mc_nbt_document *doc = NULL;
    mc_nbt_error pe;
    CHECK(mc_nbt_read_compound(bytes, sizeof(bytes), NULL, &doc, NULL, &pe) == MC_NBT_OK);
    const mc_nbt_tag *root = mc_nbt_document_root(doc), *data = root->value.compound.items[0],
                     *name = data->value.compound.items[0], *player = data->value.compound.items[1];
    mc_world_info_data *adopted = NULL;
    mc_world_info_error ie;
    mc_nbt_document *before = doc;
    CHECK(mc_world_info_data_adopt_document(&doc, 0, &adopted, &ie) == MC_WORLD_INFO_LIMIT &&
          doc == before && adopted == NULL);
    CHECK(mc_world_info_data_adopt_document(&doc, SIZE_MAX, &adopted, &ie) == MC_WORLD_INFO_OK &&
          doc == NULL && mc_world_info_data_document(adopted) == before);
    mc_world_info_name *n = mc_world_info_data_name(adopted);
    mc_world_info_player *p = mc_world_info_data_player(adopted);
    CHECK(mc_world_info_name_text(n).units == name->value.string.units);
    CHECK(mc_world_info_player_root(p)->value.compound.items == player->value.compound.items);
    mc_world_info *a = NULL, *b = NULL, *c = NULL;
    CHECK(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &a) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &b) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &c) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_nbt(a, adopted, &ie) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_nbt(b, adopted, &ie) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_copy(c, a) == MC_WORLD_INFO_OK);
    mc_world_info_name *an = NULL, *bn = NULL, *cn = NULL;
    CHECK(mc_world_info_name_get(a, &an) == MC_WORLD_INFO_OK &&
          mc_world_info_name_get(b, &bn) == MC_WORLD_INFO_OK &&
          mc_world_info_name_get(c, &cn) == MC_WORLD_INFO_OK);
    CHECK(an == n && bn == n && cn == n);
    CHECK(mc_world_info_name_retain(n) == MC_WORLD_INFO_OK &&
          mc_world_info_player_retain(p) == MC_WORLD_INFO_OK);
    mc_world_info_data_release(adopted);
    mc_world_info_release(a);
    mc_world_info_release(b);
    mc_world_info_release(c);
    CHECK(mc_world_info_name_text(n).units[0] == 'N' &&
          mc_world_info_player_root(p)->value.compound.items[0]->value.int_value == 2);
    mc_world_info_name_release(n);
    mc_world_info_player_release(p);
}

static void null_and_holds(void) {
    mc_gzip_input *g = NULL;
    mc_gzip_error ge;
    mc_gzip_state gs;
    CHECK(mc_gzip_input_allocate(NULL, &g, &ge) == MC_GZIP_OK);
    CHECK(mc_gzip_input_construct(g, NULL, &ge) == MC_GZIP_SOURCE_NULL && ge.source_pc == 37);
    CHECK(mc_gzip_input_state_read(g, &gs, &ge) == MC_GZIP_OK && gs.pending_inflater == NULL &&
          gs.source_inf == NULL && gs.single_byte != NULL && gs.skip_buffer != NULL &&
          gs.fill_buffer == NULL && gs.last_store_pc == 33);
    CHECK(mc_gzip_input_retain(g, &ge) == MC_GZIP_OK);
    mc_gzip_input *other = g;
    CHECK(mc_gzip_input_release(&g, &ge) == MC_GZIP_OK && g == NULL);
    CHECK(mc_gzip_input_release(&other, &ge) == MC_GZIP_OK && other == NULL);
    mc_nbt_stream *r = NULL;
    mc_nbt_stream_error ne;
    mc_nbt_byte_input input = {0};
    mc_nbt_document *out = NULL;
    CHECK(mc_nbt_stream_allocate(1048576, &r, &ne) == MC_NBT_OK);
    CHECK(mc_nbt_stream_construct(r, &input, NULL, &ne) == MC_NBT_OK);
    CHECK(mc_nbt_stream_parse(r, true, &out, &ne) == MC_NBT_SOURCE_FAILURE &&
          ne.source_fault == MC_NBT_SOURCE_NULL_INPUT && ne.source_pc == 1);
    CHECK(mc_nbt_stream_release(&r, &ne) == MC_NBT_OK);
}

static void strings(void) {
    mc_java_files *files = NULL;
    mc_java_file_error e;
    CHECK(mc_java_files_create(NULL, &files, &e) == MC_JAVA_FILE_OK);
    mc_world_info_name *parent = ascii("parent"), *child = ascii("child"), *empty = ascii("");
    mc_java_file *a = NULL, *b = NULL, *parent_file = make_file(files, NULL, parent);
    CHECK(mc_java_file_allocate(files, &a, &e) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_construct_strings(a, NULL, child, &e) == MC_JAVA_FILE_OK &&
          mc_java_file_path(a) == child);
    CHECK(mc_java_file_release(&a, &e) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_allocate(files, &a, &e) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_construct_strings(a, parent, child, &e) == MC_JAVA_FILE_OK);
    b = make_file(files, parent_file, child);
    mc_nbt_string x = mc_world_info_name_text(mc_java_file_path(a)),
                  y = mc_world_info_name_text(mc_java_file_path(b));
    CHECK(x.length == y.length && memcmp(x.units, y.units, x.length * sizeof(*x.units)) == 0);
    CHECK(mc_java_file_release(&a, &e) == MC_JAVA_FILE_OK &&
          mc_java_file_release(&b, &e) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_allocate(files, &a, &e) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_construct_strings(a, parent, NULL, &e) == MC_JAVA_FILE_SOURCE_NULL_STRING &&
          a->pending_input == NULL && a->pending_parent == NULL && a->state.path == NULL);
    CHECK(mc_java_file_release(&a, &e) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_release(&parent_file, &e) == MC_JAVA_FILE_OK);
    CHECK(mc_java_files_release(&files, &e) == MC_JAVA_FILE_OK);
    mc_java_file_options options = mc_java_file_default_options();
    options.max_path_units = 0;
    CHECK(mc_java_files_create(&options, &files, &e) == MC_JAVA_FILE_OK);
    CHECK(files->default_parent == NULL);
    CHECK(mc_java_file_allocate(files, &a, &e) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_construct_strings(a, empty, child, &e) == MC_JAVA_FILE_LIMIT &&
          files->default_parent != NULL && a->pending_parent == empty &&
          a->pending_input == child && a->state.path == NULL);
    CHECK(mc_java_file_release(&a, &e) == MC_JAVA_FILE_OK &&
          mc_java_files_release(&files, &e) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(parent);
    mc_world_info_name_release(child);
    mc_world_info_name_release(empty);
}

static void allocation_prefixes(mc_java_files *files, mc_java_file *root) {
    fixture f = open_fixture(files, root, empty_compound, sizeof(empty_compound), false);
    mc_gzip_state state;
    mc_gzip_error ge;
    CHECK(mc_gzip_input_state_read(f.gzip, &state, &ge) == MC_GZIP_OK);
    mc_gzip_input *raw = NULL;
    CHECK(mc_gzip_input_allocate(NULL, &raw, &ge) == MC_GZIP_OK);
    CHECK(mc_gzip_input_state_read(raw, &state, &ge) == MC_GZIP_OK);
    size_t base = state.owner_bytes;
    CHECK(mc_gzip_input_release(&raw, &ge) == MC_GZIP_OK);
    const size_t quota[] = {0, 1, 513, 1025, 1029, 1157};
    const int error_pc[] = {22, 31, 73, 23, 42, -1};
    const int store_pc[] = {17, 24, 68, 75, 35, 49};
    for (size_t i = 0; i < sizeof(quota) / sizeof(*quota); ++i) {
        mc_java_file_stream *in = NULL;
        mc_java_file_error fe;
        CHECK(mc_java_file_stream_allocate(files, &in, &fe) == MC_JAVA_FILE_OK &&
              mc_java_file_stream_construct(in, f.file, &fe) == MC_JAVA_FILE_OK);
        mc_gzip_options options = mc_gzip_default_options();
        options.maximum_owner_bytes = base + quota[i];
        CHECK(mc_gzip_input_allocate(&options, &raw, &ge) == MC_GZIP_OK);
        mc_gzip_status status = mc_gzip_input_construct(raw, in, &ge);
        CHECK(status == (i == 5 ? MC_GZIP_OK : MC_GZIP_LIMIT));
        if (i < 5)
            CHECK(ge.source_pc == error_pc[i] && !ge.source_exception);
        CHECK(mc_gzip_input_state_read(raw, &state, &ge) == MC_GZIP_OK &&
              state.last_store_pc == store_pc[i] && state.source_in == in);
        CHECK((state.source_inf != NULL) == (i >= 2));
        CHECK((state.pending_inflater != NULL) == (i < 2));
        CHECK(mc_gzip_input_release(&raw, &ge) == MC_GZIP_OK);
        mc_java_file_stream_state is;
        CHECK(mc_java_file_stream_read_state(in, &is, &fe) == MC_JAVA_FILE_OK && is.native_open &&
              !is.source_close_reached && is.holds == 1);
        CHECK(mc_java_file_stream_release(&in, &fe) == MC_JAVA_FILE_OK);
    }
    close_fixture(&f);
}

static void reader_prefixes(mc_java_files *files, mc_java_file *root) {
    fixture f = open_fixture(files, root, empty_compound, sizeof(empty_compound), false);
    mc_nbt_stream *r = NULL;
    mc_nbt_stream_error e;
    mc_nbt_stream_state state;
    CHECK(mc_nbt_stream_allocate(1048576, &r, &e) == MC_NBT_OK);
    CHECK(mc_nbt_stream_state_read(r, &state, &e) == MC_NBT_OK);
    size_t base = state.owner_bytes;
    CHECK(mc_nbt_stream_release(&r, &e) == MC_NBT_OK);
    const size_t quotas[] = {0, 80, 240, 248};
    const int errors[] = {8, 16, 24, -1};
    const int stores[] = {6, 10, 18, 26};
    mc_nbt_byte_input port = mc_gzip_input_byte_port(f.gzip);
    for (size_t i = 0; i < 4; ++i) {
        CHECK(mc_nbt_stream_allocate(base + quotas[i], &r, &e) == MC_NBT_OK);
        mc_nbt_status status = mc_nbt_stream_construct(r, &port, NULL, &e);
        CHECK(status == (i == 3 ? MC_NBT_OK : MC_NBT_LIMIT));
        if (i < 3)
            CHECK(e.source_pc == errors[i] && e.source_fault == MC_NBT_SOURCE_NONE);
        CHECK(mc_nbt_stream_state_read(r, &state, &e) == MC_NBT_OK &&
              state.last_data_input_store == stores[i] && state.source_in == f.gzip);
        CHECK((state.bytearr != NULL) == (i >= 1) && (state.chararr != NULL) == (i >= 2) &&
              (state.read_buffer != NULL) == (i >= 3));
        CHECK(mc_nbt_stream_release(&r, &e) == MC_NBT_OK);
    }
    close_fixture(&f);
    bool complete = false, pending = false, collection = false;
    for (size_t quota = 0; quota <= 1024 && !complete; ++quota) {
        f = open_fixture(files, root, empty_compound, sizeof(empty_compound), false);
        mc_nbt_document *doc = NULL;
        CHECK(mc_nbt_stream_allocate(1048576, &f.reader, &e) == MC_NBT_OK);
        port = mc_gzip_input_byte_port(f.gzip);
        mc_nbt_limits limits = mc_nbt_default_limits();
        limits.max_allocation_bytes = quota;
        CHECK(mc_nbt_stream_construct(f.reader, &port, &limits, &e) == MC_NBT_OK);
        mc_nbt_status status = mc_nbt_stream_parse(f.reader, true, &doc, &e);
        if (status == MC_NBT_OK) {
            complete = true;
            CHECK(doc != NULL);
            mc_nbt_document_free(doc);
        } else {
            CHECK(status == MC_NBT_LIMIT && e.source_fault == MC_NBT_SOURCE_NONE && doc == NULL);
            mc_nbt_stream_error original = e;
            CHECK(mc_nbt_stream_state_read(f.reader, &state, &e) == MC_NBT_OK &&
                  state.close_calls == 0);
            if (state.pending_tag != NULL) {
                if (state.pending_tag->type == MC_NBT_COMPOUND) {
                    CHECK(mc_nbt_document_root(state.candidate) == state.pending_tag);
                    if (state.last_tag == NULL) {
                        pending = true;
                        CHECK(original.source_class != NULL &&
                              strcmp(original.source_class, "ij") == 0 &&
                              original.source_method != NULL &&
                              strcmp(original.source_method, "a(B)Lij;") == 0 &&
                              original.source_pc == 144 && state.last_tag_store == 0 &&
                              state.last_tag_store_class == NULL && state.last_collection == NULL);
                    } else {
                        CHECK(state.last_tag == state.pending_tag && state.last_tag_store == 6 &&
                              state.last_tag_store_class != NULL &&
                              strcmp(state.last_tag_store_class, "ij") == 0 &&
                              state.last_collection == NULL && original.source_class != NULL &&
                              strcmp(original.source_class, "nu") == 0 &&
                              original.source_method != NULL &&
                              strcmp(original.source_method, "<init>()V") == 0 &&
                              original.source_pc == 5);
                    }
                } else
                    CHECK(state.pending_tag->type == MC_NBT_END &&
                          mc_nbt_document_root(state.candidate) != state.pending_tag &&
                          original.source_pc == 15);
            }
            if (state.last_tag != NULL && state.last_tag_store == 6) {
                collection = true;
                CHECK(original.source_class != NULL && strcmp(original.source_class, "nu") == 0 &&
                      original.source_pc == 5);
            }
        }
        mc_java_file_stream_state is;
        mc_java_file_error fe;
        CHECK(mc_java_file_stream_read_state(f.input, &is, &fe) == MC_JAVA_FILE_OK &&
              is.native_open && !is.source_close_reached);
        close_fixture(&f);
    }
    CHECK(complete && pending && collection);
}

static void provider(void) {
    mc_client_directory_inputs *p = NULL;
    mc_client_game_directory_error e;
    mc_client_game_directory_state state;
    mc_world_info_name *a = NULL, *b = NULL;
    mc_client_directory_platform platform;
    CHECK(mc_client_directory_inputs_create(NULL, 1048576, &p, &e) == MC_CLIENT_GAME_DIRECTORY_OK);
    CHECK(mc_client_directory_inputs_home(p, &a, &state, &e) == MC_CLIENT_GAME_DIRECTORY_OK);
    CHECK(mc_client_directory_inputs_home(p, &b, &state, &e) == MC_CLIENT_GAME_DIRECTORY_OK &&
          a == b);
    CHECK(mc_client_directory_inputs_platform(p, &platform, &e) == MC_CLIENT_GAME_DIRECTORY_OK);
    CHECK((unsigned)platform <= MC_CLIENT_DIRECTORY_UNKNOWN);
    CHECK(mc_client_directory_inputs_release(&p, &e) == MC_CLIENT_GAME_DIRECTORY_OK && p == NULL);
    CHECK(mc_world_info_name_text(a).length != 0);
    mc_world_info_name_release(a);
    mc_world_info_name_release(b);
}

static void run(mc_nbt_string root_text) {
    mc_java_files *files = NULL;
    mc_java_file_error fe;
    CHECK(mc_java_files_create(NULL, &files, &fe) == MC_JAVA_FILE_OK);
    mc_world_info_name *name = units(root_text);
    mc_java_file *root = make_file(files, NULL, name);
    mc_world_info_name_release(name);
    bool exists = true, made = false;
    CHECK(mc_java_file_exists(root, &exists, &fe) == MC_JAVA_FILE_OK && !exists);
    CHECK(mc_java_file_mkdirs(root, &made, &fe) == MC_JAVA_FILE_OK && made);
    strings();
    null_and_holds();
    document_adoption();
    allocation_prefixes(files, root);
    reader_prefixes(files, root);
    simple_and_crc(files, root);
    primitive_prefixes(files, root);
    tag_prefixes(files, root);
    utf_prefixes(files, root);
    provider();
    CHECK(mc_java_file_release(&root, &fe) == MC_JAVA_FILE_OK);
    CHECK(mc_java_files_release(&files, &fe) == MC_JAVA_FILE_OK);
    printf("Source stream: %zu checks; real descriptor GZIP NBT and document identity\n", checks);
}

static int arguments(void) {
    fputs("Source stream input: expected absent owned fixture root\n", stderr);
    return 2;
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    if (argc != 2)
        return arguments();
    run((mc_nbt_string){(const uint16_t *)argv[1], wcslen(argv[1])});
    return 0;
}
#else
int main(int argc, char **argv) {
    if (argc != 2)
        return arguments();
    mc_world_info_name *name = NULL;
    mc_java_file_error e;
    if (mc_file_decode_utf8(argv[1], SIZE_MAX, &name, &e) != MC_JAVA_FILE_OK)
        return arguments();
    run(mc_world_info_name_text(name));
    mc_world_info_name_release(name);
    return 0;
}
#endif
