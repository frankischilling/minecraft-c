#include "mc/world/saved_data/value.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct mc_saved_data_class {
    mc_world_info_side side;
};

static const mc_saved_data_class map_classes[] = {{MC_WORLD_INFO_CLIENT}, {MC_WORLD_INFO_SERVER}};

typedef struct saved_map_list {
    void **items;
    size_t count, capacity;
} saved_map_list;

typedef struct saved_map_players {
    void *first, *last;
    size_t count;
} saved_map_players;

struct mc_saved_data_value {
    size_t holds;
    const mc_saved_data_class *type;
    mc_world_info_name *name;
    bool dirty;
    int32_t x_center, z_center, updates;
    int8_t dimension, scale;
    mc_nbt_byte_array *colors;
    saved_map_list trackers, decorations;
    saved_map_players player_trackers;
    mc_saved_data_map_cells live;
};

struct mc_saved_data_record {
    mc_nbt_tag root, fields[7];
    const mc_nbt_tag *items[7];
    uint16_t names[7][9];
    mc_nbt_byte_array *colors;
};

static mc_world_storage_status fail(mc_saved_data_error *error, mc_world_storage_status status,
                                    const char *operation, mc_saved_data_exception exception,
                                    unsigned int pc, unsigned int getter, const char *field,
                                    int32_t index, size_t length) {
    if (error != NULL)
        *error = (mc_saved_data_error){.cause = {status, operation, 0},
                                       .exception = exception,
                                       .bytecode = pc,
                                       .getter_bytecode = getter,
                                       .field = field,
                                       .index = index,
                                       .length = length};
    return status;
}

static mc_world_storage_status good(mc_saved_data_error *error) {
    return fail(error, MC_WORLD_STORAGE_OK, NULL, MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NULL, 0, 0);
}

static mc_world_storage_status array_status(mc_nbt_status status) {
    return status == MC_NBT_OK                  ? MC_WORLD_STORAGE_OK
           : status == MC_NBT_LIMIT             ? MC_WORLD_STORAGE_LIMIT
           : status == MC_NBT_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                : MC_WORLD_STORAGE_INVALID_ARGUMENT;
}

const mc_saved_data_class *mc_saved_data_map_class(mc_world_info_side side) {
    return side == MC_WORLD_INFO_CLIENT   ? &map_classes[0]
           : side == MC_WORLD_INFO_SERVER ? &map_classes[1]
                                          : NULL;
}

bool mc_saved_data_class_side(const mc_saved_data_class *type, mc_world_info_side *out) {
    if ((type != &map_classes[0] && type != &map_classes[1]) || out == NULL)
        return false;
    *out = type->side;
    return true;
}

mc_world_storage_status mc_saved_data_map_create(const mc_saved_data_class *type,
                                                 mc_world_info_name *name, size_t max_new_bytes,
                                                 mc_saved_data_value **out,
                                                 mc_saved_data_error *error) {
    mc_world_info_side side;
    if (out == NULL || !mc_saved_data_class_side(type, &side))
        return fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "MapData constructor arguments",
                    MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NULL, 0, 0);
    if (sizeof(mc_saved_data_value) > max_new_bytes)
        return fail(error, MC_WORLD_STORAGE_LIMIT, "MapData owner budget",
                    MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NULL, 0, 0);
    mc_saved_data_value *value = calloc(1, sizeof(*value));
    if (value == NULL)
        return fail(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "MapData owner",
                    MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NULL, 0, 0);
    if (name != NULL && mc_world_info_name_retain(name) != MC_WORLD_INFO_OK) {
        free(value);
        return fail(error, MC_WORLD_STORAGE_LIMIT, "SavedData name hold",
                    MC_SAVED_DATA_NO_EXCEPTION, 2, 0, NULL, 0, 0);
    }
    value->holds = 1;
    value->type = type;
    value->name = name;
    mc_nbt_status created =
        mc_nbt_byte_array_create(16384, NULL, max_new_bytes - sizeof(*value), &value->colors);
    if (created != MC_NBT_OK) {
        mc_saved_data_value_release(value);
        return fail(error, array_status(created), "MapData colors construction",
                    MC_SAVED_DATA_NO_EXCEPTION, 9, 0, "colors", 0, 0);
    }
    value->live = (mc_saved_data_map_cells){&value->x_center, &value->z_center, &value->updates,
                                            &value->dimension, &value->scale};
    *out = value;
    return good(error);
}

