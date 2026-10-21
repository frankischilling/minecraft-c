#include "mc/world/storage/chunk.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

enum { FIELD_COUNT = 11, ARRAY_COUNT = 5 };

static const char *const field_names[FIELD_COUNT] = {
    "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
    "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};
static const mc_nbt_type field_types[FIELD_COUNT] = {
    MC_NBT_INT,        MC_NBT_INT,        MC_NBT_LONG,       MC_NBT_BYTE_ARRAY,
    MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY, MC_NBT_BYTE_ARRAY,
    MC_NBT_BYTE,       MC_NBT_LIST,       MC_NBT_LIST};
static const size_t array_lengths[ARRAY_COUNT] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES,
                                                  MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                                                  MC_CHUNK_HEIGHTMAP_BYTES};
static const uint16_t root_name[] = {'R', 0, 0xd800, 0xdc00, 0xffff};
static const uint16_t unknown_root_name[] = {'L', 'e', 'v', 'e', 'l', 0, 0xdfff};
static const uint16_t unknown_level_name[] = {'B', 'l', 'o', 'c', 'k', 's', 0, 0xd800};
static const uint16_t opaque_text[] = {'u', 0, 0xd800, 'n', 0xdc00, 0xffff};
static const uint8_t opaque_bytes[] = {0, 0xff, 0x80, 1, 0x7f};

typedef struct fixture {
    mc_nbt_tag root, level, fields[FIELD_COUNT];
    mc_nbt_tag root_extra, root_tail, level_extra, level_tail;
    mc_nbt_tag entity[2], entity_id[2], entity_payload[2], entity_nested;
    mc_nbt_tag tile, tile_id, tile_payload;
    const mc_nbt_tag *root_items[3], *level_items[FIELD_COUNT + 2];
    const mc_nbt_tag *entity_items[2], *entity_children[2][2], *nested_items[1];
    const mc_nbt_tag *tile_items[1], *tile_children[2];
    uint16_t names[32][16];
    size_t name_count;
    uint8_t arrays[ARRAY_COUNT][MC_CHUNK_BLOCKS + 1];
} fixture;

typedef struct normal_input {
    mc_chunk_state state;
    mc_chunk_data data;
    uint8_t blocks[MC_CHUNK_BLOCKS + 7];
} normal_input;

static void named(fixture *value, mc_nbt_tag *tag, const char *name, mc_nbt_type type) {
    uint16_t *units = value->names[value->name_count++];
    size_t length = strlen(name);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)name[i];
    tag->name = (mc_nbt_string){units, length};
    tag->type = type;
}

/* Constructed contract cases. These do not claim original-runtime parity. */
static void fixture_init(fixture *value, bool entities, bool tiles) {
    memset(value, 0, sizeof(*value));
    value->root.type = MC_NBT_COMPOUND;
    value->root.name = (mc_nbt_string){root_name, sizeof(root_name) / sizeof(root_name[0])};
    named(value, &value->level, "Level", MC_NBT_COMPOUND);
    for (size_t i = 0; i < FIELD_COUNT; ++i)
        named(value, &value->fields[i], field_names[i], field_types[i]);
    value->fields[0].value.int_value = INT32_MIN;
    value->fields[1].value.int_value = INT32_MAX;
    value->fields[2].value.long_value = INT64_MIN;
    value->fields[8].value.byte_value = -128;
    for (size_t a = 0; a < ARRAY_COUNT; ++a) {
        for (size_t i = 0; i < sizeof(value->arrays[a]); ++i)
            value->arrays[a][i] = (uint8_t)((i * (a * 2u + 1u) + a * 37u) & 255u);
        value->fields[3u + a].value.byte_array.data = value->arrays[a];
        value->fields[3u + a].value.byte_array.length = array_lengths[a];
    }
    for (size_t i = 0; i < 2; ++i) {
        value->entity[i].type = MC_NBT_COMPOUND;
        named(value, &value->entity_id[i], "id", MC_NBT_STRING);
        value->entity_id[i].value.string =
            (mc_nbt_string){opaque_text, sizeof(opaque_text) / sizeof(opaque_text[0])};
        named(value, &value->entity_payload[i], "Payload", MC_NBT_BYTE_ARRAY);
        value->entity_payload[i].value.byte_array.data = opaque_bytes;
        value->entity_payload[i].value.byte_array.length = sizeof(opaque_bytes) - i;
        value->entity_children[i][0] = &value->entity_id[i];
        value->entity_children[i][1] = &value->entity_payload[i];
        value->entity[i].value.compound.items = value->entity_children[i];
        value->entity[i].value.compound.length = 2;
        value->entity_items[i] = &value->entity[i];
    }
    named(value, &value->entity_nested, "Nested", MC_NBT_COMPOUND);
    value->nested_items[0] = &value->entity_payload[1];
    value->entity_nested.value.compound.items = value->nested_items;
    value->entity_nested.value.compound.length = 1;
    value->entity_children[1][1] = &value->entity_nested;
    value->fields[9].value.list.element_type = MC_NBT_COMPOUND;
    value->fields[9].value.list.items = value->entity_items;
    value->fields[9].value.list.length = entities ? 2u : 0u;
    value->tile.type = MC_NBT_COMPOUND;
    named(value, &value->tile_id, "id", MC_NBT_STRING);
    value->tile_id.value.string = value->entity_id[0].value.string;
    named(value, &value->tile_payload, "Unmodeled", MC_NBT_STRING);
    value->tile_payload.value.string = value->tile_id.value.string;
    value->tile_children[0] = &value->tile_id;
    value->tile_children[1] = &value->tile_payload;
    value->tile.value.compound.items = value->tile_children;
    value->tile.value.compound.length = 2;
    value->tile_items[0] = &value->tile;
    value->fields[10].value.list.element_type = MC_NBT_COMPOUND;
    value->fields[10].value.list.items = value->tile_items;
    value->fields[10].value.list.length = tiles ? 1u : 0u;
    value->root_extra.type = MC_NBT_STRING;
    value->root_extra.name = (mc_nbt_string){unknown_root_name, sizeof(unknown_root_name) /
                                                                    sizeof(unknown_root_name[0])};
    value->root_extra.value.string = value->tile_id.value.string;
    named(value, &value->root_tail, "Tail", MC_NBT_LONG);
    value->root_tail.value.long_value = INT64_MAX;
    value->level_extra.type = MC_NBT_BYTE_ARRAY;
    value->level_extra.name = (mc_nbt_string){
        unknown_level_name, sizeof(unknown_level_name) / sizeof(unknown_level_name[0])};
    value->level_extra.value.byte_array = value->entity_payload[0].value.byte_array;
    named(value, &value->level_tail, "Tail", MC_NBT_STRING);
    value->level_tail.value.string = value->tile_id.value.string;
    static const size_t order[FIELD_COUNT] = {9, 3, 1, 0, 7, 8, 2, 10, 6, 4, 5};
    value->level_items[0] = &value->level_extra;
    for (size_t i = 0; i < FIELD_COUNT; ++i)
        value->level_items[1u + i] = &value->fields[order[i]];
    value->level_items[FIELD_COUNT + 1u] = &value->level_tail;
    value->level.value.compound.items = value->level_items;
    value->level.value.compound.length = FIELD_COUNT + 2u;
    value->root_items[0] = &value->root_extra;
    value->root_items[1] = &value->level;
    value->root_items[2] = &value->root_tail;
    value->root.value.compound.items = value->root_items;
    value->root.value.compound.length = 3;
}

