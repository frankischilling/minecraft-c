#ifndef MC_CLIENT_FIRST_PERSON_HAND_INTERNAL_H
#define MC_CLIENT_FIRST_PERSON_HAND_INTERNAL_H
#include "mc/client/world/first_person_hand.h"

/* Actual px.b(FI)V, entered synchronously under its real parent PX guard.
 * Out starts NULL. Allocated terminal failures are inspected and retired,
 * never replayed. Native cleanup does not synthesize a source PopMatrix. */
mc_client_first_person_hand_status
entity_renderer_first_person_hand_entered(mc_client_source_px *, mc_math_classes *, float, int32_t,
                                          mc_client_first_person_hand_call **,
                                          mc_client_first_person_hand_error *);
#endif
