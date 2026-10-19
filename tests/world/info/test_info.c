#include "mc/world/info/info.h"

#include "millis.h"

#include <stdio.h>
#include <string.h>

static unsigned int checks;
static unsigned int ordered_brackets;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

enum { DATA_FIELDS = 14 };
static const char *const keys[DATA_FIELDS] = {
    "RandomSeed", "SpawnX",  "SpawnY",   "SpawnZ",  "Time",        "LastPlayed", "SizeOnDisk",
    "LevelName",  "version", "rainTime", "raining", "thunderTime", "thundering", "Player"};
static const mc_nbt_type types[DATA_FIELDS] = {
    MC_NBT_LONG,   MC_NBT_INT, MC_NBT_INT, MC_NBT_INT,  MC_NBT_LONG, MC_NBT_LONG, MC_NBT_LONG,
    MC_NBT_STRING, MC_NBT_INT, MC_NBT_INT, MC_NBT_BYTE, MC_NBT_INT,  MC_NBT_BYTE, MC_NBT_COMPOUND};
static const uint16_t title[] = {'w', 0, 0xd800, 'x', 0xdc00, 0xffff};
static const uint16_t source_title[] = {'D', 0, 0xdfff};
static const uint16_t alternate_root[] = {'o', 'l', 'd', 0, 0xd800};

typedef struct fixture {
    mc_nbt_tag data, field[DATA_FIELDS], dimension, player_extra, extra_float, extra_double;
    const mc_nbt_tag *items[DATA_FIELDS + 2], *player_items[2];
    uint16_t names[18][16];
    size_t name_count;
} fixture;

static bool text_equal(mc_nbt_string left, mc_nbt_string right)
{
    return left.length == right.length &&
           (left.length == 0 ||
            (left.units != NULL && right.units != NULL &&
             memcmp(left.units, right.units, left.length * sizeof(uint16_t)) == 0));
}

static bool named_ascii(const mc_nbt_tag *tag, const char *name)
{
    if (tag == NULL || tag->name.length != strlen(name) ||
        (tag->name.length != 0 && tag->name.units == NULL))
        return false;
    for (size_t i = 0; i < tag->name.length; ++i)
        if (tag->name.units[i] != (uint16_t)(unsigned char)name[i])
            return false;
    return true;
}

static const mc_nbt_tag *find(const mc_nbt_tag *root, const char *name)
{
    uint16_t units[32];
    const size_t length = strlen(name);
    if (length > sizeof(units) / sizeof(units[0]))
        return NULL;
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)name[i];
    return mc_nbt_compound_get(root, units, length);
}

static void named(fixture *value, mc_nbt_tag *tag, const char *name, mc_nbt_type type)
{
    uint16_t *units = value->names[value->name_count++];
    const size_t length = strlen(name);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)name[i];
    tag->name = (mc_nbt_string){units, length};
    tag->type = type;
}

static void fixture_init(fixture *value)
{
    memset(value, 0, sizeof(*value));
    value->data.type = MC_NBT_COMPOUND;
    value->data.name =
        (mc_nbt_string){source_title, sizeof(source_title) / sizeof(source_title[0])};
    for (size_t i = 0; i < DATA_FIELDS; ++i) {
        named(value, &value->field[i], keys[i], types[i]);
        value->items[i] = &value->field[i];
    }
    value->field[0].value.long_value = INT64_MIN + 3;
    value->field[1].value.int_value = INT32_MIN;
    value->field[2].value.int_value = -17;
    value->field[3].value.int_value = INT32_MAX;
    value->field[4].value.long_value = INT64_MAX - 11;
    value->field[5].value.long_value = -999;
    value->field[6].value.long_value = INT64_MIN + 9;
    value->field[7].value.string = (mc_nbt_string){title, sizeof(title) / sizeof(title[0])};
    value->field[8].value.int_value = -19132;
    value->field[9].value.int_value = INT32_MIN;
    value->field[10].value.byte_value = -128;
    value->field[11].value.int_value = INT32_MAX;
    value->field[12].value.byte_value = 127;
    named(value, &value->dimension, "Dimension", MC_NBT_INT);
    value->dimension.value.int_value = -7;
    named(value, &value->player_extra, "PlayerRaw", MC_NBT_FLOAT);
    value->player_extra.value.float_bits = UINT32_C(0x7fa12345);
    value->player_items[0] = &value->dimension;
    value->player_items[1] = &value->player_extra;
    value->field[13].value.compound.items = value->player_items;
    value->field[13].value.compound.length = 2;
    named(value, &value->extra_float, "UnknownF", MC_NBT_FLOAT);
    value->extra_float.value.float_bits = UINT32_C(0x7faabcde);
    named(value, &value->extra_double, "UnknownD", MC_NBT_DOUBLE);
    value->extra_double.value.double_bits = UINT64_C(0x7ff0000000001234);
    value->items[14] = &value->extra_float;
    value->items[15] = &value->extra_double;
    value->data.value.compound.items = value->items;
    value->data.value.compound.length = DATA_FIELDS + 2u;
}

static mc_world_info_scalars prefix(size_t count)
{
    mc_world_info_scalars value = {0};
    if (count > 0)
        value.seed = INT64_MIN + 3;
    if (count > 1)
        value.spawn_x = INT32_MIN;
    if (count > 2)
        value.spawn_y = -17;
    if (count > 3)
        value.spawn_z = INT32_MAX;
    if (count > 4)
        value.time = INT64_MAX - 11;
    if (count > 5)
        value.last_played = -999;
    if (count > 6)
        value.size_on_disk = INT64_MIN + 9;
    if (count > 8)
        value.version = -19132;
    if (count > 9)
        value.rain_time = INT32_MIN;
    if (count > 10)
        value.raining = true;
    if (count > 11)
        value.thunder_time = INT32_MAX;
    if (count > 12)
        value.thundering = true;
    return value;
}

static bool scalars_equal(const mc_world_info_scalars *left, const mc_world_info_scalars *right)
{
    return left->seed == right->seed && left->spawn_x == right->spawn_x &&
           left->spawn_y == right->spawn_y && left->spawn_z == right->spawn_z &&
           left->time == right->time && left->last_played == right->last_played &&
           left->size_on_disk == right->size_on_disk &&
           left->player_dimension == right->player_dimension && left->version == right->version &&
           left->rain_time == right->rain_time && left->thunder_time == right->thunder_time &&
           left->raining == right->raining && left->thundering == right->thundering;
}

