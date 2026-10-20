#include "mc/world/storage/chunk.h"

#include "chunk_build_internal.h"

#include <stdlib.h>
#include <string.h>

enum { STORED_FIELD_COUNT = 11 };

struct mc_stored_chunk {
    mc_chunk_state state;
    mc_chunk_data data;
    uint8_t blocks[MC_CHUNK_BLOCKS];
    mc_nbt_document *source;
};

static const char *const stored_names[STORED_FIELD_COUNT] = {
    "xPos",       "zPos",      "LastUpdate",       "Blocks",   "Data",        "SkyLight",
    "BlockLight", "HeightMap", "TerrainPopulated", "Entities", "TileEntities"};

static mc_stored_chunk_status stored_error(mc_stored_chunk_error *error,
                                           mc_stored_chunk_status status, const char *field,
                                           mc_nbt_error nbt)
{
    if (error != NULL)
        *error = (mc_stored_chunk_error){status, field, nbt};
    return status;
}

static mc_stored_chunk_status stored_status(mc_stored_chunk_error *error,
                                            mc_stored_chunk_status status, const char *field)
{
    return stored_error(error, status, field, (mc_nbt_error){MC_NBT_OK, 0});
}

static mc_stored_chunk_status stored_nbt_error(mc_stored_chunk_error *error, mc_nbt_status status,
                                               mc_nbt_error nbt)
{
    mc_stored_chunk_status result = status == MC_NBT_LIMIT ? MC_STORED_CHUNK_LIMIT
                                    : status == MC_NBT_ALLOCATION_FAILED
                                        ? MC_STORED_CHUNK_ALLOCATION_FAILED
                                        : MC_STORED_CHUNK_NBT_ERROR;
    return stored_error(error, result, NULL, nbt);
}

const char *mc_stored_chunk_status_string(mc_stored_chunk_status status)
{
    switch (status) {
        case MC_STORED_CHUNK_OK:
            return "ok";
        case MC_STORED_CHUNK_INVALID_ARGUMENT:
            return "invalid argument";
        case MC_STORED_CHUNK_MISSING_LEVEL:
            return "missing Level";
        case MC_STORED_CHUNK_MISSING_BLOCKS:
            return "missing Blocks";
        case MC_STORED_CHUNK_INVALID_FIELD:
            return "wrong chunk field type";
        case MC_STORED_CHUNK_INVALID_SHAPE:
            return "unsupported chunk array shape";
        case MC_STORED_CHUNK_UNSUPPORTED_EDIT:
            return "unsupported entity or tile edit";
        case MC_STORED_CHUNK_LIMIT:
            return "resource limit";
        case MC_STORED_CHUNK_ALLOCATION_FAILED:
            return "allocation failed";
        case MC_STORED_CHUNK_NBT_ERROR:
            return "invalid NBT record";
    }
    return "unknown stored chunk status";
}

static mc_nbt_string stored_name(const char *ascii, uint16_t units[16])
{
    size_t length = strlen(ascii);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)ascii[i];
    return (mc_nbt_string){units, length};
}

static const mc_nbt_tag *stored_get(const mc_nbt_tag *compound, const char *ascii)
{
    uint16_t units[16];
    mc_nbt_string name = stored_name(ascii, units);
    return mc_nbt_compound_get(compound, name.units, name.length);
}

static bool stored_typed(const mc_nbt_tag *parent, const char *name, mc_nbt_type type,
                         const mc_nbt_tag **out, mc_stored_chunk_error *error)
{
    const mc_nbt_tag *tag = stored_get(parent, name);
    if (tag != NULL && tag->type != type) {
        (void)stored_status(error, MC_STORED_CHUNK_INVALID_FIELD, name);
        return false;
    }
    *out = tag;
    return true;
}

