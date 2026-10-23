#ifndef MC_TEST_DEDICATED_STATE_PROBE_H
#define MC_TEST_DEDICATED_STATE_PROBE_H

/* Private finite GAME observation, included after natural_probe.c's helpers.
 * References identify actual typed C owners or subobjects. Their spelling does
 * not claim Java heap objects, layouts, collection backings or AtomicLongs. */
#include "../../../../src/core/java/random_profile_internal.h"
#include "../../../../src/world/runtime/dedicated/entity_index.h"
#include "../../../../src/world/runtime/runtime_internal.h"
#include "mc/world/runtime/scheduling.h"
#include "mc/world/schedule/membership.h"
#include "mc/world/schedule/ordered.h"

#include <stdatomic.h>
#include <stddef.h>

#define DEDICATED_REFERENCE_LIMIT 8192u
#define DEDICATED_TICK_LIMIT 4096u

typedef enum dedicated_reference_kind {
    D_WORLD,
    D_SERVER,
    D_PROVIDER,
    D_GENERATOR,
    D_CACHE,
    D_PENDING,
    D_LIST,
    D_ACTIVE_SET,
    D_ORDERED,
    D_MEMBERSHIP,
    D_TICK,
    D_DIMENSION,
    D_MANAGER,
    D_BIOME,
    D_SPAWN_LIST,
    D_SPAWN,
    D_LOGICAL_CLASS,
    D_CANONICAL_CLASS,
    D_INFO,
    D_INFO_NAME,
    D_REGISTRY_STRING,
    D_SAVED_DATA,
    D_SAVED_CACHE,
    D_SAVED_LIST,
    D_SAVED_IDS,
    D_STORE,
    D_LOADER,
    D_PATH,
    D_CHUNK,
    D_CHUNK_HEADER,
    D_CHUNK_DATA,
    D_NIBBLE,
    D_SLICE_ARRAY,
    D_SLICE,
    D_TILE_MAP,
    D_BYTES,
    D_DOUBLES,
    D_FLOATS,
    D_BIOME_ARRAY,
    D_PERLIN_GROUP,
    D_PERLIN,
    D_SIMPLEX_GROUP,
    D_SIMPLEX,
    D_PERMUTATION,
    D_CAVE,
    D_GRID,
    D_GRID_ROW,
    D_LIGHT,
    D_LIGHT_TYPE,
    D_SERVER_LIST,
    D_SERVER_SYNC,
    D_SERVER_TRACKERS,
    D_INDEX,
    D_INDEX_TABLE,
    D_PROFILE,
    D_MATH,
    D_INPUTS,
    D_RANDOM,
    D_CHUNK_POINTERS,
    D_DIMENSION_BIOME,
    D_TEMPERATURE,
    D_UTF8,
    D_INT_SET,
    D_REFERENCE_KIND_COUNT
} dedicated_reference_kind;

typedef struct dedicated_reference {
    const void *pointer;
    dedicated_reference_kind kind;
} dedicated_reference;

struct dedicated_observation {
    mc_server_runtime *server;
    const mc_java_random_constructor_input *inputs;
    size_t input_count;
    mc_chunk_state *old_far, *reloaded_far;
    size_t boundary_ordinal;
    dedicated_reference references[DEDICATED_REFERENCE_LIMIT];
    size_t reference_count;
    mc_chunk_state *retained[NATURAL_IDENTITIES];
    size_t retained_count;
    const mc_scheduled_tick *ordered[DEDICATED_TICK_LIMIT];
    const mc_scheduled_tick *membership[DEDICATED_TICK_LIMIT];
    const mc_scheduled_tick *ticks[DEDICATED_TICK_LIMIT];
    size_t tick_count;
};

static bool dedicated_fail(natural_probe *probe, const char *operation) {
    return problem(probe, operation, "dedicated_observation", 1u);
}

static const char *dedicated_kind(dedicated_reference_kind kind) {
    static const char *const names[] = {"mc_world_runtime",
                                        "mc_server_runtime",
                                        "mc_world_provider",
                                        "mc_normal_generator",
                                        "mc_chunk_cache",
                                        "mc_chunk_pending",
                                        "mc_world_reference_list",
                                        "mc_world_active_chunks",
                                        "mc_scheduled_ordered",
                                        "mc_scheduled_membership",
                                        "mc_scheduled_tick",
                                        "mc_world_dimension",
                                        "mc_biome_manager",
                                        "mc_biome",
                                        "mc_biome_spawn_list",
                                        "mc_biome_spawn",
                                        "mc_biome_entity_class",
                                        "mc_entity_class_literal",
                                        "mc_world_info",
                                        "mc_world_info_name",
                                        "mc_registry_string",
                                        "mc_world_saved_data",
                                        "native.saved_named_cache",
                                        "native.saved_ordered_list",
                                        "native.saved_id_map",
                                        "mc_world_store",
                                        "mc_world_store_loader",
                                        "native.path_binding",
                                        "mc_chunk_state",
                                        "mc_world_chunk",
                                        "mc_chunk_data",
                                        "mc_world_chunk_nibble",
                                        "mc_world_chunk_list_array",
                                        "mc_world_chunk_entity_list",
                                        "mc_world_chunk_tile_map",
                                        "C.uint8_span",
                                        "C.double_span",
                                        "C.float_span",
                                        "mc_biome_reference_array",
                                        "C.mc_noise_perlin_group",
                                        "mc_noise_perlin",
                                        "C.mc_noise_simplex_group",
                                        "mc_noise_simplex",
                                        "C.uint16_permutation_span",
                                        "native.normal_cave_subobject",
                                        "C.int32_matrix",
                                        "C.int32_row",
                                        "mc_world_light_region",
                                        "mc_world_light_type",
                                        "mc_server_reference_list",
                                        "mc_server_synchronized_list",
                                        "mc_server_tracker_array",
                                        "mc_world_entity_index",
                                        "C.entity_index_pointer_table",
                                        "mc_java_random_profile",
                                        "mc_java_math_random_owner",
                                        "C.constructor_input_span",
                                        "mc_java_random",
                                        "C.mc_chunk_state_pointer_span",
                                        "mc_world_dimension_biome",
                                        "mc_temperature_array",
                                        "C.utf8_path_span",
                                        "mc_java_int_set"};
    _Static_assert(sizeof(names) / sizeof(names[0]) == D_REFERENCE_KIND_COUNT,
                   "complete native reference kinds");
    return (unsigned int)kind < sizeof(names) / sizeof(names[0]) ? names[kind] : NULL;
}

static bool dedicated_ref(natural_probe *probe, const void *pointer,
                          dedicated_reference_kind kind) {
    if (pointer == NULL)
        return emit(&probe->json, "null");
    const char *name = dedicated_kind(kind);
    if (name == NULL)
        return dedicated_fail(probe, "Unknown native reference kind");
    dedicated_observation *observation = probe->dedicated;
    size_t index = 0;
    while (index < observation->reference_count &&
           (observation->references[index].pointer != pointer ||
            observation->references[index].kind != kind))
        ++index;
    if (index == observation->reference_count) {
        if (index == DEDICATED_REFERENCE_LIMIT)
            return dedicated_fail(probe, "Dedicated reference capacity");
        observation->references[index] = (dedicated_reference){pointer, kind};
        ++observation->reference_count;
    }
    return emit(&probe->json, "{\"ref\":\"d%zu\",\"native_kind\":", index) &&
           string(&probe->json, name) && emit(&probe->json, "}");
}

static bool dedicated_units(natural_probe *probe, const uint16_t *units, size_t count) {
    if (count > 65536u || (count != 0 && units == NULL))
        return dedicated_fail(probe, "Complete UTF16 observation bound");
    if (!emit(&probe->json, "\""))
        return false;
    for (size_t i = 0; i < count; ++i)
        if (!emit(&probe->json, "%04x", (unsigned int)units[i]))
            return false;
    return emit(&probe->json, "\"");
}

static bool dedicated_name(natural_probe *probe, const mc_world_info_name *name) {
    if (name == NULL)
        return emit(&probe->json, "null");
    mc_nbt_string text = mc_world_info_name_text(name);
    return emit(&probe->json, "{\"owner\":") && dedicated_ref(probe, name, D_INFO_NAME) &&
           emit(&probe->json, ",\"utf16\":") && dedicated_units(probe, text.units, text.length) &&
           emit(&probe->json, "}");
}

static bool dedicated_registry_string(natural_probe *probe, const mc_registry_string *text) {
    if (text == NULL)
        return emit(&probe->json, "null");
    mc_registry_string_view view;
    if (!mc_registry_string_read(text, &view))
        return dedicated_fail(probe, "Actual Registry String view");
    return emit(&probe->json, "{\"owner\":") && dedicated_ref(probe, text, D_REGISTRY_STRING) &&
           emit(&probe->json, ",\"utf16\":") && dedicated_units(probe, view.units, view.length) &&
           emit(&probe->json, "}");
}

static uint32_t dedicated_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool dedicated_bytes(natural_probe *probe, const uint8_t *data, size_t count) {
    if (data == NULL)
        return count == 0 ? emit(&probe->json, "null")
                          : dedicated_fail(probe, "Nonnull complete byte span");
    return emit(&probe->json, "{\"owner\":") && dedicated_ref(probe, data, D_BYTES) &&
           emit(&probe->json, ",\"data\":") && byte_array(&probe->json, data, count) &&
           emit(&probe->json, "}");
}

static bool dedicated_doubles(natural_probe *probe, const double *data, size_t count) {
    if (data == NULL)
        return count == 0 ? emit(&probe->json, "null")
                          : dedicated_fail(probe, "Nonnull complete double span");
    if (count > 32768u || !emit(&probe->json, "{\"owner\":") ||
        !dedicated_ref(probe, data, D_DOUBLES) ||
        !emit(&probe->json, ",\"length\":%zu,\"values\":\"", count))
        return dedicated_fail(probe, "Complete double span bound");
    for (size_t i = 0; i < count; ++i)
        if (!emit(&probe->json, "%016" PRIx64, double_bits(data[i])))
            return false;
    return emit(&probe->json, "\"}");
}

