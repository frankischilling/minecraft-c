#include "mc/storage/level/level.h"

#include <stdlib.h>
#include <string.h>

enum { LEVEL_FIELD_COUNT = 14 };

static const char *const level_names[LEVEL_FIELD_COUNT] = {
    "RandomSeed", "SpawnX",  "SpawnY",   "SpawnZ",  "Time",        "LastPlayed", "SizeOnDisk",
    "LevelName",  "version", "rainTime", "raining", "thunderTime", "thundering", "Player"};

static const mc_nbt_type level_types[LEVEL_FIELD_COUNT] = {
    MC_NBT_LONG,   MC_NBT_INT, MC_NBT_INT, MC_NBT_INT,  MC_NBT_LONG, MC_NBT_LONG, MC_NBT_LONG,
    MC_NBT_STRING, MC_NBT_INT, MC_NBT_INT, MC_NBT_BYTE, MC_NBT_INT,  MC_NBT_BYTE, MC_NBT_COMPOUND};

static mc_level_status level_error(mc_level_error *error, mc_level_status status, const char *field,
                                   mc_nbt_error nbt) {
    if (error != NULL) {
        *error = (mc_level_error){status, field, nbt};
    }
    return status;
}

static mc_level_status level_status(mc_level_error *error, mc_level_status status,
                                    const char *field) {
    return level_error(error, status, field, (mc_nbt_error){MC_NBT_OK, 0});
}

const char *mc_level_status_string(mc_level_status status) {
    switch (status) {
        case MC_LEVEL_OK:
            return "ok";
        case MC_LEVEL_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_LEVEL_INVALID_FIELD:
            return "wrong level field type";
        case MC_LEVEL_LIMIT:
            return "resource limit";
        case MC_LEVEL_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_LEVEL_NBT_ERROR:
            return "invalid NBT result";
    }
    return "unknown level status";
}

static mc_nbt_string level_name(const char *ascii, uint16_t units[16]) {
    size_t length = strlen(ascii);
    for (size_t i = 0; i < length; ++i) {
        units[i] = (uint16_t)(unsigned char)ascii[i];
    }
    return (mc_nbt_string){units, length};
}

static const mc_nbt_tag *level_get(const mc_nbt_tag *compound, const char *ascii) {
    uint16_t units[16];
    mc_nbt_string name = level_name(ascii, units);
    return mc_nbt_compound_get(compound, name.units, name.length);
}

static bool level_typed(const mc_nbt_tag *parent, const char *name, mc_nbt_type type,
                        const mc_nbt_tag **out, mc_level_error *error) {
    const mc_nbt_tag *tag = level_get(parent, name);
    if (tag != NULL && tag->type != type) {
        (void)level_status(error, MC_LEVEL_INVALID_FIELD, name);
        return false;
    }
    *out = tag;
    return true;
}