static const mc_nbt_tag *get(const mc_nbt_tag *parent, const char *name) {
    uint16_t units[16];
    size_t length = strlen(name);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)name[i];
    return mc_nbt_compound_get(parent, units, length);
}

static int tree_bytes(const mc_nbt_tag *root, uint8_t **out, size_t *length) {
    size_t measured = 0;
    CHECK(mc_nbt_measure(root, NULL, &measured, NULL) == MC_NBT_OK);
    uint8_t *bytes = malloc(measured + 3u);
    CHECK(bytes != NULL);
    size_t written = 0;
    CHECK(mc_nbt_write(root, NULL, bytes, measured, &written, NULL) == MC_NBT_OK);
    CHECK(written == measured);
    bytes[written] = 0xa5;
    bytes[written + 1u] = 0x5a;
    bytes[written + 2u] = 0xff;
    *out = bytes;
    *length = written;
    return 0;
}

static int decode_fixture(const fixture *value, mc_stored_chunk **out) {
    uint8_t *bytes = NULL;
    size_t length = 0, consumed = 0;
    CHECK(tree_bytes(&value->root, &bytes, &length) == 0);
    mc_stored_chunk_error error;
    CHECK(mc_stored_chunk_decode(bytes, length + 3u, NULL, out, &consumed, &error) ==
          MC_STORED_CHUNK_OK);
    CHECK(consumed == length && error.status == MC_STORED_CHUNK_OK && error.field == NULL &&
          error.nbt.status == MC_NBT_OK);
    memset(bytes, 0xa5, length + 3u);
    free(bytes);
    return 0;
}

static int same_tag(const mc_nbt_tag *left, const mc_nbt_tag *right) {
    uint8_t *a = NULL, *b = NULL;
    size_t a_length = 0, b_length = 0;
    CHECK(tree_bytes(left, &a, &a_length) == 0);
    CHECK(tree_bytes(right, &b, &b_length) == 0);
    CHECK(a_length == b_length && memcmp(a, b, a_length) == 0);
    free(a);
    free(b);
    return 0;
}

static int same_string(mc_nbt_string actual, const uint16_t *units, size_t length) {
    CHECK(actual.length == length);
    CHECK(length == 0 ||
          (actual.units != NULL && memcmp(actual.units, units, length * sizeof(*units)) == 0));
    return 0;
}

static uint8_t *data_array(mc_chunk_data *data, size_t array) {
    switch (array) {
        case 0:
            return data->blocks;
        case 1:
            return data->metadata;
        case 2:
            return data->skylight;
        case 3:
            return data->block_light;
        default:
            return data->heightmap;
    }
}

static int expect_payload(const mc_chunk_state *state, const fixture *value) {
    CHECK(state != NULL && state->data != NULL && state->data->blocks != NULL);
    CHECK(state->data->blocks_capacity == MC_CHUNK_BLOCKS);
    for (size_t a = 0; a < ARRAY_COUNT; ++a)
        CHECK(memcmp(data_array(state->data, a), value->arrays[a], array_lengths[a]) == 0);
    return 0;
}

static int expect_loaded_flags(const mc_chunk_state *state, bool entities) {
    CHECK(!state->blank && !state->dirty && !state->active && !state->never_save);
    CHECK(state->last_save_time == 0 && state->data->minimum_height == 0);
    CHECK(state->has_entities == entities);
    return 0;
}

static int expect_built_fields(const mc_nbt_tag *root, const mc_chunk_state *state, int64_t time) {
    const mc_nbt_tag *level = get(root, "Level");
    CHECK(root->type == MC_NBT_COMPOUND && level != NULL && level->type == MC_NBT_COMPOUND);
    CHECK(get(level, "xPos")->type == MC_NBT_INT &&
          get(level, "xPos")->value.int_value == state->x);
    CHECK(get(level, "zPos")->type == MC_NBT_INT &&
          get(level, "zPos")->value.int_value == state->z);
    CHECK(get(level, "LastUpdate")->type == MC_NBT_LONG &&
          get(level, "LastUpdate")->value.long_value == time);
    CHECK(get(level, "TerrainPopulated")->type == MC_NBT_BYTE &&
          get(level, "TerrainPopulated")->value.byte_value == (state->terrain_populated ? 1 : 0));
    for (size_t a = 0; a < ARRAY_COUNT; ++a) {
        const mc_nbt_tag *tag = get(level, field_names[3u + a]);
        CHECK(tag != NULL && tag->type == MC_NBT_BYTE_ARRAY &&
              tag->value.byte_array.length == array_lengths[a]);
        CHECK(memcmp(tag->value.byte_array.data, data_array(state->data, a), array_lengths[a]) ==
              0);
    }
    return 0;
}

static void omit(fixture *value, size_t field) {
    size_t length = value->level.value.compound.length;
    for (size_t i = 0; i < length; ++i) {
        if (value->level_items[i] == &value->fields[field]) {
            memmove(&value->level_items[i], &value->level_items[i + 1u],
                    (length - i - 1u) * sizeof(value->level_items[0]));
            value->level.value.compound.length = length - 1u;
            return;
        }
    }
    abort();
}

