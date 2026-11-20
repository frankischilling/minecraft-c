#ifndef MC_ENTITY_MOVEMENT_MOVEMENT_INTERNAL_H
#define MC_ENTITY_MOVEMENT_MOVEMENT_INTERNAL_H

#include "mc/entity/movement/movement.h"

/* The enclosing operation supplies a fresh private movement frame. Retain
 * the actual return PC, completed work and reached children in that frame. */
mc_entity_status mc_entity_move_in_place(mc_entity *, mc_registry_classes *, mc_math_classes *,
                                         double, double, double, const mc_entity_move_limits *,
                                         mc_entity_move_error *);

#endif
