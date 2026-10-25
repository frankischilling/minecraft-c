#ifndef MC_WORLD_RUNTIME_MATERIAL_MOVEMENT_H
#define MC_WORLD_RUNTIME_MATERIAL_MOVEMENT_H

#include "mc/entity/vector/vector.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/movement.h"

typedef struct mc_world_material_movement_limits {
    size_t max_cell_work;
} mc_world_material_movement_limits;

#define MC_WORLD_MATERIAL_MOVEMENT_DEFAULT_CELL_WORK ((size_t)1048576)

typedef struct mc_world_material_movement_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode, fluid_bytecode, fluid_child_bytecode, acceleration_bytecode,
        stair_bytecode;
    size_t delegation_depth;
    size_t cell_work, max_cell_work;
    mc_world_storage_error cause;
    mc_world_movement_error area;
    mc_world_collision_error block;
    mc_world_material_error material;
    mc_registry_error registry;
    mc_entity_vector_error vector;
    mc_entity_error entity;
} mc_world_material_movement_error;

mc_world_material_movement_error mc_world_material_movement_empty_error(void);

/* Exact fd.a(eq,ln)/dj.a(cz,hj): half-open floored cell scan with identity
 * comparison, no presence preflight or fluid-height test. NULL material is an
 * ordinary identity argument. Exact fd.b(eq)/dj.c(cz) additionally decrements
 * each negative minimum, then invokes actual Material.d on nonnull Blocks.
 * Both perform actual acquiring World reads in X/Y/Z order, including outside
 * borders/Y and air. Reversed and nonfinite boxes retain Java loop behavior.
 *
 * Exact fd/dj.a(bounds,material,Entity) first checks inclusive chunk presence,
 * then allocates from the same loaded bt/ba pool, invokes actual per-Block
 * acceleration and applies normalized flow*0.014 in X/Y/Z store order.
 * Flowing and still fluid use the real rp/kv vector body; Stairs follows its
 * actual wrapped Block virtual call, and remaining registered concrete Blocks
 * inherit the proved empty uu/na body. No vector pool reset,
 * tick, missing chunk surrogate, Material substitute or Entity subtype test
 * is inserted. Entity may be NULL until a nonzero accumulated vector actually
 * reaches the velocity fields. A nonnull Entity must be a live held actual
 * owner in the same Entity namespace when those fields are reached.
 *
 * Hold all owners exclusive and dependencies live. GAME bound prefixes are
 * admitted; retirement/composition rejects. Outputs/errors/limits must be
 * disjoint from owners and each other. Native failure preserves out, completed
 * acquisitions, class/pool effects and velocity stores. Optional limits bound
 * cell/recursive-stair work without changing a completed source answer; exhaustion fails at
 * the pending cell. Strict x86-64 FP is required at reached arithmetic. */
mc_world_storage_status
mc_world_runtime_contains_material(mc_world_runtime *, const mc_entity_bounds *, mc_material *,
                                   const mc_world_material_movement_limits *, bool *,
                                   mc_world_material_movement_error *);
mc_world_storage_status mc_world_runtime_contains_liquid(mc_world_runtime *,
                                                         const mc_entity_bounds *,
                                                         const mc_world_material_movement_limits *,
                                                         bool *,
                                                         mc_world_material_movement_error *);
mc_world_storage_status
mc_world_runtime_material_accelerate(mc_world_runtime *, const mc_entity_bounds *, mc_material *,
                                     mc_entity *, const mc_world_material_movement_limits *, bool *,
                                     mc_world_material_movement_error *);

#endif