static bool same_fields(const mc_world_info_fields *left, const mc_world_info_fields *right)
{
    return scalars_equal(&left->value, &right->value) && left->name == right->name &&
           left->player == right->player;
}

static bool source_is(const mc_nbt_tag *root)
{
    if (root == NULL || root->type != MC_NBT_COMPOUND)
        return false;
    const mc_nbt_tag *a = find(root, "UnknownF"), *b = find(root, "UnknownD");
    return root->value.compound.length == 16 &&
           text_equal(root->name, (mc_nbt_string){source_title, sizeof(source_title) /
                                                                    sizeof(source_title[0])}) &&
           a != NULL && a->type == MC_NBT_FLOAT && a->value.float_bits == UINT32_C(0x7faabcde) &&
           b != NULL && b->type == MC_NBT_DOUBLE &&
           b->value.double_bits == UINT64_C(0x7ff0000000001234);
}

static int test_fresh(mc_world_info_side side)
{
    int result = 1;
    mc_world_info *info = NULL, *held = NULL;
    mc_world_info_name *name = NULL, *equal = NULL, *old = NULL;
    mc_world_info_fields fields = {0};
    mc_world_info_scalars expected = {0};
    mc_world_info_state state = {0};
    struct {
        uint64_t before;
        mc_world_info_fields value;
        uint64_t after;
    } guarded = {0}, saved;
    guarded.before = UINT64_C(0x0123456789abcdef);
    guarded.after = UINT64_C(0xfedcba9876543210);
    guarded.value.value.seed = 77;
    memcpy(&saved, &guarded, sizeof(saved));

    CHECK(mc_world_info_fields_get(NULL, &guarded.value) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(memcmp(&saved, &guarded, sizeof(saved)) == 0);
    CHECK(mc_world_info_allocate(side, &info) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_fields_get(info, &guarded.value) == MC_WORLD_INFO_OK);
    CHECK(guarded.before == saved.before && guarded.after == saved.after);
    CHECK(scalars_equal(&guarded.value.value, &expected) && guarded.value.name == NULL &&
          guarded.value.player == NULL);
    CHECK(mc_world_info_state_get(info, &state) == MC_WORLD_INFO_OK && state.side == side &&
          state.phase == MC_WORLD_INFO_RAW && state.last_store_pc == 0 &&
          mc_world_info_source(info) == NULL);
    CHECK(mc_world_info_time_set(info, 17) == MC_WORLD_INFO_INVALID_PHASE);
    CHECK(mc_world_info_name_set(info, NULL) == MC_WORLD_INFO_INVALID_PHASE);
    CHECK(mc_world_info_player_set(info, NULL) == MC_WORLD_INFO_INVALID_PHASE);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, sizeof(title) / sizeof(title[0])},
                                  SIZE_MAX, &name) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, sizeof(title) / sizeof(title[0])},
                                  SIZE_MAX, &equal) == MC_WORLD_INFO_OK);
    CHECK(name != equal &&
          text_equal(mc_world_info_name_text(name), mc_world_info_name_text(equal)));
    CHECK(mc_world_info_construct(info, INT64_MIN, name) == MC_WORLD_INFO_OK);
    expected.seed = INT64_MIN;
    CHECK(mc_world_info_fields_get(info, &fields) == MC_WORLD_INFO_OK &&
          scalars_equal(&fields.value, &expected) && fields.name == name && fields.player == NULL);
    CHECK(mc_world_info_state_get(info, &state) == MC_WORLD_INFO_OK &&
          state.phase == MC_WORLD_INFO_READY && state.last_store_pc == 11);
    CHECK(mc_world_info_retain(info) == MC_WORLD_INFO_OK);
    held = info;
    mc_world_info_release(info);
    info = NULL;
    mc_world_info_name_release(name);
    name = NULL; /* Info is now its sole owner. */
    CHECK(mc_world_info_name_set(held, fields.name) == MC_WORLD_INFO_OK);
    CHECK(text_equal(mc_world_info_name_text(fields.name),
                     (mc_nbt_string){title, sizeof(title) / sizeof(title[0])}));
    CHECK(mc_world_info_name_retain(fields.name) == MC_WORLD_INFO_OK);
    old = fields.name;
    CHECK(mc_world_info_name_set(held, equal) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_fields_get(held, &fields) == MC_WORLD_INFO_OK && fields.name == equal &&
          fields.name != old);
    CHECK(mc_world_info_name_set(held, NULL) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_fields_get(held, &fields) == MC_WORLD_INFO_OK && fields.name == NULL);
    CHECK(mc_world_info_construct(held, 0, equal) == MC_WORLD_INFO_INVALID_PHASE);
    mc_world_info_release(held);
    held = NULL;
    CHECK(text_equal(mc_world_info_name_text(old), mc_world_info_name_text(equal)));
    result = 0;
cleanup:
    mc_world_info_release(info);
    mc_world_info_release(held);
    mc_world_info_name_release(name);
    mc_world_info_name_release(equal);
    mc_world_info_name_release(old);
    return result;
}