static void normal_init(normal_input *value) {
    memset(value, 0, sizeof(*value));
    value->state.data = &value->data;
    value->state.x = -33;
    value->state.z = INT32_MAX;
    value->state.dirty = true;
    value->state.active = true;
    value->state.never_save = true;
    value->state.terrain_populated = true;
    value->state.last_save_time = INT64_MIN;
    value->data.blocks = value->blocks;
    value->data.blocks_capacity = sizeof(value->blocks);
    value->data.minimum_height = 66;
    for (size_t a = 0; a < ARRAY_COUNT; ++a)
        for (size_t i = 0; i < array_lengths[a]; ++i)
            data_array(&value->data, a)[i] = (uint8_t)((i * 13u + a * 29u) & 255u);
    memset(value->blocks + MC_CHUNK_BLOCKS, 0xe7, 7);
}

static int test_create_and_independent_lifetimes(void) {
    normal_input *input = malloc(sizeof(*input));
    normal_input *expected = malloc(sizeof(*expected));
    CHECK(input != NULL && expected != NULL);
    normal_init(input);
    normal_init(expected);
    mc_stored_chunk *owner = NULL;
    mc_stored_chunk_error error;
    CHECK(mc_stored_chunk_create(&input->state, NULL, &owner, &error) == MC_STORED_CHUNK_OK);
    CHECK(error.status == MC_STORED_CHUNK_OK && error.field == NULL &&
          error.nbt.status == MC_NBT_OK);
    mc_chunk_state *state = mc_stored_chunk_state(owner);
    mc_chunk_data *data = state->data;
    uint8_t *blocks = data->blocks;
    CHECK(state != &input->state && data != &input->data && blocks != input->blocks);
    CHECK(mc_stored_chunk_root(owner) == NULL && data->blocks_capacity == MC_CHUNK_BLOCKS);
    CHECK(state->x == -33 && state->z == INT32_MAX && !state->blank && state->dirty &&
          state->active && state->never_save && state->terrain_populated && !state->has_entities &&
          state->last_save_time == INT64_MIN && data->minimum_height == 66);
    memset(input, 0xa5, sizeof(*input));
    free(input);
    for (size_t a = 0; a < ARRAY_COUNT; ++a)
        CHECK(memcmp(data_array(data, a), data_array(&expected->data, a), array_lengths[a]) == 0);
    mc_chunk_state saved_state;
    mc_chunk_data saved_data;
    memcpy(&saved_state, state, sizeof(saved_state));
    memcpy(&saved_data, data, sizeof(saved_data));
    mc_nbt_document *built = NULL;
    CHECK(mc_stored_chunk_build(owner, INT64_MAX, NULL, &built, NULL) == MC_STORED_CHUNK_OK);
    const mc_nbt_tag *root = mc_nbt_document_root(built);
    const mc_nbt_tag *level = get(root, "Level");
    CHECK(root->name.length == 0 && root->value.compound.length == 1);
    CHECK(level->value.compound.length == FIELD_COUNT);
    CHECK(get(level, "Entities")->value.list.length == 0 &&
          get(level, "TileEntities")->value.list.length == 0);
    CHECK(expect_built_fields(root, state, INT64_MAX) == 0);
    CHECK(memcmp(state, &saved_state, sizeof(saved_state)) == 0 &&
          memcmp(data, &saved_data, sizeof(saved_data)) == 0);
    CHECK(mc_stored_chunk_state(owner) == state && state->data == data && data->blocks == blocks);
    uint8_t *before = NULL;
    size_t length = 0;
    CHECK(tree_bytes(root, &before, &length) == 0);
    memset(blocks, 0, MC_CHUNK_BLOCKS);
    memset(data->metadata, 0, sizeof(data->metadata));
    state->x = 0;
    state->last_save_time = 37;
    CHECK(memcmp(get(level, "Blocks")->value.byte_array.data, expected->blocks, MC_CHUNK_BLOCKS) ==
          0);
    mc_stored_chunk_free(owner);
    uint8_t *after = NULL;
    size_t after_length = 0;
    CHECK(tree_bytes(root, &after, &after_length) == 0);
    CHECK(length == after_length && memcmp(before, after, length) == 0);
    mc_stored_chunk *loaded = NULL;
    CHECK(mc_stored_chunk_decode(after, after_length, NULL, &loaded, NULL, NULL) ==
          MC_STORED_CHUNK_OK);
    CHECK(expect_loaded_flags(mc_stored_chunk_state(loaded), false) == 0);
    CHECK(mc_stored_chunk_state(loaded)->x == -33 &&
          mc_stored_chunk_state(loaded)->terrain_populated);
    mc_stored_chunk_free(loaded);
    mc_nbt_document_free(built);
    free(after);
    free(before);
    free(expected);
    CHECK(mc_stored_chunk_state(NULL) == NULL && mc_stored_chunk_root(NULL) == NULL);
    mc_stored_chunk_free(NULL);
    return 0;
}

static int test_decode_defaults_and_ignored_time(void) {
    fixture *value = malloc(sizeof(*value));
    CHECK(value != NULL);
    fixture_init(value, false, false);
    static const size_t optional[] = {0, 1, 2, 8, 9, 10};
    for (size_t i = 0; i < sizeof(optional) / sizeof(optional[0]); ++i)
        omit(value, optional[i]);
    mc_stored_chunk *owner = NULL;
    CHECK(decode_fixture(value, &owner) == 0);
    mc_chunk_state *state = mc_stored_chunk_state(owner);
    CHECK(state->x == 0 && state->z == 0 && !state->terrain_populated);
    CHECK(expect_payload(state, value) == 0 && expect_loaded_flags(state, false) == 0);
    mc_stored_chunk_free(owner);
    static const int8_t booleans[] = {-128, -1, 0, 1, 2, 127};
    for (size_t i = 0; i < sizeof(booleans) / sizeof(booleans[0]); ++i) {
        fixture_init(value, false, false);
        value->fields[8].value.byte_value = booleans[i];
        CHECK(decode_fixture(value, &owner) == 0);
        state = mc_stored_chunk_state(owner);
        CHECK(state->x == INT32_MIN && state->z == INT32_MAX);
        CHECK(state->terrain_populated == (booleans[i] != 0));
        CHECK(expect_payload(state, value) == 0 && expect_loaded_flags(state, false) == 0);
        mc_stored_chunk_free(owner);
    }
    for (int type = MC_NBT_BYTE; type <= MC_NBT_COMPOUND; ++type) {
        fixture_init(value, false, false);
        memset(&value->fields[2].value, 0, sizeof(value->fields[2].value));
        value->fields[2].type = (mc_nbt_type)type;
        CHECK(decode_fixture(value, &owner) == 0);
        state = mc_stored_chunk_state(owner);
        CHECK(expect_loaded_flags(state, false) == 0 && expect_payload(state, value) == 0);
        CHECK(get(get(mc_stored_chunk_root(owner), "Level"), "LastUpdate")->type ==
              (mc_nbt_type)type);
        mc_stored_chunk_free(owner);
    }
    free(value);
    return 0;
}

