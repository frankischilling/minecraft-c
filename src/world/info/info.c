#include "mc/world/info/info.h"

#include "mc/storage/nbt/tree.h"
#include "millis.h"

#include <stdlib.h>
#include <string.h>

typedef struct info_backing {
    size_t holds;
    mc_nbt_tree *tree;
} info_backing;

struct mc_world_info_name {
    size_t holds;
    size_t length;
    const uint16_t *units;
};

struct mc_world_info_player {
    size_t holds;
    info_backing *backing;
    mc_nbt_tag root;
    mc_world_info_name *name;
};

struct mc_world_info_data {
    size_t holds;
    info_backing *backing;
    mc_world_info_name *name;
    mc_world_info_player *player;
    bool name_override, player_override;
};

struct mc_world_info {
    size_t holds;
    mc_world_info_state state;
    mc_world_info_fields fields;
    mc_world_info_live live;
    mc_world_info_data *data;
};

enum { INFO_RECORD_FIELDS = 13, INFO_RECORD_CHILDREN = 14, INFO_KEY_UNITS = 16 };

struct mc_world_info_record {
    mc_nbt_tag root;
    mc_nbt_tag fields[INFO_RECORD_FIELDS];
    uint16_t names[INFO_RECORD_FIELDS][INFO_KEY_UNITS];
    const mc_nbt_tag *items[INFO_RECORD_CHILDREN];
    mc_world_info_name *name;
    mc_world_info_player *player;
};

static const uint16_t info_player_key[] = {'P', 'l', 'a', 'y', 'e', 'r'};
static mc_world_info_name info_empty_name = {SIZE_MAX, 0, NULL};
static mc_world_info_name info_player_name = {SIZE_MAX, 6, info_player_key};

static mc_world_info_status info_error(mc_world_info_error *error, mc_world_info_status status,
                                       const char *field, mc_nbt_error nbt) {
    if (error != NULL)
        *error = (mc_world_info_error){status, field, nbt};
    return status;
}

static mc_world_info_status info_status(mc_world_info_error *error, mc_world_info_status status,
                                        const char *field) {
    return info_error(error, status, field, (mc_nbt_error){MC_NBT_OK, 0});
}

static mc_world_info_status info_nbt_error(mc_world_info_error *error, mc_nbt_status status,
                                           mc_nbt_error nbt) {
    const mc_world_info_status result =
        status == MC_NBT_INVALID_ARGUMENT    ? MC_WORLD_INFO_INVALID_ARGUMENT
        : status == MC_NBT_LIMIT             ? MC_WORLD_INFO_LIMIT
        : status == MC_NBT_ALLOCATION_FAILED ? MC_WORLD_INFO_ALLOCATION_FAILED
                                             : MC_WORLD_INFO_NBT_ERROR;
    return info_error(error, result, NULL, nbt);
}

const char *mc_world_info_status_string(mc_world_info_status status) {
    switch (status) {
        case MC_WORLD_INFO_OK:
            return "ok";
        case MC_WORLD_INFO_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_WORLD_INFO_INVALID_PHASE:
            return "invalid construction phase";
        case MC_WORLD_INFO_INVALID_FIELD:
            return "wrong world info field type";
        case MC_WORLD_INFO_UNSUPPORTED_OPERATION:
            return "operation absent on this side";
        case MC_WORLD_INFO_LIMIT:
            return "resource limit";
        case MC_WORLD_INFO_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_WORLD_INFO_NBT_ERROR:
            return "invalid NBT source";
        case MC_WORLD_INFO_CLOCK_FAILURE:
            return "wall clock failure";
    }
    return "unknown world info status";
}

static bool info_add_size(size_t *total, size_t added) {
    if (added > SIZE_MAX - *total)
        return false;
    *total += added;
    return true;
}

static const mc_nbt_tag *info_get(const mc_nbt_tag *compound, const char *key) {
    uint16_t units[INFO_KEY_UNITS];
    const size_t length = strlen(key);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)key[i];
    return mc_nbt_compound_get(compound, units, length);
}