static bool dedicated_floats(natural_probe *probe, const float *data, size_t count) {
    if (data == NULL || count > 32768u)
        return dedicated_fail(probe, "Complete float span binding");
    if (!emit(&probe->json, "{\"owner\":") || !dedicated_ref(probe, data, D_FLOATS) ||
        !emit(&probe->json, ",\"length\":%zu,\"values\":\"", count))
        return false;
    for (size_t i = 0; i < count; ++i)
        if (!emit(&probe->json, "%08" PRIx32, dedicated_float_bits(data[i])))
            return false;
    return emit(&probe->json, "\"}");
}

static bool dedicated_random(natural_probe *probe, const mc_java_random *random) {
    if (random == NULL)
        return dedicated_fail(probe, "Actual ordinary Random owner");
    return emit(&probe->json, "{\"owner\":") && dedicated_ref(probe, random, D_RANDOM) &&
           emit(&probe->json, ",\"value\":") && random_state(&probe->json, random) &&
           emit(&probe->json, "}");
}

static bool dedicated_list(natural_probe *probe, const mc_world_reference_list *list,
                           bool allow_lights) {
    if (list == NULL || list->count > list->capacity ||
        list->capacity > SIZE_MAX / sizeof(*list->items) ||
        (list->capacity != 0 && list->items == NULL) || (!allow_lights && list->count != 0))
        return dedicated_fail(probe, "Unsupported actual World List contents");
    return emit(&probe->json, "{\"owner\":") && dedicated_ref(probe, list, D_LIST) &&
           emit(&probe->json, ",\"size\":%zu,\"entries\":", list->count) &&
           emit(&probe->json, allow_lights ? "null,\"entries_in\":\"lights.records\"}" : "[]}");
}

static bool dedicated_base_world(natural_probe *probe, const mc_world_runtime_fields *fields) {
    natural_json *json = &probe->json;
    if (!emit(json, "{\"owner\":") || !dedicated_ref(probe, probe->world, D_WORLD) ||
        !emit(json,
              ",\"fields\":{\"a\":%s,\"H\":\"%016" PRIx64 "\",\"f\":%" PRId32 ",\"g\":%" PRId32
              ",\"h\":%" PRId32 ",\"i\":\"%08" PRIx32 "\",\"j\":\"%08" PRIx32
              "\",\"k\":\"%08" PRIx32 "\",\"l\":\"%08" PRIx32 "\",\"m\":%" PRId32 ",\"n\":%" PRId32
              ",\"o\":%s,\"I\":\"%016" PRIx64 "\",\"p\":%" PRId32 ",\"q\":%" PRId32
              ",\"s\":%s,\"y\":%s,\"J\":%s,\"L\":%s,\"M\":%" PRId32
              ",\"N\":%s,\"O\":%s,\"Q\":%" PRId32 ",\"B\":%s",
              boolean(fields->a), (uint64_t)fields->H, fields->f, fields->g, fields->h,
              dedicated_float_bits(fields->i), dedicated_float_bits(fields->j),
              dedicated_float_bits(fields->k), dedicated_float_bits(fields->l), fields->m,
              fields->n, boolean(fields->o), (uint64_t)fields->I, fields->p, fields->q,
              boolean(fields->s), boolean(fields->y), boolean(fields->J), boolean(fields->L),
              fields->M, boolean(fields->N), boolean(fields->O), fields->Q, boolean(fields->B)))
        return false;
    const char *const list_names[] = {"C", "b", "D", "c", "G", "d", "e", "u", "K", "R"};
    const mc_world_reference_list *lists[] = {fields->C, fields->b, fields->D, fields->c,
                                              fields->G, fields->d, fields->e, fields->u,
                                              fields->K, fields->R};
    for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); ++i)
        if (!emit(json, ",\"%s\":", list_names[i]) || !dedicated_ref(probe, lists[i], D_LIST))
            return false;
    const char *const names[] = {"E", "F", "r", "t", "v", "w", "x", "z", "P"};
    const void *owners[] = {fields->E, fields->F, fields->r, fields->t, fields->v,
                            fields->w, fields->x, fields->z, fields->P};
    const dedicated_reference_kind kinds[] = {D_ORDERED,   D_MEMBERSHIP, D_RANDOM,
                                              D_DIMENSION, D_PROVIDER,   D_STORE,
                                              D_INFO,      D_SAVED_DATA, D_ACTIVE_SET};
    for (size_t i = 0; i < sizeof(owners) / sizeof(owners[0]); ++i)
        if (!emit(json, ",\"%s\":", names[i]) || !dedicated_ref(probe, owners[i], kinds[i]))
            return false;
    if (!emit(json, "},\"collections\":{"))
        return false;
    for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); ++i)
        if (!emit(json, "%s\"%s\":", i == 0 ? "" : ",", list_names[i]) ||
            !dedicated_list(probe, lists[i], i == 0))
            return false;
    /* The real private P descriptor is exposed by construction_internal.h. */
    if (fields->P == NULL || fields->P->count != 0 || fields->P->count > fields->P->capacity ||
        (fields->P->capacity != 0 && fields->P->entries == NULL))
        return dedicated_fail(probe, "Unsupported actual active Chunk set");
    return emit(json, ",\"P\":{\"owner\":") && dedicated_ref(probe, fields->P, D_ACTIVE_SET) &&
           emit(json, ",\"size\":%zu,\"entries\":[]}}}", fields->P->count);
}

static bool dedicated_server(natural_probe *probe) {
    mc_server_runtime_fields fields;
    mc_server_construction state;
    if (mc_server_runtime_inspect(probe->dedicated->server, &fields, &state) != MC_SERVER_OK ||
        state.phase != MC_SERVER_READY || fields.c != NULL || fields.d != NULL ||
        fields.e != NULL || fields.f != NULL || fields.p != NULL)
        return dedicated_fail(probe, "Unsupported actual Server service graph");
    const void *const *r_items = NULL;
    const void *const *s_items = NULL;
    size_t r_count = 0, s_count = 0, k_count = 0;
    const mc_server_reference_list *backing = NULL;
    mc_server_tracker *const *trackers = NULL;
    if (mc_server_reference_list_view(fields.r, &r_items, &r_count) != MC_SERVER_OK ||
        mc_server_synchronized_list_view(fields.s, &backing) != MC_SERVER_OK ||
        mc_server_reference_list_view(backing, &s_items, &s_count) != MC_SERVER_OK ||
        mc_server_tracker_array_view(fields.k, &trackers, &k_count) != MC_SERVER_OK ||
        fields.r == backing || r_count != 0 || s_count != 0 || k_count != 2 || trackers == NULL ||
        trackers[0] != NULL || trackers[1] != NULL)
        return dedicated_fail(probe, "Unsupported actual Server collections");
    natural_json *json = &probe->json;
    return emit(json, "{\"owner\":") && dedicated_ref(probe, probe->dedicated->server, D_SERVER) &&
           emit(json,
                ",\"fields\":{\"c\":null,\"d\":null,\"e\":null,\"f\":null,\"p\":null,"
                "\"q\":%s,\"g\":%s,\"h\":%" PRId32 ",\"i\":",
                boolean(fields.q), boolean(fields.g), fields.h) &&
           dedicated_name(probe, fields.i) &&
           emit(json, ",\"j\":%" PRId32 ",\"l\":%s,\"m\":%s,\"n\":%s,\"o\":%s,\"r\":", fields.j,
                boolean(fields.l), boolean(fields.m), boolean(fields.n), boolean(fields.o)) &&
           dedicated_ref(probe, fields.r, D_SERVER_LIST) && emit(json, ",\"s\":") &&
           dedicated_ref(probe, fields.s, D_SERVER_SYNC) && emit(json, ",\"k\":") &&
           dedicated_ref(probe, fields.k, D_SERVER_TRACKERS) && emit(json, "},\"r\":{\"owner\":") &&
           dedicated_ref(probe, fields.r, D_SERVER_LIST) &&
           emit(json, ",\"size\":%zu,\"entries\":[]},\"s\":{\"owner\":", r_count) &&
           dedicated_ref(probe, fields.s, D_SERVER_SYNC) &&
           emit(json, ",\"size\":%zu,\"entries\":[]},\"k\":{\"owner\":", s_count) &&
           dedicated_ref(probe, fields.k, D_SERVER_TRACKERS) &&
           emit(json, ",\"length\":%zu,\"entries\":[null,null]},\"native_private\":{\"s_backing\":",
                k_count) &&
           dedicated_ref(probe, backing, D_SERVER_LIST) &&
           emit(json, ",\"lifetime_started\":%s,\"references\":%zu}}",
                boolean(state.lifetime_started), state.references);
}

