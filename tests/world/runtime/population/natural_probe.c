/* Finite GAME World observation. All source state comes from fresh constructors
 * and the scheduled actual cache c calls; no captured result is imported. Native
 * terminal retirement runs only after the complete observation is encoded. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "mc/resources/files/files.h"
#include "mc/world/biome/registry.h"
#include "mc/world/biome/sampling.h"
#include "mc/world/dimension/biome_runtime.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/lighting.h"
#include "mc/world/runtime/saved_data.h"
#include "mc/world/runtime/saving/saving.h"

#include "../../../../src/block/registry/registry_internal.h"
#include "../../../../src/platform/time/millis.h"
#include "../../../../src/world/biome/biome_internal.h"
#include "../../../../src/world/runtime/construction_internal.h"
#include "../../../../src/world/runtime/loading/loading_internal.h"
#include "../../../../src/world/runtime/provider_internal.h"
#include "../../../../src/world/storage/files.h"
#include "../../../../src/world/storage/objects_internal.h"

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

enum { NATURAL_CAPACITY = 32, NATURAL_IDENTITIES = NATURAL_CAPACITY + 1 };

typedef enum natural_array_domain {
    NATURAL_DOUBLE_ARRAY,
    NATURAL_BIOME_ARRAY,
    NATURAL_TEMPERATURE_ARRAY
} natural_array_domain;

#define NATURAL_JSON_LIMIT (64u * 1024u * 1024u)
#define NATURAL_ARRAY_IDENTITIES 128u
#define NATURAL_DISK_ENTRIES 256u
#define NATURAL_DISK_FILE_BYTES (4u * 1024u * 1024u)
#define NATURAL_DISK_BYTES (8u * 1024u * 1024u)

typedef struct natural_json {
    char *bytes;
    size_t length, capacity;
    bool failed;
} natural_json;

typedef struct natural_persistence {
    const char *root;
    char *store_root;
    mc_world_info *info;
    int64_t session_token, construction_low, construction_high;
    bool construction_low_valid, construction_high_valid, map_ready;
    bool full_world_save, collections_captured;
    const void *collection_owners[8];
    size_t retained_disk_bytes;
    /* The actual per-attempt records are large; keep them off recursive/native
     * default-stack frames and never infer children from ambient last_error. */
    mc_world_runtime_lifecycle_error save_error;
    mc_world_game_load_error load_error;
} natural_persistence;

typedef struct natural_named {
    mc_world_info_name *name, *lookup;
    mc_saved_data_value *values[2];
    size_t value_count;
    mc_world_saved_data *reader;
    const void *references[128];
    const char *reference_classes[128];
    size_t reference_count;
} natural_named;

typedef struct dedicated_observation dedicated_observation;

typedef struct natural_probe {
    bool client, constructor_called, enumeration_checked;
    mc_resource_files *resources;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *registry;
    mc_entity_classes *entities;
    mc_world_runtime_classes *classes;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_info_name *name;
    mc_world_dimension *dimension;
    mc_world_runtime *world;
    mc_world_provider_fields provider;
    mc_world_storage_error storage_error, inspection_error;
    mc_registry_error registry_error;
    mc_world_storage_status constructor_status;
    const char *failed_stage, *failed_domain;
    unsigned int failed_status;
    mc_chunk_state *identities[NATURAL_IDENTITIES];
    bool published[NATURAL_IDENTITIES];
    size_t identity_count;
    const void *arrays[NATURAL_ARRAY_IDENTITIES];
    natural_array_domain array_domains[NATURAL_ARRAY_IDENTITIES];
    size_t array_count;
    mc_biome_double_reference held_climate[NATURAL_ARRAY_IDENTITIES];
    size_t held_climate_count;
    mc_temperature_array *held_temperature[NATURAL_ARRAY_IDENTITIES];
    size_t held_temperature_count;
    uint8_t block_map[256];
    natural_persistence *persistence;
    natural_named *named;
    dedicated_observation *dedicated;
    natural_json json;
} natural_probe;

static bool named_boundary(natural_probe *probe);

static const char *boolean(bool value) {
    return value ? "true" : "false";
}

static bool problem(natural_probe *probe, const char *stage, const char *domain,
                    unsigned int status) {
    if (probe->failed_stage == NULL) {
        probe->failed_stage = stage;
        probe->failed_domain = domain;
        probe->failed_status = status;
    }
    return false;
}

static bool reserve(natural_json *json, size_t extra) {
    if (json->failed || extra > NATURAL_JSON_LIMIT - json->length) {
        json->failed = true;
        return false;
    }
    const size_t needed = json->length + extra + 1u;
    if (needed <= json->capacity)
        return true;
    size_t capacity = json->capacity == 0 ? 4096u : json->capacity;
    while (capacity < needed) {
        if (capacity > NATURAL_JSON_LIMIT / 2u) {
            capacity = NATURAL_JSON_LIMIT + 1u;
            break;
        }
        capacity *= 2u;
    }
    char *bytes = realloc(json->bytes, capacity);
    if (bytes == NULL) {
        json->failed = true;
        return false;
    }
    json->bytes = bytes;
    json->capacity = capacity;
    return true;
}

static bool emit(natural_json *json, const char *format, ...) {
    if (json->failed)
        return false;
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    const int count = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (count < 0 || !reserve(json, (size_t)count)) {
        va_end(args);
        json->failed = true;
        return false;
    }
    const int written =
        vsnprintf(json->bytes + json->length, json->capacity - json->length, format, args);
    va_end(args);
    if (written != count) {
        json->failed = true;
        return false;
    }
    json->length += (size_t)count;
    return true;
}

static bool string(natural_json *json, const char *value) {
    if (value == NULL)
        return emit(json, "null");
    if (!emit(json, "\""))
        return false;
    for (const unsigned char *p = (const unsigned char *)value; *p != 0; ++p) {
        if (*p == '\\' || *p == '"') {
            if (!emit(json, "\\%c", (int)*p))
                return false;
        } else if (*p < 32u || *p > 126u) {
            if (!emit(json, "\\u%04x", (unsigned int)*p))
                return false;
        } else if (!emit(json, "%c", (int)*p)) {
            return false;
        }
    }
    return emit(json, "\"");
}

static uint64_t double_bits(double value) {
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 projection");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static char *decode_path(const char *hex) {
    const size_t length = strlen(hex);
    if (length == 0 || length % 2u != 0 || length / 2u > MC_WS_PATH_MAX_BYTES)
        return NULL;
    char *path = malloc(length / 2u + 1u);
    if (path == NULL)
        return NULL;
    for (size_t i = 0; i < length / 2u; ++i) {
        unsigned int value = 0;
        for (size_t j = 0; j < 2; ++j) {
            const unsigned char c = (unsigned char)hex[2u * i + j];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                free(path);
                return NULL;
            }
            value =
                value * 16u + (c <= '9' ? (unsigned int)(c - '0') : (unsigned int)(c - 'a') + 10u);
        }
        if (value == 0) {
            free(path);
            return NULL;
        }
        path[i] = (char)(unsigned char)value;
    }
    path[length / 2u] = '\0';
    return path;
}

static bool storage(natural_json *json, const mc_world_storage_error *error) {
    return emit(json, "{\"status\":%u,\"status_name\":", (unsigned int)error->status) &&
           string(json, mc_world_storage_status_string(error->status)) &&
           emit(json, ",\"operation\":") && string(json, error->operation) &&
           emit(json, ",\"system_error\":%" PRIu32 "}", error->system_error);
}

static bool registry_diagnostic(natural_json *json, const mc_registry_error *error) {
    if (error == NULL)
        return emit(json, "null");
    return emit(json, "{\"status\":%u,\"message\":", (unsigned int)error->status) &&
           string(json, error->message) &&
           emit(json,
                ",\"initializer_pc\":%" PRId32 ",\"block_id\":%" PRId32 ",\"constructor_class\":",
                error->initializer_pc, error->block_id) &&
           string(json, error->constructor_class) &&
           emit(json,
                ",\"constructor_pc\":%" PRId32
                ",\"profile_status\":%u,\"active_initializer_class\":",
                error->constructor_pc, (unsigned int)error->profile_status) &&
           string(json, error->active_initializer_class) &&
           emit(json,
                ",\"active_initializer_pc\":%" PRId32 ",\"item_argument\":%" PRId32
                ",\"format\":{\"status\":%u,\"offset\":%zu,\"length\":%zu,\"value\":%" PRId32
                ",\"flags\":%" PRIu32 ",\"conversion\":%u,\"date_time\":%s}}",
                error->active_initializer_pc, error->item_argument,
                (unsigned int)error->format_error.status, error->format_error.offset,
                error->format_error.length, error->format_error.value, error->format_error.flags,
                (unsigned int)error->format_error.conversion,
                boolean(error->format_error.date_time));
}

static bool diagnostic(natural_probe *probe) {
    natural_json *json = &probe->json;
    if (probe->failed_stage == NULL)
        return emit(json, "null");
    if (!emit(json, "{\"stage\":") || !string(json, probe->failed_stage) ||
        !emit(json, ",\"status_domain\":") || !string(json, probe->failed_domain) ||
        !emit(json, ",\"native_status\":%u,\"last_storage_call\":", probe->failed_status) ||
        !storage(json, &probe->storage_error) || !emit(json, ",\"registry_call\":") ||
        !registry_diagnostic(json, &probe->registry_error) || !emit(json, ",\"world_last_error\":"))
        return false;
    const mc_world_storage_error *last = mc_world_runtime_last_error(probe->world);
    if (!(last == NULL ? emit(json, "null") : storage(json, last)))
        return false;
    if (!emit(json, ",\"world_last_registry\":") ||
        !registry_diagnostic(json, mc_world_runtime_last_registry_error(probe->world)))
        return false;
    if (probe->persistence != NULL && (!emit(json, ",\"last_observation_storage_call\":") ||
                                       !storage(json, &probe->inspection_error)))
        return false;
    if (!emit(json, ",\"construction\":"))
        return false;
    if (probe->world == NULL)
        return emit(json, "null}");
    mc_world_runtime_construction state;
    if (mc_world_runtime_construction_get(probe->world, &state) != MC_WORLD_STORAGE_OK)
        return false;
    return emit(
        json,
        "{\"phase\":%u,\"entry\":%u,\"last_store_pc\":%u,\"failed_call_pc\":%u,"
        "\"contexts_bound\":%s,\"cache_registered\":%s,\"random_status\":%u,"
        "\"clock_status\":%" PRId32 ",\"constructor_called\":%s,\"constructor_status\":%u}}",
        (unsigned int)state.phase, (unsigned int)state.entry, state.last_store_pc,
        state.failed_call_pc, boolean(state.contexts_bound), boolean(state.cache_registered),
        (unsigned int)state.random_status, state.clock_status, boolean(probe->constructor_called),
        (unsigned int)probe->constructor_status);
}

static size_t chunk_id(natural_probe *probe, mc_chunk_state *state) {
    if (state == NULL)
        return SIZE_MAX;
    for (size_t i = 0; i < probe->identity_count; ++i)
        if (probe->identities[i] == state)
            return i;
    if (probe->identity_count == NATURAL_IDENTITIES) {
        problem(probe, "Chunk identity projection", "probe", 1);
        return SIZE_MAX;
    }
    const size_t id = probe->identity_count++;
    probe->identities[id] = state;
    return id;
}

