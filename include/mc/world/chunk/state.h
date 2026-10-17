#ifndef MC_WORLD_CHUNK_STATE_H
#define MC_WORLD_CHUNK_STATE_H

#include "mc/world/chunk/data.h"

/* Stable caller-owned identity shared by cache entries and generation. A normal
 * chunk borrows data; cached height queries need only its embedded heightmap.
 * Distinct normal identities have distinct data; caches can repeat one state
 * pointer. Full finishing additionally needs valid mutable block storage. A blank chunk
 * always reports height zero and may have NULL data. Coordinates do not alter
 * packed-key lookup: aliases can resolve a chunk with different coordinates.
 */
typedef struct mc_chunk_state {
    mc_chunk_data *data;
    int32_t x, z;
    bool blank;
    bool dirty;
} mc_chunk_state;

#endif
