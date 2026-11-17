#ifndef MC_WORLD_RUNTIME_REGISTRY_FAILURE_H
#define MC_WORLD_RUNTIME_REGISTRY_FAILURE_H

#include "mc/block/registry/registry.h"

typedef enum mc_world_registry_failure_kind {
    MC_WORLD_REGISTRY_FAILURE_UNCLASSIFIED,
    MC_WORLD_REGISTRY_FAILURE_NATIVE,
    MC_WORLD_REGISTRY_FAILURE_SOURCE_EXCEPTION,
    MC_WORLD_REGISTRY_FAILURE_SOURCE_ERROR
} mc_world_registry_failure_kind;

/* Passive classification of this exact reached cause while its actual defining
 * Registry is alive. No Registry/producer occurrence or object pointer is held.
 * exception_class is a static classifier literal, not a producer-owned name.
 * cause has the same borrowed-label lifetime as the paired mc_registry_error;
 * matching compares field identities/scalars without dereferencing its labels.
 * Copy this view together with that cause. A different/overwritten cause must
 * not inherit the old classification. Zero is an explicitly unclassified legacy
 * projection, distinct from a positively classified native refusal. */
typedef struct mc_world_registry_failure {
    mc_world_registry_failure_kind kind;
    const char *exception_class;
    mc_registry_error cause;
} mc_world_registry_failure;

bool mc_world_registry_failure_matches(const mc_world_registry_failure *,
                                       const mc_registry_error *);

#endif
