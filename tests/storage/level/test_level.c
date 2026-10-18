#include "mc/storage/level/level.h"

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

static const char *const field_names[] = {
    "RandomSeed", "SpawnX",  "SpawnY",   "SpawnZ",  "Time",        "LastPlayed", "SizeOnDisk",
    "LevelName",  "version", "rainTime", "raining", "thunderTime", "thundering", "Player"};

static const mc_nbt_type field_types[] = {
    MC_NBT_LONG,   MC_NBT_INT, MC_NBT_INT, MC_NBT_INT,  MC_NBT_LONG, MC_NBT_LONG, MC_NBT_LONG,
    MC_NBT_STRING, MC_NBT_INT, MC_NBT_INT, MC_NBT_BYTE, MC_NBT_INT,  MC_NBT_BYTE, MC_NBT_COMPOUND};

static const uint16_t root_name[] = {'R', 0, 0xd800};
static const uint16_t level_name[] = {'n', 0, 0xdfff, 0xffff, 'Z'};
static const uint8_t opaque_bytes[] = {0, 0x7f, 0x80, 0xff, 0x31};

typedef struct fixture {
    mc_nbt_tag root, data, fields[14];
    mc_nbt_tag root_extra, root_tail, data_extra, data_tail;
    mc_nbt_tag dimension, player_extra, player_payload;
    const mc_nbt_tag *root_items[3];
    const mc_nbt_tag *data_items[16];
    const mc_nbt_tag *player_items[2];
    const mc_nbt_tag *extra_items[1];
    uint16_t names[24][16];
    size_t name_count;
} fixture;

static void named(fixture *value, mc_nbt_tag *tag, const char *name, mc_nbt_type type)
{
    uint16_t *units = value->names[value->name_count++];
    size_t length = strlen(name);
    for (size_t i = 0; i < length; ++i) {
        units[i] = (uint16_t)(unsigned char)name[i];
    }
    tag->name = (mc_nbt_string){units, length};
    tag->type = type;
}

/* Synthetic schema-contract fixtures, not original-runtime parity evidence. */
static void fixture_init(fixture *value)
{
    memset(value, 0, sizeof(*value));
    value->root.type = MC_NBT_COMPOUND;
    value->root.name = (mc_nbt_string){root_name, sizeof(root_name) / sizeof(root_name[0])};
    named(value, &value->data, "Data", MC_NBT_COMPOUND);
    for (size_t i = 0; i < 14; ++i) {
        named(value, &value->fields[i], field_names[i], field_types[i]);
    }
    value->fields[0].value.long_value = INT64_MIN;
    value->fields[1].value.int_value = INT32_MIN;
    value->fields[2].value.int_value = INT32_MAX;
    value->fields[3].value.int_value = -1;
    value->fields[4].value.long_value = INT64_MAX;
    value->fields[5].value.long_value = -123456789;
    value->fields[6].value.long_value = INT64_MIN + 1;
    value->fields[7].value.string =
        (mc_nbt_string){level_name, sizeof(level_name) / sizeof(level_name[0])};
    value->fields[8].value.int_value = INT32_MIN;
    value->fields[9].value.int_value = INT32_MAX;
    value->fields[10].value.byte_value = -128;
    value->fields[11].value.int_value = INT32_MIN;
    value->fields[12].value.byte_value = 2;
    named(value, &value->dimension, "Dimension", MC_NBT_INT);
    value->dimension.value.int_value = INT32_MIN;
    named(value, &value->player_extra, "Opaque", MC_NBT_COMPOUND);
    named(value, &value->player_payload, "Payload", MC_NBT_BYTE_ARRAY);
    value->player_payload.value.byte_array.data = opaque_bytes;
    value->player_payload.value.byte_array.length = sizeof(opaque_bytes);
    value->extra_items[0] = &value->player_payload;
    value->player_extra.value.compound.items = value->extra_items;
    value->player_extra.value.compound.length = 1;
    value->player_items[0] = &value->dimension;
    value->player_items[1] = &value->player_extra;
    value->fields[13].value.compound.items = value->player_items;
    value->fields[13].value.compound.length = 2;
    named(value, &value->root_extra, "RootOnly", MC_NBT_BYTE_ARRAY);
    value->root_extra.value.byte_array = value->player_payload.value.byte_array;
    named(value, &value->root_tail, "RootTail", MC_NBT_LONG);
    value->root_tail.value.long_value = INT64_MAX - 1;
    named(value, &value->data_extra, "DataOnly", MC_NBT_BYTE_ARRAY);
    value->data_extra.value.byte_array = value->player_payload.value.byte_array;
    named(value, &value->data_tail, "DataTail", MC_NBT_STRING);
    value->data_tail.value.string = value->fields[7].value.string;
    static const size_t order[] = {7, 0, 13, 5, 2, 1, 3, 4, 6, 8, 9, 10, 11, 12};
    value->data_items[0] = &value->data_extra;
    for (size_t i = 0; i < 14; ++i) {
        value->data_items[i + 1u] = &value->fields[order[i]];
    }
    value->data_items[15] = &value->data_tail;
    value->data.value.compound.items = value->data_items;
    value->data.value.compound.length = 16;
    value->root_items[0] = &value->root_extra;
    value->root_items[1] = &value->data;
    value->root_items[2] = &value->root_tail;
    value->root.value.compound.items = value->root_items;
    value->root.value.compound.length = 3;
}