static bool track_chunks(natural_probe *probe) {
    mc_chunk_cache *cache = probe->provider.cache;
    if (cache == NULL || !mc_chunk_cache_validate(cache) ||
        !mc_chunk_pending_validate(probe->provider.pending) || cache->count > NATURAL_CAPACITY ||
        cache->insertion_count > NATURAL_CAPACITY)
        return problem(probe, "Cache projection metadata", "probe", 1);
    if (probe->identity_count == 0 && chunk_id(probe, probe->provider.fallback) != 0)
        return false;
    memset(probe->published, 0, sizeof(probe->published));
    const size_t fallback = chunk_id(probe, probe->provider.fallback);
    if (fallback == SIZE_MAX)
        return false;
    probe->published[fallback] = true;
    for (size_t i = 0; i < cache->insertion_count; ++i) {
        const size_t id = chunk_id(probe, cache->insertions[i]);
        if (id == SIZE_MAX)
            return false;
        probe->published[id] = true;
    }
    for (size_t i = 0; i < cache->count; ++i) {
        const size_t id = chunk_id(probe, cache->entries[i].chunk);
        if (id == SIZE_MAX)
            return false;
        probe->published[id] = true;
    }
    const size_t count = mc_world_store_chunk_count(probe->store);
    if (count > NATURAL_CAPACITY)
        return problem(probe, "Store projection capacity", "probe", 1);
    mc_chunk_state *enumerated[NATURAL_CAPACITY];
    for (size_t i = 0; i < count; ++i) {
        mc_chunk_state *state = NULL;
        if (mc_world_store_chunk_at(probe->store, i, &state, &probe->inspection_error) !=
                MC_WORLD_STORAGE_OK ||
            state == NULL || !mc_world_store_owns_chunk(probe->store, state) ||
            chunk_id(probe, state) == SIZE_MAX)
            return problem(probe, "Store identity projection", "storage",
                           (unsigned int)probe->inspection_error.status);
        for (size_t j = 0; j < i; ++j)
            if (enumerated[j] == state)
                return problem(probe, "Repeated Store enumeration identity", "probe", 1);
        enumerated[i] = state;
    }
    if (mc_world_store_chunk_count(probe->store) != count)
        return problem(probe, "Store enumeration count changed", "probe", 1);
    if (!probe->enumeration_checked && count != 0) {
        mc_chunk_state *sentinel = enumerated[0];
        if (mc_world_store_chunk_at(probe->store, count, &sentinel, &probe->inspection_error) !=
                MC_WORLD_STORAGE_INVALID_ARGUMENT ||
            sentinel != enumerated[0] ||
            mc_world_store_chunk_at(NULL, 0, &sentinel, &probe->inspection_error) !=
                MC_WORLD_STORAGE_INVALID_ARGUMENT ||
            sentinel != enumerated[0] ||
            mc_world_store_chunk_at(probe->store, 0, NULL, &probe->inspection_error) !=
                MC_WORLD_STORAGE_INVALID_ARGUMENT ||
            sentinel != enumerated[0] || mc_world_store_chunk_count(probe->store) != count)
            return problem(probe, "Store enumeration failure contract", "probe", 1);
        probe->enumeration_checked = true;
    }
    return true;
}

static bool byte_array(natural_json *json, const uint8_t *bytes, size_t length) {
    if (bytes == NULL || !emit(json, "{\"length\":%zu,\"values\":\"", length))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (!emit(json, "%02x", (unsigned int)bytes[i]))
            return false;
    return emit(json, "\"}");
}

static bool chunk_data(natural_probe *probe, mc_chunk_state *state) {
    const mc_chunk_data *data = state->data;
    if (data == NULL || data->blocks == NULL || data->blocks_capacity < MC_CHUNK_BLOCKS)
        return problem(probe, "Chunk data projection", "probe", 1);
    natural_json *json = &probe->json;
    return emit(json, "{\"blocks\":") && byte_array(json, data->blocks, MC_CHUNK_BLOCKS) &&
           emit(json, ",\"metadata\":") &&
           byte_array(json, data->metadata, sizeof(data->metadata)) &&
           emit(json, ",\"skylight\":") &&
           byte_array(json, data->skylight, sizeof(data->skylight)) &&
           emit(json, ",\"block_light\":") &&
           byte_array(json, data->block_light, sizeof(data->block_light)) &&
           emit(json, ",\"heightmap\":") &&
           byte_array(json, data->heightmap, sizeof(data->heightmap)) &&
           emit(json, ",\"minimum_height\":%" PRId32 "}", data->minimum_height);
}

static bool chunk_projection(natural_probe *probe, mc_chunk_state *state, size_t id) {
    natural_json *json = &probe->json;
    return emit(json,
                "{\"id\":%zu,\"coordinates\":[%" PRId32 ",%" PRId32
                "],\"blank\":%s,\"flags\":{\"c\":%s,\"n\":%s,\"o\":%s,\"p\":%s,\"q\":%s},"
                "\"time\":\"%016" PRIx64 "\",\"data\":",
                id, state->x, state->z, boolean(state->blank), boolean(state->active),
                boolean(state->terrain_populated), boolean(state->dirty),
                boolean(state->never_save), boolean(state->has_entities),
                (uint64_t)state->last_save_time) &&
           (state->blank ? emit(json, "null") : chunk_data(probe, state)) && emit(json, "}");
}

static size_t array_id(natural_probe *probe, const void *array, natural_array_domain domain) {
    for (size_t i = 0; i < probe->array_count; ++i)
        if (probe->arrays[i] == array && probe->array_domains[i] == domain)
            return i;
    if (probe->array_count == NATURAL_ARRAY_IDENTITIES) {
        problem(probe, "Array identity projection capacity", "probe", 1);
        return SIZE_MAX;
    }
    const size_t id = probe->array_count++;
    probe->arrays[id] = array;
    probe->array_domains[id] = domain;
    return id;
}

static bool doubles(natural_probe *probe, const double *values, size_t length, const void *identity,
                    natural_array_domain domain) {
    natural_json *json = &probe->json;
    if (identity == NULL)
        return length == 0 && emit(json, "null");
    if (values == NULL || length > 4096)
        return problem(probe, "Double array projection", "probe", 1);
    const size_t id = array_id(probe, identity, domain);
    if (id == SIZE_MAX || !emit(json, "{\"identity\":%zu,\"length\":%zu,\"values\":\"", id, length))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (!emit(json, "%016" PRIx64, double_bits(values[i])))
            return false;
    return emit(json, "\"}");
}

static bool biomes(natural_probe *probe, const mc_biome_reference_array *array,
                   const mc_biome_registry_view *statics) {
    natural_json *json = &probe->json;
    if (array == NULL)
        return emit(json, "null");
    const mc_biome *const declarations[] = {
        statics->a, statics->b, statics->c, statics->d, statics->e, statics->f, statics->g,
        statics->h, statics->i, statics->j, statics->k, statics->l, statics->m};
    const size_t length = mc_biome_reference_array_length(array);
    const mc_biome *const *values = mc_biome_reference_array_data(array);
    const size_t id = array_id(probe, array, NATURAL_BIOME_ARRAY);
    if (id == SIZE_MAX || values == NULL || length > 4096 ||
        !emit(json, "{\"identity\":%zu,\"length\":%zu,\"values\":[", id, length))
        return false;
    for (size_t i = 0; i < length; ++i) {
        if (!emit(json, "%s", i == 0 ? "" : ","))
            return false;
        if (values[i] == NULL) {
            if (!emit(json, "null"))
                return false;
            continue;
        }
        size_t label = 0;
        while (label < sizeof(declarations) / sizeof(declarations[0]) &&
               values[i] != declarations[label])
            ++label;
        if (label == sizeof(declarations) / sizeof(declarations[0]))
            return problem(probe, "Biome reference has no declared static label", "probe", 1);
        if (!emit(json, "\"%c\"", (int)('a' + label)))
            return false;
    }
    return emit(json, "]}");
}

static bool random_state(natural_json *json, const mc_java_random *random) {
    if (random == NULL)
        return emit(json, "null");
    if (!emit(json,
              "{\"state\":\"%012" PRIx64 "\",\"have_next_next_gaussian\":%s,"
              "\"next_next_gaussian_bits\":\"%016" PRIx64 "\",\"next_longs\":[",
              random->state, boolean(random->have_next_next_gaussian),
              double_bits(random->next_next_gaussian)))
        return false;
    mc_java_random continuation = *random;
    for (size_t i = 0; i < 8u; ++i)
        if (!emit(json, "%s\"%016" PRIx64 "\"", i == 0 ? "" : ",",
                  (uint64_t)mc_java_random_next_i64(&continuation)))
            return false;
    return emit(json, "]}");
}

static bool hold_climate(natural_probe *probe, const mc_biome_double_buffer *buffer) {
    if (buffer->data == NULL)
        return buffer->length == 0;
    for (size_t i = 0; i < probe->held_climate_count; ++i)
        if (probe->held_climate[i].data == buffer->data)
            return true;
    if (probe->held_climate_count == NATURAL_ARRAY_IDENTITIES)
        return problem(probe, "Captured climate owner capacity", "probe", 1);
    const mc_biome_status status =
        mc_biome_double_capture(buffer, &probe->held_climate[probe->held_climate_count]);
    if (status != MC_BIOME_OK)
        return problem(probe, "Captured climate owner", "biome", (unsigned int)status);
    ++probe->held_climate_count;
    return true;
}

static bool hold_temperature(natural_probe *probe, mc_temperature_array *array) {
    for (size_t i = 0; i < probe->held_temperature_count; ++i)
        if (probe->held_temperature[i] == array)
            return true;
    if (probe->held_temperature_count == NATURAL_ARRAY_IDENTITIES)
        return problem(probe, "Captured temperature owner capacity", "probe", 1);
    const mc_biome_status status = mc_temperature_array_retain(array);
    if (status != MC_BIOME_OK)
        return problem(probe, "Captured temperature owner", "biome", (unsigned int)status);
    probe->held_temperature[probe->held_temperature_count++] = array;
    return true;
}

