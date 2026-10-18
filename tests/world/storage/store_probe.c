#include "mc/world/storage/store.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* Trusted test adapter, not a game command interface. All filesystem paths and
 * NBT bytes arrive as lowercase hex, independently of the native argv codepage.
 * LIMITS changes subsequent opens only. A cache belongs to one dimension until
 * CACHE_RESET; resetting/closing retires its references before freeing owners.
 * Registration callbacks are explicit empty collections, never entity/feature
 * simulations. Handles are stable, monotonically assigned and never recycled. */
#define DATA_LIMIT (2u * 1024u * 1024u - 256u)
#define LINE_LIMIT (4u * 1024u * 1024u)
#define INPUT_LIMIT (32u * 1024u * 1024u)
#define OUTPUT_LIMIT (128u * 1024u * 1024u)
#define COMMAND_LIMIT 4096u
#define HANDLE_LIMIT 1024u
#define CACHE_LIMIT 128u
#define LIST_LIMIT 256u

typedef struct callback_event {
    mc_chunk_state *chunk;
    size_t slice;
    bool tiles;
} callback_event;

typedef struct slot {
    mc_world_store *store;
    mc_level_info draft;
    bool has_draft;
    mc_chunk_cache cache;
    mc_chunk_cache_entry entries[CACHE_LIMIT];
    mc_chunk_state *insertions[LIST_LIMIT];
    mc_chunk_pending pending;
    int32_t keys[CACHE_LIMIT];
    mc_chunk_state fallback;
    mc_world_store_loader loader;
    mc_chunk_acquire_context context;
    int64_t world_time;
    bool dimension_set;
    int32_t dimension;
    callback_event callbacks[9];
    size_t callback_count;
} slot;

typedef struct handle {
    mc_chunk_state *chunk;
    size_t slot;
} handle;

static slot slots[2];
static handle handles[HANDLE_LIMIT];
static size_t handle_count, selected, output_bytes;
static bool output_ok = true;

static void emit(const char *text)
{
    size_t length = strlen(text);
    if (!output_ok || length > OUTPUT_LIMIT - output_bytes) {
        output_ok = false;
        return;
    }
    output_bytes += length;
    if (fwrite(text, 1, length, stdout) != length)
        output_ok = false;
}

static void format(const char *pattern, ...)
{
    char text[1024];
    va_list arguments;
    va_start(arguments, pattern);
    int length = vsnprintf(text, sizeof(text), pattern, arguments);
    va_end(arguments);
    if (length < 0 || (size_t)length >= sizeof(text))
        output_ok = false;
    else
        emit(text);
}

static void hex(const uint8_t *bytes, size_t length)
{
    static const char digits[] = "0123456789abcdef";
    char text[257];
    for (size_t offset = 0; offset < length;) {
        size_t count = length - offset < 128u ? length - offset : 128u;
        for (size_t i = 0; i < count; ++i) {
            text[i * 2u] = digits[bytes[offset + i] >> 4u];
            text[i * 2u + 1u] = digits[bytes[offset + i] & 15u];
        }
        text[count * 2u] = '\0';
        emit(text);
        offset += count;
    }
}

static void json_string(const char *text)
{
    if (text == NULL) {
        emit("null");
        return;
    }
    emit("\"");
    for (; *text != '\0'; ++text) {
        unsigned char byte = (unsigned char)*text;
        if (byte < 32u || byte >= 127u || byte == '"' || byte == '\\')
            format("\\u%04x", (unsigned)byte);
        else {
            char value[2] = {(char)byte, '\0'};
            emit(value);
        }
    }
    emit("\"");
}

static bool number(const char *text, uint64_t limit, uint64_t *out)
{
    if (*text == '\0' || (text[0] == '0' && text[1] != '\0'))
        return false;
    uint64_t value = 0;
    for (; *text != '\0'; ++text) {
        if (*text < '0' || *text > '9')
            return false;
        uint64_t digit = (uint64_t)(*text - '0');
        if (value > limit / 10u || (value == limit / 10u && digit > limit % 10u))
            return false;
        value = value * 10u + digit;
    }
    *out = value;
    return true;
}

