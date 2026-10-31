#ifndef MC_WORLD_RUNTIME_ACQUISITION_INTERNAL_H
#define MC_WORLD_RUNTIME_ACQUISITION_INTERNAL_H

#include "mc/world/runtime/acquisition_error.h"

mc_chunk_acquire_status mc_world_runtime_get_detailed(mc_world_runtime *runtime, int32_t x,
                                                      int32_t z, mc_chunk_state **out,
                                                      mc_world_runtime_acquisition_error *error);

#endif
