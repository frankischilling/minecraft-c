#ifndef MC_ENTITY_LIGHTNING_INTERNAL_H
#define MC_ENTITY_LIGHTNING_INTERNAL_H

#include "../entity_internal.h"
#include "mc/entity/lightning/construction.h"

typedef struct mc_lightning_entity_state {
    int64_t a;
    int32_t b, c;
    unsigned int ancestor_pc, constructor_pc;
    size_t neighbor_attempts, own_random_calls;
    int32_t sampled_x, sampled_y, sampled_z;
    bool area_reached, area_present;
    mc_lightning_error failure;
    /* One independent capture keeps the exact borrowed mutation receipt valid
     * through this Entity's lifetime. Error/snapshot copies acquire no hold. */
    mc_world_access_failure callback_failure;
} mc_lightning_entity_state;

typedef struct mc_lightning_entity_object {
    mc_entity entity;
    mc_lightning_entity_state state;
} mc_lightning_entity_object;

/* Constructor-owned failure seam; called once after the source body failed.
 * The native invariant test uses the same capture/final-release ownership. */
mc_entity_status mc_lightning_capture_failure(mc_entity *);
void mc_lightning_dispose(mc_entity *);

#endif
