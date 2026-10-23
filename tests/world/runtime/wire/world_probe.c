/* Bounded native observation of an existing copied GAME World. The plan holds
 * operations and coordinates, never expected arrays, strengths or owner state.
 * Construction alone may replace session.lock. No save or tick is requested.
 * Pointer labels describe actual native address aliases, not JVM identities;
 * source wire key/node identities come from the real collection inspector. */
#include "mc/block/registry/runtime.h"
#include "mc/entity/classes/classes.h"
#include "mc/storage/region/region.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/scheduling.h"

#include "../../../../src/block/registry/registry_internal.h"
#include "../../../../src/platform/time/millis.h"
#include "../../../../src/world/chunk/object_state_internal.h"
#include "../../../../src/world/runtime/construction_internal.h"
#include "../../../../src/world/runtime/mutation/mutation_internal.h"
#include "../../../../src/world/runtime/provider_internal.h"
#include "../../../../src/world/runtime/runtime_internal.h"
#include "../../../../src/world/storage/objects_internal.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_COMMANDS = 96, MAX_CHUNKS = 32, MAX_ENTRIES = 32768, MAX_REFS = 262144 };

#define OUTPUT_LIMIT (64u * 1024u * 1024u)
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define TRY(value)                                                                                 \
    do {                                                                                           \
        if (!(value))                                                                              \
            return false;                                                                          \
    } while (0)

typedef enum operation { ACQUIRE, SETUP_SET, SET, META, NOTIFY, WIRE_NEIGHBOR } operation;

typedef struct command {
    operation kind;
    int32_t args[5];
    char text[256];
} command;

typedef struct input_stream {
    const uint8_t *bytes;
    size_t length, offset;
    bool closed;
} input_stream;

typedef struct resources {
    input_stream streams[3];
    size_t opens, closes;
    bool failed;
    mc_registry_resources api;
} resources;

typedef struct json {
    char *bytes;
    size_t length, capacity, written;
    FILE *output;
    bool failed;
} json;

typedef struct probe {
    bool client, bound;
    resources resources;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *registry;
    mc_entity_classes *entities;
    mc_world_runtime_classes *classes;
    mc_world_dimension *dimension;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_info_name *name;
    mc_world_runtime *world;
    mc_world_provider_fields provider;
    mc_world_storage_error storage;
    mc_world_mutation_error mutation;
    mc_chunk_state *chunks[MAX_CHUNKS];
    bool leased[MAX_CHUNKS];
    size_t chunk_count;
    const void **references;
    size_t reference_count;
    const mc_scheduled_tick **ticks;
    command commands[MAX_COMMANDS];
    size_t command_count, completed;
    int64_t session_token;
    const char *failure;
    unsigned int operation_status;
    json json;
} probe;

static const char *boolean(bool value) {
    return value ? "true" : "false";
}

static bool fail(probe *p, const char *stage) {
    if (p->failure == NULL)
        p->failure = stage;
    return false;
}

static FILE *open_file(const char *path, const char *mode) {
#ifdef _MSC_VER
    FILE *file = NULL;
    return fopen_s(&file, path, mode) == 0 ? file : NULL;
#else
    return fopen(path, mode);
#endif
}

static bool reserve(json *j, size_t extra) {
    if (j->failed || extra > OUTPUT_LIMIT - j->written ||
        j->length > OUTPUT_LIMIT - j->written - extra) {
        j->failed = true;
        return false;
    }
    const size_t need = j->length + extra + 1u;
    if (need > j->capacity) {
        size_t capacity = j->capacity == 0 ? 4096u : j->capacity;
        while (capacity < need) {
            if (capacity > (OUTPUT_LIMIT + 1u) / 2u) {
                capacity = OUTPUT_LIMIT + 1u;
                break;
            }
            capacity *= 2u;
        }
        char *replacement = realloc(j->bytes, capacity);
        if (replacement == NULL) {
            j->failed = true;
            return false;
        }
        j->bytes = replacement;
        j->capacity = capacity;
    }
    return true;
}

static bool emit(json *j, const char *format, ...) {
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    const int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0 || !reserve(j, (size_t)length)) {
        va_end(args);
        return false;
    }
    const int written = vsnprintf(j->bytes + j->length, j->capacity - j->length, format, args);
    va_end(args);
    if (written != length) {
        j->failed = true;
        return false;
    }
    j->length += (size_t)length;
    return true;
}

static bool string(json *j, const char *text) {
    if (text == NULL)
        return emit(j, "null");
    TRY(emit(j, "\""));
    for (const unsigned char *c = (const unsigned char *)text; *c != 0; ++c) {
        if (*c == '"' || *c == '\\')
            TRY(emit(j, "\\%c", *c));
        else if (*c < 32u)
            TRY(emit(j, "\\u%04x", (unsigned int)*c));
        else
            TRY(emit(j, "%c", *c));
    }
    return emit(j, "\"");
}

static bool finish_line(json *j) {
    TRY(emit(j, "\n"));
    if (fwrite(j->bytes, 1, j->length, j->output) != j->length || fflush(j->output) != 0) {
        j->failed = true;
        return false;
    }
    j->written += j->length;
    j->length = 0;
    return true;
}

static size_t reference(probe *p, const void *value) {
    if (value == NULL)
        return 0;
    for (size_t i = 0; i < p->reference_count; ++i)
        if (p->references[i] == value)
            return i + 1u;
    if (p->reference_count == MAX_REFS) {
        (void)fail(p, "native pointer alias bound");
        return 0;
    }
    p->references[p->reference_count++] = value;
    return p->reference_count;
}