static bool integer(const char *text, int32_t *out)
{
    bool negative = *text == '-';
    uint64_t value;
    if (!number(text + (negative ? 1 : 0), negative ? UINT64_C(2147483648) : INT32_MAX, &value) ||
        (negative && value == 0))
        return false;
    *out = negative ? -1 - (int32_t)(value - 1u) : (int32_t)value;
    return true;
}

static int digit(unsigned char byte)
{
    return byte >= '0' && byte <= '9'   ? byte - '0'
           : byte >= 'a' && byte <= 'f' ? byte - 'a' + 10
                                        : -1;
}

static bool decode(const char *text, size_t limit, uint8_t **out, size_t *length)
{
    size_t count = strcmp(text, "-") == 0 ? 0 : strlen(text);
    if (count % 2u != 0 || count / 2u > limit)
        return false;
    uint8_t *bytes = malloc(count / 2u + 1u);
    if (bytes == NULL)
        return false;
    for (size_t i = 0; i < count / 2u; ++i) {
        int high = digit((unsigned char)text[i * 2u]);
        int low = digit((unsigned char)text[i * 2u + 1u]);
        if (high < 0 || low < 0) {
            free(bytes);
            return false;
        }
        bytes[i] = (uint8_t)((unsigned)high * 16u + (unsigned)low);
    }
    bytes[count / 2u] = 0;
    *out = bytes;
    *length = count / 2u;
    return true;
}

static bool long_bits(const char *text, int64_t *out)
{
    if (strlen(text) != 16)
        return false;
    uint64_t value = 0;
    for (size_t i = 0; i < 16; ++i) {
        int part = digit((unsigned char)text[i]);
        if (part < 0)
            return false;
        value = value * UINT64_C(16) + (unsigned)part;
    }
    *out = value <= INT64_MAX ? (int64_t)value : -1 - (int64_t)(UINT64_MAX - value);
    return true;
}

static mc_world_storage_status failure(mc_world_storage_error *error,
                                       mc_world_storage_status status, const char *operation)
{
    *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static size_t identity(const mc_chunk_state *chunk)
{
    if (chunk == NULL || chunk->blank)
        return 0;
    for (size_t i = 0; i < handle_count; ++i)
        if (handles[i].chunk == chunk)
            return i + 1u;
    if (handle_count == HANDLE_LIMIT) {
        output_ok = false;
        return 0;
    }
    handles[handle_count] = (handle){(mc_chunk_state *)chunk, selected};
    return ++handle_count;
}

static mc_chunk_state *find_handle(const char *text)
{
    uint64_t id;
    if (!number(text, handle_count, &id) || id == 0 || handles[id - 1u].slot != selected)
        return NULL;
    return handles[id - 1u].chunk;
}

static bool register_tiles(void *world, const mc_chunk_acquire_context *context,
                           mc_chunk_state *chunk)
{
    slot *owner = world;
    if (context != &owner->context || !chunk->active || owner->callback_count >= 9)
        return false;
    owner->callbacks[owner->callback_count++] = (callback_event){chunk, 0, true};
    return true;
}

static bool register_entities(void *world, const mc_chunk_acquire_context *context,
                              mc_chunk_state *chunk, size_t slice)
{
    slot *owner = world;
    if (context != &owner->context || !chunk->active || slice >= 8 || owner->callback_count >= 9)
        return false;
    owner->callbacks[owner->callback_count++] = (callback_event){chunk, slice, false};
    return true;
}

static bool reset_cache(slot *owner)
{
    owner->callback_count = 0;
    owner->dimension_set = false;
    owner->fallback = (mc_chunk_state){.blank = true};
    owner->loader = (mc_world_store_loader){.store = owner->store};
    owner->context = (mc_chunk_acquire_context){.cache = &owner->cache,
                                                .pending = &owner->pending,
                                                .fallback = &owner->fallback,
                                                .world_time = &owner->world_time,
                                                .policy = MC_CHUNK_ACQUIRE_BASE,
                                                .world = owner,
                                                .loader = &owner->loader,
                                                .load = mc_world_store_load_callback,
                                                .register_tiles = register_tiles,
                                                .register_entities = register_entities};
    return mc_chunk_cache_init(&owner->cache, owner->entries, CACHE_LIMIT, owner->insertions,
                               LIST_LIMIT) &&
           mc_chunk_pending_init(&owner->pending, owner->keys, CACHE_LIMIT);
}

static void close_slot(size_t index)
{
    slot *owner = &slots[index];
    (void)reset_cache(owner);
    owner->draft = (mc_level_info){0};
    owner->has_draft = false;
    for (size_t i = 0; i < handle_count; ++i)
        if (handles[i].slot == index)
            handles[i].chunk = NULL;
    mc_world_store_free(owner->store);
    memset(owner, 0, sizeof(*owner));
}

static const mc_nbt_tag *field(const mc_nbt_tag *parent, const char *name)
{
    uint16_t units[16];
    size_t length = strlen(name);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)name[i];
    return mc_nbt_compound_get(parent, units, length);
}