mc_level_status mc_level_decode(const mc_nbt_tag *root, mc_level_info *out, mc_level_error *error) {
    if (root == NULL || root->type != MC_NBT_COMPOUND || out == NULL) {
        return level_status(error, MC_LEVEL_INVALID_ARGUMENT, NULL);
    }
    const mc_nbt_tag *data;
    if (!level_typed(root, "Data", MC_NBT_COMPOUND, &data, error)) {
        return MC_LEVEL_INVALID_FIELD;
    }
    mc_level_info info = {0};
    const mc_nbt_tag *tag;
#define LEVEL_READ(key, type, member, value)                                                       \
    do {                                                                                           \
        if (!level_typed(data, key, type, &tag, error)) {                                          \
            return MC_LEVEL_INVALID_FIELD;                                                         \
        }                                                                                          \
        if (tag != NULL) {                                                                         \
            info.member = tag->value;                                                              \
        }                                                                                          \
    } while (0)
    LEVEL_READ("RandomSeed", MC_NBT_LONG, seed, value.long_value);
    LEVEL_READ("SpawnX", MC_NBT_INT, spawn_x, value.int_value);
    LEVEL_READ("SpawnY", MC_NBT_INT, spawn_y, value.int_value);
    LEVEL_READ("SpawnZ", MC_NBT_INT, spawn_z, value.int_value);
    LEVEL_READ("Time", MC_NBT_LONG, time, value.long_value);
    LEVEL_READ("LastPlayed", MC_NBT_LONG, last_played, value.long_value);
    LEVEL_READ("SizeOnDisk", MC_NBT_LONG, size_on_disk, value.long_value);
    LEVEL_READ("LevelName", MC_NBT_STRING, level_name, value.string);
    LEVEL_READ("version", MC_NBT_INT, version, value.int_value);
    LEVEL_READ("rainTime", MC_NBT_INT, rain_time, value.int_value);
    LEVEL_READ("raining", MC_NBT_BYTE, raining, value.byte_value != 0);
    LEVEL_READ("thunderTime", MC_NBT_INT, thunder_time, value.int_value);
    LEVEL_READ("thundering", MC_NBT_BYTE, thundering, value.byte_value != 0);
#undef LEVEL_READ
    if (!level_typed(data, "Player", MC_NBT_COMPOUND, &info.player, error) ||
        !level_typed(info.player, "Dimension", MC_NBT_INT, &tag, error)) {
        return MC_LEVEL_INVALID_FIELD;
    }
    if (tag != NULL) {
        info.player_dimension = tag->value.int_value;
    }
    *out = info;
    return level_status(error, MC_LEVEL_OK, NULL);
}

static mc_level_status level_nbt_error(mc_level_error *error, mc_nbt_status status,
                                       mc_nbt_error nbt) {
    mc_level_status result = status == MC_NBT_LIMIT               ? MC_LEVEL_LIMIT
                             : status == MC_NBT_ALLOCATION_FAILED ? MC_LEVEL_ALLOCATION_FAILED
                                                                  : MC_LEVEL_NBT_ERROR;
    return level_error(error, result, NULL, nbt);
}

static void level_fields(const mc_level_info *info, int64_t saved_at_ms,
                         mc_nbt_tag fields[LEVEL_FIELD_COUNT],
                         uint16_t names[LEVEL_FIELD_COUNT][16]) {
    for (size_t i = 0; i < LEVEL_FIELD_COUNT; ++i) {
        fields[i].name = level_name(level_names[i], names[i]);
    }
    fields[0].type = MC_NBT_LONG;
    fields[0].value.long_value = info->seed;
    fields[1].type = fields[2].type = fields[3].type = MC_NBT_INT;
    fields[1].value.int_value = info->spawn_x;
    fields[2].value.int_value = info->spawn_y;
    fields[3].value.int_value = info->spawn_z;
    fields[4].type = fields[5].type = fields[6].type = MC_NBT_LONG;
    fields[4].value.long_value = info->time;
    fields[5].value.long_value = saved_at_ms;
    fields[6].value.long_value = info->size_on_disk;
    fields[7].type = MC_NBT_STRING;
    fields[7].value.string = info->level_name;
    fields[8].type = fields[9].type = fields[11].type = MC_NBT_INT;
    fields[8].value.int_value = info->version;
    fields[9].value.int_value = info->rain_time;
    fields[10].type = fields[12].type = MC_NBT_BYTE;
    fields[10].value.byte_value = info->raining ? 1 : 0;
    fields[11].value.int_value = info->thunder_time;
    fields[12].value.byte_value = info->thundering ? 1 : 0;
    if (info->player != NULL) {
        fields[13].type = MC_NBT_COMPOUND;
        fields[13].value.compound = info->player->value.compound;
    }
}

static size_t level_overlay(const mc_nbt_tag *baseline, const mc_nbt_tag *const *fields,
                            size_t count, const mc_nbt_tag **items) {
    bool used[LEVEL_FIELD_COUNT] = {false};
    size_t length = 0;
    size_t old_length = baseline == NULL ? 0 : baseline->value.compound.length;
    for (size_t i = 0; i < old_length; ++i) {
        const mc_nbt_tag *old = baseline->value.compound.items[i];
        size_t replacement = count;
        for (size_t j = 0; j < count; ++j) {
            if (old->name.length == fields[j]->name.length &&
                memcmp(old->name.units, fields[j]->name.units,
                       old->name.length * sizeof(uint16_t)) == 0) {
                replacement = j;
                break;
            }
        }
        if (replacement == count) {
            items[length++] = old;
        } else {
            used[replacement] = true;
            if (fields[replacement]->type != MC_NBT_END) {
                items[length++] = fields[replacement];
            }
        }
    }
    for (size_t i = 0; i < count; ++i) {
        if (!used[i] && fields[i]->type != MC_NBT_END) {
            items[length++] = fields[i];
        }
    }
    return length;
}

