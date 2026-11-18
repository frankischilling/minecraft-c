#ifndef MC_CLIENT_WORLD_CHUNK_BUDGET_H
#define MC_CLIENT_WORLD_CHUNK_BUDGET_H

/* Far has 26 columns per axis. ChunkCache construction can read one column
 * past either edge, so reserve room for 28 by 28 columns plus movement. */
enum { MC_CLIENT_WORLD_CHUNK_CAPACITY = 1024 };

#endif