static bool provider_snapshot(natural_probe *probe, const mc_java_random *world_random) {
    natural_json *json = &probe->json;
    const mc_world_provider_fields *provider = &probe->provider;
    const mc_world_provider_generation_fields *generation = provider->generation;
    if (provider->generator == NULL || generation == NULL || provider->biome == NULL)
        return problem(probe, "Provider projection binding", "probe", 1);
    mc_biome_registry_view statics;
    mc_world_biome_game_fields actual;
    mc_world_dimension_biome_fields climate;
    if (mc_biome_registry_inspect(probe->registry, &statics, NULL) != MC_REGISTRY_OK ||
        mc_world_dimension_biome_game_fields_get(provider->biome, &actual) !=
            MC_WORLD_DIMENSION_OK ||
        mc_world_dimension_biome_fields_get(provider->biome, &climate) != MC_WORLD_DIMENSION_OK)
        return problem(probe, "Provider Biome projection", "probe", 1);
    if (!emit(json, "{\"generator_random\":") ||
        !random_state(json, &provider->generator->generator_random) ||
        !emit(json, ",\"cave_random\":") ||
        !random_state(json, &provider->generator->cave_random) ||
        !emit(json, ",\"world_random\":") || !random_state(json, world_random) ||
        !emit(json, ",\"generation\":{"))
        return false;
    const char *names[] = {"q", "d", "e", "f", "g", "h", "r", "s", "t"};
    const mc_terrain_double_span *spans[] = {&generation->q, &generation->d, &generation->e,
                                             &generation->f, &generation->g, &generation->h,
                                             &generation->r, &generation->s, &generation->t};
    for (size_t i = 0; i < sizeof(spans) / sizeof(spans[0]); ++i)
        if (!emit(json, "%s\"%s\":", i == 0 ? "" : ",", names[i]) ||
            !doubles(probe, spans[i]->data, spans[i]->length, spans[i]->data, NATURAL_DOUBLE_ARRAY))
            return false;
    if (!emit(json, "},\"v\":") || !biomes(probe, generation->v, &statics) ||
        !emit(json, ",\"manager\":{"))
        return false;
    const mc_biome_double_buffer *buffers[] = {climate.a, climate.b, climate.c};
    for (size_t i = 0; i < sizeof(buffers) / sizeof(buffers[0]); ++i) {
        const mc_biome_double_buffer *buffer = buffers[i];
        if (buffer == NULL || !hold_climate(probe, buffer) ||
            !emit(json, "%s\"%c\":", i == 0 ? "" : ",", (int)('a' + i)) ||
            !doubles(probe, buffer->data, buffer->length, buffer->data, NATURAL_DOUBLE_ARRAY))
            return false;
    }
    if (!emit(json, ",\"d\":") || !biomes(probe, actual.d, &statics) || !emit(json, "},\"w\":"))
        return false;
    if (provider->w == NULL)
        return emit(json, "null}");
    mc_temperature_array_view temperature;
    if (!hold_temperature(probe, provider->w) ||
        mc_temperature_array_get(provider->w, &temperature) != MC_BIOME_OK ||
        !doubles(probe, temperature.data, temperature.length, provider->w,
                 NATURAL_TEMPERATURE_ARRAY))
        return false;
    return emit(json, "}");
}

static bool snapshot(natural_probe *probe) {
    mc_world_runtime_fields fields;
    mc_world_runtime_class_state class_state;
    mc_world_dimension_fields dimension;
    mc_level_info info;
    if (mc_world_runtime_fields_get(probe->world, &fields) != MC_WORLD_STORAGE_OK ||
        mc_world_provider_bind_fields(fields.v, &probe->provider, &probe->inspection_error) !=
            MC_WORLD_STORAGE_OK ||
        mc_world_runtime_classes_get(probe->classes, &class_state) != MC_WORLD_STORAGE_OK ||
        mc_world_dimension_fields_get(fields.t, &dimension) != MC_WORLD_DIMENSION_OK ||
        mc_world_runtime_read_info(probe->world, &info, &probe->inspection_error) !=
            MC_WORLD_STORAGE_OK ||
        !track_chunks(probe))
        return problem(probe, "Snapshot binding", "probe", 1);
    natural_json *json = &probe->json;
    mc_chunk_cache *cache = probe->provider.cache;
    if (!emit(json,
              "{\"context\":{\"dedicated\":false,\"world_time\":\"%016" PRIx64
              "\",\"world_force\":%s,\"provider_force\":null,\"spawn\":[%" PRId32 ",%" PRId32
              "],\"no_sky\":%s},\"world\":{\"map\":[",
              (uint64_t)info.time, boolean(fields.y), info.spawn_x, info.spawn_z,
              boolean(dimension.e)))
        return false;
    size_t order[NATURAL_CAPACITY];
    for (size_t i = 0; i < cache->count; ++i)
        order[i] = i;
    for (size_t i = 1; i < cache->count; ++i) {
        size_t j = i;
        while (j != 0 && cache->entries[order[j - 1u]].key > cache->entries[order[j]].key) {
            const size_t temporary = order[j];
            order[j] = order[j - 1u];
            order[--j] = temporary;
        }
    }
    for (size_t i = 0; i < cache->count; ++i)
        if (!emit(json, "%s[%" PRId32 ",%zu]", i == 0 ? "" : ",", cache->entries[order[i]].key,
                  chunk_id(probe, cache->entries[order[i]].chunk)))
            return false;
    if (!emit(json, "],\"insertions\":["))
        return false;
    for (size_t i = 0; i < cache->insertion_count; ++i)
        if (!emit(json, "%s%zu", i == 0 ? "" : ",", chunk_id(probe, cache->insertions[i])))
            return false;
    if (!emit(json, "],\"pending\":["))
        return false;
    const mc_chunk_pending *pending = probe->provider.pending;
    for (size_t i = 0; i < pending->count; ++i)
        if (!emit(json, "%s%" PRId32, i == 0 ? "" : ",", pending->keys[i]))
            return false;
    if (!emit(json, "],\"chunks\":["))
        return false;
    size_t emitted = 0;
    for (size_t i = 0; i < probe->identity_count; ++i) {
        /* Raw loader results remain real Store owners without becoming cache
         * members. Enumeration and terminal retirement still include them. */
        if (probe->persistence != NULL && !probe->published[i])
            continue;
        if (!emit(json, "%s", emitted++ == 0 ? "" : ",") ||
            !chunk_projection(probe, probe->identities[i], i))
            return false;
    }
    mc_world_light_list_view lights;
    if (mc_world_runtime_light_list_get(probe->world, &lights, &probe->inspection_error) !=
            MC_WORLD_STORAGE_OK ||
        lights.count > MC_LIGHT_QUEUE_LIMIT || !emit(json, "],\"queue\":["))
        return false;
    for (size_t i = 0; i < lights.count; ++i) {
        const mc_world_light_region *region = NULL;
        if (mc_world_runtime_light_at(probe->world, i, &region, &probe->inspection_error) !=
            MC_WORLD_STORAGE_OK)
            return false;
        const mc_world_light_region_fields *item = mc_world_runtime_light_region_fields_get(region);
        if (item == NULL)
            return false;
        const mc_world_light_type_fields *type = mc_world_runtime_light_type_fields_get(item->a);
        const mc_light_bounds *b = &item->bounds;
        if (type == NULL ||
            !emit(json,
                  "%s[%u,%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "]",
                  i == 0 ? "" : ",", (unsigned int)type->kind, b->min_x, b->min_y, b->min_z,
                  b->max_x, b->max_y, b->max_z))
            return false;
    }
    return emit(json, "],\"depth\":%" PRId32 "},\"provider\":", class_state.enqueue_depth) &&
           provider_snapshot(probe, fields.r) &&
           emit(json,
                ",\"shared\":{\"drain_depth\":%" PRId32
                ",\"chunk_positive_sky\":%s,\"falling\":%s}}",
                fields.M, boolean(probe->classes->chunk_a), boolean(probe->registry->gk_a));
}

#define NATURAL_REQUIRE(probe, expression, expected, domain, stage)                                \
    do {                                                                                           \
        const unsigned int status_ = (unsigned int)(expression);                                   \
        if (status_ != (unsigned int)(expected))                                                   \
            return problem(probe, stage, domain, status_);                                         \
    } while (0)