static bool dedicated_derived(natural_probe *probe,
                              const mc_world_runtime_dedicated_fields *fields) {
    mc_world_entity_index_view index;
    mc_world_entity_index_error error;
    if (fields->C != probe->world->fields.v || fields->F != probe->dedicated->server ||
        mc_world_entity_index_inspect(fields->G, &index, &error) != MC_WORLD_STORAGE_OK ||
        index.owner != fields->G || index.a == NULL || index.a_length > 4096u || index.b != 0)
        return dedicated_fail(probe, "Actual dedicated fields or nonempty Entity index");
    natural_json *json = &probe->json;
    if (!emit(json, "{\"fields\":{\"C\":") || !dedicated_ref(probe, fields->C, D_PROVIDER) ||
        !emit(json, ",\"D\":%s,\"E\":%s,\"F\":", boolean(fields->D), boolean(fields->E)) ||
        !dedicated_ref(probe, fields->F, D_SERVER) || !emit(json, ",\"G\":") ||
        !dedicated_ref(probe, fields->G, D_INDEX) ||
        !emit(json, "},\"entity_index\":{\"owner\":") ||
        !dedicated_ref(probe, index.owner, D_INDEX) || !emit(json, ",\"a\":{\"owner\":") ||
        !dedicated_ref(probe, index.a, D_INDEX_TABLE) ||
        !emit(json, ",\"length\":%zu,\"entries\":[", index.a_length))
        return false;
    for (size_t i = 0; i < index.a_length; ++i)
        if (index.a[i] != NULL)
            return dedicated_fail(probe, "Unexpected actual Entity index node");
        else if (!emit(json, "%snull", i == 0 ? "" : ","))
            return false;
    return emit(
        json, "]},\"b\":%" PRId32 ",\"c\":%" PRId32 ",\"d\":\"%08" PRIx32 "\",\"e\":%" PRId32 "}}",
        index.b, index.c, dedicated_float_bits(index.d), index.e);
}

static bool dedicated_retain_chunk(natural_probe *probe, mc_chunk_state *chunk) {
    if (chunk == NULL)
        return dedicated_fail(probe, "Nonnull observed Chunk");
    dedicated_observation *observation = probe->dedicated;
    for (size_t i = 0; i < observation->retained_count; ++i)
        if (observation->retained[i] == chunk)
            return true;
    if (observation->retained_count == NATURAL_IDENTITIES)
        return dedicated_fail(probe, "Retained Chunk identity capacity");
    observation->retained[observation->retained_count++] = chunk;
    return true;
}

static bool dedicated_cache(natural_probe *probe) {
    mc_chunk_cache *cache = probe->provider.cache;
    const mc_chunk_pending *pending = probe->provider.pending;
    if (cache == NULL || pending == NULL || probe->provider.force_load == NULL ||
        !mc_chunk_cache_validate(cache) || !mc_chunk_pending_validate(pending) ||
        cache->count > NATURAL_CAPACITY || cache->insertion_count > NATURAL_CAPACITY ||
        pending->count > NATURAL_CAPACITY ||
        !dedicated_retain_chunk(probe, probe->provider.fallback))
        return dedicated_fail(probe, "Actual dedicated cache descriptors");
    natural_json *json = &probe->json;
    if (!emit(json, "{\"owner\":") || !dedicated_ref(probe, probe->world->fields.v, D_PROVIDER) ||
        !emit(json, ",\"force_load\":%s,\"world\":", boolean(*probe->provider.force_load)) ||
        !dedicated_ref(probe, probe->provider.world, D_WORLD) || !emit(json, ",\"fallback\":") ||
        !dedicated_ref(probe, probe->provider.fallback, D_CHUNK) || !emit(json, ",\"loader\":") ||
        !dedicated_ref(probe, &probe->world->loader, D_LOADER) || !emit(json, ",\"generator\":") ||
        !dedicated_ref(probe, probe->provider.generator, D_GENERATOR) ||
        !emit(json, ",\"map\":{\"owner\":") || !dedicated_ref(probe, cache, D_CACHE) ||
        !emit(json, ",\"size\":%zu,\"order\":\"signed_key_transport\",\"entries\":[", cache->count))
        return false;
    size_t order[NATURAL_CAPACITY];
    for (size_t i = 0; i < cache->count; ++i)
        order[i] = i;
    for (size_t i = 1; i < cache->count; ++i) {
        size_t j = i;
        while (j != 0 && cache->entries[order[j - 1u]].key > cache->entries[order[j]].key) {
            const size_t swap = order[j];
            order[j] = order[j - 1u];
            order[--j] = swap;
        }
    }
    for (size_t i = 0; i < cache->count; ++i) {
        const mc_chunk_cache_entry *entry = &cache->entries[order[i]];
        if (!dedicated_retain_chunk(probe, entry->chunk) ||
            !emit(json, "%s{\"key\":%" PRId32 ",\"value\":", i == 0 ? "" : ",", entry->key) ||
            !dedicated_ref(probe, entry->chunk, D_CHUNK) || !emit(json, "}"))
            return false;
    }
    if (!emit(json, "],\"native_entry_order\":["))
        return false;
    for (size_t i = 0; i < cache->count; ++i)
        if (!emit(json, "%s%" PRId32, i == 0 ? "" : ",", cache->entries[i].key))
            return false;
    if (!emit(json, "]},\"insertion_list\":{\"native_owner\":") ||
        !dedicated_ref(probe, cache->insertions, D_CHUNK_POINTERS) ||
        !emit(json, ",\"size\":%zu,\"entries\":[", cache->insertion_count))
        return false;
    for (size_t i = 0; i < cache->insertion_count; ++i)
        if (!dedicated_retain_chunk(probe, cache->insertions[i]) ||
            !emit(json, "%s", i == 0 ? "" : ",") ||
            !dedicated_ref(probe, cache->insertions[i], D_CHUNK))
            return false;
    if (!emit(json, "]},\"pending\":{\"owner\":") || !dedicated_ref(probe, pending, D_PENDING) ||
        !emit(json, ",\"size\":%zu,\"order\":\"native_insertion_survivor\",\"keys\":[",
              pending->count))
        return false;
    for (size_t i = 0; i < pending->count; ++i)
        if (!emit(json, "%s%" PRId32, i == 0 ? "" : ",", pending->keys[i]))
            return false;
    int32_t selected[NATURAL_CAPACITY];
    size_t selected_count = 0;
    if (pending->history == NULL || mc_java_int_set_size(pending->history) != pending->count ||
        mc_java_int_set_copy_order(pending->history, selected, NATURAL_CAPACITY, &selected_count) !=
            MC_JAVA_INT_SET_OK ||
        selected_count != pending->count)
        return dedicated_fail(probe, "Actual pending selection history");
    if (!emit(json, "],\"selection_history\":{\"native_owner\":") ||
        !dedicated_ref(probe, pending->history, D_INT_SET) ||
        !emit(json, ",\"order\":\"actual_JDK_profile_selection\",\"keys\":["))
        return false;
    for (size_t i = 0; i < selected_count; ++i)
        if (!emit(json, "%s%" PRId32, i == 0 ? "" : ",", selected[i]))
            return false;
    return emit(json, "]}}}");
}

static bool dedicated_permutation(natural_probe *probe, const uint16_t *values) {
    if (values == NULL || !emit(&probe->json, "{\"owner\":") ||
        !dedicated_ref(probe, values, D_PERMUTATION) ||
        !emit(&probe->json, ",\"length\":512,\"element_bits\":16,\"values\":["))
        return false;
    for (size_t i = 0; i < 512u; ++i)
        if (!emit(&probe->json, "%s%u", i == 0 ? "" : ",", (unsigned int)values[i]))
            return false;
    return emit(&probe->json, "]}");
}

static bool dedicated_perlin_group(natural_probe *probe, const mc_noise_perlin *layers,
                                   size_t count) {
    natural_json *json = &probe->json;
    if (layers == NULL || count > 32u || !emit(json, "{\"native_span\":") ||
        !dedicated_ref(probe, layers, D_PERLIN_GROUP) ||
        !emit(json, ",\"count\":%zu,\"layers\":[", count))
        return dedicated_fail(probe, "Actual Perlin group span");
    for (size_t i = 0; i < count; ++i) {
        const mc_noise_perlin *layer = &layers[i];
        if (!emit(json, "%s{\"owner\":", i == 0 ? "" : ",") ||
            !dedicated_ref(probe, layer, D_PERLIN) ||
            !emit(json,
                  ",\"offset_x\":\"%016" PRIx64 "\",\"offset_y\":\"%016" PRIx64
                  "\",\"offset_z\":\"%016" PRIx64 "\",\"permutation\":",
                  double_bits(layer->offset_x), double_bits(layer->offset_y),
                  double_bits(layer->offset_z)) ||
            !dedicated_permutation(probe, layer->permutation) || !emit(json, "}"))
            return false;
    }
    return emit(json, "]}");
}

static bool dedicated_simplex_group(natural_probe *probe, const mc_noise_simplex *layers,
                                    size_t count) {
    natural_json *json = &probe->json;
    if (layers == NULL || count > 16u || !emit(json, "{\"native_span\":") ||
        !dedicated_ref(probe, layers, D_SIMPLEX_GROUP) ||
        !emit(json, ",\"count\":%zu,\"layers\":[", count))
        return dedicated_fail(probe, "Actual simplex group span");
    for (size_t i = 0; i < count; ++i) {
        const mc_noise_simplex *layer = &layers[i];
        if (!emit(json, "%s{\"owner\":", i == 0 ? "" : ",") ||
            !dedicated_ref(probe, layer, D_SIMPLEX) ||
            !emit(json,
                  ",\"offset_x\":\"%016" PRIx64 "\",\"offset_z\":\"%016" PRIx64
                  "\",\"offset_unused\":\"%016" PRIx64 "\",\"permutation\":",
                  double_bits(layer->offset_x), double_bits(layer->offset_z),
                  double_bits(layer->offset_unused)) ||
            !dedicated_permutation(probe, layer->permutation) || !emit(json, "}"))
            return false;
    }
    return emit(json, "]}");
}

