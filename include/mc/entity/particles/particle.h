#ifndef MC_ENTITY_PARTICLES_PARTICLE_H
#define MC_ENTITY_PARTICLES_PARTICLE_H

#include "mc/core/math/classes.h"
#include "mc/entity/entity.h"
#include "mc/entity/movement/movement.h"
#include "mc/item/registry/runtime.h"

typedef enum mc_particle_cause {
    MC_PARTICLE_NONE,
    MC_PARTICLE_OWNER,
    MC_PARTICLE_ENTITY,
    MC_PARTICLE_RANDOM,
    MC_PARTICLE_NULL_REFERENCE,
    MC_PARTICLE_MATH,
    MC_PARTICLE_REGISTRY,
    MC_PARTICLE_MOVEMENT
} mc_particle_cause;

typedef enum mc_particle_throwable {
    MC_PARTICLE_NO_THROWABLE,
    MC_PARTICLE_SOURCE_EXCEPTION,
    MC_PARTICLE_SOURCE_ERROR,
    MC_PARTICLE_UNCLASSIFIED_THROWABLE
} mc_particle_throwable;

/* Passive values copied only from actual completed constructor operations.
 * Math slots follow60/81/102/120/123; private slots238/252/266/287. The private
 * initial state is captured immediately before the actual first float call,
 * never derived from its final state or supplied as constructor entropy. */
typedef struct mc_particle_constructor_random {
    double math_values[5];
    float own_values[4];
    size_t math_count, own_count;
    bool own_initial_stored;
    uintptr_t own_identity;
    mc_java_random own_initial;
} mc_particle_constructor_random;

typedef struct mc_particle_error {
    mc_entity_status status;
    mc_particle_cause cause;
    const char *operation, *source_class, *source_method;
    unsigned int source_pc, particle_pc, pickup_pc;
    mc_entity_error entity;
    mc_java_random_profile_status random;
    /* Append-only compact PB/XW constructor diagnostics. Legacy EM callers
     * do not carry a movement aggregate through their pickup stack frames. */
    const char *source_descriptor, *exception_class;
    unsigned int item_pc, tick_pc;
    bool native_policy, source_exception;
    mc_particle_throwable throwable;
    mc_math_class_error math;
    mc_registry_error registry;
    mc_particle_constructor_random constructor_random;
} mc_particle_error;

/* PB-only full tick diagnostics belong in caller-owned/heap storage. The
 * selected movement cause is a complete value; its WorldAccess receipts are
 * borrowed from the live producer graph and must be captured by a retained
 * parent before that graph can retire. No movement pointer escapes scratch. */
typedef struct mc_item_particle_tick_error {
    mc_particle_error particle;
    mc_entity_move_error movement;
} mc_item_particle_tick_error;

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
    unsigned int item_pc, tick_pc;
    bool tick_active;
} mc_particle_snapshot;

/* Caller-owned full initialization; never consumes an owning child receipt. */
void mc_particle_error_init(mc_particle_error *error);
void mc_item_particle_tick_error_init(mc_item_particle_tick_error *error);

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

/* Genuine client PB, with XW and SN ancestors. Allocate RAW before evaluating
 * the actual constructor operands. Construction irreversibly preserves IDs,
 * fields and entropy on failure. The nullable Item is a live operand only,
 * never a retained Source field; registry keeps its real storage alive through
 * the call. Registry/Math are native contexts, used only when their reached
 * bodies require them. In particular IN active use164 follows all five shared
 * Math.random draws, and the Item invocation17 follows returned XW.
 *
 * error is required, caller-owned, exclusive and disjoint from all inputs.
 * Keep the actual same-profile Registry/Math/World and Entity graph live and
 * exclusive through synchronous descendants and diagnostic consumption.
 * No entropy injection, constructor retry, pool reset or Source repair occurs.
 * Ordinary Entity release retires RAW, failed and READY PB allocations. */
mc_entity_status mc_item_particle_allocate(mc_entity_classes *classes, mc_entity **out,
                                           mc_entity_error *error);
mc_entity_status mc_item_particle_construct(mc_entity *raw, mc_world_runtime *world,
                                            mc_registry_classes *registry, mc_math_classes *math,
                                            double x, double y, double z, const mc_item *item,
                                            mc_particle_error *error);
/* Inherited XW.w_, including age postincrement, expiry K then continued
 * gravity/movement/damping. The real SN movement context supplies its same
 * loaded Registry namespace. NULL limits selects its existing work ceiling.
 * Failure retains every Source prefix and complete movement cause. No tick
 * executes on an already active same PB. No direct Random/IN call occurs.
 * error is required caller-owned/heap storage, unsuitable for small stacks. */
mc_entity_status mc_item_particle_tick(mc_entity *particle, mc_registry_classes *registry,
                                       const mc_entity_move_limits *limits,
                                       mc_item_particle_tick_error *error);
/* Actual PB.c_ literal2; no phase or Source field access. */
mc_entity_status mc_item_particle_layer(const mc_entity *particle, int32_t *out);

#endif