static bool initialize(natural_probe *probe, const char *resources, const char *root,
                       const mc_math_trig *trig) {
    NATURAL_REQUIRE(probe, mc_resource_files_create(resources, &probe->resources), MC_REGISTRY_OK,
                    "registry", "Resource source");
    NATURAL_REQUIRE(probe, mc_materials_create(&probe->materials), true, "probe", "Materials");
    NATURAL_REQUIRE(probe, mc_java_random_profile_create(SIZE_MAX, &probe->profile),
                    MC_JAVA_RANDOM_PROFILE_OK, "random_profile", "Application Random profile");
    const mc_registry_options options = {.side = probe->client ? MC_REGISTRY_CLIENT
                                                               : MC_REGISTRY_SERVER,
                                         .random_profile = probe->profile,
                                         .materials = probe->materials,
                                         .resources = mc_resource_files_source(probe->resources)};
    NATURAL_REQUIRE(
        probe, mc_registry_classes_allocate(&options, &probe->registry, &probe->registry_error),
        MC_REGISTRY_OK, "registry", "Registry allocation");
    NATURAL_REQUIRE(probe,
                    mc_entity_classes_create(probe->client ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER,
                                             probe->profile, NULL, &probe->entities),
                    MC_ENTITY_OK, "entity", "Entity class binding");
    NATURAL_REQUIRE(probe,
                    mc_world_runtime_classes_create(
                        probe->client ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, probe->profile,
                        trig, &probe->classes, &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "World class binding");
    NATURAL_REQUIRE(probe,
                    mc_world_runtime_classes_bind_registry(probe->classes, probe->registry,
                                                           &probe->registry_error),
                    MC_REGISTRY_OK, "registry", "World Registry binding");
    NATURAL_REQUIRE(probe, mc_world_runtime_classes_bind_entities(probe->classes, probe->entities),
                    MC_ENTITY_OK, "entity", "World Entity binding");
    /* The original observation initializes uu/na and kd/gs before constructing
     * its World. These are real class active uses with the pinned resources. */
    NATURAL_REQUIRE(probe,
                    mc_registry_classes_initialize_blocks(probe->registry, &probe->registry_error),
                    MC_REGISTRY_OK, "registry", "Block active use");
    NATURAL_REQUIRE(probe,
                    mc_registry_classes_initialize_biomes(probe->registry, &probe->registry_error),
                    MC_REGISTRY_OK, "registry", "Biome active use");
    bool exists = true;
    NATURAL_REQUIRE(probe, mc_ws_path_exists(root, &exists, &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "Fresh World root admission");
    if (exists)
        return problem(probe, "World root already exists", "probe", 1);
    NATURAL_REQUIRE(probe, mc_world_regions_create(NULL, &probe->regions, &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "Region owner");
    const char *store_root = root;
    if (probe->persistence != NULL) {
        mc_registry_block_view blocks;
        NATURAL_REQUIRE(
            probe,
            mc_registry_classes_inspect_blocks(probe->registry, &blocks, &probe->registry_error),
            MC_REGISTRY_OK, "registry", "Block-map registration view");
        if (blocks.phase != MC_REGISTRY_READY || blocks.m == NULL)
            return problem(probe, "Block-map registration is not READY", "probe", 1);
        probe->block_map[0] = 0;
        for (size_t i = 1; i < sizeof(probe->block_map); ++i)
            probe->block_map[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
        probe->persistence->map_ready = true;
        NATURAL_REQUIRE(
            probe,
            mc_ws_path_join(root, "world", &probe->persistence->store_root, &probe->storage_error),
            MC_WORLD_STORAGE_OK, "storage", "Persistence Store root binding");
        store_root = probe->persistence->store_root;
    } else {
        for (size_t i = 0; i < sizeof(probe->block_map); ++i)
            probe->block_map[i] = (uint8_t)i;
    }
    int64_t token;
    NATURAL_REQUIRE(probe, mc_platform_current_time_millis(&token), MC_PLATFORM_MILLIS_OK, "clock",
                    "Store session clock");
    if (probe->persistence != NULL)
        probe->persistence->session_token = token;
    mc_world_store_options store = {.session_token = token,
                                    .block_map = probe->block_map,
                                    .block_map_length = sizeof(probe->block_map),
                                    .limits = mc_world_store_default_limits()};
    store.limits.max_loaded_chunks = NATURAL_CAPACITY;
    NATURAL_REQUIRE(probe,
                    mc_world_store_open(store_root, probe->regions, &store, &probe->store,
                                        &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "Fresh Store");
    static const char title[] = "cache-callback-reference";
    uint16_t units[sizeof(title) - 1u];
    for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); ++i)
        units[i] = (uint16_t)(unsigned char)title[i];
    NATURAL_REQUIRE(
        probe,
        mc_world_info_name_copy((mc_nbt_string){units, sizeof(units) / sizeof(units[0])}, SIZE_MAX,
                                &probe->name),
        MC_WORLD_INFO_OK, "info", "World name");
    if (!probe->client)
        NATURAL_REQUIRE(probe, mc_world_dimension_create(0, NULL, &probe->dimension, NULL),
                        MC_WORLD_DIMENSION_OK, "dimension", "Server normal Dimension");
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = NATURAL_CAPACITY;
    NATURAL_REQUIRE(probe,
                    mc_world_runtime_allocate_world(
                        probe->client ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                      : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE,
                        probe->classes, &limits, &probe->world, &probe->storage_error),
                    MC_WORLD_STORAGE_OK, "storage", "World allocation");
    const mc_world_runtime_world_arguments arguments = {
        .store = probe->store, .name = probe->name, .seed = 23, .dimension = probe->dimension};
    probe->constructor_called = true;
    probe->constructor_status =
        mc_world_runtime_construct_world(probe->world, &arguments, &probe->storage_error);
    if (probe->constructor_status != MC_WORLD_STORAGE_OK)
        return problem(probe, "Fresh World constructor", "storage",
                       (unsigned int)probe->constructor_status);
    return true;
}

static bool observe(natural_probe *probe, bool initialized, bool ring) {
    natural_json *json = &probe->json;
    if (!emit(json,
              "{\"schema\":1,\"scope\":\"%s\",\"side\":\"%s\","
              "\"seed_hex\":\"0000000000000017\",%s\"initial\":",
              ring ? "native-game-normal-population-ring" : "native-game-normal-population",
              probe->client ? "client" : "server", ring ? "\"profile\":\"ring\"," : ""))
        return false;
    if (!(initialized ? snapshot(probe) : emit(json, "null")) || !emit(json, ",\"operations\":["))
        return false;
    /* The origin profile is the unchanged first four calls. */
    static const int32_t coordinates[][2] = {{1, 0},  {0, 1},  {1, 1},  {1, 1},   {-1, 0},
                                             {-1, 1}, {0, -1}, {1, -1}, {-1, -1}, {-1, -1}};
    const size_t operation_count = ring ? sizeof(coordinates) / sizeof(coordinates[0]) : 4u;
    if (initialized && (probe->persistence == NULL || probe->failed_stage == NULL)) {
        for (size_t i = 0; i < operation_count; ++i) {
            mc_chunk_state *result = NULL;
            const mc_chunk_acquire_status status = mc_world_runtime_acquire(
                probe->world, coordinates[i][0], coordinates[i][1], &result);
            if (status != MC_CHUNK_ACQUIRE_OK)
                problem(probe, "Cache c acquisition", "acquisition", (unsigned int)status);
            if (!emit(json,
                      "%s{\"index\":%zu,\"method\":\"c\",\"coordinates\":[%" PRId32 ",%" PRId32
                      "],\"result\":",
                      i == 0 ? "" : ",", i, coordinates[i][0], coordinates[i][1]))
                return false;
            if (result == NULL) {
                if (!emit(json, "null"))
                    return false;
            } else {
                /* Assign new normal IDs by actual insertion order first. */
                mc_world_runtime_fields fields;
                if (mc_world_runtime_fields_get(probe->world, &fields) != MC_WORLD_STORAGE_OK ||
                    mc_world_provider_bind_fields(fields.v, &probe->provider,
                                                  &probe->inspection_error) !=
                        MC_WORLD_STORAGE_OK ||
                    !track_chunks(probe) || !emit(json, "%zu", chunk_id(probe, result)))
                    return false;
            }
            if (!emit(json, ",\"status\":") ||
                !string(json, mc_chunk_acquire_status_string(status)) ||
                !emit(json, ",\"snapshot\":") || !snapshot(probe) || !emit(json, "}"))
                return false;
            if (status != MC_CHUNK_ACQUIRE_OK)
                break;
        }
    }
    return emit(json, "],\"failure\":") && diagnostic(probe) && emit(json, "}\n");
}

typedef struct natural_disk_stamp {
    bool directory;
    uint64_t bytes, modified, changed, identity_a, identity_b;
    uint32_t attributes;
} natural_disk_stamp;

typedef struct natural_disk_entry {
    char *path;
    natural_disk_stamp stamp;
} natural_disk_entry;

typedef struct natural_disk_inventory {
    natural_disk_entry entries[NATURAL_DISK_ENTRIES];
    size_t count, files, directories, bytes;
} natural_disk_inventory;

static bool disk_system_error(natural_probe *probe, const char *operation, uint32_t code) {
    probe->inspection_error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, operation, code};
    return problem(probe, operation, "disk-system", code);
}

#ifdef _WIN32
static wchar_t *disk_wide(natural_probe *probe, const char *path) {
    const size_t bytes = strlen(path);
    if (bytes > MC_WS_PATH_MAX_BYTES) {
        problem(probe, "Disk UTF-8 path bound", "probe", 1);
        return NULL;
    }
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (count <= 0) {
        disk_system_error(probe, "Disk strict UTF-8 conversion", (uint32_t)GetLastError());
        return NULL;
    }
    if (count > 32768) {
        problem(probe, "Disk UTF-16 path bound", "probe", 1);
        return NULL;
    }
    wchar_t *wide = malloc((size_t)count * sizeof(*wide));
    if (wide == NULL) {
        problem(probe, "Disk wide path allocation", "probe", 1);
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, count) != count) {
        disk_system_error(probe, "Disk strict UTF-8 conversion", (uint32_t)GetLastError());
        free(wide);
        return NULL;
    }
    return wide;
}
#endif

static bool disk_stat(natural_probe *probe, const char *path, natural_disk_stamp *out) {
    natural_disk_stamp stamp = {0};
#ifdef _WIN32
    wchar_t *wide = disk_wide(probe, path);
    if (wide == NULL)
        return false;
    WIN32_FILE_ATTRIBUTE_DATA data;
    const BOOL found = GetFileAttributesExW(wide, GetFileExInfoStandard, &data);
    const DWORD error = found ? ERROR_SUCCESS : GetLastError();
    free(wide);
    if (!found)
        return disk_system_error(probe, "Disk GetFileAttributesExW", (uint32_t)error);
    if ((data.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE)) != 0)
        return problem(probe, "Disk reparse/device entry rejected", "probe", 1);
    stamp.directory = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    stamp.bytes = ((uint64_t)data.nFileSizeHigh << 32u) | data.nFileSizeLow;
    stamp.modified =
        ((uint64_t)data.ftLastWriteTime.dwHighDateTime << 32u) | data.ftLastWriteTime.dwLowDateTime;
    stamp.changed =
        ((uint64_t)data.ftCreationTime.dwHighDateTime << 32u) | data.ftCreationTime.dwLowDateTime;
    stamp.attributes = (uint32_t)data.dwFileAttributes;
#else
    struct stat data;
    if (lstat(path, &data) != 0)
        return disk_system_error(probe, "Disk lstat", (uint32_t)errno);
    if (!S_ISDIR(data.st_mode) && !S_ISREG(data.st_mode))
        return problem(probe, "Disk symlink/special entry rejected", "probe", 1);
    if (data.st_size < 0)
        return problem(probe, "Disk negative file size", "probe", 1);
    stamp.directory = S_ISDIR(data.st_mode);
    stamp.bytes = (uint64_t)data.st_size;
    stamp.modified = (uint64_t)data.st_mtim.tv_sec;
    stamp.changed = (uint64_t)data.st_ctim.tv_sec;
    stamp.identity_a = (uint64_t)data.st_dev;
    stamp.identity_b = (uint64_t)data.st_ino;
    stamp.attributes = (uint32_t)data.st_mode;
#endif
    *out = stamp;
    return true;
}

static bool disk_same(const natural_disk_stamp *a, const natural_disk_stamp *b) {
    return a->directory == b->directory && a->bytes == b->bytes && a->modified == b->modified &&
           a->changed == b->changed && a->identity_a == b->identity_a &&
           a->identity_b == b->identity_b && a->attributes == b->attributes;
}

static bool disk_component(const char *name) {
    if (name[0] == '\0' || strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return false;
    /* Game-created names in this finite profile are ASCII. This explicit
     * admission avoids locale conversion of arbitrary external directory data. */
    for (const unsigned char *p = (const unsigned char *)name; *p != 0; ++p)
        if (*p < 32u || *p > 126u || *p == '/' || *p == '\\' || *p == ':')
            return false;
    return true;
}

static bool disk_add(natural_probe *probe, natural_disk_inventory *inventory, const char *parent,
                     const char *name) {
    if (inventory->count == NATURAL_DISK_ENTRIES || !disk_component(name))
        return problem(probe, "Disk entry/name bound", "probe", 1);
    const size_t parent_length = strcmp(parent, ".") == 0 ? 0 : strlen(parent);
    const size_t name_length = strlen(name);
    if (parent_length + name_length + 1u > MC_WS_PATH_MAX_BYTES)
        return problem(probe, "Disk relative path bound", "probe", 1);
    char *relative = malloc(parent_length + name_length + (parent_length == 0 ? 1u : 2u));
    if (relative == NULL)
        return problem(probe, "Disk relative path allocation", "probe", 1);
    if (parent_length == 0)
        memcpy(relative, name, name_length + 1u);
    else {
        memcpy(relative, parent, parent_length);
        relative[parent_length] = '/';
        memcpy(relative + parent_length + 1u, name, name_length + 1u);
    }
    char *absolute = NULL;
    natural_disk_stamp stamp = {0};
    const bool ok = mc_ws_path_join(probe->persistence->root, relative, &absolute,
                                    &probe->inspection_error) == MC_WORLD_STORAGE_OK &&
                    disk_stat(probe, absolute, &stamp);
    free(absolute);
    if (!ok) {
        free(relative);
        return problem(probe, "Disk entry inspection", "storage",
                       (unsigned int)probe->inspection_error.status);
    }
    if (!stamp.directory && (stamp.bytes > NATURAL_DISK_FILE_BYTES ||
                             stamp.bytes > NATURAL_DISK_BYTES - inventory->bytes)) {
        free(relative);
        return problem(probe, "Disk file/boundary byte bound", "probe", 1);
    }
    inventory->entries[inventory->count++] = (natural_disk_entry){relative, stamp};
    if (stamp.directory)
        ++inventory->directories;
    else {
        ++inventory->files;
        inventory->bytes += (size_t)stamp.bytes;
    }
    return true;
}

static bool disk_children(natural_probe *probe, natural_disk_inventory *inventory,
                          const char *relative) {
    char *absolute = NULL;
    if (mc_ws_path_join(probe->persistence->root, relative, &absolute, &probe->inspection_error) !=
        MC_WORLD_STORAGE_OK)
        return problem(probe, "Disk directory path", "storage",
                       (unsigned int)probe->inspection_error.status);
    bool ok = true;
#ifdef _WIN32
    char *pattern = NULL;
    if (mc_ws_path_join(absolute, "*", &pattern, &probe->inspection_error) != MC_WORLD_STORAGE_OK) {
        free(absolute);
        return problem(probe, "Disk search path", "storage",
                       (unsigned int)probe->inspection_error.status);
    }
    wchar_t *wide = disk_wide(probe, pattern);
    free(pattern);
    free(absolute);
    if (wide == NULL)
        return false;
    WIN32_FIND_DATAW found;
    HANDLE search = FindFirstFileW(wide, &found);
    const DWORD search_error = search == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    free(wide);
    if (search == INVALID_HANDLE_VALUE) {
        return search_error == ERROR_FILE_NOT_FOUND ||
               disk_system_error(probe, "Disk FindFirstFileW", (uint32_t)search_error);
    }
    for (;;) {
        if (wcscmp(found.cFileName, L".") != 0 && wcscmp(found.cFileName, L"..") != 0) {
            char name[MAX_PATH];
            size_t n = 0;
            while (found.cFileName[n] != 0 && n + 1u < sizeof(name) && found.cFileName[n] >= 32 &&
                   found.cFileName[n] <= 126) {
                name[n] = (char)found.cFileName[n];
                ++n;
            }
            name[n] = '\0';
            if (found.cFileName[n] != 0 || !disk_add(probe, inventory, relative, name)) {
                ok = problem(probe, "Disk non-ASCII/invalid entry", "probe", 1);
                break;
            }
        }
        if (!FindNextFileW(search, &found)) {
            const DWORD error = GetLastError();
            if (error != ERROR_NO_MORE_FILES)
                ok = disk_system_error(probe, "Disk FindNextFileW", (uint32_t)error);
            break;
        }
    }
    if (!FindClose(search))
        ok = disk_system_error(probe, "Disk FindClose", (uint32_t)GetLastError());
#else
    DIR *directory = opendir(absolute);
    const int open_error = directory == NULL ? errno : 0;
    free(absolute);
    if (directory == NULL)
        return disk_system_error(probe, "Disk opendir", (uint32_t)open_error);
    for (;;) {
        errno = 0;
        const struct dirent *entry = readdir(directory);
        if (entry == NULL) {
            if (errno != 0)
                ok = disk_system_error(probe, "Disk readdir", (uint32_t)errno);
            break;
        }
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 &&
            !disk_add(probe, inventory, relative, entry->d_name)) {
            ok = false;
            break;
        }
    }
    if (closedir(directory) != 0)
        ok = disk_system_error(probe, "Disk closedir", (uint32_t)errno);
#endif
    return ok;
}

