#include "mc/world/saved_data/storage.h"

#include "files.h"
#include "mc/storage/compression/compression.h"
#include "mc/world/storage/regions.h"
#include "mc/world/storage/store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Native constructed manager, ownership and file controls. These cases do not
 * execute original classes or qualify original HashMap/file-failure behavior. */
static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool name(const char *text, mc_world_info_name **out) {
    uint16_t units[128];
    size_t length = strlen(text);
    if (length > sizeof(units) / sizeof(units[0]))
        return false;
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)text[i];
    return mc_world_info_name_copy((mc_nbt_string){units, length}, SIZE_MAX, out) ==
           MC_WORLD_INFO_OK;
}

static bool graph(const mc_world_saved_data_view *view, bool stored, size_t named, size_t ordered,
                  size_t ids) {
    return (view->storage != NULL) == stored && view->named_cache != NULL &&
           view->ordered_list != NULL && view->id_map != NULL &&
           view->named_cache != view->ordered_list && view->named_cache != view->id_map &&
           view->ordered_list != view->id_map && view->named_count == named &&
           view->ordered_count == ordered && view->id_count == ids;
}

static bool same_graph(const mc_world_saved_data_view *left,
                       const mc_world_saved_data_view *right) {
    return left->storage == right->storage && left->named_cache == right->named_cache &&
           left->ordered_list == right->ordered_list && left->id_map == right->id_map;
}

static bool ordered(const mc_world_saved_data *owner, mc_saved_data_value *const *values,
                    size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (mc_world_saved_data_ordered_at(owner, i) != values[i])
            return false;
    return mc_world_saved_data_ordered_at(owner, count) == NULL;
}

static bool zero_colors(const mc_saved_data_map_fields *fields) {
    if (fields->colors == NULL || mc_nbt_byte_array_length(fields->colors) != 16384)
        return false;
    for (size_t i = 0; i < 16384; ++i)
        if (mc_nbt_byte_array_data(fields->colors)[i] != 0)
            return false;
    return true;
}

