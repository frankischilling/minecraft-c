#ifndef MC_ENTITY_PARTICLES_PARTICLE_INTERNAL_H
#define MC_ENTITY_PARTICLES_PARTICLE_INTERNAL_H

#include "../entity_internal.h"
#include "mc/entity/particles/particle.h"

struct mc_particle_state {
    mc_particle_fields fields;

    union {
        mc_pickup_particle_fields pickup; /* Actual EM declared fields. */

        struct {
            unsigned int item_pc, tick_pc;
            bool tick_active;
        } item_native; /* PB/QM native diagnostics; no declared Source fields. */
    };

    const mc_block *digging_block;         /* Actual QM.a. */
    mc_registry_classes *digging_registry; /* Native lifetime of that Block. */
    int32_t digging_face;                  /* Actual QM.o. */
    unsigned int particle_pc, pickup_pc;
};

typedef struct mc_pickup_particle_object {
    mc_entity entity;
    struct mc_particle_state state;
} mc_pickup_particle_object;

/* PB has only the inherited XW Source fields. Its native allocation uses this
 * same complete particle layout; unused EM cells are not PB Source fields. */
typedef mc_pickup_particle_object mc_item_particle_object;

/* Small constructor diagnostic frame keeps the qualified EM facade off the
 * large complete movement-error stack. Publish expands only into caller-owned
 * public storage. No frame member owns a WorldAccess receipt. */
typedef struct mc_particle_constructor_error {
    mc_entity_status status;
    mc_particle_cause cause;
    const char *operation, *source_class, *source_method;
    unsigned int source_pc, particle_pc, pickup_pc;
    mc_entity_error entity;
    mc_java_random_profile_status random;
    const char *source_descriptor, *exception_class;
    unsigned int item_pc, tick_pc;
    bool native_policy, source_exception;
    mc_particle_throwable throwable;
    mc_math_class_error math;
    mc_registry_error registry;
    mc_particle_constructor_random constructor_random;
} mc_particle_constructor_error;

void mc_particle_constructor_error_publish(mc_particle_error *,
                                           const mc_particle_constructor_error *);
mc_entity_status mc_particle_constructor_fail(mc_particle_constructor_error *, mc_entity_status,
                                              mc_particle_cause, const char *, const char *,
                                              const char *, unsigned int);
bool mc_particle_construct_ancestor(mc_entity *, mc_world_runtime *, double, double, double, double,
                                    double, double, mc_math_classes *, bool, mc_math_classes **,
                                    mc_particle_constructor_error *);
/* Same genuine PB body as the public facade. A nonnull reached_math points
 * to an initially NULL, disjoint native owning slot. Only reached XW164 may
 * retain/capture Math into it, BEFORE ensure and including ensure failure.
 * The retained parent consumes that exact occurrence after all diagnostics
 * and pending children retire. A NULL slot preserves public release behavior.
 * No Math service read/hold is moved before the five shared Random draws. */
mc_entity_status mc_item_particle_construct_entered(mc_entity *, mc_world_runtime *,
                                                    mc_registry_classes *, mc_math_classes *,
                                                    double, double, double, const mc_item *,
                                                    mc_math_classes **reached_math,
                                                    mc_particle_error *);
/* Passive exact immutable declaring-class cause classification, shared with
 * the N caller's reached GM getstatic. Never alters the supplied Registry. */
void mc_particle_registry_classification(mc_registry_classes *, const mc_registry_error *,
                                         mc_particle_throwable *, const char **);

void mc_pickup_particle_dispose(mc_entity *entity);
void mc_digging_particle_dispose(mc_entity *entity);
mc_entity_status mc_particle_tick_inherited(mc_entity *, mc_entity_class_kind,
                                            mc_registry_classes *, const mc_entity_move_limits *,
                                            mc_item_particle_tick_error *);
mc_entity_status mc_digging_particle_construct_entered(mc_entity *, mc_world_runtime *,
                                                       mc_registry_classes *, mc_math_classes *,
                                                       double, double, double, double, double,
                                                       double, const mc_block *, int32_t, int32_t,
                                                       mc_math_classes **, mc_particle_error *);
mc_entity_status mc_explosion_particle_construct_entered(mc_entity *, mc_world_runtime *,
                                                         mc_math_classes *, double, double, double,
                                                         double, double, double, mc_math_classes **,
                                                         mc_particle_error *);
void mc_particle_movement_exception(mc_particle_error *, const mc_entity_move_error *,
                                    mc_registry_classes *);

#endif
