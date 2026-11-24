#ifndef MC_ENTITY_PRIMED_TNT_INTERNAL_H
#define MC_ENTITY_PRIMED_TNT_INTERNAL_H

#include "../entity_internal.h"
#include "mc/entity/primed_tnt/construction.h"

typedef struct mc_primed_tnt_entity_state {
    int32_t fuse;
    unsigned int constructor_pc, world_constructor_pc;
    bool full_constructor;
} mc_primed_tnt_entity_state;

/* Only the canonical PRIMED_TNT allocator creates this complete allocation.
 * Entity is its first member; no extra pointer enlarges unrelated Entities. */
typedef struct mc_primed_tnt_entity_object {
    mc_entity entity;
    mc_primed_tnt_entity_state state;
} mc_primed_tnt_entity_object;

static inline mc_primed_tnt_entity_state *mc_primed_tnt_state(mc_entity *entity) {
    return &((mc_primed_tnt_entity_object *)entity)->state;
}

#endif