static int expect_decode_failure(const mc_nbt_tag *root, mc_stored_chunk *sentinel,
                                 mc_stored_chunk_status expected, const char *field) {
    uint8_t *bytes = NULL, *saved = NULL;
    size_t length = 0, consumed = 98765;
    CHECK(tree_bytes(root, &bytes, &length) == 0);
    saved = malloc(length);
    CHECK(saved != NULL);
    memcpy(saved, bytes, length);
    mc_stored_chunk *result = sentinel;
    mc_stored_chunk_error error;
    CHECK(mc_stored_chunk_decode(bytes, length, NULL, &result, &consumed, &error) == expected);
    CHECK(result == sentinel && consumed == 98765 && error.status == expected);
    CHECK(field == NULL ? error.field == NULL
                        : error.field != NULL && strcmp(error.field, field) == 0);
    CHECK(error.nbt.status == MC_NBT_OK && memcmp(saved, bytes, length) == 0);
    CHECK(mc_stored_chunk_decode(bytes, length, NULL, &result, &consumed, NULL) == expected);
    CHECK(result == sentinel && consumed == 98765);
    free(saved);
    free(bytes);
    return 0;
}

static int test_exact_types_missing_fields_and_shapes(void) {
    fixture *value = malloc(sizeof(*value));
    normal_input *input = malloc(sizeof(*input));
    CHECK(value != NULL && input != NULL);
    normal_init(input);
    mc_stored_chunk *sentinel = NULL;
    CHECK(mc_stored_chunk_create(&input->state, NULL, &sentinel, NULL) == MC_STORED_CHUNK_OK);
    fixture_init(value, false, false);
    value->root.value.compound.length = 0;
    CHECK(expect_decode_failure(&value->root, sentinel, MC_STORED_CHUNK_MISSING_LEVEL, "Level") ==
          0);
    fixture_init(value, false, false);
    value->level.type = MC_NBT_BYTE;
    value->level.value.byte_value = 0;
    CHECK(expect_decode_failure(&value->root, sentinel, MC_STORED_CHUNK_INVALID_FIELD, "Level") ==
          0);
    for (size_t field = 0; field < FIELD_COUNT; ++field) {
        if (field == 2)
            continue;
        fixture_init(value, false, false);
        value->fields[field].type = field_types[field] == MC_NBT_BYTE ? MC_NBT_INT : MC_NBT_BYTE;
        memset(&value->fields[field].value, 0, sizeof(value->fields[field].value));
        CHECK(expect_decode_failure(&value->root, sentinel, MC_STORED_CHUNK_INVALID_FIELD,
                                    field_names[field]) == 0);
    }
    for (size_t a = 0; a < ARRAY_COUNT; ++a) {
        fixture_init(value, false, false);
        omit(value, 3u + a);
        CHECK(expect_decode_failure(&value->root, sentinel,
                                    a == 0 ? MC_STORED_CHUNK_MISSING_BLOCKS
                                           : MC_STORED_CHUNK_INVALID_SHAPE,
                                    field_names[3u + a]) == 0);
        for (size_t shape = 0; shape < 3; ++shape) {
            fixture_init(value, false, false);
            value->fields[3u + a].value.byte_array.length = shape == 0   ? 0
                                                            : shape == 1 ? array_lengths[a] - 1u
                                                                         : array_lengths[a] + 1u;
            CHECK(expect_decode_failure(&value->root, sentinel, MC_STORED_CHUNK_INVALID_SHAPE,
                                        field_names[3u + a]) == 0);
        }
    }
    for (size_t field = 9; field < FIELD_COUNT; ++field) {
        fixture_init(value, true, true);
        mc_nbt_tag item = {0};
        item.type = MC_NBT_INT;
        item.value.int_value = -1;
        const mc_nbt_tag *items[] = {&item};
        value->fields[field].value.list.element_type = MC_NBT_INT;
        value->fields[field].value.list.items = items;
        value->fields[field].value.list.length = 1;
        CHECK(expect_decode_failure(&value->root, sentinel, MC_STORED_CHUNK_INVALID_FIELD,
                                    field_names[field]) == 0);
    }
    mc_stored_chunk_free(sentinel);
    free(input);
    free(value);
    return 0;
}

