#ifndef MC_WORLD_CHUNK_OBJECT_STATE_INTERNAL_H
#define MC_WORLD_CHUNK_OBJECT_STATE_INTERNAL_H

#include "mc/entity/entity.h"
#include "object_internal.h"

enum { CHUNK_ENTITY_SLICES = 8 };

struct mc_world_chunk_nibble {
    uint8_t *a;
    size_t length;
};

struct mc_world_chunk_tile_map {
    size_t count;
};

struct mc_world_chunk_entity_list {
    mc_entity **items;
    size_t count, capacity;
};

struct mc_world_chunk_list_array {
    mc_world_chunk_entity_list *items[CHUNK_ENTITY_SLICES];
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

    /* Native ownership metadata, never additional Java fields. The current
     * ledger includes all currently allocated Entity pointer capacities.
     * max is retained verbatim from create/bind's admitted native budget. */
    chunk_storage_kind storage_kind;
    uint8_t *bound_blocks;
    uint8_t *owned_blocks;
    int32_t constructor_x, constructor_z;
    size_t allocation_bytes, max_allocation_bytes;
    bool entity_containers_retired;
    /* Installed only by concrete Entity insertion after a real hold. This
     * native finalizer keeps lower Chunk storage independent of runtime
     * constructors; it is never a source/gameplay observer callback. */
    void (*release_entity)(mc_entity *entity);
};

typedef struct generated_chunk_backing {
    mc_world_chunk chunk;
    mc_chunk_state state;
    mc_chunk_data data;
} generated_chunk_backing;

#endif
