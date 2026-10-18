#ifndef MC_WORLD_CHUNK_REFERENCES_H
#define MC_WORLD_CHUNK_REFERENCES_H

#include "mc/world/chunk/state.h"

/* Optional owned-chunk lease accounting for retained coordinator locals. Both
 * functions are required when this descriptor is present. retain succeeds only
 * while the identity remains live; failure preserves all accounting. A success
 * receives exactly one release, including callback failure paths. These hooks
 * only change lease counters: no callbacks, graph mutation, retirement or I/O.
 * They do not detect arbitrary raw C pointers. The caller must separately keep
 * every unregistered borrowed identity/view live until its use ends. */
typedef struct mc_chunk_references {
    void *owner;
    bool (*retain)(void *owner, mc_chunk_state *chunk);
    void (*release)(void *owner, mc_chunk_state *chunk);
} mc_chunk_references;

#endif