static int disk_order(const void *left, const void *right) {
    const natural_disk_entry *a = left, *b = right;
    return strcmp(a->path, b->path);
}

static bool disk_inventory(natural_probe *probe) {
    natural_disk_inventory *inventory = calloc(1, sizeof(*inventory));
    if (inventory == NULL)
        return problem(probe, "Disk inventory allocation", "probe", 1);
    bool ok = disk_stat(probe, probe->persistence->root, &inventory->entries[0].stamp);
    if (ok && !inventory->entries[0].stamp.directory)
        ok = problem(probe, "Disk root is not a directory", "probe", 1);
    if (ok) {
        inventory->entries[0].path = malloc(2u);
        if (inventory->entries[0].path == NULL)
            ok = problem(probe, "Disk root entry allocation", "probe", 1);
        else {
            memcpy(inventory->entries[0].path, ".", 2u);
            inventory->count = inventory->directories = 1u;
        }
    }
    for (size_t i = 0; ok && i < inventory->count; ++i)
        if (inventory->entries[i].stamp.directory)
            ok = disk_children(probe, inventory, inventory->entries[i].path);
    if (ok && inventory->bytes > NATURAL_DISK_BYTES - probe->persistence->retained_disk_bytes)
        ok = problem(probe, "Retained disk observation byte bound", "probe", 1);
    if (ok) {
        qsort(inventory->entries, inventory->count, sizeof(inventory->entries[0]), disk_order);
        ok = emit(&probe->json, "{\"complete\":true,\"entries\":[");
    }
    for (size_t i = 0; ok && i < inventory->count; ++i) {
        natural_disk_entry *entry = &inventory->entries[i];
        char *absolute = NULL;
        natural_disk_stamp before, after;
        mc_ws_file_bytes bytes = {0};
        ok = mc_ws_path_join(probe->persistence->root, entry->path, &absolute,
                             &probe->inspection_error) == MC_WORLD_STORAGE_OK &&
             disk_stat(probe, absolute, &before) && disk_same(&entry->stamp, &before);
        if (ok && !entry->stamp.directory)
            ok = mc_ws_read_file(absolute, NATURAL_DISK_FILE_BYTES, &bytes,
                                 &probe->inspection_error) == MC_WORLD_STORAGE_OK &&
                 bytes.length == entry->stamp.bytes && disk_stat(probe, absolute, &after) &&
                 disk_same(&before, &after);
        if (!ok)
            problem(probe, "Disk entry changed or read failed", "storage",
                    (unsigned int)probe->inspection_error.status);
        if (ok)
            ok = emit(&probe->json, "%s{\"path\":", i == 0 ? "" : ",") &&
                 string(&probe->json, entry->path) && emit(&probe->json, ",\"kind\":") &&
                 string(&probe->json, entry->stamp.directory ? "directory" : "file");
        if (ok && !entry->stamp.directory) {
            probe->persistence->retained_disk_bytes += bytes.length;
            ok = emit(&probe->json, ",\"bytes\":%zu,\"hex\":\"", bytes.length);
            for (size_t j = 0; ok && j < bytes.length; ++j)
                ok = emit(&probe->json, "%02x", (unsigned int)bytes.data[j]);
            if (ok)
                ok = emit(&probe->json, "\"");
        }
        if (ok)
            ok = emit(&probe->json, "}");
        mc_ws_free_file_bytes(&bytes);
        free(absolute);
    }
    if (ok)
        ok = emit(&probe->json, "],\"files\":%zu,\"directories\":%zu,\"bytes\":%zu}",
                  inventory->files, inventory->directories, inventory->bytes);
    for (size_t i = 0; i < inventory->count; ++i)
        free(inventory->entries[i].path);
    free(inventory);
    return ok;
}

static bool info_projection(natural_probe *probe) {
    mc_world_runtime_fields world;
    mc_world_info_fields fields;
    if (mc_world_runtime_fields_get(probe->world, &world) != MC_WORLD_STORAGE_OK ||
        world.x != probe->persistence->info ||
        mc_world_info_fields_get(probe->persistence->info, &fields) != MC_WORLD_INFO_OK)
        return problem(probe, "Held Info observation binding", "probe", 1);
    if (fields.player != NULL)
        return problem(probe, "Info Player outside empty-player profile", "probe", 1);
    const mc_world_info_scalars *v = &fields.value;
    natural_json *json = &probe->json;
    if (!emit(json,
              "{\"owner\":0,\"seed\":\"%016" PRIx64 "\",\"spawn_x\":%" PRId32
              ",\"spawn_y\":%" PRId32 ",\"spawn_z\":%" PRId32 ",\"time\":\"%016" PRIx64
              "\",\"last_played\":\"%016" PRIx64 "\",\"size_on_disk\":\"%016" PRIx64
              "\",\"player\":null,\"player_dimension\":%" PRId32 ",\"level_name_utf16\":",
              (uint64_t)v->seed, v->spawn_x, v->spawn_y, v->spawn_z, (uint64_t)v->time,
              (uint64_t)v->last_played, (uint64_t)v->size_on_disk, v->player_dimension))
        return false;
    if (fields.name == NULL) {
        if (!emit(json, "null"))
            return false;
    } else {
        const mc_nbt_string text = mc_world_info_name_text(fields.name);
        if (text.length > NATURAL_JSON_LIMIT / 4u || (text.length != 0 && text.units == NULL) ||
            !emit(json, "\""))
            return problem(probe, "Info name observation bound", "probe", 1);
        for (size_t i = 0; i < text.length; ++i)
            if (!emit(json, "%04x", (unsigned int)text.units[i]))
                return false;
        if (!emit(json, "\""))
            return false;
    }
    return emit(json,
                ",\"version\":%" PRId32 ",\"raining\":%s,\"rain_time\":%" PRId32
                ",\"thundering\":%s,\"thunder_time\":%" PRId32 "}",
                v->version, boolean(v->raining), v->rain_time, boolean(v->thundering),
                v->thunder_time);
}

static bool persistence_boundary(natural_probe *probe) {
    return emit(&probe->json, "{\"snapshot\":") && snapshot(probe) &&
           emit(&probe->json, ",\"info\":") && info_projection(probe) &&
           emit(&probe->json, ",\"disk\":") && disk_inventory(probe) && emit(&probe->json, "}");
}

static bool world_collections(natural_probe *probe) {
    mc_world_runtime_fields fields;
    mc_world_saved_data_view saved;
    if (mc_world_runtime_fields_get(probe->world, &fields) != MC_WORLD_STORAGE_OK ||
        fields.w != probe->store || fields.d == NULL || fields.z == NULL ||
        fields.d->count > fields.d->capacity ||
        fields.d->capacity > SIZE_MAX / sizeof(*fields.d->items) ||
        (fields.d->capacity != 0 && fields.d->items == NULL) ||
        mc_world_saved_data_inspect(fields.z, &saved) != MC_WORLD_STORAGE_OK)
        return problem(probe, "Actual full-save collection graph", "probe", 1);
    const void *owners[] = {probe->world,      fields.w,           fields.d,     fields.z,
                            saved.named_cache, saved.ordered_list, saved.id_map, saved.storage};
    for (size_t i = 0; i < sizeof(owners) / sizeof(owners[0]); ++i) {
        if (owners[i] == NULL)
            return problem(probe, "Nonnull full-save collection owner", "probe", 1);
        for (size_t j = 0; j < i; ++j)
            if (owners[i] == owners[j])
                return problem(probe, "Distinct full-save collection owners", "probe", 1);
        if (probe->persistence->collections_captured &&
            owners[i] != probe->persistence->collection_owners[i])
            return problem(probe, "Stable actual full-save collection owners", "probe", 1);
        probe->persistence->collection_owners[i] = owners[i];
    }
    probe->persistence->collections_captured = true;
    if (fields.d->count != 0 || saved.named_count != 0 || saved.ordered_count != 0 ||
        saved.id_count != 0)
        return problem(probe, "Full-save actual empty collection domain", "probe", 1);
    return emit(&probe->json,
                "{\"world\":0,\"players\":{\"owner\":2,\"size\":%zu,\"entries\":[]},"
                "\"saved_data\":{\"owner\":3,\"storage\":1,\"native_storage_binding\":7,"
                "\"named_cache\":{\"owner\":4,\"size\":%zu,\"entries\":[]},"
                "\"ordered_list\":{\"owner\":5,\"size\":%zu,\"entries\":[]},"
                "\"id_map\":{\"owner\":6,\"size\":%zu,\"entries\":[]}}}",
                fields.d->count, saved.named_count, saved.ordered_count, saved.id_count);
}

