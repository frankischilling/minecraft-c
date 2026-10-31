#ifndef MC_ENTITY_PARTICLES_ORIGIN_H
#define MC_ENTITY_PARTICLES_ORIGIN_H

#include "mc/entity/classes/classes.h"

/* The actual XW class's three camera-origin fields, shared by particles in
 * this same Entity namespace. They are not per-EffectRenderer/instance state. */
typedef struct mc_particle_origin {
    double l, m, n;
} mc_particle_origin;

/* Ordered single putstatic for actual CLIENT XW: axis0/1/2 selects l/m/n.
 * Native argument admission precedes the actual SN superclass initialization,
 * which occurs immediately before this one store. Its original error is
 * preserved; no instance constructor, Registry, World or FP operation occurs.
 * The caller supplies its own reached source site and operand evaluation order.
 * Failure changes no origin cell. Successful stores preserve raw double bits.
 * Keep classes live/exclusive; optional error is fresh and disjoint from it. */
mc_entity_status mc_entity_particle_origin_store(mc_entity_classes *, unsigned int axis,
                                                 double value, mc_entity_error *);

/* Passive actual cells, including their default +0 before source active use.
 * No initialization/readiness/hold/FP work is performed. Out is required and
 * changes only on OK; error is optional/fresh. Classes, out and error must be
 * live, exclusive and mutually disjoint. A read does not establish active use. */
mc_entity_status mc_entity_particle_origin_read(const mc_entity_classes *, mc_particle_origin *out,
                                                mc_entity_error *);

#endif
