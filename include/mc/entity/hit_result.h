#ifndef MC_ENTITY_HIT_RESULT_H
#define MC_ENTITY_HIT_RESULT_H

#include "mc/entity/bounds/bounds.h"
#include "mc/entity/entity.h"
#include "mc/entity/vector/vector.h"

/* Canonical VF/NH and JG/GD in the SAME loaded Entity classes namespace.
 * Allocate RAW before evaluating constructor arguments. Construction preserves
 * reached failed fields and cannot retry. Nullable inputs are source operands.
 * All access is exclusive; error/output storage must be disjoint from owners.
 * Views borrow the actual identities, not immutable coordinate snapshots.
 * Retain/release balance independent native occurrences in every phase.
 * Release(NULL slot value) succeeds; failure preserves remaining rights/slot.
 * The same tag is aliased as mc_client_source_vf by the application owner. */
typedef struct mc_entity_hit_result mc_entity_hit_result;
typedef struct mc_entity_hit_kind mc_entity_hit_kind;
typedef struct mc_entity_hit_name mc_entity_hit_name;
typedef struct mc_entity_bounds_intercept_call mc_entity_bounds_intercept_call;

typedef enum mc_entity_hit_status {
    MC_ENTITY_HIT_OK = 0,
    MC_ENTITY_HIT_INVALID_ARGUMENT,
    MC_ENTITY_HIT_INVALID_PHASE,
    MC_ENTITY_HIT_SOURCE_NULL_REFERENCE,
    MC_ENTITY_HIT_NATIVE_FAILED,
    MC_ENTITY_HIT_VECTOR_FAILED,
    MC_ENTITY_HIT_CLASS_FAILED,
    MC_ENTITY_HIT_UNSUPPORTED_SOURCE,
    MC_ENTITY_HIT_UNSUPPORTED_FP,
    MC_ENTITY_HIT_BUSY
} mc_entity_hit_status;

typedef enum mc_entity_hit_phase {
    MC_ENTITY_HIT_RAW = 0,
    MC_ENTITY_HIT_CONSTRUCTING,
    MC_ENTITY_HIT_READY,
    MC_ENTITY_HIT_FAILED_CONSTRUCTION
} mc_entity_hit_phase;

typedef struct mc_entity_hit_kind_error {
    mc_entity_hit_status status;
    const char *operation;
    bool native_policy;
    mc_entity_geometry_site site;
    mc_entity_error native;
} mc_entity_hit_kind_error;

typedef struct mc_entity_hit_error {
    mc_entity_hit_status status;
    const char *operation;
    bool native_policy;
    mc_entity_geometry_site site;
    mc_entity_error native;
    mc_entity_vector_error vector;
    mc_entity_hit_kind_error kind;
} mc_entity_hit_error;

typedef struct mc_entity_hit_fields {
    const mc_entity_hit_kind *a;
    int32_t b, c, d, e;
    mc_entity_vector *f;
    mc_entity *g;
} mc_entity_hit_fields;

typedef struct mc_entity_hit_result_view {
    const mc_entity_classes *classes;
    mc_entity_hit_phase phase;
    mc_entity_hit_fields fields;
    size_t native_holds;
    const mc_entity_hit_error *construction_error;
} mc_entity_hit_result_view;

mc_entity_hit_status mc_entity_hit_result_allocate(mc_entity_classes *classes,
                                                   mc_entity_hit_result **out,
                                                   mc_entity_hit_error *error);
mc_entity_hit_status mc_entity_hit_result_construct_block(mc_entity_hit_result *raw, int32_t x,
                                                          int32_t y, int32_t z, int32_t face,
                                                          const mc_entity_vector *point,
                                                          mc_entity_hit_error *error);
mc_entity_hit_status mc_entity_hit_result_construct_entity(mc_entity_hit_result *raw,
                                                           mc_entity *target,
                                                           mc_entity_hit_error *error);
mc_entity_hit_status mc_entity_hit_result_retain(mc_entity_hit_result *result,
                                                 mc_entity_hit_error *error);
mc_entity_hit_status mc_entity_hit_result_release(mc_entity_hit_result **owned,
                                                  mc_entity_hit_error *error);
mc_entity_hit_status mc_entity_hit_result_inspect(const mc_entity_hit_result *result,
                                                  mc_entity_hit_result_view *out);

