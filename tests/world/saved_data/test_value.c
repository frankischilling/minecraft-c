#include "mc/world/saved_data/value.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Native constructed ownership and source-order cases. These tests do not
 * execute an original MapData class or qualify original/native behavior. */
static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

enum { DIMENSION, CENTER_X, CENTER_Z, SCALE, WIDTH, HEIGHT, COLORS, FIELD_COUNT };

static const char *const field_names[FIELD_COUNT] = {"dimension", "xCenter", "zCenter", "scale",
                                                     "width",     "height",  "colors"};
static const mc_nbt_type field_types[FIELD_COUNT] = {MC_NBT_BYTE,      MC_NBT_INT,   MC_NBT_INT,
                                                     MC_NBT_BYTE,      MC_NBT_SHORT, MC_NBT_SHORT,
                                                     MC_NBT_BYTE_ARRAY};

typedef struct input_fixture {
    mc_nbt_tag root, fields[FIELD_COUNT];
    const mc_nbt_tag *items[FIELD_COUNT];
    uint16_t names[FIELD_COUNT][9];
} input_fixture;

static void fixture(input_fixture *input, int16_t width, int16_t height,
                    mc_nbt_byte_array *colors) {
    *input = (input_fixture){0};
    input->root.type = MC_NBT_COMPOUND;
    input->root.value.compound.items = input->items;
    input->root.value.compound.length = FIELD_COUNT;
    for (size_t i = 0; i < FIELD_COUNT; ++i) {
        size_t length = strlen(field_names[i]);
        for (size_t j = 0; j < length; ++j)
            input->names[i][j] = (uint16_t)(unsigned char)field_names[i][j];
        input->fields[i].type = field_types[i];
        input->fields[i].name = (mc_nbt_string){input->names[i], length};
        input->items[i] = &input->fields[i];
    }
    input->fields[DIMENSION].value.byte_value = 2;
    input->fields[CENTER_X].value.int_value = 3;
    input->fields[CENTER_Z].value.int_value = 4;
    input->fields[SCALE].value.byte_value = 3;
    input->fields[WIDTH].value.short_value = width;
    input->fields[HEIGHT].value.short_value = height;
    input->fields[COLORS].value.byte_array.data = mc_nbt_byte_array_data(colors);
    input->fields[COLORS].value.byte_array.length = mc_nbt_byte_array_length(colors);
}

static uint8_t pattern(size_t index) {
    return (uint8_t)(index % 251u + 1u);
}

static void fill(mc_nbt_byte_array *array) {
    for (size_t i = 0; i < mc_nbt_byte_array_length(array); ++i)
        mc_nbt_byte_array_data(array)[i] = pattern(i);
}

static bool zero_array(const mc_nbt_byte_array *array, size_t length) {
    if (array == NULL || mc_nbt_byte_array_length(array) != length)
        return false;
    for (size_t i = 0; i < length; ++i)
        if (mc_nbt_byte_array_data(array)[i] != 0)
            return false;
    return true;
}

static bool collections(const mc_saved_data_map_fields *fields) {
    return fields->trackers != NULL && fields->decorations != NULL &&
           fields->player_trackers != NULL && fields->trackers != fields->decorations &&
           fields->trackers != fields->player_trackers &&
           fields->decorations != fields->player_trackers && fields->tracker_count == 0 &&
           fields->decoration_count == 0 && fields->player_tracker_count == 0;
}

static bool same_collections(const mc_saved_data_map_fields *left,
                             const mc_saved_data_map_fields *right) {
    return collections(right) && left->trackers == right->trackers &&
           left->decorations == right->decorations &&
           left->player_trackers == right->player_trackers;
}

static const mc_nbt_tag *child(const mc_nbt_tag *root, const char *name) {
    uint16_t units[9];
    size_t length = strlen(name);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)name[i];
    return mc_nbt_compound_get(root, units, length);
}

static bool error_ok(const mc_saved_data_error *error) {
    return error->cause.status == MC_WORLD_STORAGE_OK && error->cause.operation == NULL &&
           error->cause.system_error == 0 && error->exception == MC_SAVED_DATA_NO_EXCEPTION &&
           error->bytecode == 0 && error->getter_bytecode == 0 && error->field == NULL &&
           error->index == 0 && error->length == 0;
}

