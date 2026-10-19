#include "mc/world/info/info.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

enum { MAX_HOLDS = 32, MAX_IDENTITIES = 128, OUTPUT_CAPACITY = 131072, DATA_FIELDS = 14 };

typedef struct observation {
    mc_world_info *infos[MAX_HOLDS];
    mc_world_info_name *names[MAX_HOLDS];
    mc_world_info_player *players[MAX_HOLDS];
    mc_world_info_data *data[MAX_HOLDS];
    mc_world_info_record *records[MAX_HOLDS];
    size_t info_count, name_count, player_count, data_count, record_count;
    const void *identities[MAX_IDENTITIES];
    unsigned int identity_count, snapshots, serializations, refusals, rows;
    char output[OUTPUT_CAPACITY];
    size_t length;
    const char *failure;
} observation;

typedef struct prepared_data {
    mc_nbt_tag root, fields[DATA_FIELDS], dimension, keep;
    const mc_nbt_tag *items[DATA_FIELDS], *player_items[2];
    uint16_t key_units[DATA_FIELDS + 2][16];
} prepared_data;

static const char *const keys[DATA_FIELDS] = {
    "RandomSeed", "SpawnX",  "SpawnY",   "SpawnZ",  "Time",        "LastPlayed", "SizeOnDisk",
    "LevelName",  "version", "rainTime", "raining", "thunderTime", "thundering", "Player"};
static const mc_nbt_type types[DATA_FIELDS] = {
    MC_NBT_LONG,   MC_NBT_INT, MC_NBT_INT, MC_NBT_INT,  MC_NBT_LONG, MC_NBT_LONG, MC_NBT_LONG,
    MC_NBT_STRING, MC_NBT_INT, MC_NBT_INT, MC_NBT_BYTE, MC_NBT_INT,  MC_NBT_BYTE, MC_NBT_COMPOUND};
static const char selectors[] = "abcdefghijklmno";
static const char field_types[] = "JIIIJJJCISIZIZI";
static const uint16_t name_units[] = {'A', 0, 0x03a9, 0xd800};

static bool fail(observation *o, const char *message)
{
    if (o->failure == NULL)
        o->failure = message;
    return false;
}

