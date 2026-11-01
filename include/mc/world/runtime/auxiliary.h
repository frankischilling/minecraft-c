#ifndef MC_WORLD_RUNTIME_AUXILIARY_H
#define MC_WORLD_RUNTIME_AUXILIARY_H

#include "mc/block/registry/runtime.h"
#include "mc/world/runtime/world_access.h"

typedef struct mc_world_mutation_error mc_world_mutation_error;

/* Noncopyable caller-owned diagnostic, initially zero. A reached Fire air-write
 * allocates its large mutation child only when needed; failure keeps that exact
 * frame until error_retire. WorldAccess failures borrow the actual concrete
 * receiver's receipt, whose enclosing World/client graph stays live. */
typedef struct mc_world_auxiliary_error {
    mc_world_storage_status status;
    const char *operation, *method, *descriptor;
    unsigned int source_pc, world_access_pc;
    bool returned, native_policy, source_exception;
    const char *exception_class;
    mc_world_storage_error cause, query;
    mc_registry_error registry;
    mc_world_access_failure world_access_failure;
    mc_world_mutation_error *mutation;
} mc_world_auxiliary_error;

/* Actual client fd.a(Player,IIIII)V. Traverse current u by index, retaining only
 * the current receiver across the synchronous callback. Duplicate/null entries
 * and callback mutations preserve source order. No event is silently ignored.
 * Caller supplies the already evaluated nullable Player and event operands.
 * Fresh disjoint error is required; World/dependencies stay live/exclusive. */
mc_world_storage_status mc_world_runtime_auxiliary_effect(mc_world_runtime *, mc_entity *player,
                                                          int32_t effect, int32_t x, int32_t y,
                                                          int32_t z, int32_t data,
                                                          mc_world_auxiliary_error *);

/* Actual fd.a(Player,IIII)V: adjust the neighbor in face order, read its ID,
 * then read Block.as. Fire invokes effect1004 before notifying air-write; the
 * setter's Boolean is ignored, as in source. out is a native observation of
 * whether the Fire arm returned, and stays unchanged on failure. */
mc_world_storage_status mc_world_runtime_extinguish_fire(mc_world_runtime *, mc_entity *player,
                                                         int32_t x, int32_t y, int32_t z,
                                                         int32_t face, bool *out,
                                                         mc_world_auxiliary_error *);

/* Free only this diagnostic's optional owned frame and zero the record. This
 * performs no source/WorldAccess operation and consumes no borrowed receipt.
 * The enclosing receiver owns its failed native child through checked cleanup. */
bool mc_world_auxiliary_error_retire(mc_world_auxiliary_error *);

#endif