mc_world_info_status mc_world_info_name_copy(mc_nbt_string text, size_t max_bytes,
                                             mc_world_info_name **out) {
    if (out == NULL || (text.length != 0 && text.units == NULL))
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    if (text.length > (SIZE_MAX - sizeof(mc_world_info_name)) / sizeof(uint16_t))
        return MC_WORLD_INFO_LIMIT;
    const size_t bytes = sizeof(mc_world_info_name) + text.length * sizeof(uint16_t);
    if (bytes > max_bytes)
        return MC_WORLD_INFO_LIMIT;
    mc_world_info_name *name = malloc(bytes);
    if (name == NULL)
        return MC_WORLD_INFO_ALLOCATION_FAILED;
    name->holds = 1;
    name->length = text.length;
    uint16_t *units = (uint16_t *)(name + 1);
    name->units = units;
    if (text.length != 0)
        memcpy(units, text.units, text.length * sizeof(uint16_t));
    *out = name;
    return MC_WORLD_INFO_OK;
}

mc_nbt_string mc_world_info_name_text(const mc_world_info_name *name) {
    return name == NULL || name->length == 0 ? (mc_nbt_string){NULL, 0}
                                             : (mc_nbt_string){name->units, name->length};
}

mc_world_info_status mc_world_info_name_retain(mc_world_info_name *name) {
    if (name == NULL)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    if (name == &info_empty_name || name == &info_player_name)
        return MC_WORLD_INFO_OK;
    if (name->holds == SIZE_MAX)
        return MC_WORLD_INFO_LIMIT;
    ++name->holds;
    return MC_WORLD_INFO_OK;
}

void mc_world_info_name_release(mc_world_info_name *name) {
    if (name != NULL && name != &info_empty_name && name != &info_player_name && --name->holds == 0)
        free(name);
}

static mc_world_info_status info_backing_retain(info_backing *backing) {
    if (backing->holds == SIZE_MAX)
        return MC_WORLD_INFO_LIMIT;
    ++backing->holds;
    return MC_WORLD_INFO_OK;
}

static void info_backing_release(info_backing *backing) {
    if (backing != NULL && --backing->holds == 0) {
        mc_nbt_tree_free(backing->tree);
        free(backing);
    }
}

static info_backing *info_backing_create(mc_nbt_tree *tree) {
    info_backing *backing = malloc(sizeof(*backing));
    if (backing != NULL) {
        backing->holds = 1;
        backing->tree = tree;
    }
    return backing;
}

static mc_world_info_status info_player_wrap(info_backing *backing, const mc_nbt_tag *compound,
                                             mc_world_info_player **out) {
    mc_world_info_player *player = malloc(sizeof(*player));
    if (player == NULL)
        return MC_WORLD_INFO_ALLOCATION_FAILED;
    const mc_world_info_status status = info_backing_retain(backing);
    if (status != MC_WORLD_INFO_OK) {
        free(player);
        return status;
    }
    player->holds = 1;
    player->backing = backing;
    player->root = *compound;
    player->name = &info_empty_name;
    if (compound->name.length != 0) {
        const mc_world_info_status named =
            mc_world_info_name_copy(compound->name, SIZE_MAX, &player->name);
        if (named != MC_WORLD_INFO_OK) {
            info_backing_release(backing);
            free(player);
            return named;
        }
    }
    player->root.name = mc_world_info_name_text(player->name);
    *out = player;
    return MC_WORLD_INFO_OK;
}

mc_world_info_status mc_world_info_player_copy(const mc_nbt_tag *compound,
                                               const mc_nbt_limits *requested_limits,
                                               mc_world_info_player **out,
                                               mc_world_info_error *error) {
    if (out == NULL || compound == NULL || compound->type != MC_NBT_COMPOUND)
        return info_status(error, MC_WORLD_INFO_INVALID_ARGUMENT, NULL);
    const mc_nbt_limits limits =
        requested_limits == NULL ? mc_nbt_default_limits() : *requested_limits;
    mc_nbt_tree *tree = NULL;
    mc_nbt_error nbt;
    const mc_nbt_status copied = mc_nbt_tree_copy_compound(compound, &limits, &tree, &nbt);
    if (copied != MC_NBT_OK)
        return info_nbt_error(error, copied, nbt);
    size_t bytes = mc_nbt_tree_allocation_bytes(tree);
    if (!info_add_size(&bytes, sizeof(info_backing)) ||
        !info_add_size(&bytes, sizeof(mc_world_info_player)) ||
        bytes > limits.max_allocation_bytes) {
        mc_nbt_tree_free(tree);
        return info_status(error, MC_WORLD_INFO_LIMIT, NULL);
    }
    const mc_nbt_tag *root = mc_nbt_tree_root(tree);
    if (root->name.length != 0 && (!info_add_size(&bytes, sizeof(mc_world_info_name)) ||
                                   root->name.length > SIZE_MAX / sizeof(uint16_t) ||
                                   !info_add_size(&bytes, root->name.length * sizeof(uint16_t)) ||
                                   bytes > limits.max_allocation_bytes)) {
        mc_nbt_tree_free(tree);
        return info_status(error, MC_WORLD_INFO_LIMIT, NULL);
    }
    info_backing *backing = info_backing_create(tree);
    if (backing == NULL) {
        mc_nbt_tree_free(tree);
        return info_status(error, MC_WORLD_INFO_ALLOCATION_FAILED, NULL);
    }
    mc_world_info_player *player = NULL;
    const mc_world_info_status status = info_player_wrap(backing, mc_nbt_tree_root(tree), &player);
    info_backing_release(backing);
    if (status != MC_WORLD_INFO_OK)
        return info_status(error, status, NULL);
    *out = player;
    return info_status(error, MC_WORLD_INFO_OK, NULL);
}

