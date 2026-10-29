#ifndef MC_CLIENT_ENTITY_RENDERER_RAIN_TICK_INTERNAL_H
#define MC_CLIENT_ENTITY_RENDERER_RAIN_TICK_INTERNAL_H

#include "mc/client/world/renderer_rain.h"

/* Only the enclosing canonical PX tick may call this while it owns the SAME
 * PX active guard and retained occurrence. The child borrows that occurrence
 * through retirement, adds no retain and never clears the parent's guard.
 * The parent retires this child before releasing its occurrence. */
mc_client_renderer_rain_status
entity_renderer_rain_tick_entered(mc_client_source_px *, mc_client_renderer_rain_call **,
                                  mc_client_renderer_rain_error *admission_error);

#endif
