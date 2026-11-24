#ifndef MC_WORLD_RUNTIME_WORLD_TICK_SCHEDULED_INTERNAL_H
#define MC_WORLD_RUNTIME_WORLD_TICK_SCHEDULED_INTERNAL_H

#include "mc/world/runtime/world_tick.h"

/* The caller owns a fresh zero-initialized frame, including across reentry.
 * Keep it private until the drain returns. */
mc_world_storage_status mc_world_runtime_drain_scheduled_into(mc_world_runtime *, bool force,
                                                              bool *pending,
                                                              mc_world_scheduled_drain_error *);

#endif
