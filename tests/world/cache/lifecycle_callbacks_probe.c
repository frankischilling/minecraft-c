#include "mc/world/cache/lifecycle.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* Trusted external callback-boundary adapter, not an entity implementation.
 * CHUNK imports one complete external decoder result. The real acquisition and
 * lifecycle modules perform stamping, publication, activation and detachment.
 * Registration callbacks record their dispatch without simulating Boat, Sign,
 * world registries, deferred ownership or serializer bodies. SET MAIN_Q is an
 * explicit external scalar effect, not a C serialization/eligibility oracle.
 * There is no filesystem, generator, population body or implicit save here.
 *
 * ASCII/tab commands, one per LF-terminated line (CRLF also accepted):
 * OPEN base|dedicated time16 spawnX spawnZ worldForce providerForce saveOff
 *      blankX blankZ blank_cnopq5bits blank_last16
 * CHUNK x z cnopq5bits last16 minimum blocks metadata skylight block_light heightmap
 * TIME time16; SET MAIN_Q keep|0|1; ACQUIRE x z; DETACH; DROP x z;
 * MAINTAIN [time16]; QUIT. Hex is lowercase; arrays have their fixed full sizes.
 * OPEN and CHUNK each occur once. CHUNK precedes coordinator operations and
 * QUIT. QUIT's terminating LF must be followed by exact EOF, without padding. The
 * imported owner is ID1 and the virtual blank is ID0. A load returns ID1 once,
 * then reports missing; no new identity is manufactured. Missing generation
 * uses the coordinator's real fallback policy. Caller callbacks all return OK;
 * the maintenance provider returns false. API errors retain partial snapshots.
 * Parse, journal, snapshot and stream failures terminate with a nonzero status.
 *
 * Every journal boundary copies all five arrays, headers and the live graph;
 * unchanged bytes are never inferred from dirty flags. Static snapshot storage
 * avoids large Windows stack frames. All borrowed owners remain live to exit.
 */
#define MAP_LIMIT 16u
#define LIST_LIMIT 32u
#define PENDING_LIMIT 16u
#define EVENT_LIMIT 64u
#define COMMAND_LIMIT 256u
#define LINE_LIMIT (2u * 1024u * 1024u)
#define INPUT_LIMIT (32u * 1024u * 1024u)
#define OUTPUT_LIMIT (128u * 1024u * 1024u)
#define FIELD_LIMIT 16u

typedef struct payload {
    uint8_t blocks[MC_CHUNK_BLOCKS];
    uint8_t metadata[MC_CHUNK_NIBBLE_BYTES];
    uint8_t skylight[MC_CHUNK_NIBBLE_BYTES];
    uint8_t block_light[MC_CHUNK_NIBBLE_BYTES];
    uint8_t heightmap[MC_CHUNK_HEIGHTMAP_BYTES];
} payload;

typedef struct graph_snapshot {
    mc_chunk_state normal, fallback;
    payload bytes;
    int32_t minimum, spawn_x, spawn_z;
    int main_q;
    int64_t time;
    bool imported, load_available, world_force, provider_force, saving_disabled, dedicated;
    size_t map_count, list_count, pending_count, normal_leases, blank_leases, retains, releases;
    int32_t map_keys[MAP_LIMIT], order[PENDING_LIMIT], membership[PENDING_LIMIT];
    size_t map_values[MAP_LIMIT], list[LIST_LIMIT];
} graph_snapshot;

typedef struct callback_event {
    const char *kind, *edge, *outcome;
    int slice, selected;
    bool owner_matched, context_matched, has_request;
    int32_t request_x, request_z;
    graph_snapshot state;
} callback_event;