static bool dedicated_biome_array(natural_probe *probe, const mc_biome_reference_array *array) {
    /* These handles and replaced arrays remain in the live Registry arena.
     * Supplier replacement releases a manager hold, never an array allocation. */
    if (array == NULL)
        return emit(&probe->json, "null");
    const size_t count = mc_biome_reference_array_length(array);
    const mc_biome *const *values = mc_biome_reference_array_data(array);
    if (values == NULL || count > 32768u || !emit(&probe->json, "{\"owner\":") ||
        !dedicated_ref(probe, array, D_BIOME_ARRAY) ||
        !emit(&probe->json, ",\"length\":%zu,\"entries\":[", count))
        return dedicated_fail(probe, "Actual Biome reference array");
    for (size_t i = 0; i < count; ++i)
        if (!emit(&probe->json, "%s", i == 0 ? "" : ",") ||
            !dedicated_ref(probe, values[i], D_BIOME))
            return false;
    return emit(&probe->json, "]}");
}

static bool dedicated_grid(natural_probe *probe, int32_t (*grid)[MC_WORLD_PROVIDER_GRID_WIDTH]) {
    natural_json *json = &probe->json;
    if (grid == NULL || !emit(json, "{\"owner\":") || !dedicated_ref(probe, grid, D_GRID) ||
        !emit(json, ",\"shape\":[32,32],\"element_bits\":32,\"rows\":["))
        return dedicated_fail(probe, "Actual provider matrix");
    for (size_t x = 0; x < MC_WORLD_PROVIDER_GRID_WIDTH; ++x) {
        if (!emit(json, "%s{\"owner\":", x == 0 ? "" : ",") ||
            !dedicated_ref(probe, grid[x], D_GRID_ROW) || !emit(json, ",\"values\":["))
            return false;
        for (size_t z = 0; z < MC_WORLD_PROVIDER_GRID_WIDTH; ++z)
            if (!emit(json, "%s%" PRId32, z == 0 ? "" : ",", grid[x][z]))
                return false;
        if (!emit(json, "]}"))
            return false;
    }
    return emit(json, "]}");
}

static bool dedicated_normal(natural_probe *probe) {
    mc_normal_generator *generator = probe->provider.generator;
    const mc_world_provider_generation_fields *generation = probe->provider.generation;
    if (generator == NULL || generation == NULL || !generator->initialized ||
        generator->phase != MC_NORMAL_GENERATOR_PHASE_READY ||
        generator->origin != MC_NORMAL_GENERATOR_PROVIDER ||
        generator->biome_manager != probe->provider.manager ||
        generator->trig != probe->classes->trig)
        return dedicated_fail(probe, "Actual GAME normal generator");
    natural_json *json = &probe->json;
    if (!emit(json, "{\"owner\":") || !dedicated_ref(probe, generator, D_GENERATOR) ||
        !emit(json, ",\"world\":") || !dedicated_ref(probe, probe->provider.world, D_WORLD) ||
        !emit(json, ",\"world_binding_location\":\"outer_provider.world\",\"biome_manager\":") ||
        !dedicated_ref(probe, generator->biome_manager, D_MANAGER) ||
        !emit(json, ",\"biome_owner_correspondence\":") ||
        !dedicated_ref(probe, probe->provider.biome, D_DIMENSION_BIOME) ||
        !emit(json, ",\"world_seed\":\"%016" PRIx64 "\",\"generator_random\":",
              (uint64_t)generator->world_seed) ||
        !dedicated_random(probe, &generator->generator_random) ||
        !emit(json,
              ",\"cave\":{\"representation\":\"split_fields_in_mc_normal_generator\",\"range\":"
              "%" PRId32 ",\"random\":",
              generator->cave_range) ||
        !dedicated_random(probe, &generator->cave_random) || !emit(json, "},\"noise\":{"))
        return false;
    const char *const names[] = {
        "low",          "high",        "mix",  "surface_primary", "surface_secondary",
        "column_scale", "column_bias", "later"};
    const mc_terrain_normal_state *terrain = &generator->terrain;
    const mc_noise_perlin *groups[] = {terrain->low_layers,
                                       terrain->high_layers,
                                       terrain->mix_layers,
                                       terrain->surface_primary_layers,
                                       terrain->surface_secondary_layers,
                                       terrain->column_scale_layers,
                                       terrain->column_bias_layers,
                                       terrain->later_layers};
    const size_t counts[] = {16u, 16u, 8u, 4u, 4u, 10u, 16u, 8u};
    for (size_t i = 0; i < sizeof(groups) / sizeof(groups[0]); ++i)
        if (!emit(json, "%s\"%s\":", i == 0 ? "" : ",", names[i]) ||
            !dedicated_perlin_group(probe, groups[i], counts[i]))
            return false;
    if (!emit(json, "},\"scratch\":{"))
        return false;
    const char *const span_names[] = {"q", "d", "e", "f", "g", "h", "r", "s", "t"};
    const mc_terrain_double_span *spans[] = {&generation->q, &generation->d, &generation->e,
                                             &generation->f, &generation->g, &generation->h,
                                             &generation->r, &generation->s, &generation->t};
    for (size_t i = 0; i < sizeof(spans) / sizeof(spans[0]); ++i)
        if (!emit(json, "%s\"%s\":", i == 0 ? "" : ",", span_names[i]) ||
            !dedicated_doubles(probe, spans[i]->data, spans[i]->length))
            return false;
    if (!emit(json, "},\"v\":") || !dedicated_biome_array(probe, generation->v) ||
        !emit(json, ",\"w\":"))
        return false;
    if (probe->provider.w == NULL) {
        if (!emit(json, "null"))
            return false;
    } else {
        mc_temperature_array_view view;
        if (!hold_temperature(probe, probe->provider.w) ||
            mc_temperature_array_get(probe->provider.w, &view) != MC_BIOME_OK ||
            !emit(json, "{\"owner\":") || !dedicated_ref(probe, probe->provider.w, D_TEMPERATURE) ||
            !emit(json, ",\"native_payload\":") ||
            !dedicated_doubles(probe, view.data, view.length) || !emit(json, "}"))
            return dedicated_fail(probe, "Actual provider temperature array");
    }
    return emit(json, ",\"grid\":") && dedicated_grid(probe, probe->provider.constructor_grid) &&
           emit(json, "}");
}

static bool dedicated_dimension(natural_probe *probe, const mc_world_runtime_fields *world) {
    mc_world_dimension_fields fields;
    mc_world_dimension_state state;
    if (mc_world_dimension_fields_get(world->t, &fields) != MC_WORLD_DIMENSION_OK ||
        mc_world_dimension_state_get(world->t, &state) != MC_WORLD_DIMENSION_OK ||
        state.kind != MC_WORLD_DIMENSION_OVERWORLD || state.phase != MC_WORLD_DIMENSION_READY ||
        fields.a != probe->world || fields.b != probe->provider.biome)
        return dedicated_fail(probe, "Actual dedicated normal Dimension");
    natural_json *json = &probe->json;
    return emit(json, "{\"owner\":") && dedicated_ref(probe, world->t, D_DIMENSION) &&
           emit(json, ",\"a\":") && dedicated_ref(probe, fields.a, D_WORLD) &&
           emit(json, ",\"b\":") && dedicated_ref(probe, fields.b, D_DIMENSION_BIOME) &&
           emit(json, ",\"c\":%s,\"d\":%s,\"e\":%s,\"f\":", boolean(fields.c), boolean(fields.d),
                boolean(fields.e)) &&
           dedicated_floats(probe, fields.f, 16u) &&
           emit(json, ",\"g\":%" PRId32 ",\"h\":", fields.g) &&
           dedicated_floats(probe, fields.h, 4u) && emit(json, "}");
}

static bool dedicated_manager(natural_probe *probe) {
    mc_world_dimension_biome_fields fields;
    mc_world_biome_game_fields game;
    if (mc_world_dimension_biome_fields_get(probe->provider.biome, &fields) !=
            MC_WORLD_DIMENSION_OK ||
        mc_world_dimension_biome_game_fields_get(probe->provider.biome, &game) !=
            MC_WORLD_DIMENSION_OK ||
        fields.fixed || fields.normal == NULL || fields.normal != probe->provider.manager ||
        game.registry != probe->registry)
        return dedicated_fail(probe, "Actual GAME biome manager");
    natural_json *json = &probe->json;
    if (!emit(json, "{\"owner\":") ||
        !dedicated_ref(probe, probe->provider.biome, D_DIMENSION_BIOME) ||
        !emit(json, ",\"native_numerical_subobject\":") ||
        !dedicated_ref(probe, fields.normal, D_MANAGER))
        return false;
    const mc_biome_double_buffer *buffers[] = {fields.a, fields.b, fields.c};
    for (size_t i = 0; i < sizeof(buffers) / sizeof(buffers[0]); ++i) {
        const mc_biome_double_buffer *buffer = buffers[i];
        if (buffer == NULL || (buffer->data != NULL && !hold_climate(probe, buffer)) ||
            !emit(json, ",\"%c\":", (int)('a' + i)) ||
            !dedicated_doubles(probe, buffer->data, buffer->length))
            return dedicated_fail(probe, "Actual nullable manager double array");
    }
    return emit(json, ",\"d\":") && dedicated_biome_array(probe, game.d) &&
           emit(json, ",\"simplex\":{\"temperature\":") &&
           dedicated_simplex_group(probe, fields.normal->temperature_layers, 4u) &&
           emit(json, ",\"rainfall\":") &&
           dedicated_simplex_group(probe, fields.normal->rainfall_layers, 4u) &&
           emit(json, ",\"auxiliary\":") &&
           dedicated_simplex_group(probe, fields.normal->auxiliary_layers, 2u) && emit(json, "}}");
}

