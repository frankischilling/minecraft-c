#ifndef MC_ENTITY_MOVEMENT_MOVEMENT_H
#define MC_ENTITY_MOVEMENT_MOVEMENT_H

#include "mc/entity/movement/effects.h"
#include "mc/world/runtime/contact.h"
#include "mc/world/runtime/step.h"

typedef struct mc_entity_move_limits {
    size_t max_loop_work;
} mc_entity_move_limits;

#define MC_ENTITY_MOVE_DEFAULT_LOOP_WORK ((size_t)1048576)

typedef struct mc_entity_move_error {
    mc_entity_status status;
    const char *operation;
    unsigned int bytecode;
    size_t loop_work, max_loop_work;
    mc_entity_error entity;
    mc_entity_bounds_error bounds;
    mc_world_collision_error collision, block_read;
    mc_world_movement_error world;
    mc_world_entity_contact_error contact;
    mc_world_step_error step;
    mc_entity_effect_error effect;
    mc_registry_error registry;
} mc_entity_move_error;

/* Passive full fresh initialization in caller storage; NULL is a no-op.
 * Retains the same complete children/sentinels as the value-returning helper,
 * without an additional automatic movement-error aggregate. No owner access,
 * allocation, source operation or Class/World state change occurs. */
void mc_entity_move_error_init(mc_entity_move_error *error);
mc_entity_move_error mc_entity_move_empty_error(void);

/* Actual sn.b/lq.a(DDD)V for held READY canonical Item/Falling/LocalPlayer. Move
 * the same permanent bounds, retain real pooled copies and mutable collision
 * List views, and preserve every completed field/World/pool prefix on failure.
 * The caller keeps owners exclusive through synchronous descendants. Native
 * holds protect lifetime, not values or mutable List contents. No implicit
 * pool reset, collision snapshot, World rebind, tick or publication occurs.
 * No-clip never reads World; later source calls reload their actual receiver.
 * Reached Entity code statics require the held World's exact Entity class
 * owner; matching Random profiles alone do not establish a loaded namespace.
 * Constructor diagnostics remain untouched. Error storage and limits must be
 * disjoint from owners/dependencies. The strict x86-64 FP profile is required.
 *
 * NULL limits selects the default native loop-work ceiling. A work unit is a
 * ledge collision attempt, indexed clipping iteration or contact cell. The
 * ceiling prevents unbounded native work for source loops with nonfinite or
 * Java-wrapped inputs. Exhaustion fails LIMIT at the pending source call,
 * keeping its completed prefix; it never reports truncated work as success.
 * Existing allocation and descendant limits remain independent. Successful
 * diagnostics have status OK and the reached return PC88 or2028.
 * The true-walking interval preserves sqrt-to-float distance, live support/
 * fence/snow/material reads, sound identity/arguments, then actual Block.b.
 * LocalPlayer fall/damage/statistic/close frontiers are reported by effects;
 * no successful empty descendant or implicit Living travel is supplied.
 * Other Entity kinds, active ticks and
 * unowned nonnull relationship dispatch remain explicit unfinished work. */
mc_entity_status mc_entity_move(mc_entity *entity, double x, double y, double z,
                                const mc_entity_move_limits *limits, mc_entity_move_error *error);
/* The caller's actual loaded Registry namespace is independent of nullable
 * Entity.world. It is borrowed/exclusive and read only at a reached source
 * static operation. No-clip reads neither World nor the code Registry. */
mc_entity_status mc_entity_move_in_context(mc_entity *entity, mc_registry_classes *code_registry,
                                           double x, double y, double z,
                                           const mc_entity_move_limits *limits,
                                           mc_entity_move_error *error);

#endif