static mc_level_status level_build_document(const mc_level_info *info, int64_t saved_at_ms,
                                            const mc_nbt_tag *const *record_fields,
                                            const mc_nbt_tag *baseline, const mc_nbt_tag *data,
                                            const mc_nbt_limits *requested_limits,
                                            mc_nbt_document **out, mc_level_error *error) {
    mc_nbt_limits limits = requested_limits == NULL ? mc_nbt_default_limits() : *requested_limits;
    if (limits.max_depth == 0 || limits.max_depth > 128) {
        return level_status(error, MC_LEVEL_INVALID_ARGUMENT, NULL);
    }
    size_t root_count = baseline == NULL ? 0 : baseline->value.compound.length;
    size_t data_count = data == NULL ? 0 : data->value.compound.length;
    if (root_count > limits.max_elements || data_count > limits.max_elements ||
        root_count > SIZE_MAX - LEVEL_FIELD_COUNT - 1u ||
        data_count > SIZE_MAX - LEVEL_FIELD_COUNT - 1u - root_count) {
        return level_status(error, MC_LEVEL_LIMIT, NULL);
    }
    size_t pointer_count = root_count + data_count + LEVEL_FIELD_COUNT + 1u;
    if (pointer_count > SIZE_MAX / sizeof(const mc_nbt_tag *) ||
        pointer_count * sizeof(const mc_nbt_tag *) > limits.max_allocation_bytes) {
        return level_status(error, MC_LEVEL_LIMIT, NULL);
    }
    size_t scratch_bytes = pointer_count * sizeof(const mc_nbt_tag *);
    const mc_nbt_tag **items = malloc(scratch_bytes);
    if (items == NULL) {
        return level_status(error, MC_LEVEL_ALLOCATION_FAILED, NULL);
    }
    mc_nbt_tag fields[LEVEL_FIELD_COUNT] = {0};
    uint16_t names[LEVEL_FIELD_COUNT][16];
    const mc_nbt_tag *field_pointers[LEVEL_FIELD_COUNT];
    if (info != NULL) {
        level_fields(info, saved_at_ms, fields, names);
        for (size_t i = 0; i < LEVEL_FIELD_COUNT; ++i)
            field_pointers[i] = &fields[i];
    } else {
        for (size_t i = 0; i < LEVEL_FIELD_COUNT; ++i)
            field_pointers[i] = record_fields[i];
    }
    uint16_t data_name[16];
    mc_nbt_tag replacement = {0};
    replacement.type = MC_NBT_COMPOUND;
    replacement.name = level_name("Data", data_name);
    replacement.value.compound.items = items;
    replacement.value.compound.length =
        level_overlay(data, field_pointers, LEVEL_FIELD_COUNT, items);
    mc_nbt_tag root = {0};
    root.type = MC_NBT_COMPOUND;
    if (baseline != NULL) {
        root.name = baseline->name;
    }
    root.value.compound.items = items + data_count + LEVEL_FIELD_COUNT;
    const mc_nbt_tag *root_fields[] = {&replacement};
    root.value.compound.length =
        level_overlay(baseline, root_fields, 1, items + data_count + LEVEL_FIELD_COUNT);
    limits.max_allocation_bytes -= scratch_bytes;
    size_t length = 0;
    mc_nbt_error nbt = {MC_NBT_OK, 0};
    mc_nbt_status status = mc_nbt_measure(&root, &limits, &length, &nbt);
    uint8_t *encoded = NULL;
    mc_nbt_document *document = NULL;
    if (status == MC_NBT_OK) {
        if (length > limits.max_allocation_bytes) {
            status = MC_NBT_LIMIT;
            nbt = (mc_nbt_error){status, 0};
        } else {
            encoded = malloc(length);
            if (encoded == NULL) {
                status = MC_NBT_ALLOCATION_FAILED;
                nbt = (mc_nbt_error){status, 0};
            }
        }
    }
    if (status == MC_NBT_OK) {
        limits.max_allocation_bytes -= length;
        size_t written = 0;
        status = mc_nbt_write(&root, &limits, encoded, length, &written, &nbt);
        if (status == MC_NBT_OK) {
            status = mc_nbt_read_compound(encoded, written, &limits, &document, NULL, &nbt);
        }
    }
    free(encoded);
    free(items);
    if (status != MC_NBT_OK) {
        return level_nbt_error(error, status, nbt);
    }
    *out = document;
    return level_status(error, MC_LEVEL_OK, NULL);
}

