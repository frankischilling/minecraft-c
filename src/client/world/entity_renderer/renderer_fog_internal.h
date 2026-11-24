#ifndef MC_CLIENT_ENTITY_RENDERER_FOG_INTERNAL_H
#define MC_CLIENT_ENTITY_RENDERER_FOG_INTERNAL_H

#include "mc/client/world/renderer_fog.h"

/* Enter only while the enclosing canonical PX frame owns its active guard
 * and occurrence. This child borrows both through retirement and never
 * clears the parent's guard. Out starts NULL; failure retains every reached
 * source/GL effect and typed cause. Parent retires child before releasing PX. */
mc_client_renderer_fog_status
entity_renderer_fog_entered(mc_client_source_px *, mc_math_classes *, float partial,
                            mc_client_renderer_fog_call **,
                            mc_client_renderer_fog_error *admission_error);

#endif
