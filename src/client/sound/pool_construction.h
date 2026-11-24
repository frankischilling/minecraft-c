#ifndef MC_CLIENT_SOUND_POOL_CONSTRUCTION_H
#define MC_CLIENT_SOUND_POOL_CONSTRUCTION_H
#include "mc/client/sound/construction.h"

/* Private canonical ownership seam. Only sound_pool_create and the concrete
 * manager use these calls, once, on a fresh unexposed allocation. No source
 * pool operation or mutation is admitted on a pending failed candidate.
 * The existing pool layout/header/budget are unchanged. */
mc_sound_status mc_sound_pool_allocate_raw(size_t max_allocation_bytes, mc_sound_pool **out,
                                           mc_sound_error *);
mc_sound_status mc_sound_pool_construct_raw(mc_sound_pool *, mc_java_random_profile *,
                                            mc_sound_pool_constructor_prefix *, mc_sound_error *);
#endif