static bool opaque(const mc_stored_chunk *owner)
{
    const mc_nbt_tag *level = field(mc_stored_chunk_root(owner), "Level");
    const mc_nbt_tag *entities = field(level, "Entities"), *tiles = field(level, "TileEntities");
    return (entities != NULL && entities->value.list.length != 0) ||
           (tiles != NULL && tiles->value.list.length != 0);
}

static void print_info(const mc_level_info *info)
{
    if (info == NULL) {
        emit("null");
        return;
    }
    format("{\"seed\":\"%016" PRIx64 "\",\"spawn\":[%" PRId32 ",%" PRId32 ",%" PRId32
           "],\"time\":\"%016" PRIx64 "\",\"last_played\":\"%016" PRIx64
           "\",\"size_on_disk\":\"%016" PRIx64 "\",\"level_name\":\"",
           (uint64_t)info->seed, info->spawn_x, info->spawn_y, info->spawn_z, (uint64_t)info->time,
           (uint64_t)info->last_played, (uint64_t)info->size_on_disk);
    for (size_t i = 0; i < info->level_name.length; ++i)
        format("%04x", (unsigned)info->level_name.units[i]);
    format("\",\"version\":%" PRId32 ",\"rain_time\":%" PRId32
           ",\"raining\":%s,\"thunder_time\":%" PRId32
           ",\"thundering\":%s,\"has_player\":%s,\"player_dimension\":%" PRId32 "}",
           info->version, info->rain_time, info->raining ? "true" : "false", info->thunder_time,
           info->thundering ? "true" : "false", info->player == NULL ? "false" : "true",
           info->player_dimension);
}

static void array(const char *name, const uint8_t *bytes, size_t length)
{
    format("\"%s\":{\"length\":%zu,\"values\":\"", name, length);
    hex(bytes, length);
    emit("\"}");
}

static void print_chunk(const mc_chunk_state *chunk)
{
    if (chunk == NULL) {
        emit("null");
        return;
    }
    format("{\"coordinates\":[%" PRId32 ",%" PRId32 "],\"blank\":%s,\"flags\":{\"c\":%s,\"n\":%s,"
           "\"o\":%s,\"p\":%s,\"q\":%s},\"last_save_time\":\"%016" PRIx64 "\",\"data\":",
           chunk->x, chunk->z, chunk->blank ? "true" : "false", chunk->active ? "true" : "false",
           chunk->terrain_populated ? "true" : "false", chunk->dirty ? "true" : "false",
           chunk->never_save ? "true" : "false", chunk->has_entities ? "true" : "false",
           (uint64_t)chunk->last_save_time);
    if (chunk->blank) {
        emit("null}");
        return;
    }
    const mc_chunk_data *data = chunk->data;
    emit("{");
    array("blocks", data->blocks, MC_CHUNK_BLOCKS);
    emit(",");
    array("metadata", data->metadata, sizeof(data->metadata));
    emit(",");
    array("skylight", data->skylight, sizeof(data->skylight));
    emit(",");
    array("block_light", data->block_light, sizeof(data->block_light));
    emit(",");
    array("heightmap", data->heightmap, sizeof(data->heightmap));
    format(",\"minimum_height\":%" PRId32 "}}", data->minimum_height);
}

