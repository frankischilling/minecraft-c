#ifndef MC_WORLD_BIOME_REGISTRY_INTERNAL_H
#define MC_WORLD_BIOME_REGISTRY_INTERNAL_H

#include "mc/world/biome/registry.h"

struct mc_biome_entity_class {
    const char *client_name;
    const char *server_name;
};

typedef struct mc_registry_biome_storage {
    mc_biome *named[13];
    const mc_biome **x;
    size_t x_length;
    /* Native provenance only; defined/linked by the concrete sampler. */
    struct mc_biome_reference_array *sample_arrays;
    /* Stable logical loaded-Class handles; names are resolved only at ldc.
     * No target Entity class initialization or allocation is performed. */
    mc_biome_entity_class entity_classes[13];
} mc_registry_biome_storage;

mc_registry_status mc_registry_biomes_active_use(mc_registry_classes *classes);
mc_registry_status mc_registry_biome_static_load(mc_registry_classes *classes, char field,
                                                 const mc_biome **out);
mc_registry_status mc_registry_biome_lookup(mc_registry_classes *classes, double temperature,
                                            double rainfall, const mc_biome **out);
/* Private controlled input to the actual mutable p/q fields. Requires a real
 * Biome belonging to this admitted owner; performs no class active use. */
mc_registry_status mc_registry_biome_byte_store(mc_registry_classes *classes, mc_biome *biome,
                                                char field, int8_t value);

/* Private ordinary construction on an admitted real class owner. Allocate
 * initializes the actual declaring class before RAW allocation. A separate
 * instance failure never poisons an already READY declaring class. Native
 * invalid inputs preserve out; every successful allocation remains arena-held.
 * construct requires that same owner's once-only RAW object. */
mc_registry_status mc_registry_biome_allocate(mc_registry_classes *classes, mc_biome_kind kind,
                                              mc_biome **out);
mc_registry_status mc_registry_biome_construct(mc_biome *biome);

#endif