static bool boundary_value(natural_probe *probe, bool *observed) {
    const size_t start = probe->json.length;
    if (probe->named != NULL)
        *observed = named_boundary(probe);
    else if (probe->persistence->full_world_save)
        *observed = emit(&probe->json, "{\"state\":") && persistence_boundary(probe) &&
                    emit(&probe->json, ",\"collections\":") && world_collections(probe) &&
                    emit(&probe->json, "}");
    else
        *observed = persistence_boundary(probe);
    if (*observed)
        return true;
    problem(probe, "Persistence boundary observation", "probe", 1);
    if (probe->json.failed)
        return false;
    /* Do not publish a truncated object as a complete inventory/boundary. The
     * first actual operation/projection error remains in the final record. */
    probe->json.length = start;
    return emit(&probe->json, "null");
}

static bool nbt_diagnostic(natural_json *json, const mc_nbt_error *error) {
    return emit(json, "{\"status\":%u,\"offset\":%zu}", (unsigned int)error->status, error->offset);
}

static bool stack_diagnostic(natural_json *json, const mc_item_stack_error *error) {
    return emit(json, "{\"status\":%u,\"field\":", (unsigned int)error->status) &&
           string(json, error->field) && emit(json, ",\"nbt\":") &&
           nbt_diagnostic(json, &error->nbt) && emit(json, "}");
}

static bool site_diagnostic(natural_json *json, const mc_minecart_site *site) {
    return emit(json,
                "{\"selected\":%s,\"entered\":%s,\"declaring_class\":", boolean(site->selected),
                boolean(site->entered)) &&
           string(json, site->declaring_class) && emit(json, ",\"method\":") &&
           string(json, site->method) &&
           emit(json, ",\"bytecode\":%u}", (unsigned int)site->bytecode);
}

static bool entity_diagnostic(natural_json *json, const mc_entity_error *error) {
    return emit(json, "{\"status\":%u,\"operation\":", (unsigned int)error->status) &&
           string(json, error->operation) &&
           emit(json,
                ",\"bytecode\":%u,\"profile\":%u,\"item\":%u,\"world\":%u,"
                "\"minecart_world_constructor\":",
                (unsigned int)error->bytecode, (unsigned int)error->profile,
                (unsigned int)error->item, (unsigned int)error->world) &&
           site_diagnostic(json, &error->minecart_world_constructor) &&
           emit(json, ",\"minecart_constructor\":") &&
           site_diagnostic(json, &error->minecart_constructor) &&
           emit(json, ",\"minecart_initializer\":") &&
           site_diagnostic(json, &error->minecart_initializer) &&
           emit(json, ",\"minecart_initializer_cause\":") &&
           site_diagnostic(json, &error->minecart_initializer_cause) && emit(json, "}");
}

static bool entity_nbt_diagnostic(natural_json *json, const mc_entity_nbt_error *error) {
    const mc_entity_registry_error *r = &error->registry;
    const mc_minecart_inventory_context *i = &error->inventory;
    return emit(json, "{\"status\":%u,\"operation\":", (unsigned int)error->status) &&
           string(json, error->operation) && emit(json, ",\"field\":") &&
           string(json, error->field) &&
           emit(json,
                ",\"index\":%zu,\"wrapper_bytecode\":%u,\"name_bytecode\":%u,"
                "\"factory_bytecode\":%u,\"base_bytecode\":%u,\"item_bytecode\":%u,"
                "\"falling_bytecode\":%u,\"stack_bytecode\":%u,\"initializer_bytecode\":%u,"
                "\"registration_bytecode\":%u,\"construction\":",
                error->index, error->wrapper_bytecode, error->name_bytecode,
                error->factory_bytecode, error->base_bytecode, error->item_bytecode,
                error->falling_bytecode, error->stack_bytecode, error->initializer_bytecode,
                error->registration_bytecode) &&
           entity_diagnostic(json, &error->construction) && emit(json, ",\"stack\":") &&
           stack_diagnostic(json, &error->stack) && emit(json, ",\"nbt\":") &&
           nbt_diagnostic(json, &error->nbt) &&
           emit(json,
                ",\"registry\":{\"status\":%u,\"cause\":%u,\"operation\":", (unsigned int)r->status,
                (unsigned int)r->cause) &&
           string(json, r->operation) &&
           emit(json,
                ",\"initializer_bytecode\":%u,\"registration_bytecode\":%u,"
                "\"class_cause\":%u},\"minecart_read\":",
                r->initializer_bytecode, r->registration_bytecode, (unsigned int)r->class_cause) &&
           site_diagnostic(json, &error->minecart_read) && emit(json, ",\"minecart_write\":") &&
           site_diagnostic(json, &error->minecart_write) &&
           emit(json, ",\"inventory\":{\"status\":%u,\"operation\":", (unsigned int)i->status) &&
           string(json, i->operation) &&
           emit(json, ",\"slot_selected\":%s,\"slot\":%" PRId32 ",\"stack_cause\":%u,\"current\":",
                boolean(i->slot_selected), i->slot, (unsigned int)i->stack_cause) &&
           site_diagnostic(json, &i->current) && emit(json, ",\"stack_split\":") &&
           site_diagnostic(json, &i->stack_split) && emit(json, ",\"stack_constructor\":") &&
           site_diagnostic(json, &i->stack_constructor) && emit(json, ",\"stack_read\":") &&
           site_diagnostic(json, &i->stack_read) && emit(json, "}}");
}

static bool world_entity_diagnostic(natural_json *json, const mc_world_entity_error *error) {
    return emit(json, "{\"status\":%u,\"operation\":", (unsigned int)error->status) &&
           string(json, error->operation) &&
           emit(json,
                ",\"bytecode\":%u,\"presence_bytecode\":%u,\"provider_bytecode\":%u,"
                "\"chunk_bytecode\":%u,\"observer_bytecode\":%u,\"acquisition\":%u,\"cause\":",
                error->bytecode, error->presence_bytecode, error->provider_bytecode,
                error->chunk_bytecode, error->observer_bytecode,
                (unsigned int)error->acquisition) &&
           storage(json, &error->cause) && emit(json, ",\"entity\":") &&
           entity_diagnostic(json, &error->entity) && emit(json, "}");
}

static bool batch_diagnostic(natural_json *json, const mc_world_entity_batch_error *error) {
    return emit(json, "{\"index\":%zu,\"publication\":", error->index) &&
           world_entity_diagnostic(json, &error->publication) && emit(json, "}");
}

static bool load_entity_diagnostic(natural_json *json, const mc_world_entity_load_error *error) {
    return emit(json, "{\"index\":%zu,\"factory\":", error->index) &&
           entity_nbt_diagnostic(json, &error->factory) && emit(json, ",\"provenance\":") &&
           entity_nbt_diagnostic(json, &error->provenance) &&
           emit(json, ",\"insertion\":{\"status\":%u,\"operation\":",
                (unsigned int)error->insertion.status) &&
           string(json, error->insertion.operation) &&
           emit(json, ",\"bytecode\":%u,\"cause\":", error->insertion.bytecode) &&
           storage(json, &error->insertion.cause) && emit(json, ",\"entity\":") &&
           entity_diagnostic(json, &error->insertion.entity) && emit(json, "},\"native\":") &&
           storage(json, &error->native) && emit(json, "}");
}

static bool live_tile_diagnostic(natural_json *json, const mc_live_tile_error *error) {
    const mc_tile_error *t = &error->tile;
    const mc_live_tile_call_frame *p = &error->acquisition_parent;
    return emit(json, "{\"status\":%u,\"operation\":", (unsigned int)error->status) &&
           string(json, error->operation) &&
           emit(json,
                ",\"bytecode\":%u,\"chunk_bytecode\":%u,\"observer_bytecode\":%u,"
                "\"initializer_bytecode\":%u,\"cause\":",
                error->bytecode, error->chunk_bytecode, error->observer_bytecode,
                error->initializer_bytecode) &&
           storage(json, &error->cause) && emit(json, ",\"registry\":") &&
           registry_diagnostic(json, &error->registry) &&
           emit(json, ",\"tile\":{\"status\":%u,\"field\":", (unsigned int)t->status) &&
           string(json, t->field) && emit(json, ",\"item_index\":%zu,\"item\":", t->item_index) &&
           stack_diagnostic(json, &t->item) && emit(json, ",\"nbt\":") &&
           nbt_diagnostic(json, &t->nbt) &&
           emit(json,
                ",\"source_bytecode\":%u,\"base_read_bytecode\":%u,\"read_bytecode\":%u,"
                "\"operation\":",
                t->source_bytecode, t->base_read_bytecode, t->read_bytecode) &&
           string(json, t->operation) &&
           emit(json,
                ",\"getter_bytecode\":%u,\"base_save_bytecode\":%u,\"save_bytecode\":%u,"
                "\"item_save_bytecode\":%u,\"registry\":",
                t->getter_bytecode, t->base_save_bytecode, t->save_bytecode,
                t->item_save_bytecode) &&
           registry_diagnostic(json, &t->registry) &&
           emit(json,
                ",\"constructor_bytecode\":%u,\"initializer_bytecode\":%u,"
                "\"piston_initializer_bytecode\":%u},\"entity\":",
                t->constructor_bytecode, t->initializer_bytecode, t->piston_initializer_bytecode) &&
           entity_diagnostic(json, &error->entity) && emit(json, ",\"publication\":") &&
           world_entity_diagnostic(json, &error->publication) && emit(json, ",\"batch\":") &&
           batch_diagnostic(json, &error->batch) &&
           emit(json,
                ",\"caller_bytecode\":%u,\"hook_bytecode\":%u,\"remove_bytecode\":%u,"
                "\"tile_bytecode\":%u,\"inventory_bytecode\":%u,\"acquisition_caller_bytecode\":%u,"
                "\"activation_bytecode\":%u,\"activation_world_bytecode\":%u,\"load_bytecode\":%u,"
                "\"load_builder_bytecode\":%u,\"load_entity\":",
                error->caller_bytecode, error->hook_bytecode, error->remove_bytecode,
                error->tile_bytecode, error->inventory_bytecode, error->acquisition_caller_bytecode,
                error->activation_bytecode, error->activation_world_bytecode, error->load_bytecode,
                error->load_builder_bytecode) &&
           load_entity_diagnostic(json, &error->load_entity) &&
           emit(json,
                ",\"provider_bytecode\":%u,\"acquisition_parent\":{\"bytecode\":%u,"
                "\"chunk_bytecode\":%u,\"observer_bytecode\":%u,\"initializer_bytecode\":%u,"
                "\"caller_bytecode\":%u,\"hook_bytecode\":%u,\"remove_bytecode\":%u,"
                "\"provider_bytecode\":%u,\"tile_bytecode\":%u,\"inventory_bytecode\":%u}}",
                error->provider_bytecode, p->bytecode, p->chunk_bytecode, p->observer_bytecode,
                p->initializer_bytecode, p->caller_bytecode, p->hook_bytecode, p->remove_bytecode,
                p->provider_bytecode, p->tile_bytecode, p->inventory_bytecode);
}