static bool stored_array(const mc_nbt_tag *level, const char *name, uint8_t *output, size_t length,
                         mc_stored_chunk_error *error)
{
    const mc_nbt_tag *tag;
    if (!stored_typed(level, name, MC_NBT_BYTE_ARRAY, &tag, error))
        return false;
    if (tag == NULL || tag->value.byte_array.length != length) {
        (void)stored_status(error, MC_STORED_CHUNK_INVALID_SHAPE, name);
        return false;
    }
    memcpy(output, tag->value.byte_array.data, length);
    return true;
}

static bool stored_list(const mc_nbt_tag *level, const char *name, const mc_nbt_tag **out,
                        mc_stored_chunk_error *error)
{
    if (!stored_typed(level, name, MC_NBT_LIST, out, error))
        return false;
    if (*out != NULL) {
        for (size_t i = 0; i < (*out)->value.list.length; ++i) {
            if ((*out)->value.list.items[i]->type != MC_NBT_COMPOUND) {
                (void)stored_status(error, MC_STORED_CHUNK_INVALID_FIELD, name);
                return false;
            }
        }
    }
    return true;
}

static bool stored_limits(const mc_nbt_limits *requested, mc_nbt_limits *limits)
{
    *limits = requested == NULL ? mc_nbt_default_limits() : *requested;
    return limits->max_depth != 0 && limits->max_depth <= 128;
}

static void stored_bind(mc_stored_chunk *chunk)
{
    chunk->state.data = &chunk->data;
    chunk->data.blocks = chunk->blocks;
    chunk->data.blocks_capacity = sizeof(chunk->blocks);
}

static mc_stored_chunk_status stored_decode_fields(mc_stored_chunk *chunk, bool game,
                                                   mc_stored_chunk_error *error)
{
    const mc_nbt_tag *root = mc_nbt_document_root(chunk->source);
    const mc_nbt_tag *level;
    if (!stored_typed(root, "Level", MC_NBT_COMPOUND, &level, error))
        return MC_STORED_CHUNK_INVALID_FIELD;
    if (level == NULL)
        return stored_status(error, MC_STORED_CHUNK_MISSING_LEVEL, "Level");
    if (stored_get(level, "Blocks") == NULL)
        return stored_status(error, MC_STORED_CHUNK_MISSING_BLOCKS, "Blocks");
    const mc_nbt_tag *x, *z, *populated, *entities = NULL, *tiles = NULL;
    if (!stored_typed(level, "xPos", MC_NBT_INT, &x, error) ||
        !stored_typed(level, "zPos", MC_NBT_INT, &z, error) ||
        !stored_array(level, "Blocks", chunk->blocks, sizeof(chunk->blocks), error) ||
        !stored_array(level, "Data", chunk->data.metadata, sizeof(chunk->data.metadata), error) ||
        !stored_array(level, "SkyLight", chunk->data.skylight, sizeof(chunk->data.skylight),
                      error) ||
        !stored_array(level, "BlockLight", chunk->data.block_light, sizeof(chunk->data.block_light),
                      error) ||
        !stored_array(level, "HeightMap", chunk->data.heightmap, sizeof(chunk->data.heightmap),
                      error) ||
        !stored_typed(level, "TerrainPopulated", MC_NBT_BYTE, &populated, error)) {
        return error == NULL ? MC_STORED_CHUNK_INVALID_FIELD : error->status;
    }
    if (!game && (!stored_list(level, "Entities", &entities, error) ||
                  !stored_list(level, "TileEntities", &tiles, error))) {
        return error == NULL ? MC_STORED_CHUNK_INVALID_FIELD : error->status;
    }
    (void)tiles;
    chunk->state.x = x == NULL ? 0 : x->value.int_value;
    chunk->state.z = z == NULL ? 0 : z->value.int_value;
    chunk->state.terrain_populated = populated != NULL && populated->value.byte_value != 0;
    chunk->state.has_entities = !game && entities != NULL && entities->value.list.length != 0;
    return stored_status(error, MC_STORED_CHUNK_OK, NULL);
}

