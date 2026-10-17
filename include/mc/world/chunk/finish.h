#ifndef MC_WORLD_CHUNK_FINISH_H
#define MC_WORLD_CHUNK_FINISH_H

#include "mc/world/lighting/world.h"

#define MC_CHUNK_FINISH_QUERIES 1024u

typedef struct mc_chunk_height_query {
    int32_t x, z, local_height, world_height;
} mc_chunk_height_query;

typedef struct mc_chunk_finish_trace {
    mc_chunk_height_query *entries;
    size_t capacity, count;
} mc_chunk_finish_trace;

/* Full normal finishing, before cache publication: local height/direct sky,
 * then X/Z column order and west/east/north/south world queries and queued sky
 * intervals, then dirty=true. Cache entries resolving this same identity see
 * its newly rebuilt heights; an unpublished identity remains invisible.
 * Java coordinate wrapping and original border/key behavior are preserved.
 * Blocks, metadata, block light, cache, depth and unrelated chunk state survive.
 *
 * Preflight rejects blank/invalid chunks, invalid opacity/world/trace storage
 * or insufficient queue reserve without any mutation. A bounded queue needs
 * 1024 free slots; the original full million-entry storage can instead clear
 * through its normal abort policy. No FULL request is silently discarded.
 * Optional trace must have 1024 slots, distinct from all runtime storage;
 * success replaces its count with 1024 and preserves the suffix. NULL disables
 * tracing. queue_aborts is optional and must be disjoint from runtime/trace
 * storage; success stores the number of original abort diagnostics to report.
 * No allocation, loading, publication, population or propagation.
 */
bool mc_chunk_finish_normal(mc_chunk_state *chunk, mc_world_lighting *world, const int32_t *opacity,
                            size_t opacity_length, mc_chunk_finish_trace *trace,
                            size_t *queue_aborts);

#endif
