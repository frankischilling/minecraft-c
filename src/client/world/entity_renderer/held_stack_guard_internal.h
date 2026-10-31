#ifndef MC_CLIENT_HELD_STACK_GUARD_INTERNAL_H
#define MC_CLIENT_HELD_STACK_GUARD_INTERNAL_H
#include "mc/client/world/entity_renderer.h"

/* Native-only distinct RA.a(LS,IZ) guard. Parent/token identify the actual
 * retained TH call and its charged child workspace, never Source operands. */
mc_client_entity_renderer_status
entity_renderer_held_stack_guard_begin(mc_client_held_item *, const void *parent, const void *token,
                                       mc_client_entity_renderer_error *);
mc_client_entity_renderer_status
entity_renderer_held_stack_guard_discharge(mc_client_held_item *, const void *parent,
                                           const void *token, mc_client_entity_renderer_error *);
#endif
