#ifndef MC_WORLD_RUNTIME_CHUNK_RANDOM_H
#define MC_WORLD_RUNTIME_CHUNK_RANDOM_H

#include "mc/world/runtime/acquisition_error.h"

typedef struct mc_world_chunk_random_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_world_chunk_random_site;

typedef struct mc_world_chunk_random_state {
    bool returned, blank, seed_read, constructed;
    unsigned int source_pc;
    int32_t x, z;
    int64_t world_seed, seed;
    mc_world_runtime *source_world; /* Borrowed actual Chunk.d. */
} mc_world_chunk_random_state;

typedef struct mc_world_chunk_random_error {
    mc_world_storage_status status;
    const char *operation, *exception_class;
    mc_world_chunk_random_site site, child;
    bool native_policy, source_exception;
    mc_world_storage_error cause;
} mc_world_chunk_random_error;

/* Actual client lm/li.a(J)Random, including Blank's concrete override.
 * world supplies the exact acquired Chunk's native provenance. Only its real
 * embedded fallback or a same-store concrete Chunk is admitted. Actual d/j/k
 * select the seed, including aliases; requested coordinates are not inputs.
 * Hold the supplying graph exclusive. Source World/Info reads admit allocated
 * prefixes; no metadata fallback or READY gate is added. Fresh seeded output
 * consumes no default-constructor entropy. Failure preserves out and records
 * the reached prefix in state/error. Native holds/leases end synchronously. */
mc_world_storage_status mc_world_runtime_chunk_random(mc_world_runtime *, mc_chunk_state *,
                                                      int64_t salt, mc_java_random *out,
                                                      mc_world_chunk_random_state *,
                                                      mc_world_chunk_random_error *);

#endif
