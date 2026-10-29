#ifndef MC_CLIENT_ENTITY_RENDERER_FOG_STATE_INTERNAL_H
#define MC_CLIENT_ENTITY_RENDERER_FOG_STATE_INTERNAL_H

#include "mc/client/world/renderer_fog_state.h"

/* Actual a(IF), entered only while parent owns the canonical PX active guard
 * and retained occurrence. Child borrows both until retirement; never clears
 * parent active. Out starts NULL. Partial is preserved, never preflighted or
 * used in floating arithmetic because the source method does not load it. */
mc_client_renderer_fog_state_status
entity_renderer_fog_state_entered(mc_client_source_px *, int32_t pass, float partial,
                                  mc_client_renderer_fog_state_call **,
                                  mc_client_renderer_fog_state_error *admission_error);

#endif
