#ifndef MC_ENTITY_ARROW_INTERNAL_H
#define MC_ENTITY_ARROW_INTERNAL_H

#include "../entity_internal.h"
#include "mc/entity/arrow/construction.h"

typedef struct mc_arrow_entity_state {
    mc_arrow_fields fields;
    /* Independent counted occurrence backing actual source SL.c. */
    mc_entity *shooter_hold;
    unsigned int constructor_pc, heading_pc;
    bool constructor_active, heading_active;
    mc_arrow_error failure;
} mc_arrow_entity_state;

typedef struct mc_arrow_entity_object {
    mc_entity entity;
    mc_arrow_entity_state state;
} mc_arrow_entity_object;

bool mc_entity_has_arrow(const mc_entity *);

#endif