static bool response(mc_world_regions *regions, mc_world_storage_status status,
                     const mc_world_storage_error *error, mc_chunk_state *chunk,
                     const uint32_t *growth, int preserved, int acquisition)
{
    if (status != error->status ||
        (status == MC_WORLD_STORAGE_OK
             ? error->operation != NULL || error->system_error != 0
             : error->operation == NULL || strlen(error->operation) > 256u))
        return false;
    mc_world_regions_usage usage = mc_world_regions_get_usage(regions);
    format("{\"status\":%u,\"error\":{\"status\":%u,\"operation\":", (unsigned)status,
           (unsigned)error->status);
    json_string(error->operation);
    format(",\"system_error\":%u},\"selected\":%zu,\"stores\":[", (unsigned)error->system_error,
           selected);
    for (size_t i = 0; i < 2; ++i)
        format("%s{\"open\":%s,\"chunks\":%zu}", i == 0 ? "" : ",",
               slots[i].store == NULL ? "false" : "true",
               mc_world_store_chunk_count(slots[i].store));
    format("],\"usage\":{\"count\":%zu,\"bytes\":%zu},\"info\":", usage.count, usage.bytes);
    print_info(mc_world_store_info(slots[selected].store));
    emit(",\"metadata_hex\":");
    const mc_nbt_tag *root = mc_world_store_metadata_root(slots[selected].store);
    if (root == NULL)
        emit("null");
    else {
        size_t length = 0;
        if (mc_nbt_measure(root, NULL, &length, NULL) != MC_NBT_OK || length > DATA_LIMIT)
            return false;
        uint8_t *bytes = malloc(length);
        if (bytes == NULL)
            return false;
        bool written = mc_nbt_write(root, NULL, bytes, length, &length, NULL) == MC_NBT_OK;
        if (written) {
            emit("\"");
            hex(bytes, length);
            emit("\"");
        }
        free(bytes);
        if (!written)
            return false;
    }
    emit(",\"handle\":");
    if (chunk == NULL)
        emit("null");
    else
        format("%zu", identity(chunk));
    emit(",\"chunk\":");
    print_chunk(chunk);
    emit(",\"growth\":");
    if (growth == NULL)
        emit("null");
    else
        format("%u", (unsigned)*growth);
    format(",\"outputs_preserved\":%s,\"acquire\":", preserved < 0 ? "null"
                                                     : preserved   ? "true"
                                                                   : "false");
    if (acquisition < 0)
        emit("null");
    else {
        slot *owner = &slots[selected];
        format("{\"status\":%u,\"loader_status\":%u,\"entries\":[", (unsigned)acquisition,
               (unsigned)owner->loader.last_error.status);
        for (size_t i = 0; i < owner->cache.count; ++i)
            format("%s[%" PRId32 ",%zu]", i == 0 ? "" : ",", owner->cache.entries[i].key,
                   identity(owner->cache.entries[i].chunk));
        emit("],\"insertions\":[");
        for (size_t i = 0; i < owner->cache.insertion_count; ++i)
            format("%s%zu", i == 0 ? "" : ",", identity(owner->cache.insertions[i]));
        emit("],\"pending\":[");
        for (size_t i = 0; i < owner->pending.count; ++i)
            format("%s%" PRId32, i == 0 ? "" : ",", owner->pending.keys[i]);
        emit("],\"callbacks\":[");
        for (size_t i = 0; i < owner->callback_count; ++i) {
            callback_event *event = &owner->callbacks[i];
            format("%s{\"kind\":\"%s\",\"handle\":%zu,\"active\":%s,\"slice\":", i == 0 ? "" : ",",
                   event->tiles ? "tiles" : "entities", identity(event->chunk),
                   event->chunk->active ? "true" : "false");
            if (event->tiles)
                emit("null}");
            else
                format("%zu}", event->slice);
        }
        emit("]}");
    }
    emit("}\n");
    return output_ok && fflush(stdout) == 0 && !ferror(stdout);
}

static bool edit_info(mc_level_info *info, const char *name, const char *value)
{
    if (strcmp(name, "seed") == 0)
        return long_bits(value, &info->seed);
    if (strcmp(name, "time") == 0)
        return long_bits(value, &info->time);
    if (strcmp(name, "last_played") == 0)
        return long_bits(value, &info->last_played);
    if (strcmp(name, "size_on_disk") == 0)
        return long_bits(value, &info->size_on_disk);
    if (strcmp(name, "version") == 0)
        return integer(value, &info->version);
    if (strcmp(name, "spawn_x") == 0)
        return integer(value, &info->spawn_x);
    if (strcmp(name, "spawn_y") == 0)
        return integer(value, &info->spawn_y);
    if (strcmp(name, "spawn_z") == 0)
        return integer(value, &info->spawn_z);
    return false;
}