static bool save_diagnostic(natural_json *json, const mc_world_runtime_lifecycle_error *error) {
    const mc_world_game_save_error *s = &error->save;
    return emit(json, "{\"status\":%u,\"cause\":", (unsigned int)error->status) &&
           storage(json, &error->cause) &&
           emit(json, ",\"save\":{\"status\":%u,\"cause\":", (unsigned int)s->status) &&
           storage(json, &s->cause) &&
           emit(json,
                ",\"region_bytecode\":%u,\"serializer_bytecode\":%u,\"entity_slice\":%zu,"
                "\"entity_index\":%zu,\"entity\":",
                s->region_bytecode, s->serializer_bytecode, s->entity_slice, s->entity_index) &&
           entity_nbt_diagnostic(json, &s->entity) && emit(json, ",\"entity_records\":") &&
           nbt_diagnostic(json, &s->entity_records) && emit(json, ",\"tile\":") &&
           live_tile_diagnostic(json, &s->tile) &&
           emit(json, ",\"composition\":{\"status\":%u,\"field\":",
                (unsigned int)s->composition.status) &&
           string(json, s->composition.field) && emit(json, ",\"nbt\":") &&
           nbt_diagnostic(json, &s->composition.nbt) && emit(json, "},\"encoding\":") &&
           nbt_diagnostic(json, &s->encoding) && emit(json, "},\"tile\":") &&
           live_tile_diagnostic(json, &error->tile) && emit(json, ",\"batch\":") &&
           batch_diagnostic(json, &error->batch) &&
           emit(json, ",\"activation_bytecode\":%u,\"activation_world_bytecode\":%u}",
                error->activation_bytecode, error->activation_world_bytecode);
}

static bool load_diagnostic(natural_json *json, const mc_world_game_load_error *error) {
    return emit(json, "{\"status\":%u,\"cause\":", (unsigned int)error->status) &&
           storage(json, &error->cause) &&
           emit(json,
                ",\"region_bytecode\":%u,\"builder_bytecode\":%u,\"tile\":", error->region_bytecode,
                error->builder_bytecode) &&
           live_tile_diagnostic(json, &error->tile) && emit(json, ",\"entity\":") &&
           load_entity_diagnostic(json, &error->entity) && emit(json, "}");
}

typedef struct natural_epoch {
    int64_t low, high;
    bool low_valid, high_valid;
} natural_epoch;

static bool epoch_sample(natural_probe *probe, int64_t *value, bool *valid) {
    const mc_platform_millis_status status = mc_platform_current_time_millis(value);
    *valid = status == MC_PLATFORM_MILLIS_OK;
    return *valid || problem(probe, "Persistence platform clock", "clock", (unsigned int)status);
}

static bool epoch_projection(natural_json *json, const natural_epoch *epoch) {
    return emit(json, "[") &&
           (epoch->low_valid ? emit(json, "%" PRId64, epoch->low) : emit(json, "null")) &&
           emit(json, ",") &&
           (epoch->high_valid ? emit(json, "%" PRId64, epoch->high) : emit(json, "null")) &&
           emit(json, "]");
}

static bool persistence_time(natural_probe *probe, bool *ready) {
    natural_json *json = &probe->json;
    bool before = false, after = false, attempted = false;
    int64_t previous = 0;
    bool previous_valid = false;
    natural_epoch epoch = {0};
    mc_world_info_status status = MC_WORLD_INFO_OK;
    if (!emit(json, "{\"value\":1234,\"before\":") || !boundary_value(probe, &before))
        return false;
    if (before) {
        status = mc_world_info_time_get(probe->persistence->info, &previous);
        previous_valid = status == MC_WORLD_INFO_OK;
        if (!previous_valid)
            problem(probe, "Held Info time getter", "info", (unsigned int)status);
        if (previous_valid && epoch_sample(probe, &epoch.low, &epoch.low_valid)) {
            attempted = true;
            status = mc_world_info_time_set(probe->persistence->info, 1234);
            if (status != MC_WORLD_INFO_OK)
                problem(probe, "Held Info time setter", "info", (unsigned int)status);
            (void)epoch_sample(probe, &epoch.high, &epoch.high_valid);
        }
    }
    if (!emit(json, ",\"previous\":") ||
        !(previous_valid ? emit(json, "\"%016" PRIx64 "\"", (uint64_t)previous)
                         : emit(json, "null")) ||
        !emit(json, ",\"epoch_ms\":") || !epoch_projection(json, &epoch) ||
        !emit(json, ",\"status\":") ||
        !(attempted ? string(json, mc_world_info_status_string(status)) : emit(json, "null")) ||
        !emit(json, ",\"native_status\":") ||
        !(attempted ? emit(json, "%u", (unsigned int)status) : emit(json, "null")) ||
        !emit(json, ",\"after\":") || !boundary_value(probe, &after) || !emit(json, "}"))
        return false;
    *ready = before && after && attempted && probe->failed_stage == NULL;
    return true;
}

static bool persistence_save(natural_probe *probe, size_t index, bool *ready) {
    natural_json *json = &probe->json;
    bool before = false, after = false, attempted = false, completed = false;
    natural_epoch epoch = {0};
    mc_chunk_lifecycle_status status = MC_CHUNK_LIFECYCLE_OK;
    if (!emit(json, "{\"index\":%zu,\"force\":true,\"progress\":null,\"before\":", index) ||
        !boundary_value(probe, &before))
        return false;
    if (before && epoch_sample(probe, &epoch.low, &epoch.low_valid)) {
        attempted = true;
        status = probe->persistence->full_world_save
                     ? mc_world_runtime_save_detailed(probe->world, true,
                                                      &probe->persistence->save_error)
                     : mc_world_runtime_save_chunks_detailed(probe->world, true, &completed,
                                                             &probe->persistence->save_error);
        if (status != MC_CHUNK_LIFECYCLE_OK)
            problem(probe, "Forced World or cache save", "lifecycle", (unsigned int)status);
        (void)epoch_sample(probe, &epoch.high, &epoch.high_valid);
    }
    if (!emit(json,
              probe->persistence->full_world_save ? ",\"returned_void\":" : ",\"completed\":") ||
        !(attempted && status == MC_CHUNK_LIFECYCLE_OK
              ? emit(json, "%s", boolean(probe->persistence->full_world_save || completed))
              : emit(json, "null")) ||
        !emit(json, ",\"epoch_ms\":") || !epoch_projection(json, &epoch) ||
        !emit(json, ",\"status\":") ||
        !(attempted ? string(json, mc_chunk_lifecycle_status_string(status))
                    : emit(json, "null")) ||
        !emit(json, ",\"native_status\":") ||
        !(attempted ? emit(json, "%u", (unsigned int)status) : emit(json, "null")) ||
        !emit(json, ",\"error\":") ||
        !(attempted && status != MC_CHUNK_LIFECYCLE_OK
              ? save_diagnostic(json, &probe->persistence->save_error)
              : emit(json, "null")) ||
        !emit(json, ",\"after\":") || !boundary_value(probe, &after) || !emit(json, "}"))
        return false;
    *ready = before && after && attempted && probe->failed_stage == NULL;
    return true;
}

static bool direct_chunk_binding(natural_probe *probe, mc_chunk_state *state) {
    mc_world_chunk *chunk = mc_world_store_chunk_object(probe->store, state);
    if (chunk == NULL || mc_world_chunk_state(chunk) != state)
        return problem(probe, "Direct loader concrete Chunk identity", "probe", 1);
    mc_world_chunk_fields fields;
    mc_world_storage_status status = mc_world_chunk_fields_get(chunk, &fields);
    if (status != MC_WORLD_STORAGE_OK)
        return problem(probe, "Direct loader concrete Chunk fields", "storage",
                       (unsigned int)status);
    if (fields.d != probe->world)
        return problem(probe, "Direct loader actual Chunk World binding", "probe", 1);
    size_t count = SIZE_MAX;
    status = mc_world_chunk_tile_count(fields.l, &count);
    if (status != MC_WORLD_STORAGE_OK)
        return problem(probe, "Direct loader actual Tile map", "storage", (unsigned int)status);
    if (count != 0)
        return problem(probe, "Direct loader expected empty Tile map", "probe", 1);
    const mc_world_chunk_entity_list *slices[8] = {0};
    for (size_t i = 0; i < sizeof(slices) / sizeof(slices[0]); ++i) {
        status = mc_world_chunk_slice_at(fields.m, i, &slices[i]);
        if (status != MC_WORLD_STORAGE_OK)
            return problem(probe, "Direct loader actual Entity slice", "storage",
                           (unsigned int)status);
        status = mc_world_chunk_entity_count(slices[i], &count);
        if (status != MC_WORLD_STORAGE_OK)
            return problem(probe, "Direct loader actual Entity count", "storage",
                           (unsigned int)status);
        if (count != 0)
            return problem(probe, "Direct loader expected empty Entity slice", "probe", 1);
        for (size_t j = 0; j < i; ++j)
            if (slices[i] == slices[j])
                return problem(probe, "Direct loader distinct Entity slice owners", "probe", 1);
    }
    const mc_world_chunk_entity_list *sentinel = slices[0];
    if (mc_world_chunk_slice_at(fields.m, 8u, &sentinel) != MC_WORLD_STORAGE_INVALID_ARGUMENT ||
        sentinel != slices[0])
        return problem(probe, "Direct loader exactly eight Entity slices", "probe", 1);
    return true;
}

static bool persistence_load(natural_probe *probe, size_t index, int32_t x, int32_t z,
                             bool *ready) {
    natural_json *json = &probe->json;
    bool before = false, after = false, attempted = false;
    natural_epoch epoch = {0};
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    mc_chunk_state *result = NULL;
    size_t id = SIZE_MAX;
    if (!emit(json, "{\"index\":%zu,\"coordinates\":[%" PRId32 ",%" PRId32 "],\"before\":", index,
              x, z) ||
        !boundary_value(probe, &before))
        return false;
    if (before && epoch_sample(probe, &epoch.low, &epoch.low_valid)) {
        attempted = true;
        const size_t previous_count = probe->identity_count;
        status = mc_world_runtime_load_game(probe->world, x, z, &result,
                                            &probe->persistence->load_error);
        if (status != MC_WORLD_STORAGE_OK)
            problem(probe, "Direct GAME loader", "storage", (unsigned int)status);
        (void)epoch_sample(probe, &epoch.high, &epoch.high_valid);
        if (status == MC_WORLD_STORAGE_OK && result == NULL)
            problem(probe, "Direct loader successful result is NULL", "probe", 1);
        if (result != NULL) {
            id = chunk_id(probe, result);
            if (id == SIZE_MAX || id != previous_count || probe->published[id] ||
                !mc_world_store_owns_chunk(probe->store, result) || result->x != x ||
                result->z != z)
                problem(probe, "Direct loader fresh unpublished result contract", "probe", 1);
            else
                (void)direct_chunk_binding(probe, result);
        }
    }
    if (!emit(json, ",\"epoch_ms\":") || !epoch_projection(json, &epoch) ||
        !emit(json, ",\"status\":") ||
        !(attempted ? string(json, mc_world_storage_status_string(status)) : emit(json, "null")) ||
        !emit(json, ",\"native_status\":") ||
        !(attempted ? emit(json, "%u", (unsigned int)status) : emit(json, "null")) ||
        !emit(json, ",\"error\":") ||
        !(attempted && status != MC_WORLD_STORAGE_OK
              ? load_diagnostic(json, &probe->persistence->load_error)
              : emit(json, "null")) ||
        !emit(json, ",\"result\":") ||
        !(id == SIZE_MAX ? emit(json, "null") : emit(json, "%zu", id)) ||
        !emit(json, ",\"chunk\":") ||
        !(id == SIZE_MAX ? emit(json, "null") : chunk_projection(probe, result, id)) ||
        !emit(json, ",\"normalization_map\":") ||
        !byte_array(json, probe->block_map, sizeof(probe->block_map)) ||
        !emit(json, ",\"after\":") || !boundary_value(probe, &after) || !emit(json, "}"))
        return false;
    *ready = before && after && attempted && probe->failed_stage == NULL;
    return true;
}

