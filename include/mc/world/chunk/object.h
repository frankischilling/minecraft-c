#ifndef MC_WORLD_CHUNK_OBJECT_H
#define MC_WORLD_CHUNK_OBJECT_H

#include "mc/world/chunk/state.h"
#include "mc/world/storage/status.h"

typedef struct mc_world_runtime mc_world_runtime;
typedef struct mc_world_chunk mc_world_chunk;
typedef struct mc_world_chunk_nibble mc_world_chunk_nibble;
typedef struct mc_world_chunk_tile_entity_map mc_world_chunk_tile_entity_map;
typedef struct mc_world_chunk_list_array mc_world_chunk_list_array;
typedef struct mc_world_chunk_entity_list mc_world_chunk_entity_list;
typedef struct mc_entity mc_entity;

/* Passive snapshots of the seventeen actual instance fields. Scalars read the
 * same cells used by cache/lifecycle/save, never a detached flag projection.
 * e/f/g and each of m's eight lists are distinct stable child identities.
 * This finite owner represents generated and bound loaded Chunk objects and actual
 * known-empty children, without a general Java collection implementation. */
typedef struct mc_world_chunk_fields {
    uint8_t *b;
    bool c;
    mc_world_runtime *d;
    const mc_world_chunk_nibble *e, *f, *g;
    uint8_t *h;
    int32_t i, j, k;
    const mc_world_chunk_tile_entity_map *l;
    const mc_world_chunk_list_array *m;
    bool n, o, p, q;
    int64_t r;
} mc_world_chunk_fields;

typedef struct mc_world_chunk_nibble_fields {
    uint8_t *a;
    size_t length;
} mc_world_chunk_nibble_fields;

/* All returned handles, pointers and views borrow the store-owned Chunk.
 * Retire external views before checked store retirement/free. Mutable state
 * permits ordinary flags/array contents, but never rebinding data/blocks or
 * changing concrete Chunk final j/k coordinates. Generic unbound stored states
 * keep their separate editing contract. Grant exclusive access;
 * output slots must be live and disjoint from owners/dependencies/each other.
 * Failure preserves out; NULL state/byte queries return NULL/0. */
mc_chunk_state *mc_world_chunk_state(mc_world_chunk *chunk);
size_t mc_world_chunk_allocation_bytes(const mc_world_chunk *chunk);
mc_world_storage_status mc_world_chunk_fields_get(const mc_world_chunk *chunk,
                                                  mc_world_chunk_fields *out);
mc_world_storage_status mc_world_chunk_nibble_fields_get(const mc_world_chunk_nibble *nibble,
                                                         mc_world_chunk_nibble_fields *out);
mc_world_storage_status mc_world_chunk_slice_at(const mc_world_chunk_list_array *array,
                                                size_t index,
                                                const mc_world_chunk_entity_list **out);
mc_world_storage_status mc_world_chunk_tile_entity_count(const mc_world_chunk_tile_entity_map *map,
                                                         size_t *out);
mc_world_storage_status mc_world_chunk_entity_count(const mc_world_chunk_entity_list *list,
                                                    size_t *out);
/* Borrow one actual occurrence in the stable slice List; duplicates remain
 * separate entries. Keep the Chunk alive and unchanged while using the result.
 * Failure preserves out; this neither retains nor dispatches Entity behavior. */
mc_world_storage_status mc_world_chunk_entity_at(const mc_world_chunk_entity_list *list,
                                                 size_t index, mc_entity **out);

#endif
