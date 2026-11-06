#ifndef MC_ENTITY_FALLING_SAND_TICK_H
#define MC_ENTITY_FALLING_SAND_TICK_H

#include "mc/entity/drop.h"
#include "mc/entity/movement/movement.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/placement.h"

typedef struct mc_falling_entity_tick_work mc_falling_entity_tick_work;

typedef enum mc_falling_entity_tick_child {
    MC_FALLING_TICK_CHILD_NONE,
    MC_FALLING_TICK_CHILD_MOVEMENT,
    MC_FALLING_TICK_CHILD_BLOCK,
    MC_FALLING_TICK_CHILD_MUTATION,
    MC_FALLING_TICK_CHILD_PLACEMENT,
    MC_FALLING_TICK_CHILD_PREDICATE,
    MC_FALLING_TICK_CHILD_EFFECT,
    MC_FALLING_TICK_CHILD_STACK,
    MC_FALLING_TICK_CHILD_CONSTRUCTOR,
    MC_FALLING_TICK_CHILD_PUBLICATION,
    MC_FALLING_TICK_CHILD_REGISTRY
} mc_falling_entity_tick_child;

/* Zero-initialized, noncopyable retained diagnostic. Typed pointers borrow
 * this same private work until successful retirement. No owner is inferred
 * from scalar IDs or a discarded child error. */
typedef struct mc_falling_entity_tick_error {
    mc_entity_status status, cleanup_status;
    const char *operation, *cleanup_operation;
    unsigned int bytecode;
    const char *drop_method;
    unsigned int drop_wrapper_bytecode, drop_bytecode;
    mc_falling_entity_tick_child child;
    const mc_entity_move_error *movement;
    const mc_world_storage_error *block;
    const mc_world_runtime_acquisition_error *acquisition;
    const mc_world_mutation_error *mutation, *predicate;
    const mc_world_placement_error *placement;
    const mc_entity_effect_error *effect;
    const mc_entity_error *constructor;
    const mc_world_entity_error *publication;
    const mc_registry_error *registry;
    const mc_entity_drop_error *drop; /* Exact retained inherited SN call, borrowed. */
    mc_item_stack_status stack_status;
    mc_entity *item;      /* Same reached Item allocation, borrowed. */
    mc_item_stack *stack; /* Same reached Stack allocation, borrowed. */
    mc_falling_entity_tick_work *work;
} mc_falling_entity_tick_error;

/* Initialize fresh storage only; never overwrite a live work occurrence.
 * Retiring an empty/admission-only record succeeds and returns it to empty.
 * Active/reached child cleanup refusal preserves work and the source cause;
 * cleanup_status/operation describe only that separate native refusal. */
void mc_falling_entity_tick_error_init(mc_falling_entity_tick_error *);
bool mc_falling_entity_tick_error_retire(mc_falling_entity_tick_error *);

/* Actual client ju.w_(), including inherited mover/K and actual sn.b(II)
 * Item creation/publication on a failed landing or age>100. No super tick,
 * first-age guard, piston special case, implicit pool reset or remote gate is
 * introduced. Each source World read is fresh; earlier effects survive every
 * reached failure. Java int wrapping and strict binary64 arithmetic apply.
 *
 * Actual code Registry is independent of nullable Entity.world. The caller
 * keeps the complete graph exclusive/live through callbacks and retirement.
 * NULL movement limits select the existing mover ceiling. Error is mandatory
 * and starts zeroed; a live failure cannot be reused or copied. Success retires
 * temporary native occurrences and returns an empty diagnostic. Failure keeps
 * actual actor/Registry/World/Stack/Item and child producers until checked
 * error_retire; that operation never replays source work or removes published
 * actors. Constructor diagnostics are unchanged. Server ticking is unqualified. */
mc_entity_status mc_falling_entity_tick_in_context(mc_entity *, mc_registry_classes *,
                                                   const mc_entity_move_limits *,
                                                   mc_falling_entity_tick_error *);

#endif
