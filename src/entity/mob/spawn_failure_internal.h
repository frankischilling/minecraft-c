#ifndef MC_ENTITY_MOB_SPAWN_FAILURE_INTERNAL_H
#define MC_ENTITY_MOB_SPAWN_FAILURE_INTERNAL_H

#include "mc/entity/mob/spawning.h"

/* Passive projection of the selected failing invocation. Successful or
 * unrelated descendants are excluded even if their native record remains in
 * storage. These are borrowed mutable diagnostic cells, never owning handles. */
typedef struct mc_mob_spawn_failure_view {
    mc_world_storage_error *cause;
    mc_registry_error *registry;
    mc_world_query_error *query;
    mc_world_nearby_error *nearby;
    mc_world_runtime_acquisition_error *acquisition;
} mc_mob_spawn_failure_view;

mc_mob_spawn_failure_view mc_mob_spawn_error_project(mc_mob_spawn_error *);

#endif