static mc_chunk_cache cache;
static mc_chunk_pending pending;
static mc_java_int_set *history;
static mc_chunk_cache_entry entries[MAP_LIMIT];
static mc_chunk_state *insertions[LIST_LIMIT];
static int32_t pending_keys[PENDING_LIMIT];
static mc_chunk_data data;
static uint8_t blocks[MC_CHUNK_BLOCKS];
static mc_chunk_state normal, fallback;
static mc_chunk_acquire_context acquisition;
static mc_chunk_lifecycle_context lifecycle;
static mc_chunk_references references;
static callback_event events[EVENT_LIMIT];
static graph_snapshot final_state;
static int world_owner, loader_owner, provider_owner, reference_owner;
static int64_t world_time;
static int32_t spawn_x, spawn_z;
static bool world_force, provider_force, saving_disabled, opened, imported, load_available;
static bool adapter_failed, output_ok = true;
static int main_q = -1;
static size_t event_count, normal_leases, blank_leases, retains, releases, output_bytes;

static void emit(const char *text) {
    size_t count = strlen(text);
    if (!output_ok || count > OUTPUT_LIMIT - output_bytes) {
        output_ok = false;
        return;
    }
    output_bytes += count;
    if (fwrite(text, 1, count, stdout) != count)
        output_ok = false;
}

static void format(const char *pattern, ...) {
    char text[512];
    va_list arguments;
    va_start(arguments, pattern);
    int count = vsnprintf(text, sizeof(text), pattern, arguments);
    va_end(arguments);
    if (count < 0 || (size_t)count >= sizeof(text))
        output_ok = false;
    else
        emit(text);
}

static const char *boolean(bool value) {
    return value ? "true" : "false";
}

static void hex(const uint8_t *bytes, size_t count) {
    static const char digits[] = "0123456789abcdef";
    char text[257];
    for (size_t offset = 0; offset < count;) {
        size_t length = count - offset < 128u ? count - offset : 128u;
        for (size_t i = 0; i < length; ++i) {
            text[i * 2u] = digits[bytes[offset + i] >> 4u];
            text[i * 2u + 1u] = digits[bytes[offset + i] & 15u];
        }
        text[length * 2u] = '\0';
        emit(text);
        offset += length;
    }
}

static int identity(const mc_chunk_state *chunk) {
    return chunk == &normal ? 1 : chunk == &fallback ? 0 : -1;
}

static bool snapshot(graph_snapshot *out) {
    if (!mc_chunk_acquire_validate(&acquisition) || !mc_chunk_lifecycle_validate(&lifecycle) ||
        cache.count > MAP_LIMIT || cache.insertion_count > LIST_LIMIT ||
        pending.count > PENDING_LIMIT || !fallback.blank || fallback.data != NULL ||
        (imported && (normal.blank || normal.data != &data || data.blocks != blocks ||
                      data.blocks_capacity != sizeof(blocks))))
        return false;
    out->normal = normal;
    out->fallback = fallback;
    out->minimum = imported ? data.minimum_height : 0;
    out->time = world_time;
    out->spawn_x = spawn_x;
    out->spawn_z = spawn_z;
    out->world_force = world_force;
    out->provider_force = provider_force;
    out->saving_disabled = saving_disabled;
    out->dedicated = acquisition.policy == MC_CHUNK_ACQUIRE_DEDICATED;
    out->main_q = main_q;
    out->imported = imported;
    out->load_available = load_available;
    out->normal_leases = normal_leases;
    out->blank_leases = blank_leases;
    out->retains = retains;
    out->releases = releases;
    out->map_count = cache.count;
    out->list_count = cache.insertion_count;
    out->pending_count = pending.count;
    if (imported) {
        memcpy(out->bytes.blocks, blocks, sizeof(blocks));
        memcpy(out->bytes.metadata, data.metadata, sizeof(data.metadata));
        memcpy(out->bytes.skylight, data.skylight, sizeof(data.skylight));
        memcpy(out->bytes.block_light, data.block_light, sizeof(data.block_light));
        memcpy(out->bytes.heightmap, data.heightmap, sizeof(data.heightmap));
    }
    for (size_t i = 0; i < cache.count; ++i) {
        int value = identity(cache.entries[i].chunk);
        if (value < 0)
            return false;
        out->map_keys[i] = cache.entries[i].key;
        out->map_values[i] = (size_t)value;
    }
    for (size_t i = 0; i < cache.insertion_count; ++i) {
        int value = identity(cache.insertions[i]);
        if (value < 0)
            return false;
        out->list[i] = (size_t)value;
    }
    size_t count = 0;
    if (mc_java_int_set_copy_order(history, out->order, PENDING_LIMIT, &count) !=
            MC_JAVA_INT_SET_OK ||
        count != pending.count)
        return false;
    memcpy(out->membership, pending.keys, pending.count * sizeof(*pending.keys));
    return true;
}