static bool document_from(const mc_nbt_tag *root, mc_nbt_document **out) {
    size_t length = 0, written = 0;
    if (mc_nbt_measure(root, NULL, &length, NULL) != MC_NBT_OK)
        return false;
    uint8_t *bytes = malloc(length);
    if (bytes == NULL)
        return false;
    bool okay = mc_nbt_write(root, NULL, bytes, length, &written, NULL) == MC_NBT_OK &&
                written == length &&
                mc_nbt_read_compound(bytes, written, NULL, out, NULL, NULL) == MC_NBT_OK;
    free(bytes);
    return okay;
}

static bool exact_budget_document(const mc_nbt_tag *root, mc_nbt_document **out) {
    size_t length = 0, written = 0;
    if (mc_nbt_measure(root, NULL, &length, NULL) != MC_NBT_OK)
        return false;
    uint8_t *bytes = malloc(length);
    if (bytes == NULL)
        return false;
    bool okay =
        mc_nbt_write(root, NULL, bytes, length, &written, NULL) == MC_NBT_OK && written == length;
    size_t low = 0, high = 65536;
    mc_nbt_limits limits = mc_nbt_default_limits();
    while (okay && low < high) {
        size_t middle = low + (high - low) / 2u;
        limits.max_allocation_bytes = middle;
        mc_nbt_document *trial = NULL;
        mc_nbt_status status = mc_nbt_read_compound(bytes, written, &limits, &trial, NULL, NULL);
        if (status == MC_NBT_OK) {
            high = middle;
            mc_nbt_document_free(trial);
        } else if (status == MC_NBT_LIMIT) {
            low = middle + 1u;
        } else {
            okay = false;
        }
    }
    limits.max_allocation_bytes = low;
    if (okay)
        okay = mc_nbt_read_compound(bytes, written, &limits, out, NULL, NULL) == MC_NBT_OK;
    free(bytes);
    return okay;
}