static int test_opaque_utf16_preservation_and_result_ownership(void) {
    fixture *value = malloc(sizeof(*value));
    CHECK(value != NULL);
    fixture_init(value, true, true);
    uint8_t *bytes = NULL;
    size_t length = 0;
    CHECK(tree_bytes(&value->root, &bytes, &length) == 0);
    mc_nbt_document *expected = NULL;
    CHECK(mc_nbt_read_compound(bytes, length, NULL, &expected, NULL, NULL) == MC_NBT_OK);
    mc_stored_chunk *owner = NULL;
    CHECK(mc_stored_chunk_decode(bytes, length, NULL, &owner, NULL, NULL) == MC_STORED_CHUNK_OK);
    memset(bytes, 0xa5, length);
    free(bytes);
    memset(value, 0xa5, sizeof(*value));
    free(value);
    mc_chunk_state *state = mc_stored_chunk_state(owner);
    CHECK(expect_loaded_flags(state, true) == 0);
    const mc_nbt_tag *source = mc_stored_chunk_root(owner);
    CHECK(same_tag(source, mc_nbt_document_root(expected)) == 0);
    const mc_nbt_tag *source_level = get(source, "Level");
    CHECK(state->data->blocks != get(source_level, "Blocks")->value.byte_array.data);
    state->dirty = true;
    state->active = true;
    state->never_save = true;
    state->last_save_time = -1234;
    state->data->minimum_height = 37;
    mc_chunk_state saved_state;
    mc_chunk_data saved_data;
    memcpy(&saved_state, state, sizeof(saved_state));
    memcpy(&saved_data, state->data, sizeof(saved_data));
    mc_nbt_document *result = NULL;
    CHECK(mc_stored_chunk_build(owner, INT64_MAX, NULL, &result, NULL) == MC_STORED_CHUNK_OK);
    const mc_nbt_tag *root = mc_nbt_document_root(result);
    const mc_nbt_tag *level = get(root, "Level");
    CHECK(expect_built_fields(root, state, INT64_MAX) == 0);
    CHECK(same_string(root->name, root_name, sizeof(root_name) / sizeof(root_name[0])) == 0);
    CHECK(root->name.units != source->name.units && root != source && level != source_level);
    CHECK(root->value.compound.length == source->value.compound.length &&
          level->value.compound.length == source_level->value.compound.length);
    for (size_t i = 0; i < root->value.compound.length; ++i)
        CHECK(same_string(root->value.compound.items[i]->name,
                          source->value.compound.items[i]->name.units,
                          source->value.compound.items[i]->name.length) == 0);
    for (size_t i = 0; i < level->value.compound.length; ++i)
        CHECK(same_string(level->value.compound.items[i]->name,
                          source_level->value.compound.items[i]->name.units,
                          source_level->value.compound.items[i]->name.length) == 0);
    CHECK(same_tag(source->value.compound.items[0], root->value.compound.items[0]) == 0);
    CHECK(same_tag(source_level->value.compound.items[0], level->value.compound.items[0]) == 0);
    CHECK(same_tag(get(source, "Tail"), get(root, "Tail")) == 0);
    CHECK(same_tag(get(source_level, "Tail"), get(level, "Tail")) == 0);
    for (size_t i = 9; i < FIELD_COUNT; ++i) {
        CHECK(same_tag(get(source_level, field_names[i]), get(level, field_names[i])) == 0);
        CHECK(get(source_level, field_names[i])->value.list.items !=
              get(level, field_names[i])->value.list.items);
    }
    CHECK(get(level, "Entities")->value.list.length == 2 &&
          get(level, "TileEntities")->value.list.length == 1);
    CHECK(memcmp(state, &saved_state, sizeof(saved_state)) == 0 &&
          memcmp(state->data, &saved_data, sizeof(saved_data)) == 0);
    CHECK(same_tag(source, mc_nbt_document_root(expected)) == 0);
    uint8_t *before = NULL, *after = NULL;
    size_t before_length = 0, after_length = 0;
    CHECK(tree_bytes(root, &before, &before_length) == 0);
    mc_nbt_document_free(expected);
    mc_stored_chunk_free(owner);
    CHECK(tree_bytes(root, &after, &after_length) == 0);
    CHECK(before_length == after_length && memcmp(before, after, before_length) == 0);
    mc_stored_chunk *loaded = NULL;
    CHECK(mc_stored_chunk_decode(after, after_length, NULL, &loaded, NULL, NULL) ==
          MC_STORED_CHUNK_OK);
    CHECK(expect_loaded_flags(mc_stored_chunk_state(loaded), true) == 0);
    CHECK(mc_stored_chunk_state(loaded)->x == INT32_MIN &&
          mc_stored_chunk_state(loaded)->z == INT32_MAX);
    mc_stored_chunk_free(loaded);
    mc_nbt_document_free(result);
    free(before);
    free(after);
    return 0;
}

static int expect_build_failure(mc_stored_chunk *owner, const mc_nbt_limits *limits,
                                mc_nbt_document *sentinel, mc_stored_chunk_status expected) {
    mc_chunk_state *state = mc_stored_chunk_state(owner);
    mc_chunk_state saved_state = {0};
    mc_chunk_data saved_data = {0};
    uint8_t saved_blocks[MC_CHUNK_BLOCKS];
    size_t block_length = 0;
    if (state != NULL) {
        memcpy(&saved_state, state, sizeof(saved_state));
        if (state->data != NULL) {
            memcpy(&saved_data, state->data, sizeof(saved_data));
            if (state->data->blocks != NULL) {
                block_length = state->data->blocks_capacity < MC_CHUNK_BLOCKS
                                   ? state->data->blocks_capacity
                                   : MC_CHUNK_BLOCKS;
                memcpy(saved_blocks, state->data->blocks, block_length);
            }
        }
    }
    const mc_nbt_tag *source = mc_stored_chunk_root(owner);
    uint8_t *before = NULL;
    size_t length = 0;
    if (source != NULL)
        CHECK(tree_bytes(source, &before, &length) == 0);
    mc_nbt_document *result = sentinel;
    mc_stored_chunk_error error;
    CHECK(mc_stored_chunk_build(owner, 1234, limits, &result, &error) == expected);
    CHECK(result == sentinel && error.status == expected);
    if (state != NULL) {
        CHECK(memcmp(state, &saved_state, sizeof(saved_state)) == 0);
        if (state->data != NULL) {
            CHECK(memcmp(state->data, &saved_data, sizeof(saved_data)) == 0);
            if (block_length != 0)
                CHECK(memcmp(state->data->blocks, saved_blocks, block_length) == 0);
        }
    }
    if (source != NULL) {
        uint8_t *after = NULL;
        size_t after_length = 0;
        CHECK(tree_bytes(source, &after, &after_length) == 0);
        CHECK(length == after_length && memcmp(before, after, length) == 0);
        free(after);
    }
    free(before);
    return 0;
}