static mc_stored_chunk_status stored_decode(const uint8_t *input, size_t length,
                                            const mc_nbt_limits *requested, bool game,
                                            mc_stored_chunk **out, size_t *consumed,
                                            mc_stored_chunk_error *error)
{
    mc_nbt_limits limits;
    if (out == NULL || (input == NULL && length != 0) || !stored_limits(requested, &limits))
        return stored_status(error, MC_STORED_CHUNK_INVALID_ARGUMENT, NULL);
    if (sizeof(mc_stored_chunk) > limits.max_allocation_bytes)
        return stored_status(error, MC_STORED_CHUNK_LIMIT, NULL);
    mc_stored_chunk *chunk = calloc(1, sizeof(*chunk));
    if (chunk == NULL)
        return stored_status(error, MC_STORED_CHUNK_ALLOCATION_FAILED, NULL);
    limits.max_allocation_bytes -= sizeof(*chunk);
    size_t used = 0;
    mc_nbt_error nbt;
    mc_nbt_status status =
        mc_nbt_read_compound(input, length, &limits, &chunk->source, &used, &nbt);
    if (status != MC_NBT_OK) {
        free(chunk);
        return stored_nbt_error(error, status, nbt);
    }
    stored_bind(chunk);
    mc_stored_chunk_error field_error;
    mc_stored_chunk_status decoded = stored_decode_fields(chunk, game, &field_error);
    if (decoded != MC_STORED_CHUNK_OK) {
        mc_stored_chunk_free(chunk);
        if (error != NULL)
            *error = field_error;
        return decoded;
    }
    *out = chunk;
    if (consumed != NULL)
        *consumed = used;
    return stored_status(error, MC_STORED_CHUNK_OK, NULL);
}

mc_stored_chunk_status mc_stored_chunk_decode(const uint8_t *input, size_t length,
                                              const mc_nbt_limits *requested, mc_stored_chunk **out,
                                              size_t *consumed, mc_stored_chunk_error *error)
{
    return stored_decode(input, length, requested, false, out, consumed, error);
}

mc_stored_chunk_status mc_stored_chunk_decode_game(const uint8_t *input, size_t length,
                                                   const mc_nbt_limits *requested,
                                                   mc_stored_chunk **out, size_t *consumed,
                                                   mc_stored_chunk_error *error)
{
    return stored_decode(input, length, requested, true, out, consumed, error);
}

mc_stored_chunk_status mc_stored_chunk_create(const mc_chunk_state *state,
                                              const mc_nbt_limits *requested, mc_stored_chunk **out,
                                              mc_stored_chunk_error *error)
{
    mc_nbt_limits limits;
    if (state == NULL || out == NULL || state->blank || state->data == NULL ||
        state->data->blocks == NULL || state->data->blocks_capacity < MC_CHUNK_BLOCKS ||
        !stored_limits(requested, &limits))
        return stored_status(error, MC_STORED_CHUNK_INVALID_ARGUMENT, NULL);
    if (state->has_entities)
        return stored_status(error, MC_STORED_CHUNK_UNSUPPORTED_EDIT, "Entities");
    if (sizeof(mc_stored_chunk) > limits.max_allocation_bytes)
        return stored_status(error, MC_STORED_CHUNK_LIMIT, NULL);
    mc_stored_chunk *chunk = malloc(sizeof(*chunk));
    if (chunk == NULL)
        return stored_status(error, MC_STORED_CHUNK_ALLOCATION_FAILED, NULL);
    chunk->state = *state;
    chunk->data = *state->data;
    memcpy(chunk->blocks, state->data->blocks, sizeof(chunk->blocks));
    chunk->source = NULL;
    stored_bind(chunk);
    *out = chunk;
    return stored_status(error, MC_STORED_CHUNK_OK, NULL);
}

mc_chunk_state *mc_stored_chunk_state(mc_stored_chunk *chunk)
{
    return chunk == NULL ? NULL : &chunk->state;
}

