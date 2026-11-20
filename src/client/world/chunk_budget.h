#ifndef MC_CLIENT_WORLD_CHUNK_BUDGET_H
#define MC_CLIENT_WORLD_CHUNK_BUDGET_H

/* Far can touch a 28-by-28 footprint including ChunkCache's halo, and
 * the client provider keeps earlier Chunks resident during movement.
 * Use the same ceiling for Store ownership and provider publication. */
enum { MC_CLIENT_WORLD_CHUNK_CAPACITY = 4096 };

#endif
