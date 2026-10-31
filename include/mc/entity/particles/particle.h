#ifndef MC_ENTITY_PARTICLES_PARTICLE_H
#define MC_ENTITY_PARTICLES_PARTICLE_H

#include "mc/entity/entity.h"

typedef enum mc_particle_cause {
    MC_PARTICLE_NONE,
    MC_PARTICLE_OWNER,
    MC_PARTICLE_ENTITY,
    MC_PARTICLE_RANDOM,
    MC_PARTICLE_NULL_REFERENCE
} mc_particle_cause;

typedef struct mc_particle_error {
    mc_entity_status status;
    mc_particle_cause cause;
    const char *operation, *source_class, *source_method;
    unsigned int source_pc, particle_pc, pickup_pc;
    mc_entity_error entity;
    mc_java_random_profile_status random;
} mc_particle_error;

typedef struct mc_particle_fields {
    int32_t b;
    float c, d;
    int32_t e, f;
    float g, h, i, j, k;
} mc_particle_fields;

typedef struct mc_pickup_particle_fields {
    mc_entity *a, *o;
    int32_t p, q;
    float r;
} mc_pickup_particle_fields;

typedef struct mc_particle_snapshot {
    mc_entity_phase phase;
    unsigned int particle_pc, pickup_pc;
    mc_particle_fields particle;
    mc_pickup_particle_fields pickup;
} mc_particle_snapshot;

/* Genuine client em allocation/constructor, including its xw/sn ancestors.
 * Allocate RAW before evaluating constructor arguments. The nullable World
 * borrows the same Random profile. The picked Entity and nullable target are
 * held READY non-particle identities from the same class owner, matching the
 * actual dc.b pickup actor pipeline. Arbitrary nested particle-reference graphs
 * are outside this interface. Phase/identity rejection preserves RAW before
 * source execution. Retained actor depth is therefore constant; references are
 * retained only at reached stores and remain immutable through retirement.
 * Construction uses the shared Math owner and the new Entity's ordinary Random;
 * no caller-supplied samples or seed are accepted. The captured strict x86-64
 * FP environment remains stable throughout. Failures retain actual completed
 * fields/IDs/entropy and FAILED_CONSTRUCTION, without rollback or retry.
 * Owners/errors/arguments remain live, exclusive and disjoint through calls. */
mc_entity_status mc_pickup_particle_allocate(mc_entity_classes *classes, mc_entity **out,
                                             mc_entity_error *error);
mc_entity_status mc_pickup_particle_construct(mc_entity *raw, mc_world_runtime *world,
                                              mc_entity *picked, mc_entity *target, float offset,
                                              mc_particle_error *error);

/* Actual em.w_: increment p, compare q, then inherited sn.K when equal.
 * This override neither calls xw.w_ nor queries World or the retained targets.
 * Rendering, other particle kinds and effect-renderer ownership are separate. */
mc_entity_status mc_pickup_particle_tick(mc_entity *particle, mc_particle_error *error);
mc_entity_status mc_pickup_particle_layer(const mc_entity *particle, int32_t *out);
bool mc_particle_read(const mc_entity *particle, mc_particle_snapshot *out);

#endif