mc_world_storage_status mc_saved_data_value_retain(mc_saved_data_value *value) {
    if (value == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (value->holds == SIZE_MAX)
        return MC_WORLD_STORAGE_LIMIT;
    ++value->holds;
    return MC_WORLD_STORAGE_OK;
}

void mc_saved_data_value_release(mc_saved_data_value *value) {
    if (value != NULL && --value->holds == 0) {
        mc_world_info_name_release(value->name);
        mc_nbt_byte_array_release(value->colors);
        free(value->trackers.items);
        free(value->decorations.items);
        free(value);
    }
}

mc_world_info_name *mc_saved_data_value_name(const mc_saved_data_value *value) {
    return value == NULL ? NULL : value->name;
}

const mc_saved_data_class *mc_saved_data_value_class(const mc_saved_data_value *value) {
    return value == NULL ? NULL : value->type;
}

bool mc_saved_data_value_dirty(const mc_saved_data_value *value) {
    return value != NULL && value->dirty;
}

mc_world_storage_status mc_saved_data_value_dirty_set(mc_saved_data_value *value, bool dirty) {
    if (value == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    value->dirty = dirty;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_saved_data_value_mark_dirty(mc_saved_data_value *value) {
    return mc_saved_data_value_dirty_set(value, true);
}

mc_world_storage_status mc_saved_data_map_fields_get(const mc_saved_data_value *value,
                                                     mc_saved_data_map_fields *out) {
    if (value == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = (mc_saved_data_map_fields){.type = value->type,
                                      .name = value->name,
                                      .dirty = value->dirty,
                                      .x_center = value->x_center,
                                      .z_center = value->z_center,
                                      .updates = value->updates,
                                      .dimension = value->dimension,
                                      .scale = value->scale,
                                      .colors = value->colors,
                                      .trackers = &value->trackers,
                                      .decorations = &value->decorations,
                                      .player_trackers = &value->player_trackers,
                                      .tracker_count = value->trackers.count,
                                      .decoration_count = value->decorations.count,
                                      .player_tracker_count = value->player_trackers.count};
    return MC_WORLD_STORAGE_OK;
}

const mc_saved_data_map_cells *mc_saved_data_map_live(mc_saved_data_value *value) {
    return value == NULL ? NULL : &value->live;
}

mc_world_storage_status mc_saved_data_map_colors_set(mc_saved_data_value *value,
                                                     mc_nbt_byte_array *colors) {
    if (value == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (colors != NULL) {
        mc_nbt_status held = mc_nbt_byte_array_retain(colors);
        if (held != MC_NBT_OK)
            return array_status(held);
    }
    mc_nbt_byte_array *previous = value->colors;
    value->colors = colors;
    mc_nbt_byte_array_release(previous);
    return MC_WORLD_STORAGE_OK;
}

static const mc_nbt_tag *field(const mc_nbt_tag *compound, const char *name) {
    uint16_t units[9];
    size_t length = strlen(name);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)name[i];
    return mc_nbt_compound_get(compound, units, length);
}

static bool number(const mc_nbt_tag *compound, const char *name, mc_nbt_type type, unsigned int pc,
                   int32_t *out, mc_saved_data_error *error) {
    const mc_nbt_tag *tag = field(compound, name);
    if (tag == NULL) {
        *out = 0;
        return true;
    }
    if (tag->type != type) {
        fail(error, MC_WORLD_STORAGE_INVALID_DATA, "MapData numeric tag cast",
             MC_SAVED_DATA_CLASS_CAST, pc, 25, name, 0, 0);
        return false;
    }
    *out = type == MC_NBT_BYTE    ? tag->value.byte_value
           : type == MC_NBT_SHORT ? tag->value.short_value
                                  : tag->value.int_value;
    return true;
}

static bool input_colors(const mc_nbt_tag *compound, mc_nbt_document *document,
                         mc_nbt_byte_array *supplied, unsigned int pc, size_t max_new_bytes,
                         mc_nbt_byte_array **out, mc_saved_data_error *error) {
    const mc_nbt_tag *tag = field(compound, "colors");
    if (tag == NULL) {
        mc_nbt_status status = mc_nbt_byte_array_create(0, NULL, max_new_bytes, out);
        if (status != MC_NBT_OK)
            fail(error, array_status(status), "MapData missing colors allocation",
                 MC_SAVED_DATA_NO_EXCEPTION, pc, 14, "colors", 0, 0);
        return status == MC_NBT_OK;
    }
    if (tag->type != MC_NBT_BYTE_ARRAY) {
        fail(error, MC_WORLD_STORAGE_INVALID_DATA, "MapData colors tag cast",
             MC_SAVED_DATA_CLASS_CAST, pc, 27, "colors", 0, 0);
        return false;
    }
    if (document != NULL) {
        mc_nbt_status status = mc_nbt_document_byte_array(document, tag, out);
        if (status != MC_NBT_OK)
            fail(error, array_status(status), "MapData decoded colors identity",
                 MC_SAVED_DATA_NO_EXCEPTION, pc, 0, "colors", 0, 0);
        return status == MC_NBT_OK;
    }
    if (tag->value.byte_array.data != mc_nbt_byte_array_data(supplied) ||
        tag->value.byte_array.length != mc_nbt_byte_array_length(supplied)) {
        fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "MapData actual colors backing binding",
             MC_SAVED_DATA_NO_EXCEPTION, pc, 0, "colors", 0, 0);
        return false;
    }
    if (supplied != NULL) {
        mc_nbt_status status = mc_nbt_byte_array_retain(supplied);
        if (status != MC_NBT_OK) {
            fail(error, array_status(status), "MapData input colors hold",
                 MC_SAVED_DATA_NO_EXCEPTION, pc, 0, "colors", 0, 0);
            return false;
        }
    }
    *out = supplied;
    return true;
}

static mc_world_storage_status map_read(mc_saved_data_value *value, const mc_nbt_tag *compound,
                                        mc_nbt_document *document, mc_nbt_byte_array *colors,
                                        size_t max_new_bytes, mc_saved_data_error *error) {
    mc_saved_data_error local;
    if (error == NULL)
        error = &local;
    good(error);
    if (value == NULL)
        return fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "MapData read receiver",
                    MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NULL, 0, 0);
    if (compound == NULL || compound->type != MC_NBT_COMPOUND)
        return fail(error, MC_WORLD_STORAGE_INVALID_DATA, "MapData read Compound receiver",
                    compound == NULL ? MC_SAVED_DATA_NULL_REFERENCE : MC_SAVED_DATA_CLASS_CAST, 4,
                    0, "dimension", 0, 0);
    int32_t number_value, width, height;
    if (!number(compound, "dimension", MC_NBT_BYTE, 4, &number_value, error))
        return error->cause.status;
    value->dimension = (int8_t)number_value;
    if (!number(compound, "xCenter", MC_NBT_INT, 14, &value->x_center, error) ||
        !number(compound, "zCenter", MC_NBT_INT, 24, &value->z_center, error) ||
        !number(compound, "scale", MC_NBT_BYTE, 34, &number_value, error))
        return error->cause.status;
    value->scale = (int8_t)number_value;
    if (value->scale < 0)
        value->scale = 0;
    if (value->scale > 4)
        value->scale = 4;
    if (!number(compound, "width", MC_NBT_SHORT, 68, &width, error) ||
        !number(compound, "height", MC_NBT_SHORT, 75, &height, error))
        return error->cause.status;
    mc_nbt_byte_array *source = NULL;
    if (!input_colors(compound, document, colors, width == 128 && height == 128 ? 97u : 109u,
                      max_new_bytes, &source, error))
        return error->cause.status;
    if (width == 128 && height == 128) {
        mc_nbt_byte_array *previous = value->colors;
        value->colors = source;
        mc_nbt_byte_array_release(previous);
        return good(error);
    }
    mc_nbt_byte_array *replacement = NULL;
    size_t input_new =
        field(compound, "colors") == NULL ? mc_nbt_byte_array_allocation_bytes(source) : 0;
    mc_nbt_status allocated =
        mc_nbt_byte_array_create(16384, NULL, max_new_bytes - input_new, &replacement);
    if (allocated != MC_NBT_OK) {
        mc_nbt_byte_array_release(source);
        return fail(error, array_status(allocated), "MapData resized colors allocation",
                    MC_SAVED_DATA_NO_EXCEPTION, 118, 0, "colors", 0, 0);
    }
    mc_nbt_byte_array *previous = value->colors;
    value->colors = replacement;
    mc_nbt_byte_array_release(previous);
    uint8_t *destination = mc_nbt_byte_array_data(replacement);
    const uint8_t *pixels = mc_nbt_byte_array_data(source);
    const size_t length = mc_nbt_byte_array_length(source);
    const int32_t offset_x = (128 - width) / 2, offset_z = (128 - height) / 2;
    /* The original AND guards never skip a pixel. Keep their comparisons separate
       so strict compilers accept the same behavior without a tautology warning. */
    for (int32_t z = 0; z < height; ++z) {
        int32_t dz = z + offset_z;
        const bool z_below = dz < 0, z_above = dz >= 128;
        if (z_below && z_above)
            continue;
        for (int32_t x = 0; x < width; ++x) {
            int32_t dx = x + offset_x;
            const bool x_below = dx < 0, x_above = dx >= 128;
            if (x_below && x_above)
                continue;
            int32_t from = x + z * width;
            if (source == NULL || from < 0 || (size_t)from >= length) {
                const bool null_source = source == NULL;
                mc_nbt_byte_array_release(source);
                return fail(error, MC_WORLD_STORAGE_INVALID_DATA, "MapData source colors load",
                            null_source ? MC_SAVED_DATA_NULL_REFERENCE : MC_SAVED_DATA_ARRAY_BOUNDS,
                            227, 0, "colors", from, length);
            }
            uint8_t pixel = pixels[(size_t)from];
            int32_t to = dx + dz * 128;
            if (to < 0 || to >= 16384) {
                mc_nbt_byte_array_release(source);
                return fail(error, MC_WORLD_STORAGE_INVALID_DATA,
                            "MapData destination colors store", MC_SAVED_DATA_ARRAY_BOUNDS, 228, 0,
                            "colors", to, 16384);
            }
            destination[(size_t)to] = pixel;
        }
    }
    mc_nbt_byte_array_release(source);
    return good(error);
}

mc_world_storage_status mc_saved_data_map_read(mc_saved_data_value *value,
                                               const mc_nbt_tag *compound,
                                               mc_nbt_byte_array *colors, size_t max_new_bytes,
                                               mc_saved_data_error *error) {
    return map_read(value, compound, NULL, colors, max_new_bytes, error);
}

mc_world_storage_status mc_saved_data_map_read_document(mc_saved_data_value *value,
                                                        mc_nbt_document *document,
                                                        const mc_nbt_tag *compound,
                                                        size_t max_new_bytes,
                                                        mc_saved_data_error *error) {
    if (document == NULL)
        return fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "MapData decoded input document",
                    MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NULL, 0, 0);
    return map_read(value, compound, document, NULL, max_new_bytes, error);
}

static void record_field(mc_saved_data_record *record, size_t index, const char *name,
                         mc_nbt_type type) {
    size_t length = strlen(name);
    for (size_t i = 0; i < length; ++i)
        record->names[index][i] = (uint16_t)(unsigned char)name[i];
    record->fields[index].type = type;
    record->fields[index].name = (mc_nbt_string){record->names[index], length};
    record->items[index] = &record->fields[index];
    record->root.value.compound.length = index + 1;
}

mc_world_storage_status mc_saved_data_map_write(const mc_saved_data_value *value,
                                                size_t max_new_bytes, mc_saved_data_record **out,
                                                mc_saved_data_error *error) {
    if (value == NULL || out == NULL)
        return fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "MapData write arguments",
                    MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NULL, 0, 0);
    if (sizeof(mc_saved_data_record) > max_new_bytes)
        return fail(error, MC_WORLD_STORAGE_LIMIT, "MapData record budget",
                    MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NULL, 0, 0);
    mc_saved_data_record *record = calloc(1, sizeof(*record));
    if (record == NULL)
        return fail(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "MapData record",
                    MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NULL, 0, 0);
    record->root.type = MC_NBT_COMPOUND;
    record->root.value.compound.items = record->items;
    record_field(record, 0, "dimension", MC_NBT_BYTE);
    record->fields[0].value.byte_value = value->dimension;
    record_field(record, 1, "xCenter", MC_NBT_INT);
    record->fields[1].value.int_value = value->x_center;
    record_field(record, 2, "zCenter", MC_NBT_INT);
    record->fields[2].value.int_value = value->z_center;
    record_field(record, 3, "scale", MC_NBT_BYTE);
    record->fields[3].value.byte_value = value->scale;
    record_field(record, 4, "width", MC_NBT_SHORT);
    record->fields[4].value.short_value = 128;
    record_field(record, 5, "height", MC_NBT_SHORT);
    record->fields[5].value.short_value = 128;
    if (value->colors != NULL) {
        mc_nbt_status held = mc_nbt_byte_array_retain(value->colors);
        if (held != MC_NBT_OK) {
            free(record);
            return fail(error, array_status(held), "MapData output colors hold",
                        MC_SAVED_DATA_NO_EXCEPTION, 65, 0, "colors", 0, 0);
        }
    }
    record->colors = value->colors;
    record_field(record, 6, "colors", MC_NBT_BYTE_ARRAY);
    record->fields[6].value.byte_array.data = mc_nbt_byte_array_data(value->colors);
    record->fields[6].value.byte_array.length = mc_nbt_byte_array_length(value->colors);
    *out = record;
    return good(error);
}

const mc_nbt_tag *mc_saved_data_record_root(const mc_saved_data_record *record,
                                            mc_saved_data_error *error) {
    if (record == NULL) {
        fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "SavedData record receiver",
             MC_SAVED_DATA_NO_EXCEPTION, 0, 0, NULL, 0, 0);
        return NULL;
    }
    if (record->colors == NULL) {
        fail(error, MC_WORLD_STORAGE_INVALID_DATA, "MapData nullable colors encoding",
             MC_SAVED_DATA_NULL_REFERENCE, 0, 0, "colors", 0, 0);
        return NULL;
    }
    good(error);
    return &record->root;
}

mc_nbt_byte_array *mc_saved_data_record_colors(const mc_saved_data_record *record) {
    return record == NULL ? NULL : record->colors;
}

void mc_saved_data_record_release(mc_saved_data_record *record) {
    if (record != NULL) {
        mc_nbt_byte_array_release(record->colors);
        free(record);
    }
}
