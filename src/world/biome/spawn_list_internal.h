#ifndef MC_WORLD_BIOME_SPAWN_LIST_INTERNAL_H
#define MC_WORLD_BIOME_SPAWN_LIST_INTERNAL_H

#include "../../block/registry/registry_internal.h"

struct mc_biome_spawn_list {
    mc_registry_list data;
};

struct mc_biome_spawn {
    mc_biome_spawn_fields fields;
};

#endif
