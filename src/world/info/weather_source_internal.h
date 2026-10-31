#ifndef MC_WORLD_INFO_WEATHER_SOURCE_INTERNAL_H
#define MC_WORLD_INFO_WEATHER_SOURCE_INTERNAL_H
#include "mc/world/info/info.h"

/* Exact passive per-side scalar bodies, without a constructor-phase gate.
 * Caller holds the actual Info/class namespace through the operation. NULL
 * receiver/output rejects without stores; no snapshots, I/O, World dependency,
 * allocation, Random or constructor-PC change occurs. Public READY operations
 * keep their existing admission and delegate to these same field kernels. */
mc_world_info_status mc_world_info_source_thunder_time_get(const mc_world_info *, int32_t *);
mc_world_info_status mc_world_info_source_thunder_time_set(mc_world_info *, int32_t);
mc_world_info_status mc_world_info_source_thundering_get(const mc_world_info *, bool *);
mc_world_info_status mc_world_info_source_thundering_set(mc_world_info *, bool);
mc_world_info_status mc_world_info_source_rain_time_get(const mc_world_info *, int32_t *);
mc_world_info_status mc_world_info_source_rain_time_set(mc_world_info *, int32_t);
mc_world_info_status mc_world_info_source_raining_get(const mc_world_info *, bool *);
mc_world_info_status mc_world_info_source_raining_set(mc_world_info *, bool);
#endif
