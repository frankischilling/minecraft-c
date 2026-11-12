#ifndef MC_WORLD_RUNTIME_SCHEDULING_CONTEXT_H
#define MC_WORLD_RUNTIME_SCHEDULING_CONTEXT_H

#include "mc/block/registry/registry.h"
#include "mc/world/schedule/membership.h"
#include "mc/world/schedule/ordered.h"
#include "mc/world/storage/status.h"

/* Passive projection of one actually reached scheduler call. The full public
 * scheduler error owns its concrete tick child; this projection cannot contain
 * that child recursively. It retains scheduler and child presence/provider
 * sites independently without adding a gameplay recursion or work limit. */
typedef struct mc_world_schedule_prefix {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode, presence_bytecode, provider_bytecode;
    mc_world_storage_error cause;
    mc_scheduled_tick_status record;
    mc_scheduled_membership_status membership;
    mc_scheduled_ordered_status ordered;
    mc_registry_error registry;
} mc_world_schedule_prefix;

#endif
