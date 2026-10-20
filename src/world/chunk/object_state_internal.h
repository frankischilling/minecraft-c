#ifndef MC_WORLD_CHUNK_OBJECT_STATE_INTERNAL_H
#define MC_WORLD_CHUNK_OBJECT_STATE_INTERNAL_H

#include "mc/entity/entity.h"
#include "object_internal.h"

enum { CHUNK_ENTITY_SLICES = 8 };

struct mc_world_chunk_nibble {
    uint8_t *a;
    size_t length;
};

struct mc_tile_map_backing;

struct mc_world_chunk_tile_map {
    size_t count;
    struct mc_tile_map_backing *backing;
    /* Native ownership hooks, never source Map operations. The finalizer
     * releases this descriptor's actual backing without inspecting source l.
     * The inspector validates and measures only the owner's embedded backing,
     * including its terminal state, independently of source l. It must not
     * allocate, invoke gameplay/callbacks, or recurse into Chunk validation;
     * it writes out only on success. Both hooks remain installed at retirement. */
    void (*release_backing)(mc_world_chunk_tile_map *descriptor);
    bool (*backing_bytes)(const mc_world_chunk *owner, size_t *out);
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
     * ledger includes all currently allocated Entity pointer capacities and
     * the embedded Tile Map's actual owned backing.
     * max is retained verbatim from create/bind's admitted native budget. */
    chunk_storage_kind storage_kind;
    uint8_t *bound_blocks;
    uint8_t *owned_blocks;
    int32_t constructor_x, constructor_z;
    size_t allocation_bytes, max_allocation_bytes;
    bool entity_containers_retired;
    /* Terminal native failure admission. Passive identity/owned-resource
     * inspection remains valid; no source continuation or save is permitted. */
    bool source_disabled;
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