const mc_nbt_tag *mc_world_info_player_root(const mc_world_info_player *player) {
    return player == NULL ? NULL : &player->root;
}

mc_world_info_status mc_world_info_player_name_set(mc_world_info_player *player,
                                                   mc_world_info_name *name) {
    if (player == NULL)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    if (name != NULL) {
        const mc_world_info_status status = mc_world_info_name_retain(name);
        if (status != MC_WORLD_INFO_OK)
            return status;
    }
    mc_world_info_name *old = player->name;
    player->name = name;
    player->root.name = mc_world_info_name_text(name);
    mc_world_info_name_release(old);
    return MC_WORLD_INFO_OK;
}

mc_world_info_name *mc_world_info_player_name(const mc_world_info_player *player) {
    return player == NULL ? NULL : player->name == NULL ? &info_empty_name : player->name;
}

mc_world_info_status mc_world_info_player_retain(mc_world_info_player *player) {
    if (player == NULL)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    if (player->holds == SIZE_MAX)
        return MC_WORLD_INFO_LIMIT;
    ++player->holds;
    return MC_WORLD_INFO_OK;
}

void mc_world_info_player_release(mc_world_info_player *player) {
    if (player != NULL && --player->holds == 0) {
        mc_world_info_name_release(player->name);
        info_backing_release(player->backing);
        free(player);
    }
}

mc_world_info_status mc_world_info_data_copy(const mc_nbt_tag *data,
                                             const mc_nbt_limits *requested_limits,
                                             mc_world_info_data **out, mc_world_info_error *error) {
    if (out == NULL || data == NULL || data->type != MC_NBT_COMPOUND)
        return info_status(error, MC_WORLD_INFO_INVALID_ARGUMENT, NULL);
    const mc_nbt_limits limits =
        requested_limits == NULL ? mc_nbt_default_limits() : *requested_limits;
    mc_nbt_tree *tree = NULL;
    mc_nbt_error nbt;
    const mc_nbt_status copied = mc_nbt_tree_copy_compound(data, &limits, &tree, &nbt);
    if (copied != MC_NBT_OK)
        return info_nbt_error(error, copied, nbt);
    const mc_nbt_tag *root = mc_nbt_tree_root(tree);
    const mc_nbt_tag *name_tag = info_get(root, "LevelName");
    const mc_nbt_tag *player_tag = info_get(root, "Player");
    size_t bytes = mc_nbt_tree_allocation_bytes(tree);
    bool fits = info_add_size(&bytes, sizeof(info_backing)) &&
                info_add_size(&bytes, sizeof(mc_world_info_data));
    if (name_tag != NULL && name_tag->type == MC_NBT_STRING) {
        fits = fits && info_add_size(&bytes, sizeof(mc_world_info_name));
        if (name_tag->value.string.length > SIZE_MAX / sizeof(uint16_t))
            fits = false;
        else
            fits = fits && info_add_size(&bytes, name_tag->value.string.length * sizeof(uint16_t));
    }
    if (player_tag != NULL && player_tag->type == MC_NBT_COMPOUND) {
        fits = fits && info_add_size(&bytes, sizeof(mc_world_info_player));
        if (player_tag->name.length != 0) {
            fits = fits && info_add_size(&bytes, sizeof(mc_world_info_name));
            if (player_tag->name.length > SIZE_MAX / sizeof(uint16_t))
                fits = false;
            else
                fits = fits && info_add_size(&bytes, player_tag->name.length * sizeof(uint16_t));
        }
    }
    if (!fits || bytes > limits.max_allocation_bytes) {
        mc_nbt_tree_free(tree);
        return info_status(error, MC_WORLD_INFO_LIMIT, NULL);
    }
    info_backing *backing = info_backing_create(tree);
    if (backing == NULL) {
        mc_nbt_tree_free(tree);
        return info_status(error, MC_WORLD_INFO_ALLOCATION_FAILED, NULL);
    }
    mc_world_info_data *owned = calloc(1, sizeof(*owned));
    if (owned == NULL) {
        info_backing_release(backing);
        return info_status(error, MC_WORLD_INFO_ALLOCATION_FAILED, NULL);
    }
    owned->holds = 1;
    owned->backing = backing;
    mc_world_info_status status = MC_WORLD_INFO_OK;
    if (name_tag == NULL)
        owned->name = &info_empty_name;
    else if (name_tag->type == MC_NBT_STRING)
        status = mc_world_info_name_copy(name_tag->value.string, limits.max_allocation_bytes,
                                         &owned->name);
    if (status == MC_WORLD_INFO_OK && player_tag != NULL && player_tag->type == MC_NBT_COMPOUND)
        status = info_player_wrap(backing, player_tag, &owned->player);
    if (status != MC_WORLD_INFO_OK) {
        mc_world_info_data_release(owned);
        return info_status(error, status, NULL);
    }
    *out = owned;
    return info_status(error, MC_WORLD_INFO_OK, NULL);
}