static const mc_nbt_tag *get(const mc_nbt_tag *parent, const char *name)
{
    uint16_t units[16];
    size_t length = strlen(name);
    for (size_t i = 0; i < length; ++i) {
        units[i] = (uint16_t)(unsigned char)name[i];
    }
    return mc_nbt_compound_get(parent, units, length);
}

static int tree_bytes(const mc_nbt_tag *root, uint8_t **out, size_t *length)
{
    size_t measured = 0;
    CHECK(mc_nbt_measure(root, NULL, &measured, NULL) == MC_NBT_OK);
    uint8_t *bytes = malloc(measured);
    CHECK(bytes != NULL);
    size_t written = 0;
    CHECK(mc_nbt_write(root, NULL, bytes, measured, &written, NULL) == MC_NBT_OK);
    CHECK(written == measured);
    *out = bytes;
    *length = written;
    return 0;
}

static int parsed(const mc_nbt_tag *root, mc_nbt_document **out)
{
    uint8_t *bytes = NULL;
    size_t length = 0;
    CHECK(tree_bytes(root, &bytes, &length) == 0);
    size_t consumed = 0;
    CHECK(mc_nbt_read_compound(bytes, length, NULL, out, &consumed, NULL) == MC_NBT_OK);
    CHECK(consumed == length);
    free(bytes);
    return 0;
}

static int expect_string(mc_nbt_string actual, const uint16_t *units, size_t length)
{
    CHECK(actual.length == length);
    if (length != 0) {
        CHECK(actual.units != NULL && memcmp(actual.units, units, length * sizeof(*units)) == 0);
    }
    return 0;
}

static int expect_same_tag(const mc_nbt_tag *left, const mc_nbt_tag *right)
{
    uint8_t *left_bytes = NULL;
    uint8_t *right_bytes = NULL;
    size_t left_length = 0, right_length = 0;
    CHECK(tree_bytes(left, &left_bytes, &left_length) == 0);
    CHECK(tree_bytes(right, &right_bytes, &right_length) == 0);
    CHECK(left_length == right_length && memcmp(left_bytes, right_bytes, left_length) == 0);
    free(left_bytes);
    free(right_bytes);
    return 0;
}

static int expect_decode_failure(const mc_nbt_tag *root, const char *field,
                                 mc_level_status expected)
{
    mc_level_info result, saved;
    memset(&result, 0xa5, sizeof(result));
    memcpy(&saved, &result, sizeof(saved));
    mc_level_error error = {MC_LEVEL_OK, NULL, {MC_NBT_OK, 0}};
    CHECK(mc_level_decode(root, &result, &error) == expected);
    CHECK(memcmp(&result, &saved, sizeof(result)) == 0);
    CHECK(error.status == expected && error.nbt.status == MC_NBT_OK);
    CHECK(field == NULL ? error.field == NULL
                        : error.field != NULL && strcmp(error.field, field) == 0);
    return 0;
}

