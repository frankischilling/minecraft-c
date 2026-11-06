#ifndef MC_ENTITY_POSITION_INTERNAL_H
#define MC_ENTITY_POSITION_INTERNAL_H

#include "entity_internal.h"

/* Actual sn.e(DDD) arithmetic, after its x/y/z stores. No argument admission,
 * owner mutation, pointer dereference of aW, pool work or callback occurs.
 * Callers own strict FP admission and the reached bounds invocation. */
mc_entity_bounds mc_entity_source_position_values(const mc_entity *entity, double x, double y,
                                                  double z, float half_width, float height);

typedef struct mc_entity_source_position_error {
    mc_entity_status status;
    const char *operation;
    unsigned int bytecode;
    bool native_policy, source_null;
} mc_entity_source_position_error;

/* Actual SN.e(DDD) on a held canonical receiver, without READY admission.
 * Position stores precede FP admission; capture the nullable permanent bounds
 * before evaluating its six arguments, then invoke that same receiver.
 * This synchronous leaf owns no children, allocations or borrowed diagnostics.
 * A NULL native Entity is an admission error; callers classify their actual
 * null virtual invocation before entering this body. */
mc_entity_status mc_entity_source_position_checked(mc_entity *, double x, double y, double z,
                                                   mc_entity_source_position_error *);

/* Actual SN.b(FF): two stores, no bounds rebuild, FP operation or READY gate.
 * The caller owns any inherited LS.b wrapper and its source invocation site. */
void mc_entity_source_size(mc_entity *, float width, float height);

#endif