static bool journal(const char *kind, const char *edge, const char *outcome, int slice,
                    mc_chunk_state *selected, bool owner_matched, bool context_matched,
                    bool has_request, int32_t x, int32_t z) {
    if (event_count == EVENT_LIMIT) {
        adapter_failed = true;
        return false;
    }
    callback_event *event = &events[event_count];
    event->kind = kind;
    event->edge = edge;
    event->outcome = outcome;
    event->slice = slice;
    event->selected = identity(selected);
    event->owner_matched = owner_matched;
    event->context_matched = context_matched;
    event->has_request = has_request;
    event->request_x = x;
    event->request_z = z;
    if (!owner_matched || !context_matched || (selected != NULL && event->selected < 0) ||
        !snapshot(&event->state)) {
        adapter_failed = true;
        return false;
    }
    ++event_count;
    return true;
}

static bool retain(void *owner, mc_chunk_state *chunk) {
    int id = identity(chunk);
    size_t *depth = id == 1 ? &normal_leases : &blank_leases;
    if (owner != &reference_owner || id < 0 || *depth == SIZE_MAX || retains == SIZE_MAX) {
        adapter_failed = true;
        return false;
    }
    ++*depth;
    ++retains;
    return true;
}

static void release(void *owner, mc_chunk_state *chunk) {
    int id = identity(chunk);
    size_t *depth = id == 1 ? &normal_leases : &blank_leases;
    if (owner != &reference_owner || id < 0 || *depth == 0 || releases == SIZE_MAX) {
        adapter_failed = true;
        return;
    }
    --*depth;
    ++releases;
}

static mc_chunk_load_result load(void *owner, const mc_chunk_acquire_context *context, int32_t x,
                                 int32_t z, mc_chunk_state **out) {
    mc_chunk_state *selected = load_available ? &normal : NULL;
    if (!journal("load", "entry", NULL, -1, selected, owner == &loader_owner,
                 context == &acquisition, true, x, z))
        return MC_CHUNK_LOAD_FATAL;
    bool available = load_available;
    if (available) {
        load_available = false;
        *out = &normal;
    }
    if (!journal("load", "return", available ? "loaded" : "miss", -1, selected,
                 owner == &loader_owner, context == &acquisition, true, x, z))
        return MC_CHUNK_LOAD_FATAL;
    return available ? MC_CHUNK_LOAD_LOADED : MC_CHUNK_LOAD_MISS;
}

static bool registration(void *owner, const mc_chunk_acquire_context *context,
                         mc_chunk_state *chunk, const char *kind, int slice) {
    return journal(kind, "entry", NULL, slice, chunk, owner == &world_owner,
                   context == &acquisition, false, 0, 0) &&
           journal(kind, "return", "ok", slice, chunk, owner == &world_owner,
                   context == &acquisition, false, 0, 0);
}

static bool register_tile_entities(void *owner, const mc_chunk_acquire_context *context,
                                   mc_chunk_state *chunk) {
    return registration(owner, context, chunk, "register_tiles", -1);
}

static bool register_entities(void *owner, const mc_chunk_acquire_context *context,
                              mc_chunk_state *chunk, size_t slice) {
    if (slice >= 8) {
        adapter_failed = true;
        return false;
    }
    return registration(owner, context, chunk, "register_entities", (int)slice);
}

