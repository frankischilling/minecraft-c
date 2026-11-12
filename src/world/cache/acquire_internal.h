#ifndef MC_WORLD_CACHE_ACQUIRE_INTERNAL_H
#define MC_WORLD_CACHE_ACQUIRE_INTERNAL_H
#include "mc/world/cache/acquire.h"
/* Private owned-provider adapter only. It proves its own map representation;
 * all other acquisition checks remain identical to the public validator. */
bool mc_chunk_acquire_validate_without_cache(const mc_chunk_acquire_context *);
#endif