const mc_nbt_tag *mc_stored_chunk_root(const mc_stored_chunk *chunk)
{
    return chunk == NULL ? NULL : mc_nbt_document_root(chunk->source);
}

static bool stored_valid(const mc_stored_chunk *chunk)
{
    return chunk != NULL && !chunk->state.blank && chunk->state.data == &chunk->data &&
           chunk->data.blocks == chunk->blocks &&
           chunk->data.blocks_capacity == sizeof(chunk->blocks);
}

bool mc_stored_chunk_storage_valid(const mc_stored_chunk *chunk)
{
    return stored_valid(chunk);
}

mc_stored_chunk_status mc_stored_chunk_normalize(mc_stored_chunk *chunk, const uint8_t *map,
                                                 size_t length)
{
    if (!stored_valid(chunk) || map == NULL || length < 256)
        return MC_STORED_CHUNK_INVALID_ARGUMENT;
    for (size_t i = 0; i < 256; ++i) {
        if (map[i] != 0 && map[i] != (uint8_t)i)
            return MC_STORED_CHUNK_INVALID_ARGUMENT;
    }
    for (size_t i = 0; i < sizeof(chunk->blocks); ++i)
        chunk->blocks[i] = map[chunk->blocks[i]];
    return MC_STORED_CHUNK_OK;
}

static bool stored_same_array(const mc_nbt_tag *level, const char *name, const uint8_t *data,
                              size_t length)
{
    const mc_nbt_tag *tag = stored_get(level, name);
    return tag != NULL && tag->value.byte_array.length == length &&
           memcmp(tag->value.byte_array.data, data, length) == 0;
}

static bool stored_supported(const mc_stored_chunk *chunk, const mc_nbt_tag *level)
{
    const mc_nbt_tag *entities = stored_get(level, "Entities");
    const mc_nbt_tag *tiles = stored_get(level, "TileEntities");
    bool has_entities = entities != NULL && entities->value.list.length != 0;
    bool has_tiles = tiles != NULL && tiles->value.list.length != 0;
    if (chunk->state.has_entities != has_entities)
        return false;
    if (!has_entities && !has_tiles)
        return true;
    const mc_nbt_tag *x = stored_get(level, "xPos"), *z = stored_get(level, "zPos");
    const mc_nbt_tag *populated = stored_get(level, "TerrainPopulated");
    return chunk->state.x == (x == NULL ? 0 : x->value.int_value) &&
           chunk->state.z == (z == NULL ? 0 : z->value.int_value) &&
           chunk->state.terrain_populated ==
               (populated != NULL && populated->value.byte_value != 0) &&
           stored_same_array(level, "Blocks", chunk->blocks, sizeof(chunk->blocks)) &&
           stored_same_array(level, "Data", chunk->data.metadata, sizeof(chunk->data.metadata)) &&
           stored_same_array(level, "SkyLight", chunk->data.skylight,
                             sizeof(chunk->data.skylight)) &&
           stored_same_array(level, "BlockLight", chunk->data.block_light,
                             sizeof(chunk->data.block_light)) &&
           stored_same_array(level, "HeightMap", chunk->data.heightmap,
                             sizeof(chunk->data.heightmap));
}

