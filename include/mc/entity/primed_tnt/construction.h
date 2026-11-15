#ifndef MC_ENTITY_PRIMED_TNT_CONSTRUCTION_H
#define MC_ENTITY_PRIMED_TNT_CONSTRUCTION_H

#include "mc/core/math/math.h"
#include "mc/entity/entity.h"

typedef struct mc_primed_tnt_entity_snapshot {
    int32_t fuse;
    unsigned int constructor_bytecode;
    unsigned int world_constructor_bytecode;
    bool full_constructor;
} mc_primed_tnt_entity_snapshot;

/* Actual client QW allocation and constructors. Allocation performs the real
 * Entity active use before native live/byte limits. The world-only constructor
 * calls SN once, sets fuse0, prevent-spawn collision, size0.98f and height/2
 * offset; it does not reposition the original bounds or consume Math entropy.
 *
 * The full constructor then positions the same bounds, draws actual shared
 * Math.random, and reaches the supplied defining IN table separately for sine
 * and cosine. A missing table refuses only at the reached use, after earlier
 * position/Random effects. The caller keeps this actual table alive; no hidden
 * class initialization or substitute numerical table occurs here. Native cold
 * IN initialization, ticks, explosion and subtype NBT are separate operations.
 *
 * RAW admission and failed-prefix ownership follow the ordinary Entity
 * contract. A nullable World is a real constructor argument. Error, output,
 * Entity and table storage must be disjoint and exclusively live. */
mc_entity_status mc_primed_tnt_entity_allocate(mc_entity_classes *, mc_entity **,
                                               mc_entity_error *);
mc_entity_status mc_primed_tnt_entity_construct_world(mc_entity *, mc_world_runtime *,
                                                      mc_entity_error *);
mc_entity_status mc_primed_tnt_entity_construct(mc_entity *, mc_world_runtime *, double, double,
                                                double, const mc_math_trig *, mc_entity_error *);

/* Passive native view of the actual QW subobject, including RAW/failed
 * construction. constructor_bytecode is the most recently reached operation
 * in the selected outer constructor. world_constructor_bytecode preserves
 * its actual QW(World) child independently of the SN base child error. */
mc_entity_status mc_primed_tnt_entity_get_snapshot(const mc_entity *,
                                                   mc_primed_tnt_entity_snapshot *);

#endif
