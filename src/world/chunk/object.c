#include "object_state_internal.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(offsetof(generated_chunk_backing, chunk) == 0,
               "generated Chunk header is the allocation's first member");
_Static_assert(offsetof(generated_chunk_backing, state) % _Alignof(mc_chunk_state) == 0,
               "generated Chunk state alignment");
_Static_assert(offsetof(generated_chunk_backing, data) % _Alignof(mc_chunk_data) == 0,
               "generated Chunk data alignment");

static mc_world_storage_status chunk_result(mc_world_storage_error *error,
                                            mc_world_storage_status status, const char *operation) {
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static void nibble_construct(mc_world_chunk_nibble *nibble, uint8_t *storage,
                             size_t packed_length) {
    /* wi/ob.<init>(I)V: raw a=NULL, zero byte[input length >> 1] at8,
     * then final a store10. The fixed caller supplies that packed span. */
    nibble->a = NULL;
    nibble->length = 0;
    memset(storage, 0, packed_length);
    nibble->a = storage;
    nibble->length = packed_length;
}

static bool tile_entity_map_virgin(const mc_world_chunk_tile_entity_map *map) {
    return map->count == 0 && map->backing == NULL && map->release_backing == NULL &&
           map->backing_bytes == NULL;
}

static bool tile_entity_map_owned_bytes(const mc_world_chunk *chunk, size_t *out) {
    const mc_world_chunk_tile_entity_map *const map = &chunk->tile_entity_map;
    if (tile_entity_map_virgin(map)) {
        *out = 0;
        return true;
    }
    if (map->release_backing == NULL || map->backing_bytes == NULL)
        return false;

    /* This native inspection validates the actual owned descriptor, never
     * the mutable source l projection or a second lower ledger. A retired
     * descriptor retains both hooks but owns no allocation or occurrences. */
    size_t bytes = 0;
    if (!map->backing_bytes(chunk, &bytes) ||
        (map->backing == NULL && (map->count != 0 || bytes != 0)) ||
        (map->backing != NULL && bytes == 0))
        return false;
    *out = bytes;
    return true;
}

mc_world_storage_status mc_world_chunk_storage_create(mc_world_runtime *world, int32_t x, int32_t z,
                                                      uint8_t **blocks, size_t length,
                                                      size_t max_allocation_bytes,
                                                      mc_world_chunk **out,
                                                      mc_world_storage_error *error) {
    if (world == NULL || blocks == NULL || *blocks == NULL || length != (size_t)MC_CHUNK_BLOCKS ||
        out == NULL)
        return chunk_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                            "Chunk constructor arguments");

    if (length > SIZE_MAX - sizeof(generated_chunk_backing))
        return chunk_result(error, MC_WORLD_STORAGE_LIMIT, "Chunk constructor allocation size");
    const size_t allocation_bytes = sizeof(generated_chunk_backing) + length;
    if (allocation_bytes > max_allocation_bytes)
        return chunk_result(error, MC_WORLD_STORAGE_LIMIT, "Chunk constructor allocation budget");

    /* One native backing request precedes the source field-store sequence.
     * Child headers/spans are distinct embedded objects, not separate native
     * requests or a claim about Java collection layout/OOM timing. */
    generated_chunk_backing *backing = malloc(sizeof(*backing));
    if (backing == NULL)
        return chunk_result(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "Chunk constructor backing");
    mc_world_chunk *chunk = &backing->chunk;
    mc_chunk_state *state = &backing->state;
    mc_chunk_data *data = &backing->data;

    /* Actual raw primitive/reference defaults. Native data bindings and
     * ownership metadata are not source fields. Packed/height bytes remain
     * untouched until their individual source array-construction stages. */
    chunk->state = state;
    chunk->data = data;
    *state = (mc_chunk_state){0};
    state->data = data;
    data->blocks = NULL;
    data->blocks_capacity = 0;
    data->minimum_height = 0;
    chunk->d = NULL;
    chunk->e = NULL;
    chunk->f = NULL;
    chunk->g = NULL;
    chunk->h = NULL;
    chunk->l = NULL;
    chunk->m = NULL;
    chunk->storage_kind = CHUNK_STORAGE_GENERATED;
    chunk->bound_blocks = NULL;
    chunk->owned_blocks = NULL;
    chunk->constructor_x = x;
    chunk->constructor_z = z;
    chunk->allocation_bytes = allocation_bytes;
    chunk->max_allocation_bytes = max_allocation_bytes;
    chunk->entity_containers_retired = false;
    chunk->source_disabled = false;
    chunk->release_entity = NULL;

    /* lm/hi.<init>(World,II)V: fresh empty Map, l12. */
    chunk->tile_entity_map = (mc_world_chunk_tile_entity_map){0};
    chunk->l = &chunk->tile_entity_map;

    /* Fresh List[8] with null elements at18, m21. */
    chunk->slices.length = CHUNK_ENTITY_SLICES;
    for (size_t index = 0; index < CHUNK_ENTITY_SLICES; ++index)
        chunk->slices.items[index] = NULL;
    chunk->m = &chunk->slices;

    /* Explicit source stores n26/o31/q36/r41, d46, j51/k56. */
    state->terrain_populated = false;
    state->dirty = false;
    state->has_entities = false;
    state->last_save_time = 0;
    chunk->d = world;
    state->x = x;
    state->z = z;

    /* Zero height byte[256] at63, h65. */
    memset(data->heightmap, 0, sizeof(data->heightmap));
    chunk->h = data->heightmap;

    /* Eight separate empty List identities: ctor91/store94 in index order. */
    for (size_t index = 0; index < CHUNK_ENTITY_SLICES; ++index) {
        chunk->entities[index] = (mc_world_chunk_entity_list){0};
        chunk->slices.items[index] = &chunk->entities[index];
    }

    /* lm/hi.<init>(World,[B,II)V: the same supplied byte[] b10,
     * then distinct wi/ob objects e23/f36/g49. Never clear/copy b. */
    data->blocks = *blocks;
    data->blocks_capacity = length;
    chunk->bound_blocks = *blocks;
    nibble_construct(&chunk->metadata, data->metadata, length >> 1u);
    chunk->e = &chunk->metadata;
    nibble_construct(&chunk->skylight, data->skylight, length >> 1u);
    chunk->f = &chunk->skylight;
    nibble_construct(&chunk->block_light, data->block_light, length >> 1u);
    chunk->g = &chunk->block_light;

    /* No remaining failure/callback: transfer once, then publish. */
    chunk->owned_blocks = *blocks;
    *blocks = NULL;
    *out = chunk;
    return chunk_result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_chunk_storage_bind_loaded(mc_world_runtime *world,
                                                           mc_chunk_state *state,
                                                           size_t max_allocation_bytes,
                                                           mc_world_chunk **out,
                                                           mc_world_storage_error *error) {
    if (world == NULL || state == NULL || state->blank || state->data == NULL ||
        state->data->blocks == NULL || state->data->blocks_capacity != (size_t)MC_CHUNK_BLOCKS ||
        out == NULL)
        return chunk_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                            "loaded Chunk binding arguments");
    if (sizeof(mc_world_chunk) > max_allocation_bytes)
        return chunk_result(error, MC_WORLD_STORAGE_LIMIT, "loaded Chunk header budget");

    mc_world_chunk *chunk = malloc(sizeof(*chunk));
    if (chunk == NULL)
        return chunk_result(error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                            "loaded Chunk header allocation");

    /* The stored owner has already decoded its actual cells. Normalization
     * belongs to its caller's load protocol. Bind only reference/child headers;
     * never clear, copy,
     * rebind or resynchronize any borrowed state/data/array. */
    mc_chunk_data *data = state->data;
    *chunk = (mc_world_chunk){.state = state,
                              .data = data,
                              .d = world,
                              .h = data->heightmap,
                              .storage_kind = CHUNK_STORAGE_LOADED,
                              .bound_blocks = data->blocks,
                              .constructor_x = state->x,
                              .constructor_z = state->z,
                              .allocation_bytes = sizeof(*chunk),
                              .max_allocation_bytes = max_allocation_bytes};
    chunk->metadata = (mc_world_chunk_nibble){data->metadata, MC_CHUNK_NIBBLE_BYTES};
    chunk->skylight = (mc_world_chunk_nibble){data->skylight, MC_CHUNK_NIBBLE_BYTES};
    chunk->block_light = (mc_world_chunk_nibble){data->block_light, MC_CHUNK_NIBBLE_BYTES};
    chunk->e = &chunk->metadata;
    chunk->f = &chunk->skylight;
    chunk->g = &chunk->block_light;
    chunk->tile_entity_map = (mc_world_chunk_tile_entity_map){0};
    chunk->l = &chunk->tile_entity_map;
    chunk->slices.length = CHUNK_ENTITY_SLICES;
    for (size_t index = 0; index < CHUNK_ENTITY_SLICES; ++index)
        chunk->slices.items[index] = &chunk->entities[index];
    chunk->m = &chunk->slices;

    *out = chunk;
    return chunk_result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_chunk_state *mc_world_chunk_state(mc_world_chunk *chunk) {
    return chunk == NULL ? NULL : chunk->state;
}

size_t mc_world_chunk_allocation_bytes(const mc_world_chunk *chunk) {
    return chunk == NULL ? 0 : chunk->allocation_bytes;
}

mc_world_storage_status mc_world_chunk_fields_get(const mc_world_chunk *chunk,
                                                  mc_world_chunk_fields *out) {
    if (chunk == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    /* Inspection is passive, including when a caller has violated a mutable
     * data-binding invariant. In particular d never depends on empty/shape
     * validation, so a store World-lifetime query cannot miss its owner. */
    const mc_chunk_state *state = chunk->state;
    const mc_chunk_data *data = chunk->data;
    *out = (mc_world_chunk_fields){.b = data->blocks,
                                   .c = state->active,
                                   .d = chunk->d,
                                   .e = chunk->e,
                                   .f = chunk->f,
                                   .g = chunk->g,
                                   .h = chunk->h,
                                   .i = data->minimum_height,
                                   .j = state->x,
                                   .k = state->z,
                                   .l = chunk->l,
                                   .m = chunk->m,
                                   .n = state->terrain_populated,
                                   .o = state->dirty,
                                   .p = state->never_save,
                                   .q = state->has_entities,
                                   .r = state->last_save_time};
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_chunk_nibble_fields_get(const mc_world_chunk_nibble *nibble,
                                                         mc_world_chunk_nibble_fields *out) {
    if (nibble == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = (mc_world_chunk_nibble_fields){nibble->a, nibble->length};
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_chunk_slice_at(const mc_world_chunk_list_array *array,
                                                size_t index,
                                                const mc_world_chunk_entity_list **out) {
    if (array == NULL || out == NULL || index >= array->length)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = array->items[index];
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_chunk_tile_entity_count(const mc_world_chunk_tile_entity_map *map,
                                                         size_t *out) {
    if (map == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = map->count;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_chunk_entity_count(const mc_world_chunk_entity_list *list,
                                                    size_t *out) {
    if (list == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = list->count;
    return MC_WORLD_STORAGE_OK;
}

bool mc_world_chunk_storage_empty(const mc_world_chunk *chunk) {
    if (!mc_world_chunk_source_enabled(chunk) || !tile_entity_map_virgin(&chunk->tile_entity_map) ||
        !mc_world_chunk_entities_validate(chunk) || chunk->d == NULL || chunk->state == NULL ||
        chunk->data == NULL || chunk->state->blank || chunk->state->data != chunk->data ||
        chunk->bound_blocks == NULL || chunk->state->x != chunk->constructor_x ||
        chunk->state->z != chunk->constructor_z || chunk->data->blocks != chunk->bound_blocks ||
        chunk->data->blocks_capacity != (size_t)MC_CHUNK_BLOCKS || chunk->e != &chunk->metadata ||
        chunk->f != &chunk->skylight || chunk->g != &chunk->block_light ||
        chunk->h != chunk->data->heightmap || chunk->l != &chunk->tile_entity_map ||
        chunk->m != &chunk->slices || chunk->metadata.a != chunk->data->metadata ||
        chunk->metadata.length != (size_t)MC_CHUNK_NIBBLE_BYTES ||
        chunk->skylight.a != chunk->data->skylight ||
        chunk->skylight.length != (size_t)MC_CHUNK_NIBBLE_BYTES ||
        chunk->block_light.a != chunk->data->block_light ||
        chunk->block_light.length != (size_t)MC_CHUNK_NIBBLE_BYTES ||
        chunk->slices.length != CHUNK_ENTITY_SLICES)
        return false;
    if (chunk->storage_kind == CHUNK_STORAGE_GENERATED) {
        if (chunk->owned_blocks != chunk->bound_blocks)
            return false;
    } else if (chunk->storage_kind != CHUNK_STORAGE_LOADED || chunk->owned_blocks != NULL) {
        return false;
    }
    for (size_t index = 0; index < CHUNK_ENTITY_SLICES; ++index) {
        if (chunk->slices.items[index] != &chunk->entities[index] ||
            chunk->entities[index].count != 0)
            return false;
    }
    return true;
}

void mc_world_chunk_storage_free(mc_world_chunk *chunk) {
    if (chunk != NULL) {
        /* Generated header is the first member of its malloc backing. Loaded
         * headers have no owned blocks/tail; their stored payload outlives this
         * call and is freed separately by the store. */
        mc_world_chunk_containers_release(chunk);
        free(chunk->owned_blocks);
        free(chunk);
    }
}

static bool list_valid(const mc_world_chunk_entity_list *list) {
    return list != NULL && list->count <= list->capacity && list->capacity <= (size_t)INT32_MAX &&
           list->capacity <= SIZE_MAX / sizeof(*list->items) &&
           ((list->capacity == 0 && list->items == NULL) ||
            (list->capacity != 0 && list->items != NULL));
}

mc_world_storage_status mc_world_chunk_entity_at(const mc_world_chunk_entity_list *list,
                                                 size_t index, mc_entity **out) {
    if (out == NULL || !list_valid(list) || index >= list->count)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = list->items[index];
    return MC_WORLD_STORAGE_OK;
}

bool mc_world_chunk_entities_validate(const mc_world_chunk *chunk) {
    if (chunk == NULL || chunk->slices.length != CHUNK_ENTITY_SLICES ||
        chunk->allocation_bytes > chunk->max_allocation_bytes)
        return false;

    size_t bytes;
    if (chunk->storage_kind == CHUNK_STORAGE_GENERATED) {
        _Static_assert((size_t)MC_CHUNK_BLOCKS <= SIZE_MAX - sizeof(generated_chunk_backing),
                       "generated Chunk backing and blocks must fit size_t");
        bytes = sizeof(generated_chunk_backing) + (size_t)MC_CHUNK_BLOCKS;
    } else if (chunk->storage_kind == CHUNK_STORAGE_LOADED) {
        bytes = sizeof(mc_world_chunk);
    } else {
        return false;
    }
    /* One direct ownership ledger: actual base storage, validated embedded
     * Map backing, and each actual Entity capacity. Never derive one owner
     * by subtracting another from allocation_bytes. The Map inspector must
     * not call this validator and must not acquire a source l dereference. */
    size_t map_bytes;
    if (!tile_entity_map_owned_bytes(chunk, &map_bytes) || map_bytes > SIZE_MAX - bytes)
        return false;
    bytes += map_bytes;
    for (size_t index = 0; index < CHUNK_ENTITY_SLICES; ++index) {
        const mc_world_chunk_entity_list *const list = &chunk->entities[index];
        if (chunk->slices.items[index] != list || !list_valid(list) ||
            (list->count != 0 && chunk->release_entity == NULL))
            return false;
        if (chunk->entity_containers_retired &&
            (list->items != NULL || list->count != 0 || list->capacity != 0))
            return false;
        if (list->items != NULL)
            for (size_t previous = 0; previous < index; ++previous)
                if (list->items == chunk->entities[previous].items)
                    return false;
        const size_t array_bytes = list->capacity * sizeof(*list->items);
        if (array_bytes > SIZE_MAX - bytes)
            return false;
        bytes += array_bytes;
        for (size_t occurrence = 0; occurrence < list->count; ++occurrence)
            if (list->items[occurrence] == NULL)
                return false;
    }
    return bytes == chunk->allocation_bytes;
}

void mc_world_chunk_entities_release(mc_world_chunk *chunk) {
    if (chunk == NULL)
        return;
    chunk->entity_containers_retired = true;
    /* The complete application/container admission happened BEFORE the first
     * release. All pointers below are genuine held occurrences. No validation,
     * source callbacks, failure, future-container check or q/bF store occurs. */
    for (size_t index = 0; index < CHUNK_ENTITY_SLICES; ++index) {
        mc_world_chunk_entity_list *const list = &chunk->entities[index];
        mc_entity **const items = list->items;
        const size_t count = list->count;
        const size_t bytes = list->capacity * sizeof(*items);
        list->items = NULL;
        list->count = 0;
        list->capacity = 0;
        for (size_t occurrence = 0; occurrence < count; ++occurrence)
            chunk->release_entity(items[occurrence]);
        free(items);
        chunk->allocation_bytes -= bytes;
    }
}

void mc_world_chunk_containers_release(mc_world_chunk *chunk) {
    if (chunk == NULL)
        return;
    mc_world_chunk_entities_release(chunk);
    /* Finalize the actual embedded owner even if source l was replaced.
     * This is native held-resource cleanup, never a source Map callback. */
    if (chunk->tile_entity_map.release_backing != NULL)
        chunk->tile_entity_map.release_backing(&chunk->tile_entity_map);
}

bool mc_world_chunk_entity_containers_retired(const mc_world_chunk *chunk) {
    return chunk != NULL && chunk->entity_containers_retired;
}

bool mc_world_chunk_source_enabled(const mc_world_chunk *chunk) {
    return chunk != NULL && !chunk->entity_containers_retired && !chunk->source_disabled;
}

void mc_world_chunk_source_disable(mc_world_chunk *chunk) {
    chunk->source_disabled = true;
}