#define REQUIRE(o, condition)                                                                      \
    do {                                                                                           \
        if (!(condition))                                                                          \
            return fail((o), #condition);                                                          \
    } while (0)

static bool emit(observation *o, const char *format, ...)
{
    if (o->failure != NULL)
        return false;
    va_list arguments;
    va_start(arguments, format);
    const int count =
        vsnprintf(o->output + o->length, sizeof(o->output) - o->length, format, arguments);
    va_end(arguments);
    REQUIRE(o, count >= 0 && (size_t)count < sizeof(o->output) - o->length);
    for (size_t index = 0; index < (size_t)count; ++index)
        if (o->output[o->length + index] == '\n')
            ++o->rows;
    o->length += (size_t)count;
    return true;
}

/* IDs are allocated only at observation sites, in Java evaluation order.
 * Actual owners below stay held through END, including obsolete root names. */
static unsigned int identity(observation *o, const void *pointer)
{
    if (pointer == NULL)
        return 0;
    for (unsigned int index = 0; index < o->identity_count; ++index)
        if (o->identities[index] == pointer)
            return index + 1u;
    if (o->identity_count == MAX_IDENTITIES) {
        (void)fail(o, "identity capacity");
        return 0;
    }
    o->identities[o->identity_count++] = pointer;
    return o->identity_count;
}

static bool hold_name(observation *o, mc_world_info_name *name)
{
    if (name == NULL)
        return true;
    for (size_t index = 0; index < o->name_count; ++index)
        if (o->names[index] == name)
            return true;
    REQUIRE(o, o->name_count < MAX_HOLDS);
    REQUIRE(o, mc_world_info_name_retain(name) == MC_WORLD_INFO_OK);
    o->names[o->name_count++] = name;
    return true;
}

static bool new_name(observation *o, mc_nbt_string text, mc_world_info_name **out)
{
    REQUIRE(o, o->name_count < MAX_HOLDS);
    mc_world_info_name *name = NULL;
    REQUIRE(o, mc_world_info_name_copy(text, SIZE_MAX, &name) == MC_WORLD_INFO_OK);
    o->names[o->name_count++] = name;
    *out = name;
    return true;
}

static bool ascii_name(observation *o, const char *text, mc_world_info_name **out)
{
    uint16_t units[64];
    const size_t length = strlen(text);
    REQUIRE(o, length <= sizeof(units) / sizeof(units[0]));
    for (size_t index = 0; index < length; ++index)
        units[index] = (uint16_t)(unsigned char)text[index];
    return new_name(o, (mc_nbt_string){units, length}, out);
}

static bool new_info(observation *o, mc_world_info_side side, mc_world_info **out)
{
    REQUIRE(o, o->info_count < MAX_HOLDS);
    mc_world_info *info = NULL;
    REQUIRE(o, mc_world_info_allocate(side, &info) == MC_WORLD_INFO_OK);
    o->infos[o->info_count++] = info;
    *out = info;
    return true;
}

static bool new_data(observation *o, const mc_nbt_tag *root, mc_world_info_data **out)
{
    REQUIRE(o, o->data_count < MAX_HOLDS);
    mc_world_info_data *data = NULL;
    mc_world_info_error error = {0};
    REQUIRE(o, mc_world_info_data_copy(root, NULL, &data, &error) == MC_WORLD_INFO_OK);
    o->data[o->data_count++] = data;
    *out = data;
    return true;
}

static mc_nbt_string ascii_units(uint16_t *units, const char *text)
{
    const size_t length = strlen(text);
    for (size_t index = 0; index < length; ++index)
        units[index] = (uint16_t)(unsigned char)text[index];
    return (mc_nbt_string){units, length};
}

static void prepare(prepared_data *p)
{
    memset(p, 0, sizeof(*p));
    p->root.type = MC_NBT_COMPOUND;
    p->root.value.compound.items = p->items;
    p->root.value.compound.length = DATA_FIELDS;
    for (size_t index = 0; index < DATA_FIELDS; ++index) {
        p->fields[index].type = types[index];
        p->fields[index].name = ascii_units(p->key_units[index], keys[index]);
        p->items[index] = &p->fields[index];
    }
    /* These are the prepared Java arguments, not captured expected fields. */
    p->fields[0].value.long_value = -INT64_C(81985529216486896); /* fedcba9876543210 */
    p->fields[1].value.int_value = INT32_MIN;
    p->fields[2].value.int_value = 257;
    p->fields[3].value.int_value = INT32_MAX;
    p->fields[4].value.long_value = INT64_C(0x0102030405060708);
    p->fields[5].value.long_value = INT64_C(0x1112131415161718);
    p->fields[6].value.long_value = -17;
    p->fields[7].value.string =
        (mc_nbt_string){name_units, sizeof(name_units) / sizeof(name_units[0])};
    p->fields[8].value.int_value = INT32_C(0x76543210);
    p->fields[9].value.int_value = -7;
    p->fields[10].value.byte_value = INT8_MIN;
    p->fields[11].value.int_value = INT32_MAX;
    p->fields[12].value.byte_value = 7;
    p->dimension.type = MC_NBT_INT;
    p->dimension.name = ascii_units(p->key_units[14], "Dimension");
    p->dimension.value.int_value = -1;
    p->keep.type = MC_NBT_INT;
    p->keep.name = ascii_units(p->key_units[15], "Keep");
    p->keep.value.int_value = INT32_C(0x12345678);
    p->player_items[0] = &p->dimension;
    p->player_items[1] = &p->keep;
    p->fields[13].value.compound.items = p->player_items;
    p->fields[13].value.compound.length = 2;
}

static bool new_player(observation *o, int32_t dimension, int32_t marker, const char *name,
                       mc_world_info_player **out)
{
    REQUIRE(o, o->player_count < MAX_HOLDS);
    prepared_data p;
    prepare(&p);
    uint16_t units[64];
    REQUIRE(o, strlen(name) <= sizeof(units) / sizeof(units[0]));
    p.dimension.value.int_value = dimension;
    p.keep.value.int_value = marker;
    p.fields[13].name = ascii_units(units, name);
    mc_world_info_player *player = NULL;
    mc_world_info_error error = {0};
    REQUIRE(o, mc_world_info_player_copy(&p.fields[13], NULL, &player, &error) == MC_WORLD_INFO_OK);
    o->players[o->player_count++] = player;
    *out = player;
    return true;
}

static const mc_nbt_tag *find(const mc_nbt_tag *root, const char *key)
{
    uint16_t units[32];
    if (strlen(key) > sizeof(units) / sizeof(units[0]))
        return NULL;
    const mc_nbt_string name = ascii_units(units, key);
    return mc_nbt_compound_get(root, name.units, name.length);
}

static bool emit_units(observation *o, mc_nbt_string text)
{
    REQUIRE(o, text.length <= 64 && (text.length == 0 || text.units != NULL));
    if (text.length == 0)
        return emit(o, "-");
    for (size_t index = 0; index < text.length; ++index)
        REQUIRE(o, emit(o, "%04x", (unsigned int)text.units[index]));
    return true;
}

static bool emit_name(observation *o, const char *kind, unsigned int step, mc_world_info_name *name)
{
    REQUIRE(o, hold_name(o, name));
    const unsigned int id = identity(o, name);
    const mc_nbt_string text = mc_world_info_name_text(name);
    REQUIRE(o, emit(o, "%s\t%u\t%u\t", kind, step, id));
    if (name == NULL)
        REQUIRE(o, emit(o, "-1\t"));
    else
        REQUIRE(o, emit(o, "%zu\t", text.length));
    REQUIRE(o, emit_units(o, text));
    return emit(o, "\n");
}

static bool emit_player(observation *o, unsigned int step, mc_world_info_player *player)
{
    if (player == NULL)
        return emit(o, "PLAYER\t%u\t0\t0\t-1\t-\t0\t-\t0\t-\n", step);
    const mc_nbt_tag *root = mc_world_info_player_root(player);
    REQUIRE(o, root != NULL && root->type == MC_NBT_COMPOUND && root->value.compound.length == 2);
    const mc_nbt_tag *dimension = find(root, "Dimension"), *keep = find(root, "Keep");
    REQUIRE(o, dimension != NULL && dimension->type == MC_NBT_INT && keep != NULL &&
                   keep->type == MC_NBT_INT);
    mc_world_info_name *name = mc_world_info_player_name(player);
    REQUIRE(o, name != NULL && hold_name(o, name));
    const mc_nbt_string text = mc_world_info_name_text(name);
    REQUIRE(o, root->name.units == text.units && root->name.length == text.length);
    const unsigned int player_id = identity(o, root);
    const unsigned int name_id = identity(o, name);
    const unsigned int dimension_id = identity(o, dimension);
    const unsigned int keep_id = identity(o, keep);
    REQUIRE(o, emit(o, "PLAYER\t%u\t%u\t%u\t%zu\t", step, player_id, name_id, text.length));
    REQUIRE(o, emit_units(o, text));
    return emit(o, "\t%u\t%08" PRIx32 "\t%u\t%08" PRIx32 "\n", dimension_id,
                (uint32_t)dimension->value.int_value, keep_id, (uint32_t)keep->value.int_value);
}

static bool same_fields(const mc_world_info_fields *a, const mc_world_info_fields *b)
{
    return a->value.seed == b->value.seed && a->value.spawn_x == b->value.spawn_x &&
           a->value.spawn_y == b->value.spawn_y && a->value.spawn_z == b->value.spawn_z &&
           a->value.time == b->value.time && a->value.last_played == b->value.last_played &&
           a->value.size_on_disk == b->value.size_on_disk && a->player == b->player &&
           a->value.player_dimension == b->value.player_dimension && a->name == b->name &&
           a->value.version == b->value.version && a->value.raining == b->value.raining &&
           a->value.rain_time == b->value.rain_time && a->value.thundering == b->value.thundering &&
           a->value.thunder_time == b->value.thunder_time;
}

static bool snapshot(observation *o, unsigned int step, const mc_world_info *info)
{
    mc_world_info_fields f = {0};
    REQUIRE(o, mc_world_info_fields_get(info, &f) == MC_WORLD_INFO_OK);
    const unsigned int info_id = identity(o, info);
    REQUIRE(o, emit(o, "SNAPSHOT\t%u\t%u\n", step, info_id));
    for (size_t index = 0; index < 15; ++index) {
        REQUIRE(o, emit(o, "FIELD\t%u\t%c\t%c\t", step, selectors[index], field_types[index]));
        switch (selectors[index]) {
            case 'a':
                REQUIRE(o, emit(o, "%016" PRIx64, (uint64_t)f.value.seed));
                break;
            case 'b':
                REQUIRE(o, emit(o, "%08" PRIx32, (uint32_t)f.value.spawn_x));
                break;
            case 'c':
                REQUIRE(o, emit(o, "%08" PRIx32, (uint32_t)f.value.spawn_y));
                break;
            case 'd':
                REQUIRE(o, emit(o, "%08" PRIx32, (uint32_t)f.value.spawn_z));
                break;
            case 'e':
                REQUIRE(o, emit(o, "%016" PRIx64, (uint64_t)f.value.time));
                break;
            case 'f':
                REQUIRE(o, emit(o, "%016" PRIx64, (uint64_t)f.value.last_played));
                break;
            case 'g':
                REQUIRE(o, emit(o, "%016" PRIx64, (uint64_t)f.value.size_on_disk));
                break;
            case 'h': {
                const unsigned int id = identity(o, mc_world_info_player_root(f.player));
                REQUIRE(o, emit(o, "%u", id));
                break;
            }
            case 'i':
                REQUIRE(o, emit(o, "%08" PRIx32, (uint32_t)f.value.player_dimension));
                break;
            case 'j': {
                REQUIRE(o, hold_name(o, f.name));
                const unsigned int id = identity(o, f.name);
                REQUIRE(o, emit(o, "%u", id));
                break;
            }
            case 'k':
                REQUIRE(o, emit(o, "%08" PRIx32, (uint32_t)f.value.version));
                break;
            case 'l':
                REQUIRE(o, emit(o, "%u", f.value.raining ? 1u : 0u));
                break;
            case 'm':
                REQUIRE(o, emit(o, "%08" PRIx32, (uint32_t)f.value.rain_time));
                break;
            case 'n':
                REQUIRE(o, emit(o, "%u", f.value.thundering ? 1u : 0u));
                break;
            case 'o':
                REQUIRE(o, emit(o, "%08" PRIx32, (uint32_t)f.value.thunder_time));
                break;
            default:
                return fail(o, "field selector");
        }
        REQUIRE(o, emit(o, "\n"));
    }
    REQUIRE(o, emit_name(o, "NAME", step, f.name) && emit_player(o, step, f.player));
    ++o->snapshots;
    return true;
}

static bool serialize(observation *o, unsigned int serial, const mc_world_info *info)
{
    REQUIRE(o, o->record_count < MAX_HOLDS);
    mc_world_info_fields before = {0}, after = {0};
    mc_world_info_error error = {0};
    REQUIRE(o, mc_world_info_fields_get(info, &before) == MC_WORLD_INFO_OK);
    mc_world_info_record *record = NULL;
    REQUIRE(o, mc_world_info_record_create(info, &record, &error) == MC_WORLD_INFO_OK);
    o->records[o->record_count++] = record;
    const mc_nbt_tag *root = mc_world_info_record_root(record);
    REQUIRE(o, root != NULL && root->type == MC_NBT_COMPOUND);
    for (size_t index = 0; index + 1 < o->record_count; ++index)
        REQUIRE(o, mc_world_info_record_root(o->records[index]) != root);
    const mc_nbt_tag *player = mc_world_info_player_root(before.player);
    REQUIRE(o, root->value.compound.length == (player == NULL ? 13u : 14u) &&
                   find(root, "Player") == player && root != player);
    mc_world_info_name *name = mc_world_info_record_name(record);
    REQUIRE(o, name == before.name && name != NULL && hold_name(o, name));
    const unsigned int info_id = identity(o, info);
    const unsigned int root_id = identity(o, root);
    REQUIRE(o, emit(o, "SERIAL\t%u\t%u\t%u\t%zu\n", serial, info_id, root_id,
                    root->value.compound.length));
    for (size_t index = 0; index < DATA_FIELDS; ++index) {
        const mc_nbt_tag *tag = find(root, keys[index]);
        if (index == 13) {
            const unsigned int player_id = identity(o, tag);
            REQUIRE(o, emit(o, "SERIAL_FIELD\t%u\t%zu\t10\t%u\n", serial, index, player_id));
            if (tag != NULL) {
                const mc_nbt_string text =
                    mc_world_info_name_text(mc_world_info_player_name(before.player));
                static const uint16_t player_key[] = {'P', 'l', 'a', 'y', 'e', 'r'};
                REQUIRE(o, text.length == sizeof(player_key) / sizeof(player_key[0]) &&
                               text.units != NULL &&
                               memcmp(text.units, player_key, sizeof(player_key)) == 0);
            }
            continue;
        }
        REQUIRE(o, tag != NULL && tag->type == types[index]);
        if (index == 5) {
            REQUIRE(o, emit(o, "SERIAL_FIELD\t%u\t%zu\t4\tSAMPLED\n", serial, index));
            REQUIRE(o, emit(o, "CLOCK_OUTPUT\t%u\t%016" PRIx64 "\n", serial,
                            (uint64_t)tag->value.long_value));
        } else if (tag->type == MC_NBT_STRING) {
            const mc_nbt_string text = mc_world_info_name_text(name);
            REQUIRE(o, tag->value.string.units == text.units &&
                           tag->value.string.length == text.length);
            const unsigned int name_id = identity(o, name);
            REQUIRE(o, emit(o, "SERIAL_FIELD\t%u\t%zu\t8\t%u\n", serial, index, name_id));
            REQUIRE(o, emit_name(o, "SERIAL_NAME", serial, name));
        } else if (tag->type == MC_NBT_LONG) {
            REQUIRE(o, emit(o, "SERIAL_FIELD\t%u\t%zu\t4\t%016" PRIx64 "\n", serial, index,
                            (uint64_t)tag->value.long_value));
        } else if (tag->type == MC_NBT_INT) {
            REQUIRE(o, emit(o, "SERIAL_FIELD\t%u\t%zu\t3\t%08" PRIx32 "\n", serial, index,
                            (uint32_t)tag->value.int_value));
        } else {
            REQUIRE(o, tag->type == MC_NBT_BYTE);
            REQUIRE(o, emit(o, "SERIAL_FIELD\t%u\t%zu\t1\t%02x\n", serial, index,
                            (unsigned int)(uint8_t)tag->value.byte_value));
        }
    }
    REQUIRE(o, mc_world_info_fields_get(info, &after) == MC_WORLD_INFO_OK &&
                   same_fields(&before, &after));
    ++o->serializations;
    return true;
}

static bool refuse_null_name(observation *o, unsigned int serial, const mc_world_info *info)
{
    mc_world_info_fields before = {0}, after = {0};
    mc_world_info_error error = {0};
    REQUIRE(o, mc_world_info_fields_get(info, &before) == MC_WORLD_INFO_OK && before.name == NULL &&
                   before.player != NULL);
    mc_world_info_name *player_name = mc_world_info_player_name(before.player);
    REQUIRE(o, hold_name(o, player_name));
    const mc_nbt_tag *player_root = mc_world_info_player_root(before.player);
    mc_world_info_record *record = NULL;
    const mc_world_info_status status = mc_world_info_record_create(info, &record, &error);
    const bool no_data = record == NULL;
    if (record != NULL)
        mc_world_info_record_release(record);
    const bool exact_cause = status == MC_WORLD_INFO_INVALID_FIELD && error.status == status &&
                             error.field != NULL && strcmp(error.field, "LevelName") == 0;
    REQUIRE(o, mc_world_info_fields_get(info, &after) == MC_WORLD_INFO_OK);
    const bool unchanged = same_fields(&before, &after);
    const bool player_unchanged = after.player == before.player &&
                                  mc_world_info_player_root(after.player) == player_root &&
                                  mc_world_info_player_name(after.player) == player_name;
    REQUIRE(o, exact_cause && unchanged && player_unchanged && no_data);
    const unsigned int info_id = identity(o, info);
    /* Explicit semantic status mapping for this reached LevelName failure;
     * native C does not throw a Java exception or expose the failed clock local. */
    REQUIRE(o, emit(o, "EXCEPTION\t%u\t%u\tjava.lang.IllegalArgumentException\t%u\t%u\t%u\t%u\n",
                    serial, info_id, exact_cause ? 1u : 0u, unchanged ? 1u : 0u,
                    player_unchanged ? 1u : 0u, no_data ? 1u : 0u));
    ++o->refusals;
    return true;
}

static bool run(observation *o, mc_world_info_side side)
{
    const bool client = side == MC_WORLD_INFO_CLIENT;
    mc_world_info_name *name_a = NULL, *name_b = NULL, *before_name = NULL, *before_null = NULL;
    mc_world_info *fresh = NULL, *null_name = NULL, *loaded = NULL, *copy = NULL, *empty = NULL,
                  *empty_copy = NULL;
    mc_world_info_player *original_player = NULL, *replacement = NULL;
    mc_world_info_data *data = NULL, *empty_data = NULL;
    mc_world_info_fields f = {0}, source = {0}, empty_fields = {0}, empty_copy_fields = {0};
    mc_world_info_error error = {0};
    prepared_data input;
    prepare(&input);
    REQUIRE(o, emit(o, "WORLD_INFO_OBSERVATION_V1\t%s\n", client ? "client" : "server"));
    REQUIRE(o,
            emit(o, "POLICY\tOBJECT_GRAPH_ONLY\tEXCLUDE_ONLY_CLOCK_OUTPUT_FROM_STABLE_FIXTURE\n"));
    REQUIRE(o, emit(o, "CAPABILITY\tCLIENT_PLAYER_SET\t%u\n", client ? 1u : 0u));
    REQUIRE(o, new_name(o, (mc_nbt_string){name_units, sizeof(name_units) / sizeof(name_units[0])},
                        &name_a));
    REQUIRE(o, new_name(o, (mc_nbt_string){name_units, sizeof(name_units) / sizeof(name_units[0])},
                        &name_b));
    REQUIRE(o, name_a != name_b);
    REQUIRE(o, new_info(o, side, &fresh) &&
                   mc_world_info_construct(fresh, INT64_MIN + 12345, name_a) == MC_WORLD_INFO_OK);
    REQUIRE(o, new_info(o, side, &null_name) &&
                   mc_world_info_construct(null_name, INT64_MAX, NULL) == MC_WORLD_INFO_OK);
    REQUIRE(o, snapshot(o, 0, fresh) && snapshot(o, 1, null_name));
    REQUIRE(o, new_player(o, -1, INT32_C(0x12345678), "Initial", &original_player));
    REQUIRE(o, new_data(o, &input.root, &data));
    REQUIRE(o, mc_world_info_data_name_set(data, name_a) == MC_WORLD_INFO_OK);
    REQUIRE(o, mc_world_info_data_player_set(data, original_player) == MC_WORLD_INFO_OK);
    REQUIRE(o, new_info(o, side, &loaded) &&
                   mc_world_info_construct_nbt(loaded, data, &error) == MC_WORLD_INFO_OK);
    REQUIRE(o, new_info(o, side, &copy) &&
                   mc_world_info_construct_copy(copy, loaded) == MC_WORLD_INFO_OK);
    REQUIRE(o, mc_world_info_fields_get(loaded, &source) == MC_WORLD_INFO_OK &&
                   mc_world_info_fields_get(copy, &f) == MC_WORLD_INFO_OK && loaded != copy &&
                   source.player == original_player && f.player == original_player &&
                   source.name == name_a && f.name == name_a);
    REQUIRE(o, snapshot(o, 2, loaded) && snapshot(o, 3, copy));
    REQUIRE(o, mc_world_info_time_set(copy, INT64_MIN + 1) == MC_WORLD_INFO_OK);
    REQUIRE(o, mc_world_info_size_set(copy, INT64_MAX) == MC_WORLD_INFO_OK);
    REQUIRE(o, mc_world_info_spawn_set(copy, -3, -64, 257) == MC_WORLD_INFO_OK);
    REQUIRE(o, mc_world_info_version_set(copy, 7) == MC_WORLD_INFO_OK);
    REQUIRE(o, mc_world_info_rain_time_set(copy, 17) == MC_WORLD_INFO_OK);
    REQUIRE(o, mc_world_info_raining_set(copy, false) == MC_WORLD_INFO_OK);
    REQUIRE(o, mc_world_info_thunder_time_set(copy, 19) == MC_WORLD_INFO_OK);
    REQUIRE(o, mc_world_info_thundering_set(copy, false) == MC_WORLD_INFO_OK);
    REQUIRE(o,
            mc_world_info_fields_get(loaded, &f) == MC_WORLD_INFO_OK && same_fields(&source, &f));
    REQUIRE(o, snapshot(o, 4, copy) && snapshot(o, 5, loaded));
    REQUIRE(o, mc_world_info_name_set(copy, name_b) == MC_WORLD_INFO_OK);
    REQUIRE(o, snapshot(o, 6, copy));
    REQUIRE(o, mc_world_info_name_set(copy, NULL) == MC_WORLD_INFO_OK && snapshot(o, 7, copy));
    REQUIRE(o, mc_world_info_name_set(copy, name_b) == MC_WORLD_INFO_OK);
    REQUIRE(o, ascii_name(o, "Before", &before_name) &&
                   mc_world_info_player_name_set(original_player, before_name) == MC_WORLD_INFO_OK);
    REQUIRE(o, snapshot(o, 8, copy) && serialize(o, 0, copy) && snapshot(o, 9, copy) &&
                   snapshot(o, 10, loaded));
    if (client) {
        REQUIRE(o, new_player(o, 9, INT32_C(0x6543210), "Replacement", &replacement));
        REQUIRE(o, mc_world_info_player_set(copy, replacement) == MC_WORLD_INFO_OK);
        REQUIRE(o, mc_world_info_fields_get(copy, &f) == MC_WORLD_INFO_OK &&
                       f.player == replacement && f.value.player_dimension == -1);
        REQUIRE(o, mc_world_info_fields_get(loaded, &source) == MC_WORLD_INFO_OK &&
                       source.player == original_player);
        REQUIRE(o, snapshot(o, 11, copy) && serialize(o, 1, copy) && snapshot(o, 12, copy) &&
                       snapshot(o, 13, loaded));
        REQUIRE(o, mc_world_info_player_set(copy, NULL) == MC_WORLD_INFO_OK);
        REQUIRE(o, mc_world_info_fields_get(copy, &f) == MC_WORLD_INFO_OK && f.player == NULL &&
                       f.value.player_dimension == -1);
        REQUIRE(o, snapshot(o, 14, copy) && serialize(o, 2, copy));
    }
    const mc_nbt_tag empty_root = {.type = MC_NBT_COMPOUND};
    REQUIRE(o, new_data(o, &empty_root, &empty_data));
    REQUIRE(o, new_info(o, side, &empty) &&
                   mc_world_info_construct_nbt(empty, empty_data, &error) == MC_WORLD_INFO_OK);
    REQUIRE(o, new_info(o, side, &empty_copy) &&
                   mc_world_info_construct_copy(empty_copy, empty) == MC_WORLD_INFO_OK);
    REQUIRE(o, mc_world_info_fields_get(empty, &empty_fields) == MC_WORLD_INFO_OK &&
                   mc_world_info_fields_get(empty_copy, &empty_copy_fields) == MC_WORLD_INFO_OK &&
                   empty_fields.name != NULL && empty_fields.name == empty_copy_fields.name &&
                   empty_fields.name == mc_world_info_data_name(empty_data) &&
                   mc_world_info_name_text(empty_fields.name).length == 0);
    REQUIRE(o, snapshot(o, 15, empty) && snapshot(o, 16, empty_copy));
    REQUIRE(o, serialize(o, 3, fresh) && serialize(o, 4, empty));
    REQUIRE(o, ascii_name(o, "BeforeNull", &before_null) &&
                   mc_world_info_player_name_set(original_player, before_null) == MC_WORLD_INFO_OK);
    REQUIRE(o, mc_world_info_name_set(loaded, NULL) == MC_WORLD_INFO_OK && snapshot(o, 17, loaded));
    REQUIRE(o, refuse_null_name(o, 5, loaded) && snapshot(o, 18, loaded));
    REQUIRE(o, emit(o, "END\t%u\t%u\t%u\t%u\n", o->snapshots, o->serializations, o->serializations,
                    o->refusals));
    REQUIRE(o, o->snapshots == (client ? 19u : 15u) && o->serializations == (client ? 5u : 3u) &&
                   o->refusals == 1u && o->rows == (client ? 432u : 326u));
    return true;
}

static void cleanup(observation *o)
{
    while (o->record_count != 0)
        mc_world_info_record_release(o->records[--o->record_count]);
    while (o->info_count != 0)
        mc_world_info_release(o->infos[--o->info_count]);
    while (o->data_count != 0)
        mc_world_info_data_release(o->data[--o->data_count]);
    while (o->player_count != 0)
        mc_world_info_player_release(o->players[--o->player_count]);
    while (o->name_count != 0)
        mc_world_info_name_release(o->names[--o->name_count]);
}

int main(int argc, char **argv)
{
    if (argc != 2 || (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0)) {
        fprintf(stderr, "observation_probe: expected client|server\n");
        return 1;
    }
    observation o = {0};
    const bool client = strcmp(argv[1], "client") == 0;
    const bool succeeded = run(&o, client ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER);
    if (!succeeded) {
        fprintf(stderr, "observation_probe: %s\n", o.failure != NULL ? o.failure : "failed");
        cleanup(&o);
        return 1;
    }
#if defined(_WIN32)
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) {
        fprintf(stderr, "observation_probe: binary stdout failed\n");
        cleanup(&o);
        return 1;
    }
#endif
    const bool written = fwrite(o.output, 1, o.length, stdout) == o.length && fflush(stdout) == 0;
    cleanup(&o);
    if (!written) {
        fprintf(stderr, "observation_probe: stdout failed\n");
        return 1;
    }
    return 0;
}
