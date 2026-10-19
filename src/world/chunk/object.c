#include "object_internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum { CHUNK_ENTITY_SLICES = 8 };

struct mc_world_chunk_nibble {
    uint8_t *a;
    size_t length;
};

struct mc_world_chunk_tile_map {
    size_t count;
};

struct mc_world_chunk_entity_list {
    size_t count;
};

struct mc_world_chunk_list_array {
    const mc_world_chunk_entity_list *items[CHUNK_ENTITY_SLICES];
    size_t length;
};

typedef enum chunk_storage_kind {
    CHUNK_STORAGE_GENERATED,
    CHUNK_STORAGE_LOADED
} chunk_storage_kind;

struct mc_world_chunk {
    mc_chunk_state *state;
    mc_chunk_data *data;

    mc_world_runtime *d;
    mc_world_chunk_nibble *e, *f, *g;
    uint8_t *h;
    mc_world_chunk_tile_map *l;
    mc_world_chunk_list_array *m;

    mc_world_chunk_nibble metadata, skylight, block_light;
    mc_world_chunk_tile_map tile_map;
    mc_world_chunk_list_array slices;
    mc_world_chunk_entity_list entities[CHUNK_ENTITY_SLICES];

    /* Native ownership/invariant metadata, not additional Java fields. */
    chunk_storage_kind storage_kind;
    uint8_t *bound_blocks;
    uint8_t *owned_blocks;
    int32_t constructor_x, constructor_z;
    size_t allocation_bytes;
};

/* The first member is the same opaque header used by loaded bindings. Normal
 * C struct layout supplies the state/data alignment without tail arithmetic. */
typedef struct generated_chunk_backing {
    mc_world_chunk chunk;
    mc_chunk_state state;
    mc_chunk_data data;
} generated_chunk_backing;

_Static_assert(offsetof(generated_chunk_backing, chunk) == 0,
               "generated Chunk header is the allocation's first member");
_Static_assert(offsetof(generated_chunk_backing, state) % _Alignof(mc_chunk_state) == 0,
               "generated Chunk state alignment");
_Static_assert(offsetof(generated_chunk_backing, data) % _Alignof(mc_chunk_data) == 0,
               "generated Chunk data alignment");

static mc_world_storage_status chunk_result(mc_world_storage_error *error,
                                            mc_world_storage_status status, const char *operation)
{
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static void nibble_construct(mc_world_chunk_nibble *nibble, uint8_t *storage, size_t packed_length)
{
    /* wi/ob.<init>(I)V: raw a=NULL, zero byte[input length >> 1] at8,
     * then final a store10. The fixed caller supplies that packed span. */
    nibble->a = NULL;
    nibble->length = 0;
    memset(storage, 0, packed_length);
    nibble->a = storage;
    nibble->length = packed_length;
}

mc_world_storage_status mc_world_chunk_storage_create(mc_world_runtime *world, int32_t x, int32_t z,
                                                      uint8_t **blocks, size_t length,
                                                      size_t max_allocation_bytes,
                                                      mc_world_chunk **out,
                                                      mc_world_storage_error *error)
{
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

    /* lm/hi.<init>(World,II)V: fresh empty Map, l12. */
    chunk->tile_map.count = 0;
    chunk->l = &chunk->tile_map;

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
        chunk->entities[index].count = 0;
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
                                                           mc_world_storage_error *error)
{
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

    /* The stored owner has already decoded its actual cells and normalized
     * blocks. Bind only our own reference/child headers; never clear, copy,
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
                              .allocation_bytes = sizeof(*chunk)};
    chunk->metadata = (mc_world_chunk_nibble){data->metadata, MC_CHUNK_NIBBLE_BYTES};
    chunk->skylight = (mc_world_chunk_nibble){data->skylight, MC_CHUNK_NIBBLE_BYTES};
    chunk->block_light = (mc_world_chunk_nibble){data->block_light, MC_CHUNK_NIBBLE_BYTES};
    chunk->e = &chunk->metadata;
    chunk->f = &chunk->skylight;
    chunk->g = &chunk->block_light;
    chunk->l = &chunk->tile_map;
    chunk->slices.length = CHUNK_ENTITY_SLICES;
    for (size_t index = 0; index < CHUNK_ENTITY_SLICES; ++index)
        chunk->slices.items[index] = &chunk->entities[index];
    chunk->m = &chunk->slices;

    *out = chunk;
    return chunk_result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_chunk_state *mc_world_chunk_state(mc_world_chunk *chunk)
{
    return chunk == NULL ? NULL : chunk->state;
}

size_t mc_world_chunk_allocation_bytes(const mc_world_chunk *chunk)
{
    return chunk == NULL ? 0 : chunk->allocation_bytes;
}

mc_world_storage_status mc_world_chunk_fields_get(const mc_world_chunk *chunk,
                                                  mc_world_chunk_fields *out)
{
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
                                                         mc_world_chunk_nibble_fields *out)
{
    if (nibble == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = (mc_world_chunk_nibble_fields){nibble->a, nibble->length};
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_chunk_slice_at(const mc_world_chunk_list_array *array,
                                                size_t index,
                                                const mc_world_chunk_entity_list **out)
{
    if (array == NULL || out == NULL || index >= array->length)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = array->items[index];
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_chunk_tile_count(const mc_world_chunk_tile_map *map, size_t *out)
{
    if (map == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = map->count;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_chunk_entity_count(const mc_world_chunk_entity_list *list,
                                                    size_t *out)
{
    if (list == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = list->count;
    return MC_WORLD_STORAGE_OK;
}

bool mc_world_chunk_storage_empty(const mc_world_chunk *chunk)
{
    if (chunk == NULL || chunk->d == NULL || chunk->state == NULL || chunk->data == NULL ||
        chunk->state->blank || chunk->state->data != chunk->data || chunk->bound_blocks == NULL ||
        chunk->state->x != chunk->constructor_x || chunk->state->z != chunk->constructor_z ||
        chunk->data->blocks != chunk->bound_blocks ||
        chunk->data->blocks_capacity != (size_t)MC_CHUNK_BLOCKS || chunk->e != &chunk->metadata ||
        chunk->f != &chunk->skylight || chunk->g != &chunk->block_light ||
        chunk->h != chunk->data->heightmap || chunk->l != &chunk->tile_map ||
        chunk->m != &chunk->slices || chunk->metadata.a != chunk->data->metadata ||
        chunk->metadata.length != (size_t)MC_CHUNK_NIBBLE_BYTES ||
        chunk->skylight.a != chunk->data->skylight ||
        chunk->skylight.length != (size_t)MC_CHUNK_NIBBLE_BYTES ||
        chunk->block_light.a != chunk->data->block_light ||
        chunk->block_light.length != (size_t)MC_CHUNK_NIBBLE_BYTES || chunk->tile_map.count != 0 ||
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

void mc_world_chunk_storage_free(mc_world_chunk *chunk)
{
    if (chunk != NULL) {
        /* Generated header is the first member of its malloc backing. Loaded
         * headers have no owned blocks/tail; their stored payload outlives this
         * call and is freed separately by the store. */
        free(chunk->owned_blocks);
        free(chunk);
    }
}