static int test_construction(mc_world_info_side side) {
    int result = 1;
    mc_saved_data_value *value = NULL, *peer = NULL, *null_name = NULL, *empty_name = NULL;
    mc_world_info_name *name = NULL, *empty = NULL;
    mc_saved_data_map_fields initial, other, current;
    mc_saved_data_error error;
    static const uint16_t text[] = {'m', 0, 0xd800, 0xdc00};
    const mc_saved_data_class *type = mc_saved_data_map_class(side);
    mc_world_info_side selected = side;
    CHECK(type != NULL && mc_saved_data_class_side(type, &selected) && selected == side);
    CHECK(mc_saved_data_map_class((mc_world_info_side)99) == NULL);
    CHECK(!mc_saved_data_class_side(NULL, &selected) && selected == side);
    CHECK(!mc_saved_data_class_side(type, NULL));
    CHECK(mc_world_info_name_copy((mc_nbt_string){text, 4}, SIZE_MAX, &name) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){NULL, 0}, SIZE_MAX, &empty) == MC_WORLD_INFO_OK);
    CHECK(mc_saved_data_map_create(type, name, SIZE_MAX, &value, &error) == MC_WORLD_STORAGE_OK &&
          error_ok(&error));
    CHECK(mc_saved_data_map_create(type, name, SIZE_MAX, &peer, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_create(type, NULL, SIZE_MAX, &null_name, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_create(type, empty, SIZE_MAX, &empty_name, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_fields_get(value, &initial) == MC_WORLD_STORAGE_OK &&
          collections(&initial));
    CHECK(mc_saved_data_map_fields_get(peer, &other) == MC_WORLD_STORAGE_OK && collections(&other));
    CHECK(initial.type == type && initial.name == name && !initial.dirty &&
          initial.dimension == 0 && initial.x_center == 0 && initial.z_center == 0 &&
          initial.scale == 0 && initial.updates == 0);
    CHECK(zero_array(initial.colors, 16384) && zero_array(other.colors, 16384) &&
          initial.colors != other.colors);
    CHECK(initial.trackers != other.trackers && initial.decorations != other.decorations &&
          initial.player_trackers != other.player_trackers && other.name == name);
    CHECK(mc_saved_data_value_name(null_name) == NULL &&
          mc_saved_data_value_name(empty_name) == empty && empty != NULL);
    mc_world_info_name_release(name);
    name = NULL;
    CHECK(mc_saved_data_value_name(value) == initial.name &&
          mc_world_info_name_text(initial.name).length == 4 &&
          memcmp(mc_world_info_name_text(initial.name).units, text, sizeof(text)) == 0);
    CHECK(mc_saved_data_value_mark_dirty(value) == MC_WORLD_STORAGE_OK &&
          mc_saved_data_value_dirty(value) && !mc_saved_data_value_dirty(peer));
    CHECK(mc_saved_data_value_dirty_set(value, false) == MC_WORLD_STORAGE_OK &&
          !mc_saved_data_value_dirty(value));
    const mc_saved_data_map_cells *live = mc_saved_data_map_live(value);
    CHECK(live != NULL && live != mc_saved_data_map_live(peer));
    *live->x_center = INT32_MIN;
    *live->z_center = INT32_MAX;
    *live->updates = -17;
    *live->dimension = INT8_MIN;
    *live->scale = INT8_MAX;
    CHECK(mc_saved_data_map_fields_get(value, &current) == MC_WORLD_STORAGE_OK &&
          current.x_center == INT32_MIN && current.z_center == INT32_MAX &&
          current.updates == -17 && current.dimension == INT8_MIN && current.scale == INT8_MAX &&
          same_collections(&initial, &current));
    CHECK(mc_saved_data_map_colors_set(value, initial.colors) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_colors_set(peer, initial.colors) == MC_WORLD_STORAGE_OK &&
          mc_saved_data_map_fields_get(peer, &other) == MC_WORLD_STORAGE_OK &&
          other.colors == initial.colors);
    CHECK(mc_saved_data_value_retain(value) == MC_WORLD_STORAGE_OK);
    mc_saved_data_value_release(value);
    CHECK(mc_saved_data_value_class(value) == type);
    mc_saved_data_value *candidate = value;
    CHECK(mc_saved_data_map_create(type, initial.name, 0, &candidate, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          candidate == value);
    CHECK(mc_saved_data_map_create(type, initial.name,
                                   mc_nbt_byte_array_allocation_bytes(initial.colors), &candidate,
                                   &error) == MC_WORLD_STORAGE_LIMIT &&
          candidate == value);
    CHECK(mc_saved_data_map_create(NULL, NULL, SIZE_MAX, &candidate, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          candidate == value);
    CHECK(mc_saved_data_map_create(type, NULL, SIZE_MAX, NULL, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    current = initial;
    CHECK(mc_saved_data_map_fields_get(NULL, &current) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          current.colors == initial.colors && current.name == initial.name &&
          collections(&current));
    CHECK(mc_saved_data_map_fields_get(value, NULL) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_saved_data_value_retain(NULL) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          mc_saved_data_value_dirty_set(NULL, true) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          mc_saved_data_value_mark_dirty(NULL) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          mc_saved_data_map_colors_set(NULL, NULL) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_saved_data_value_name(NULL) == NULL && mc_saved_data_value_class(NULL) == NULL &&
          !mc_saved_data_value_dirty(NULL) && mc_saved_data_map_live(NULL) == NULL);
    mc_saved_data_value_release(NULL);
    result = 0;
cleanup:
    mc_saved_data_value_release(value);
    mc_saved_data_value_release(peer);
    mc_saved_data_value_release(null_name);
    mc_saved_data_value_release(empty_name);
    mc_world_info_name_release(name);
    mc_world_info_name_release(empty);
    return result;
}

static int test_standard_document(mc_world_info_side side) {
    int result = 1;
    mc_saved_data_value *value = NULL;
    mc_saved_data_record *record = NULL;
    mc_nbt_byte_array *source = NULL, *decoded = NULL, *old = NULL;
    mc_nbt_document *document = NULL;
    mc_saved_data_error error;
    mc_saved_data_map_fields before, after;
    static const size_t lengths[] = {0, 1, 3, 16383, 16384, 16385};
    for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), NULL, SIZE_MAX, &value,
                                       NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_saved_data_map_fields_get(value, &before) == MC_WORLD_STORAGE_OK);
        CHECK(mc_nbt_byte_array_retain(before.colors) == MC_NBT_OK);
        old = before.colors;
        CHECK(mc_saved_data_value_mark_dirty(value) == MC_WORLD_STORAGE_OK);
        *mc_saved_data_map_live(value)->updates = 41;
        CHECK(mc_nbt_byte_array_create(lengths[i], NULL, SIZE_MAX, &source) == MC_NBT_OK);
        fill(source);
        input_fixture input;
        fixture(&input, 128, 128, source);
        CHECK(document_from(&input.root, &document));
        mc_nbt_byte_array_release(source);
        source = NULL;
        const mc_nbt_tag *tag = child(mc_nbt_document_root(document), "colors");
        CHECK(tag != NULL && tag->value.byte_array.length == lengths[i]);
        CHECK(mc_saved_data_map_read_document(value, document, mc_nbt_document_root(document), 0,
                                              &error) == MC_WORLD_STORAGE_OK &&
              error_ok(&error));
        CHECK(mc_nbt_document_byte_array(document, tag, &decoded) == MC_NBT_OK);
        CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
              after.colors == decoded && after.colors != old && after.dirty &&
              after.updates == 41 && same_collections(&before, &after));
        CHECK(mc_nbt_byte_array_data(decoded) == tag->value.byte_array.data &&
              mc_nbt_byte_array_length(decoded) == lengths[i] && zero_array(old, 16384));
        CHECK(mc_saved_data_map_read_document(value, document, mc_nbt_document_root(document), 0,
                                              NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
              after.colors == decoded);
        CHECK(mc_saved_data_map_write(value, SIZE_MAX, &record, &error) == MC_WORLD_STORAGE_OK &&
              error_ok(&error));
        CHECK(mc_saved_data_record_colors(record) == decoded);
        const mc_nbt_tag *root = mc_saved_data_record_root(record, &error);
        CHECK(root != NULL && error_ok(&error) && root->value.compound.length == 7 &&
              child(root, "width")->value.short_value == 128 &&
              child(root, "height")->value.short_value == 128 &&
              child(root, "colors")->value.byte_array.data == tag->value.byte_array.data &&
              child(root, "colors")->value.byte_array.length == lengths[i]);
        *mc_saved_data_map_live(value)->scale = 127;
        *mc_saved_data_map_live(value)->x_center = -90;
        CHECK(child(root, "scale")->value.byte_value == 3 &&
              child(root, "xCenter")->value.int_value == 3);
        CHECK(mc_saved_data_map_colors_set(value, NULL) == MC_WORLD_STORAGE_OK);
        mc_nbt_document_free(document);
        document = NULL;
        mc_nbt_byte_array_release(decoded);
        decoded = NULL;
        mc_saved_data_value_release(value);
        value = NULL;
        if (lengths[i] != 0) {
            mc_nbt_byte_array_data(mc_saved_data_record_colors(record))[lengths[i] - 1u] = 0xa5;
            CHECK(tag->value.byte_array.data[lengths[i] - 1u] == 0xa5 &&
                  child(root, "colors")->value.byte_array.data[lengths[i] - 1u] == 0xa5);
        }
        size_t measured = 0;
        CHECK(mc_saved_data_record_root(record, &error) == root &&
              mc_nbt_measure(root, NULL, &measured, NULL) == MC_NBT_OK && measured > lengths[i]);
        mc_saved_data_record_release(record);
        record = NULL;
        mc_nbt_byte_array_release(old);
        old = NULL;
    }
    result = 0;
cleanup:
    mc_saved_data_record_release(record);
    mc_saved_data_value_release(value);
    mc_nbt_byte_array_release(source);
    mc_nbt_byte_array_release(decoded);
    mc_nbt_byte_array_release(old);
    mc_nbt_document_free(document);
    return result;
}

static int test_numbers_and_failures(mc_world_info_side side) {
    int result = 1;
    mc_saved_data_value *value = NULL;
    mc_nbt_byte_array *source = NULL;
    mc_saved_data_error error;
    mc_saved_data_map_fields initial, after;
    input_fixture input;
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), NULL, SIZE_MAX, &value, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_nbt_byte_array_create(1, NULL, SIZE_MAX, &source) == MC_NBT_OK);
    CHECK(mc_saved_data_value_mark_dirty(value) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_fields_get(value, &initial) == MC_WORLD_STORAGE_OK);
    *mc_saved_data_map_live(value)->updates = 19;
    static const int8_t scales[] = {-128, -1, 0, 1, 4, 5, 127};
    static const int8_t expected[] = {0, 0, 0, 1, 4, 4, 4};
    for (size_t i = 0; i < sizeof(scales) / sizeof(scales[0]); ++i) {
        fixture(&input, 128, 128, source);
        input.fields[DIMENSION].value.byte_value = INT8_MIN;
        input.fields[CENTER_X].value.int_value = INT32_MIN;
        input.fields[CENTER_Z].value.int_value = INT32_MAX;
        input.fields[SCALE].value.byte_value = scales[i];
        CHECK(mc_saved_data_map_read(value, &input.root, source, 0, &error) ==
                  MC_WORLD_STORAGE_OK &&
              error_ok(&error));
        CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
              after.dimension == INT8_MIN && after.x_center == INT32_MIN &&
              after.z_center == INT32_MAX && after.scale == expected[i] && after.colors == source &&
              after.updates == 19 && after.dirty && same_collections(&initial, &after));
    }
    static const unsigned int pcs[] = {4, 14, 24, 34, 68, 75, 97};
    for (size_t i = 0; i < FIELD_COUNT; ++i) {
        const mc_saved_data_map_cells *live = mc_saved_data_map_live(value);
        *live->dimension = -6;
        *live->x_center = -9;
        *live->z_center = -8;
        *live->scale = -5;
        fixture(&input, 128, 128, source);
        input.fields[SCALE].value.byte_value = 7;
        input.fields[i].type = MC_NBT_LONG;
        CHECK(mc_saved_data_map_read(value, &input.root, source, SIZE_MAX, &error) ==
              MC_WORLD_STORAGE_INVALID_DATA);
        CHECK(error.exception == MC_SAVED_DATA_CLASS_CAST && error.bytecode == pcs[i] &&
              error.getter_bytecode == (i == COLORS ? 27u : 25u) &&
              strcmp(error.field, field_names[i]) == 0);
        CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
              after.dimension == (i > DIMENSION ? 2 : -6) &&
              after.x_center == (i > CENTER_X ? 3 : -9) &&
              after.z_center == (i > CENTER_Z ? 4 : -8) && after.scale == (i > SCALE ? 4 : -5) &&
              after.colors == source && after.updates == 19 && after.dirty &&
              same_collections(&initial, &after));
    }
    CHECK(mc_saved_data_map_read(value, NULL, source, SIZE_MAX, &error) ==
              MC_WORLD_STORAGE_INVALID_DATA &&
          error.exception == MC_SAVED_DATA_NULL_REFERENCE && error.bytecode == 4);
    fixture(&input, 128, 128, source);
    input.root.type = MC_NBT_INT;
    CHECK(mc_saved_data_map_read(value, &input.root, source, SIZE_MAX, &error) ==
              MC_WORLD_STORAGE_INVALID_DATA &&
          error.exception == MC_SAVED_DATA_CLASS_CAST && error.bytecode == 4);
    fixture(&input, 128, 128, source);
    CHECK(mc_saved_data_map_read(value, &input.root, NULL, SIZE_MAX, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          error.bytecode == 97 && error.exception == MC_SAVED_DATA_NO_EXCEPTION);
    CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
          after.dimension == 2 && after.x_center == 3 && after.z_center == 4 && after.scale == 3 &&
          after.colors == source);
    input.root.value.compound.length = 0;
    CHECK(mc_saved_data_map_read(value, &input.root, source, SIZE_MAX, &error) ==
              MC_WORLD_STORAGE_OK &&
          error_ok(&error));
    CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
          after.dimension == 0 && after.x_center == 0 && after.z_center == 0 && after.scale == 0 &&
          zero_array(after.colors, 16384) && after.updates == 19 && after.dirty &&
          same_collections(&initial, &after));
    result = 0;