static void stored_fields(const mc_chunk_state *state, const mc_nbt_tag *level, int64_t world_time,
                          bool retain_lists, mc_nbt_tag fields[STORED_FIELD_COUNT],
                          uint16_t names[STORED_FIELD_COUNT][16])
{
    for (size_t i = 0; i < STORED_FIELD_COUNT; ++i)
        fields[i].name = stored_name(stored_names[i], names[i]);
    fields[0].type = fields[1].type = MC_NBT_INT;
    fields[0].value.int_value = state->x;
    fields[1].value.int_value = state->z;
    fields[2].type = MC_NBT_LONG;
    fields[2].value.long_value = world_time;
    const uint8_t *arrays[] = {state->data->blocks, state->data->metadata, state->data->skylight,
                               state->data->block_light, state->data->heightmap};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, sizeof(state->data->metadata),
                              sizeof(state->data->skylight), sizeof(state->data->block_light),
                              sizeof(state->data->heightmap)};
    for (size_t i = 0; i < 5; ++i) {
        fields[3u + i].type = MC_NBT_BYTE_ARRAY;
        fields[3u + i].value.byte_array.data = arrays[i];
        fields[3u + i].value.byte_array.length = lengths[i];
    }
    fields[8].type = MC_NBT_BYTE;
    fields[8].value.byte_value = state->terrain_populated ? 1 : 0;
    for (size_t i = 9; i < STORED_FIELD_COUNT; ++i) {
        const mc_nbt_tag *old = retain_lists ? stored_get(level, stored_names[i]) : NULL;
        fields[i].type = MC_NBT_LIST;
        if (old == NULL) {
            fields[i].value.list.element_type = MC_NBT_COMPOUND;
        } else {
            fields[i].value.list = old->value.list;
        }
    }
}

static size_t stored_overlay(const mc_nbt_tag *baseline, const mc_nbt_tag *fields, size_t count,
                             const mc_nbt_tag **items)
{
    bool used[STORED_FIELD_COUNT] = {false};
    size_t length = 0;
    size_t old_length = baseline == NULL ? 0 : baseline->value.compound.length;
    for (size_t i = 0; i < old_length; ++i) {
        const mc_nbt_tag *old = baseline->value.compound.items[i];
        size_t replacement = count;
        for (size_t j = 0; j < count; ++j) {
            if (old->name.length == fields[j].name.length &&
                memcmp(old->name.units, fields[j].name.units,
                       old->name.length * sizeof(uint16_t)) == 0) {
                replacement = j;
                break;
            }
        }
        if (replacement == count) {
            items[length++] = old;
        } else {
            used[replacement] = true;
            items[length++] = &fields[replacement];
        }
    }
    for (size_t i = 0; i < count; ++i) {
        if (!used[i])
            items[length++] = &fields[i];
    }
    return length;
}

static mc_stored_chunk_status stored_build(const mc_chunk_state *state, const mc_nbt_tag *baseline,
                                           const mc_nbt_tag *level, int64_t world_time,
                                           mc_nbt_limits limits, mc_nbt_document **out,
                                           mc_stored_chunk_error *error)
{
    size_t root_count = baseline == NULL ? 0 : baseline->value.compound.length;
    size_t level_count = level == NULL ? 0 : level->value.compound.length;
    if (root_count > limits.max_elements || level_count > limits.max_elements ||
        root_count > SIZE_MAX - STORED_FIELD_COUNT - 1u ||
        level_count > SIZE_MAX - STORED_FIELD_COUNT - 1u - root_count)
        return stored_status(error, MC_STORED_CHUNK_LIMIT, NULL);
    size_t pointer_count = root_count + level_count + STORED_FIELD_COUNT + 1u;
    if (pointer_count > SIZE_MAX / sizeof(const mc_nbt_tag *) ||
        pointer_count * sizeof(const mc_nbt_tag *) > limits.max_allocation_bytes)
        return stored_status(error, MC_STORED_CHUNK_LIMIT, NULL);
    size_t scratch_bytes = pointer_count * sizeof(const mc_nbt_tag *);
    const mc_nbt_tag **items = malloc(scratch_bytes);
    if (items == NULL)
        return stored_status(error, MC_STORED_CHUNK_ALLOCATION_FAILED, NULL);
    mc_nbt_tag fields[STORED_FIELD_COUNT] = {0};
    uint16_t names[STORED_FIELD_COUNT][16];
    stored_fields(state, level, world_time, true, fields, names);
    uint16_t level_name[16];
    mc_nbt_tag replacement = {0};
    replacement.type = MC_NBT_COMPOUND;
    replacement.name = stored_name("Level", level_name);
    replacement.value.compound.items = items;
    replacement.value.compound.length = stored_overlay(level, fields, STORED_FIELD_COUNT, items);
    mc_nbt_tag root = {0};
    root.type = MC_NBT_COMPOUND;
    if (baseline != NULL)
        root.name = baseline->name;
    root.value.compound.items = items + level_count + STORED_FIELD_COUNT;
    root.value.compound.length =
        stored_overlay(baseline, &replacement, 1, items + level_count + STORED_FIELD_COUNT);
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
        if (status == MC_NBT_OK)
            status = mc_nbt_read_compound(encoded, written, &limits, &document, NULL, &nbt);
    }
    free(encoded);
    free(items);
    if (status != MC_NBT_OK)
        return stored_nbt_error(error, status, nbt);
    *out = document;
    return stored_status(error, MC_STORED_CHUNK_OK, NULL);
}

