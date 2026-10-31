#ifndef MC_WORLD_RUNTIME_CHUNKS_INTERNAL_H
#define MC_WORLD_RUNTIME_CHUNKS_INTERNAL_H

#include "mc/world/runtime/chunks.h"

/* Actual c(II)/f(III) helper closure, shared by full finishing and incremental
 * setters. Enclosing caller holds actual World and exact Chunk lease. Local
 * x/z are the real source arguments. Preserve the caller's outer bytecode;
 * only column/interval and nested causes are changed here. */
bool mc_world_runtime_chunk_column(mc_world_chunk *chunk, int32_t x, int32_t z,
                                   mc_world_chunk_error *error);

#endif
