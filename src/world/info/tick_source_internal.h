#ifndef MC_WORLD_INFO_TICK_SOURCE_INTERNAL_H
#define MC_WORLD_INFO_TICK_SOURCE_INTERNAL_H
#include "mc/world/info/info.h"
/* Exact phase-independent client ei.c/d/e and a(J) scalar bodies. */
mc_world_info_status mc_world_info_source_spawn_get(const mc_world_info *, unsigned int axis,
                                                    int32_t *);
/* Actual ei.a/b/c(int): one current scalar store, without a READY gate or
 * reads/writes of the other two coordinates. */
mc_world_info_status mc_world_info_source_spawn_axis_set(mc_world_info *, unsigned int axis,
                                                         int32_t);
mc_world_info_status mc_world_info_source_time_set(mc_world_info *, int64_t);
#endif