mc_world_info_status mc_world_info_data_retain(mc_world_info_data *data) {
    if (data == NULL)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    if (data->holds == SIZE_MAX)
        return MC_WORLD_INFO_LIMIT;
    ++data->holds;
    return MC_WORLD_INFO_OK;
}

void mc_world_info_data_release(mc_world_info_data *data) {
    if (data != NULL && --data->holds == 0) {
        mc_world_info_name_release(data->name);
        mc_world_info_player_release(data->player);
        info_backing_release(data->backing);
        free(data);
    }
}

mc_world_info_name *mc_world_info_data_name(const mc_world_info_data *data) {
    return data == NULL ? NULL : data->name;
}

mc_world_info_player *mc_world_info_data_player(const mc_world_info_data *data) {
    return data == NULL ? NULL : data->player;
}

mc_world_info_status mc_world_info_data_name_set(mc_world_info_data *data,
                                                 mc_world_info_name *name) {
    if (data == NULL || name == NULL)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    const mc_world_info_status status = mc_world_info_name_retain(name);
    if (status != MC_WORLD_INFO_OK)
        return status;
    mc_world_info_name *old = data->name;
    data->name = name;
    data->name_override = true;
    mc_world_info_name_release(old);
    return MC_WORLD_INFO_OK;
}

mc_world_info_status mc_world_info_data_player_set(mc_world_info_data *data,
                                                   mc_world_info_player *player) {
    if (data == NULL || player == NULL)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    mc_world_info_status status = mc_world_info_player_retain(player);
    if (status != MC_WORLD_INFO_OK)
        return status;
    status = mc_world_info_player_name_set(player, &info_player_name);
    if (status != MC_WORLD_INFO_OK) {
        mc_world_info_player_release(player);
        return status;
    }
    mc_world_info_player *old = data->player;
    data->player = player;
    data->player_override = true;
    mc_world_info_player_release(old);
    return MC_WORLD_INFO_OK;
}

static mc_world_info_status info_ready(const mc_world_info *info) {
    if (info == NULL)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    return info->state.phase == MC_WORLD_INFO_READY ? MC_WORLD_INFO_OK
                                                    : MC_WORLD_INFO_INVALID_PHASE;
}

static mc_world_info_status info_begin(mc_world_info *raw) {
    if (raw == NULL)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    if (raw->state.phase != MC_WORLD_INFO_RAW)
        return MC_WORLD_INFO_INVALID_PHASE;
    raw->state.phase = MC_WORLD_INFO_FAILED_CONSTRUCTION;
    return MC_WORLD_INFO_OK;
}

