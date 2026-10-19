#include "mc/world/runtime/runtime.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* Trusted copied-world adapter, not a game command interface. Input is ASCII,
 * tab separated, with UTF-8 paths and clocks encoded as lowercase hex. OPEN
 * creates no players directory and uses the modules' codec/image budgets with
 * 256 retained/reserved chunks. There is no generator, feature body or entity
 * implementation. ENTITY_HINT explicitly replays only the stale q scalar.
 *
 * PAYLOAD lines precede the corresponding RESPONSE. A response includes every
 * alive owned identity, even after unloading; only RETIRE or CLOSE removes it.
 * IDs increase for the whole process and never identify a reused allocation.
 * Every snapshot rereads all five arrays before reusing a payload version.
 * Metadata is encoded with the normal NBT writer, not normalized by this probe.
 * Command API failures retain their partial state and do not stop the stream.
 * Adapter parse/allocation/output/snapshot failures stop with a nonzero exit.
 */
#define COMMAND_LIMIT 4096u
#define LINE_LIMIT 4096u
#define INPUT_LIMIT (32u * 1024u * 1024u)
#define OUTPUT_LIMIT (128u * 1024u * 1024u)
#define HANDLE_LIMIT 256u
#define MAP_LIMIT 256u
#define LIST_LIMIT 512u
#define PENDING_LIMIT 256u
#define TIMESTAMP_LIMIT 256u
#define CALLBACK_LIMIT 16u
#define PAYLOAD_BYTES (MC_CHUNK_BLOCKS + 3u * MC_CHUNK_NIBBLE_BYTES + MC_CHUNK_HEIGHTMAP_BYTES)

typedef struct chunk_handle {
    mc_chunk_state *chunk;
    size_t id, version;
    uint8_t *payload;
} chunk_handle;

typedef struct callback_event {
    const char *kind;
    int stage;
    size_t ordinal;
} callback_event;

typedef struct probe {
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_runtime *runtime;
    mc_java_int_set *history;
    mc_chunk_cache cache;
    mc_chunk_cache_entry entries[MAP_LIMIT];
    mc_chunk_state *insertions[LIST_LIMIT];
    mc_chunk_pending pending;
    int32_t keys[PENDING_LIMIT];
    chunk_handle handles[HANDLE_LIMIT];
    size_t handle_count, next_id;
    int32_t dimension;
    mc_chunk_acquire_policy policy;
    uint8_t block_map[256];
    int64_t saved_at_ms;
    uint32_t timestamp, timestamps[TIMESTAMP_LIMIT];
    size_t timestamp_count, timestamp_index;
    callback_event callbacks[CALLBACK_LIMIT];
    size_t callback_count, auxiliary_count, provider_count, level_count, chunks_count;
    bool callback_ok;
} probe;

static probe owner;
static uint8_t payload_scratch[PAYLOAD_BYTES];
static size_t output_bytes;
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

static bool bits(const char *text, size_t digits, uint64_t *out)
{
    if (strlen(text) != digits)
        return false;
    uint64_t value = 0;
    for (size_t i = 0; i < digits; ++i) {
        int part = digit((unsigned char)text[i]);
        if (part < 0)
            return false;
        value = value * UINT64_C(16) + (unsigned)part;
    }
    *out = value;
    return true;
}

static int64_t signed_bits(uint64_t value)
{
    return value <= INT64_MAX ? (int64_t)value : -1 - (int64_t)(UINT64_MAX - value);
}

static bool decode(const char *text, uint8_t *out, size_t length)
{
    if (strlen(text) != length * 2u)
        return false;
    for (size_t i = 0; i < length; ++i) {
        int high = digit((unsigned char)text[i * 2u]);
        int low = digit((unsigned char)text[i * 2u + 1u]);
        if (high < 0 || low < 0)
            return false;
        out[i] = (uint8_t)((unsigned)high * 16u + (unsigned)low);
    }
    return true;
}

static bool utf8(const uint8_t *bytes, size_t length)
{
    for (size_t i = 0; i < length;) {
        unsigned first = bytes[i++];
        if (first < 128u)
            continue;
        size_t remaining;
        unsigned low = 128u, high = 191u;
        if (first >= 194u && first <= 223u)
            remaining = 1u;
        else if (first >= 224u && first <= 239u) {
            remaining = 2u;
            if (first == 224u)
                low = 160u;
            if (first == 237u)
                high = 159u;
        } else if (first >= 240u && first <= 244u) {
            remaining = 3u;
            if (first == 240u)
                low = 144u;
            if (first == 244u)
                high = 143u;
        } else
            return false;
        if (remaining > length - i || bytes[i] < low || bytes[i] > high)
            return false;
        ++i;
        for (size_t j = 1u; j < remaining; ++j, ++i) {
            if (bytes[i] < 128u || bytes[i] > 191u)
                return false;
        }
    }
    return true;
}