static int test_opaque_edit_rejections_and_empty_editing(void) {
    fixture *value = malloc(sizeof(*value));
    CHECK(value != NULL);
    for (size_t mode = 0; mode < 3; ++mode) {
        fixture_init(value, mode != 1, mode != 0);
        mc_stored_chunk *owner = NULL;
        CHECK(decode_fixture(value, &owner) == 0);
        mc_nbt_document *sentinel = NULL;
        CHECK(mc_stored_chunk_build(owner, 7, NULL, &sentinel, NULL) == MC_STORED_CHUNK_OK);
        mc_chunk_state *state = mc_stored_chunk_state(owner);
        for (size_t edit = 0; edit < 9; ++edit) {
            mc_chunk_state saved;
            memcpy(&saved, state, sizeof(saved));
            if (edit == 0)
                ++state->x;
            else if (edit == 1)
                --state->z;
            else if (edit == 2)
                state->terrain_populated = !state->terrain_populated;
            else if (edit == 3)
                state->has_entities = !state->has_entities;
            uint8_t *changed = NULL;
            uint8_t byte = 0;
            if (edit >= 4) {
                size_t a = edit - 4u;
                changed = data_array(state->data, a) + array_lengths[a] - 1u;
                byte = *changed;
                *changed ^= 0x80u;
            }
            CHECK(expect_build_failure(owner, NULL, sentinel, MC_STORED_CHUNK_UNSUPPORTED_EDIT) ==
                  0);
            if (changed != NULL)
                *changed = byte;
            memcpy(state, &saved, sizeof(saved));
        }
        mc_nbt_document *restored = NULL;
        CHECK(mc_stored_chunk_build(owner, -7, NULL, &restored, NULL) == MC_STORED_CHUNK_OK);
        mc_nbt_document_free(restored);
        mc_nbt_document_free(sentinel);
        mc_stored_chunk_free(owner);
    }
    fixture_init(value, false, false);
    mc_stored_chunk *owner = NULL;
    CHECK(decode_fixture(value, &owner) == 0);
    mc_chunk_state *state = mc_stored_chunk_state(owner);
    state->x = INT32_MAX;
    state->z = -33;
    state->terrain_populated = false;
    for (size_t a = 0; a < ARRAY_COUNT; ++a)
        memset(data_array(state->data, a), (int)(a + 1u), array_lengths[a]);
    mc_nbt_document *result = NULL;
    CHECK(mc_stored_chunk_build(owner, INT64_MIN, NULL, &result, NULL) == MC_STORED_CHUNK_OK);
    CHECK(expect_built_fields(mc_nbt_document_root(result), state, INT64_MIN) == 0);
    CHECK(same_tag(mc_stored_chunk_root(owner)->value.compound.items[0],
                   mc_nbt_document_root(result)->value.compound.items[0]) == 0);
    state->has_entities = true;
    CHECK(expect_build_failure(owner, NULL, result, MC_STORED_CHUNK_UNSUPPORTED_EDIT) == 0);
    mc_nbt_document_free(result);
    mc_stored_chunk_free(owner);
    free(value);
    return 0;
}

static int test_frozen_normalization_and_descriptor_guards(void) {
    fixture *value = malloc(sizeof(*value));
    normal_input *alternate = malloc(sizeof(*alternate));
    CHECK(value != NULL && alternate != NULL);
    fixture_init(value, false, false);
    normal_init(alternate);
    mc_stored_chunk *owner = NULL;
    CHECK(decode_fixture(value, &owner) == 0);
    mc_chunk_state *state = mc_stored_chunk_state(owner);
    mc_chunk_data *data = state->data;
    uint8_t *blocks = data->blocks;
    state->data->minimum_height = 37;
    mc_chunk_state saved_state;
    mc_chunk_data saved_data;
    memcpy(&saved_state, state, sizeof(saved_state));
    memcpy(&saved_data, data, sizeof(saved_data));
    uint8_t saved_blocks[MC_CHUNK_BLOCKS];
    memcpy(saved_blocks, blocks, sizeof(saved_blocks));
    uint8_t map[257];
    for (size_t i = 0; i < 256; ++i)
        map[i] = (uint8_t)i;
    map[7] = map[255] = 0;
    map[256] = 42;
    CHECK(mc_stored_chunk_normalize(owner, map, sizeof(map)) == MC_STORED_CHUNK_OK);
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i)
        CHECK(blocks[i] == (saved_blocks[i] == 7 || saved_blocks[i] == 255 ? 0 : saved_blocks[i]));
    CHECK(memcmp(state, &saved_state, sizeof(saved_state)) == 0 &&
          memcmp(data, &saved_data, sizeof(saved_data)) == 0);
    CHECK(same_tag(&value->root, mc_stored_chunk_root(owner)) == 0);
    CHECK(mc_stored_chunk_state(owner) == state && state->data == data && data->blocks == blocks);
    memcpy(saved_blocks, blocks, sizeof(saved_blocks));
    map[9] = 0;
    map[255] = 254;
    CHECK(mc_stored_chunk_normalize(owner, map, sizeof(map)) == MC_STORED_CHUNK_INVALID_ARGUMENT);
    CHECK(mc_stored_chunk_normalize(owner, NULL, 256) == MC_STORED_CHUNK_INVALID_ARGUMENT);
    CHECK(mc_stored_chunk_normalize(owner, map, 255) == MC_STORED_CHUNK_INVALID_ARGUMENT);
    CHECK(mc_stored_chunk_normalize(NULL, map, 256) == MC_STORED_CHUNK_INVALID_ARGUMENT);
    CHECK(memcmp(blocks, saved_blocks, sizeof(saved_blocks)) == 0);
    CHECK(memcmp(state, &saved_state, sizeof(saved_state)) == 0 &&
          memcmp(data, &saved_data, sizeof(saved_data)) == 0);
    memset(map, 0, sizeof(map));
    mc_nbt_document *sentinel = NULL;
    CHECK(mc_stored_chunk_build(owner, 7, NULL, &sentinel, NULL) == MC_STORED_CHUNK_OK);
    for (size_t guard = 0; guard < 6; ++guard) {
        if (guard == 0)
            state->data = &alternate->data;
        else if (guard == 1)
            state->data = NULL;
        else if (guard == 2)
            data->blocks = alternate->blocks;
        else if (guard == 3)
            data->blocks_capacity = MC_CHUNK_BLOCKS - 1u;
        else if (guard == 4)
            data->blocks_capacity = MC_CHUNK_BLOCKS + 1u;
        else
            state->blank = true;
        CHECK(expect_build_failure(owner, NULL, sentinel, MC_STORED_CHUNK_INVALID_ARGUMENT) == 0);
        CHECK(mc_stored_chunk_normalize(owner, map, 256) == MC_STORED_CHUNK_INVALID_ARGUMENT);
        CHECK(memcmp(blocks, saved_blocks, sizeof(saved_blocks)) == 0);
        memcpy(state, &saved_state, sizeof(saved_state));
        memcpy(data, &saved_data, sizeof(saved_data));
    }
    CHECK(mc_stored_chunk_normalize(owner, map, 256) == MC_STORED_CHUNK_OK);
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i)
        CHECK(blocks[i] == 0);
    CHECK(memcmp(state, &saved_state, sizeof(saved_state)) == 0 &&
          memcmp(data, &saved_data, sizeof(saved_data)) == 0);
    mc_nbt_document_free(sentinel);
    mc_stored_chunk_free(owner);
    free(alternate);
    free(value);
    return 0;
}