mc_world_info_status mc_world_info_allocate(mc_world_info_side side, mc_world_info **out) {
    if (out == NULL || (side != MC_WORLD_INFO_CLIENT && side != MC_WORLD_INFO_SERVER))
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    mc_world_info *info = calloc(1, sizeof(*info));
    if (info == NULL)
        return MC_WORLD_INFO_ALLOCATION_FAILED;
    info->holds = 1;
    info->state.side = side;
    info->state.phase = MC_WORLD_INFO_RAW;
    info->live = (mc_world_info_live){.time = &info->fields.value.time,
                                      .spawn_x = &info->fields.value.spawn_x,
                                      .spawn_y = &info->fields.value.spawn_y,
                                      .spawn_z = &info->fields.value.spawn_z};
    *out = info;
    return MC_WORLD_INFO_OK;
}

mc_world_info_status mc_world_info_construct(mc_world_info *raw, int64_t seed,
                                             mc_world_info_name *name) {
    mc_world_info_status status = info_begin(raw);
    if (status != MC_WORLD_INFO_OK)
        return status;
    raw->fields.value.seed = seed;
    raw->state.last_store_pc = 6;
    if (name != NULL) {
        status = mc_world_info_name_retain(name);
        if (status != MC_WORLD_INFO_OK)
            return status;
    }
    raw->fields.name = name;
    raw->state.last_store_pc = 11;
    raw->state.phase = MC_WORLD_INFO_READY;
    return MC_WORLD_INFO_OK;
}

mc_world_info_status mc_world_info_construct_nbt(mc_world_info *raw, mc_world_info_data *data,
                                                 mc_world_info_error *error) {
    mc_world_info_status status = info_begin(raw);
    if (status != MC_WORLD_INFO_OK)
        return info_status(error, status, NULL);
    if (data == NULL)
        return info_status(error, MC_WORLD_INFO_INVALID_ARGUMENT, "RandomSeed");
    status = mc_world_info_data_retain(data);
    if (status != MC_WORLD_INFO_OK)
        return info_status(error, status, NULL);
    raw->data = data;
    const mc_nbt_tag *root = mc_nbt_tree_root(data->backing->tree);
    const mc_nbt_tag *tag;
#define INFO_READ(key, expected_type, member, expression, pc)                                      \
    do {                                                                                           \
        tag = info_get(root, key);                                                                 \
        if (tag != NULL && tag->type != (expected_type))                                           \
            return info_status(error, MC_WORLD_INFO_INVALID_FIELD, key);                           \
        raw->fields.value.member = tag == NULL ? 0 : (expression);                                 \
        raw->state.last_store_pc = pc;                                                             \
    } while (0)
    INFO_READ("RandomSeed", MC_NBT_LONG, seed, tag->value.long_value, 11);
    INFO_READ("SpawnX", MC_NBT_INT, spawn_x, tag->value.int_value, 21);
    INFO_READ("SpawnY", MC_NBT_INT, spawn_y, tag->value.int_value, 31);
    INFO_READ("SpawnZ", MC_NBT_INT, spawn_z, tag->value.int_value, 41);
    INFO_READ("Time", MC_NBT_LONG, time, tag->value.long_value, 51);
    INFO_READ("LastPlayed", MC_NBT_LONG, last_played, tag->value.long_value, 61);
    INFO_READ("SizeOnDisk", MC_NBT_LONG, size_on_disk, tag->value.long_value, 71);
    tag = info_get(root, "LevelName");
    if (!data->name_override && tag != NULL && tag->type != MC_NBT_STRING)
        return info_status(error, MC_WORLD_INFO_INVALID_FIELD, "LevelName");
    status = mc_world_info_name_retain(data->name);
    if (status != MC_WORLD_INFO_OK)
        return info_status(error, status, "LevelName");
    raw->fields.name = data->name;
    raw->state.last_store_pc = 81;
    INFO_READ("version", MC_NBT_INT, version, tag->value.int_value, 91);
    INFO_READ("rainTime", MC_NBT_INT, rain_time, tag->value.int_value, 101);
    INFO_READ("raining", MC_NBT_BYTE, raining, tag->value.byte_value != 0, 111);
    INFO_READ("thunderTime", MC_NBT_INT, thunder_time, tag->value.int_value, 121);
    INFO_READ("thundering", MC_NBT_BYTE, thundering, tag->value.byte_value != 0, 131);
#undef INFO_READ
    tag = info_get(root, "Player");
    if (data->player_override || tag != NULL) {
        if (!data->player_override && tag->type != MC_NBT_COMPOUND)
            return info_status(error, MC_WORLD_INFO_INVALID_FIELD, "Player");
        status = mc_world_info_player_retain(data->player);
        if (status != MC_WORLD_INFO_OK)
            return info_status(error, status, "Player");
        raw->fields.player = data->player;
        raw->state.last_store_pc = 150;
        tag = info_get(mc_world_info_player_root(data->player), "Dimension");
        if (tag != NULL && tag->type != MC_NBT_INT)
            return info_status(error, MC_WORLD_INFO_INVALID_FIELD, "Dimension");
        raw->fields.value.player_dimension = tag == NULL ? 0 : tag->value.int_value;
        raw->state.last_store_pc = 163;
    }
    raw->state.phase = MC_WORLD_INFO_READY;
    return info_status(error, MC_WORLD_INFO_OK, NULL);
}