mc_stored_chunk_status mc_stored_chunk_build(const mc_stored_chunk *chunk, int64_t world_time,
                                             const mc_nbt_limits *requested, mc_nbt_document **out,
                                             mc_stored_chunk_error *error)
{
    mc_nbt_limits limits;
    if (!stored_valid(chunk) || out == NULL || !stored_limits(requested, &limits))
        return stored_status(error, MC_STORED_CHUNK_INVALID_ARGUMENT, NULL);
    const mc_nbt_tag *baseline = mc_stored_chunk_root(chunk);
    const mc_nbt_tag *level = stored_get(baseline, "Level");
    if (!stored_supported(chunk, level))
        return stored_status(error, MC_STORED_CHUNK_UNSUPPORTED_EDIT, NULL);
    return stored_build(&chunk->state, baseline, level, world_time, limits, out, error);
}

mc_stored_chunk_status mc_stored_chunk_build_fresh(const mc_chunk_state *state, int64_t world_time,
                                                   const mc_nbt_limits *requested,
                                                   mc_nbt_document **out,
                                                   mc_stored_chunk_error *error)
{
    mc_nbt_limits limits;
    if (state == NULL || out == NULL || state->blank || state->data == NULL ||
        state->data->blocks == NULL || state->data->blocks_capacity < MC_CHUNK_BLOCKS ||
        !stored_limits(requested, &limits))
        return stored_status(error, MC_STORED_CHUNK_INVALID_ARGUMENT, NULL);
    if (state->has_entities)
        return stored_status(error, MC_STORED_CHUNK_UNSUPPORTED_EDIT, "Entities");
    return stored_build(state, NULL, NULL, world_time, limits, out, error);
}

