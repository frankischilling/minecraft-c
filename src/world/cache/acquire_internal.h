#ifndef MC_WORLD_CACHE_ACQUIRE_INTERNAL_H
#define MC_WORLD_CACHE_ACQUIRE_INTERNAL_H
#include "mc/world/cache/acquire.h"
/* Private owned-provider adapter only. It proves its own map representation;
 * all other acquisition checks remain identical to the public validator. */
bool mc_chunk_acquire_validate_without_cache(const mc_chunk_acquire_context *);

/* Run the complete GET validation and lookup without callbacks. True returns
 * the final status; false leaves outputs untouched and needs mc_chunk_acquire. */
bool mc_chunk_acquire_get_without_loading(const mc_chunk_acquire_context *, int32_t, int32_t,
                                          mc_chunk_state **, mc_chunk_acquire_status *);
#endif