static int test_create_arguments_and_allocation_boundary(void) {
    normal_input *input = malloc(sizeof(*input));
    normal_input *saved_input = malloc(sizeof(*saved_input));
    CHECK(input != NULL && saved_input != NULL);
    normal_init(input);
    memcpy(saved_input, input, sizeof(*saved_input));
    mc_stored_chunk *sentinel = NULL;
    CHECK(mc_stored_chunk_create(&input->state, NULL, &sentinel, NULL) == MC_STORED_CHUNK_OK);
    mc_stored_chunk *result = sentinel;
    for (size_t invalid = 0; invalid < 5; ++invalid) {
        mc_chunk_state state = input->state;
        mc_chunk_data data = input->data;
        state.data = &data;
        if (invalid == 0)
            state.blank = true;
        else if (invalid == 1)
            state.data = NULL;
        else if (invalid == 2)
            data.blocks = NULL;
        else if (invalid == 3)
            data.blocks_capacity = MC_CHUNK_BLOCKS - 1u;
        else
            state.has_entities = true;
        mc_chunk_state saved_state;
        mc_chunk_data saved_data;
        memcpy(&saved_state, &state, sizeof(saved_state));
        memcpy(&saved_data, &data, sizeof(saved_data));
        mc_stored_chunk_error error;
        mc_stored_chunk_status expected =
            invalid == 4 ? MC_STORED_CHUNK_UNSUPPORTED_EDIT : MC_STORED_CHUNK_INVALID_ARGUMENT;
        CHECK(mc_stored_chunk_create(&state, NULL, &result, &error) == expected);
        CHECK(result == sentinel && error.status == expected);
        CHECK(memcmp(&state, &saved_state, sizeof(saved_state)) == 0 &&
              memcmp(&data, &saved_data, sizeof(saved_data)) == 0);
    }
    CHECK(mc_stored_chunk_create(NULL, NULL, &result, NULL) == MC_STORED_CHUNK_INVALID_ARGUMENT);
    CHECK(result == sentinel);
    CHECK(mc_stored_chunk_create(&input->state, NULL, NULL, NULL) ==
          MC_STORED_CHUNK_INVALID_ARGUMENT);
    size_t lower = 0, upper = 1048576;
    while (lower < upper) {
        size_t middle = lower + (upper - lower) / 2u;
        mc_nbt_limits limits = mc_nbt_default_limits();
        limits.max_allocation_bytes = middle;
        mc_stored_chunk *candidate = NULL;
        mc_stored_chunk_status status =
            mc_stored_chunk_create(&input->state, &limits, &candidate, NULL);
        CHECK(status == MC_STORED_CHUNK_OK || status == MC_STORED_CHUNK_LIMIT);
        if (status == MC_STORED_CHUNK_OK) {
            mc_stored_chunk_free(candidate);
            upper = middle;
        } else {
            CHECK(candidate == NULL);
            lower = middle + 1u;
        }
    }
    CHECK(lower > MC_CHUNK_BLOCKS && lower < 1048576);
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = lower - 1u;
    CHECK(mc_stored_chunk_create(&input->state, &limits, &result, NULL) == MC_STORED_CHUNK_LIMIT);
    CHECK(result == sentinel);
    ++limits.max_allocation_bytes;
    mc_stored_chunk *bounded = NULL;
    CHECK(mc_stored_chunk_create(&input->state, &limits, &bounded, NULL) == MC_STORED_CHUNK_OK);
    CHECK(memcmp(input, saved_input, sizeof(*saved_input)) == 0);
    mc_stored_chunk_free(bounded);
    mc_stored_chunk_free(sentinel);
    free(saved_input);
    free(input);
    return 0;
}

