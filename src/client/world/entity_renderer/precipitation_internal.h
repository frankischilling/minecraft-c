#ifndef MC_CLIENT_PRECIPITATION_INTERNAL_H
#define MC_CLIENT_PRECIPITATION_INTERNAL_H
#include "mc/client/world/precipitation.h"

/* Same PX active guard/occurrence belongs to the enclosing World frame.
 * The parent retires this child before releasing that occurrence. */
mc_client_precipitation_status
entity_renderer_precipitation_entered(mc_client_source_px *, mc_math_classes *, float,
                                      mc_client_precipitation_call **,
                                      mc_client_precipitation_error *);
#endif