mc_level_status mc_level_build(const mc_level_info *info, const mc_nbt_tag *baseline,
                               int64_t saved_at_ms, const mc_nbt_limits *requested_limits,
                               mc_nbt_document **out, mc_level_error *error) {
    if (info == NULL || out == NULL || (baseline != NULL && baseline->type != MC_NBT_COMPOUND)) {
        return level_status(error, MC_LEVEL_INVALID_ARGUMENT, NULL);
    }
    const mc_nbt_tag *data;
    const mc_nbt_tag *dimension;
    if (!level_typed(baseline, "Data", MC_NBT_COMPOUND, &data, error)) {
        return MC_LEVEL_INVALID_FIELD;
    }
    if (info->player != NULL && info->player->type != MC_NBT_COMPOUND) {
        return level_status(error, MC_LEVEL_INVALID_FIELD, "Player");
    }
    if (!level_typed(info->player, "Dimension", MC_NBT_INT, &dimension, error)) {
        return MC_LEVEL_INVALID_FIELD;
    }
    (void)dimension;
    return level_build_document(info, saved_at_ms, NULL, baseline, data, requested_limits, out,
                                error);
}

mc_level_status mc_level_build_record(const mc_nbt_tag *record, const mc_nbt_tag *baseline,
                                      const mc_nbt_limits *requested_limits, mc_nbt_document **out,
                                      mc_level_error *error) {
    if (record == NULL || record->type != MC_NBT_COMPOUND || out == NULL ||
        (baseline != NULL && baseline->type != MC_NBT_COMPOUND)) {
        return level_status(error, MC_LEVEL_INVALID_ARGUMENT, NULL);
    }
    const mc_nbt_tag *data;
    if (!level_typed(baseline, "Data", MC_NBT_COMPOUND, &data, error)) {
        return MC_LEVEL_INVALID_FIELD;
    }
    const size_t count = record->value.compound.length;
    if (count != LEVEL_FIELD_COUNT - 1u && count != LEVEL_FIELD_COUNT) {
        return level_status(error, MC_LEVEL_INVALID_FIELD, "Data");
    }
    const mc_nbt_tag *fields[LEVEL_FIELD_COUNT];
    for (size_t i = 0; i < LEVEL_FIELD_COUNT; ++i) {
        if (!level_typed(record, level_names[i], level_types[i], &fields[i], error)) {
            return MC_LEVEL_INVALID_FIELD;
        }
        if (i != LEVEL_FIELD_COUNT - 1u && fields[i] == NULL) {
            return level_status(error, MC_LEVEL_INVALID_FIELD, level_names[i]);
        }
    }
    if (count != LEVEL_FIELD_COUNT - 1u + (fields[LEVEL_FIELD_COUNT - 1u] != NULL ? 1u : 0u)) {
        return level_status(error, MC_LEVEL_INVALID_FIELD, "Data");
    }
    uint16_t player_name[16];
    mc_nbt_tag absent_player = {0};
    absent_player.name = level_name("Player", player_name);
    if (fields[LEVEL_FIELD_COUNT - 1u] == NULL)
        fields[LEVEL_FIELD_COUNT - 1u] = &absent_player;
    return level_build_document(NULL, 0, fields, baseline, data, requested_limits, out, error);
}