static int expect_build_failure(const mc_level_info *info, const mc_nbt_tag *baseline,
                                const mc_nbt_limits *limits, mc_nbt_document *sentinel,
                                mc_level_status expected, const char *field)
{
    uint8_t *before = NULL;
    size_t before_length = 0;
    if (baseline != NULL) {
        CHECK(tree_bytes(baseline, &before, &before_length) == 0);
    }
    mc_level_info saved;
    if (info != NULL) {
        memcpy(&saved, info, sizeof(saved));
    }
    mc_nbt_document *result = sentinel;
    mc_level_error error = {MC_LEVEL_OK, NULL, {MC_NBT_OK, 0}};
    CHECK(mc_level_build(info, baseline, 1234, limits, &result, &error) == expected);
    CHECK(result == sentinel && error.status == expected);
    CHECK(field == NULL ? error.field == NULL
                        : error.field != NULL && strcmp(error.field, field) == 0);
    if (info != NULL) {
        CHECK(memcmp(&saved, info, sizeof(saved)) == 0);
    }
    if (baseline != NULL) {
        uint8_t *after = NULL;
        size_t after_length = 0;
        CHECK(tree_bytes(baseline, &after, &after_length) == 0);
        CHECK(before_length == after_length && memcmp(before, after, before_length) == 0);
        free(after);
    }
    free(before);
    return 0;
}

static int test_defaults_and_exact_types(void)
{
    mc_nbt_tag empty = {0};
    empty.type = MC_NBT_COMPOUND;
    mc_nbt_document *document = NULL;
    CHECK(parsed(&empty, &document) == 0);
    for (size_t present = 0; present < 2; ++present) {
        mc_level_info info;
        mc_level_error error;
        CHECK(mc_level_decode(mc_nbt_document_root(document), &info, &error) == MC_LEVEL_OK);
        CHECK(info.seed == 0 && info.spawn_x == 0 && info.spawn_y == 0 && info.spawn_z == 0);
        CHECK(info.time == 0 && info.last_played == 0 && info.size_on_disk == 0);
        CHECK(info.level_name.length == 0 && info.version == 0);
        CHECK(info.rain_time == 0 && !info.raining && info.thunder_time == 0 && !info.thundering);
        CHECK(info.player == NULL && info.player_dimension == 0);
        CHECK(error.status == MC_LEVEL_OK && error.field == NULL && error.nbt.status == MC_NBT_OK);
        if (present == 0) {
            mc_nbt_document *built = NULL;
            CHECK(mc_level_build(&info, NULL, 1234, NULL, &built, NULL) == MC_LEVEL_OK);
            const mc_nbt_tag *data = get(mc_nbt_document_root(built), "Data");
            CHECK(data->value.compound.length == 13 && get(data, "Player") == NULL);
            mc_level_info decoded;
            CHECK(mc_level_decode(mc_nbt_document_root(built), &decoded, NULL) == MC_LEVEL_OK);
            CHECK(decoded.seed == 0 && decoded.spawn_x == 0 && decoded.spawn_y == 0 &&
                  decoded.spawn_z == 0 && decoded.time == 0 && decoded.last_played == 1234 &&
                  decoded.size_on_disk == 0 && decoded.level_name.length == 0 &&
                  decoded.version == 0);
            CHECK(!decoded.raining && !decoded.thundering && decoded.rain_time == 0 &&
                  decoded.thunder_time == 0 && decoded.player == NULL);
            mc_nbt_document_free(built);
        }
        mc_nbt_document_free(document);
        if (present == 0) {
            fixture value;
            fixture_init(&value);
            value.data.value.compound.length = 0;
            value.root.value.compound.items = &value.root_items[1];
            value.root.value.compound.length = 1;
            CHECK(parsed(&value.root, &document) == 0);
        }
    }
    for (size_t i = 0; i < 14; ++i) {
        fixture value;
        fixture_init(&value);
        value.fields[i].type = field_types[i] == MC_NBT_BYTE ? MC_NBT_INT : MC_NBT_BYTE;
        memset(&value.fields[i].value, 0, sizeof(value.fields[i].value));
        CHECK(parsed(&value.root, &document) == 0);
        CHECK(expect_decode_failure(mc_nbt_document_root(document), field_names[i],
                                    MC_LEVEL_INVALID_FIELD) == 0);
        mc_nbt_document_free(document);
    }
    fixture value;
    fixture_init(&value);
    value.dimension.type = MC_NBT_BYTE;
    value.dimension.value.byte_value = -1;
    CHECK(parsed(&value.root, &document) == 0);
    CHECK(expect_decode_failure(mc_nbt_document_root(document), "Dimension",
                                MC_LEVEL_INVALID_FIELD) == 0);
    mc_nbt_document_free(document);
    fixture_init(&value);
    value.data.type = MC_NBT_BYTE;
    value.data.value.byte_value = 0;
    CHECK(parsed(&value.root, &document) == 0);
    CHECK(expect_decode_failure(mc_nbt_document_root(document), "Data", MC_LEVEL_INVALID_FIELD) ==
          0);
    mc_nbt_document_free(document);
    fixture_init(&value);
    value.fields[13].value.compound.length = 0;
    CHECK(parsed(&value.root, &document) == 0);
    mc_level_info info;
    CHECK(mc_level_decode(mc_nbt_document_root(document), &info, NULL) == MC_LEVEL_OK);
    CHECK(info.player != NULL && info.player_dimension == 0);
    CHECK(mc_level_decode(mc_nbt_document_root(document), NULL, NULL) == MC_LEVEL_INVALID_ARGUMENT);
    mc_nbt_document_free(document);
    CHECK(expect_decode_failure(NULL, NULL, MC_LEVEL_INVALID_ARGUMENT) == 0);
    mc_nbt_tag wrong_root = {0};
    wrong_root.type = MC_NBT_BYTE;
    CHECK(expect_decode_failure(&wrong_root, NULL, MC_LEVEL_INVALID_ARGUMENT) == 0);
    return 0;
}

