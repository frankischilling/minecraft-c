#ifndef MC_WORLD_STORAGE_CHUNK_LOOKUP_INTERNAL_H
#define MC_WORLD_STORAGE_CHUNK_LOOKUP_INTERNAL_H

#include "mc/world/storage/store.h"

/* Passive scalar-only native ownership receipt. No Chunk, node, index slot or
 * caller-owned cache escapes. Reads/ordinary lookups change none of these cells.
 * base_owner_bytes includes the current enlarged Store and, for source owners,
 * its retained Handler prefix. Optional index live/rehash peak bytes are separate
 * from source-prefix allocation receipts and each codec/document/Chunk budget. */
typedef struct mc_world_store_chunk_lookup_view {
    bool indexed, disabled;
    size_t chunk_count, index_count, capacity, index_bytes, peak_bytes;
    size_t byte_limit, base_owner_bytes;
} mc_world_store_chunk_lookup_view;

bool mc_world_store_chunk_lookup_inspect(const mc_world_store *,
                                         mc_world_store_chunk_lookup_view *);

#endif