static mc_world_storage_status error_status(mc_world_storage_error *error,
                                            mc_world_storage_status status, const char *operation)
{
    *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static bool callback(const char *kind, int stage, size_t ordinal)
{
    if (owner.callback_count == CALLBACK_LIMIT) {
        owner.callback_ok = false;
        return false;
    }
    owner.callbacks[owner.callback_count++] = (callback_event){kind, stage, ordinal};
    return true;
}

static mc_chunk_callback_result
provider_maintenance(void *provider, const mc_chunk_lifecycle_context *context, bool *result)
{
    if (provider != &owner || context != mc_world_runtime_lifecycle(owner.runtime) ||
        result == NULL) {
        owner.callback_ok = false;
        return MC_CHUNK_CALLBACK_FATAL;
    }
    if (!callback("provider", -1, ++owner.provider_count))
        return MC_CHUNK_CALLBACK_FATAL;
    *result = false;
    return MC_CHUNK_CALLBACK_OK;
}

static mc_chunk_callback_result auxiliary(void *world, const mc_world_save_context *context)
{
    if (world != &owner || context != mc_world_runtime_saving(owner.runtime)) {
        owner.callback_ok = false;
        return MC_CHUNK_CALLBACK_FATAL;
    }
    return callback("auxiliary", -1, ++owner.auxiliary_count) ? MC_CHUNK_CALLBACK_OK
                                                              : MC_CHUNK_CALLBACK_FATAL;
}

static mc_chunk_callback_result progress(void *target, const mc_world_save_context *context,
                                         mc_world_save_stage stage)
{
    if (target != &owner || context != mc_world_runtime_saving(owner.runtime) ||
        (stage != MC_WORLD_SAVE_LEVEL && stage != MC_WORLD_SAVE_CHUNKS)) {
        owner.callback_ok = false;
        return MC_CHUNK_CALLBACK_FATAL;
    }
    size_t ordinal = stage == MC_WORLD_SAVE_LEVEL ? ++owner.level_count : ++owner.chunks_count;
    return callback("progress", (int)stage, ordinal) ? MC_CHUNK_CALLBACK_OK
                                                     : MC_CHUNK_CALLBACK_FATAL;
}

static uint32_t region_clock(void *clock)
{
    /* Only the external test-input cursor advances, without changing any
     * runtime/store binding or inferring which attempted write committed. */
    probe *state = clock;
    return state->timestamp_index < state->timestamp_count
               ? state->timestamps[state->timestamp_index++]
               : state->timestamp;
}

static int64_t metadata_clock(void *clock)
{
    const probe *state = clock;
    return state->saved_at_ms;
}

static chunk_handle *find_handle(size_t id)
{
    for (size_t i = 0; i < owner.handle_count; ++i) {
        if (owner.handles[i].id == id)
            return &owner.handles[i];
    }
    return NULL;
}

static bool identity(mc_chunk_state *chunk, size_t *id)
{
    const mc_chunk_acquire_context *context = mc_world_runtime_acquisition(owner.runtime);
    if (context == NULL || chunk == NULL)
        return false;
    if (chunk == context->fallback) {
        *id = 0;
        return chunk->blank;
    }
    for (size_t i = 0; i < owner.handle_count; ++i) {
        if (owner.handles[i].chunk == chunk) {
            *id = owner.handles[i].id;
            return true;
        }
    }
    if (!mc_world_store_owns_chunk(owner.store, chunk) || chunk->blank || chunk->data == NULL ||
        chunk->data->blocks == NULL || chunk->data->blocks_capacity < MC_CHUNK_BLOCKS ||
        owner.handle_count == HANDLE_LIMIT || owner.next_id == SIZE_MAX)
        return false;
    uint8_t *payload = malloc(PAYLOAD_BYTES);
    if (payload == NULL)
        return false;
    chunk_handle *handle = &owner.handles[owner.handle_count++];
    *handle = (chunk_handle){.chunk = chunk, .id = ++owner.next_id, .payload = payload};
    *id = handle->id;
    return true;
}

static bool discover(mc_chunk_state *returned)
{
    if (owner.runtime == NULL)
        return owner.store == NULL && owner.regions == NULL && owner.history == NULL &&
               owner.handle_count == 0;
    const mc_chunk_acquire_context *context = mc_world_runtime_acquisition(owner.runtime);
    if (context == NULL || context->cache != &owner.cache || context->pending != &owner.pending ||
        context->references == NULL || !mc_chunk_acquire_validate(context) ||
        !mc_chunk_lifecycle_validate(mc_world_runtime_lifecycle(owner.runtime)) ||
        !mc_world_save_validate(mc_world_runtime_saving(owner.runtime)) ||
        owner.pending.history != owner.history || mc_world_store_info(owner.store) == NULL ||
        mc_world_runtime_live(owner.runtime) == NULL)
        return false;
    size_t ignored;
    if (returned != NULL && !identity(returned, &ignored))
        return false;
    for (size_t i = 0; i < owner.cache.count; ++i) {
        if (!identity(owner.entries[i].chunk, &ignored))
            return false;
    }
    for (size_t i = 0; i < owner.cache.insertion_count; ++i) {
        if (!identity(owner.insertions[i], &ignored))
            return false;
    }
    if (mc_world_store_chunk_count(owner.store) != owner.handle_count)
        return false;
    for (size_t i = 0; i < owner.handle_count; ++i) {
        const mc_chunk_state *chunk = owner.handles[i].chunk;
        if (!mc_world_store_owns_chunk(owner.store, chunk) || chunk->blank || chunk->data == NULL ||
            chunk->data->blocks == NULL || chunk->data->blocks_capacity < MC_CHUNK_BLOCKS)
            return false;
    }
    return true;
}

static void forget_handles(void)
{
    for (size_t i = 0; i < owner.handle_count; ++i)
        free(owner.handles[i].payload);
    memset(owner.handles, 0, sizeof(owner.handles));
    owner.handle_count = 0;
}

static void free_storage(void)
{
    forget_handles();
    mc_world_store_free(owner.store);
    owner.store = NULL;
    mc_world_regions_free(owner.regions);
    owner.regions = NULL;
    mc_java_int_set_free(owner.history);
    owner.history = NULL;
    owner.cache = (mc_chunk_cache){0};
    owner.pending = (mc_chunk_pending){0};
    memset(owner.entries, 0, sizeof(owner.entries));
    memset(owner.insertions, 0, sizeof(owner.insertions));
    memset(owner.keys, 0, sizeof(owner.keys));
}

static bool cleanup(void)
{
    /* End-of-process teardown explicitly retires all adapter/cache views. It
     * is neither an unload command nor an implicit save/GC observation. */
    forget_handles();
    if (owner.runtime != NULL) {
        owner.cache.count = owner.cache.insertion_count = 0;
        memset(owner.entries, 0, sizeof(owner.entries));
        memset(owner.insertions, 0, sizeof(owner.insertions));
        if (mc_chunk_pending_clear(&owner.pending) != MC_CHUNK_ACQUIRE_OK)
            return false;
        memset(owner.keys, 0, sizeof(owner.keys));
        if (mc_world_runtime_free(owner.runtime, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        owner.runtime = NULL;
    }
    free_storage();
    return true;
}

static mc_world_storage_status open_world(const char *root, int64_t token, const uint8_t *map,
                                          int32_t dimension, mc_chunk_acquire_policy policy,
                                          mc_world_storage_error *error)
{
    if (owner.runtime != NULL)
        return error_status(error, MC_WORLD_STORAGE_BUSY, "probe already open");
    mc_world_storage_status status = mc_world_regions_create(NULL, &owner.regions, error);
    mc_world_store_options options = {.session_token = token,
                                      .block_map = map,
                                      .block_map_length = 256u,
                                      .limits = mc_world_store_default_limits()};
    options.limits.max_loaded_chunks = HANDLE_LIMIT;
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_world_store_open(root, owner.regions, &options, &owner.store, error);
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_world_store_load_info(owner.store, error);
    if (status == MC_WORLD_STORAGE_OK) {
        mc_java_int_set_limits limits = mc_java_int_set_default_limits();
        limits.max_entries = PENDING_LIMIT;
        mc_java_int_set_status created = mc_java_int_set_create(&limits, &owner.history);
        if (created != MC_JAVA_INT_SET_OK)
            status = error_status(error,
                                  created == MC_JAVA_INT_SET_ALLOCATION_FAILED
                                      ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                      : MC_WORLD_STORAGE_LIMIT,
                                  "probe pending history");
    }
    if (status == MC_WORLD_STORAGE_OK &&
        (!mc_chunk_cache_init(&owner.cache, owner.entries, MAP_LIMIT, owner.insertions,
                              LIST_LIMIT) ||
         !mc_chunk_pending_init_ordered(&owner.pending, owner.keys, PENDING_LIMIT, owner.history)))
        status = error_status(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "probe fixed storage");
    if (status == MC_WORLD_STORAGE_OK) {
        mc_world_runtime_options runtime = {.store = owner.store,
                                            .cache = &owner.cache,
                                            .pending = &owner.pending,
                                            .dimension = dimension,
                                            .policy = policy,
                                            .provider = &owner,
                                            .provider_maintenance = provider_maintenance,
                                            .auxiliary = &owner,
                                            .save_auxiliary = auxiliary,
                                            .progress = &owner,
                                            .report_progress = progress,
                                            .clock = &owner,
                                            .region_timestamp = region_clock,
                                            .metadata_timestamp = metadata_clock};
        status = mc_world_runtime_create(&runtime, &owner.runtime, error);
    }
    if (status != MC_WORLD_STORAGE_OK)
        free_storage();
    else {
        owner.dimension = dimension;
        owner.policy = policy;
        memcpy(owner.block_map, map, sizeof(owner.block_map));
    }
    return status;
}

static void print_error(const mc_world_storage_error *error)
{
    if (error == NULL) {
        emit("null");
        return;
    }
    format("{\"status\":%u,\"operation\":", (unsigned)error->status);
    json_string(error->operation);
    format(",\"system_error\":%" PRIu32 "}", error->system_error);
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

static void print_chunk(const mc_chunk_state *chunk, size_t id, size_t version)
{
    format("{\"id\":%zu,\"coordinates\":[%" PRId32 ",%" PRId32
           "],\"blank\":%s,\"flags\":{\"c\":%s,\"n\":%s,\"o\":%s,\"p\":%s,\"q\":%s},"
           "\"last_save_time\":\"%016" PRIx64 "\",\"minimum_height\":",
           id, chunk->x, chunk->z, chunk->blank ? "true" : "false",
           chunk->active ? "true" : "false", chunk->terrain_populated ? "true" : "false",
           chunk->dirty ? "true" : "false", chunk->never_save ? "true" : "false",
           chunk->has_entities ? "true" : "false", (uint64_t)chunk->last_save_time);
    if (chunk->blank)
        emit("null,\"payload_version\":null}");
    else
        format("%" PRId32 ",\"payload_version\":%zu}", chunk->data->minimum_height, version);
}

static bool print_payloads(void)
{
    static const char *names[] = {"blocks", "metadata", "skylight", "block_light", "heightmap"};
    const size_t lengths[] = {MC_CHUNK_BLOCKS, MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_NIBBLE_BYTES,
                              MC_CHUNK_NIBBLE_BYTES, MC_CHUNK_HEIGHTMAP_BYTES};
    for (size_t i = 0; i < owner.handle_count; ++i) {
        chunk_handle *handle = &owner.handles[i];
        const mc_chunk_data *data = handle->chunk->data;
        const uint8_t *arrays[] = {data->blocks, data->metadata, data->skylight, data->block_light,
                                   data->heightmap};
        size_t offset = 0;
        for (size_t array = 0; array < 5u; ++array) {
            memcpy(payload_scratch + offset, arrays[array], lengths[array]);
            offset += lengths[array];
        }
        if (handle->version != 0u && memcmp(payload_scratch, handle->payload, PAYLOAD_BYTES) == 0)
            continue;
        if (handle->version == SIZE_MAX)
            return false;
        memcpy(handle->payload, payload_scratch, PAYLOAD_BYTES);
        ++handle->version;
        format("{\"kind\":\"PAYLOAD\",\"id\":%zu,\"version\":%zu", handle->id, handle->version);
        offset = 0;
        for (size_t array = 0; array < 5u; ++array) {
            format(",\"%s\":\"", names[array]);
            hex(handle->payload + offset, lengths[array]);
            emit("\"");
            offset += lengths[array];
        }
        emit("}\n");
    }
    return output_ok;
}

static bool response(size_t index, const char *command, const char *category, unsigned status,
                     const mc_world_storage_error *error, int result, int completed,
                     mc_chunk_state *returned)
{
    if (!owner.callback_ok || !discover(returned))
        return false;
    const mc_nbt_tag *root = mc_world_store_metadata_root(owner.store);
    mc_level_info live_info;
    if (owner.runtime != NULL &&
        mc_world_runtime_read_info(owner.runtime, &live_info, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    uint8_t *metadata = NULL;
    size_t metadata_length = 0;
    if (root != NULL) {
        if (mc_nbt_measure(root, NULL, &metadata_length, NULL) != MC_NBT_OK)
            return false;
        metadata = malloc(metadata_length);
        if (metadata == NULL)
            return false;
        if (mc_nbt_write(root, NULL, metadata, metadata_length, &metadata_length, NULL) !=
            MC_NBT_OK) {
            free(metadata);
            return false;
        }
    }
    int32_t order[PENDING_LIMIT];
    size_t count = 0, returned_id = 0;
    if ((owner.history != NULL && mc_java_int_set_copy_order(owner.history, order, PENDING_LIMIT,
                                                             &count) != MC_JAVA_INT_SET_OK) ||
        count != owner.pending.count || (returned != NULL && !identity(returned, &returned_id)) ||
        !print_payloads()) {
        free(metadata);
        return false;
    }
    format("{\"kind\":\"RESPONSE\",\"schema\":1,\"command_index\":%zu,\"command\":", index);
    json_string(command);
    emit(",\"category\":");
    json_string(category);
    format(",\"status\":%u,\"result\":%s,\"completed\":%s,\"returned_id\":", status,
           result < 0    ? "null"
           : result != 0 ? "true"
                         : "false",
           completed < 0    ? "null"
           : completed != 0 ? "true"
                            : "false");
    if (returned == NULL)
        emit("null");
    else
        format("%zu", returned_id);
    emit(",\"error\":");
    print_error(error);
    emit(",\"callbacks\":[");
    for (size_t i = 0; i < owner.callback_count; ++i) {
        const callback_event *event = &owner.callbacks[i];
        format("%s{\"kind\":", i == 0 ? "" : ",");
        json_string(event->kind);
        emit(",\"stage\":");
        if (event->stage < 0)
            emit("null");
        else
            format("%d", event->stage);
        format(",\"ordinal\":%zu}", event->ordinal);
    }
    format("],\"state\":{\"open\":%s,\"dimension\":", owner.runtime == NULL ? "false" : "true");
    if (owner.runtime == NULL)
        emit("null,\"policy\":null");
    else
        format("%" PRId32 ",\"policy\":\"%s\"", owner.dimension,
               owner.policy == MC_CHUNK_ACQUIRE_BASE ? "base" : "dedicated");
    format(",\"clocks\":{\"saved_at_ms\":\"%016" PRIx64 "\",\"timestamp\":\"%08" PRIx32
           "\",\"timestamp_count\":%zu,\"timestamp_index\":%zu,\"timestamp_queue\":[",
           (uint64_t)owner.saved_at_ms, owner.timestamp, owner.timestamp_count,
           owner.timestamp_index);
    for (size_t i = 0; i < owner.timestamp_count; ++i)
        format("%s\"%08" PRIx32 "\"", i == 0 ? "" : ",", owner.timestamps[i]);
    emit("]},\"live\":");
    const mc_world_runtime_state *live = mc_world_runtime_live(owner.runtime);
    if (live == NULL)
        emit("null");
    else
        format("{\"time\":\"%016" PRIx64 "\",\"spawn\":[%" PRId32 ",%" PRId32 ",%" PRId32
               "],\"world_force_load\":%s,\"provider_force_load\":%s,\"saving_disabled\":%s}",
               (uint64_t)*live->time, *live->spawn_x, *live->spawn_y, *live->spawn_z,
               *live->world_force_load ? "true" : "false",
               *live->provider_force_load ? "true" : "false",
               *live->saving_disabled ? "true" : "false");
    emit(",\"info\":");
    print_info(owner.runtime == NULL ? NULL : &live_info);
    emit(",\"retained_info\":");
    print_info(mc_world_store_info(owner.store));
    emit(",\"metadata_hex\":");
    if (metadata == NULL)
        emit("null");
    else {
        emit("\"");
        hex(metadata, metadata_length);
        emit("\"");
    }
    free(metadata);
    format(",\"owned_count\":%zu,\"reservation_count\":%zu,\"chunks\":[",
           mc_world_store_chunk_count(owner.store), mc_world_store_reservation_count(owner.store));
    for (size_t i = 0; i < owner.handle_count; ++i) {
        const chunk_handle *handle = &owner.handles[i];
        if (i != 0)
            emit(",");
        print_chunk(handle->chunk, handle->id, handle->version);
    }
    emit("],\"fallback\":");
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(owner.runtime);
    if (acquisition == NULL)
        emit("null");
    else
        print_chunk(acquisition->fallback, 0, 0);
    emit(",\"map\":[");
    for (size_t i = 0; i < owner.cache.count; ++i) {
        size_t id;
        if (!identity(owner.entries[i].chunk, &id))
            return false;
        format("%s[%" PRId32 ",%zu]", i == 0 ? "" : ",", owner.entries[i].key, id);
    }
    emit("],\"list\":[");
    for (size_t i = 0; i < owner.cache.insertion_count; ++i) {
        size_t id;
        if (!identity(owner.insertions[i], &id))
            return false;
        format("%s%zu", i == 0 ? "" : ",", id);
    }
    emit("],\"pending\":[");
    for (size_t i = 0; i < count; ++i)
        format("%s%" PRId32, i == 0 ? "" : ",", order[i]);
    emit("],\"pending_membership\":[");
    for (size_t i = 0; i < owner.pending.count; ++i)
        format("%s%" PRId32, i == 0 ? "" : ",", owner.keys[i]);
    format(
        "],\"callback_counts\":{\"auxiliary\":%zu,\"provider\":%zu,\"level\":%zu,\"chunks\":%zu},"
        "\"last_error\":",
        owner.auxiliary_count, owner.provider_count, owner.level_count, owner.chunks_count);
    print_error(mc_world_runtime_last_error(owner.runtime));
    mc_world_regions_usage usage = mc_world_regions_get_usage(owner.regions);
    format(",\"regions\":{\"count\":%zu,\"bytes\":%zu}}}\n", usage.count, usage.bytes);
    return output_ok && fflush(stdout) == 0 && !ferror(stdout);
}

int main(void)
{
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1)
        return 1;
#endif
    owner.callback_ok = true;
    char line[LINE_LIMIT + 1u];
    size_t commands = 0, total_input = 0;
    int exit_status = 0;
    for (;;) {
        size_t length = 0;
        int byte;
        while ((byte = getchar()) != EOF && byte != '\n') {
            if (++total_input > INPUT_LIMIT || length == LINE_LIMIT ||
                (byte != '\t' && (byte < 32 || byte >= 127))) {
                exit_status = 2;
                goto finish;
            }
            line[length++] = (char)byte;
        }
        if (byte == EOF) {
            exit_status = ferror(stdin) ? 1 : length != 0 || commands == 0 ? 2 : 0;
            break;
        }
        if (++total_input > INPUT_LIMIT || ++commands > COMMAND_LIMIT || length == 0) {
            exit_status = 2;
            break;
        }
        line[length] = '\0';
        char *words[TIMESTAMP_LIMIT + 2u], *start = line;
        size_t count = 0;
        for (char *cursor = line;; ++cursor) {
            if (*cursor != '\t' && *cursor != '\0')
                continue;
            char separator = *cursor;
            if (cursor == start || count == sizeof(words) / sizeof(words[0])) {
                exit_status = 2;
                goto finish;
            }
            *cursor = '\0';
            words[count++] = start;
            start = cursor + 1;
            if (separator == '\0')
                break;
        }
        owner.callback_count = 0;
        const char *category = "adapter";
        unsigned status = MC_WORLD_STORAGE_OK;
        mc_world_storage_error error = {0};
        mc_chunk_state *returned = NULL;
        int result = -1, completed = -1;
        if (strcmp(words[0], "OPEN") == 0 && count == 6u) {
            size_t path_length = strlen(words[1]) / 2u;
            uint8_t path[LINE_LIMIT / 2u + 1u], map[256];
            uint64_t token;
            int32_t dimension;
            if (path_length == 0 || !decode(words[1], path, path_length) ||
                memchr(path, 0, path_length) != NULL || !utf8(path, path_length) ||
                !bits(words[2], 16u, &token) || !decode(words[3], map, sizeof(map)) ||
                !integer(words[4], &dimension) ||
                (strcmp(words[5], "base") != 0 && strcmp(words[5], "dedicated") != 0)) {
                exit_status = 2;
                goto finish;
            }
            path[path_length] = 0;
            category = "storage";
            status = (unsigned)open_world(
                (const char *)path, signed_bits(token), map, dimension,
                strcmp(words[5], "base") == 0 ? MC_CHUNK_ACQUIRE_BASE : MC_CHUNK_ACQUIRE_DEDICATED,
                &error);
        } else if (strcmp(words[0], "COMPETE") == 0 && count == 2u) {
            uint64_t token;
            if (!bits(words[1], 16u, &token)) {
                exit_status = 2;
                goto finish;
            }
            category = "storage";
            if (owner.store == NULL)
                status = (unsigned)error_status(&error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                                "probe runtime is closed");
            else {
                /* Explicit filesystem fixture setup through the real handler
                 * constructor. The active runtime keeps its original token. */
                mc_world_store_options options = {.session_token = signed_bits(token),
                                                  .block_map = owner.block_map,
                                                  .block_map_length = sizeof(owner.block_map),
                                                  .limits = mc_world_store_default_limits()};
                options.limits.max_loaded_chunks = HANDLE_LIMIT;
                mc_world_store *competing = NULL;
                status = (unsigned)mc_world_store_open(mc_world_store_root(owner.store),
                                                       owner.regions, &options, &competing, &error);
                mc_world_store_free(competing);
            }
        } else if (strcmp(words[0], "CLOCKS") == 0 && count == 3u) {
            uint64_t saved_at, timestamp;
            if (!bits(words[1], 16u, &saved_at) || !bits(words[2], 8u, &timestamp)) {
                exit_status = 2;
                goto finish;
            }
            owner.saved_at_ms = signed_bits(saved_at);
            owner.timestamp = (uint32_t)timestamp;
        } else if (strcmp(words[0], "TIMESTAMPS") == 0 && count >= 2u) {
            uint64_t requested;
            uint32_t values[TIMESTAMP_LIMIT];
            if (!number(words[1], TIMESTAMP_LIMIT, &requested) || count != (size_t)requested + 2u) {
                exit_status = 2;
                goto finish;
            }
            for (size_t i = 0; i < (size_t)requested; ++i) {
                uint64_t value;
                if (!bits(words[i + 2u], 8u, &value)) {
                    exit_status = 2;
                    goto finish;
                }
                values[i] = (uint32_t)value;
            }
            memcpy(owner.timestamps, values, (size_t)requested * sizeof(values[0]));
            owner.timestamp_count = (size_t)requested;
            owner.timestamp_index = 0;
        } else if (strcmp(words[0], "CLOSE") == 0 && count == 1u) {
            category = "storage";
            status = (unsigned)mc_world_runtime_free(owner.runtime, &error);
            if (status == MC_WORLD_STORAGE_OK) {
                owner.runtime = NULL;
                free_storage();
            }
        } else if ((strcmp(words[0], "ACQUIRE") == 0 || strcmp(words[0], "GET") == 0 ||
                    strcmp(words[0], "DROP") == 0) &&
                   count == 3u) {
            int32_t x, z;
            if (!integer(words[1], &x) || !integer(words[2], &z)) {
                exit_status = 2;
                goto finish;
            }
            category = "acquire";
            mc_chunk_state marker = {0}, *output = &marker;
            mc_chunk_acquire_status acquired;
            if (strcmp(words[0], "DROP") == 0)
                acquired = mc_world_runtime_mark_drop(owner.runtime, x, z);
            else if (strcmp(words[0], "ACQUIRE") == 0)
                acquired = mc_world_runtime_acquire(owner.runtime, x, z, &output);
            else
                acquired = mc_world_runtime_get(owner.runtime, x, z, &output);
            status = (unsigned)acquired;
            if (strcmp(words[0], "DROP") != 0) {
                if (acquired == MC_CHUNK_ACQUIRE_OK) {
                    if (output == NULL || output == &marker) {
                        exit_status = 1;
                        goto finish;
                    }
                    returned = output;
                } else if (output != &marker) {
                    exit_status = 1;
                    goto finish;
                }
            }
            const mc_world_storage_error *last = mc_world_runtime_last_error(owner.runtime);
            if (last != NULL)
                error = *last;
        } else if (strcmp(words[0], "TIME") == 0 && count == 2u) {
            uint64_t value;
            if (!bits(words[1], 16u, &value)) {
                exit_status = 2;
                goto finish;
            }
            const mc_world_runtime_state *live = mc_world_runtime_live(owner.runtime);
            if (live == NULL)
                status = (unsigned)error_status(&error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                                "probe runtime is closed");
            else
                *live->time = signed_bits(value);
        } else if (strcmp(words[0], "ELIGIBLE") == 0 && count == 3u) {
            uint64_t id, force;
            if (!number(words[1], SIZE_MAX, &id) || !number(words[2], 1u, &force)) {
                exit_status = 2;
                goto finish;
            }
            chunk_handle *handle = find_handle((size_t)id);
            const mc_chunk_acquire_context *context = mc_world_runtime_acquisition(owner.runtime);
            mc_chunk_state *chunk = id == 0 && context != NULL ? context->fallback
                                    : handle != NULL           ? handle->chunk
                                                               : NULL;
            if (chunk == NULL)
                status = (unsigned)error_status(&error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                                "probe unknown eligibility identity");
            else
                result = mc_chunk_save_eligible(
                             chunk, (*mc_world_runtime_live(owner.runtime)->time), force != 0)
                             ? 1
                             : 0;
        } else if ((strcmp(words[0], "DIRTY") == 0 && count == 2u) ||
                   (strcmp(words[0], "ENTITY_HINT") == 0 && count == 3u) ||
                   (strcmp(words[0], "RETIRE") == 0 && count == 2u)) {
            uint64_t id, hint = 0;
            if (!number(words[1], SIZE_MAX, &id) || (count == 3u && !number(words[2], 1u, &hint))) {
                exit_status = 2;
                goto finish;
            }
            chunk_handle *handle = find_handle((size_t)id);
            if (strcmp(words[0], "RETIRE") == 0)
                category = "storage";
            if (handle == NULL)
                status = (unsigned)error_status(&error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                                "probe unknown owned identity");
            else if (strcmp(words[0], "DIRTY") == 0)
                handle->chunk->dirty = true;
            else if (strcmp(words[0], "ENTITY_HINT") == 0)
                handle->chunk->has_entities = hint != 0;
            else {
                status = (unsigned)mc_world_store_retire_chunk(owner.store, handle->chunk, &error);
                if (status == MC_WORLD_STORAGE_OK) {
                    size_t index = (size_t)(handle - owner.handles);
                    free(handle->payload);
                    memmove(owner.handles + index, owner.handles + index + 1u,
                            (owner.handle_count - index - 1u) * sizeof(owner.handles[0]));
                    --owner.handle_count;
                    owner.handles[owner.handle_count] = (chunk_handle){0};
                }
            }
        } else if (((strcmp(words[0], "SAVE") == 0 || strcmp(words[0], "WORLD_SAVE") == 0) &&
                    count == 2u) ||
                   (strcmp(words[0], "CLIENT_SAVE") == 0 && count == 2u) ||
                   (strcmp(words[0], "MAINTAIN") == 0 && count == 1u)) {
            bool output = true;
            mc_chunk_lifecycle_status saved;
            if (strcmp(words[0], "MAINTAIN") == 0) {
                saved = mc_world_runtime_maintain(owner.runtime, &output);
                if (saved == MC_CHUNK_LIFECYCLE_OK)
                    result = output ? 1 : 0;
            } else if (strcmp(words[0], "CLIENT_SAVE") == 0) {
                int32_t pass;
                if (!integer(words[1], &pass)) {
                    exit_status = 2;
                    goto finish;
                }
                saved = mc_world_runtime_save_client(owner.runtime, pass, &output);
                if (saved == MC_CHUNK_LIFECYCLE_OK)
                    completed = output ? 1 : 0;
            } else {
                uint64_t force;
                if (!number(words[1], 1u, &force)) {
                    exit_status = 2;
                    goto finish;
                }
                if (strcmp(words[0], "WORLD_SAVE") == 0)
                    saved = mc_world_runtime_save(owner.runtime, force != 0);
                else {
                    saved = mc_world_runtime_save_chunks(owner.runtime, force != 0, &output);
                    if (saved == MC_CHUNK_LIFECYCLE_OK)
                        completed = output ? 1 : 0;
                }
            }
            if (saved != MC_CHUNK_LIFECYCLE_OK && !output) {
                exit_status = 1;
                goto finish;
            }
            category = "lifecycle";
            status = (unsigned)saved;
            const mc_world_storage_error *last = mc_world_runtime_last_error(owner.runtime);
            if (last != NULL)
                error = *last;
        } else {
            exit_status = 2;
            goto finish;
        }
        if (!response(commands, words[0], category, status, &error, result, completed, returned)) {
            exit_status = 1;
            break;
        }
    }
finish:
    if (exit_status != 0)
        fputs("lifecycle probe parse, resource or snapshot failure\n", stderr);
    if (!cleanup()) {
        fputs("lifecycle probe teardown failure\n", stderr);
        exit_status = 1;
    }
    return exit_status;
}
