#ifndef MC_WORLD_RUNTIME_WEATHER_H
#define MC_WORLD_RUNTIME_WEATHER_H

#include "mc/world/dimension/world_provider.h"
#include "mc/world/info/info.h"
#include "mc/world/runtime/construction.h"

typedef struct mc_world_weather_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode;
    const char *info_method;
    unsigned int info_bytecode;
    mc_world_info_status info;
    mc_world_provider_status provider;
    mc_world_storage_error cause;
} mc_world_weather_error;

void mc_world_weather_error_init(mc_world_weather_error *);

/* Actual client fd.m()V, the first fd.l descendant at1. Requires a live,
 * exclusive CLIENT GAME owner that has not begun native retirement. RAW or
 * failed source prefixes are admitted until their reached nullable operations.
 * World, current Info/provider/Random dependencies and disjoint optional error
 * storage stay live/exclusive through the call. Source fields are read/reloaded
 * in order; no snapshot replaces the current owners or their scalar stores.
 *
 * xa.e returns before m/Info/Random/FP. Positive m decrements before timers;
 * thunder precedes rain, then i=j precedes the first floating operation.
 * Interpolation uses the source binary64 .01 step and binary32 stores, with
 * ordered clamps that leave NaN unchanged. Native unsupported FP preserves
 * all earlier timer/draw/field effects. No source rollback, class initialization,
 * callback, new Random, pool reset or aggregate fd.l success is supplied.
 *
 * Nonclient owners fail native admission before source effects; this is not a
 * fabricated Java exception. Caller error is fresh and published only at
 * return. Admission preserves earlier World diagnostics; entered attempts
 * publish their fresh reached status to World.last_error. Server weather and
 * the remaining World time/spawn/scheduler/sky bodies remain separate work. */
mc_world_storage_status mc_world_runtime_tick_weather(mc_world_runtime *, mc_world_weather_error *);

#endif
