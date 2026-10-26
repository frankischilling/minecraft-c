#ifndef MC_ENTITY_PARTICLES_PARTICLE_INTERNAL_H
#define MC_ENTITY_PARTICLES_PARTICLE_INTERNAL_H

#include "../entity_internal.h"
#include "mc/entity/particles/particle.h"

struct mc_particle_state {
    mc_particle_fields fields;
    mc_pickup_particle_fields pickup;
    unsigned int particle_pc, pickup_pc;
};

typedef struct mc_pickup_particle_object {
    mc_entity entity;
    struct mc_particle_state state;
} mc_pickup_particle_object;

void mc_pickup_particle_dispose(mc_entity *entity);

#endif