mc_world_info_status mc_world_info_construct_copy(mc_world_info *raw, const mc_world_info *source) {
    mc_world_info_status status = info_begin(raw);
    if (status != MC_WORLD_INFO_OK)
        return status;
    if (source == NULL || source->state.side != raw->state.side)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    status = info_ready(source);
    if (status != MC_WORLD_INFO_OK)
        return status;
    if (source->data != NULL) {
        status = mc_world_info_data_retain(source->data);
        if (status != MC_WORLD_INFO_OK)
            return status;
        raw->data = source->data;
    }
#define INFO_COPY(member, pc)                                                                      \
    do {                                                                                           \
        raw->fields.value.member = source->fields.value.member;                                    \
        raw->state.last_store_pc = pc;                                                             \
    } while (0)
    INFO_COPY(seed, 9);
    INFO_COPY(spawn_x, 17);
    INFO_COPY(spawn_y, 25);
    INFO_COPY(spawn_z, 33);
    INFO_COPY(time, 41);
    INFO_COPY(last_played, 49);
    INFO_COPY(size_on_disk, 57);
    if (source->fields.player != NULL) {
        status = mc_world_info_player_retain(source->fields.player);
        if (status != MC_WORLD_INFO_OK)
            return status;
    }
    raw->fields.player = source->fields.player;
    raw->state.last_store_pc = 65;
    INFO_COPY(player_dimension, 73);
    if (source->fields.name != NULL) {
        status = mc_world_info_name_retain(source->fields.name);
        if (status != MC_WORLD_INFO_OK)
            return status;
    }
    raw->fields.name = source->fields.name;
    raw->state.last_store_pc = 81;
    INFO_COPY(version, 89);
    INFO_COPY(rain_time, 97);
    INFO_COPY(raining, 105);
    INFO_COPY(thunder_time, 113);
    INFO_COPY(thundering, 121);
#undef INFO_COPY
    raw->state.phase = MC_WORLD_INFO_READY;
    return MC_WORLD_INFO_OK;
}

mc_world_info_status mc_world_info_retain(mc_world_info *info) {
    if (info == NULL)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    if (info->holds == SIZE_MAX)
        return MC_WORLD_INFO_LIMIT;
    ++info->holds;
    return MC_WORLD_INFO_OK;
}

void mc_world_info_release(mc_world_info *info) {
    if (info != NULL && --info->holds == 0) {
        mc_world_info_name_release(info->fields.name);
        mc_world_info_player_release(info->fields.player);
        mc_world_info_data_release(info->data);
        free(info);
    }
}

mc_world_info_status mc_world_info_fields_get(const mc_world_info *info,
                                              mc_world_info_fields *out) {
    if (info == NULL || out == NULL)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    *out = info->fields;
    return MC_WORLD_INFO_OK;
}

const mc_world_info_live *mc_world_info_live_get(mc_world_info *info) {
    return info_ready(info) == MC_WORLD_INFO_OK ? &info->live : NULL;
}

mc_world_info_status mc_world_info_time_get(const mc_world_info *info, int64_t *out) {
    if (info == NULL || out == NULL)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    *out = info->fields.value.time;
    return MC_WORLD_INFO_OK;
}

mc_world_info_status mc_world_info_raining_get(const mc_world_info *info, bool *out) {
    if (out == NULL)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    mc_world_info_status status = info_ready(info);
    if (status != MC_WORLD_INFO_OK)
        return status;
    *out = info->fields.value.raining;
    return MC_WORLD_INFO_OK;
}