static bool dedicated_spawn_list(natural_probe *probe, const mc_biome_spawn_list *list) {
    const size_t count = mc_biome_spawn_list_size(list);
    natural_json *json = &probe->json;
    if (list == NULL || count > 256u || !emit(json, "{\"owner\":") ||
        !dedicated_ref(probe, list, D_SPAWN_LIST) ||
        !emit(json, ",\"size\":%zu,\"entries\":[", count))
        return dedicated_fail(probe, "Actual Biome spawn List");
    for (size_t i = 0; i < count; ++i) {
        const mc_biome_spawn *entry = mc_biome_spawn_list_at(list, i);
        const mc_biome_spawn_fields *fields = mc_biome_spawn_fields_get(entry);
        const mc_entity_class_literal *canonical = NULL;
        mc_entity_class_literal_view type;
        if (entry == NULL || fields == NULL || fields->a == NULL ||
            mc_biome_entity_class_canonical(probe->registry, fields->a, &canonical, NULL) !=
                MC_REGISTRY_OK ||
            mc_entity_class_literal_inspect(canonical, &type) != MC_ENTITY_OK ||
            type.owner != probe->entities || type.source != MC_ENTITY_SERVER ||
            type.server_name == NULL)
            return dedicated_fail(probe, "Actual canonical Biome Entity Class join");
        if (!emit(json, "%s{\"owner\":", i == 0 ? "" : ",") ||
            !dedicated_ref(probe, entry, D_SPAWN) || !emit(json, ",\"logical_class\":") ||
            !dedicated_ref(probe, fields->a, D_LOGICAL_CLASS) ||
            !emit(json, ",\"canonical_class\":") ||
            !dedicated_ref(probe, canonical, D_CANONICAL_CLASS) ||
            !emit(json,
                  ",\"canonical_kind\":%u,\"canonical_server_name\":", (unsigned int)type.kind) ||
            !string(json, type.server_name) || !emit(json, ",\"weight\":%" PRId32 "}", fields->b))
            return false;
    }
    return emit(json, "]}");
}

static bool dedicated_biomes(natural_probe *probe, const mc_biome_registry_view *statics) {
    natural_json *json = &probe->json;
    if (probe->dedicated->boundary_ordinal == 0)
        return emit(json, "{\"observation\":\"unobserved_before_first_acquire\",\"statics\":null,"
                          "\"objects\":[]}");
    if (statics->phase != MC_REGISTRY_READY)
        return dedicated_fail(probe, "Reached Biome class is not READY");
    const mc_biome *values[] = {statics->a, statics->b, statics->c, statics->d, statics->e,
                                statics->f, statics->g, statics->h, statics->i, statics->j,
                                statics->k, statics->l, statics->m};
    if (!emit(json, "{\"observation\":\"read_after_acquire\",\"statics\":{"))
        return false;
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        if (values[i] == NULL || !emit(json, "%s\"%c\":", i == 0 ? "" : ",", (int)('a' + i)) ||
            !dedicated_ref(probe, values[i], D_BIOME))
            return dedicated_fail(probe, "Actual published Biome static");
    if (!emit(json, "},\"objects\":["))
        return false;
    size_t written = 0;
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        bool repeated = false;
        for (size_t j = 0; j < i; ++j)
            repeated = repeated || values[i] == values[j];
        if (repeated)
            continue;
        const mc_biome_fields *fields = mc_biome_fields_get(values[i]);
        const mc_biome_kind kind = mc_biome_kind_get(values[i]);
        if (fields == NULL || kind < 0 || kind >= MC_BIOME_KIND_COUNT ||
            !emit(json, "%s{\"owner\":", written++ == 0 ? "" : ",") ||
            !dedicated_ref(probe, values[i], D_BIOME) ||
            !emit(json, ",\"kind\":%u,\"n\":", (unsigned int)kind) ||
            !dedicated_registry_string(probe, fields->n) ||
            !emit(json,
                  ",\"o\":%" PRId32 ",\"p\":%d,\"q\":%d,\"r\":%" PRId32 ",\"v\":%s,\"w\":%s,\"s\":",
                  fields->o, (int)fields->p, (int)fields->q, fields->r, boolean(fields->v),
                  boolean(fields->w)) ||
            !dedicated_spawn_list(probe, fields->s) || !emit(json, ",\"t\":") ||
            !dedicated_spawn_list(probe, fields->t) || !emit(json, ",\"u\":") ||
            !dedicated_spawn_list(probe, fields->u) || !emit(json, "}"))
            return false;
    }
    return emit(json, "]}");
}

static bool dedicated_nibble(natural_probe *probe, const mc_world_chunk_nibble *nibble,
                             const uint8_t *expected) {
    mc_world_chunk_nibble_fields fields;
    if (nibble == NULL ||
        mc_world_chunk_nibble_fields_get(nibble, &fields) != MC_WORLD_STORAGE_OK ||
        fields.a != expected || fields.length != MC_CHUNK_NIBBLE_BYTES)
        return dedicated_fail(probe, "Actual Chunk nibble descriptor and payload binding");
    return emit(&probe->json, "{\"owner\":") && dedicated_ref(probe, nibble, D_NIBBLE) &&
           emit(&probe->json, ",\"a\":") && dedicated_bytes(probe, fields.a, fields.length) &&
           emit(&probe->json, "}");
}

static bool dedicated_chunk_children(natural_probe *probe, const mc_world_chunk_fields *fields) {
    size_t tiles = 0;
    if (fields->l == NULL || fields->m == NULL ||
        mc_world_chunk_tile_count(fields->l, &tiles) != MC_WORLD_STORAGE_OK || tiles != 0)
        return dedicated_fail(probe, "Unsupported nonempty actual Chunk Tile map");
    natural_json *json = &probe->json;
    if (!emit(json, "\"l\":{\"owner\":") || !dedicated_ref(probe, fields->l, D_TILE_MAP) ||
        !emit(json, ",\"size\":%zu,\"entries\":[]},\"m\":{\"owner\":", tiles) ||
        !dedicated_ref(probe, fields->m, D_SLICE_ARRAY) ||
        !emit(json, ",\"length\":8,\"entries\":["))
        return false;
    const mc_world_chunk_entity_list *slices[8] = {NULL};
    for (size_t i = 0; i < sizeof(slices) / sizeof(slices[0]); ++i) {
        size_t count = 0;
        if (mc_world_chunk_slice_at(fields->m, i, &slices[i]) != MC_WORLD_STORAGE_OK ||
            slices[i] == NULL ||
            mc_world_chunk_entity_count(slices[i], &count) != MC_WORLD_STORAGE_OK || count != 0)
            return dedicated_fail(probe, "Unsupported nonempty actual Chunk Entity slice");
        for (size_t j = 0; j < i; ++j)
            if (slices[j] == slices[i])
                return dedicated_fail(probe, "Aliased actual Chunk Entity slice owners");
        if (!emit(json, "%s{\"owner\":", i == 0 ? "" : ",") ||
            !dedicated_ref(probe, slices[i], D_SLICE) ||
            !emit(json, ",\"size\":%zu,\"entries\":[]}", count))
            return false;
    }
    const mc_world_chunk_entity_list *sentinel = slices[0];
    if (mc_world_chunk_slice_at(fields->m, 8u, &sentinel) != MC_WORLD_STORAGE_INVALID_ARGUMENT ||
        sentinel != slices[0])
        return dedicated_fail(probe, "Actual Chunk slice extent");
    return emit(json, "]}");
}

static bool dedicated_chunk(natural_probe *probe, mc_chunk_state *state) {
    natural_json *json = &probe->json;
    if (state == NULL || state->data == NULL || state->data->blocks == NULL ||
        state->data->blocks_capacity < MC_CHUNK_BLOCKS)
        return dedicated_fail(probe, "Complete actual Chunk data");
    mc_chunk_data *data = state->data;
    if (!emit(json, "{\"owner\":") || !dedicated_ref(probe, state, D_CHUNK) ||
        !emit(json, ",\"coordinates\":[%" PRId32 ",%" PRId32 "],\"blank\":%s,\"data_owner\":",
              state->x, state->z, boolean(state->blank)) ||
        !dedicated_ref(probe, data, D_CHUNK_DATA) ||
        !emit(json, ",\"minimum_height\":%" PRId32, data->minimum_height))
        return false;
    mc_world_chunk *chunk = mc_world_store_chunk_object(probe->store, state);
    if (state == probe->provider.fallback) {
        if (chunk != NULL || !state->blank ||
            !emit(json,
                  ",\"normal_children\":\"absent_for_embedded_fallback\",\"state\":{"
                  "\"active\":%s,\"populated\":%s,\"dirty\":%s,\"never_save\":%s,"
                  "\"has_entities\":%s,\"last_save_time\":\"%016" PRIx64
                  "\"},\"arrays\":{\"blocks\":",
                  boolean(state->active), boolean(state->terrain_populated), boolean(state->dirty),
                  boolean(state->never_save), boolean(state->has_entities),
                  (uint64_t)state->last_save_time) ||
            !dedicated_bytes(probe, data->blocks, MC_CHUNK_BLOCKS) ||
            !emit(json, ",\"metadata\":") ||
            !dedicated_bytes(probe, data->metadata, MC_CHUNK_NIBBLE_BYTES) ||
            !emit(json, ",\"skylight\":") ||
            !dedicated_bytes(probe, data->skylight, MC_CHUNK_NIBBLE_BYTES) ||
            !emit(json, ",\"block_light\":") ||
            !dedicated_bytes(probe, data->block_light, MC_CHUNK_NIBBLE_BYTES) ||
            !emit(json, ",\"heightmap\":") ||
            !dedicated_bytes(probe, data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES))
            return dedicated_fail(probe, "Actual embedded fallback representation");
        return emit(json, "}}");
    }
    mc_world_chunk_fields fields;
    if (chunk == NULL || state->blank || mc_world_chunk_state(chunk) != state ||
        !mc_world_store_chunk_source_enabled(probe->store, state) ||
        mc_world_chunk_fields_get(chunk, &fields) != MC_WORLD_STORAGE_OK ||
        fields.d != probe->world || fields.b != data->blocks || fields.h != data->heightmap ||
        fields.j != state->x || fields.k != state->z || fields.i != data->minimum_height ||
        fields.c != state->active || fields.n != state->terrain_populated ||
        fields.o != state->dirty || fields.p != state->never_save ||
        fields.q != state->has_entities || fields.r != state->last_save_time ||
        fields.e == fields.f || fields.e == fields.g || fields.f == fields.g)
        return dedicated_fail(probe, "Actual concrete Chunk header/state bindings");
    return emit(json, ",\"native_header\":") && dedicated_ref(probe, chunk, D_CHUNK_HEADER) &&
           emit(json, ",\"fields\":{\"b\":") && dedicated_bytes(probe, fields.b, MC_CHUNK_BLOCKS) &&
           emit(json, ",\"c\":%s,\"d\":", boolean(fields.c)) &&
           dedicated_ref(probe, fields.d, D_WORLD) && emit(json, ",\"e\":") &&
           dedicated_nibble(probe, fields.e, data->metadata) && emit(json, ",\"f\":") &&
           dedicated_nibble(probe, fields.f, data->skylight) && emit(json, ",\"g\":") &&
           dedicated_nibble(probe, fields.g, data->block_light) && emit(json, ",\"h\":") &&
           dedicated_bytes(probe, fields.h, MC_CHUNK_HEIGHTMAP_BYTES) &&
           emit(json, ",\"i\":%" PRId32 ",\"j\":%" PRId32 ",\"k\":%" PRId32 ",", fields.i, fields.j,
                fields.k) &&
           dedicated_chunk_children(probe, &fields) &&
           emit(json, ",\"n\":%s,\"o\":%s,\"p\":%s,\"q\":%s,\"r\":\"%016" PRIx64 "\"}}",
                boolean(fields.n), boolean(fields.o), boolean(fields.p), boolean(fields.q),
                (uint64_t)fields.r);
}