#include "named_data_probe.h"

static bool observe_persistence(natural_probe *probe, bool initialized) {
    natural_json *json = &probe->json;
    natural_persistence *p = probe->persistence;
    const natural_epoch construction = {p->construction_low, p->construction_high,
                                        p->construction_low_valid, p->construction_high_valid};
    if (!emit(json,
              "{\"schema\":1,\"scope\":\"%s\","
              "\"profile\":\"%s\",\"side\":\"%s\",\"seed_hex\":\"0000000000000017\","
              "\"construction_epoch_ms\":",
              p->full_world_save ? "native-game-population-full-world-save"
                                 : "native-game-population-save-readback",
              p->full_world_save ? "full-world-save" : "save-readback",
              probe->client ? "client" : "server") ||
        !epoch_projection(json, &construction) || !emit(json, ",\"population\":") ||
        !observe(probe, initialized, false) ||
        !emit(json, p->full_world_save ? ",\"world_save\":{\"session_token\":"
                                       : ",\"persistence\":{\"session_token\":"))
        return false;
    if (!(probe->store == NULL ? emit(json, "null")
                               : emit(json, "\"%016" PRIx64 "\"", (uint64_t)p->session_token)) ||
        !emit(json, ",\"block_map\":") ||
        !(p->map_ready ? byte_array(json, probe->block_map, sizeof(probe->block_map))
                       : emit(json, "null")) ||
        !emit(json, ",\"initial\":"))
        return false;
    bool ready = initialized && probe->failed_stage == NULL;
    if (ready) {
        mc_world_runtime_fields fields;
        const mc_world_storage_status status = mc_world_runtime_fields_get(probe->world, &fields);
        if (status != MC_WORLD_STORAGE_OK || fields.x == NULL)
            ready =
                problem(probe, "Persistence actual Info binding", "storage", (unsigned int)status);
        else {
            const mc_world_info_status held = mc_world_info_retain(fields.x);
            if (held != MC_WORLD_INFO_OK)
                ready = problem(probe, "Persistence actual Info hold", "info", (unsigned int)held);
            else
                p->info = fields.x;
        }
    }
    if (ready) {
        bool observed;
        if (!boundary_value(probe, &observed))
            return false;
        ready = observed && probe->failed_stage == NULL;
    } else if (!emit(json, "null")) {
        return false;
    }
    if (!emit(json, ",\"time_api\":") ||
        !(ready ? persistence_time(probe, &ready) : emit(json, "null")) ||
        !emit(json, p->full_world_save ? ",\"saves\":[" : ",\"cache_saves\":["))
        return false;
    for (size_t i = 0; ready && i < 2u; ++i)
        if (!emit(json, "%s", i == 0 ? "" : ",") || !persistence_save(probe, i, &ready))
            return false;
    if (p->full_world_save)
        return emit(json, "],\"failure\":") && diagnostic(probe) && emit(json, "}}\n");
    if (!emit(json, "],\"direct_loads\":["))
        return false;
    static const int32_t coordinates[][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    for (size_t i = 0; ready && i < sizeof(coordinates) / sizeof(coordinates[0]); ++i)
        if (!emit(json, "%s", i == 0 ? "" : ",") ||
            !persistence_load(probe, i, coordinates[i][0], coordinates[i][1], &ready))
            return false;
    return emit(json, "],\"failure\":") && diagnostic(probe) && emit(json, "}}\n");
}

static bool cleanup(natural_probe *probe) {
    if (probe->named != NULL) {
        mc_world_saved_data_free(probe->named->reader);
        probe->named->reader = NULL;
        for (size_t i = 0; i < probe->named->value_count; ++i)
            mc_saved_data_value_release(probe->named->values[i]);
        probe->named->value_count = 0;
        mc_world_info_name_release(probe->named->name);
        mc_world_info_name_release(probe->named->lookup);
        probe->named->name = probe->named->lookup = NULL;
    }
    if (probe->persistence != NULL) {
        mc_world_info_release(probe->persistence->info);
        probe->persistence->info = NULL;
    }
    for (size_t i = 0; i < probe->held_climate_count; ++i)
        mc_biome_double_release(&probe->held_climate[i]);
    probe->held_climate_count = 0;
    for (size_t i = 0; i < probe->held_temperature_count; ++i)
        mc_temperature_array_release(probe->held_temperature[i]);
    probe->held_temperature_count = 0;
    if (probe->world != NULL) {
        /* This terminal native release is outside every captured source state;
         * it performs no save, original unload or future gameplay callback. */
        if (mc_world_runtime_retire_entity_containers(probe->world, &probe->inspection_error) !=
            MC_WORLD_STORAGE_OK)
            return false;
        mc_world_runtime_fields fields;
        if (mc_world_runtime_fields_get(probe->world, &fields) != MC_WORLD_STORAGE_OK)
            return false;
        if (fields.v != NULL) {
            if (mc_world_provider_bind_fields(fields.v, &probe->provider,
                                              &probe->inspection_error) != MC_WORLD_STORAGE_OK ||
                mc_chunk_pending_clear(probe->provider.pending) != MC_CHUNK_ACQUIRE_OK ||
                !mc_chunk_cache_init(probe->provider.cache, probe->provider.cache->entries,
                                     probe->provider.cache->capacity,
                                     probe->provider.cache->insertions,
                                     probe->provider.cache->insertion_capacity))
                return false;
        }
        if (mc_world_runtime_retire_world(probe->world, &probe->inspection_error) !=
            MC_WORLD_STORAGE_OK)
            return false;
    }
    while (mc_world_store_chunk_count(probe->store) != 0) {
        mc_chunk_state *state = NULL;
        if (mc_world_store_chunk_at(probe->store, 0, &state, &probe->inspection_error) !=
                MC_WORLD_STORAGE_OK ||
            mc_world_store_retire_chunk(probe->store, state, &probe->inspection_error) !=
                MC_WORLD_STORAGE_OK)
            return false;
    }
    if (mc_world_runtime_free(probe->world, &probe->inspection_error) != MC_WORLD_STORAGE_OK)
        return false;
    probe->world = NULL;
    mc_world_dimension_release(probe->dimension);
    probe->dimension = NULL;
    if (mc_world_runtime_classes_free(probe->classes, &probe->inspection_error) !=
        MC_WORLD_STORAGE_OK)
        return false;
    probe->classes = NULL;
    if (mc_registry_classes_free(probe->registry, NULL) != MC_REGISTRY_OK)
        return false;
    probe->registry = NULL;
    if (mc_entity_classes_free(probe->entities) != MC_ENTITY_OK)
        return false;
    probe->entities = NULL;
    mc_world_info_name_release(probe->name);
    probe->name = NULL;
    if (mc_world_store_free_checked(&probe->store, &probe->inspection_error) != MC_WORLD_STORAGE_OK)
        return false;
    mc_world_regions_free(probe->regions);
    probe->regions = NULL;
    mc_java_random_profile_free(probe->profile);
    probe->profile = NULL;
    mc_materials_destroy(probe->materials);
    probe->materials = NULL;
    if (mc_resource_files_free(probe->resources) != MC_REGISTRY_OK)
        return false;
    probe->resources = NULL;
    return true;
}

#include "dedicated_probe.h"

int main(int argc, char **argv) {
    if (argc == 7 && strcmp(argv[1], "server") == 0 &&
        strcmp(argv[5], "dedicated-natural-unload") == 0)
        return dedicated_main(argv);
    if ((argc != 5 && argc != 6) ||
        (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0))
        return 1;
    const bool ring = argc == 6 && strcmp(argv[5], "ring") == 0;
    const bool save_readback = argc == 6 && strcmp(argv[5], "save-readback") == 0;
    const bool full_world_save = argc == 6 && strcmp(argv[5], "full-world-save") == 0;
    const bool named_data = argc == 6 && strcmp(argv[5], "named-map-data") == 0;
    const bool persistent = save_readback || full_world_save || named_data;
    if (argc == 6 && !ring && !persistent && strcmp(argv[5], "origin") != 0)
        return 1;
    char *resources = decode_path(argv[2]), *root = decode_path(argv[3]);
    natural_probe *probe = calloc(1, sizeof(*probe));
    mc_math_trig *trig = calloc(1, sizeof(*trig));
    natural_persistence *persistence = persistent ? calloc(1, sizeof(*persistence)) : NULL;
    natural_named *named = named_data ? calloc(1, sizeof(*named)) : NULL;
    if (resources == NULL || root == NULL || probe == NULL || trig == NULL ||
        (persistent && persistence == NULL) || (named_data && named == NULL)) {
        free(resources);
        free(root);
        free(probe);
        free(trig);
        free(persistence);
        free(named);
        return 1;
    }
    probe->persistence = persistence;
    probe->named = named;
    if (persistence != NULL) {
        persistence->root = root;
        persistence->full_world_save = full_world_save || named_data;
    }
    probe->client = strcmp(argv[1], "client") == 0;
    probe->registry_error.initializer_pc = -1;
    probe->registry_error.block_id = -1;
    probe->registry_error.constructor_pc = -1;
    probe->registry_error.active_initializer_pc = -1;
    probe->registry_error.item_argument = -1;
    const bool math_ready = mc_math_trig_init(trig);
    bool clock_ready = true;
    if (persistence != NULL && math_ready)
        clock_ready = epoch_sample(probe, &persistence->construction_low,
                                   &persistence->construction_low_valid);
    const bool initialized = math_ready && clock_ready && initialize(probe, resources, root, trig);
    if (persistence != NULL && persistence->construction_low_valid)
        (void)epoch_sample(probe, &persistence->construction_high,
                           &persistence->construction_high_valid);
    if (!initialized && probe->failed_stage == NULL)
        problem(probe, "Math lookup initialization", "probe", 1);
    const bool encoded = named_data   ? observe_named(probe, initialized)
                         : persistent ? observe_persistence(probe, initialized)
                                      : observe(probe, initialized, ring);
    bool written = false;
    if (encoded && !probe->json.failed)
        written = mc_ws_write_file(argv[4], (const uint8_t *)probe->json.bytes, probe->json.length,
                                   true, &probe->inspection_error) == MC_WORLD_STORAGE_OK;
    const bool passed = initialized && encoded && written && probe->failed_stage == NULL;
    if (!passed)
        fprintf(stderr, "natural population observation failed: %s (%s %u)\n",
                probe->failed_stage == NULL ? "projection or output" : probe->failed_stage,
                probe->failed_domain == NULL ? "probe" : probe->failed_domain,
                probe->failed_status);
    const bool released = cleanup(probe);
    if (!released) {
        fputs("natural population cleanup refused; live dependencies retained\n", stderr);
        if (persistent)
            fprintf(stderr, "cleanup storage status: %s (%s, system %" PRIu32 ")\n",
                    mc_world_storage_status_string(probe->inspection_error.status),
                    probe->inspection_error.operation == NULL ? "no operation"
                                                              : probe->inspection_error.operation,
                    probe->inspection_error.system_error);
    }
    free(resources);
    free(root);
    free(probe->json.bytes);
    if (released) {
        if (persistence != NULL)
            free(persistence->store_root);
        free(persistence);
        free(named);
        free(probe);
        free(trig);
    }
    return passed && released ? 0 : 1;
}