mc_world_info_status mc_world_info_thundering_get(const mc_world_info *info, bool *out) {
    if (out == NULL)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    mc_world_info_status status = info_ready(info);
    if (status != MC_WORLD_INFO_OK)
        return status;
    *out = info->fields.value.thundering;
    return MC_WORLD_INFO_OK;
}

mc_world_info_status mc_world_info_state_get(const mc_world_info *info, mc_world_info_state *out) {
    if (info == NULL || out == NULL)
        return MC_WORLD_INFO_INVALID_ARGUMENT;
    *out = info->state;
    return MC_WORLD_INFO_OK;
}

const mc_nbt_tag *mc_world_info_source(const mc_world_info *info) {
    return info == NULL || info->data == NULL ? NULL : mc_nbt_tree_root(info->data->backing->tree);
}

mc_world_info_status mc_world_info_name_set(mc_world_info *info, mc_world_info_name *name) {
    mc_world_info_status status = info_ready(info);
    if (status != MC_WORLD_INFO_OK)
        return status;
    if (name != NULL) {
        status = mc_world_info_name_retain(name);
        if (status != MC_WORLD_INFO_OK)
            return status;
    }
    mc_world_info_name *old = info->fields.name;
    info->fields.name = name;
    mc_world_info_name_release(old);
    return MC_WORLD_INFO_OK;
}

mc_world_info_status mc_world_info_player_set(mc_world_info *info, mc_world_info_player *player) {
    mc_world_info_status status = info_ready(info);
    if (status != MC_WORLD_INFO_OK)
        return status;
    if (info->state.side != MC_WORLD_INFO_CLIENT)
        return MC_WORLD_INFO_UNSUPPORTED_OPERATION;
    if (player != NULL) {
        status = mc_world_info_player_retain(player);
        if (status != MC_WORLD_INFO_OK)
            return status;
    }
    mc_world_info_player *old = info->fields.player;
    info->fields.player = player;
    mc_world_info_player_release(old);
    return MC_WORLD_INFO_OK;
}

#define INFO_SETTER(function, member, type)                                                        \
    mc_world_info_status function(mc_world_info *info, type value) {                               \
        const mc_world_info_status status = info_ready(info);                                      \
        if (status != MC_WORLD_INFO_OK)                                                            \
            return status;                                                                         \
        info->fields.value.member = value;                                                         \
        return MC_WORLD_INFO_OK;                                                                   \
    }
INFO_SETTER(mc_world_info_time_set, time, int64_t)
INFO_SETTER(mc_world_info_size_set, size_on_disk, int64_t)
INFO_SETTER(mc_world_info_version_set, version, int32_t)
INFO_SETTER(mc_world_info_rain_time_set, rain_time, int32_t)
INFO_SETTER(mc_world_info_thunder_time_set, thunder_time, int32_t)
INFO_SETTER(mc_world_info_raining_set, raining, bool)
INFO_SETTER(mc_world_info_thundering_set, thundering, bool)
#undef INFO_SETTER

mc_world_info_status mc_world_info_spawn_set(mc_world_info *info, int32_t x, int32_t y, int32_t z) {
    const mc_world_info_status status = info_ready(info);
    if (status != MC_WORLD_INFO_OK)
        return status;
    info->fields.value.spawn_x = x;
    info->fields.value.spawn_y = y;
    info->fields.value.spawn_z = z;
    return MC_WORLD_INFO_OK;
}

static mc_nbt_tag *info_record_add(mc_world_info_record *record, const char *key,
                                   mc_nbt_type type) {
    const size_t index = record->root.value.compound.length;
    mc_nbt_tag *tag = &record->fields[index];
    const size_t length = strlen(key);
    for (size_t i = 0; i < length; ++i)
        record->names[index][i] = (uint16_t)(unsigned char)key[i];
    tag->name = (mc_nbt_string){record->names[index], length};
    tag->type = type;
    record->items[index] = tag;
    ++record->root.value.compound.length;
    return tag;
}