static mc_chunk_callback_result lifecycle_callback(void *owner,
                                                   const mc_chunk_lifecycle_context *context,
                                                   mc_chunk_state *chunk, const char *kind,
                                                   int slice, bool loader) {
    bool matched = owner == (loader ? &loader_owner : &world_owner);
    if (!journal(kind, "entry", NULL, slice, chunk, matched, context == &lifecycle, false, 0, 0))
        return MC_CHUNK_CALLBACK_FATAL;
    if (strcmp(kind, "save_main") == 0 && chunk == &normal && main_q >= 0)
        chunk->has_entities = main_q != 0;
    return journal(kind, "return", "ok", slice, chunk, matched, context == &lifecycle, false, 0, 0)
               ? MC_CHUNK_CALLBACK_OK
               : MC_CHUNK_CALLBACK_FATAL;
}

static mc_chunk_callback_result invalidate_tile_entities(void *owner,
                                                         const mc_chunk_lifecycle_context *context,
                                                         mc_chunk_state *chunk) {
    return lifecycle_callback(owner, context, chunk, "invalidate_tiles", -1, false);
}

static mc_chunk_callback_result defer_entities(void *owner,
                                               const mc_chunk_lifecycle_context *context,
                                               mc_chunk_state *chunk, size_t slice) {
    if (slice >= 8) {
        adapter_failed = true;
        return MC_CHUNK_CALLBACK_FATAL;
    }
    return lifecycle_callback(owner, context, chunk, "defer_entities", (int)slice, false);
}

static mc_chunk_callback_result save_main(void *owner, const mc_chunk_lifecycle_context *context,
                                          mc_chunk_state *chunk) {
    return lifecycle_callback(owner, context, chunk, "save_main", -1, true);
}

static mc_chunk_callback_result save_extra(void *owner, const mc_chunk_lifecycle_context *context,
                                           mc_chunk_state *chunk) {
    return lifecycle_callback(owner, context, chunk, "save_extra", -1, true);
}

static mc_chunk_callback_result loader_maintenance(void *owner,
                                                   const mc_chunk_lifecycle_context *context) {
    return lifecycle_callback(owner, context, NULL, "loader_maintenance", -1, true);
}

static mc_chunk_callback_result flush(void *owner, const mc_chunk_lifecycle_context *context) {
    return lifecycle_callback(owner, context, NULL, "flush", -1, true);
}

static mc_chunk_callback_result
provider_maintenance(void *owner, const mc_chunk_lifecycle_context *context, bool *out) {
    if (!journal("provider_maintenance", "entry", NULL, -1, NULL, owner == &provider_owner,
                 context == &lifecycle, false, 0, 0))
        return MC_CHUNK_CALLBACK_FATAL;
    *out = false;
    return journal("provider_maintenance", "return", "false", -1, NULL, owner == &provider_owner,
                   context == &lifecycle, false, 0, 0)
               ? MC_CHUNK_CALLBACK_OK
               : MC_CHUNK_CALLBACK_FATAL;
}

static bool integer(const char *text, int32_t *out) {
    bool negative = *text == '-';
    text += negative ? 1 : 0;
    if (*text == '\0' || (text[0] == '0' && text[1] != '\0'))
        return false;
    uint64_t value = 0;
    uint64_t limit = negative ? UINT64_C(2147483648) : INT32_MAX;
    for (; *text != '\0'; ++text) {
        if (*text < '0' || *text > '9')
            return false;
        unsigned digit = (unsigned)(*text - '0');
        if (value > limit / 10u || (value == limit / 10u && digit > limit % 10u))
            return false;
        value = value * 10u + digit;
    }
    if (negative && value == 0)
        return false;
    *out = negative ? -1 - (int32_t)(value - 1u) : (int32_t)value;
    return true;
}

