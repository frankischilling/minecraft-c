#ifndef MC_WORLD_PATH_PATH_H
#define MC_WORLD_PATH_PATH_H

#include "mc/core/math/classes.h"
#include "mc/entity/entity.h"
#include "mc/entity/vector/vector.h"

typedef struct mc_path mc_path;
typedef struct mc_path_point mc_path_point;

typedef enum mc_path_status {
    MC_PATH_OK,
    MC_PATH_ARGUMENT,
    MC_PATH_STATE,
    MC_PATH_BUSY,
    MC_PATH_LIMIT,
    MC_PATH_ALLOCATION,
    MC_PATH_SOURCE_EXCEPTION,
    MC_PATH_CHILD_FAILED,
    MC_PATH_UNIMPLEMENTED
} mc_path_status;

typedef struct mc_path_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_path_site;

typedef struct mc_path_error {
    mc_path_status status;
    const char *operation, *exception_class;
    bool native_policy, source_exception;
    mc_path_site site, child_site;
    mc_entity_error entity;
    mc_entity_vector_error vector;
    mc_math_class_error math;
} mc_path_error;

typedef struct mc_path_point_state {
    int32_t a, b, c, j, d;
    float e, f, g;
    const mc_path_point *h;
    bool i, constructed;
    unsigned int constructor_pc;
} mc_path_point_state;

typedef struct mc_path_state {
    int32_t a, c;
    mc_path_point *const *b;
    size_t array_length, holds, arena_holds, arena_points, arena_bytes;
    mc_entity_classes *classes;
    const void *arena_identity;
    bool constructed;
    unsigned int constructor_pc;
} mc_path_state;

/* DH contains only its real D array, length and shared cursor. The native
 * arena preserves D identities/predecessors without a World/Entity backedge.
 * A valid occurrence release is infallible; all access is exclusive. */
void mc_path_error_init(mc_path_error *);
mc_path_status mc_path_retain(mc_path *, mc_path_error *);
void mc_path_release(mc_path *);
bool mc_path_inspect(const mc_path *, mc_path_state *);
bool mc_path_point_inspect(const mc_path *, const mc_path_point *, mc_path_point_state *);
mc_path_status mc_path_advance(mc_path *, mc_path_error *);
mc_path_status mc_path_finished(mc_path *, bool *, mc_path_error *);
mc_path_status mc_path_last(mc_path *, const mc_path_point **, mc_path_error *);
mc_path_status mc_path_waypoint(mc_path *, mc_entity *, mc_entity_vector **, mc_path_error *);

#endif
