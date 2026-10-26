#ifndef MC_ENTITY_MOVEMENT_ENVIRONMENT_H
#define MC_ENTITY_MOVEMENT_ENVIRONMENT_H

#include "mc/entity/entity.h"
#include "mc/world/runtime/collision/boxes.h"
#include "mc/world/runtime/material_movement.h"

typedef enum mc_entity_environment_cause {
    MC_ENTITY_ENVIRONMENT_NONE = 0,
    MC_ENTITY_ENVIRONMENT_OWNER,
    MC_ENTITY_ENVIRONMENT_NULL_REFERENCE,
    MC_ENTITY_ENVIRONMENT_NAMESPACE,
    MC_ENTITY_ENVIRONMENT_FP,
    MC_ENTITY_ENVIRONMENT_BOUNDS,
    MC_ENTITY_ENVIRONMENT_REGISTRY,
    MC_ENTITY_ENVIRONMENT_WORLD,
    MC_ENTITY_ENVIRONMENT_UNSUPPORTED_VIRTUAL
} mc_entity_environment_cause;

typedef struct mc_entity_environment_error {
    mc_entity_status status;
    const char *operation, *source_class;
    mc_entity_environment_cause cause;
    unsigned int bytecode;
    mc_entity_error entity;
    mc_entity_bounds_error bounds;
    mc_registry_error registry;
    mc_world_collision_error collision;
    mc_world_material_movement_error material;
} mc_entity_environment_error;

mc_entity_environment_error mc_entity_environment_empty_error(void);

/* Actual sn/lq water, lava, stored-in-water, clearance and relative-velocity
 * methods on genuinely held READY canonical Item/Falling/Minecart/LocalPlayer
 * owners. Water dispatches hl/ez's actual override (unmodified permanent
 * bounds) or inherited sn/lq (expand then contract, two pooled objects).
 * Lava expands once; clearance shifts once, queries the actual mutable
 * collision List, and tests liquid only after an empty List. Source methods
 * do not assign bx/bA: stored_in_water reads the real existing flag only.
 * No active Living/Player tick, movement integration or implicit pool reset.
 *
 * The actual code Registry is an explicit native namespace context, independent
 * of nullable Entity.world. Retain validates side/profile without class use;
 * the reached g/h Material read follows all prior pooled bounds operations.
 * Keep it live/exclusive with its Material/profile dependencies. Nonnull World
 * must use that same Registry and Entity-class owner; no synthetic World or
 * replacement Material set is created. The native borrow prevents retirement
 * until the call ends, including on a source-descendant failure.
 *
 * Relative acceleration borrows the caller's initialized, unmodified trig table
 * as the existing core arithmetic context. It performs no World access and
 * works with the source null World. Future callers must supply the table for
 * their actual shared game-class namespace; this call does not initialize,
 * copy, hash, rebind or retain an independent Math class/table.
 *
 * Keep Entity, Worlds, contexts and all dependencies live/exclusive through
 * synchronous descendants. Outputs/errors/limits must be disjoint from owners
 * and each other. Native failure preserves outputs and every completed bounds,
 * List, vector and field prefix; constructor diagnostics are untouched. Holds
 * preserve storage, not mutable pooled coordinates or List contents. Optional
 * material limits apply to the reached World material scan, not collision
 * collection; existing descendant limits remain independent. Strict x86-64
 * FP is required only at reached arithmetic. Unsupported collision subtypes
 * retain their actual existing failure rather than a fabricated null box. */
mc_entity_status mc_entity_environment_water(mc_entity *, mc_registry_classes *,
                                             const mc_world_material_movement_limits *, bool *,
                                             mc_entity_environment_error *);
mc_entity_status mc_entity_environment_lava(mc_entity *, mc_registry_classes *,
                                            const mc_world_material_movement_limits *, bool *,
                                            mc_entity_environment_error *);
mc_entity_status mc_entity_environment_stored_in_water(mc_entity *, bool *,
                                                       mc_entity_environment_error *);
mc_entity_status mc_entity_environment_clearance(mc_entity *, double, double, double,
                                                 const mc_world_material_movement_limits *, bool *,
                                                 mc_entity_environment_error *);
mc_entity_status mc_entity_environment_relative(mc_entity *, const mc_math_trig *, float, float,
                                                float, mc_entity_environment_error *);

#endif