static mc_world_info_status info_record_create(const mc_world_info *info,
                                               mc_world_info_player *player, bool stored_player,
                                               mc_world_info_record **out,
                                               mc_world_info_error *error) {
    if (out == NULL)
        return info_status(error, MC_WORLD_INFO_INVALID_ARGUMENT, NULL);
    mc_world_info_status status = info_ready(info);
    if (status != MC_WORLD_INFO_OK)
        return info_status(error, status, NULL);
    mc_world_info_record *record = calloc(1, sizeof(*record));
    if (record == NULL)
        return info_status(error, MC_WORLD_INFO_ALLOCATION_FAILED, NULL);
    record->root.type = MC_NBT_COMPOUND;
    record->root.value.compound.items = record->items;
    if (stored_player) {
        /* Capture h before entering the original private writer. */
        player = info->fields.player;
    }
    if (player != NULL) {
        status = mc_world_info_player_retain(player);
        if (status != MC_WORLD_INFO_OK) {
            mc_world_info_record_release(record);
            return info_status(error, status, "Player");
        }
        record->player = player;
    }
    info_record_add(record, "RandomSeed", MC_NBT_LONG)->value.long_value = info->fields.value.seed;
    info_record_add(record, "SpawnX", MC_NBT_INT)->value.int_value = info->fields.value.spawn_x;
    info_record_add(record, "SpawnY", MC_NBT_INT)->value.int_value = info->fields.value.spawn_y;
    info_record_add(record, "SpawnZ", MC_NBT_INT)->value.int_value = info->fields.value.spawn_z;
    info_record_add(record, "Time", MC_NBT_LONG)->value.long_value = info->fields.value.time;
    info_record_add(record, "SizeOnDisk", MC_NBT_LONG)->value.long_value =
        info->fields.value.size_on_disk;
    int64_t timestamp = 0;
    if (mc_platform_current_time_millis(&timestamp) != MC_PLATFORM_MILLIS_OK) {
        mc_world_info_record_release(record);
        return info_status(error, MC_WORLD_INFO_CLOCK_FAILURE, "LastPlayed");
    }
    info_record_add(record, "LastPlayed", MC_NBT_LONG)->value.long_value = timestamp;
    if (info->fields.name == NULL) {
        mc_world_info_record_release(record);
        return info_status(error, MC_WORLD_INFO_INVALID_FIELD, "LevelName");
    }
    status = mc_world_info_name_retain(info->fields.name);
    if (status != MC_WORLD_INFO_OK) {
        mc_world_info_record_release(record);
        return info_status(error, status, "LevelName");
    }
    record->name = info->fields.name;
    info_record_add(record, "LevelName", MC_NBT_STRING)->value.string =
        mc_world_info_name_text(record->name);
    info_record_add(record, "version", MC_NBT_INT)->value.int_value = info->fields.value.version;
    info_record_add(record, "rainTime", MC_NBT_INT)->value.int_value = info->fields.value.rain_time;
    info_record_add(record, "raining", MC_NBT_BYTE)->value.byte_value =
        info->fields.value.raining ? 1 : 0;
    info_record_add(record, "thunderTime", MC_NBT_INT)->value.int_value =
        info->fields.value.thunder_time;
    info_record_add(record, "thundering", MC_NBT_BYTE)->value.byte_value =
        info->fields.value.thundering ? 1 : 0;
    if (record->player != NULL) {
        /* The literal retain cannot overflow; preserve the same live root. */
        (void)mc_world_info_player_name_set(record->player, &info_player_name);
        record->items[record->root.value.compound.length] = &record->player->root;
        ++record->root.value.compound.length;
    }
    *out = record;
    return info_status(error, MC_WORLD_INFO_OK, NULL);
}

mc_world_info_status mc_world_info_record_create(const mc_world_info *info,
                                                 mc_world_info_record **out,
                                                 mc_world_info_error *error) {
    return info_record_create(info, NULL, true, out, error);
}

mc_world_info_status mc_world_info_record_create_with_player(const mc_world_info *info,
                                                             mc_world_info_player *player,
                                                             mc_world_info_record **out,
                                                             mc_world_info_error *error) {
    return info_record_create(info, player, false, out, error);
}

const mc_nbt_tag *mc_world_info_record_root(const mc_world_info_record *record) {
    return record == NULL ? NULL : &record->root;
}

mc_world_info_name *mc_world_info_record_name(const mc_world_info_record *record) {
    return record == NULL ? NULL : record->name;
}

void mc_world_info_record_release(mc_world_info_record *record) {
    if (record != NULL) {
        mc_world_info_name_release(record->name);
        mc_world_info_player_release(record->player);
        free(record);
    }
}