static bool dedicated_chunks(natural_probe *probe) {
    dedicated_observation *observation = probe->dedicated;
    if ((observation->old_far != NULL && !dedicated_retain_chunk(probe, observation->old_far)) ||
        (observation->reloaded_far != NULL &&
         !dedicated_retain_chunk(probe, observation->reloaded_far)) ||
        mc_world_store_game_stage_count(probe->store) != 0)
        return dedicated_fail(probe, "Unsupported incomplete GAME stage at a dedicated boundary");
    const size_t count = mc_world_store_chunk_count(probe->store);
    if (count > NATURAL_CAPACITY || observation->retained_count != count + 1u)
        return dedicated_fail(probe, "Complete Store/observed Chunk owner count");
    for (size_t i = 0; i < count; ++i) {
        mc_chunk_state *actual = NULL;
        if (mc_world_store_chunk_at(probe->store, i, &actual, &probe->inspection_error) !=
                MC_WORLD_STORAGE_OK ||
            actual == NULL || actual == probe->provider.fallback)
            return dedicated_fail(probe, "Actual Store Chunk owner enumeration");
        bool found = false;
        for (size_t j = 0; j < observation->retained_count; ++j)
            found = found || observation->retained[j] == actual;
        if (!found)
            return dedicated_fail(probe, "Unobserved owned Chunk at a complete boundary");
    }
    if (!emit(&probe->json, "{\"count\":%zu,\"old_far\":", observation->retained_count) ||
        !dedicated_ref(probe, observation->old_far, D_CHUNK) ||
        !emit(&probe->json, ",\"reloaded_far\":") ||
        !dedicated_ref(probe, observation->reloaded_far, D_CHUNK) ||
        !emit(&probe->json, ",\"objects\":["))
        return false;
    for (size_t i = 0; i < observation->retained_count; ++i)
        if (!emit(&probe->json, "%s", i == 0 ? "" : ",") ||
            !dedicated_chunk(probe, observation->retained[i]))
            return false;
    return emit(&probe->json, "]}");
}

static bool dedicated_lights(natural_probe *probe, const mc_world_runtime_fields *world) {
    mc_world_light_list_view view;
    if (mc_world_runtime_light_list_get(probe->world, &view, &probe->inspection_error) !=
            MC_WORLD_STORAGE_OK ||
        view.identity != world->C || view.count > DEDICATED_TICK_LIMIT ||
        view.count > view.capacity)
        return dedicated_fail(probe, "Complete actual light List");
    natural_json *json = &probe->json;
    if (!emit(json, "{\"owner\":") || !dedicated_ref(probe, view.identity, D_LIST) ||
        !emit(json, ",\"size\":%zu,\"records\":[", view.count))
        return false;
    const mc_world_light_type *types[2] = {NULL, NULL};
    size_t type_count = 0;
    for (size_t i = 0; i < view.count; ++i) {
        const mc_world_light_region *region = NULL;
        if (mc_world_runtime_light_at(probe->world, i, &region, &probe->inspection_error) !=
                MC_WORLD_STORAGE_OK ||
            region == NULL)
            return dedicated_fail(probe, "Actual light record occurrence");
        const mc_world_light_region_fields *fields =
            mc_world_runtime_light_region_fields_get(region);
        if (fields == NULL || fields->a == NULL || fields->a->classes != probe->classes)
            return dedicated_fail(probe, "Actual light record/type owner binding");
        bool found = false;
        for (size_t j = 0; j < type_count; ++j)
            found = found || types[j] == fields->a;
        if (!found) {
            if (type_count == 2u)
                return dedicated_fail(probe, "Unknown actual light type");
            types[type_count++] = fields->a;
        }
        const mc_light_bounds *bounds = &fields->bounds;
        if (!emit(json, "%s{\"owner\":", i == 0 ? "" : ",") ||
            !dedicated_ref(probe, region, D_LIGHT) || !emit(json, ",\"a\":") ||
            !dedicated_ref(probe, fields->a, D_LIGHT_TYPE) ||
            !emit(json,
                  ",\"bounds\":[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32
                  ",%" PRId32 "]}",
                  bounds->min_x, bounds->min_y, bounds->min_z, bounds->max_x, bounds->max_y,
                  bounds->max_z))
            return false;
    }
    if (!emit(json, "],\"types\":["))
        return false;
    for (size_t i = 0; i < type_count; ++i) {
        const mc_world_light_type_fields *fields = mc_world_runtime_light_type_fields_get(types[i]);
        if (fields == NULL || !emit(json, "%s{\"owner\":", i == 0 ? "" : ",") ||
            !dedicated_ref(probe, types[i], D_LIGHT_TYPE) ||
            !emit(json, ",\"kind\":%u,\"c\":%" PRId32 "}", (unsigned int)fields->kind, fields->c))
            return dedicated_fail(probe, "Actual light type fields");
    }
    return emit(json, "]}");
}

static bool dedicated_tick_refs(natural_probe *probe, const mc_scheduled_tick *const *ticks,
                                size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (ticks[i] == NULL || !emit(&probe->json, "%s", i == 0 ? "" : ",") ||
            !dedicated_ref(probe, ticks[i], D_TICK))
            return dedicated_fail(probe, "Actual scheduled occurrence");
        size_t j = 0;
        while (j < probe->dedicated->tick_count && probe->dedicated->ticks[j] != ticks[i])
            ++j;
        if (j == probe->dedicated->tick_count) {
            if (j == DEDICATED_TICK_LIMIT)
                return dedicated_fail(probe, "Complete scheduled record capacity");
            probe->dedicated->ticks[j] = ticks[i];
            ++probe->dedicated->tick_count;
        }
    }
    return true;
}

static bool dedicated_scheduled(natural_probe *probe, const mc_world_runtime_fields *world) {
    dedicated_observation *observation = probe->dedicated;
    int32_t e_count = 0, f_count = 0;
    size_t e_written = 0, f_written = 0;
    if (mc_scheduled_ordered_size(world->E, &e_count) != MC_SCHEDULED_ORDERED_OK ||
        mc_scheduled_membership_size(world->F, &f_count) != MC_SCHEDULED_MEMBERSHIP_OK ||
        e_count < 0 || f_count < 0 || (uint32_t)e_count > DEDICATED_TICK_LIMIT ||
        (uint32_t)f_count > DEDICATED_TICK_LIMIT ||
        mc_scheduled_ordered_snapshot(world->E, observation->ordered, DEDICATED_TICK_LIMIT,
                                      &e_written) != MC_SCHEDULED_ORDERED_OK ||
        mc_scheduled_membership_snapshot(world->F, observation->membership, DEDICATED_TICK_LIMIT,
                                         &f_written) != MC_SCHEDULED_MEMBERSHIP_OK ||
        e_written != (size_t)e_count || f_written != (size_t)f_count)
        return dedicated_fail(probe, "Complete actual scheduled collection inventories");
    observation->tick_count = 0;
    natural_json *json = &probe->json;
    if (!emit(json, "{\"E\":{\"owner\":") || !dedicated_ref(probe, world->E, D_ORDERED) ||
        !emit(json, ",\"size\":%" PRId32 ",\"order\":\"actual_structural_inorder\",\"entries\":[",
              e_count) ||
        !dedicated_tick_refs(probe, observation->ordered, e_written) ||
        !emit(json, "]},\"F\":{\"owner\":") || !dedicated_ref(probe, world->F, D_MEMBERSHIP) ||
        !emit(json, ",\"size\":%" PRId32 ",\"order\":\"native_bucket_traversal\",\"entries\":[",
              f_count) ||
        !dedicated_tick_refs(probe, observation->membership, f_written) ||
        !emit(json, "]},\"records\":["))
        return false;
    for (size_t i = 0; i < observation->tick_count; ++i) {
        mc_scheduled_tick_fields fields;
        if (mc_scheduled_tick_fields_get(observation->ticks[i], &fields) != MC_SCHEDULED_TICK_OK ||
            !emit(json, "%s{\"owner\":", i == 0 ? "" : ",") ||
            !dedicated_ref(probe, observation->ticks[i], D_TICK) ||
            !emit(json,
                  ",\"x\":%" PRId32 ",\"y\":%" PRId32 ",\"z\":%" PRId32 ",\"block\":%" PRId32
                  ",\"due_time\":\"%016" PRIx64 "\",\"sequence\":\"%016" PRIx64 "\"}",
                  fields.x, fields.y, fields.z, fields.block, (uint64_t)fields.due_time,
                  (uint64_t)fields.sequence))
            return dedicated_fail(probe, "Actual scheduled record fields");
    }
    return emit(json, "]}");
}

