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
    struct mc_client_effect_draw_call *draw_call;
    struct mc_effect_tick_call *tick_call;
    struct mc_effect_block_call *block_call;
};

typedef struct mc_effect_renderer_terminal_view {
    mc_entity_classes *classes;
    mc_client_render_engine *render_engine;
    mc_world_runtime *const *world_slot;
    size_t holds;
    const mc_effect_particle_list *lists[4];
    size_t particle_occurrences;
} mc_effect_renderer_terminal_view;

/* Passive native inspection of this exact READY owner and its four distinct
 * Lists. Nonempty Lists admit only complete canonical PB objects in layer2,
 * bound to this same World and namespace. Every actual List slot remains an
 * independent owning occurrence; no List is cleared and no hold allowance
 * is inferred. The returned slot is the actual owning World field, including
 * when NULL. Keep r live and exclusive; the view expires on its next mutation
 * or retirement. No source/class work, retain/release, cleanup or diagnostic-
 * owner mutation occurs. out is required and error is optional; both must be
 * live and disjoint from each other and r. Failure preserves *out. Entered
 * owners are BUSY; partial construction and other non-READY phases are refused. */
mc_effect_renderer_status
mc_client_effect_renderer_terminal_inspect(const mc_client_effect_renderer *r,
                                           mc_effect_renderer_terminal_view *out,
                                           mc_effect_renderer_error *error);

#endif