typedef struct mc_entity_hit_kind_view {
    const mc_entity_classes *classes;
    mc_entity_hit_phase phase;
    const mc_entity_hit_name *name; /* actual immutable Enum.name identity */
    const uint16_t *name_units;
    size_t name_length;
    int32_t ordinal;
} mc_entity_hit_kind_view;

typedef struct mc_entity_hit_kind_class_view {
    mc_entity_class_phase phase;
    const mc_entity_hit_kind *a, *b;         /* actual published statics */
    const mc_entity_hit_kind *candidates[2]; /* native retained NEW prefixes */
    const void *values_identity;
    size_t values_length;
    const mc_entity_hit_kind *values[2];
    const mc_entity_hit_kind_error *failure;
} mc_entity_hit_kind_class_view;

mc_entity_hit_status mc_entity_hit_kind_inspect(const mc_entity_hit_kind *kind,
                                                mc_entity_hit_kind_view *out);
mc_entity_hit_status mc_entity_hit_kind_class_inspect(const mc_entity_classes *classes,
                                                      mc_entity_hit_kind_class_view *out);

/* Bounded retained EQ/CZ invocation; begin may publish a failed call. Caller
 * keeps the SAME borrowed bounds receiver/owning Entity live until release.
 * Each canonical input and each generated plane vector receives its own hold.
 * Generated retains rejected slots; locals reflect source filtering. Normal
 * result is borrowed: retain it before call release/App publication. Failure
 * never becomes a successful miss; terminal and cleanup causes remain distinct.
 * Before frame publication failure preserves out and writes admission_error. */
typedef struct mc_entity_bounds_intercept_error {
    mc_entity_hit_status status;
    const char *operation;
    bool native_policy;
    mc_entity_geometry_site site;
    mc_entity_error native;
    mc_entity_geometry_error geometry;
    mc_entity_hit_error hit;
} mc_entity_bounds_intercept_error;

typedef enum mc_entity_bounds_intercept_phase {
    MC_ENTITY_BOUNDS_INTERCEPT_ACTIVE,
    MC_ENTITY_BOUNDS_INTERCEPT_RETURNED,
    MC_ENTITY_BOUNDS_INTERCEPT_FAILED,
    MC_ENTITY_BOUNDS_INTERCEPT_RETIRING
} mc_entity_bounds_intercept_phase;

typedef struct mc_entity_bounds_intercept_view {
    mc_entity_bounds_intercept_phase phase;
    bool active, retiring, source_returned;
    mc_entity_hit_status status;
    const mc_entity_bounds_intercept_error *error;
    const mc_entity_bounds_intercept_error *cleanup;
    const mc_entity_bounds *receiver; /* borrowed actual receiver */
    mc_entity_vector *start, *end;    /* same held input identities, nullable */
    mc_entity_vector *generated[6];   /* native ledger; includes rejected slots */
    mc_entity_vector *locals[6];      /* actual source locals, filtered to null */
    mc_entity_vector *nearest;
    int32_t face;
    mc_entity_hit_result *candidate; /* borrowed RAW/failed/ready NEW prefix */
    mc_entity_hit_result *result;    /* borrowed nullable NORMAL return only */
} mc_entity_bounds_intercept_view;

mc_entity_hit_status mc_entity_bounds_intercept_begin(
    mc_entity_classes *classes, const mc_entity_bounds *receiver, mc_entity_vector *start,
    mc_entity_vector *end, mc_entity_bounds_intercept_call **out, mc_entity_error *admission_error);
mc_entity_hit_status mc_entity_bounds_intercept_inspect(const mc_entity_bounds_intercept_call *call,
                                                        mc_entity_bounds_intercept_view *out);
mc_entity_hit_status mc_entity_bounds_intercept_release(mc_entity_bounds_intercept_call **owned,
                                                        mc_entity_hit_error *error);

/* Actual client h_/m_ dispatch, no READY/World/Random effects. */
mc_entity_hit_status mc_entity_pickable(mc_entity *receiver, bool *out, mc_entity_hit_error *error);
mc_entity_hit_status mc_entity_pick_border_size(mc_entity *receiver, float *out,
                                                mc_entity_hit_error *error);

#endif