static int test_decode_and_build_limits_and_failures(void) {
    fixture *value = malloc(sizeof(*value));
    CHECK(value != NULL);
    fixture_init(value, false, false);
    uint8_t *bytes = NULL;
    size_t length = 0;
    CHECK(tree_bytes(&value->root, &bytes, &length) == 0);
    mc_stored_chunk *owner = NULL;
    CHECK(mc_stored_chunk_decode(bytes, length, NULL, &owner, NULL, NULL) == MC_STORED_CHUNK_OK);
    mc_nbt_document *sentinel = NULL;
    CHECK(mc_stored_chunk_build(owner, 7, NULL, &sentinel, NULL) == MC_STORED_CHUNK_OK);
    CHECK(expect_build_failure(NULL, NULL, sentinel, MC_STORED_CHUNK_INVALID_ARGUMENT) == 0);
    CHECK(mc_stored_chunk_build(owner, 0, NULL, NULL, NULL) == MC_STORED_CHUNK_INVALID_ARGUMENT);
    for (size_t mode = 0; mode < 8; ++mode) {
        mc_nbt_limits limits = mc_nbt_default_limits();
        mc_stored_chunk_status expected = MC_STORED_CHUNK_LIMIT;
        if (mode == 0)
            limits.max_input_bytes = 0;
        else if (mode == 1)
            limits.max_allocation_bytes = 0;
        else if (mode == 2)
            limits.max_tags = 1;
        else if (mode == 3)
            limits.max_elements = MC_CHUNK_BLOCKS - 1u;
        else if (mode == 4)
            limits.max_depth = 1;
        else {
            limits.max_depth = mode == 5 ? 0 : mode == 6 ? 129 : SIZE_MAX;
            expected = MC_STORED_CHUNK_INVALID_ARGUMENT;
        }
        mc_stored_chunk *result = owner;
        size_t consumed = 98765;
        mc_stored_chunk_error error;
        CHECK(mc_stored_chunk_decode(bytes, length, &limits, &result, &consumed, &error) ==
              expected);
        CHECK(result == owner && consumed == 98765 && error.status == expected);
        CHECK(expect_build_failure(owner, &limits, sentinel, expected) == 0);
        if (mode >= 5) {
            normal_input *input = malloc(sizeof(*input));
            CHECK(input != NULL);
            normal_init(input);
            CHECK(mc_stored_chunk_create(&input->state, &limits, &result, NULL) == expected);
            CHECK(result == owner);
            free(input);
        }
    }
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_output_bytes = 0;
    CHECK(expect_build_failure(owner, &limits, sentinel, MC_STORED_CHUNK_LIMIT) == 0);
    mc_stored_chunk *result = owner;
    size_t consumed = 98765;
    mc_stored_chunk_error error;
    CHECK(mc_stored_chunk_decode(bytes, length - 1u, NULL, &result, &consumed, &error) ==
          MC_STORED_CHUNK_NBT_ERROR);
    CHECK(result == owner && consumed == 98765 && error.nbt.status == MC_NBT_TRUNCATED);
    CHECK(mc_stored_chunk_decode(NULL, 1, NULL, &result, &consumed, NULL) ==
          MC_STORED_CHUNK_INVALID_ARGUMENT);
    CHECK(mc_stored_chunk_decode(bytes, length, NULL, NULL, &consumed, NULL) ==
          MC_STORED_CHUNK_INVALID_ARGUMENT);
    CHECK(mc_stored_chunk_decode(NULL, 0, NULL, &result, &consumed, &error) ==
          MC_STORED_CHUNK_NBT_ERROR);
    CHECK(result == owner && consumed == 98765 && error.nbt.status == MC_NBT_TRUNCATED);
    static const uint8_t wrong_root[] = {MC_NBT_BYTE, 0, 0, 0};
    CHECK(mc_stored_chunk_decode(wrong_root, sizeof(wrong_root), NULL, &result, &consumed,
                                 &error) == MC_STORED_CHUNK_NBT_ERROR);
    CHECK(result == owner && consumed == 98765 && error.nbt.status == MC_NBT_ROOT_TYPE);
    for (size_t operation = 0; operation < 2; ++operation) {
        size_t lower = 0, upper = 1048576;
        while (lower < upper) {
            size_t middle = lower + (upper - lower) / 2u;
            limits = mc_nbt_default_limits();
            limits.max_allocation_bytes = middle;
            mc_stored_chunk_status status;
            if (operation == 0) {
                mc_stored_chunk *candidate = NULL;
                status = mc_stored_chunk_decode(bytes, length, &limits, &candidate, NULL, NULL);
                if (status == MC_STORED_CHUNK_OK)
                    mc_stored_chunk_free(candidate);
                else
                    CHECK(candidate == NULL);
            } else {
                mc_nbt_document *candidate = NULL;
                status = mc_stored_chunk_build(owner, 7, &limits, &candidate, NULL);
                if (status == MC_STORED_CHUNK_OK)
                    mc_nbt_document_free(candidate);
                else
                    CHECK(candidate == NULL);
            }
            CHECK(status == MC_STORED_CHUNK_OK || status == MC_STORED_CHUNK_LIMIT);
            if (status == MC_STORED_CHUNK_OK)
                upper = middle;
            else
                lower = middle + 1u;
        }
        CHECK(lower > length && lower < 1048576);
        limits = mc_nbt_default_limits();
        limits.max_allocation_bytes = lower - 1u;
        if (operation == 0) {
            CHECK(mc_stored_chunk_decode(bytes, length, &limits, &result, &consumed, NULL) ==
                  MC_STORED_CHUNK_LIMIT);
            CHECK(result == owner && consumed == 98765);
            ++limits.max_allocation_bytes;
            mc_stored_chunk *bounded = NULL;
            CHECK(mc_stored_chunk_decode(bytes, length, &limits, &bounded, NULL, NULL) ==
                  MC_STORED_CHUNK_OK);
            CHECK(expect_payload(mc_stored_chunk_state(bounded), value) == 0);
            mc_stored_chunk_free(bounded);
        } else {
            CHECK(expect_build_failure(owner, &limits, sentinel, MC_STORED_CHUNK_LIMIT) == 0);
            ++limits.max_allocation_bytes;
            mc_nbt_document *bounded = NULL;
            CHECK(mc_stored_chunk_build(owner, 7, &limits, &bounded, NULL) == MC_STORED_CHUNK_OK);
            CHECK(same_tag(mc_nbt_document_root(sentinel), mc_nbt_document_root(bounded)) == 0);
            mc_nbt_document_free(bounded);
        }
    }
    uint8_t *built_bytes = NULL;
    size_t built_length = 0;
    CHECK(tree_bytes(mc_nbt_document_root(sentinel), &built_bytes, &built_length) == 0);
    for (size_t mode = 0; mode < 2; ++mode) {
        limits = mc_nbt_default_limits();
        if (mode == 0)
            limits.max_input_bytes = built_length - 1u;
        else
            limits.max_output_bytes = built_length - 1u;
        CHECK(expect_build_failure(owner, &limits, sentinel, MC_STORED_CHUNK_LIMIT) == 0);
    }
    limits = mc_nbt_default_limits();
    limits.max_input_bytes = limits.max_output_bytes = built_length;
    limits.max_depth = 3;
    limits.max_elements = MC_CHUNK_BLOCKS;
    mc_nbt_document *bounded = NULL;
    CHECK(mc_stored_chunk_build(owner, 7, &limits, &bounded, NULL) == MC_STORED_CHUNK_OK);
    CHECK(same_tag(mc_nbt_document_root(sentinel), mc_nbt_document_root(bounded)) == 0);
    mc_nbt_document_free(bounded);
    mc_nbt_document_free(sentinel);
    mc_stored_chunk_free(owner);
    free(built_bytes);
    free(bytes);
    free(value);
    return 0;
}

static int test_status_strings(void) {
    static const char *const expected[] = {"ok",
                                           "invalid argument",
                                           "missing Level",
                                           "missing Blocks",
                                           "wrong chunk field type",
                                           "unsupported chunk array shape",
                                           "unsupported entity or tile edit",
                                           "resource limit",
                                           "allocation failed",
                                           "invalid NBT record"};
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i)
        CHECK(strcmp(mc_stored_chunk_status_string((mc_stored_chunk_status)i), expected[i]) == 0);
    CHECK(strcmp(mc_stored_chunk_status_string((mc_stored_chunk_status)999),
                 "unknown stored chunk status") == 0);
    return 0;
}

int main(void) {
    if (test_create_and_independent_lifetimes() != 0 ||
        test_decode_defaults_and_ignored_time() != 0 ||
        test_exact_types_missing_fields_and_shapes() != 0 ||
        test_opaque_utf16_preservation_and_result_ownership() != 0 ||
        test_opaque_edit_rejections_and_empty_editing() != 0 ||
        test_frozen_normalization_and_descriptor_guards() != 0 ||
        test_create_arguments_and_allocation_boundary() != 0 ||
        test_decode_and_build_limits_and_failures() != 0 || test_status_strings() != 0)
        return 1;
    printf("stored chunk: %u constructed contract checks passed\n", checks);
    return 0;
}