static bool open_store(const char *root, const char *scenario, mc_world_info_side side,
                       mc_world_regions *regions, mc_world_store **out) {
    char leaf[128];
    int length = snprintf(leaf, sizeof(leaf), "%s-%s", scenario,
                          side == MC_WORLD_INFO_CLIENT ? "client" : "server");
    if (length < 0 || (size_t)length >= sizeof(leaf))
        return false;
    char *path = NULL;
    if (mc_ws_path_join(root, leaf, &path, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    uint8_t map[256];
    for (size_t i = 0; i < 256; ++i)
        map[i] = (uint8_t)i;
    mc_world_store_options options = {0};
    options.block_map = map;
    options.block_map_length = sizeof(map);
    options.limits = mc_world_store_default_limits();
    options.session_token = INT64_C(0x0123456789abcdef);
    bool okay = mc_world_store_open(path, regions, &options, out, NULL) == MC_WORLD_STORAGE_OK;
    free(path);
    return okay;
}

static bool data_path(const mc_world_store *store, const char *stem, char **out) {
    char leaf[160];
    int length = snprintf(leaf, sizeof(leaf), "data/%s.dat", stem);
    return length >= 0 && (size_t)length < sizeof(leaf) &&
           mc_ws_path_join(mc_world_store_root(store), leaf, out, NULL) == MC_WORLD_STORAGE_OK;
}

static bool file_is(const char *path, const uint8_t *expected, size_t length) {
    mc_ws_file_bytes bytes = {0};
    bool okay = mc_ws_read_file(path, 1024u * 1024u, &bytes, NULL) == MC_WORLD_STORAGE_OK &&
                bytes.length == length &&
                (length == 0 || memcmp(bytes.data, expected, length) == 0);
    mc_ws_free_file_bytes(&bytes);
    return okay;
}

static bool absent(const char *path) {
    bool exists = true;
    return mc_ws_path_exists(path, &exists, NULL) == MC_WORLD_STORAGE_OK && !exists;
}

static bool write_gzip_raw(const char *path, const uint8_t *raw, size_t length) {
    mc_compression_buffer compressed = {0};
    bool okay = mc_compression_encode(MC_COMPRESSION_GZIP, raw, length, NULL, &compressed) ==
                MC_COMPRESSION_OK;
    if (okay)
        okay = mc_ws_write_file(path, compressed.data, compressed.length, false, NULL) ==
               MC_WORLD_STORAGE_OK;
    mc_compression_buffer_free(&compressed);
    return okay;
}

static bool write_gzip_tree(const char *path, const mc_nbt_tag *root) {
    size_t length = 0, written = 0;
    if (mc_nbt_measure(root, NULL, &length, NULL) != MC_NBT_OK)
        return false;
    uint8_t *raw = malloc(length);
    if (raw == NULL)
        return false;
    bool okay = mc_nbt_write(root, NULL, raw, length, &written, NULL) == MC_NBT_OK &&
                written == length && write_gzip_raw(path, raw, written);
    free(raw);
    return okay;
}

static bool read_gzip_tree(const char *path, mc_nbt_document **out) {
    mc_ws_file_bytes file = {0};
    mc_compression_buffer raw = {0};
    size_t consumed = 0;
    bool okay =
        mc_ws_read_file(path, 1024u * 1024u, &file, NULL) == MC_WORLD_STORAGE_OK &&
        file.length > 2 && file.data[0] == 0x1f && file.data[1] == 0x8b &&
        mc_compression_decode(MC_COMPRESSION_GZIP, file.data, file.length, NULL, &raw, &consumed) ==
            MC_COMPRESSION_OK &&
        consumed == file.length &&
        mc_nbt_read_compound(raw.data, raw.length, NULL, out, &consumed, NULL) == MC_NBT_OK &&
        consumed == raw.length;
    mc_ws_free_file_bytes(&file);
    mc_compression_buffer_free(&raw);
    return okay;
}

static const mc_nbt_tag *child(const mc_nbt_tag *root, const char *key) {
    uint16_t units[9];
    size_t length = strlen(key);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)key[i];
    return mc_nbt_compound_get(root, units, length);
}

static int test_memory(mc_world_info_side side) {
    int result = 1;
    mc_world_saved_data *owner = NULL;
    mc_saved_data_value *a = NULL, *b = NULL, *c = NULL, *hit = NULL, *foreign = NULL;
    mc_world_info_name *first = NULL, *equal = NULL, *middle = NULL, *last = NULL, *empty = NULL,
                       *missing = NULL;
    mc_world_info_name *unicode = NULL, *unicode_equal = NULL;
    mc_world_saved_data_view initial, current;
    mc_world_storage_error error;
    CHECK(name("same", &first) && name("same", &equal) && name("middle", &middle) &&
          name("last", &last) && name("", &empty) && name("missing", &missing) && first != equal &&
          empty != NULL);
    static const uint16_t unusual[] = {'u', 0, 0xd800, 0xffff};
    CHECK(mc_world_info_name_copy((mc_nbt_string){unusual, 4}, SIZE_MAX, &unicode) ==
              MC_WORLD_INFO_OK &&
          mc_world_info_name_copy((mc_nbt_string){unusual, 4}, SIZE_MAX, &unicode_equal) ==
              MC_WORLD_INFO_OK &&
          unicode != unicode_equal);
    CHECK(mc_world_saved_data_create_for_side(NULL, side, NULL, &owner, &error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), NULL, SIZE_MAX, &a, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), NULL, SIZE_MAX, &b, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), NULL, SIZE_MAX, &c, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_inspect(owner, &initial) == MC_WORLD_STORAGE_OK &&
          graph(&initial, false, 0, 0, 0));
    CHECK(initial.class_bound && initial.side == side);
    const mc_saved_data_class *foreign_class = mc_saved_data_map_class(
        side == MC_WORLD_INFO_CLIENT ? MC_WORLD_INFO_SERVER : MC_WORLD_INFO_CLIENT);
    CHECK(mc_saved_data_map_create(foreign_class, NULL, SIZE_MAX, &foreign, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_put(owner, first, foreign, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_world_saved_data_inspect(owner, &current) == MC_WORLD_STORAGE_OK &&
          graph(&current, false, 0, 0, 0));
    CHECK(mc_world_saved_data_put(owner, first, a, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_put(owner, middle, b, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_put(owner, last, a, &error) == MC_WORLD_STORAGE_OK);
    mc_saved_data_value *before[] = {a, b, a};
    CHECK(ordered(owner, before, 3));
    CHECK(mc_world_saved_data_put(owner, last, c, &error) == MC_WORLD_STORAGE_OK);
    mc_saved_data_value *after_last[] = {b, a, c};
    CHECK(ordered(owner, after_last, 3));
    CHECK(mc_world_saved_data_put(owner, equal, b, &error) == MC_WORLD_STORAGE_OK);
    mc_saved_data_value *after_equal[] = {b, c, b};
    CHECK(ordered(owner, after_equal, 3));
    mc_world_info_name *stored_key = NULL;
    mc_saved_data_value *stored_value = NULL;
    CHECK(mc_world_saved_data_named_at(owner, 2, &stored_key, &stored_value) &&
          stored_key == equal && stored_value == b);
    mc_world_info_name_release(equal);
    equal = NULL;
    CHECK(mc_world_info_name_text(stored_key).length == 4);
    CHECK(mc_world_saved_data_put(owner, NULL, a, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_put(owner, empty, c, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_put(owner, unicode, c, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_get(owner, NULL, unicode_equal, &hit, &error) ==
              MC_WORLD_STORAGE_OK &&
          hit == c);
    mc_saved_data_value_release(hit);
    hit = NULL;
    CHECK(mc_world_saved_data_get(owner, NULL, first, &hit, &error) == MC_WORLD_STORAGE_OK &&
          hit == b);
    mc_saved_data_value_release(hit);
    hit = NULL;
    const mc_saved_data_class *other_class = mc_saved_data_map_class(
        side == MC_WORLD_INFO_CLIENT ? MC_WORLD_INFO_SERVER : MC_WORLD_INFO_CLIENT);
    CHECK(mc_world_saved_data_get(owner, other_class, middle, &hit, &error) ==
              MC_WORLD_STORAGE_OK &&
          hit == b && mc_saved_data_value_class(hit) == mc_saved_data_map_class(side));
    mc_saved_data_value_release(hit);
    hit = NULL;
    CHECK(mc_world_saved_data_get(owner, NULL, NULL, &hit, &error) == MC_WORLD_STORAGE_OK &&
          hit == a);
    mc_saved_data_value_release(hit);
    hit = NULL;
    CHECK(mc_world_saved_data_get(owner, NULL, empty, &hit, &error) == MC_WORLD_STORAGE_OK &&
          hit == c);
    mc_saved_data_value_release(hit);
    hit = NULL;
    CHECK(mc_world_saved_data_get(owner, NULL, missing, &hit, &error) == MC_WORLD_STORAGE_OK &&
          hit == NULL);
    static const uint16_t counter_units[] = {'i', 'd'};
    const mc_nbt_string counter = {counter_units, 2};
    int32_t id = -1;
    CHECK(mc_world_saved_data_next_id(owner, &counter, &id, &error) == MC_WORLD_STORAGE_OK &&
          id == 0);
    CHECK(mc_world_saved_data_inspect(owner, &current) == MC_WORLD_STORAGE_OK &&
          same_graph(&initial, &current) && graph(&current, false, 6, 6, 1));
    CHECK(mc_world_saved_data_put(owner, first, NULL, &error) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_world_saved_data_inspect(owner, &current) == MC_WORLD_STORAGE_OK &&
          graph(&current, false, 6, 6, 1));
    CHECK(mc_saved_data_map_colors_set(a, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_value_mark_dirty(a) == MC_WORLD_STORAGE_OK &&
          mc_saved_data_value_mark_dirty(b) == MC_WORLD_STORAGE_OK &&
          mc_saved_data_value_mark_dirty(c) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_save(owner, &error) == MC_WORLD_STORAGE_OK &&
          !mc_saved_data_value_dirty(a) && !mc_saved_data_value_dirty(b) &&
          !mc_saved_data_value_dirty(c) &&
          mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_OK);
    stored_key = first;
    stored_value = a;
    CHECK(!mc_world_saved_data_named_at(owner, 6, &stored_key, &stored_value) &&
          stored_key == first && stored_value == a);
    CHECK(!mc_world_saved_data_named_at(owner, 0, NULL, &stored_value) && stored_value == a);
    CHECK(mc_world_saved_data_get(owner, NULL, first, &hit, &error) == MC_WORLD_STORAGE_OK &&
          hit == b);
    mc_saved_data_value_release(b);
    b = NULL;
    mc_world_saved_data_free(owner);
    owner = NULL;
    mc_saved_data_map_fields fields;
    CHECK(mc_saved_data_map_fields_get(hit, &fields) == MC_WORLD_STORAGE_OK &&
          fields.type == mc_saved_data_map_class(side) && !fields.dirty && zero_colors(&fields));
    result = 0;
cleanup:
    mc_world_saved_data_free(owner);
    mc_saved_data_value_release(a);
    mc_saved_data_value_release(b);
    mc_saved_data_value_release(c);
    mc_saved_data_value_release(foreign);
    mc_saved_data_value_release(hit);
    mc_world_info_name_release(first);
    mc_world_info_name_release(equal);
    mc_world_info_name_release(middle);
    mc_world_info_name_release(last);
    mc_world_info_name_release(empty);
    mc_world_info_name_release(missing);
    mc_world_info_name_release(unicode);
    mc_world_info_name_release(unicode_equal);
    return result;
}

static int test_capacity(mc_world_info_side side) {
    int result = 1;
    mc_world_saved_data *owner = NULL;
    mc_saved_data_value *a = NULL, *b = NULL, *hit = NULL;
    mc_world_info_name *first = NULL, *second = NULL;
    mc_world_saved_data_limits limits = mc_world_saved_data_default_limits();
    mc_world_saved_data_view view;
    mc_world_storage_error error;
    CHECK(name("a", &first) && name("b", &second));
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), NULL, SIZE_MAX, &a, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), NULL, SIZE_MAX, &b, NULL) ==
          MC_WORLD_STORAGE_OK);
    limits.max_ordered_entries = 1;
    CHECK(mc_world_saved_data_create_for_side(NULL, side, &limits, &owner, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_put(owner, first, a, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_put(owner, second, b, &error) == MC_WORLD_STORAGE_LIMIT);
    CHECK(mc_world_saved_data_inspect(owner, &view) == MC_WORLD_STORAGE_OK &&
          graph(&view, false, 2, 1, 0) && mc_world_saved_data_ordered_at(owner, 0) == a);
    CHECK(mc_world_saved_data_get(owner, NULL, second, &hit, &error) == MC_WORLD_STORAGE_OK &&
          hit == b);
    mc_saved_data_value_release(hit);
    hit = NULL;
    CHECK(mc_saved_data_value_mark_dirty(a) == MC_WORLD_STORAGE_OK &&
          mc_saved_data_value_mark_dirty(b) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_save(owner, &error) == MC_WORLD_STORAGE_OK &&
          !mc_saved_data_value_dirty(a) && mc_saved_data_value_dirty(b));
    CHECK(mc_world_saved_data_put(owner, first, b, &error) == MC_WORLD_STORAGE_OK &&
          mc_world_saved_data_ordered_at(owner, 0) == b &&
          mc_world_saved_data_ordered_at(owner, 1) == NULL);
    CHECK(mc_world_saved_data_save(owner, &error) == MC_WORLD_STORAGE_OK &&
          !mc_saved_data_value_dirty(b));
    mc_world_saved_data_free(owner);
    owner = NULL;
    limits.max_ordered_entries = 0;
    CHECK(mc_world_saved_data_create_for_side(NULL, side, &limits, &owner, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_value_mark_dirty(a) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_put(owner, first, a, &error) == MC_WORLD_STORAGE_LIMIT);
    CHECK(mc_world_saved_data_inspect(owner, &view) == MC_WORLD_STORAGE_OK &&
          graph(&view, false, 1, 0, 0));
    CHECK(mc_world_saved_data_save(owner, &error) == MC_WORLD_STORAGE_OK &&
          mc_saved_data_value_dirty(a));
    mc_world_saved_data_free(owner);
    owner = NULL;
    limits = mc_world_saved_data_default_limits();
    limits.max_named_entries = 1;
    CHECK(mc_world_saved_data_create_for_side(NULL, side, &limits, &owner, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_put(owner, first, a, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_put(owner, second, b, &error) == MC_WORLD_STORAGE_LIMIT);
    CHECK(mc_world_saved_data_inspect(owner, &view) == MC_WORLD_STORAGE_OK &&
          graph(&view, false, 1, 1, 0) && mc_world_saved_data_ordered_at(owner, 0) == a);
    result = 0;
cleanup:
    mc_world_saved_data_free(owner);
    mc_saved_data_value_release(a);
    mc_saved_data_value_release(b);
    mc_saved_data_value_release(hit);
    mc_world_info_name_release(first);
    mc_world_info_name_release(second);
    return result;
}

static int test_roundtrip(const char *root, mc_world_regions *regions, mc_world_info_side side) {
    int result = 1;
    mc_world_store *store = NULL;
    mc_world_saved_data *owner = NULL, *loader = NULL;
    mc_saved_data_value *value = NULL, *loaded = NULL, *hit = NULL;
    mc_saved_data_record *record = NULL;
    mc_world_info_name *lookup = NULL, *disk_name = NULL;
    mc_nbt_byte_array *colors = NULL;
    mc_nbt_document *document = NULL;
    char *path = NULL, *wrong = NULL, *lock = NULL, *ids = NULL;
    mc_ws_file_bytes saved = {0};
    mc_world_storage_error error;
    const uint8_t pixels[] = {5, 9, 11}, marker[] = {0xde, 0xad, 0xbe, 0xef};
    const uint8_t stale[] = {7, 6, 5, 4, 3, 2, 1, 0};
    CHECK(open_store(root, "named-roundtrip", side, regions, &store));
    CHECK(data_path(store, "disk-name", &path) && data_path(store, "lookup-key", &wrong) &&
          data_path(store, "idcounts", &ids));
    CHECK(mc_ws_path_join(mc_world_store_root(store), "session.lock", &lock, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(name("lookup-key", &lookup) && name("disk-name", &disk_name));
    CHECK(mc_world_saved_data_create_for_side(store, side, NULL, &owner, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), disk_name, SIZE_MAX, &value,
                                   NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_nbt_byte_array_create(sizeof(pixels), pixels, SIZE_MAX, &colors) == MC_NBT_OK);
    CHECK(mc_saved_data_map_colors_set(value, colors) == MC_WORLD_STORAGE_OK);
    const mc_saved_data_map_cells *live = mc_saved_data_map_live(value);
    *live->dimension = -1;
    *live->scale = -3;
    *live->x_center = 11;
    *live->z_center = -22;
    CHECK(mc_world_saved_data_put(owner, lookup, value, &error) == MC_WORLD_STORAGE_OK &&
          !mc_saved_data_value_dirty(value));
    CHECK(mc_saved_data_value_mark_dirty(value) == MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_write_file(lock, stale, sizeof(stale), false, &error) == MC_WORLD_STORAGE_OK &&
          mc_world_store_check_session(store, &error) == MC_WORLD_STORAGE_SESSION_MISMATCH);
    CHECK(mc_world_saved_data_save(owner, &error) == MC_WORLD_STORAGE_OK &&
          !mc_saved_data_value_dirty(value));
    CHECK(absent(wrong) && absent(ids) && file_is(lock, stale, sizeof(stale)) &&
          mc_world_store_info(store) == NULL && mc_world_store_metadata_root(store) == NULL);
    CHECK(mc_ws_read_file(path, 1024u * 1024u, &saved, NULL) == MC_WORLD_STORAGE_OK &&
          saved.length > 2);
    CHECK(read_gzip_tree(path, &document));
    const mc_nbt_tag *outer = mc_nbt_document_root(document);
    const mc_nbt_tag *data = child(outer, "data");
    CHECK(outer->type == MC_NBT_COMPOUND && outer->name.length == 0 &&
          outer->value.compound.length == 1 && data != NULL && data->type == MC_NBT_COMPOUND &&
          data->value.compound.length == 7 && child(outer, "Data") == NULL);
    CHECK(child(data, "dimension")->type == MC_NBT_BYTE &&
          child(data, "dimension")->value.byte_value == -1 &&
          child(data, "scale")->value.byte_value == -3 &&
          child(data, "width")->value.short_value == 128 &&
          child(data, "height")->value.short_value == 128 &&
          child(data, "colors")->type == MC_NBT_BYTE_ARRAY &&
          child(data, "colors")->value.byte_array.length == sizeof(pixels) &&
          memcmp(child(data, "colors")->value.byte_array.data, pixels, sizeof(pixels)) == 0);
    mc_nbt_document_free(document);
    document = NULL;
    CHECK(mc_world_saved_data_create_for_side(store, side, NULL, &loader, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_get(loader, NULL, lookup, &hit, &error) == MC_WORLD_STORAGE_OK &&
          hit == NULL);
    CHECK(mc_world_saved_data_get(loader, NULL, disk_name, &hit, &error) == MC_WORLD_STORAGE_OK &&
          hit == NULL &&
          mc_world_saved_data_last_value_error(loader)->exception == MC_SAVED_DATA_NULL_REFERENCE);
    CHECK(mc_world_saved_data_get(loader,
                                  mc_saved_data_map_class(side == MC_WORLD_INFO_CLIENT
                                                              ? MC_WORLD_INFO_SERVER
                                                              : MC_WORLD_INFO_CLIENT),
                                  disk_name, &hit, &error) == MC_WORLD_STORAGE_OK &&
          hit == NULL &&
          mc_world_saved_data_last_value_error(loader)->exception == MC_SAVED_DATA_CLASS_CAST &&
          mc_world_saved_data_last_value_error(loader)->bytecode == 76);
    mc_world_saved_data_view cast_view;
    CHECK(mc_world_saved_data_inspect(loader, &cast_view) == MC_WORLD_STORAGE_OK &&
          graph(&cast_view, true, 0, 0, 0));
    CHECK(mc_world_saved_data_get(loader, mc_saved_data_map_class(side), disk_name, &loaded,
                                  &error) == MC_WORLD_STORAGE_OK &&
          loaded != NULL && loaded != value);
    mc_saved_data_map_fields fields;
    CHECK(mc_saved_data_map_fields_get(loaded, &fields) == MC_WORLD_STORAGE_OK &&
          fields.name == disk_name && fields.type == mc_saved_data_map_class(side) &&
          fields.dimension == -1 && fields.x_center == 11 && fields.z_center == -22 &&
          fields.scale == 0 && !fields.dirty && fields.colors != colors &&
          mc_nbt_byte_array_data(fields.colors) != mc_nbt_byte_array_data(colors) &&
          mc_nbt_byte_array_length(fields.colors) == sizeof(pixels) &&
          memcmp(mc_nbt_byte_array_data(fields.colors), pixels, sizeof(pixels)) == 0);
    CHECK(mc_world_saved_data_get(loader, NULL, disk_name, &hit, &error) == MC_WORLD_STORAGE_OK &&
          hit == loaded);
    mc_saved_data_value_release(hit);
    hit = NULL;
    mc_world_saved_data_free(loader);
    loader = NULL;
    mc_world_store_free(store);
    store = NULL;
    CHECK(mc_ws_write_file(path, marker, sizeof(marker), false, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_save(owner, &error) == MC_WORLD_STORAGE_OK &&
          file_is(path, marker, sizeof(marker)));
    CHECK(mc_ws_write_file(path, saved.data, saved.length, false, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_write(loaded, SIZE_MAX, &record, NULL) == MC_WORLD_STORAGE_OK);
    mc_saved_data_value_release(loaded);
    loaded = NULL;
    mc_nbt_byte_array_data(mc_saved_data_record_colors(record))[1] = 0x88;
    CHECK(child(mc_saved_data_record_root(record, NULL), "colors")->value.byte_array.data[1] ==
          0x88);
    result = 0;
cleanup:
    mc_saved_data_record_release(record);
    mc_world_saved_data_free(owner);
    mc_world_saved_data_free(loader);
    mc_saved_data_value_release(value);
    mc_saved_data_value_release(loaded);
    mc_saved_data_value_release(hit);
    mc_world_info_name_release(lookup);
    mc_world_info_name_release(disk_name);
    mc_nbt_byte_array_release(colors);
    mc_nbt_document_free(document);
    mc_ws_free_file_bytes(&saved);
    free(path);
    free(wrong);
    free(lock);
    free(ids);
    mc_world_store_free(store);
    return result;
}

enum { DIMENSION, CENTER_X, CENTER_Z, SCALE, WIDTH, HEIGHT, COLORS, FIELD_COUNT };

typedef struct map_fixture {
    mc_nbt_tag outer, data, fields[FIELD_COUNT];
    const mc_nbt_tag *outer_items[1], *items[FIELD_COUNT];
    uint16_t names[FIELD_COUNT][9];
} map_fixture;

static void fixture(map_fixture *input) {
    static const char *const names[] = {"dimension", "xCenter", "zCenter", "scale",
                                        "width",     "height",  "colors"};
    static const mc_nbt_type types[] = {MC_NBT_BYTE,  MC_NBT_INT,   MC_NBT_INT,       MC_NBT_BYTE,
                                        MC_NBT_SHORT, MC_NBT_SHORT, MC_NBT_BYTE_ARRAY};
    static const uint16_t data_name[] = {'d', 'a', 't', 'a'};
    static const uint8_t pixels[] = {11, 12, 13};
    *input = (map_fixture){0};
    input->outer.type = input->data.type = MC_NBT_COMPOUND;
    input->outer.value.compound.items = input->outer_items;
    input->outer.value.compound.length = 1;
    input->outer_items[0] = &input->data;
    input->data.name = (mc_nbt_string){data_name, 4};
    input->data.value.compound.items = input->items;
    input->data.value.compound.length = FIELD_COUNT;
    for (size_t i = 0; i < FIELD_COUNT; ++i) {
        size_t length = strlen(names[i]);
        for (size_t j = 0; j < length; ++j)
            input->names[i][j] = (uint16_t)(unsigned char)names[i][j];
        input->fields[i].type = types[i];
        input->fields[i].name = (mc_nbt_string){input->names[i], length};
        input->items[i] = &input->fields[i];
    }
    input->fields[DIMENSION].value.byte_value = -7;
    input->fields[CENTER_X].value.int_value = 21;
    input->fields[CENTER_Z].value.int_value = -22;
    input->fields[SCALE].value.byte_value = 7;
    input->fields[WIDTH].value.short_value = 128;
    input->fields[HEIGHT].value.short_value = 128;
    input->fields[COLORS].value.byte_array.data = pixels;
    input->fields[COLORS].value.byte_array.length = sizeof(pixels);
}

static int test_load_failures(const char *root, mc_world_regions *regions,
                              mc_world_info_side side) {
    int result = 1;
    mc_world_store *store = NULL;
    mc_world_saved_data *owner = NULL;
    mc_saved_data_value *loaded = NULL, *hit = NULL, *sentinel = NULL;
    mc_world_info_name *key = NULL;
    char *path = NULL;
    mc_world_storage_error error;
    mc_world_saved_data_view view;
    CHECK(open_store(root, "named-load-errors", side, regions, &store));
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), NULL, SIZE_MAX, &sentinel,
                                   NULL) == MC_WORLD_STORAGE_OK);
    for (unsigned int mode = 0; mode < 6; ++mode) {
        char stem[32];
        int length = snprintf(stem, sizeof(stem), "case-%u", mode);
        CHECK(length > 0 && (size_t)length < sizeof(stem) && name(stem, &key) &&
              data_path(store, stem, &path));
        map_fixture input;
        fixture(&input);
        if (mode == 0) {
            const uint8_t invalid[] = {1, 2, 3};
            CHECK(mc_ws_write_file(path, invalid, sizeof(invalid), false, NULL) ==
                  MC_WORLD_STORAGE_OK);
        } else if (mode == 1) {
            const uint8_t truncated[] = {10, 0, 0, 1};
            CHECK(write_gzip_raw(path, truncated, sizeof(truncated)));
        } else {
            if (mode == 2) {
                input.data.type = MC_NBT_INT;
                input.data.value.int_value = 7;
            } else if (mode == 3) {
                input.fields[CENTER_Z].type = MC_NBT_STRING;
                input.fields[CENTER_Z].value.string = (mc_nbt_string){NULL, 0};
            } else if (mode == 4) {
                input.fields[WIDTH].value.short_value = 2;
                input.fields[HEIGHT].value.short_value = 2;
            } else {
                static const uint16_t uppercase[] = {'D', 'a', 't', 'a'};
                input.data.name = (mc_nbt_string){uppercase, 4};
            }
            CHECK(write_gzip_tree(path, &input.outer));
        }
        CHECK(mc_world_saved_data_create_for_side(store, side, NULL, &owner, NULL) ==
              MC_WORLD_STORAGE_OK);
        CHECK(mc_world_saved_data_get(owner, mc_saved_data_map_class(side), key, &loaded, &error) ==
                  MC_WORLD_STORAGE_OK &&
              loaded != NULL);
        CHECK(mc_world_saved_data_inspect(owner, &view) == MC_WORLD_STORAGE_OK &&
              graph(&view, true, 1, 1, 0) && mc_world_saved_data_ordered_at(owner, 0) == loaded);
        mc_saved_data_map_fields fields;
        CHECK(mc_saved_data_map_fields_get(loaded, &fields) == MC_WORLD_STORAGE_OK &&
              fields.name == key && !fields.dirty);
        if (mode < 3 || mode == 5) {
            CHECK(fields.dimension == 0 && fields.x_center == 0 && fields.z_center == 0 &&
                  fields.scale == 0 && zero_colors(&fields));
        } else if (mode == 3) {
            CHECK(fields.dimension == -7 && fields.x_center == 21 && fields.z_center == 0 &&
                  fields.scale == 0 && zero_colors(&fields));
            const mc_saved_data_error *value_error = mc_world_saved_data_last_value_error(owner);
            CHECK(value_error->exception == MC_SAVED_DATA_CLASS_CAST &&
                  value_error->bytecode == 24 && strcmp(value_error->field, "zCenter") == 0);
        } else {
            CHECK(fields.dimension == -7 && fields.x_center == 21 && fields.z_center == -22 &&
                  fields.scale == 4 && fields.colors != NULL &&
                  mc_nbt_byte_array_length(fields.colors) == 16384);
            const uint8_t *pixels = mc_nbt_byte_array_data(fields.colors);
            for (size_t i = 0; i < 16384; ++i)
                CHECK(pixels[i] == (i == 8127 ? 11 : i == 8128 ? 12 : i == 8255 ? 13 : 0));
            const mc_saved_data_error *value_error = mc_world_saved_data_last_value_error(owner);
            CHECK(value_error->exception == MC_SAVED_DATA_ARRAY_BOUNDS &&
                  value_error->bytecode == 227 && value_error->index == 3 &&
                  value_error->length == 3);
        }
        CHECK(mc_world_saved_data_last_caught(owner)->status ==
              (mode == 5 ? MC_WORLD_STORAGE_OK : MC_WORLD_STORAGE_INVALID_DATA));
        if (mode == 2)
            CHECK(mc_world_saved_data_last_value_error(owner)->exception ==
                      MC_SAVED_DATA_CLASS_CAST &&
                  mc_world_saved_data_last_value_error(owner)->bytecode == 145);
        CHECK(mc_world_saved_data_get(owner, NULL, key, &hit, &error) == MC_WORLD_STORAGE_OK &&
              hit == loaded);
        mc_saved_data_value_release(hit);
        hit = NULL;
        mc_world_saved_data_free(owner);
        owner = NULL;
        CHECK(mc_saved_data_map_fields_get(loaded, &fields) == MC_WORLD_STORAGE_OK &&
              fields.colors != NULL);
        mc_saved_data_value_release(loaded);
        loaded = NULL;
        mc_world_info_name_release(key);
        key = NULL;
        free(path);
        path = NULL;
    }
    CHECK(name("escaping-limit", &key) && data_path(store, "escaping-limit", &path));
    map_fixture input;
    fixture(&input);
    CHECK(write_gzip_tree(path, &input.outer));
    mc_world_saved_data_limits limits = mc_world_saved_data_default_limits();
    limits.nbt.max_tags = 0;
    CHECK(mc_world_saved_data_create_for_side(store, side, &limits, &owner, NULL) ==
          MC_WORLD_STORAGE_OK);
    hit = sentinel;
    CHECK(mc_world_saved_data_get(owner, mc_saved_data_map_class(side), key, &hit, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          hit == sentinel);
    hit = NULL;
    CHECK(mc_world_saved_data_inspect(owner, &view) == MC_WORLD_STORAGE_OK &&
          graph(&view, true, 0, 0, 0) &&
          mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_OK);
    mc_world_saved_data_free(owner);
    owner = NULL;
    limits = mc_world_saved_data_default_limits();
    limits.max_ordered_entries = 0;
    CHECK(mc_world_saved_data_create_for_side(store, side, &limits, &owner, NULL) ==
          MC_WORLD_STORAGE_OK);
    hit = sentinel;
    CHECK(mc_world_saved_data_get(owner, mc_saved_data_map_class(side), key, &hit, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          hit == sentinel);
    hit = NULL;
    CHECK(mc_world_saved_data_inspect(owner, &view) == MC_WORLD_STORAGE_OK &&
          graph(&view, true, 1, 0, 0));
    CHECK(mc_world_saved_data_get(owner, NULL, key, &loaded, &error) == MC_WORLD_STORAGE_OK &&
          loaded != NULL);
    result = 0;
cleanup:
    if (hit != sentinel)
        mc_saved_data_value_release(hit);
    mc_world_saved_data_free(owner);
    mc_saved_data_value_release(loaded);
    mc_saved_data_value_release(sentinel);
    mc_world_info_name_release(key);
    free(path);
    mc_world_store_free(store);
    return result;
}

static int test_save_failures(const char *root, mc_world_regions *regions,
                              mc_world_info_side side) {
    int result = 1;
    mc_world_store *store = NULL;
    mc_world_saved_data *owner = NULL, *limited = NULL;
    mc_saved_data_value *io_value = NULL, *null_value = NULL, *limit_value = NULL,
                        *later_value = NULL;
    mc_world_info_name *io_name = NULL, *null_name = NULL, *limit_name = NULL, *later_name = NULL;
    char *io_path = NULL, *null_path = NULL, *limit_path = NULL, *later_path = NULL;
    mc_world_storage_error error, caught;
    const uint8_t marker[] = {0x31, 0x41, 0x59};
    CHECK(open_store(root, "named-save-errors", side, regions, &store));
    CHECK(name("directory", &io_name) && name("null-array", &null_name) &&
          name("limit", &limit_name) && name("later", &later_name));
    CHECK(data_path(store, "directory", &io_path) && data_path(store, "null-array", &null_path) &&
          data_path(store, "limit", &limit_path) && data_path(store, "later", &later_path));
    CHECK(mc_ws_mkdirs(io_path, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), io_name, SIZE_MAX, &io_value,
                                   NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), null_name, SIZE_MAX, &null_value,
                                   NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_create_for_side(store, side, NULL, &owner, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_put(owner, io_name, io_value, NULL) == MC_WORLD_STORAGE_OK &&
          mc_saved_data_value_mark_dirty(io_value) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_save(owner, &error) == MC_WORLD_STORAGE_OK &&
          !mc_saved_data_value_dirty(io_value) &&
          mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_IO_ERROR);
    CHECK(mc_ws_write_file(null_path, marker, sizeof(marker), false, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_colors_set(null_value, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_saved_data_put(owner, null_name, null_value, NULL) == MC_WORLD_STORAGE_OK &&
          mc_saved_data_value_mark_dirty(null_value) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_save(owner, &error) == MC_WORLD_STORAGE_OK &&
          !mc_saved_data_value_dirty(null_value) && file_is(null_path, NULL, 0));
    CHECK(mc_world_saved_data_last_caught(owner)->status == MC_WORLD_STORAGE_INVALID_DATA &&
          mc_world_saved_data_last_value_error(owner)->exception == MC_SAVED_DATA_NULL_REFERENCE);
    caught = *mc_world_saved_data_last_caught(owner);
    CHECK(mc_world_saved_data_save(owner, &error) == MC_WORLD_STORAGE_OK &&
          file_is(null_path, NULL, 0) &&
          mc_world_saved_data_last_caught(owner)->status == caught.status &&
          mc_world_saved_data_last_caught(owner)->operation == caught.operation &&
          mc_world_saved_data_last_caught(owner)->system_error == caught.system_error);
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), limit_name, SIZE_MAX,
                                   &limit_value, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), later_name, SIZE_MAX,
                                   &later_value, NULL) == MC_WORLD_STORAGE_OK);
    mc_world_saved_data_limits limits = mc_world_saved_data_default_limits();
    limits.nbt.max_output_bytes = 0;
    CHECK(mc_world_saved_data_create_for_side(store, side, &limits, &limited, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_put(limited, limit_name, limit_value, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_saved_data_put(limited, later_name, later_value, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_ws_write_file(limit_path, marker, sizeof(marker), false, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_ws_write_file(later_path, marker, sizeof(marker), false, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_value_mark_dirty(limit_value) == MC_WORLD_STORAGE_OK &&
          mc_saved_data_value_mark_dirty(later_value) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_saved_data_save(limited, &error) == MC_WORLD_STORAGE_LIMIT &&
          error.status == MC_WORLD_STORAGE_LIMIT && mc_saved_data_value_dirty(limit_value) &&
          mc_saved_data_value_dirty(later_value));
    CHECK(file_is(limit_path, NULL, 0) && file_is(later_path, marker, sizeof(marker)) &&
          mc_world_saved_data_last_caught(limited)->status == MC_WORLD_STORAGE_OK);
    result = 0;
cleanup:
    mc_world_saved_data_free(owner);
    mc_world_saved_data_free(limited);
    mc_saved_data_value_release(io_value);
    mc_saved_data_value_release(null_value);
    mc_saved_data_value_release(limit_value);
    mc_saved_data_value_release(later_value);
    mc_world_info_name_release(io_name);
    mc_world_info_name_release(null_name);
    mc_world_info_name_release(limit_name);
    mc_world_info_name_release(later_name);
    free(io_path);
    free(null_path);
    free(limit_path);
    free(later_path);
    mc_world_store_free(store);
    return result;
}

static bool decode_path(const char *hex, char *out, size_t capacity) {
    size_t length = strlen(hex);
    if (length == 0 || length % 2u != 0 || length / 2u >= capacity)
        return false;
    for (size_t i = 0; i < length / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            unsigned char c = (unsigned char)hex[2u * i + j];
            unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                 : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                        : 16u;
            if (digit > 15u)
                return false;
            value = value * 16u + digit;
        }
        if (value == 0)
            return false;
        out[i] = (char)(unsigned char)value;
    }
    out[length / 2u] = '\0';
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    char root[8192];
    mc_world_regions *regions = NULL;
    if (argc != 3 || strcmp(argv[1], "saved-data-named") != 0 ||
        !decode_path(argv[2], root, sizeof(root)))
        return 1;
    if (mc_world_regions_create(NULL, &regions, NULL) != MC_WORLD_STORAGE_OK)
        return 1;
    const mc_world_info_side sides[] = {MC_WORLD_INFO_CLIENT, MC_WORLD_INFO_SERVER};
    for (size_t i = 0; i < sizeof(sides) / sizeof(sides[0]); ++i)
        if (test_memory(sides[i]) || test_capacity(sides[i]) ||
            test_roundtrip(root, regions, sides[i]) ||
            test_load_failures(root, regions, sides[i]) ||
            test_save_failures(root, regions, sides[i]))
            goto cleanup;
    printf("PASS: %u native constructed named SavedData checks (saved-data-named)\n", checks);
    result = 0;
cleanup:
    mc_world_regions_free(regions);
    return result;
}
