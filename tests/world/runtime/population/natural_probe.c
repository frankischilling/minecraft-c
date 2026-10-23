/* Finite GAME World observation. All source state comes from fresh constructors
 * and the scheduled actual cache c calls; no captured result is imported. Native
 * terminal retirement runs only after the complete observation is encoded. */
#include "mc/resources/files/files.h"
#include "mc/world/biome/registry.h"
#include "mc/world/biome/sampling.h"
#include "mc/world/dimension/biome_runtime.h"
#include "mc/world/runtime/entities.h"
#include "mc/world/runtime/lighting.h"

#include "../../../../src/block/registry/registry_internal.h"
#include "../../../../src/platform/time/millis.h"
#include "../../../../src/world/biome/biome_internal.h"
#include "../../../../src/world/runtime/construction_internal.h"
#include "../../../../src/world/runtime/provider_internal.h"
#include "../../../../src/world/storage/files.h"
#include "../../../../src/world/storage/objects_internal.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { NATURAL_CAPACITY = 32, NATURAL_IDENTITIES = NATURAL_CAPACITY + 1 };

typedef enum natural_array_domain {
    NATURAL_DOUBLE_ARRAY,
    NATURAL_BIOME_ARRAY,
    NATURAL_TEMPERATURE_ARRAY
} natural_array_domain;

#define NATURAL_JSON_LIMIT (64u * 1024u * 1024u)
#define NATURAL_ARRAY_IDENTITIES 128u

typedef struct natural_json {
    char *bytes;
    size_t length, capacity;
    bool failed;
} natural_json;

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
    size_t identity_count;
    const void *arrays[NATURAL_ARRAY_IDENTITIES];
    natural_array_domain array_domains[NATURAL_ARRAY_IDENTITIES];
    size_t array_count;
    mc_biome_double_reference held_climate[NATURAL_ARRAY_IDENTITIES];
    size_t held_climate_count;
    mc_temperature_array *held_temperature[NATURAL_ARRAY_IDENTITIES];
    size_t held_temperature_count;
    uint8_t block_map[256];
    natural_json json;
} natural_probe;

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
        !registry_diagnostic(json, mc_world_runtime_last_registry_error(probe->world)) ||
        !emit(json, ",\"construction\":"))
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
    for (size_t i = 0; i < cache->insertion_count; ++i)
        if (chunk_id(probe, cache->insertions[i]) == SIZE_MAX)
            return false;
    for (size_t i = 0; i < cache->count; ++i)
        if (chunk_id(probe, cache->entries[i].chunk) == SIZE_MAX)
            return false;
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
    for (size_t i = 0; i < probe->identity_count; ++i) {
        mc_chunk_state *state = probe->identities[i];
        if (!emit(json,
                  "%s{\"id\":%zu,\"coordinates\":[%" PRId32 ",%" PRId32
                  "],\"blank\":%s,\"flags\":{\"c\":%s,\"n\":%s,\"o\":%s,\"p\":%s,\"q\":%s},"
                  "\"time\":\"%016" PRIx64 "\",\"data\":",
                  i == 0 ? "" : ",", i, state->x, state->z, boolean(state->blank),
                  boolean(state->active), boolean(state->terrain_populated), boolean(state->dirty),
                  boolean(state->never_save), boolean(state->has_entities),
                  (uint64_t)state->last_save_time) ||
            !(state->blank ? emit(json, "null") : chunk_data(probe, state)) || !emit(json, "}"))
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
    for (size_t i = 0; i < sizeof(probe->block_map); ++i)
        probe->block_map[i] = (uint8_t)i;
    int64_t token;
    NATURAL_REQUIRE(probe, mc_platform_current_time_millis(&token), MC_PLATFORM_MILLIS_OK, "clock",
                    "Store session clock");
    mc_world_store_options store = {.session_token = token,
                                    .block_map = probe->block_map,
                                    .block_map_length = sizeof(probe->block_map),
                                    .limits = mc_world_store_default_limits()};
    store.limits.max_loaded_chunks = NATURAL_CAPACITY;
    NATURAL_REQUIRE(
        probe,
        mc_world_store_open(root, probe->regions, &store, &probe->store, &probe->storage_error),
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
    if (initialized) {
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

static bool cleanup(natural_probe *probe) {
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

int main(int argc, char **argv) {
    if ((argc != 5 && argc != 6) ||
        (strcmp(argv[1], "client") != 0 && strcmp(argv[1], "server") != 0))
        return 1;
    const bool ring = argc == 6 && strcmp(argv[5], "ring") == 0;
    if (argc == 6 && !ring && strcmp(argv[5], "origin") != 0)
        return 1;
    char *resources = decode_path(argv[2]), *root = decode_path(argv[3]);
    natural_probe *probe = calloc(1, sizeof(*probe));
    mc_math_trig *trig = calloc(1, sizeof(*trig));
    if (resources == NULL || root == NULL || probe == NULL || trig == NULL) {
        free(resources);
        free(root);
        free(probe);
        free(trig);
        return 1;
    }
    probe->client = strcmp(argv[1], "client") == 0;
    probe->registry_error.initializer_pc = -1;
    probe->registry_error.block_id = -1;
    probe->registry_error.constructor_pc = -1;
    probe->registry_error.active_initializer_pc = -1;
    probe->registry_error.item_argument = -1;
    const bool initialized = mc_math_trig_init(trig) && initialize(probe, resources, root, trig);
    if (!initialized && probe->failed_stage == NULL)
        problem(probe, "Math lookup initialization", "probe", 1);
    const bool encoded = observe(probe, initialized, ring);
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
    if (!released)
        fputs("natural population cleanup refused; live dependencies retained\n", stderr);
    free(resources);
    free(root);
    free(probe->json.bytes);
    if (released) {
        free(probe);
        free(trig);
    }
    return passed && released ? 0 : 1;
}