cleanup:
    mc_saved_data_value_release(value);
    mc_nbt_byte_array_release(source);
    return result;
}

static int test_decoded_getter_order(mc_world_info_side side) {
    int result = 1;
    mc_saved_data_value *value = NULL;
    mc_nbt_byte_array *source = NULL;
    mc_nbt_document *document = NULL;
    mc_saved_data_map_fields before, after;
    mc_saved_data_error error;
    input_fixture input;
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), NULL, SIZE_MAX, &value, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_fields_get(value, &before) == MC_WORLD_STORAGE_OK);
    CHECK(mc_nbt_byte_array_create(1, NULL, SIZE_MAX, &source) == MC_NBT_OK);
    fixture(&input, 128, 128, source);
    input.fields[SCALE].value.byte_value = 127;
    CHECK(exact_budget_document(&input.root, &document));
    CHECK(mc_saved_data_map_read_document(value, document, mc_nbt_document_root(document), SIZE_MAX,
                                          &error) == MC_WORLD_STORAGE_LIMIT &&
          error.bytecode == 97 && error.exception == MC_SAVED_DATA_NO_EXCEPTION);
    CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
          after.colors == before.colors && after.dimension == 2 && after.x_center == 3 &&
          after.z_center == 4 && after.scale == 4);
    mc_nbt_document_free(document);
    document = NULL;
    fixture(&input, 128, 128, source);
    input.fields[SCALE].value.byte_value = -128;
    input.fields[WIDTH].type = MC_NBT_INT;
    input.fields[WIDTH].value.int_value = 128;
    CHECK(exact_budget_document(&input.root, &document));
    CHECK(mc_saved_data_map_read_document(value, document, mc_nbt_document_root(document), SIZE_MAX,
                                          &error) == MC_WORLD_STORAGE_INVALID_DATA &&
          error.bytecode == 68 && error.getter_bytecode == 25 &&
          error.exception == MC_SAVED_DATA_CLASS_CAST && strcmp(error.field, "width") == 0);
    CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
          after.colors == before.colors && after.dimension == 2 && after.x_center == 3 &&
          after.z_center == 4 && after.scale == 0 && same_collections(&before, &after));
    result = 0;