static uint64_t double_bits(double value) {
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 storage required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool random_state(probe *p, const mc_java_random *value) {
    json *j = &p->json;
    if (value == NULL)
        return emit(j, "null");
    const mc_java_random before = *value;
    mc_java_random copy = before;
    TRY(emit(j,
             "{\"object\":%zu,\"native_seed_storage\":%zu,\"seed48\":\"%012" PRIx64
             "\",\"have_next_next_gaussian\":%s,\"next_next_gaussian_bits\":\"%016" PRIx64
             "\",\"clone_next_longs\":[",
             reference(p, value), reference(p, &value->state), before.state,
             boolean(before.have_next_next_gaussian), double_bits(before.next_next_gaussian)));
    for (size_t i = 0; i < 8u; ++i)
        TRY(emit(j, "%s\"%016" PRIx64 "\"", i == 0 ? "" : ",",
                 (uint64_t)mc_java_random_next_i64(&copy)));
    TRY(value->state == before.state &&
        value->have_next_next_gaussian == before.have_next_next_gaussian &&
        double_bits(value->next_next_gaussian) == double_bits(before.next_next_gaussian));
    return emit(j, "]}");
}

static bool blob(probe *p, const void *alias, const uint8_t *bytes, size_t length) {
    static const char digits[] = "0123456789abcdef";
    json *j = &p->json;
    TRY(bytes != NULL && length <= MC_CHUNK_BLOCKS);
    TRY(emit(j, "{\"object\":%zu,\"length\":%zu,\"hex\":\"", reference(p, alias), length));
    TRY(reserve(j, 2u * length));
    for (size_t i = 0; i < length; ++i) {
        j->bytes[j->length++] = digits[bytes[i] >> 4u];
        j->bytes[j->length++] = digits[bytes[i] & 15u];
    }
    return emit(j, "\"}");
}

static mc_java_properties_status resource_read(void *context, uint8_t *out, size_t capacity,
                                               size_t *count) {
    input_stream *s = context;
    if (s == NULL || out == NULL || capacity == 0 || count == NULL || s->closed ||
        s->offset > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->bytes + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status resource_open(void *context, const char *name,
                                        mc_java_properties_input *out) {
    resources *r = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    if (name == NULL || out == NULL || r->opens == COUNT(names) ||
        strcmp(name, names[r->opens]) != 0) {
        r->failed = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    *out = (mc_java_properties_input){&r->streams[r->opens++], resource_read};
    return MC_REGISTRY_OK;
}

static mc_registry_status resource_close(void *context, const mc_java_properties_input *input) {
    resources *r = context;
    if (input == NULL || input->context != &r->streams[2] || input->read != resource_read) {
        r->failed = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    r->streams[2].closed = true;
    ++r->closes;
    return MC_REGISTRY_OK;
}

static bool resource_ready(void *context, const mc_java_properties_input *input) {
    resources *r = context;
    if (input == NULL || input->read != resource_read)
        r->failed = true;
    return false;
}

static bool path_decode(const char *hex, char *path, size_t capacity) {
    const size_t length = strlen(hex);
    if (length == 0 || length % 2u != 0 || length / 2u >= capacity)
        return false;
    for (size_t i = 0; i < length / 2u; ++i) {
        unsigned int value = 0;
        for (size_t k = 0; k < 2; ++k) {
            const unsigned char c = (unsigned char)hex[2u * i + k];
            const unsigned int digit = c >= '0' && c <= '9'   ? (unsigned int)(c - '0')
                                       : c >= 'a' && c <= 'f' ? (unsigned int)(c - 'a') + 10u
                                                              : 16u;
            if (digit > 15u)
                return false;
            value = value * 16u + digit;
        }
        if (value == 0)
            return false;
        path[i] = (char)(unsigned char)value;
    }
    path[length / 2u] = '\0';
    return true;
}

static bool parse_plan(probe *p, const char *path) {
    char bytes[16385];
    FILE *input = open_file(path, "rb");
    if (input == NULL)
        return fail(p, "open existing plan");
    const size_t length = fread(bytes, 1, sizeof(bytes), input);
    const bool valid = !ferror(input) && length <= 16384u;
    const bool closed = fclose(input) == 0;
    if (!valid || !closed || memchr(bytes, 0, length) != NULL)
        return fail(p, "complete bounded ASCII plan");
    bytes[length] = '\0';
    bool header = false, runtime = false;
    char *cursor = bytes;
    while (*cursor != '\0') {
        char *end = strchr(cursor, '\n');
        if (end != NULL)
            *end = '\0';
        while (isspace((unsigned char)*cursor))
            ++cursor;
        size_t used = strlen(cursor);
        while (used != 0 && isspace((unsigned char)cursor[used - 1u]))
            cursor[--used] = '\0';
        if (used != 0 && *cursor != '#') {
            if (!header) {
                if (strcmp(cursor, "world 0000000000000017 0 64 0") != 0)
                    return fail(p, "exact seed23 origin plan header");
                header = true;
            } else {
                if (used >= sizeof(p->commands[0].text) || p->command_count == MAX_COMMANDS)
                    return fail(p, "bounded complete plan commands");
                command *c = &p->commands[p->command_count++];
                memcpy(c->text, cursor, used + 1u);
                char *words[7];
                size_t word_count = 0;
                char *scan = cursor;
                while (*scan != '\0') {
                    while (isspace((unsigned char)*scan))
                        ++scan;
                    if (*scan == '\0')
                        break;
                    if (word_count == COUNT(words))
                        return fail(p, "exact operation arity");
                    words[word_count++] = scan;
                    while (*scan != '\0' && !isspace((unsigned char)*scan))
                        ++scan;
                    if (*scan != '\0')
                        *scan++ = '\0';
                }
                if (word_count == 0)
                    return fail(p, "exact operation arity");
                const char *const names[] = {"acquire", "setup-set", "set",
                                             "meta",    "notify",    "wire-neighbor"};
                size_t kind = 0;
                while (kind < COUNT(names) && strcmp(words[0], names[kind]) != 0)
                    ++kind;
                if (kind == COUNT(names))
                    return fail(p, "recognized plan operation");
                c->kind = (operation)kind;
                const size_t arity = c->kind == ACQUIRE ? 2u : c->kind <= SET ? 5u : 4u;
                if (word_count != arity + 1u)
                    return fail(p, "exact operation arity");
                for (size_t i = 0; i < arity; ++i) {
                    char *tail = NULL;
                    errno = 0;
                    const long value = strtol(words[i + 1u], &tail, 10);
                    if (errno != 0 || tail == words[i + 1u] || *tail != '\0' || value < INT32_MIN ||
                        value > INT32_MAX)
                        return fail(p, "decimal int32 plan argument");
                    c->args[i] = (int32_t)value;
                }
                if (c->args[0] < -1024 || c->args[0] > 1024 ||
                    (c->kind == ACQUIRE && (c->args[1] < -1024 || c->args[1] > 1024)) ||
                    (c->kind != ACQUIRE &&
                     (c->args[1] < 0 || c->args[1] > 127 || c->args[2] < -1024 ||
                      c->args[2] > 1024 || c->args[3] < 0 || c->args[3] > 255)) ||
                    (c->kind == META && c->args[3] > 15) ||
                    ((c->kind == SET || c->kind == SETUP_SET) &&
                     (c->args[4] < 0 || c->args[4] > 15)))
                    return fail(p, "plan coordinate ID and metadata bounds");
                if (c->kind == SETUP_SET && runtime)
                    return fail(p, "setup edits precede runtime edits");
                if (c->kind != SETUP_SET && c->kind != ACQUIRE)
                    runtime = true;
            }
        }
        if (end == NULL)
            break;
        cursor = end + 1;
    }
    return (header && runtime && p->command_count != 0) || fail(p, "nonempty runtime plan");
}

static bool initialize(probe *p, const char *root, const mc_math_trig *trig) {
    static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                      "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";
    char world_path[8192], metadata_path[8192];
    int n = snprintf(world_path, sizeof(world_path), "%s/world", root);
    TRY(n > 0 && (size_t)n < sizeof(world_path));
    n = snprintf(metadata_path, sizeof(metadata_path), "%s/level.dat", world_path);
    TRY(n > 0 && (size_t)n < sizeof(metadata_path));
    FILE *existing = open_file(metadata_path, "rb");
    if (existing == NULL)
        return fail(p, "prepared copied world/level.dat required");
    const int first = fgetc(existing);
    const bool closed = fclose(existing) == 0;
    TRY(first != EOF && closed);
    TRY(mc_materials_create(&p->materials));
    TRY(mc_java_random_profile_create(4u * 1024u * 1024u, &p->profile) ==
        MC_JAVA_RANDOM_PROFILE_OK);
    p->resources.streams[0] = (input_stream){.bytes = language, .length = sizeof(language) - 1u};
    p->resources.streams[1] = p->resources.streams[2] = (input_stream){.bytes = language};
    p->resources.api =
        (mc_registry_resources){&p->resources, resource_open, resource_close, resource_ready};
    const mc_registry_options registry = {p->client ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                          p->profile, p->materials, 0, &p->resources.api};
    TRY(mc_registry_classes_allocate(&registry, &p->registry, NULL) == MC_REGISTRY_OK);
    TRY(mc_world_runtime_classes_create(p->client ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER,
                                        p->profile, trig, &p->classes,
                                        &p->storage) == MC_WORLD_STORAGE_OK);
    TRY(mc_world_runtime_classes_bind_registry(p->classes, p->registry, NULL) == MC_REGISTRY_OK);
    TRY(mc_entity_classes_create(p->client ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, p->profile, NULL,
                                 &p->entities) == MC_ENTITY_OK);
    TRY(mc_world_runtime_classes_bind_entities(p->classes, p->entities) == MC_ENTITY_OK);
    TRY(mc_registry_classes_initialize_blocks(p->registry, NULL) == MC_REGISTRY_OK &&
        p->resources.opens == 3 && p->resources.closes == 1 && !p->resources.failed);
    TRY(mc_world_regions_create(NULL, &p->regions, &p->storage) == MC_WORLD_STORAGE_OK);
    TRY(mc_platform_current_time_millis(&p->session_token) == MC_PLATFORM_MILLIS_OK);
    uint8_t block_map[256];
    for (size_t i = 0; i < COUNT(block_map); ++i)
        block_map[i] = (uint8_t)i;
    mc_world_store_options store = {.session_token = p->session_token,
                                    .block_map = block_map,
                                    .block_map_length = sizeof(block_map),
                                    .limits = mc_world_store_default_limits()};
    store.limits.max_loaded_chunks = MAX_CHUNKS - 1u;
    TRY(mc_world_store_open(world_path, p->regions, &store, &p->store, &p->storage) ==
        MC_WORLD_STORAGE_OK);
    const char title[] = "cache-callback-reference";
    uint16_t units[sizeof(title) - 1u];
    for (size_t i = 0; i < COUNT(units); ++i)
        units[i] = (uint16_t)(unsigned char)title[i];
    TRY(mc_world_info_name_copy((mc_nbt_string){units, COUNT(units)}, SIZE_MAX, &p->name) ==
        MC_WORLD_INFO_OK);
    TRY(mc_world_dimension_create(0, NULL, &p->dimension, NULL) == MC_WORLD_DIMENSION_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = MAX_CHUNKS - 1u;
    limits.max_scheduled_entries = MAX_ENTRIES;
    TRY(mc_world_runtime_allocate_world(p->client ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                                  : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                                        p->classes, &limits, &p->world,
                                        &p->storage) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments args = {
        .store = p->store, .name = p->name, .seed = 23, .dimension = p->dimension};
    TRY(mc_world_runtime_construct_world(p->world, &args, &p->storage) == MC_WORLD_STORAGE_OK);
    TRY(mc_world_provider_bind_fields(p->world->fields.v, &p->provider, &p->storage) ==
        MC_WORLD_STORAGE_OK);
    p->bound = true;
    mc_level_info info;
    mc_world_light_list_view queue;
    TRY(mc_world_runtime_read_info(p->world, &info, &p->storage) == MC_WORLD_STORAGE_OK &&
        info.seed == 23 && info.spawn_x == 0 && info.spawn_y == 64 && info.spawn_z == 0 &&
        !p->world->fields.s && p->provider.cache->count == 0 &&
        p->provider.cache->insertion_count == 0 && p->provider.pending->count == 0 &&
        mc_world_runtime_light_list_get(p->world, &queue, &p->storage) == MC_WORLD_STORAGE_OK &&
        queue.count == 0);
    return true;
}

static const void *source_chunk(probe *p, mc_chunk_state *state) {
    if (state == NULL || state == p->provider.fallback)
        return state;
    return mc_world_store_chunk_object(p->store, state);
}

static bool retain_chunk(probe *p, mc_chunk_state *state) {
    if (state == NULL)
        return fail(p, "nonnull retained Chunk");
    for (size_t i = 0; i < p->chunk_count; ++i)
        if (p->chunks[i] == state)
            return true;
    if (p->chunk_count == MAX_CHUNKS)
        return fail(p, "retained Chunk bound");
    const bool normal = state != p->provider.fallback;
    if (normal && mc_world_store_lease_chunk(p->store, state, &p->storage) != MC_WORLD_STORAGE_OK)
        return fail(p, "observer Chunk lease");
    p->chunks[p->chunk_count] = state;
    p->leased[p->chunk_count++] = normal;
    return true;
}

static bool track_chunks(probe *p) {
    mc_chunk_cache *cache = p->provider.cache;
    TRY(mc_chunk_cache_validate(cache) && mc_chunk_pending_validate(p->provider.pending));
    TRY(cache->count < MAX_CHUNKS && cache->insertion_count < MAX_CHUNKS);
    TRY(retain_chunk(p, p->provider.fallback));
    for (size_t i = 0; i < cache->count; ++i)
        TRY(retain_chunk(p, cache->entries[i].chunk));
    for (size_t i = 0; i < cache->insertion_count; ++i)
        TRY(retain_chunk(p, cache->insertions[i]));
    return true;
}

static bool chunk_state(probe *p, mc_chunk_state *state) {
    json *j = &p->json;
    const mc_chunk_data *d = state->data;
    TRY(d != NULL && d->blocks != NULL && d->blocks_capacity >= MC_CHUNK_BLOCKS);
    mc_world_chunk *chunk = state->blank ? NULL : mc_world_store_chunk_object(p->store, state);
    mc_world_chunk_fields fields = {0};
    if (!state->blank) {
        TRY(chunk != NULL && mc_world_chunk_fields_get(chunk, &fields) == MC_WORLD_STORAGE_OK);
        /* Successful source admission requires an actual populated stored
         * normal owner. A reached generated/opaque state is never relabeled. */
        if (chunk->storage_kind != CHUNK_STORAGE_LOADED || !state->terrain_populated)
            return fail(p, "profile excludes reached generated or unpopulated Chunk");
    }
    TRY(emit(j,
             "{\"object\":%zu,\"native_state\":%zu,\"world\":%zu,\"xz\":[%" PRId32 ",%" PRId32
             "],\"blank\":%s,\"flags\":{\"c\":%s,\"n\":%s,\"o\":%s,"
             "\"p\":%s,\"q\":%s},\"minimum_height\":%" PRId32 ",\"time\":\"%016" PRIx64
             "\",\"arrays\":{\"blocks\":",
             reference(p, source_chunk(p, state)), reference(p, state), reference(p, fields.d),
             state->x, state->z, boolean(state->blank), boolean(state->active),
             boolean(state->terrain_populated), boolean(state->dirty), boolean(state->never_save),
             boolean(state->has_entities), d->minimum_height, (uint64_t)state->last_save_time));
    const uint8_t *blocks = state->blank ? d->blocks : fields.b;
    TRY(blob(p, blocks, blocks, MC_CHUNK_BLOCKS));
    const char *const names[] = {"metadata", "skylight", "block_light"};
    const uint8_t *const arrays[] = {d->metadata, d->skylight, d->block_light};
    const mc_world_chunk_nibble *const wrappers[] = {fields.e, fields.f, fields.g};
    for (size_t i = 0; i < COUNT(names); ++i) {
        const uint8_t *array = arrays[i];
        size_t length = MC_CHUNK_NIBBLE_BYTES;
        if (!state->blank) {
            mc_world_chunk_nibble_fields nibble;
            TRY(mc_world_chunk_nibble_fields_get(wrappers[i], &nibble) == MC_WORLD_STORAGE_OK &&
                nibble.length == MC_CHUNK_NIBBLE_BYTES);
            array = nibble.a;
            length = nibble.length;
        }
        TRY(emit(j, ",\"%s\":{\"wrapper\":%zu,\"data\":", names[i], reference(p, wrappers[i])));
        TRY(blob(p, array, array, length));
        TRY(emit(j, "}"));
    }
    TRY(emit(j, ",\"heightmap\":"));
    const uint8_t *heights = state->blank ? d->heightmap : fields.h;
    TRY(blob(p, heights, heights, MC_CHUNK_HEIGHTMAP_BYTES));
    TRY(emit(j, "},\"tiles\":%zu,\"entity_array\":%zu,\"entity_slices\":[", reference(p, fields.l),
             reference(p, fields.m)));
    if (chunk != NULL) {
        size_t tile_count = SIZE_MAX;
        TRY(mc_world_chunk_tile_count(fields.l, &tile_count) == MC_WORLD_STORAGE_OK);
        if (tile_count != 0)
            return fail(p, "profile excludes tile-bearing Chunks");
        for (size_t i = 0; i < CHUNK_ENTITY_SLICES; ++i) {
            const mc_world_chunk_entity_list *list = NULL;
            size_t count = SIZE_MAX;
            TRY(mc_world_chunk_slice_at(fields.m, i, &list) == MC_WORLD_STORAGE_OK &&
                mc_world_chunk_entity_count(list, &count) == MC_WORLD_STORAGE_OK);
            if (count != 0)
                return fail(p, "profile excludes entity-bearing Chunks and drops");
            TRY(emit(j, "%s{\"object\":%zu,\"values\":[]}", i == 0 ? "" : ",", reference(p, list)));
        }
    }
    return emit(j, "],\"representation\":\"%s\"}",
                state->blank ? "native embedded fallback; no normal child handles"
                             : "actual stored normal Chunk");
}

static bool wire_state(probe *p) {
    json *j = &p->json;
    const mc_block *wire = p->registry->blocks.m[55];
    TRY(wire != NULL && wire->kind == MC_BLOCK_SM_LO && wire->fields.bn == 55);
    mc_registry_wire_set_view view;
    TRY(mc_registry_wire_set_inspect(wire->subtype.sm.b, &view) == MC_REGISTRY_OK);
    TRY(view.size <= MAX_ENTRIES && view.capacity <= 65536u);
    uint32_t load_factor_bits;
    _Static_assert(sizeof(load_factor_bits) == sizeof(view.load_factor),
                   "binary32 storage required");
    memcpy(&load_factor_bits, &view.load_factor, sizeof(load_factor_bits));
    mc_registry_wire_node_view *nodes = NULL;
    uint64_t *roots = NULL;
    bool ok = false;
    if (view.size != 0) {
        nodes = calloc(view.size, sizeof(*nodes));
        if (nodes == NULL)
            goto cleanup;
    }
    if (view.capacity != 0) {
        roots = calloc(view.capacity, sizeof(*roots));
        if (roots == NULL)
            goto cleanup;
    }
    for (size_t i = 0; i < view.size; ++i) {
        if (mc_registry_wire_set_node_at(wire->subtype.sm.b, i, &nodes[i]) != MC_REGISTRY_OK ||
            nodes[i].bucket >= view.capacity)
            goto cleanup;
        if (roots[nodes[i].bucket] == 0)
            roots[nodes[i].bucket] = nodes[i].identity;
    }
    if (!emit(j,
              "{\"object\":%zu,\"a\":%s,\"b\":{\"set\":%zu,\"map\":%zu,\"table\":%zu,"
              "\"size\":%zu,\"capacity\":%u,\"threshold\":%" PRId32 ",\"mod_count\":%" PRId32
              ",\"load_factor_bits\":\"%08" PRIx32 "\",\"constructed_keys\":\"%016" PRIx64
              "\",\"native_snapshot_count\":%zu,\"roots\":[",
              reference(p, wire), boolean(wire->subtype.sm.a), reference(p, wire->subtype.sm.b),
              reference(p, view.map), reference(p, view.table), view.size, view.capacity,
              view.threshold, view.mod_count, load_factor_bits, view.constructed_keys,
              p->registry->wire_snapshot_count))
        goto cleanup;
    for (uint32_t i = 0; i < view.capacity; ++i)
        if (!emit(j, "%s\"%016" PRIx64 "\"", i == 0 ? "" : ",", roots[i]))
            goto cleanup;
    if (!emit(j, "],\"nodes\":["))
        goto cleanup;
    for (size_t i = 0; i < view.size; ++i) {
        const mc_registry_wire_node_view *n = &nodes[i];
        if (!emit(j,
                  "%s{\"identity\":\"%016" PRIx64 "\",\"next\":\"%016" PRIx64
                  "\",\"previous\":\"%016" PRIx64 "\",\"parent\":\"%016" PRIx64
                  "\",\"left\":\"%016" PRIx64 "\",\"right\":\"%016" PRIx64 "\",\"hash\":%" PRIu32
                  ",\"bucket\":%u,\"tree\":%s,\"red\":%s,"
                  "\"key\":{\"identity\":\"%016" PRIx64 "\",\"xyz\":[%" PRId32 ",%" PRId32
                  ",%" PRId32 "],\"identity_hash_ready\":%s,\"identity_hash\":%" PRId32 "}}",
                  i == 0 ? "" : ",", n->identity, n->next, n->previous, n->parent, n->left,
                  n->right, n->hash, n->bucket, boolean(n->tree), boolean(n->red), n->key.identity,
                  n->key.x, n->key.y, n->key.z, boolean(n->key.identity_hash_ready),
                  n->key.identity_hash))
            goto cleanup;
    }
    ok = emit(j, "],\"keyset_view\":\"not represented natively\","
                 "\"values\":\"native Set nodes have no separate map value owner\"}}");
cleanup:
    free(nodes);
    free(roots);
    return ok;
}

static bool scheduled(probe *p, bool ordered) {
    json *j = &p->json;
    int32_t size = -1;
    size_t written = 0;
    if (ordered) {
        TRY(mc_scheduled_ordered_size(p->world->fields.E, &size) == MC_SCHEDULED_ORDERED_OK);
        TRY(size >= 0 && size <= MAX_ENTRIES);
        TRY(mc_scheduled_ordered_snapshot(p->world->fields.E, p->ticks, MAX_ENTRIES, &written) ==
            MC_SCHEDULED_ORDERED_OK);
    } else {
        TRY(mc_scheduled_membership_size(p->world->fields.F, &size) == MC_SCHEDULED_MEMBERSHIP_OK);
        TRY(size >= 0 && size <= MAX_ENTRIES);
        TRY(mc_scheduled_membership_snapshot(p->world->fields.F, p->ticks, MAX_ENTRIES, &written) ==
            MC_SCHEDULED_MEMBERSHIP_OK);
    }
    TRY(written == (size_t)size);
    TRY(emit(
        j, "{\"object\":%zu,\"order\":\"%s\",\"entries\":[",
        reference(p, ordered ? (const void *)p->world->fields.E : (const void *)p->world->fields.F),
        ordered ? "native structural in-order" : "native membership bucket order"));
    for (size_t i = 0; i < written; ++i) {
        mc_scheduled_tick_fields f;
        TRY(mc_scheduled_tick_fields_get(p->ticks[i], &f) == MC_SCHEDULED_TICK_OK);
        TRY(emit(j,
                 "%s{\"object\":%zu,\"xyz\":[%" PRId32 ",%" PRId32 ",%" PRId32
                 "],\"block\":%" PRId32 ",\"time\":\"%016" PRIx64 "\",\"sequence\":\"%016" PRIx64
                 "\"}",
                 i == 0 ? "" : ",", reference(p, p->ticks[i]), f.x, f.y, f.z, f.block,
                 (uint64_t)f.due_time, (uint64_t)f.sequence));
    }
    return emit(j, "]}");
}

static bool snapshot(probe *p) {
    json *j = &p->json;
    TRY(track_chunks(p));
    mc_world_runtime_fields *f = &p->world->fields;
    mc_level_info info;
    mc_world_light_list_view lights;
    mc_world_schedule_class_state sequence;
    TRY(mc_world_runtime_read_info(p->world, &info, &p->storage) == MC_WORLD_STORAGE_OK);
    TRY(mc_world_runtime_light_list_get(p->world, &lights, &p->storage) == MC_WORLD_STORAGE_OK &&
        lights.count <= MAX_ENTRIES);
    TRY(mc_world_runtime_schedule_class_get(p->classes, &sequence) == MC_WORLD_STORAGE_OK);
    TRY(f->b != NULL && f->b->count == 0);
    TRY(emit(j,
             "{\"world\":%zu,\"provider\":%zu,\"world_cache\":%zu,\"world_storage\":%zu,"
             "\"provider_world\":%zu,\"cache\":%zu,\"cache_loader\":%zu,\"cache_fallback\":%zu,"
             "\"cache_generator\":%zu,\"world_o\":%s,"
             "\"world_B\":%s,\"world_y\":%s,\"world_time\":\"%016" PRIx64
             "\",\"enqueue_depth\":%" PRId32 ",\"drain_depth\":%" PRId32
             ",\"chunk_static_a\":%s,\"map\":{\"order\":\"native entry storage\",\"entries\":[",
             reference(p, p->world), reference(p, f->t), reference(p, f->v), reference(p, f->w),
             reference(p, p->provider.world), reference(p, p->provider.cache),
             reference(p, &p->world->loader), reference(p, source_chunk(p, p->provider.fallback)),
             reference(p, p->provider.generator), boolean(f->o), boolean(f->B), boolean(f->y),
             (uint64_t)info.time, p->classes->enqueue_depth, f->M, boolean(p->classes->chunk_a)));
    for (size_t i = 0; i < p->provider.cache->count; ++i) {
        const mc_chunk_cache_entry *e = &p->provider.cache->entries[i];
        TRY(emit(j, "%s{\"key\":%" PRId32 ",\"value\":%zu}", i == 0 ? "" : ",", e->key,
                 reference(p, source_chunk(p, e->chunk))));
    }
    TRY(emit(j, "]},\"insertion_list\":["));
    for (size_t i = 0; i < p->provider.cache->insertion_count; ++i)
        TRY(emit(j, "%s%zu", i == 0 ? "" : ",",
                 reference(p, source_chunk(p, p->provider.cache->insertions[i]))));
    TRY(emit(j, "],\"pending_set\":["));
    for (size_t i = 0; i < p->provider.pending->count; ++i)
        TRY(emit(j, "%s%" PRId32, i == 0 ? "" : ",", p->provider.pending->keys[i]));
    TRY(emit(j, "],\"chunks\":["));
    for (size_t i = 0; i < p->chunk_count; ++i) {
        TRY(emit(j, "%s", i == 0 ? "" : ","));
        TRY(chunk_state(p, p->chunks[i]));
    }
    TRY(emit(j, "],\"queues\":{\"C\":{\"object\":%zu,\"entries\":[",
             reference(p, lights.identity)));
    for (size_t i = 0; i < lights.count; ++i) {
        const mc_world_light_region *region = NULL;
        TRY(mc_world_runtime_light_at(p->world, i, &region, &p->storage) == MC_WORLD_STORAGE_OK);
        const mc_world_light_region_fields *r = mc_world_runtime_light_region_fields_get(region);
        TRY(r != NULL);
        const mc_world_light_type_fields *type = mc_world_runtime_light_type_fields_get(r->a);
        TRY(type != NULL);
        TRY(emit(j,
                 "%s{\"object\":%zu,\"type\":%zu,\"kind\":%u,\"default\":%" PRId32
                 ",\"bounds\":[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32
                 "]}",
                 i == 0 ? "" : ",", reference(p, region), reference(p, r->a),
                 (unsigned int)type->kind, type->c, r->bounds.min_x, r->bounds.min_y,
                 r->bounds.min_z, r->bounds.max_x, r->bounds.max_y, r->bounds.max_z));
    }
    TRY(emit(j, "]},\"E\":"));
    TRY(scheduled(p, true));
    TRY(emit(j, ",\"F\":"));
    TRY(scheduled(p, false));
    TRY(emit(j, "},\"schedule_class\":{\"initialized\":%s,\"next\":\"%016" PRIx64 "\"},\"wire\":",
             boolean(sequence.initialized), (uint64_t)sequence.next));
    TRY(wire_state(p));
    TRY(emit(j, ",\"random_owners\":{\"world\":"));
    TRY(random_state(p, f->r));
    TRY(emit(j, ",\"generator\":"));
    TRY(random_state(p, &p->provider.generator->generator_random));
    TRY(emit(j, ",\"cave\":"));
    TRY(random_state(p, &p->provider.generator->cave_random));
    TRY(emit(j, "},\"native_lifetime\":{\"active_calls\":%zu,\"local_leases\":%zu}}",
             p->world->active_calls, p->world->local_leases));
    return p->failure == NULL;
}

static bool boundary(probe *p, const char *name, int ordinal, const command *c) {
    json *j = &p->json;
    TRY(emit(j, "{\"type\":\"snapshot\",\"boundary\":\"%s\",\"ordinal\":%d,\"input\":", name,
             ordinal));
    TRY(string(j, c == NULL ? NULL : c->text));
    TRY(emit(j, ",\"setup\":%s,\"state\":", boolean(c != NULL && c->kind == SETUP_SET)));
    TRY(snapshot(p));
    TRY(emit(j, "}"));
    return finish_line(j);
}

static bool diagnostic(probe *p) {
    json *j = &p->json;
    const mc_world_mutation_error *e = &p->mutation;
    TRY(emit(j, "{\"status\":%u,\"operation\":", (unsigned int)e->status));
    TRY(string(j, e->operation));
    TRY(emit(j, ",\"cause_status\":%u,\"cause_operation\":", (unsigned int)e->cause.status));
    TRY(string(j, e->cause.operation));
    TRY(emit(j, ",\"system_error\":%" PRIu32 ",\"hook_class\":", e->cause.system_error));
    TRY(string(j, e->hook_class));
    TRY(emit(j, ",\"hook_method\":"));
    TRY(string(j, e->hook_method));
    TRY(emit(j, ",\"cause_hook_class\":"));
    TRY(string(j, e->cause_hook_class));
    TRY(emit(j, ",\"sites\":{"));

    const struct {
        const char *name;
        unsigned int value;
    } sites[] = {{"wrapper", e->wrapper_bytecode},
                 {"world", e->bytecode},
                 {"chunk", e->chunk_bytecode},
                 {"height", e->height_bytecode},
                 {"column", e->column_bytecode},
                 {"interval", e->interval_bytecode},
                 {"observer", e->observer_bytecode},
                 {"neighbor_call", e->neighbor_call_bytecode},
                 {"neighbor", e->neighbor_bytecode},
                 {"hook", e->hook_bytecode},
                 {"cause_hook", e->cause_hook_bytecode},
                 {"provider", e->provider_bytecode},
                 {"presence", e->presence_bytecode},
                 {"drop", e->drop_bytecode},
                 {"drop_call", e->drop_call_bytecode},
                 {"drop_spawn", e->drop_spawn_bytecode},
                 {"load", e->load_bytecode},
                 {"load_builder", e->load_builder_bytecode},
                 {"activation", e->activation_bytecode},
                 {"activation_world", e->activation_world_bytecode},
                 {"pressure", e->pressure_bytecode},
                 {"pressure_neighbor", e->pressure_neighbor_bytecode},
                 {"pressure_removed", e->pressure_removed_bytecode},
                 {"wire_added", e->wire.added},
                 {"wire_removed", e->wire.removed},
                 {"wire_neighbor", e->wire.neighbor},
                 {"wire_support", e->wire.support},
                 {"wire_drain", e->wire.drain},
                 {"wire_update", e->wire.update},
                 {"wire_notify", e->wire.notify},
                 {"wire_strength", e->wire.strength}};

    for (size_t i = 0; i < COUNT(sites); ++i)
        TRY(emit(j, "%s\"%s\":%u", i == 0 ? "" : ",", sites[i].name, sites[i].value));
    TRY(emit(
        j, "},\"registry_status\":%u,\"registry_initializer_pc\":%" PRId32 ",\"registry_message\":",
        (unsigned int)e->registry.status, e->registry.initializer_pc));
    TRY(string(j, e->registry.message));
    TRY(emit(j,
             ",\"wire_value_failed\":%s,\"wire_xyz\":[%" PRId32 ",%" PRId32 ",%" PRId32
             "],\"power_status\":%u,\"query_status\":%u,\"light_status\":%u,"
             "\"projection\":\"finite current/deepest sites and statuses; nested child graphs "
             "omitted\"}",
             boolean(e->wire_value_failed), e->wire_x, e->wire_y, e->wire_z,
             (unsigned int)e->power.status, (unsigned int)e->query.status,
             (unsigned int)e->light.status));
    return true;
}

static bool execute(probe *p, const command *c, size_t ordinal) {
    const int32_t *a = c->args;
    bool result = false;
    mc_chunk_state *chunk = NULL;
    mc_world_mutation_error_init(&p->mutation);
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    if (c->kind == ACQUIRE) {
        const mc_chunk_acquire_status acquired =
            mc_world_runtime_acquire(p->world, a[0], a[1], &chunk);
        p->operation_status = (unsigned int)acquired;
        if (acquired != MC_CHUNK_ACQUIRE_OK)
            status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    } else if (c->kind == SET || c->kind == SETUP_SET) {
        status = mc_world_runtime_block_metadata_set(p->world, a[0], a[1], a[2], a[3], a[4],
                                                     &result, &p->mutation);
        p->operation_status = (unsigned int)status;
    } else if (c->kind == META) {
        status = mc_world_runtime_metadata_set(p->world, a[0], a[1], a[2], a[3], &p->mutation);
        p->operation_status = (unsigned int)status;
    } else {
        status = mc_world_runtime_call_enter(p->world, &p->storage);
        if (status == MC_WORLD_STORAGE_OK) {
            const bool complete =
                c->kind == NOTIFY
                    ? mc_world_mutation_neighbors(p->world, a[0], a[1], a[2], a[3], &p->mutation)
                    : mc_world_mutation_wire_neighbor(p->world, p->registry->blocks.m[55], a[0],
                                                      a[1], a[2], a[3], &p->mutation);
            mc_world_runtime_call_leave(p->world);
            if (!complete)
                status = p->mutation.status;
        }
        p->operation_status = (unsigned int)status;
    }
    json *j = &p->json;
    TRY(emit(j, "{\"type\":\"result\",\"ordinal\":%zu,\"input\":", ordinal));
    TRY(string(j, c->text));
    TRY(emit(j, ",\"native_status\":%u,\"native_domain\":\"%s\",\"result\":", p->operation_status,
             c->kind == ACQUIRE ? "acquisition" : "world-storage"));
    if (c->kind == ACQUIRE)
        TRY(emit(j, "%zu", reference(p, source_chunk(p, chunk))));
    else
        TRY(emit(j, "%s", c->kind == SET || c->kind == SETUP_SET ? boolean(result) : "null"));
    TRY(emit(j, ",\"mutation\":"));
    TRY(diagnostic(p));
    TRY(emit(j, ",\"last_storage_status\":%u,\"last_storage_operation\":",
             (unsigned int)p->world->last_error.status));
    TRY(string(j, p->world->last_error.operation));
    TRY(emit(j, "}"));
    TRY(finish_line(j));
    if (status != MC_WORLD_STORAGE_OK)
        return fail(p, "reached operation failure; retained partial prefix");
    return true;
}

static bool failure_record(probe *p) {
    json *j = &p->json;
    TRY(emit(j, "{\"type\":\"failure\",\"stage\":"));
    TRY(string(j, p->failure));
    TRY(emit(j, ",\"operation_status\":%u,\"storage_status\":%u,\"storage_operation\":",
             p->operation_status, (unsigned int)p->storage.status));
    TRY(string(j, p->storage.operation));
    TRY(emit(j,
             ",\"storage_system_error\":%" PRIu32 ",\"construction\":", p->storage.system_error));
    if (p->world == NULL) {
        TRY(emit(j, "null"));
    } else {
        const mc_world_runtime_construction *c = &p->world->construction;
        TRY(emit(j, "{\"phase\":%u,\"last_store_pc\":%u,\"failed_call_pc\":%u}",
                 (unsigned int)c->phase, c->last_store_pc, c->failed_call_pc));
    }
    TRY(emit(j, ",\"registry\":"));
    if (p->registry == NULL) {
        TRY(emit(j, "null"));
    } else {
        const mc_registry_error *e = &p->registry->last_error;
        TRY(emit(j,
                 "{\"status\":%u,\"initializer_pc\":%" PRId32 ",\"constructor_pc\":%" PRId32
                 ",\"message\":",
                 (unsigned int)e->status, e->initializer_pc, e->constructor_pc));
        TRY(string(j, e->message));
        TRY(emit(j, "}"));
    }
    TRY(emit(j, ",\"mutation\":"));
    TRY(diagnostic(p));
    TRY(emit(j, "}"));
    return finish_line(j);
}

static bool cleanup(probe *p) {
    for (size_t i = 0; i < p->chunk_count; ++i)
        if (p->leased[i]) {
            if (mc_world_store_release_chunk(p->store, p->chunks[i], &p->storage) !=
                MC_WORLD_STORAGE_OK)
                return false;
            p->leased[i] = false;
        }
    if (p->world != NULL) {
        /* A constructor can publish its real provider before failing. Bind
         * that actual prefix even if normal initialization never got here. */
        if (!p->bound && p->world->fields.v != NULL) {
            TRY(mc_world_provider_bind_fields(p->world->fields.v, &p->provider, &p->storage) ==
                MC_WORLD_STORAGE_OK);
            p->bound = true;
        }
        TRY(mc_world_runtime_retire_entity_containers(p->world, &p->storage) ==
            MC_WORLD_STORAGE_OK);
        if (p->bound) {
            TRY(mc_chunk_pending_clear(p->provider.pending) == MC_CHUNK_ACQUIRE_OK);
            mc_chunk_cache *cache = p->provider.cache;
            TRY(mc_chunk_cache_init(cache, cache->entries, cache->capacity, cache->insertions,
                                    cache->insertion_capacity));
        }
        TRY(mc_world_runtime_retire_world(p->world, &p->storage) == MC_WORLD_STORAGE_OK);
    }
    while (mc_world_store_chunk_count(p->store) != 0) {
        mc_chunk_state *state = NULL;
        TRY(mc_world_store_chunk_at(p->store, 0, &state, &p->storage) == MC_WORLD_STORAGE_OK);
        TRY(mc_world_store_retire_chunk(p->store, state, &p->storage) == MC_WORLD_STORAGE_OK);
    }
    if (p->world != NULL) {
        TRY(mc_world_runtime_free(p->world, &p->storage) == MC_WORLD_STORAGE_OK);
        p->world = NULL;
    }
    mc_world_dimension_release(p->dimension);
    TRY(mc_world_runtime_classes_free(p->classes, &p->storage) == MC_WORLD_STORAGE_OK);
    TRY(mc_registry_classes_free(p->registry, NULL) == MC_REGISTRY_OK);
    TRY(mc_entity_classes_free(p->entities) == MC_ENTITY_OK);
    mc_world_info_name_release(p->name);
    TRY(mc_world_store_free_checked(&p->store, &p->storage) == MC_WORLD_STORAGE_OK);
    mc_world_regions_free(p->regions);
    mc_java_random_profile_free(p->profile);
    mc_materials_destroy(p->materials);
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    char root[8192];
    static mc_math_trig trig;
    if (argc != 5 || (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0) ||
        !path_decode(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig)) {
        fputs("usage: world_probe client|server PREPARED_ROOT_HEX PLAN_TXT FRESH_OUTPUT_JSONL\n",
              stderr);
        return 1;
    }
    probe *p = calloc(1, sizeof(*p));
    if (p == NULL)
        return 1;
    p->client = strcmp(argv[1], "client") == 0;
    p->references = calloc(MAX_REFS, sizeof(*p->references));
    p->ticks = calloc(MAX_ENTRIES, sizeof(*p->ticks));
    p->json.output = open_file(argv[4], "wbx");
    if (p->references == NULL || p->ticks == NULL || p->json.output == NULL ||
        !parse_plan(p, argv[3]))
        goto finish;
    json *j = &p->json;
    if (!emit(
            j,
            "{\"schema\":1,\"type\":\"header\",\"scope\":\"native-existing-World-wire-plan\","
            "\"qualified\":false,\"side\":\"%s\",\"planned\":%zu,"
            "\"resource_variant\":\"authored minimal four stat translations; empty stats and map\","
            "\"limits\":{\"commands\":96,\"chunks_with_fallback\":32,\"entries\":32768,"
            "\"wire_buckets\":65536,\"pointer_labels\":262144,\"output_bytes\":67108864},"
            "\"aliases\":\"labels compare native storage addresses; structural adaptation is "
            "required; "
            "wire key/node IDs are actual owner history\","
            "\"random\":\"actual default-clock World/cave state retained; continuations from value "
            "copies\","
            "\"gaps\":\"no original local snapshot or in-method trace; no JVM identity-hash "
            "schedule; "
            "entity/tile/generated/unpopulated Chunks fail; native E/F inventories omit tree/hash "
            "links; "
            "native fallback has no normal child handles; finite failure diagnostic projection\"}",
            argv[1], p->command_count) ||
        !finish_line(j))
        goto finish;
    if (!initialize(p, root, &trig)) {
        (void)fail(p, "native owner construction or loaded origin profile");
        goto finish;
    }
    if (!boundary(p, "initial", -1, NULL)) {
        (void)fail(p, "initial complete snapshot");
        goto finish;
    }
    for (size_t i = 0; i < p->command_count; ++i) {
        const command *c = &p->commands[i];
        if (!boundary(p, "before", (int)i, c)) {
            (void)fail(p, "before complete snapshot");
            goto finish;
        }
        const bool complete = execute(p, c, i);
        /* An operation failure still gets an attempted actual after snapshot;
         * preserve its diagnostics and source effects without continuing. */
        const char *failure = p->failure;
        p->failure = NULL;
        const bool captured = boundary(p, "after", (int)i, c);
        if (failure != NULL)
            p->failure = failure;
        if (!complete || !captured) {
            (void)fail(p, "after complete snapshot");
            goto finish;
        }
        ++p->completed;
    }
    result = 0;
finish:
    if (p->json.length != 0)
        p->json.length = 0; /* Failed partial records are never successful snapshots. */
    if (p->failure != NULL && p->json.output != NULL && !p->json.failed)
        (void)failure_record(p);
    const bool released = cleanup(p);
    if (!released) {
        (void)fail(p, "native terminal cleanup refused; surviving owners retained");
        result = 1;
    }
    if (p->failure != NULL || p->json.failed)
        result = 1;
    if (p->json.output != NULL && !p->json.failed) {
        if (!emit(&p->json,
                  "{\"type\":\"terminal\",\"complete\":%s,\"cleanup\":%s,\"completed\":%zu,"
                  "\"planned\":%zu,\"last_operation_status\":%u,\"session_token\":\"%016" PRIx64
                  "\",\"failure\":",
                  boolean(result == 0), boolean(released), p->completed, p->command_count,
                  p->operation_status, (uint64_t)p->session_token) ||
            !string(&p->json, p->failure) || !emit(&p->json, "}") || !finish_line(&p->json))
            result = 1;
    }
    if (p->json.output != NULL && fclose(p->json.output) != 0)
        result = 1;
    if (result != 0)
        fprintf(stderr, "native wire probe failed: %s\n",
                p->failure != NULL ? p->failure : "output or admission");
    free(p->json.bytes);
    free(p->references);
    free(p->ticks);
    free(p);
    return result;
}