static int test_boundaries_booleans_and_clock(void)
{
    static const int8_t booleans[] = {-128, -1, 0, 1, 2, 127};
    for (size_t i = 0; i < sizeof(booleans) / sizeof(booleans[0]); ++i) {
        fixture value;
        fixture_init(&value);
        value.fields[10].value.byte_value = booleans[i];
        value.fields[12].value.byte_value = booleans[5u - i];
        mc_nbt_document *source = NULL;
        CHECK(parsed(&value.root, &source) == 0);
        const mc_nbt_tag *root = mc_nbt_document_root(source);
        mc_level_info info;
        CHECK(mc_level_decode(root, &info, NULL) == MC_LEVEL_OK);
        CHECK(info.seed == INT64_MIN && info.time == INT64_MAX &&
              info.size_on_disk == INT64_MIN + 1);
        CHECK(info.spawn_x == INT32_MIN && info.spawn_y == INT32_MAX && info.spawn_z == -1);
        CHECK(info.version == INT32_MIN && info.rain_time == INT32_MAX &&
              info.thunder_time == INT32_MIN && info.player_dimension == INT32_MIN);
        CHECK(info.last_played == -123456789);
        CHECK(info.raining == (booleans[i] != 0) && info.thundering == (booleans[5u - i] != 0));
        CHECK(expect_string(info.level_name, level_name,
                            sizeof(level_name) / sizeof(level_name[0])) == 0);
        mc_level_info saved;
        memcpy(&saved, &info, sizeof(saved));
        int64_t clock = i % 2u == 0 ? INT64_MIN : INT64_MAX;
        mc_nbt_document *result = NULL;
        CHECK(mc_level_build(&info, root, clock, NULL, &result, NULL) == MC_LEVEL_OK);
        CHECK(memcmp(&info, &saved, sizeof(info)) == 0);
        const mc_nbt_tag *data = get(mc_nbt_document_root(result), "Data");
        CHECK(get(data, "LastPlayed")->value.long_value == clock);
        CHECK(get(data, "version")->value.int_value == INT32_MIN);
        CHECK(get(data, "raining")->value.byte_value == (booleans[i] != 0 ? 1 : 0));
        CHECK(get(data, "thundering")->value.byte_value == (booleans[5u - i] != 0 ? 1 : 0));
        mc_level_info decoded;
        CHECK(mc_level_decode(mc_nbt_document_root(result), &decoded, NULL) == MC_LEVEL_OK);
        CHECK(decoded.last_played == clock && decoded.time == info.time &&
              decoded.seed == info.seed && decoded.size_on_disk == info.size_on_disk);
        CHECK(decoded.level_name.units != info.level_name.units);
        CHECK(expect_string(decoded.level_name, level_name,
                            sizeof(level_name) / sizeof(level_name[0])) == 0);
        mc_nbt_document_free(result);
        mc_nbt_document_free(source);
    }
    return 0;
}

