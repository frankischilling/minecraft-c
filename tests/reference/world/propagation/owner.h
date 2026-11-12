#ifndef MC_REFERENCE_PROPAGATION_OWNER_H
#define MC_REFERENCE_PROPAGATION_OWNER_H

#include "mc/resources/files/files.h"
#include "mc/world/runtime/entities.h"

typedef struct propagation_reference_owner {
    mc_resource_files *files;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *registry;
    mc_entity_classes *entities;
    mc_world_runtime_classes *classes;
    mc_world_regions *regions;
    mc_world_store *store;
    mc_world_info_name *name;
    mc_world_runtime *world;
    mc_math_trig *trig;
} propagation_reference_owner;

/* Authored owner fixture over production constructors/storage. No generation,
 * population, scheduling, lighting or replacement-provider implementation. */
bool propagation_reference_owner_create(propagation_reference_owner *owner, const char *resources,
                                        const char *directory);
bool propagation_reference_owner_retire(propagation_reference_owner *owner);

#endif
