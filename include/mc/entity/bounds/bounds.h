#ifndef MC_ENTITY_BOUNDS_BOUNDS_H
#define MC_ENTITY_BOUNDS_BOUNDS_H

#include "mc/entity/classes/classes.h"

#include <stdbool.h>

typedef struct mc_entity_bounds {
    double min_x, min_y, min_z, max_x, max_y, max_z;
} mc_entity_bounds;

typedef struct mc_entity_bounds_error {
    mc_entity_status status;
    const char *operation;
    uint16_t bytecode;
    /* Reached pooled-factory call beneath a transform, then its actual child
     * (class initialization, direct factory or List append). Unreached frames
     * remain zero. Root factory operations use their own operation/bytecode. */
    mc_entity_error factory;
    mc_entity_error pool;
} mc_entity_bounds_error;

/* Shared eq/cz pool in this loaded Entity classes owner. A successful get or
 * pooled transform returns one native hold. Reset only resets the source
 * cursor: later gets overwrite the SAME boxes, including held boxes. Client
 * clear drops the pool references and resets the cursor, retaining backing
 * capacity. Held boxes survive clear until their final release. No operation
 * resets automatically per Entity, World or movement.
 *
 * Keep classes live and access it exclusively. Free is BUSY while any bounds
 * hold remains. Retain/release accept only actual live pooled boxes of this
 * owner; Entity's permanent bounds and caller-owned values are not adopted.
 * Holds protect storage, not coordinates or pool membership. Outputs must be
 * disjoint from owner/error storage. Failure preserves out, not reached pool
 * initialization, append, cursor or coordinate effects. Native allocation
 * limits count boxes, pointer capacity and overlapping replacement storage.
 * Acquisition is constant time except backing growth; external retain/release
 * validate identity by scanning this owner's live pooled/detached objects.
 * Client clear has no server counterpart; calling it for SERVER is invalid.
 * These calls initialize only the bounds class, never Entity or Random. */
mc_entity_status mc_entity_bounds_pool_get(mc_entity_classes *classes, double min_x, double min_y,
                                           double min_z, double max_x, double max_y, double max_z,
                                           mc_entity_bounds **out, mc_entity_bounds_error *error);
mc_entity_status mc_entity_bounds_pool_reset(mc_entity_classes *classes,
                                             mc_entity_bounds_error *error);
mc_entity_status mc_entity_bounds_pool_clear_client(mc_entity_classes *classes,
                                                    mc_entity_bounds_error *error);
mc_entity_status mc_entity_bounds_retain(mc_entity_classes *classes, mc_entity_bounds *bounds);
mc_entity_status mc_entity_bounds_release(mc_entity_classes *classes, mc_entity_bounds *bounds);

/* The complete numeric movement helpers. Inputs must be live/non-null, but
 * need not come from the pool. Six arguments are evaluated before the pooled
 * factory, so reuse of source's own slot is allowed. No ordering, finite-value,
 * positive-size or epsilon restriction is added. Sweep extends only in each
 * delta's direction; expand and contract are symmetric, shift translates both
 * ends, and copy acquires another pooled result. No Vec3, ray or String runtime
 * is implied. Two-NaN addition selects and quiets the right operand's payload,
 * as observed in the captured x86-64 interpreter. Arithmetic requires the
 * java.h strict floating environment; host FP exception flags are not exposed. */
mc_entity_status mc_entity_bounds_sweep(mc_entity_classes *classes, const mc_entity_bounds *bounds,
                                        double x, double y, double z, mc_entity_bounds **out,
                                        mc_entity_bounds_error *error);
mc_entity_status mc_entity_bounds_expand(mc_entity_classes *classes, const mc_entity_bounds *bounds,
                                         double x, double y, double z, mc_entity_bounds **out,
                                         mc_entity_bounds_error *error);
mc_entity_status mc_entity_bounds_shift(mc_entity_classes *classes, const mc_entity_bounds *bounds,
                                        double x, double y, double z, mc_entity_bounds **out,
                                        mc_entity_bounds_error *error);
mc_entity_status mc_entity_bounds_contract(mc_entity_classes *classes,
                                           const mc_entity_bounds *bounds, double x, double y,
                                           double z, mc_entity_bounds **out,
                                           mc_entity_bounds_error *error);
mc_entity_status mc_entity_bounds_copy(mc_entity_classes *classes, const mc_entity_bounds *bounds,
                                       mc_entity_bounds **out, mc_entity_bounds_error *error);

/* Allocation-free source bodies on actual coordinates. All pointers must be
 * live/non-null. Clamps treat fixed as receiver, moving as argument and return
 * the adjusted delta. Touching cross-axis faces do not collide. The rejection
 * comparisons deliberately retain the original unordered/NaN behavior.
 * Translate returns target's same identity; the native assign convenience also
 * returns target after the original void copy body. Self-assignment is valid.
 * No source class initialization or native hold change occurs. */
double mc_entity_bounds_clip_x(const mc_entity_bounds *fixed, const mc_entity_bounds *moving,
                               double delta);
double mc_entity_bounds_clip_y(const mc_entity_bounds *fixed, const mc_entity_bounds *moving,
                               double delta);
double mc_entity_bounds_clip_z(const mc_entity_bounds *fixed, const mc_entity_bounds *moving,
                               double delta);
bool mc_entity_bounds_intersects(const mc_entity_bounds *fixed, const mc_entity_bounds *moving);
mc_entity_bounds *mc_entity_bounds_translate(mc_entity_bounds *target, double x, double y,
                                             double z);
mc_entity_bounds *mc_entity_bounds_assign(mc_entity_bounds *target, const mc_entity_bounds *source);

#endif
