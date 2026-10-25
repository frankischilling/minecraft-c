#ifndef MC_ENTITY_VECTOR_VECTOR_H
#define MC_ENTITY_VECTOR_VECTOR_H

#include "mc/entity/classes/classes.h"

typedef struct mc_entity_vector {
    double x, y, z;
} mc_entity_vector;

typedef struct mc_entity_vector_error {
    mc_entity_status status;
    const char *operation;
    uint16_t bytecode;
    mc_entity_error factory, pool;
} mc_entity_vector_error;

typedef struct mc_entity_vector_snapshot {
    mc_entity_class_phase phase;
    int32_t cursor;
    const void *pool_identity;
    size_t size, capacity, holds;
} mc_entity_vector_snapshot;

/* Reached bt/ba subset: direct/pooled factories, shared pool lifecycle, add,
 * length and normalize. The same loaded Entity-class owner spans Worlds.
 * Successful factories return one native hold. Pool reset changes only the
 * cursor; later acquisition overwrites the same objects even while held.
 * Client clear detaches held objects and retains backing capacity. No caller
 * implicitly resets or clears. Direct construction canonicalizes signed zero;
 * pooled set preserves raw coordinates. Class/objects/pointer arrays use the
 * existing Entity classes allocation ledger. Failed initialization is sticky.
 * All access is exclusive; outputs/errors must be disjoint from owners.
 * Math requires the existing strict x86-64 floating environment. Only this
 * reached subset is implemented, not ray/rotation/String or all Vec3 methods. */
mc_entity_status mc_entity_vector_create(mc_entity_classes *, double, double, double,
                                         mc_entity_vector **, mc_entity_vector_error *);
mc_entity_status mc_entity_vector_pool_get(mc_entity_classes *, double, double, double,
                                           mc_entity_vector **, mc_entity_vector_error *);
mc_entity_status mc_entity_vector_pool_reset(mc_entity_classes *, mc_entity_vector_error *);
mc_entity_status mc_entity_vector_pool_clear_client(mc_entity_classes *, mc_entity_vector_error *);
mc_entity_status mc_entity_vector_retain(mc_entity_classes *, mc_entity_vector *);
mc_entity_status mc_entity_vector_release(mc_entity_classes *, mc_entity_vector *);
mc_entity_status mc_entity_vector_get_snapshot(const mc_entity_classes *,
                                               mc_entity_vector_snapshot *);
mc_entity_status mc_entity_vector_add(mc_entity_classes *, const mc_entity_vector *, double, double,
                                      double, mc_entity_vector **, mc_entity_vector_error *);
mc_entity_status mc_entity_vector_normalize(mc_entity_classes *, const mc_entity_vector *,
                                            mc_entity_vector **, mc_entity_vector_error *);
mc_entity_status mc_entity_vector_length(const mc_entity_vector *, double *,
                                         mc_entity_vector_error *);

#endif