static bool edit_chunk(mc_chunk_state *chunk, const char *name, const char *value)
{
    if (strcmp(name, "x") == 0)
        return integer(value, &chunk->x);
    if (strcmp(name, "z") == 0)
        return integer(value, &chunk->z);
    if (strcmp(name, "minimum_height") == 0)
        return integer(value, &chunk->data->minimum_height);
    if (strcmp(name, "r") == 0)
        return long_bits(value, &chunk->last_save_time);
    uint64_t bit;
    if (!number(value, 1, &bit))
        return false;
    bool *target = strcmp(name, "c") == 0   ? &chunk->active
                   : strcmp(name, "n") == 0 ? &chunk->terrain_populated
                   : strcmp(name, "o") == 0 ? &chunk->dirty
                   : strcmp(name, "p") == 0 ? &chunk->never_save
                   : strcmp(name, "q") == 0 ? &chunk->has_entities
                                            : NULL;
    if (target == NULL)
        return false;
    *target = bit != 0;
    return true;
}

int main(int argc, char **argv)
{
    (void)argv;
    if (argc != 1)
        return 2;
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1 ||
        _setmode(_fileno(stderr), _O_BINARY) == -1)
        return 1;
#endif
    char *line = malloc(LINE_LIMIT + 1u);
    mc_world_regions *regions = NULL;
    mc_world_storage_error error;
    if (line == NULL || mc_world_regions_create(NULL, &regions, &error) != MC_WORLD_STORAGE_OK) {
        free(line);
        return 1;
    }
    mc_world_store_limits limits = mc_world_store_default_limits();
    size_t total_input = 0, commands = 0;
    int result = 0;
    bool done = false;
    while (!done) {
        size_t length = 0;
        int byte;
        while ((byte = getchar()) != EOF && byte != '\n') {
            if (++total_input > INPUT_LIMIT || length == LINE_LIMIT || byte < 32 || byte >= 127) {
                result = 2;
                goto finish;
            }
            line[length++] = (char)byte;
        }
        if (byte == EOF) {
            result = ferror(stdin) ? 1 : length != 0 ? 2 : 0;
            break;
        }
        if (++total_input > INPUT_LIMIT || ++commands > COMMAND_LIMIT || length == 0) {
            result = 2;
            break;
        }
        line[length] = '\0';
        char *words[16], *start = line;
        size_t count = 0;
        for (char *cursor = line;; ++cursor) {
            if (*cursor != ' ' && *cursor != '\0')
                continue;
            char separator = *cursor;
            if (cursor == start || count == 16) {
                result = 2;
                goto finish;
            }
            *cursor = '\0';
            words[count++] = start;
            start = cursor + 1;
            if (separator == '\0')
                break;
        }
        slot *owner = &slots[selected];
        mc_world_storage_status status = MC_WORLD_STORAGE_OK;
        error = (mc_world_storage_error){status, NULL, 0};
        mc_chunk_state *chunk = NULL, marker = {0};
        uint32_t growth = UINT32_C(0xdeadbeef);
        const uint32_t *growth_result = NULL;
        int preserved = -1, acquisition = -1;
        if (strcmp(words[0], "OPEN") == 0 && count == 6) {
            uint64_t index, players;
            int64_t token;
            uint8_t *path = NULL, *map = NULL;
            size_t path_length = 0, map_length = 0;
            if (!number(words[1], 1, &index) || !long_bits(words[3], &token) ||
                !number(words[4], 1, &players) || !decode(words[2], 32768u, &path, &path_length) ||
                !decode(words[5], 256u, &map, &map_length) || map_length != 256u ||
                memchr(path, 0, path_length) != NULL) {
                free(path);
                free(map);
                result = 2;
                goto finish;
            }
            selected = (size_t)index;
            owner = &slots[selected];
            if (owner->store != NULL)
                status = failure(&error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "occupied store slot");
            else {
                mc_world_store_options options = {players != 0, token, map, map_length, limits};
                mc_world_store *opened = NULL;
                status =
                    mc_world_store_open((const char *)path, regions, &options, &opened, &error);
                preserved = status != MC_WORLD_STORAGE_OK;
                if ((preserved && opened != NULL) || (!preserved && opened == NULL))
                    output_ok = false;
                if (!preserved) {
                    owner->store = opened;
                    if (!reset_cache(owner))
                        output_ok = false;
                }
            }
            free(path);
            free(map);
        } else if (strcmp(words[0], "SELECT") == 0 && count == 2) {
            uint64_t index;
            if (!number(words[1], 1, &index)) {
                result = 2;
                goto finish;
            }
            selected = (size_t)index;
        } else if (strcmp(words[0], "CLOSE") == 0 && count == 1) {
            close_slot(selected);
        } else if (strcmp(words[0], "SESSION") == 0 && count == 1) {
            status = mc_world_store_check_session(owner->store, &error);
        } else if (strcmp(words[0], "LOAD_INFO") == 0 && count == 1) {
            status = mc_world_store_load_info(owner->store, &error);
            if (status == MC_WORLD_STORAGE_OK)
                owner->has_draft = false;
        } else if (strcmp(words[0], "INFO") == 0 && count == 1) {
        } else if (strcmp(words[0], "INFO_EDIT") == 0 && count == 3) {
            const mc_level_info *info = mc_world_store_info(owner->store);
            if (info == NULL)
                status = failure(&error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "missing live info");
            else {
                if (!owner->has_draft)
                    owner->draft = *info;
                if (!edit_info(&owner->draft, words[1], words[2])) {
                    result = 2;
                    goto finish;
                }
                owner->has_draft = true;
            }
        } else if (strcmp(words[0], "PLAYER_DROP") == 0 && count == 1) {
            const mc_level_info *info = mc_world_store_info(owner->store);
            if (info == NULL)
                status = failure(&error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "missing live info");
            else {
                if (!owner->has_draft)
                    owner->draft = *info;
                owner->draft.player = NULL;
                owner->has_draft = true;
            }
        } else if ((strcmp(words[0], "SAVE_INFO") == 0 && count == 2) ||
                   (strcmp(words[0], "INIT_INFO") == 0 && count == 3)) {
            int64_t clock;
            if (!long_bits(words[1], &clock)) {
                result = 2;
                goto finish;
            }
            mc_nbt_document *document = NULL;
            mc_level_info info;
            const mc_level_info *input =
                owner->has_draft ? &owner->draft : mc_world_store_info(owner->store);
            uint8_t *raw = NULL;
            size_t raw_length = 0;
            if (count == 3) {
                if (!decode(words[2], DATA_LIMIT, &raw, &raw_length)) {
                    result = 2;
                    goto finish;
                }
                size_t consumed = 0;
                mc_nbt_status nbt =
                    mc_nbt_read_compound(raw, raw_length, NULL, &document, &consumed, NULL);
                if (nbt != MC_NBT_OK || consumed != raw_length)
                    status = failure(&error, MC_WORLD_STORAGE_INVALID_DATA, "initializer NBT");
                else if (mc_level_decode(mc_nbt_document_root(document), &info, NULL) !=
                         MC_LEVEL_OK)
                    status =
                        failure(&error, MC_WORLD_STORAGE_INVALID_DATA, "initializer level schema");
                else
                    input = &info;
            }
            if (status == MC_WORLD_STORAGE_OK) {
                mc_level_info provided, unchanged;
                if (input != NULL) {
                    memcpy(&provided, input, sizeof(provided));
                    memcpy(&unchanged, &provided, sizeof(unchanged));
                    input = &provided;
                }
                status = mc_world_store_save_info(owner->store, input, clock, &error);
                if (input != NULL && memcmp(&provided, &unchanged, sizeof(provided)) != 0)
                    output_ok = false;
            }
            if (status == MC_WORLD_STORAGE_OK)
                owner->has_draft = false;
            free(raw);
            mc_nbt_document_free(document);
        } else if ((strcmp(words[0], "LOAD") == 0 && count == 4) ||
                   (strcmp(words[0], "RETAIN") == 0 && count == 3)) {
            int32_t dimension;
            if (!integer(words[1], &dimension)) {
                result = 2;
                goto finish;
            }
            mc_chunk_state *output = &marker;
            if (handle_count == HANDLE_LIMIT)
                status = failure(&error, MC_WORLD_STORAGE_LIMIT, "probe handles");
            else if (count == 4) {
                int32_t x, z;
                if (!integer(words[2], &x) || !integer(words[3], &z)) {
                    result = 2;
                    goto finish;
                }
                status = mc_world_store_load_chunk(owner->store, dimension, x, z, &output, &error);
            } else {
                uint8_t *raw = NULL;
                size_t raw_length = 0, consumed = 0;
                mc_stored_chunk *source = NULL;
                if (!decode(words[2], DATA_LIMIT, &raw, &raw_length)) {
                    result = 2;
                    goto finish;
                }
                mc_stored_chunk_status decoded =
                    mc_stored_chunk_decode(raw, raw_length, NULL, &source, &consumed, NULL);
                if (decoded != MC_STORED_CHUNK_OK || consumed != raw_length)
                    status = failure(&error, MC_WORLD_STORAGE_INVALID_DATA, "retain source chunk");
                else if (opaque(source))
                    status = failure(&error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                     "retain opaque records");
                else
                    status = mc_world_store_retain_chunk(
                        owner->store, dimension, mc_stored_chunk_state(source), &output, &error);
                mc_stored_chunk_free(source);
                free(raw);
            }
            preserved = status != MC_WORLD_STORAGE_OK;
            if (preserved ? output != &marker : output == &marker || output == NULL)
                output_ok = false;
            if (!preserved)
                chunk = output;
        } else if (strcmp(words[0], "CHUNK") == 0 && count == 2) {
            chunk = find_handle(words[1]);
            if (chunk == NULL)
                status = failure(&error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "chunk handle");
        } else if (strcmp(words[0], "EDIT") == 0 && count == 4) {
            chunk = find_handle(words[1]);
            if (chunk == NULL)
                status = failure(&error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "chunk handle");
            else if (!edit_chunk(chunk, words[2], words[3])) {
                result = 2;
                goto finish;
            }
        } else if (strcmp(words[0], "BYTE") == 0 && count == 5) {
            chunk = find_handle(words[1]);
            uint64_t index, value;
            if (!number(words[3], MC_CHUNK_BLOCKS, &index) || !number(words[4], 255, &value)) {
                result = 2;
                goto finish;
            }
            if (chunk == NULL)
                status = failure(&error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "chunk handle");
            else {
                size_t capacity = strcmp(words[2], "blocks") == 0      ? MC_CHUNK_BLOCKS
                                  : strcmp(words[2], "heightmap") == 0 ? MC_CHUNK_HEIGHTMAP_BYTES
                                                                       : MC_CHUNK_NIBBLE_BYTES;
                uint8_t *bytes = strcmp(words[2], "blocks") == 0        ? chunk->data->blocks
                                 : strcmp(words[2], "metadata") == 0    ? chunk->data->metadata
                                 : strcmp(words[2], "skylight") == 0    ? chunk->data->skylight
                                 : strcmp(words[2], "block_light") == 0 ? chunk->data->block_light
                                 : strcmp(words[2], "heightmap") == 0   ? chunk->data->heightmap
                                                                        : NULL;
                if (bytes == NULL || index >= capacity) {
                    result = 2;
                    goto finish;
                }
                bytes[index] = (uint8_t)value;
            }
        } else if (strcmp(words[0], "WRITE") == 0 && count == 4) {
            int64_t time;
            uint64_t timestamp;
            if (!long_bits(words[2], &time) || !number(words[3], UINT32_MAX, &timestamp)) {
                result = 2;
                goto finish;
            }
            chunk = find_handle(words[1]);
            status = mc_world_store_write_chunk(owner->store, chunk, time, (uint32_t)timestamp,
                                                &growth, &error);
            preserved = status != MC_WORLD_STORAGE_OK;
            if (preserved && growth != UINT32_C(0xdeadbeef))
                output_ok = false;
            if (!preserved)
                growth_result = &growth;
        } else if (strcmp(words[0], "CACHE_RESET") == 0 && count == 1) {
            if (!reset_cache(owner))
                output_ok = false;
        } else if (strcmp(words[0], "ACQUIRE") == 0 && count == 5) {
            int32_t dimension, x, z;
            int64_t time;
            if (!integer(words[1], &dimension) || !integer(words[2], &x) ||
                !integer(words[3], &z) || !long_bits(words[4], &time)) {
                result = 2;
                goto finish;
            }
            if (owner->store == NULL || !mc_chunk_acquire_validate(&owner->context))
                status = failure(&error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "acquisition store");
            else if (owner->dimension_set && owner->dimension != dimension)
                status = failure(&error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "reset cache before dimension change");
            else if (handle_count == HANDLE_LIMIT)
                status = failure(&error, MC_WORLD_STORAGE_LIMIT, "probe handles");
            else {
                owner->world_time = time;
                owner->dimension = dimension;
                owner->dimension_set = true;
                owner->loader.dimension = dimension;
                owner->loader.last_error = (mc_world_storage_error){0};
                owner->callback_count = 0;
                mc_chunk_state *output = &marker;
                mc_chunk_acquire_status acquired = mc_chunk_acquire(&owner->context, x, z, &output);
                acquisition = (int)acquired;
                if (acquired != MC_CHUNK_ACQUIRE_OK) {
                    if (owner->loader.last_error.status != MC_WORLD_STORAGE_OK)
                        error = owner->loader.last_error;
                    else
                        (void)failure(&error, MC_WORLD_STORAGE_LIMIT,
                                      mc_chunk_acquire_status_string(acquired));
                    status = error.status;
                }
                preserved = acquired != MC_CHUNK_ACQUIRE_OK;
                if (preserved ? output != &marker : output == &marker || output == NULL)
                    output_ok = false;
                if (!preserved)
                    chunk = output;
            }
        } else if (strcmp(words[0], "LIMITS") == 0 && count == 12) {
            uint64_t values[11];
            for (size_t i = 0; i < 11; ++i) {
                uint64_t limit = i == 0 ? 512u : i == 6 ? 128u : i == 10 ? 1024u : OUTPUT_LIMIT;
                if (!number(words[i + 1u], limit, &values[i])) {
                    result = 2;
                    goto finish;
                }
            }
            limits = (mc_world_store_limits){
                (size_t)values[0],
                {(size_t)values[1], (size_t)values[2], (size_t)values[3], (size_t)values[4],
                 (size_t)values[5], (size_t)values[6]},
                {(size_t)values[7], (size_t)values[8], (size_t)values[9], (size_t)values[10]}};
        } else if (strcmp(words[0], "CLEAR") == 0 && count == 1) {
            mc_world_regions_clear(regions);
        } else if (strcmp(words[0], "USAGE") == 0 && count == 1) {
        } else if (strcmp(words[0], "CHECK") == 0 && count == 1) {
            mc_world_store *sentinel = owner->store;
            uint32_t unchanged = UINT32_C(0xdeadbeef);
            if (mc_world_store_open(NULL, regions, NULL, &sentinel, &error) !=
                    MC_WORLD_STORAGE_INVALID_ARGUMENT ||
                sentinel != owner->store ||
                mc_world_store_write_chunk(NULL, NULL, 0, 0, &unchanged, &error) !=
                    MC_WORLD_STORAGE_INVALID_ARGUMENT ||
                unchanged != UINT32_C(0xdeadbeef) ||
                mc_world_store_load_chunk(NULL, 0, 0, 0, NULL, &error) !=
                    MC_WORLD_STORAGE_INVALID_ARGUMENT ||
                mc_world_store_save_info(NULL, NULL, 0, &error) !=
                    MC_WORLD_STORAGE_INVALID_ARGUMENT)
                output_ok = false;
            error = (mc_world_storage_error){0};
        } else if (strcmp(words[0], "QUIT") == 0 && count == 1) {
            done = true;
        } else {
            result = 2;
            goto finish;
        }
        if (!output_ok ||
            !response(regions, status, &error, chunk, growth_result, preserved, acquisition)) {
            result = 1;
            break;
        }
    }
    if (done && (getchar() != EOF || ferror(stdin)))
        result = 2;
finish:
    if (result == 1)
        fputs("world store probe contract or resource failure\n", stderr);
    close_slot(0);
    close_slot(1);
    mc_world_regions_free(regions);
    free(line);
    return result;
}