static int digit(unsigned char byte) {
    return byte >= '0' && byte <= '9'   ? byte - '0'
           : byte >= 'a' && byte <= 'f' ? byte - 'a' + 10
                                        : -1;
}

static bool decode(const char *text, uint8_t *out, size_t count) {
    if (strlen(text) != count * 2u)
        return false;
    for (size_t i = 0; i < count; ++i) {
        int a = digit((unsigned char)text[i * 2u]);
        int b = digit((unsigned char)text[i * 2u + 1u]);
        if (a < 0 || b < 0)
            return false;
        out[i] = (uint8_t)((unsigned)a * 16u + (unsigned)b);
    }
    return true;
}

static bool clock_value(const char *text, int64_t *out) {
    uint8_t bytes[8];
    if (!decode(text, bytes, sizeof(bytes)))
        return false;
    uint64_t value = 0;
    for (size_t i = 0; i < sizeof(bytes); ++i)
        value = value * UINT64_C(256) + bytes[i];
    *out = value <= INT64_MAX ? (int64_t)value : -1 - (int64_t)(UINT64_MAX - value);
    return true;
}

static bool parse_bool(const char *text, bool *out) {
    if (strcmp(text, "0") != 0 && strcmp(text, "1") != 0)
        return false;
    *out = text[0] == '1';
    return true;
}

static bool flags(const char *text, mc_chunk_state *chunk) {
    if (strlen(text) != 5)
        return false;
    for (size_t i = 0; i < 5; ++i)
        if (text[i] != '0' && text[i] != '1')
            return false;
    chunk->active = text[0] == '1';
    chunk->terrain_populated = text[1] == '1';
    chunk->dirty = text[2] == '1';
    chunk->never_save = text[3] == '1';
    chunk->has_entities = text[4] == '1';
    return true;
}

static bool open_context(char **fields, size_t count) {
    if (opened || count != 12 ||
        (strcmp(fields[1], "base") != 0 && strcmp(fields[1], "dedicated") != 0) ||
        !clock_value(fields[2], &world_time) || !integer(fields[3], &spawn_x) ||
        !integer(fields[4], &spawn_z) || !parse_bool(fields[5], &world_force) ||
        !parse_bool(fields[6], &provider_force) || !parse_bool(fields[7], &saving_disabled) ||
        !integer(fields[8], &fallback.x) || !integer(fields[9], &fallback.z) ||
        !flags(fields[10], &fallback) || !clock_value(fields[11], &fallback.last_save_time))
        return false;
    mc_java_int_set_limits limits = {PENDING_LIMIT, 65536u, 131072u};
    if (mc_java_int_set_create(&limits, &history) != MC_JAVA_INT_SET_OK ||
        !mc_chunk_cache_init(&cache, entries, MAP_LIMIT, insertions, LIST_LIMIT) ||
        !mc_chunk_pending_init_ordered(&pending, pending_keys, PENDING_LIMIT, history))
        return false;
    fallback.blank = true;
    references =
        (mc_chunk_references){.owner = &reference_owner, .retain = retain, .release = release};
    acquisition = (mc_chunk_acquire_context){.cache = &cache,
                                             .pending = &pending,
                                             .fallback = &fallback,
                                             .world_time = &world_time,
                                             .world_force_load = &world_force,
                                             .provider_force_load = &provider_force,
                                             .spawn_x = &spawn_x,
                                             .spawn_z = &spawn_z,
                                             .policy = strcmp(fields[1], "base") == 0
                                                           ? MC_CHUNK_ACQUIRE_BASE
                                                           : MC_CHUNK_ACQUIRE_DEDICATED,
                                             .world = &world_owner,
                                             .loader = &loader_owner,
                                             .load = load,
                                             .register_tile_entities = register_tile_entities,
                                             .register_entities = register_entities,
                                             .references = &references};
    lifecycle = (mc_chunk_lifecycle_context){.cache = &cache,
                                             .pending = &pending,
                                             .world_time = &world_time,
                                             .saving_disabled = &saving_disabled,
                                             .policy = acquisition.policy,
                                             .world = &world_owner,
                                             .loader = &loader_owner,
                                             .provider = &provider_owner,
                                             .save_main = save_main,
                                             .save_extra = save_extra,
                                             .flush = flush,
                                             .loader_maintenance = loader_maintenance,
                                             .invalidate_tile_entities = invalidate_tile_entities,
                                             .defer_entities = defer_entities,
                                             .provider_maintenance = provider_maintenance,
                                             .references = &references};
    opened = true;
    return true;
}