static bool dedicated_info(natural_probe *probe, const mc_world_runtime_fields *world) {
    mc_world_info_fields fields;
    mc_world_info_state state;
    if (world->x != probe->persistence->info ||
        mc_world_info_fields_get(world->x, &fields) != MC_WORLD_INFO_OK ||
        mc_world_info_state_get(world->x, &state) != MC_WORLD_INFO_OK ||
        state.side != MC_WORLD_INFO_SERVER || state.phase != MC_WORLD_INFO_READY ||
        fields.player != NULL)
        return dedicated_fail(probe, "Actual ready server Info and empty Player profile");
    const mc_world_info_scalars *value = &fields.value;
    natural_json *json = &probe->json;
    return emit(json, "{\"owner\":") && dedicated_ref(probe, world->x, D_INFO) &&
           emit(json,
                ",\"seed\":\"%016" PRIx64 "\",\"spawn_x\":%" PRId32 ",\"spawn_y\":%" PRId32
                ",\"spawn_z\":%" PRId32 ",\"time\":\"%016" PRIx64 "\",\"last_played\":\"%016" PRIx64
                "\",\"size_on_disk\":\"%016" PRIx64
                "\",\"player\":null,\"player_dimension\":%" PRId32 ",\"level_name\":",
                (uint64_t)value->seed, value->spawn_x, value->spawn_y, value->spawn_z,
                (uint64_t)value->time, (uint64_t)value->last_played, (uint64_t)value->size_on_disk,
                value->player_dimension) &&
           dedicated_name(probe, fields.name) &&
           emit(json,
                ",\"version\":%" PRId32 ",\"raining\":%s,\"rain_time\":%" PRId32
                ",\"thundering\":%s,\"thunder_time\":%" PRId32 "}",
                value->version, boolean(value->raining), value->rain_time,
                boolean(value->thundering), value->thunder_time);
}

static bool dedicated_saved_data(natural_probe *probe, const mc_world_runtime_fields *world) {
    mc_world_saved_data_view view;
    if (mc_world_saved_data_inspect(world->z, &view) != MC_WORLD_STORAGE_OK || !view.class_bound ||
        view.side != MC_WORLD_INFO_SERVER || view.storage == NULL || view.named_cache == NULL ||
        view.ordered_list == NULL || view.id_map == NULL || view.named_cache == view.ordered_list ||
        view.named_cache == view.id_map || view.ordered_list == view.id_map ||
        view.named_count != 0 || view.ordered_count != 0 || view.id_count != 0)
        return dedicated_fail(probe, "Unsupported actual SavedData descriptor graph or entries");
    natural_json *json = &probe->json;
    return emit(json, "{\"owner\":") && dedicated_ref(probe, world->z, D_SAVED_DATA) &&
           emit(json, ",\"native_storage_binding\":") &&
           dedicated_ref(probe, view.storage, D_PATH) &&
           emit(json, ",\"transport_store_correspondence\":") &&
           dedicated_ref(probe, world->w, D_STORE) && emit(json, ",\"named_cache\":{\"owner\":") &&
           dedicated_ref(probe, view.named_cache, D_SAVED_CACHE) &&
           emit(json,
                ",\"size\":%zu,\"entries\":[]},\"ordered_list\":{\"owner\":", view.named_count) &&
           dedicated_ref(probe, view.ordered_list, D_SAVED_LIST) &&
           emit(json, ",\"size\":%zu,\"entries\":[]},\"id_map\":{\"owner\":", view.ordered_count) &&
           dedicated_ref(probe, view.id_map, D_SAVED_IDS) &&
           emit(json, ",\"size\":%zu,\"entries\":[]},\"native_last_caught\":", view.id_count) &&
           storage(json, mc_world_saved_data_last_caught(world->z)) && emit(json, "}");
}

static bool dedicated_shared(natural_probe *probe, const mc_world_runtime_fields *world) {
    mc_world_runtime_class_state state;
    if (mc_world_runtime_classes_get(probe->classes, &state) != MC_WORLD_STORAGE_OK ||
        state.side != MC_WORLD_INFO_SERVER || !state.initialized)
        return dedicated_fail(probe, "Actual initialized World class");
    natural_json *json = &probe->json;
    if (!emit(json,
              "{\"world_enqueue_depth\":%" PRId32 ",\"world_drain_depth\":%" PRId32
              ",\"chunk_static_a\":%s,\"falling\":",
              state.enqueue_depth, world->M, boolean(probe->classes->chunk_a)))
        return false;
    if (probe->dedicated->boundary_ordinal == 0) {
        if (!emit(json, "{\"observation\":\"unobserved_before_first_acquire\",\"value\":null}"))
            return false;
    } else if (!emit(json, "{\"observation\":\"read_after_acquire\",\"value\":%s}",
                     boolean(probe->registry->gk_a)))
        return false;
    return emit(json, "}");
}

static bool dedicated_profile(natural_probe *probe) {
    mc_java_random_profile *profile = probe->profile;
    dedicated_observation *observation = probe->dedicated;
    mc_java_random_constructor_inputs_view inputs;
    if (profile == NULL || mc_world_runtime_random_profile(probe->world) != profile ||
        mc_java_random_profile_constructor_inputs_get(profile, &inputs) !=
            MC_JAVA_RANDOM_PROFILE_OK ||
        !inputs.bound || profile->constructor_inputs != observation->inputs ||
        inputs.count != observation->input_count ||
        inputs.count > MC_JAVA_RANDOM_PROFILE_MAX_CONSTRUCTOR_INPUTS ||
        (inputs.count != 0 && observation->inputs == NULL) || inputs.consumed > inputs.count)
        return dedicated_fail(probe, "Actual profile and constructor-input binding");
    const unsigned int phase = atomic_load_explicit(&profile->holder_phase, memory_order_acquire);
    const uint64_t uniquifier = atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
    const bool constructor_admitted =
        atomic_load_explicit(&profile->constructor_admitted, memory_order_relaxed);
    if ((phase != MC_JAVA_RANDOM_HOLDER_COLD && phase != MC_JAVA_RANDOM_HOLDER_READY) ||
        (phase == MC_JAVA_RANDOM_HOLDER_COLD && profile->math != NULL) ||
        (phase == MC_JAVA_RANDOM_HOLDER_READY && profile->math == NULL))
        return dedicated_fail(probe, "Nonquiescent or failed actual Math holder");
    natural_json *json = &probe->json;
    if (!emit(json, "{\"owner\":") || !dedicated_ref(probe, profile, D_PROFILE) ||
        !emit(json,
              ",\"uniquifier\":\"%016" PRIx64
              "\",\"constructor_admitted\":%s,\"inputs\":{\"native_span\":",
              uniquifier, boolean(constructor_admitted)) ||
        !dedicated_ref(probe, profile->constructor_inputs, D_INPUTS) ||
        !emit(json, ",\"bound\":%s,\"count\":%zu,\"consumed\":%zu,\"failure\":%u,\"values\":[",
              boolean(inputs.bound), inputs.count, inputs.consumed, (unsigned int)inputs.failure))
        return false;
    for (size_t i = 0; i < inputs.count; ++i)
        if (!emit(json,
                  "%s{\"committed_uniquifier\":\"%016" PRIx64 "\",\"nanotime\":\"%016" PRIx64 "\"}",
                  i == 0 ? "" : ",", profile->constructor_inputs[i].committed_uniquifier,
                  (uint64_t)profile->constructor_inputs[i].nanotime))
            return false;
    if (!emit(json, "]},\"math\":{\"phase\":%u,\"owner\":", phase) ||
        !dedicated_ref(probe, profile->math, D_MATH) || !emit(json, ",\"value\":"))
        return false;
    if (profile->math == NULL) {
        if (!emit(json, "null"))
            return false;
    } else {
        const mc_java_random copied = {
            .state = atomic_load_explicit(&profile->math->state, memory_order_relaxed),
            .next_next_gaussian = profile->math->next_next_gaussian,
            .have_next_next_gaussian = profile->math->have_next_next_gaussian};
        if (!random_state(json, &copied))
            return false;
    }
    return emit(json,
                "},\"native_private\":{\"allocation_bytes\":%zu,\"max_allocation_bytes\":%zu,"
                "\"state_storage\":\"inline_atomic_in_math_owner\",\"state_offset\":%zu}}",
                profile->allocated_bytes, profile->max_allocation_bytes,
                offsetof(mc_java_math_random_owner, state));
}

