#ifndef MC_WORLD_RUNTIME_POPULATION_CONTEXT_H
#define MC_WORLD_RUNTIME_POPULATION_CONTEXT_H

#include "mc/block/registry/registry.h"
#include "mc/world/storage/status.h"

#include <stdbool.h>
#include <stddef.h>

/* Nonowning source locations, independent of feature and acquisition types.
 * Only a reached failure has valid sites. The projection retains the current
 * controller and deepest explicitly propagated controller, not every recursive
 * frame or discarded feature child. It imposes no gameplay recursion limit. */
typedef struct mc_world_population_site {
    const char *provider_class, *method;
    unsigned int bytecode, world_bytecode;
    const char *selector_class, *selector_method;
    unsigned int selector_bytecode;
    size_t iteration;
    bool native_boundary;
} mc_world_population_site;

typedef struct mc_world_population_context {
    bool reached;
    mc_world_population_site current, origin;
    mc_world_storage_error cause;
    mc_registry_error registry;
} mc_world_population_context;

#endif