cleanup:
    mc_saved_data_value_release(value);
    mc_nbt_byte_array_release(source);
    mc_nbt_document_free(document);
    return result;
}

typedef enum pixel_result {
    NO_PIXELS,
    CENTER_FOUR,
    CENTER_THREE,
    ROW_129,
    ROW_256,
    COLUMN_128
} pixel_result;

static int test_resize(mc_world_info_side side) {
    static const struct {
        int16_t width, height;
        size_t length;
        bool null_source;
        mc_saved_data_exception exception;
        unsigned int pc;
        int32_t index;
        pixel_result pixels;
    } cases[] = {{2, 2, 4, false, MC_SAVED_DATA_NO_EXCEPTION, 0, 0, CENTER_FOUR},
                 {2, 2, 3, false, MC_SAVED_DATA_ARRAY_BOUNDS, 227, 3, CENTER_THREE},
                 {130, 128, 16640, false, MC_SAVED_DATA_ARRAY_BOUNDS, 228, -1, NO_PIXELS},
                 {130, 128, 0, false, MC_SAVED_DATA_ARRAY_BOUNDS, 227, 0, NO_PIXELS},
                 {130, 128, 0, true, MC_SAVED_DATA_NULL_REFERENCE, 227, 0, NO_PIXELS},
                 {128, 130, 1, false, MC_SAVED_DATA_ARRAY_BOUNDS, 228, -128, NO_PIXELS},
                 {1, 129, 129, false, MC_SAVED_DATA_ARRAY_BOUNDS, 228, 16447, COLUMN_128},
                 {1, 129, 128, false, MC_SAVED_DATA_ARRAY_BOUNDS, 227, 128, COLUMN_128},
                 {129, 1, 129, false, MC_SAVED_DATA_NO_EXCEPTION, 0, 0, ROW_129},
                 {256, 1, 256, false, MC_SAVED_DATA_NO_EXCEPTION, 0, 0, ROW_256},
                 {1, 1, 0, true, MC_SAVED_DATA_NULL_REFERENCE, 227, 0, NO_PIXELS},
                 {0, 32767, 0, true, MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NO_PIXELS},
                 {32767, 0, 0, true, MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NO_PIXELS},
                 {-1, 1, 0, true, MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NO_PIXELS},
                 {1, -1, 0, true, MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NO_PIXELS},
                 {INT16_MIN, INT16_MIN, 0, true, MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NO_PIXELS}};

    int result = 1;
    mc_saved_data_value *value = NULL;
    mc_nbt_byte_array *source = NULL, *old = NULL;
    uint8_t *expected = calloc(16384, 1);
    mc_saved_data_map_fields before, after;
    mc_saved_data_error error;
    CHECK(expected != NULL);
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), NULL, SIZE_MAX, &value,
                                       NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_saved_data_map_fields_get(value, &before) == MC_WORLD_STORAGE_OK);
        CHECK(mc_nbt_byte_array_retain(before.colors) == MC_NBT_OK);
        old = before.colors;
        mc_nbt_byte_array_data(old)[0] = 0x77;
        CHECK(mc_saved_data_value_mark_dirty(value) == MC_WORLD_STORAGE_OK);
        *mc_saved_data_map_live(value)->updates = 83;
        if (!cases[i].null_source) {
            CHECK(mc_nbt_byte_array_create(cases[i].length, NULL, SIZE_MAX, &source) == MC_NBT_OK);
            fill(source);
        }
        input_fixture input;
        fixture(&input, cases[i].width, cases[i].height, source);
        mc_world_storage_status status =
            mc_saved_data_map_read(value, &input.root, source, SIZE_MAX, &error);
        CHECK(status == (cases[i].exception == MC_SAVED_DATA_NO_EXCEPTION
                             ? MC_WORLD_STORAGE_OK
                             : MC_WORLD_STORAGE_INVALID_DATA));
        CHECK(error.exception == cases[i].exception && error.bytecode == cases[i].pc);
        if (cases[i].exception == MC_SAVED_DATA_NO_EXCEPTION)
            CHECK(error_ok(&error));
        else
            CHECK(error.index == cases[i].index &&
                  error.length == (cases[i].pc == 228 ? 16384u : cases[i].length));
        CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
              after.colors != NULL && after.colors != old && after.colors != source &&
              mc_nbt_byte_array_length(after.colors) == 16384 && after.dimension == 2 &&
              after.x_center == 3 && after.z_center == 4 && after.scale == 3 &&
              after.updates == 83 && after.dirty && same_collections(&before, &after));
        memset(expected, 0, 16384);
        if (cases[i].pixels == CENTER_FOUR || cases[i].pixels == CENTER_THREE) {
            expected[8127] = 1;
            expected[8128] = 2;
            expected[8255] = 3;
            if (cases[i].pixels == CENTER_FOUR)
                expected[8256] = 4;
        } else if (cases[i].pixels == ROW_129) {
            for (size_t j = 0; j < 129; ++j)
                expected[8064 + j] = pattern(j);
        } else if (cases[i].pixels == ROW_256) {
            for (size_t j = 0; j < 256; ++j)
                expected[8000 + j] = pattern(j);
        } else if (cases[i].pixels == COLUMN_128) {
            for (size_t j = 0; j < 128; ++j)
                expected[63 + j * 128] = pattern(j);
        }
        CHECK(memcmp(mc_nbt_byte_array_data(after.colors), expected, 16384) == 0 &&
              mc_nbt_byte_array_data(old)[0] == 0x77);
        mc_saved_data_value_release(value);
        value = NULL;
        mc_nbt_byte_array_release(source);
        source = NULL;
        mc_nbt_byte_array_release(old);
        old = NULL;
    }
    result = 0;