static bool import_chunk(char **fields, size_t count) {
    if (!opened || imported || count != 11 || !integer(fields[1], &normal.x) ||
        !integer(fields[2], &normal.z) || !flags(fields[3], &normal) ||
        !clock_value(fields[4], &normal.last_save_time) ||
        !mc_chunk_data_init(&data, blocks, sizeof(blocks)) ||
        !integer(fields[5], &data.minimum_height) || !decode(fields[6], blocks, sizeof(blocks)) ||
        !decode(fields[7], data.metadata, sizeof(data.metadata)) ||
        !decode(fields[8], data.skylight, sizeof(data.skylight)) ||
        !decode(fields[9], data.block_light, sizeof(data.block_light)) ||
        !decode(fields[10], data.heightmap, sizeof(data.heightmap)))
        return false;
    normal.data = &data;
    imported = true;
    load_available = true;
    return true;
}

static void print_chunk(const mc_chunk_state *chunk, bool blank, const graph_snapshot *state) {
    format("{\"id\":%u,\"coordinates\":[%" PRId32 ",%" PRId32 "],\"blank\":%s,"
           "\"flags\":{\"c\":%s,\"n\":%s,\"o\":%s,\"p\":%s,\"q\":%s},"
           "\"last_save_time\":\"%016" PRIx64 "\",\"minimum_height\":",
           blank ? 0u : 1u, chunk->x, chunk->z, boolean(chunk->blank), boolean(chunk->active),
           boolean(chunk->terrain_populated), boolean(chunk->dirty), boolean(chunk->never_save),
           boolean(chunk->has_entities), (uint64_t)chunk->last_save_time);
    if (blank) {
        emit("null,\"arrays\":null}");
        return;
    }
    format("%" PRId32 ",\"arrays\":{\"blocks\":\"", state->minimum);
    hex(state->bytes.blocks, sizeof(state->bytes.blocks));
    emit("\",\"metadata\":\"");
    hex(state->bytes.metadata, sizeof(state->bytes.metadata));
    emit("\",\"skylight\":\"");
    hex(state->bytes.skylight, sizeof(state->bytes.skylight));
    emit("\",\"block_light\":\"");
    hex(state->bytes.block_light, sizeof(state->bytes.block_light));
    emit("\",\"heightmap\":\"");
    hex(state->bytes.heightmap, sizeof(state->bytes.heightmap));
    emit("\"}}");
}