mc_stored_chunk_status mc_stored_chunk_build_game_records(
    const mc_stored_chunk *stored, const mc_chunk_state *state, const mc_world_chunk *chunk,
    int64_t world_time, const mc_nbt_tag *entity_records, const mc_nbt_tag *tile_records,
    const mc_nbt_tag *baseline_entities, const bool *consumed, size_t consumed_count,
    const mc_nbt_limits *requested, mc_nbt_tree **out, mc_stored_chunk_error *error)
{
    mc_nbt_limits limits;
    if (state == NULL || chunk == NULL || out == NULL || state->blank || state->data == NULL ||
        state->data->blocks == NULL || state->data->blocks_capacity != (size_t)MC_CHUNK_BLOCKS ||
        mc_world_chunk_state((mc_world_chunk *)chunk) != state ||
        (stored != NULL && (!stored_valid(stored) || state != &stored->state)) ||
        !stored_limits(requested, &limits))
        return stored_status(error, MC_STORED_CHUNK_INVALID_ARGUMENT, NULL);
    if (entity_records == NULL || entity_records->type != MC_NBT_LIST ||
        entity_records->value.list.element_type != MC_NBT_COMPOUND ||
        (entity_records->value.list.length != 0 && entity_records->value.list.items == NULL))
        return stored_status(error, MC_STORED_CHUNK_INVALID_FIELD, "Entities");
    if (tile_records == NULL || tile_records->type != MC_NBT_LIST ||
        tile_records->value.list.element_type != MC_NBT_COMPOUND)
        return stored_status(error, MC_STORED_CHUNK_INVALID_FIELD, "TileEntities");

    const mc_nbt_tag *baseline = stored == NULL ? NULL : mc_stored_chunk_root(stored);
    const mc_nbt_tag *level = stored_get(baseline, "Level");
    if (stored != NULL && (baseline == NULL || baseline->type != MC_NBT_COMPOUND || level == NULL ||
                           level->type != MC_NBT_COMPOUND))
        return stored_status(error, MC_STORED_CHUNK_MISSING_LEVEL, "Level");
    const mc_nbt_tag *old_entities = stored_get(level, "Entities");
    if (old_entities != NULL && old_entities->type != MC_NBT_LIST)
        return stored_status(error, MC_STORED_CHUNK_INVALID_FIELD, "Entities");
    const size_t old_count = old_entities == NULL ? 0 : old_entities->value.list.length;
    if (old_count == 0) {
        if (baseline_entities != NULL || consumed != NULL || consumed_count != 0)
            return stored_status(error, MC_STORED_CHUNK_INVALID_ARGUMENT, "Entities");
    } else if (baseline_entities != old_entities || consumed == NULL ||
               consumed_count != old_count || old_entities->value.list.items == NULL) {
        return stored_status(error, MC_STORED_CHUNK_UNSUPPORTED_EDIT, "Entities");
    }
    if (old_count > limits.max_elements ||
        entity_records->value.list.length > limits.max_elements ||
        tile_records->value.list.length > limits.max_elements)
        return stored_status(error, MC_STORED_CHUNK_LIMIT, NULL);
    size_t unknown_count = 0;
    for (size_t index = 0; index < old_count; ++index) {
        const mc_nbt_tag *record = old_entities->value.list.items[index];
        if (record == NULL || record->type != MC_NBT_COMPOUND)
            return stored_status(error, MC_STORED_CHUNK_INVALID_FIELD, "Entities");
        if (!consumed[index])
            ++unknown_count;
    }
    if (entity_records->value.list.length > limits.max_elements - unknown_count)
        return stored_status(error, MC_STORED_CHUNK_LIMIT, "Entities");
    const size_t entity_count = unknown_count + entity_records->value.list.length;

    mc_world_chunk_fields source;
    mc_world_chunk_nibble_fields metadata, skylight, block_light;
    if (mc_world_chunk_fields_get(chunk, &source) != MC_WORLD_STORAGE_OK)
        return stored_status(error, MC_STORED_CHUNK_INVALID_ARGUMENT, NULL);
    if (mc_world_chunk_nibble_fields_get(source.e, &metadata) != MC_WORLD_STORAGE_OK)
        return stored_status(error, MC_STORED_CHUNK_INVALID_FIELD, "Data");
    if (mc_world_chunk_nibble_fields_get(source.f, &skylight) != MC_WORLD_STORAGE_OK)
        return stored_status(error, MC_STORED_CHUNK_INVALID_FIELD, "SkyLight");
    if (mc_world_chunk_nibble_fields_get(source.g, &block_light) != MC_WORLD_STORAGE_OK)
        return stored_status(error, MC_STORED_CHUNK_INVALID_FIELD, "BlockLight");
    if (metadata.length != sizeof(state->data->metadata))
        return stored_status(error, MC_STORED_CHUNK_INVALID_SHAPE, "Data");
    if (skylight.length != sizeof(state->data->skylight))
        return stored_status(error, MC_STORED_CHUNK_INVALID_SHAPE, "SkyLight");
    if (block_light.length != sizeof(state->data->block_light))
        return stored_status(error, MC_STORED_CHUNK_INVALID_SHAPE, "BlockLight");

    size_t root_count = baseline == NULL ? 0 : baseline->value.compound.length;
    size_t level_count = level == NULL ? 0 : level->value.compound.length;
    if (root_count > limits.max_elements || level_count > limits.max_elements ||
        root_count > SIZE_MAX - STORED_FIELD_COUNT - 1u ||
        level_count > SIZE_MAX - STORED_FIELD_COUNT - 1u - root_count)
        return stored_status(error, MC_STORED_CHUNK_LIMIT, NULL);
    size_t pointer_count = root_count + level_count + STORED_FIELD_COUNT + 1u;
    const size_t entity_offset = pointer_count;
    if (entity_count > SIZE_MAX - pointer_count)
        return stored_status(error, MC_STORED_CHUNK_LIMIT, NULL);
    pointer_count += entity_count;
    if (pointer_count > SIZE_MAX / sizeof(const mc_nbt_tag *) ||
        pointer_count * sizeof(const mc_nbt_tag *) > limits.max_allocation_bytes)
        return stored_status(error, MC_STORED_CHUNK_LIMIT, NULL);
    size_t scratch_bytes = pointer_count * sizeof(const mc_nbt_tag *);
    const mc_nbt_tag **items = malloc(scratch_bytes);
    if (items == NULL)
        return stored_status(error, MC_STORED_CHUNK_ALLOCATION_FAILED, NULL);

    mc_nbt_tag fields[STORED_FIELD_COUNT] = {0};
    uint16_t names[STORED_FIELD_COUNT][16];
    stored_fields(state, level, world_time, false, fields, names);
    fields[3].value.byte_array.data = source.b;
    fields[4].value.byte_array.data = metadata.a;
    fields[5].value.byte_array.data = skylight.a;
    fields[6].value.byte_array.data = block_light.a;
    fields[7].value.byte_array.data = source.h;
    /* Explicit native storage policy: preserve only unknown source records.
     * A consumed slot receives the next current serialized occurrence, never
     * an old Entity payload or an Entity-pointer/ID-derived substitute. */
    const mc_nbt_tag **entity_items = items + entity_offset;
    size_t next_entity = 0, emitted = 0;
    for (size_t index = 0; index < old_count; ++index) {
        if (!consumed[index])
            entity_items[emitted++] = old_entities->value.list.items[index];
        else if (next_entity < entity_records->value.list.length)
            entity_items[emitted++] = entity_records->value.list.items[next_entity++];
    }
    while (next_entity < entity_records->value.list.length)
        entity_items[emitted++] = entity_records->value.list.items[next_entity++];
    fields[9].value.list.items = entity_count == 0 ? NULL : entity_items;
    fields[9].value.list.length = entity_count;
    fields[10].value.list = tile_records->value.list;
    uint16_t level_name[16];
    mc_nbt_tag replacement = {0}, root = {0};
    replacement.type = MC_NBT_COMPOUND;
    replacement.name = stored_name("Level", level_name);
    replacement.value.compound.items = items;
    replacement.value.compound.length = stored_overlay(level, fields, STORED_FIELD_COUNT, items);
    root.type = MC_NBT_COMPOUND;
    if (baseline != NULL)
        root.name = baseline->name;
    root.value.compound.items = items + level_count + STORED_FIELD_COUNT;
    root.value.compound.length =
        stored_overlay(baseline, &replacement, 1, items + level_count + STORED_FIELD_COUNT);
    limits.max_allocation_bytes -= scratch_bytes;
    mc_nbt_tree *built = NULL;
    mc_nbt_error nbt = {MC_NBT_OK, 0};
    mc_nbt_status status = mc_nbt_tree_copy_compound(&root, &limits, &built, &nbt);
    free(items);
    if (status != MC_NBT_OK)
        return stored_nbt_error(error, status, nbt);
    *out = built;
    return stored_status(error, MC_STORED_CHUNK_OK, NULL);
}

void mc_stored_chunk_free(mc_stored_chunk *chunk)
{
    if (chunk != NULL) {
        mc_nbt_document_free(chunk->source);
        free(chunk);
    }
}