static int test_preservation_player_and_ownership(void)
{
    fixture value;
    fixture_init(&value);
    mc_nbt_document *source = NULL;
    CHECK(parsed(&value.root, &source) == 0);
    const mc_nbt_tag *root = mc_nbt_document_root(source);
    const mc_nbt_tag *data = get(root, "Data");
    mc_level_info info;
    CHECK(mc_level_decode(root, &info, NULL) == MC_LEVEL_OK);
    uint8_t *source_bytes = NULL;
    size_t source_length = 0;
    CHECK(tree_bytes(root, &source_bytes, &source_length) == 0);
    info.seed = 17;
    info.spawn_x = 29;
    info.version = 42;
    mc_nbt_document *result = NULL;
    CHECK(mc_level_build(&info, root, 1234, NULL, &result, NULL) == MC_LEVEL_OK);
    const mc_nbt_tag *built_root = mc_nbt_document_root(result);
    const mc_nbt_tag *built_data = get(built_root, "Data");
    CHECK(expect_string(built_root->name, root_name, sizeof(root_name) / sizeof(root_name[0])) ==
          0);
    CHECK(built_root->value.compound.length == root->value.compound.length);
    CHECK(built_data->value.compound.length == data->value.compound.length);
    for (size_t i = 0; i < root->value.compound.length; ++i) {
        mc_nbt_string name = root->value.compound.items[i]->name;
        CHECK(expect_string(built_root->value.compound.items[i]->name, name.units, name.length) ==
              0);
    }
    for (size_t i = 0; i < data->value.compound.length; ++i) {
        mc_nbt_string name = data->value.compound.items[i]->name;
        CHECK(expect_string(built_data->value.compound.items[i]->name, name.units, name.length) ==
              0);
    }
    CHECK(get(built_data, "RandomSeed")->value.long_value == 17);
    CHECK(get(built_data, "SpawnX")->value.int_value == 29);
    CHECK(get(built_data, "version")->value.int_value == 42);
    CHECK(expect_same_tag(get(root, "RootOnly"), get(built_root, "RootOnly")) == 0);
    CHECK(expect_same_tag(get(root, "RootTail"), get(built_root, "RootTail")) == 0);
    CHECK(expect_same_tag(get(data, "DataOnly"), get(built_data, "DataOnly")) == 0);
    CHECK(expect_same_tag(get(data, "DataTail"), get(built_data, "DataTail")) == 0);
    CHECK(expect_same_tag(info.player, get(built_data, "Player")) == 0);
    CHECK(info.player != get(built_data, "Player"));
    uint8_t *unchanged = NULL;
    size_t unchanged_length = 0;
    CHECK(tree_bytes(root, &unchanged, &unchanged_length) == 0);
    CHECK(unchanged_length == source_length && memcmp(unchanged, source_bytes, source_length) == 0);
    free(unchanged);
    free(source_bytes);

    mc_nbt_document *fresh = NULL;
    CHECK(mc_level_build(&info, NULL, 5678, NULL, &fresh, NULL) == MC_LEVEL_OK);
    const mc_nbt_tag *fresh_root = mc_nbt_document_root(fresh);
    const mc_nbt_tag *fresh_data = get(fresh_root, "Data");
    CHECK(fresh_root->name.length == 0 && fresh_root->value.compound.length == 1);
    CHECK(fresh_data->value.compound.length == 14);
    CHECK(get(fresh_root, "RootOnly") == NULL && get(fresh_root, "RootTail") == NULL);
    CHECK(get(fresh_data, "DataOnly") == NULL && get(fresh_data, "DataTail") == NULL);
    CHECK(expect_same_tag(info.player, get(fresh_data, "Player")) == 0);
    for (size_t i = 0; i < 14; ++i) {
        CHECK(fresh_data->value.compound.items[i] == get(fresh_data, field_names[i]));
    }
    mc_nbt_document_free(fresh);

    info.player = NULL;
    info.player_dimension = INT32_MAX;
    mc_nbt_document *dropped = NULL;
    CHECK(mc_level_build(&info, root, 9012, NULL, &dropped, NULL) == MC_LEVEL_OK);
    const mc_nbt_tag *dropped_data = get(mc_nbt_document_root(dropped), "Data");
    CHECK(get(dropped_data, "Player") == NULL && dropped_data->value.compound.length == 15);
    CHECK(get(dropped_data, "Dimension") == NULL);
    mc_level_info decoded;
    CHECK(mc_level_decode(mc_nbt_document_root(dropped), &decoded, NULL) == MC_LEVEL_OK);
    CHECK(decoded.player == NULL && decoded.player_dimension == 0);
    CHECK(expect_same_tag(get(data, "DataOnly"), get(dropped_data, "DataOnly")) == 0);
    mc_nbt_document_free(dropped);

    uint8_t *owned_bytes = NULL;
    size_t owned_length = 0;
    CHECK(tree_bytes(built_root, &owned_bytes, &owned_length) == 0);
    mc_nbt_document_free(source);
    CHECK(mc_level_decode(built_root, &decoded, NULL) == MC_LEVEL_OK);
    CHECK(decoded.seed == 17 && decoded.spawn_x == 29 && decoded.version == 42);
    CHECK(decoded.player != NULL && decoded.player_dimension == INT32_MIN);
    CHECK(expect_string(decoded.level_name, level_name,
                        sizeof(level_name) / sizeof(level_name[0])) == 0);
    uint8_t *after_free = NULL;
    size_t after_free_length = 0;
    CHECK(tree_bytes(built_root, &after_free, &after_free_length) == 0);
    CHECK(after_free_length == owned_length && memcmp(owned_bytes, after_free, owned_length) == 0);
    free(after_free);
    free(owned_bytes);
    mc_nbt_document_free(result);
    return 0;
}