static void print_state(const graph_snapshot *state) {
    format("{\"policy\":\"%s\",\"time\":\"%016" PRIx64 "\",\"spawn\":[%" PRId32 ",%" PRId32 "],"
           "\"world_force_load\":%s,\"provider_force_load\":%s,\"saving_disabled\":%s,"
           "\"imported\":%s,\"load_available\":%s,\"main_q_effect\":",
           state->dedicated ? "dedicated" : "base", (uint64_t)state->time, state->spawn_x,
           state->spawn_z, boolean(state->world_force), boolean(state->provider_force),
           boolean(state->saving_disabled), boolean(state->imported),
           boolean(state->load_available));
    if (state->main_q < 0)
        emit("null");
    else
        emit(boolean(state->main_q != 0));
    emit(",\"normal\":");
    if (state->imported)
        print_chunk(&state->normal, false, state);
    else
        emit("null");
    emit(",\"fallback\":");
    print_chunk(&state->fallback, true, state);
    emit(",\"map\":[");
    for (size_t i = 0; i < state->map_count; ++i)
        format("%s[%" PRId32 ",%zu]", i == 0 ? "" : ",", state->map_keys[i], state->map_values[i]);
    emit("],\"list\":[");
    for (size_t i = 0; i < state->list_count; ++i)
        format("%s%zu", i == 0 ? "" : ",", state->list[i]);
    emit("],\"pending\":[");
    for (size_t i = 0; i < state->pending_count; ++i)
        format("%s%" PRId32, i == 0 ? "" : ",", state->order[i]);
    emit("],\"pending_membership\":[");
    for (size_t i = 0; i < state->pending_count; ++i)
        format("%s%" PRId32, i == 0 ? "" : ",", state->membership[i]);
    format("],\"leases\":{\"normal\":%zu,\"fallback\":%zu,\"retains\":%zu,\"releases\":%zu}}",
           state->normal_leases, state->blank_leases, state->retains, state->releases);
}

static bool respond(size_t index, const char *command, const char *category, unsigned status,
                    int result, int returned) {
    if (!snapshot(&final_state))
        return false;
    format("{\"schema\":1,\"kind\":\"RESPONSE\",\"index\":%zu,\"command\":\"%s\","
           "\"category\":\"%s\",\"status\":%u,\"result\":",
           index, command, category, status);
    emit(result < 0 ? "null" : boolean(result != 0));
    emit(",\"returned_id\":");
    if (returned < 0)
        emit("null");
    else
        format("%d", returned);
    emit(",\"callbacks\":[");
    for (size_t i = 0; i < event_count; ++i) {
        const callback_event *event = &events[i];
        format("%s{\"index\":%zu,\"kind\":\"%s\",\"edge\":\"%s\",\"outcome\":", i == 0 ? "" : ",",
               i, event->kind, event->edge);
        if (event->outcome == NULL)
            emit("null");
        else
            format("\"%s\"", event->outcome);
        emit(",\"chunk_id\":");
        if (event->selected < 0)
            emit("null");
        else
            format("%d", event->selected);
        emit(",\"slice\":");
        if (event->slice < 0)
            emit("null");
        else
            format("%d", event->slice);
        format(",\"owner_matched\":%s,\"context_matched\":%s,\"request_coordinates\":",
               boolean(event->owner_matched), boolean(event->context_matched));
        if (event->has_request)
            format("[%" PRId32 ",%" PRId32 "]", event->request_x, event->request_z);
        else
            emit("null");
        emit(",\"state\":");
        print_state(&event->state);
        emit("}");
    }
    emit("],\"state\":");
    print_state(&final_state);
    emit("}\n");
    return output_ok && fflush(stdout) == 0;
}

