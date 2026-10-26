#ifndef MC_CLIENT_PARTICLES_EFFECT_RENDERER_INTERNAL_H
#define MC_CLIENT_PARTICLES_EFFECT_RENDERER_INTERNAL_H

#include "../../entity/classes/classes_internal.h"
#include "mc/client/particles/effect_renderer.h"

typedef struct mc_effect_particle_list {
    mc_entity **elements;
    size_t size, capacity;
} mc_effect_particle_list;

struct mc_client_effect_renderer {
    mc_entity_classes *classes;
    mc_effect_renderer_phase phase;
    unsigned int constructor_pc;
    mc_world_runtime *world;
    mc_client_render_engine *render_engine;
    mc_effect_particle_list **layers;
    mc_java_random *random, *random_candidate;
    size_t allocation_bytes, max_allocation_bytes, holds;
    bool entered;
};

#endif