static bool dedicated_native_private(natural_probe *probe, const mc_world_runtime_fields *world,
                                     const mc_world_runtime_construction *construction,
                                     const mc_world_runtime_dedicated_construction *derived,
                                     const mc_biome_registry_view *biomes) {
    mc_world_schedule_class_state schedule;
    if (mc_world_runtime_schedule_class_get(probe->classes, &schedule) != MC_WORLD_STORAGE_OK ||
        probe->world->loader.store != world->w || probe->world->loader.dimension != 0 ||
        probe->provider.store != world->w || probe->provider.world != probe->world ||
        probe->provider.policy != MC_CHUNK_ACQUIRE_DEDICATED)
        return dedicated_fail(probe, "Actual native dedicated loader/provider bindings");
    if (probe->world->entity_membership.list != world->b ||
        probe->world->entity_deferral.list != world->D ||
        probe->world->nearby_entities.list != world->R ||
        probe->world->collision_boxes.list != world->K ||
        probe->world->tile_membership[0].identity != world->c ||
        probe->world->tile_membership[1].identity != world->G ||
        probe->world->entity_membership.count != 0 || probe->world->entity_deferral.count != 0 ||
        probe->world->nearby_entities.count != 0 || probe->world->collision_boxes.count != 0 ||
        probe->world->tile_membership[0].count != 0 ||
        probe->world->tile_membership[1].count != 0 || probe->world->entity_containers_retired ||
        probe->world->tile_containers_retired)
        return dedicated_fail(probe, "Unsupported actual retained Entity/Tile/collision ledger");
    const char *root = mc_world_store_root(world->w);
    if (root == NULL || strlen(root) > MC_WS_PATH_MAX_BYTES)
        return dedicated_fail(probe, "Actual Store path binding");
    natural_json *json = &probe->json;
    if (!emit(json,
              "{\"world_construction\":{\"mode\":%u,\"phase\":%u,\"entry\":%u,"
              "\"last_store_pc\":%u,\"failed_call_pc\":%u,\"contexts_bound\":%s,"
              "\"cache_registered\":%s,\"direct_allocation_bytes\":%zu},"
              "\"dedicated_construction\":{\"last_store_pc\":%u,\"failed_call_pc\":%u},"
              "\"registry\":{\"blocks_phase\":%u,\"biomes_phase\":%u},"
              "\"scheduled_class\":{\"initialized\":%s,\"next\":\"%016" PRIx64
              "\"},\"loader\":{\"owner\":",
              (unsigned int)construction->mode, (unsigned int)construction->phase,
              (unsigned int)construction->entry, construction->last_store_pc,
              construction->failed_call_pc, boolean(construction->contexts_bound),
              boolean(construction->cache_registered), construction->direct_allocation_bytes,
              derived->last_store_pc, derived->failed_call_pc,
              (unsigned int)probe->registry->blocks_phase, (unsigned int)biomes->phase,
              boolean(schedule.initialized), (uint64_t)schedule.next) ||
        !dedicated_ref(probe, &probe->world->loader, D_LOADER) || !emit(json, ",\"store\":") ||
        !dedicated_ref(probe, probe->world->loader.store, D_STORE) ||
        !emit(json, ",\"dimension\":%" PRId32 ",\"last_error\":", probe->world->loader.dimension) ||
        !storage(json, &probe->world->loader.last_error) ||
        !emit(json, "},\"store_root\":{\"native_span\":") || !dedicated_ref(probe, root, D_UTF8) ||
        !emit(json, ",\"utf8_bytes\":") || !byte_array(json, (const uint8_t *)root, strlen(root)) ||
        !emit(json,
              "},\"store_chunks\":%zu,\"store_pending_stages\":%zu,\"provider\":{"
              "\"max_chunks\":%zu,\"direct_allocation_bytes\":%zu,\"max_direct_allocation_bytes\":%"
              "zu},"
              "\"random_inline_state\":{\"storage\":\"inline_cells_in_actual_random_owner\","
              "\"state_offset\":%zu,\"state_bytes\":%zu,\"gaussian_value_offset\":%zu,\"gaussian_"
              "flag_offset\":%zu},"
              "\"jvm_internal_heap_identity\":false,\"server_waiter_representation\":\"native_"
              "lifetime_condition\","
              "\"world_native_lifetime\":{\"active_calls\":%zu,\"local_leases\":%zu,\"persistent_"
              "bindings\":%zu},"
              "\"world_ledgers\":{\"entity_membership\":%zu,\"entity_deferral\":%zu,\"nearby_"
              "entities\":%zu,"
              "\"collision_boxes\":%zu,\"tile_membership\":%zu,\"tile_deferral\":%zu},\"world_last_"
              "error\":",
              mc_world_store_chunk_count(probe->store),
              mc_world_store_game_stage_count(probe->store), probe->provider.max_chunks,
              probe->provider.direct_allocation_bytes, probe->provider.max_direct_allocation_bytes,
              offsetof(mc_java_random, state), sizeof(((mc_java_random *)0)->state),
              offsetof(mc_java_random, next_next_gaussian),
              offsetof(mc_java_random, have_next_next_gaussian), probe->world->active_calls,
              probe->world->local_leases, probe->world->persistent_bindings,
              probe->world->entity_membership.count, probe->world->entity_deferral.count,
              probe->world->nearby_entities.count, probe->world->collision_boxes.count,
              probe->world->tile_membership[0].count, probe->world->tile_membership[1].count) ||
        !storage(json, &probe->world->last_error) ||
        !emit(json, ",\"world_last_registry_error\":") ||
        !registry_diagnostic(json, &probe->world->last_registry_error) ||
        !emit(json, ",\"world_last_random_status\":%u}",
              (unsigned int)probe->world->last_random_status))
        return false;
    return true;
}

static bool dedicated_boundary(natural_probe *probe) {
    if (probe == NULL || probe->dedicated == NULL || probe->dedicated->boundary_ordinal >= 18u ||
        probe->dedicated->server == NULL || probe->persistence == NULL ||
        probe->persistence->root == NULL || probe->world == NULL || probe->classes == NULL ||
        probe->registry == NULL || probe->client)
        return probe == NULL ? false : dedicated_fail(probe, "Dedicated boundary owner admission");
    mc_world_runtime_fields world;
    mc_world_runtime_construction construction;
    mc_world_runtime_dedicated_fields derived;
    mc_world_runtime_dedicated_construction derived_construction;
    mc_biome_registry_view biomes;
    if (mc_world_runtime_fields_get(probe->world, &world) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_construction_get(probe->world, &construction) != MC_WORLD_STORAGE_OK ||
        mc_world_runtime_dedicated_get(probe->world, &derived, &derived_construction) !=
            MC_WORLD_STORAGE_OK ||
        construction.mode != MC_WORLD_RUNTIME_GAME ||
        construction.phase != MC_WORLD_RUNTIME_READY ||
        construction.entry != MC_WORLD_RUNTIME_SERVER_DEDICATED || world.w != probe->store ||
        world.x != probe->persistence->info || derived.F != probe->dedicated->server ||
        derived.C != world.v ||
        mc_world_provider_bind_fields(world.v, &probe->provider, &probe->inspection_error) !=
            MC_WORLD_STORAGE_OK ||
        mc_biome_registry_inspect(probe->registry, &biomes, NULL) != MC_REGISTRY_OK)
        return dedicated_fail(probe, "Actual dedicated boundary bindings");
    natural_json *json = &probe->json;
    const size_t start = json->length;
    const bool ok =
        emit(json,
             "{\"schema\":1,\"scope\":\"native-dedicated-game-domain-state\",\"ordinal\":%zu,"
             "\"base_world\":",
             probe->dedicated->boundary_ordinal) &&
        dedicated_base_world(probe, &world) && emit(json, ",\"dedicated\":") &&
        dedicated_derived(probe, &derived) && emit(json, ",\"server\":") &&
        dedicated_server(probe) && emit(json, ",\"cache\":") && dedicated_cache(probe) &&
        emit(json, ",\"normal\":") && dedicated_normal(probe) && emit(json, ",\"dimension\":") &&
        dedicated_dimension(probe, &world) && emit(json, ",\"manager\":") &&
        dedicated_manager(probe) && emit(json, ",\"biomes\":") &&
        dedicated_biomes(probe, &biomes) && emit(json, ",\"retained_chunks\":") &&
        dedicated_chunks(probe) && emit(json, ",\"lights\":") && dedicated_lights(probe, &world) &&
        emit(json, ",\"scheduled\":") && dedicated_scheduled(probe, &world) &&
        emit(json, ",\"info\":") && dedicated_info(probe, &world) &&
        emit(json, ",\"saved_data\":") && dedicated_saved_data(probe, &world) &&
        emit(json, ",\"shared\":") && dedicated_shared(probe, &world) &&
        emit(json, ",\"world_random\":") && dedicated_random(probe, world.r) &&
        emit(json, ",\"profile\":") && dedicated_profile(probe) && emit(json, ",\"disk\":") &&
        disk_inventory(probe) &&
        emit(json,
             ",\"correspondences\":{"
             "\"scope\":\"finite_game_fields_arrays_and_aliases\","
             "\"references\":\"actual_C_pointer_plus_native_kind\","
             "\"random\":\"semantic_owner_and_state_inline_not_AtomicLong_heap_identity\","
             "\"chunk\":\"mc_chunk_state_semantic_owner_with_native_header_relation\","
             "\"fallback\":\"embedded_state_with_actual_arrays_no_normal_child_handles\","
             "\"provider\":\"actual_outer_cache_and_embedded_numerical_generator\","
             "\"cave\":\"range_and_random_split_fields_no_separate_C_owner\","
             "\"noise_groups_grid_rows\":\"actual_C_spans_and_subobjects_not_Java_array_objects\","
             "\"permutations\":\"actual_uint16_cells_numeric_values_not_Java_int_storage\","
             "\"storage\":\"actual_native_path_binding_with_declared_Store_correspondence\","
             "\"membership_order\":\"native_bucket_order_only\","
             "\"initial_shared\":\"no_Block_or_Biome_active_use_from_observer\","
             "\"math_profile\":\"passive_native_actual_state_requires_separate_constructor_input_"
             "evidence\"},\"native_private\":") &&
        dedicated_native_private(probe, &world, &construction, &derived_construction, &biomes) &&
        emit(json, "}");
    if (ok) {
        ++probe->dedicated->boundary_ordinal;
        return true;
    }
    if (!json->failed) {
        json->length = start;
        json->bytes[start] = '\0';
        (void)emit(json, "null");
    }
    return false;
}

#endif