static int test_build_limits_and_failures(void)
{
    fixture value;
    fixture_init(&value);
    mc_nbt_document *source = NULL;
    CHECK(parsed(&value.root, &source) == 0);
    const mc_nbt_tag *root = mc_nbt_document_root(source);
    mc_level_info info;
    CHECK(mc_level_decode(root, &info, NULL) == MC_LEVEL_OK);
    CHECK(expect_build_failure(NULL, root, NULL, source, MC_LEVEL_INVALID_ARGUMENT, NULL) == 0);
    CHECK(mc_level_build(&info, root, 0, NULL, NULL, NULL) == MC_LEVEL_INVALID_ARGUMENT);
    CHECK(expect_build_failure(&info, get(get(root, "Data"), "SpawnX"), NULL, source,
                               MC_LEVEL_INVALID_ARGUMENT, NULL) == 0);
    mc_level_info invalid = info;
    invalid.player = get(get(root, "Data"), "SpawnX");
    CHECK(expect_build_failure(&invalid, root, NULL, source, MC_LEVEL_INVALID_FIELD, "Player") ==
          0);
    invalid = info;
    invalid.level_name = (mc_nbt_string){NULL, 1};
    CHECK(expect_build_failure(&invalid, root, NULL, source, MC_LEVEL_NBT_ERROR, NULL) == 0);

    fixture bad;
    fixture_init(&bad);
    bad.dimension.type = MC_NBT_BYTE;
    bad.dimension.value.byte_value = -1;
    mc_nbt_document *bad_document = NULL;
    CHECK(parsed(&bad.root, &bad_document) == 0);
    invalid = info;
    invalid.player = get(get(mc_nbt_document_root(bad_document), "Data"), "Player");
    CHECK(expect_build_failure(&invalid, root, NULL, source, MC_LEVEL_INVALID_FIELD, "Dimension") ==
          0);
    mc_nbt_document_free(bad_document);
    fixture_init(&bad);
    bad.data.type = MC_NBT_BYTE;
    bad.data.value.byte_value = 0;
    CHECK(parsed(&bad.root, &bad_document) == 0);
    CHECK(expect_build_failure(&info, mc_nbt_document_root(bad_document), NULL, source,
                               MC_LEVEL_INVALID_FIELD, "Data") == 0);
    mc_nbt_document_free(bad_document);

    for (size_t i = 0; i < 8; ++i) {
        mc_nbt_limits limits = mc_nbt_default_limits();
        switch (i) {
            case 0:
                limits.max_input_bytes = 0;
                break;
            case 1:
                limits.max_output_bytes = 0;
                break;
            case 2:
                limits.max_allocation_bytes = 0;
                break;
            case 3:
                limits.max_tags = 1;
                break;
            case 4:
                limits.max_elements = 0;
                break;
            case 5:
                limits.max_elements = 15;
                break;
            case 6:
                limits.max_depth = 2;
                break;
            default:
                limits.max_depth = 4;
                break;
        }
        CHECK(expect_build_failure(&info, root, &limits, source, MC_LEVEL_LIMIT, NULL) == 0);
    }
    static const size_t invalid_depths[] = {0, 129, SIZE_MAX};
    for (size_t i = 0; i < sizeof(invalid_depths) / sizeof(invalid_depths[0]); ++i) {
        mc_nbt_limits limits = mc_nbt_default_limits();
        limits.max_depth = invalid_depths[i];
        CHECK(expect_build_failure(&info, root, &limits, source, MC_LEVEL_INVALID_ARGUMENT, NULL) ==
              0);
    }
    mc_nbt_document *result = NULL;
    CHECK(mc_level_build(&info, root, 1234, NULL, &result, NULL) == MC_LEVEL_OK);
    uint8_t *expected = NULL;
    size_t length = 0;
    CHECK(tree_bytes(mc_nbt_document_root(result), &expected, &length) == 0);
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_input_bytes = length - 1u;
    CHECK(expect_build_failure(&info, root, &limits, source, MC_LEVEL_LIMIT, NULL) == 0);
    limits = mc_nbt_default_limits();
    limits.max_output_bytes = length - 1u;
    CHECK(expect_build_failure(&info, root, &limits, source, MC_LEVEL_LIMIT, NULL) == 0);
    limits = mc_nbt_default_limits();
    limits.max_input_bytes = limits.max_output_bytes = length;
    limits.max_depth = 5;
    limits.max_elements = 16;
    mc_nbt_document *bounded = NULL;
    CHECK(mc_level_build(&info, root, 1234, &limits, &bounded, NULL) == MC_LEVEL_OK);
    CHECK(expect_same_tag(mc_nbt_document_root(result), mc_nbt_document_root(bounded)) == 0);
    mc_nbt_document_free(bounded);

    /* Exercise a nonzero peak allocation boundary through the public API,
     * without depending on the opaque document/arena implementation layout. */
    size_t lower = 0, upper = 65536;
    while (lower < upper) {
        size_t middle = lower + (upper - lower) / 2u;
        limits = mc_nbt_default_limits();
        limits.max_allocation_bytes = middle;
        mc_nbt_document *candidate = NULL;
        mc_level_status status = mc_level_build(&info, root, 1234, &limits, &candidate, NULL);
        CHECK(status == MC_LEVEL_OK || status == MC_LEVEL_LIMIT);
        if (status == MC_LEVEL_OK) {
            mc_nbt_document_free(candidate);
            upper = middle;
        } else {
            CHECK(candidate == NULL);
            lower = middle + 1u;
        }
    }
    CHECK(lower > length && lower < 65536);
    limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = lower - 1u;
    CHECK(expect_build_failure(&info, root, &limits, source, MC_LEVEL_LIMIT, NULL) == 0);
    ++limits.max_allocation_bytes;
    CHECK(mc_level_build(&info, root, 1234, &limits, &bounded, NULL) == MC_LEVEL_OK);
    CHECK(expect_same_tag(mc_nbt_document_root(result), mc_nbt_document_root(bounded)) == 0);
    mc_nbt_document_free(bounded);
    mc_nbt_document_free(result);
    free(expected);
    mc_nbt_document_free(source);
    return 0;
}

static int test_status_strings(void)
{
    static const char *const names[] = {"ok",
                                        "invalid argument",
                                        "wrong level field type",
                                        "resource limit",
                                        "allocation failed",
                                        "invalid NBT result"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        CHECK(strcmp(mc_level_status_string((mc_level_status)i), names[i]) == 0);
    }
    CHECK(strcmp(mc_level_status_string((mc_level_status)999), "unknown level status") == 0);
    return 0;
}

int main(void)
{
    if (test_defaults_and_exact_types() != 0 || test_boundaries_booleans_and_clock() != 0 ||
        test_preservation_player_and_ownership() != 0 || test_build_limits_and_failures() != 0 ||
        test_status_strings() != 0) {
        return 1;
    }
    printf("level metadata: %u checks passed\n", checks);
    return 0;
}