cleanup:
    mc_saved_data_value_release(value);
    mc_nbt_byte_array_release(source);
    mc_nbt_byte_array_release(old);
    free(expected);
    return result;
}

static int test_write_and_limits(mc_world_info_side side) {
    int result = 1;
    mc_saved_data_value *value = NULL;
    mc_saved_data_record *record = NULL, *nullable = NULL;
    mc_nbt_byte_array *source = NULL, *empty = NULL, *old = NULL;
    mc_saved_data_error error;
    mc_saved_data_map_fields before, after;
    input_fixture input;
    CHECK(mc_saved_data_map_create(mc_saved_data_map_class(side), NULL, SIZE_MAX, &value, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_nbt_byte_array_create(3, NULL, SIZE_MAX, &source) == MC_NBT_OK);
    CHECK(mc_nbt_byte_array_create(0, NULL, SIZE_MAX, &empty) == MC_NBT_OK);
    fill(source);
    CHECK(mc_saved_data_map_fields_get(value, &before) == MC_WORLD_STORAGE_OK);
    CHECK(mc_nbt_byte_array_retain(before.colors) == MC_NBT_OK);
    old = before.colors;
    const size_t new_colors_bytes = mc_nbt_byte_array_allocation_bytes(old);
    const size_t empty_bytes = mc_nbt_byte_array_allocation_bytes(empty);
    fixture(&input, 1, 1, source);
    CHECK(mc_saved_data_map_read(value, &input.root, source, new_colors_bytes - 1u, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          error.bytecode == 118);
    CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
          after.colors == old && after.x_center == 3);
    CHECK(mc_saved_data_map_read(value, &input.root, source, new_colors_bytes, &error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
          after.colors != old && mc_nbt_byte_array_data(after.colors)[8127] == 1);
    fixture(&input, 128, 128, source);
    input.root.value.compound.length = COLORS;
    mc_nbt_byte_array *prior = after.colors;
    CHECK(mc_saved_data_map_read(value, &input.root, source, empty_bytes - 1u, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          error.bytecode == 97);
    CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
          after.colors == prior);
    CHECK(mc_saved_data_map_read(value, &input.root, source, empty_bytes, &error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
          after.colors != NULL && after.colors != source &&
          mc_nbt_byte_array_length(after.colors) == 0);
    prior = after.colors;
    input.fields[WIDTH].value.short_value = 0;
    input.fields[HEIGHT].value.short_value = 0;
    CHECK(mc_saved_data_map_read(value, &input.root, source, new_colors_bytes, &error) ==
              MC_WORLD_STORAGE_LIMIT &&
          error.bytecode == 118);
    CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
          after.colors == prior);
    CHECK(mc_saved_data_map_read(value, &input.root, source, new_colors_bytes + empty_bytes,
                                 &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
          zero_array(after.colors, 16384));
    CHECK(mc_saved_data_map_colors_set(value, source) == MC_WORLD_STORAGE_OK);
    const mc_saved_data_map_cells *live = mc_saved_data_map_live(value);
    *live->dimension = INT8_MIN;
    *live->scale = -7;
    *live->x_center = INT32_MIN;
    *live->z_center = INT32_MAX;
    CHECK(mc_saved_data_value_mark_dirty(value) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_write(value, SIZE_MAX, &record, &error) == MC_WORLD_STORAGE_OK &&
          error_ok(&error));
    const mc_nbt_tag *root = mc_saved_data_record_root(record, &error);
    CHECK(root != NULL && root->type == MC_NBT_COMPOUND && root->value.compound.length == 7);
    for (size_t i = 0; i < FIELD_COUNT; ++i)
        CHECK(child(root, field_names[i]) != NULL &&
              child(root, field_names[i])->type == field_types[i]);
    CHECK(child(root, "dimension")->value.byte_value == INT8_MIN &&
          child(root, "scale")->value.byte_value == -7 &&
          child(root, "xCenter")->value.int_value == INT32_MIN &&
          child(root, "zCenter")->value.int_value == INT32_MAX &&
          child(root, "width")->value.short_value == 128 &&
          child(root, "height")->value.short_value == 128 &&
          child(root, "colors")->value.byte_array.length == 3 && mc_saved_data_value_dirty(value));
    mc_saved_data_record *candidate = record;
    CHECK(mc_saved_data_map_write(value, 0, &candidate, &error) == MC_WORLD_STORAGE_LIMIT &&
          candidate == record);
    CHECK(mc_saved_data_map_write(NULL, SIZE_MAX, &candidate, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          candidate == record);
    CHECK(mc_saved_data_map_write(value, SIZE_MAX, NULL, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_saved_data_map_read_document(value, NULL, &input.root, SIZE_MAX, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    fixture(&input, 128, 128, NULL);
    CHECK(mc_saved_data_map_read(value, &input.root, NULL, 0, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_saved_data_map_fields_get(value, &after) == MC_WORLD_STORAGE_OK &&
          after.colors == NULL && after.dirty);
    CHECK(mc_saved_data_map_write(value, SIZE_MAX, &nullable, &error) == MC_WORLD_STORAGE_OK &&
          error_ok(&error) && nullable != NULL);
    CHECK(mc_saved_data_record_colors(nullable) == NULL &&
          mc_saved_data_record_root(nullable, &error) == NULL &&
          error.cause.status == MC_WORLD_STORAGE_INVALID_DATA &&
          error.exception == MC_SAVED_DATA_NULL_REFERENCE && strcmp(error.field, "colors") == 0);
    CHECK(mc_saved_data_record_root(NULL, &error) == NULL &&
          error.cause.status == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_saved_data_record_colors(NULL) == NULL);
    mc_saved_data_record_release(NULL);
    mc_saved_data_value_release(value);
    value = NULL;
    mc_nbt_byte_array_release(source);
    source = NULL;
    mc_nbt_byte_array_data(mc_saved_data_record_colors(record))[1] = 0xee;
    CHECK(child(mc_saved_data_record_root(record, NULL), "colors")->value.byte_array.data[1] ==
          0xee);
    result = 0;
cleanup:
    mc_saved_data_record_release(record);
    mc_saved_data_record_release(nullable);
    mc_saved_data_value_release(value);
    mc_nbt_byte_array_release(source);
    mc_nbt_byte_array_release(empty);
    mc_nbt_byte_array_release(old);
    return result;
}

int main(void) {
    if (mc_saved_data_map_class(MC_WORLD_INFO_CLIENT) ==
        mc_saved_data_map_class(MC_WORLD_INFO_SERVER))
        return 1;
    for (int side = MC_WORLD_INFO_CLIENT; side <= MC_WORLD_INFO_SERVER; ++side) {
        mc_world_info_side selected = (mc_world_info_side)side;
        if (test_construction(selected) || test_standard_document(selected) ||
            test_numbers_and_failures(selected) || test_decoded_getter_order(selected) ||
            test_resize(selected) || test_write_and_limits(selected))
            return 1;
    }
    printf("MapData: %u native constructed checks passed\n", checks);
    return 0;
}