static int test_wrong_type(mc_world_info_side side, size_t bad)
{
    int result = 1;
    fixture input;
    mc_world_info_data *data = NULL;
    mc_world_info *info = NULL;
    mc_world_info_fields fields = {0}, before = {0};
    mc_world_info_scalars expected = prefix(bad < 13u ? bad : 13u);
    mc_world_info_state state = {0};
    mc_world_info_error error = {0};
    static const unsigned int stores[] = {11, 21, 31, 41, 51, 61, 71, 81, 91, 101, 111, 121, 131};
    const unsigned int pc = bad == 14u ? 150u : bad == 0 ? 0u : stores[bad < 13u ? bad - 1u : 12u];
    fixture_init(&input);
    mc_nbt_tag *wrong = bad == 14u ? &input.dimension : &input.field[bad];
    wrong->type = wrong->type == MC_NBT_BYTE ? MC_NBT_INT : MC_NBT_BYTE;
    memset(&wrong->value, 0, sizeof(wrong->value));
    CHECK(mc_world_info_data_copy(&input.data, NULL, &data, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_allocate(side, &info) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_nbt(info, data, &error) == MC_WORLD_INFO_INVALID_FIELD);
    CHECK(error.status == MC_WORLD_INFO_INVALID_FIELD && error.field != NULL &&
          strcmp(error.field, bad == 14u ? "Dimension" : keys[bad]) == 0);
    CHECK(mc_world_info_state_get(info, &state) == MC_WORLD_INFO_OK &&
          state.phase == MC_WORLD_INFO_FAILED_CONSTRUCTION && state.last_store_pc == pc);
    CHECK(mc_world_info_fields_get(info, &fields) == MC_WORLD_INFO_OK &&
          scalars_equal(&fields.value, &expected));
    CHECK(fields.name == (bad > 7u ? mc_world_info_data_name(data) : NULL));
    CHECK(fields.player == (bad == 14u ? mc_world_info_data_player(data) : NULL));
    mc_world_info_data_release(data);
    data = NULL;
    if (fields.name != NULL)
        CHECK(text_equal(mc_world_info_name_text(fields.name),
                         (mc_nbt_string){title, sizeof(title) / sizeof(title[0])}));
    if (fields.player != NULL)
        CHECK(mc_world_info_player_root(fields.player) != NULL);
    before = fields;
    CHECK(mc_world_info_construct_nbt(info, NULL, &error) == MC_WORLD_INFO_INVALID_PHASE);
    CHECK(mc_world_info_construct(info, 0, NULL) == MC_WORLD_INFO_INVALID_PHASE);
    CHECK(mc_world_info_time_set(info, 0) == MC_WORLD_INFO_INVALID_PHASE);
    CHECK(mc_world_info_fields_get(info, &fields) == MC_WORLD_INFO_OK &&
          same_fields(&fields, &before));
    result = 0;
cleanup:
    mc_world_info_release(info);
    mc_world_info_data_release(data);
    return result;
}

static int test_copy_lifetimes(mc_world_info_side side)
{
    int result = 1;
    fixture input;
    mc_world_info_data *data = NULL, *other = NULL, *data_alias = NULL;
    mc_world_info *info[4] = {NULL, NULL, NULL, NULL};
    mc_world_info_name *held_name = NULL;
    mc_world_info_player *held_player = NULL;
    mc_world_info_fields original = {0}, current = {0}, fields[4] = {0};
    mc_world_info_scalars expected = prefix(13);
    mc_world_info_state state = {0};
    mc_world_info_error error = {0};
    const mc_nbt_tag *source = NULL;
    fixture_init(&input);
    expected.player_dimension = -7;
    CHECK(mc_world_info_data_copy(&input.data, NULL, &data, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_data_copy(&input.data, NULL, &other, &error) == MC_WORLD_INFO_OK);
    CHECK(data != other && mc_world_info_data_name(data) != mc_world_info_data_name(other) &&
          mc_world_info_data_player(data) != mc_world_info_data_player(other));
    CHECK(mc_world_info_data_retain(data) == MC_WORLD_INFO_OK);
    data_alias = data;
    mc_world_info_data_release(data);
    data = NULL;
    for (size_t i = 0; i < 4; ++i)
        CHECK(mc_world_info_allocate(side, &info[i]) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_nbt(info[0], data_alias, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_nbt(info[1], data_alias, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_nbt(info[2], other, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_copy(info[3], info[0]) == MC_WORLD_INFO_OK);
    for (size_t i = 0; i < 4; ++i) {
        CHECK(mc_world_info_fields_get(info[i], &fields[i]) == MC_WORLD_INFO_OK &&
              scalars_equal(&fields[i].value, &expected));
        CHECK(mc_world_info_state_get(info[i], &state) == MC_WORLD_INFO_OK && state.side == side &&
              state.phase == MC_WORLD_INFO_READY && state.last_store_pc == (i == 3 ? 121u : 163u));
    }
    original = fields[0];
    source = mc_world_info_source(info[0]);
    CHECK(same_fields(&original, &fields[1]) && same_fields(&original, &fields[3]));
    CHECK(fields[2].name != original.name && fields[2].player != original.player &&
          text_equal(mc_world_info_name_text(fields[2].name),
                     mc_world_info_name_text(original.name)));
    CHECK(source != &input.data && source == mc_world_info_source(info[1]) &&
          source == mc_world_info_source(info[3]) && source != mc_world_info_source(info[2]) &&
          source_is(source));
    CHECK(mc_world_info_name_retain(original.name) == MC_WORLD_INFO_OK);
    held_name = original.name;
    CHECK(mc_world_info_player_retain(original.player) == MC_WORLD_INFO_OK);
    held_player = original.player;
    mc_world_info_data_release(data_alias);
    data_alias = NULL;
    mc_world_info_data_release(other);
    other = NULL;
    memset(&input, 0, sizeof(input)); /* Copied owners may not borrow this input. */
    CHECK(source_is(source));
    CHECK(mc_world_info_time_set(info[0], INT64_MIN) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_size_set(info[0], INT64_MAX) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_spawn_set(info[0], INT32_MAX, INT32_MIN, -1) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_version_set(info[0], INT32_MIN) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_rain_time_set(info[0], INT32_MAX) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_thunder_time_set(info[0], INT32_MIN) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_raining_set(info[0], false) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_thundering_set(info[0], false) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_name_set(info[0], NULL) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_player_set(info[0], NULL) ==
          (side == MC_WORLD_INFO_CLIENT ? MC_WORLD_INFO_OK : MC_WORLD_INFO_UNSUPPORTED_OPERATION));
    CHECK(mc_world_info_fields_get(info[0], &current) == MC_WORLD_INFO_OK);
    mc_world_info_scalars changed = {.seed = INT64_MIN + 3,
                                     .spawn_x = INT32_MAX,
                                     .spawn_y = INT32_MIN,
                                     .spawn_z = -1,
                                     .time = INT64_MIN,
                                     .last_played = -999,
                                     .size_on_disk = INT64_MAX,
                                     .player_dimension = -7,
                                     .version = INT32_MIN,
                                     .rain_time = INT32_MAX,
                                     .thunder_time = INT32_MIN};
    CHECK(scalars_equal(&current.value, &changed) && current.name == NULL &&
          current.player == (side == MC_WORLD_INFO_CLIENT ? NULL : original.player));
    for (size_t i = 1; i < 4; ++i) {
        CHECK(mc_world_info_fields_get(info[i], &current) == MC_WORLD_INFO_OK &&
              same_fields(&current, &fields[i]));
    }
    mc_world_info_release(info[0]);
    info[0] = NULL;
    CHECK(source_is(mc_world_info_source(info[1])) && source_is(mc_world_info_source(info[3])));
    for (size_t i = 1; i < 4; ++i) {
        mc_world_info_release(info[i]);
        info[i] = NULL;
    }
    CHECK(text_equal(mc_world_info_name_text(held_name),
                     (mc_nbt_string){title, sizeof(title) / sizeof(title[0])}));
    const mc_nbt_tag *player = mc_world_info_player_root(held_player);
    const mc_nbt_tag *raw = find(player, "PlayerRaw");
    CHECK(player != NULL && raw != NULL && raw->type == MC_NBT_FLOAT &&
          raw->value.float_bits == UINT32_C(0x7fa12345));
    result = 0;
cleanup:
    for (size_t i = 0; i < 4; ++i)
        mc_world_info_release(info[i]);
    mc_world_info_data_release(data);
    mc_world_info_data_release(other);
    mc_world_info_data_release(data_alias);
    mc_world_info_name_release(held_name);
    mc_world_info_player_release(held_player);
    return result;
}

static bool record_is(const mc_nbt_tag *root, const mc_world_info_fields *fields,
                      int64_t last_played, bool same_player)
{
    static const size_t order[] = {0, 1, 2, 3, 4, 6, 5, 7, 8, 9, 10, 11, 12};
    if (root == NULL || root->type != MC_NBT_COMPOUND || root->name.length != 0 ||
        root->value.compound.length != (fields->player == NULL ? 13u : 14u))
        return false;
    for (size_t i = 0; i < 13; ++i) {
        const mc_nbt_tag *tag = root->value.compound.items[i];
        if (!named_ascii(tag, keys[order[i]]) || tag->type != types[order[i]])
            return false;
        switch (i) {
            case 0:
                if (tag->value.long_value != fields->value.seed)
                    return false;
                break;
            case 1:
                if (tag->value.int_value != fields->value.spawn_x)
                    return false;
                break;
            case 2:
                if (tag->value.int_value != fields->value.spawn_y)
                    return false;
                break;
            case 3:
                if (tag->value.int_value != fields->value.spawn_z)
                    return false;
                break;
            case 4:
                if (tag->value.long_value != fields->value.time)
                    return false;
                break;
            case 5:
                if (tag->value.long_value != fields->value.size_on_disk)
                    return false;
                break;
            case 6:
                if (tag->value.long_value != last_played)
                    return false;
                break;
            case 7:
                if (!text_equal(tag->value.string, mc_world_info_name_text(fields->name)))
                    return false;
                break;
            case 8:
                if (tag->value.int_value != fields->value.version)
                    return false;
                break;
            case 9:
                if (tag->value.int_value != fields->value.rain_time)
                    return false;
                break;
            case 10:
                if (tag->value.byte_value != (fields->value.raining ? 1 : 0))
                    return false;
                break;
            case 11:
                if (tag->value.int_value != fields->value.thunder_time)
                    return false;
                break;
            case 12:
                if (tag->value.byte_value != (fields->value.thundering ? 1 : 0))
                    return false;
                break;
            default:
                return false;
        }
    }
    if (find(root, "Data") != NULL || find(root, "UnknownF") != NULL ||
        find(root, "UnknownD") != NULL)
        return false;
    if (fields->player != NULL) {
        const mc_nbt_tag *player = root->value.compound.items[13];
        if (!named_ascii(player, "Player") || player->type != MC_NBT_COMPOUND ||
            (same_player && player != mc_world_info_player_root(fields->player)))
            return false;
        const mc_nbt_tag *dimension = find(player, "Dimension");
        const mc_nbt_tag *expected = find(mc_world_info_player_root(fields->player), "Dimension");
        if (dimension == NULL || expected == NULL || dimension->type != expected->type)
            return false;
        if (dimension->type == MC_NBT_INT &&
            dimension->value.int_value != expected->value.int_value)
            return false;
        if (dimension->type == MC_NBT_BYTE &&
            dimension->value.byte_value != expected->value.byte_value)
            return false;
    }
    return true;
}

static int test_records(mc_world_info_side side)
{
    int result = 1;
    fixture input;
    mc_world_info_data *data = NULL;
    mc_world_info *info = NULL, *copy = NULL;
    mc_world_info_player *replacement = NULL;
    mc_world_info_record *record = NULL;
    mc_nbt_document *decoded = NULL;
    mc_world_info_fields fields = {0}, after = {0};
    mc_world_info_error error = {0};
    mc_nbt_error nbt = {0};
    mc_nbt_limits limits = mc_nbt_default_limits();
    const mc_nbt_tag *player = NULL, *root = NULL, *tag = NULL;
    int64_t lower = 0, upper = 0, stamp = 0;
    size_t measured = 0, written = 0, consumed = 0;
    uint8_t bytes[4096], saved[4096];
    fixture_init(&input);
    CHECK(mc_world_info_data_copy(&input.data, NULL, &data, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_allocate(side, &info) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_nbt(info, data, &error) == MC_WORLD_INFO_OK);
    input.field[13].name =
        (mc_nbt_string){alternate_root, sizeof(alternate_root) / sizeof(alternate_root[0])};
    input.dimension.type = MC_NBT_BYTE;
    input.dimension.value.byte_value = -9;
    CHECK(mc_world_info_player_copy(&input.field[13], NULL, &replacement, &error) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_info_player_set(info, replacement) ==
          (side == MC_WORLD_INFO_CLIENT ? MC_WORLD_INFO_OK : MC_WORLD_INFO_UNSUPPORTED_OPERATION));
    CHECK(mc_world_info_fields_get(info, &fields) == MC_WORLD_INFO_OK &&
          fields.value.player_dimension == -7);
    if (side == MC_WORLD_INFO_CLIENT) {
        CHECK(fields.player == replacement);
        mc_world_info_player_release(replacement);
        replacement = NULL; /* Info is sole holder. */
        CHECK(mc_world_info_player_set(info, fields.player) == MC_WORLD_INFO_OK);
        CHECK(mc_world_info_player_root(fields.player) != NULL &&
              text_equal(mc_world_info_player_root(fields.player)->name,
                         (mc_nbt_string){alternate_root,
                                         sizeof(alternate_root) / sizeof(alternate_root[0])}));
    } else {
        CHECK(fields.player != replacement && fields.player == mc_world_info_data_player(data));
    }
    CHECK(mc_world_info_allocate(side, &copy) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_copy(copy, info) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_fields_get(copy, &after) == MC_WORLD_INFO_OK &&
          same_fields(&fields, &after));
    player = mc_world_info_player_root(fields.player);
    mc_world_info_data_release(data);
    data = NULL;
    CHECK(mc_platform_current_time_millis(&lower) == MC_PLATFORM_MILLIS_OK);
    CHECK(mc_world_info_record_create(info, &record, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_platform_current_time_millis(&upper) == MC_PLATFORM_MILLIS_OK);
    root = mc_world_info_record_root(record);
    tag = find(root, "LastPlayed");
    CHECK(tag != NULL && tag->type == MC_NBT_LONG);
    stamp = tag->value.long_value;
    if (lower <= upper) {
        CHECK(lower <= stamp && stamp <= upper);
        ++ordered_brackets;
    } else {
        fprintf(
            stderr,
            "WorldInfo: wall-clock reversal in serialization bracket; ordering check skipped\n");
    }
    CHECK(mc_world_info_record_name(record) == fields.name);
    CHECK(record_is(root, &fields, stamp, true));
    CHECK(find(root, "Player") == player && named_ascii(player, "Player"));
    CHECK(mc_world_info_fields_get(copy, &after) == MC_WORLD_INFO_OK &&
          after.player == fields.player && mc_world_info_player_root(after.player) == player &&
          named_ascii(mc_world_info_player_root(after.player), "Player"));
    CHECK(mc_world_info_fields_get(info, &after) == MC_WORLD_INFO_OK &&
          same_fields(&fields, &after) && after.value.last_played == -999);
    CHECK(source_is(mc_world_info_source(info)) && source_is(mc_world_info_source(copy)));
    if (side == MC_WORLD_INFO_CLIENT) {
        CHECK(find(player, "Dimension") != NULL && find(player, "Dimension")->type == MC_NBT_BYTE &&
              find(player, "Dimension")->value.byte_value == -9);
        CHECK(mc_world_info_player_set(info, NULL) == MC_WORLD_INFO_OK);
        CHECK(mc_world_info_fields_get(info, &after) == MC_WORLD_INFO_OK && after.player == NULL &&
              after.value.player_dimension == -7);
    }
    CHECK(mc_nbt_measure(root, NULL, &measured, &nbt) == MC_NBT_OK && measured <= sizeof(bytes));
    memset(bytes, 0xa5, sizeof(bytes));
    CHECK(mc_nbt_write(root, NULL, bytes, sizeof(bytes), &written, &nbt) == MC_NBT_OK &&
          written == measured);
    for (size_t i = written; i < sizeof(bytes); ++i)
        CHECK(bytes[i] == 0xa5);
    CHECK(mc_nbt_read_compound(bytes, written, NULL, &decoded, &consumed, &nbt) == MC_NBT_OK &&
          consumed == written);
    CHECK(record_is(mc_nbt_document_root(decoded), &fields, stamp, false));
    limits.max_output_bytes = 0;
    measured = 777;
    written = 888;
    memset(bytes, 0xa5, sizeof(bytes));
    memcpy(saved, bytes, sizeof(saved));
    CHECK(mc_nbt_measure(root, &limits, &measured, &nbt) == MC_NBT_LIMIT && measured == 777);
    CHECK(mc_nbt_write(root, &limits, bytes, sizeof(bytes), &written, &nbt) == MC_NBT_LIMIT &&
          written == 888);
    CHECK(memcmp(saved, bytes, sizeof(saved)) == 0 && named_ascii(player, "Player"));
    CHECK(mc_world_info_fields_get(copy, &after) == MC_WORLD_INFO_OK &&
          after.player == fields.player &&
          named_ascii(mc_world_info_player_root(after.player), "Player"));
    mc_world_info_release(info);
    info = NULL;
    mc_world_info_release(copy);
    copy = NULL;
    mc_world_info_player_release(replacement);
    replacement = NULL;
    CHECK(mc_world_info_record_root(record) == root && record_is(root, &fields, stamp, true));
    result = 0;
cleanup:
    mc_nbt_document_free(decoded);
    mc_world_info_record_release(record);
    mc_world_info_release(info);
    mc_world_info_release(copy);
    mc_world_info_player_release(replacement);
    mc_world_info_data_release(data);
    return result;
}

static int test_missing_and_phases(mc_world_info_side side)
{
    int result = 1;
    fixture input;
    mc_world_info_data *data = NULL;
    mc_world_info *empty = NULL, *null_name = NULL, *raw = NULL, *failed = NULL, *cross = NULL;
    mc_world_info_record *record = NULL, *candidate = NULL;
    mc_world_info_fields fields = {0}, zero = {0};
    mc_world_info_state state = {0};
    mc_world_info_error error = {0};
    fixture_init(&input);
    input.data.value.compound.length = 0;
    CHECK(mc_world_info_data_copy(&input.data, NULL, &data, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_data_name(data) != NULL &&
          mc_world_info_name_text(mc_world_info_data_name(data)).length == 0 &&
          mc_world_info_data_player(data) == NULL);
    CHECK(mc_world_info_allocate(side, &empty) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_nbt(empty, data, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_fields_get(empty, &fields) == MC_WORLD_INFO_OK &&
          scalars_equal(&fields.value, &zero.value) &&
          fields.name == mc_world_info_data_name(data) && fields.player == NULL);
    CHECK(mc_world_info_state_get(empty, &state) == MC_WORLD_INFO_OK &&
          state.phase == MC_WORLD_INFO_READY && state.last_store_pc == 131);
    CHECK(mc_world_info_allocate(side, &raw) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct(raw, 0, fields.name) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_record_create(raw, &record, &error) == MC_WORLD_INFO_OK);
    const mc_nbt_tag *last = find(mc_world_info_record_root(record), "LastPlayed");
    CHECK(last != NULL && last->type == MC_NBT_LONG &&
          record_is(mc_world_info_record_root(record), &fields, last->value.long_value, true));
    CHECK(mc_world_info_allocate(side, &null_name) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct(null_name, 0, NULL) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_fields_get(null_name, &fields) == MC_WORLD_INFO_OK &&
          same_fields(&fields, &zero));
    candidate = record;
    CHECK(mc_world_info_record_create(null_name, &candidate, &error) ==
          MC_WORLD_INFO_INVALID_FIELD);
    CHECK(candidate == record && error.field != NULL && strcmp(error.field, "LevelName") == 0);
    CHECK(mc_world_info_fields_get(null_name, &fields) == MC_WORLD_INFO_OK &&
          same_fields(&fields, &zero));
    CHECK(mc_world_info_allocate(side, &failed) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_nbt(failed, NULL, &error) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(error.field != NULL && strcmp(error.field, "RandomSeed") == 0);
    CHECK(mc_world_info_state_get(failed, &state) == MC_WORLD_INFO_OK &&
          state.phase == MC_WORLD_INFO_FAILED_CONSTRUCTION && state.last_store_pc == 0);
    CHECK(mc_world_info_fields_get(failed, &fields) == MC_WORLD_INFO_OK &&
          same_fields(&fields, &zero));
    CHECK(mc_world_info_construct_copy(failed, empty) == MC_WORLD_INFO_INVALID_PHASE);
    CHECK(mc_world_info_record_create(failed, &candidate, &error) == MC_WORLD_INFO_INVALID_PHASE &&
          candidate == record);
    CHECK(mc_world_info_allocate(side == MC_WORLD_INFO_CLIENT ? MC_WORLD_INFO_SERVER
                                                              : MC_WORLD_INFO_CLIENT,
                                 &cross) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_copy(cross, empty) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_state_get(cross, &state) == MC_WORLD_INFO_OK &&
          state.phase == MC_WORLD_INFO_FAILED_CONSTRUCTION && state.last_store_pc == 0);
    mc_world_info_release(failed);
    failed = NULL;
    CHECK(mc_world_info_allocate(side, &failed) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_copy(failed, NULL) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_state_get(failed, &state) == MC_WORLD_INFO_OK &&
          state.phase == MC_WORLD_INFO_FAILED_CONSTRUCTION && state.last_store_pc == 0);
    mc_world_info_release(failed);
    failed = NULL;
    mc_world_info_release(raw);
    raw = NULL;
    CHECK(mc_world_info_allocate(side, &raw) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_allocate(side, &failed) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_copy(failed, raw) == MC_WORLD_INFO_INVALID_PHASE);
    CHECK(mc_world_info_state_get(failed, &state) == MC_WORLD_INFO_OK &&
          state.phase == MC_WORLD_INFO_FAILED_CONSTRUCTION && state.last_store_pc == 0);
    result = 0;
cleanup:
    if (candidate != record)
        mc_world_info_record_release(candidate);
    mc_world_info_record_release(record);
    mc_world_info_release(empty);
    mc_world_info_release(null_name);
    mc_world_info_release(raw);
    mc_world_info_release(failed);
    mc_world_info_release(cross);
    mc_world_info_data_release(data);
    return result;
}

static int test_native_contracts(void)
{
    int result = 1;
    fixture input;
    mc_world_info_name *name = NULL, *name_out = NULL;
    mc_world_info_player *player = NULL, *player_out = NULL;
    mc_world_info_data *data = NULL, *data_out = NULL;
    mc_world_info *info = NULL, *info_out = NULL;
    mc_world_info_error error = {0};
    mc_nbt_limits limits = mc_nbt_default_limits();
    mc_world_info_state state = {MC_WORLD_INFO_SERVER, MC_WORLD_INFO_READY, 987}, saved;
    fixture_init(&input);
    CHECK(mc_world_info_name_copy((mc_nbt_string){NULL, 0}, SIZE_MAX, &name) == MC_WORLD_INFO_OK);
    CHECK(name != NULL && mc_world_info_name_text(name).length == 0);
    CHECK(mc_world_info_player_copy(&input.field[13], NULL, &player, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_data_copy(&input.data, NULL, &data, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &info) == MC_WORLD_INFO_OK);
    name_out = name;
    player_out = player;
    data_out = data;
    info_out = info;
    CHECK(mc_world_info_name_copy((mc_nbt_string){NULL, 0}, 0, &name_out) == MC_WORLD_INFO_LIMIT &&
          name_out == name);
    CHECK(mc_world_info_name_copy((mc_nbt_string){NULL, 1}, SIZE_MAX, &name_out) ==
              MC_WORLD_INFO_INVALID_ARGUMENT &&
          name_out == name);
    limits.max_allocation_bytes = 0;
    CHECK(mc_world_info_player_copy(&input.field[13], &limits, &player_out, &error) ==
              MC_WORLD_INFO_LIMIT &&
          player_out == player);
    CHECK(mc_world_info_data_copy(&input.data, &limits, &data_out, &error) == MC_WORLD_INFO_LIMIT &&
          data_out == data);
    limits = mc_nbt_default_limits();
    limits.max_depth = 0;
    CHECK(mc_world_info_player_copy(&input.field[13], &limits, &player_out, &error) ==
              MC_WORLD_INFO_INVALID_ARGUMENT &&
          player_out == player);
    CHECK(mc_world_info_data_copy(&input.data, &limits, &data_out, &error) ==
              MC_WORLD_INFO_INVALID_ARGUMENT &&
          data_out == data);
    CHECK(mc_world_info_allocate((mc_world_info_side)2, &info_out) ==
              MC_WORLD_INFO_INVALID_ARGUMENT &&
          info_out == info);
    CHECK(mc_world_info_allocate(MC_WORLD_INFO_CLIENT, NULL) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_name_copy((mc_nbt_string){NULL, 0}, SIZE_MAX, NULL) ==
          MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_player_copy(NULL, NULL, &player_out, &error) ==
              MC_WORLD_INFO_INVALID_ARGUMENT &&
          player_out == player);
    CHECK(mc_world_info_data_copy(NULL, NULL, &data_out, &error) ==
              MC_WORLD_INFO_INVALID_ARGUMENT &&
          data_out == data);
    CHECK(mc_world_info_retain(NULL) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_name_retain(NULL) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_player_retain(NULL) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_data_retain(NULL) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_fields_get(info, NULL) == MC_WORLD_INFO_INVALID_ARGUMENT);
    memcpy(&saved, &state, sizeof(saved));
    CHECK(mc_world_info_state_get(NULL, &state) == MC_WORLD_INFO_INVALID_ARGUMENT &&
          memcmp(&saved, &state, sizeof(saved)) == 0);
    CHECK(mc_world_info_state_get(info, NULL) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_name_text(NULL).units == NULL && mc_world_info_name_text(NULL).length == 0);
    CHECK(mc_world_info_player_root(NULL) == NULL && mc_world_info_record_root(NULL) == NULL &&
          mc_world_info_record_name(NULL) == NULL && mc_world_info_source(NULL) == NULL);
    CHECK(mc_world_info_time_set(NULL, 0) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_name_set(NULL, NULL) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_player_set(NULL, NULL) == MC_WORLD_INFO_INVALID_ARGUMENT);
    mc_world_info_release(NULL);
    mc_world_info_name_release(NULL);
    mc_world_info_player_release(NULL);
    mc_world_info_data_release(NULL);
    mc_world_info_record_release(NULL);
    result = 0;
cleanup:
    if (name_out != name)
        mc_world_info_name_release(name_out);
    if (player_out != player)
        mc_world_info_player_release(player_out);
    if (data_out != data)
        mc_world_info_data_release(data_out);
    if (info_out != info)
        mc_world_info_release(info_out);
    mc_world_info_name_release(name);
    mc_world_info_player_release(player);
    mc_world_info_data_release(data);
    mc_world_info_release(info);
    return result;
}

static int test_data_reference_setters(mc_world_info_side side)
{
    int result = 1;
    fixture input, wrong;
    mc_world_info_data *data = NULL, *broken = NULL;
    mc_world_info *old_info = NULL, *fresh = NULL, *loaded = NULL, *repaired = NULL;
    mc_world_info_name *name = NULL, *empty = NULL, *before_name = NULL;
    mc_world_info_name *held_before = NULL, *held_name = NULL, *empty_literal = NULL;
    mc_world_info_player *player = NULL, *held_player = NULL;
    mc_world_info_fields old = {0}, current = {0}, fresh_fields = {0};
    mc_world_info_error error = {0};
    mc_world_info_scalars expected = prefix(13), fresh_expected = {.seed = INT64_MIN + 3};
    const mc_nbt_tag *source = NULL, *player_root = NULL;
    static const uint16_t before_units[] = {'B', 'e', 'f', 'o', 'r', 'e'};
    fixture_init(&input);
    fixture_init(&wrong);
    expected.player_dimension = -7;
    CHECK(mc_world_info_data_copy(&input.data, NULL, &data, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_allocate(side, &old_info) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_nbt(old_info, data, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_fields_get(old_info, &old) == MC_WORLD_INFO_OK &&
          scalars_equal(&old.value, &expected));
    source = mc_world_info_source(old_info);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, sizeof(title) / sizeof(title[0])},
                                  SIZE_MAX, &name) == MC_WORLD_INFO_OK);
    CHECK(name != old.name &&
          text_equal(mc_world_info_name_text(name), mc_world_info_name_text(old.name)));
    CHECK(mc_world_info_data_name_set(data, name) == MC_WORLD_INFO_OK &&
          mc_world_info_data_name(data) == name);
    mc_world_info_name_release(name);
    name = NULL; /* Data now owns this reference alone. */
    CHECK(mc_world_info_data_name_set(data, mc_world_info_data_name(data)) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_allocate(side, &fresh) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct(fresh, INT64_MIN + 3, mc_world_info_data_name(data)) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_info_fields_get(fresh, &fresh_fields) == MC_WORLD_INFO_OK &&
          scalars_equal(&fresh_fields.value, &fresh_expected) &&
          fresh_fields.name == mc_world_info_data_name(data));

    CHECK(mc_world_info_player_copy(&input.field[13], NULL, &player, &error) == MC_WORLD_INFO_OK);
    player_root = mc_world_info_player_root(player);
    CHECK(player_root != NULL && mc_world_info_player_name_set(player, NULL) == MC_WORLD_INFO_OK);
    mc_world_info_name *literal = mc_world_info_player_name(player);
    CHECK(literal != NULL && mc_world_info_name_text(literal).length == 0 &&
          mc_world_info_player_root(player)->name.length == 0);
    CHECK(mc_world_info_name_retain(literal) == MC_WORLD_INFO_OK);
    empty_literal = literal;
    CHECK(mc_world_info_name_copy((mc_nbt_string){NULL, 0}, SIZE_MAX, &empty) == MC_WORLD_INFO_OK);
    CHECK(empty != literal && mc_world_info_player_name_set(player, empty) == MC_WORLD_INFO_OK &&
          mc_world_info_player_name(player) == empty);
    mc_world_info_name_release(empty);
    empty = NULL; /* The Player is sole holder. */
    CHECK(mc_world_info_player_name_set(player, mc_world_info_player_name(player)) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_info_player_name_set(player, NULL) == MC_WORLD_INFO_OK &&
          mc_world_info_player_name(player) == empty_literal);
    CHECK(mc_world_info_name_copy(
              (mc_nbt_string){before_units, sizeof(before_units) / sizeof(before_units[0])},
              SIZE_MAX, &before_name) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_player_name_set(player, before_name) == MC_WORLD_INFO_OK);
    mc_world_info_name_release(before_name);
    before_name = NULL;
    CHECK(mc_world_info_player_name_set(player, mc_world_info_player_name(player)) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_world_info_name_retain(mc_world_info_player_name(player)) == MC_WORLD_INFO_OK);
    held_before = mc_world_info_player_name(player);
    CHECK(
        text_equal(mc_world_info_player_root(player)->name,
                   (mc_nbt_string){before_units, sizeof(before_units) / sizeof(before_units[0])}));
    CHECK(mc_world_info_data_player_set(data, player) == MC_WORLD_INFO_OK &&
          mc_world_info_data_player(data) == player);
    CHECK(mc_world_info_player_root(player) == player_root && named_ascii(player_root, "Player"));
    mc_world_info_name *player_literal = mc_world_info_player_name(player);
    CHECK(
        player_literal != NULL &&
        text_equal(mc_world_info_name_text(held_before),
                   (mc_nbt_string){before_units, sizeof(before_units) / sizeof(before_units[0])}));
    mc_world_info_player_release(player);
    player = NULL; /* Data now owns it alone. */
    CHECK(mc_world_info_data_player_set(data, mc_world_info_data_player(data)) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_player_name(mc_world_info_data_player(data)) == player_literal);
    CHECK(mc_world_info_fields_get(old_info, &current) == MC_WORLD_INFO_OK &&
          same_fields(&old, &current));
    CHECK(mc_world_info_allocate(side, &loaded) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_nbt(loaded, data, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_fields_get(loaded, &current) == MC_WORLD_INFO_OK &&
          scalars_equal(&current.value, &expected) && current.name == fresh_fields.name &&
          current.name != old.name && current.player == mc_world_info_data_player(data) &&
          current.player != old.player);
    CHECK(mc_world_info_source(loaded) == source && source_is(source));
    CHECK(find(source, "LevelName") != NULL && find(source, "LevelName")->type == MC_NBT_STRING &&
          find(source, "Player") != NULL && find(source, "Player")->type == MC_NBT_COMPOUND);

    wrong.field[7].type = MC_NBT_INT;
    memset(&wrong.field[7].value, 0, sizeof(wrong.field[7].value));
    wrong.field[13].type = MC_NBT_INT;
    memset(&wrong.field[13].value, 0, sizeof(wrong.field[13].value));
    CHECK(mc_world_info_data_copy(&wrong.data, NULL, &broken, &error) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_data_name(broken) == NULL && mc_world_info_data_player(broken) == NULL);
    CHECK(mc_world_info_data_name_set(broken, current.name) == MC_WORLD_INFO_OK);
    /* Distinct compounds renamed by Data assignment share the one Player key. */
    CHECK(mc_world_info_data_player_set(broken, old.player) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_player_name(old.player) == player_literal);
    CHECK(mc_world_info_data_player_set(broken, current.player) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_allocate(side, &repaired) == MC_WORLD_INFO_OK);
    CHECK(mc_world_info_construct_nbt(repaired, broken, &error) == MC_WORLD_INFO_OK);
    mc_world_info_fields fixed = {0};
    CHECK(mc_world_info_fields_get(repaired, &fixed) == MC_WORLD_INFO_OK &&
          same_fields(&fixed, &current));
    const mc_nbt_tag *history = mc_world_info_source(repaired);
    CHECK(source_is(history) && find(history, "LevelName") != NULL &&
          find(history, "LevelName")->type == MC_NBT_INT && find(history, "Player") != NULL &&
          find(history, "Player")->type == MC_NBT_INT);
    CHECK(mc_world_info_data_name_set(NULL, current.name) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_data_name_set(data, NULL) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_data_player_set(NULL, current.player) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_data_player_set(data, NULL) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_player_name_set(NULL, current.name) == MC_WORLD_INFO_INVALID_ARGUMENT);
    CHECK(mc_world_info_player_name(NULL) == NULL);
    CHECK(mc_world_info_data_name(data) == current.name &&
          mc_world_info_data_player(data) == current.player &&
          mc_world_info_player_name(current.player) == player_literal);
    CHECK(mc_world_info_fields_get(old_info, &fixed) == MC_WORLD_INFO_OK &&
          same_fields(&fixed, &old));
    CHECK(mc_world_info_fields_get(fresh, &fixed) == MC_WORLD_INFO_OK &&
          same_fields(&fixed, &fresh_fields));
    CHECK(mc_world_info_name_retain(current.name) == MC_WORLD_INFO_OK);
    held_name = current.name;
    CHECK(mc_world_info_player_retain(current.player) == MC_WORLD_INFO_OK);
    held_player = current.player;
    mc_world_info_release(old_info);
    old_info = NULL;
    mc_world_info_release(fresh);
    fresh = NULL;
    mc_world_info_release(loaded);
    loaded = NULL;
    mc_world_info_release(repaired);
    repaired = NULL;
    mc_world_info_data_release(data);
    data = NULL;
    mc_world_info_data_release(broken);
    broken = NULL;
    CHECK(text_equal(mc_world_info_name_text(held_name),
                     (mc_nbt_string){title, sizeof(title) / sizeof(title[0])}));
    CHECK(mc_world_info_player_root(held_player) == player_root &&
          named_ascii(player_root, "Player") &&
          mc_world_info_player_name(held_player) == player_literal &&
          find(player_root, "Dimension") != NULL &&
          find(player_root, "Dimension")->value.int_value == -7);
    CHECK(
        text_equal(mc_world_info_name_text(held_before),
                   (mc_nbt_string){before_units, sizeof(before_units) / sizeof(before_units[0])}) &&
        mc_world_info_name_text(empty_literal).length == 0);
    result = 0;
cleanup:
    mc_world_info_release(old_info);
    mc_world_info_release(fresh);
    mc_world_info_release(loaded);
    mc_world_info_release(repaired);
    mc_world_info_data_release(data);
    mc_world_info_data_release(broken);
    mc_world_info_player_release(player);
    mc_world_info_player_release(held_player);
    mc_world_info_name_release(name);
    mc_world_info_name_release(empty);
    mc_world_info_name_release(before_name);
    mc_world_info_name_release(held_before);
    mc_world_info_name_release(held_name);
    mc_world_info_name_release(empty_literal);
    return result;
}

int main(void)
{
    if (test_native_contracts() != 0)
        return 1;
    for (unsigned int s = 0; s < 2; ++s) {
        const mc_world_info_side side = s == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER;
        if (test_fresh(side) != 0 || test_copy_lifetimes(side) != 0 || test_records(side) != 0 ||
            test_missing_and_phases(side) != 0 || test_data_reference_setters(side) != 0)
            return 1;
        for (size_t bad = 0; bad < 15; ++bad)
            if (test_wrong_type(side, bad) != 0)
                return 1;
    }
    printf("WorldInfo: %u/2 serialization clock brackets ordered\n", ordered_brackets);
    ++checks;
    if (ordered_brackets == 0) {
        fprintf(stderr, "WorldInfo: no ordered serialization clock bracket\n");
        return 1;
    }
    printf("PASS: %u WorldInfo checks\n", checks);
    return 0;
}