static bool execute(char **fields, size_t count, size_t index, bool *quit) {
    const char *command = fields[0], *category = "adapter";
    unsigned status = 0;
    int result = -1, returned = -1;
    if (strcmp(command, "OPEN") == 0) {
        if (!open_context(fields, count))
            return false;
    } else if (!opened) {
        return false;
    } else if (strcmp(command, "CHUNK") == 0) {
        if (!import_chunk(fields, count))
            return false;
    } else if (strcmp(command, "TIME") == 0) {
        if (count != 2 || !clock_value(fields[1], &world_time))
            return false;
    } else if (strcmp(command, "SET") == 0) {
        if (count != 3 || strcmp(fields[1], "MAIN_Q") != 0)
            return false;
        if (strcmp(fields[2], "keep") == 0)
            main_q = -1;
        else if (strcmp(fields[2], "0") == 0 || strcmp(fields[2], "1") == 0)
            main_q = fields[2][0] - '0';
        else
            return false;
    } else if (strcmp(command, "QUIT") == 0) {
        if (count != 1 || !imported)
            return false;
        *quit = true;
    } else if (!imported) {
        return false;
    } else if (strcmp(command, "ACQUIRE") == 0 || strcmp(command, "DROP") == 0) {
        int32_t x, z;
        if (count != 3 || !integer(fields[1], &x) || !integer(fields[2], &z))
            return false;
        category = "acquire";
        if (strcmp(command, "DROP") == 0) {
            status = (unsigned)mc_chunk_acquire_mark_drop(&acquisition, x, z);
        } else {
            mc_chunk_state *chunk = NULL;
            mc_chunk_acquire_status acquired = mc_chunk_acquire(&acquisition, x, z, &chunk);
            status = (unsigned)acquired;
            if (acquired == MC_CHUNK_ACQUIRE_OK) {
                returned = identity(chunk);
                if (returned < 0)
                    return false;
            }
        }
    } else if (strcmp(command, "DETACH") == 0) {
        if (count != 1)
            return false;
        category = "lifecycle";
        status = (unsigned)mc_chunk_lifecycle_detach(&lifecycle, &normal);
    } else if (strcmp(command, "MAINTAIN") == 0) {
        if ((count != 1 && count != 2) || (count == 2 && !clock_value(fields[1], &world_time)))
            return false;
        category = "lifecycle";
        bool provider_result = false;
        mc_chunk_lifecycle_status maintained =
            mc_chunk_lifecycle_maintain(&lifecycle, &provider_result);
        status = (unsigned)maintained;
        if (maintained == MC_CHUNK_LIFECYCLE_OK)
            result = provider_result ? 1 : 0;
    } else {
        return false;
    }
    return respond(index, command, category, status, result, returned) && !adapter_failed &&
           normal_leases == 0 && blank_leases == 0 && retains == releases;
}

int main(void) {
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) < 0 || _setmode(_fileno(stdout), _O_BINARY) < 0)
        return 1;
#endif
    char *line = malloc(LINE_LIMIT + 2u);
    if (line == NULL)
        return 1;
    size_t commands = 0, input_bytes = 0;
    bool quit = false, okay = true;
    while (!quit && fgets(line, (int)(LINE_LIMIT + 2u), stdin) != NULL) {
        size_t length = strlen(line);
        if (length == 0 || length > LINE_LIMIT || line[length - 1] != '\n' ||
            length > INPUT_LIMIT - input_bytes || commands == COMMAND_LIMIT) {
            okay = false;
            break;
        }
        input_bytes += length;
        line[--length] = '\0';
        if (length != 0 && line[length - 1] == '\r')
            line[--length] = '\0';
        char *fields[FIELD_LIMIT];
        size_t count = 1;
        fields[0] = line;
        for (size_t i = 0; i < length; ++i) {
            unsigned char byte = (unsigned char)line[i];
            if (byte == '\t') {
                if (i == 0 || line[i - 1] == '\0' || count == FIELD_LIMIT) {
                    okay = false;
                    break;
                }
                line[i] = '\0';
                fields[count++] = line + i + 1;
            } else if (byte < 32u || byte > 126u) {
                okay = false;
                break;
            }
        }
        if (!okay || fields[count - 1][0] == '\0') {
            okay = false;
            break;
        }
        event_count = 0;
        if (!execute(fields, count, ++commands, &quit)) {
            okay = false;
            break;
        }
    }
    if (quit) {
        while (fgetc(stdin) != EOF) {
            okay = false;
            if (input_bytes == INPUT_LIMIT)
                break;
            ++input_bytes;
        }
    }
    if (ferror(stdin) || !quit || !imported || !output_ok || adapter_failed || normal_leases != 0 ||
        blank_leases != 0)
        okay = false;
    /* No lifecycle callbacks, implicit saves or simulated world cleanup at EOF. */
    mc_java_int_set_free(history);
    free(line);
    if (!okay)
        fputs("lifecycle callback probe: invalid/unsupported input or bounded adapter failure\n",
              stderr);
    return okay ? 0 : 1;
}
