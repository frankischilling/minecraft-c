#ifndef MC_WORLD_INFO_TICK_SOURCE_INTERNAL_H
#define MC_WORLD_INFO_TICK_SOURCE_INTERNAL_H
#include "mc/world/info/info.h"
/* Exact phase-independent client ei.c/d/e and a(J) scalar bodies. */
mc_world_info_status mc_world_info_source_spawn_get(const mc_world_info *, unsigned int axis,
                                                    int32_t *);
mc_world_info_status mc_world_info_source_time_set(mc_world_info *, int64_t);
#endif
