#ifndef MC_WORLD_RUNTIME_SCHEDULING_H
#define MC_WORLD_RUNTIME_SCHEDULING_H

#include "mc/world/runtime/mutations.h"

typedef struct mc_world_schedule_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode, presence_bytecode, provider_bytecode;
    mc_world_storage_error cause;
    mc_scheduled_tick_status record;
    mc_scheduled_membership_status membership;
    mc_scheduled_ordered_status ordered;
    mc_registry_error registry;
    mc_world_mutation_error tick;
} mc_world_schedule_error;

typedef struct mc_world_schedule_class_state {
    bool initialized;
    int64_t next;
} mc_world_schedule_class_state;

/* Native diagnostic of the separate qy/ki class lifetime held above Worlds.
 * No active use, allocation, counter change or Random operation occurs here.
 * Keep the same class owner for every World in that application class lifetime;
 * another World constructor never resets this lazily initialized sequence. */
mc_world_storage_status mc_world_runtime_schedule_class_get(const mc_world_runtime_classes *classes,
                                                            mc_world_schedule_class_state *out);

/* Actual fd/dj.c(IIIII)V. There is no ordinary boolean result. Construct one
 * scheduled record before testing the live immediate flag or nonloading radius8
 * presence; even duplicates and ordinary no-ops consume its shared sequence.
 * Deferred insertion computes current Info time only for positive block IDs,
 * then invokes F.contains, F.add and E.add separately on the same record.
 * Immediate mode instead reads the actual current block and invokes its tick
 * with current World.r. Delay, queues and Info time are not read on that path.
 *
 * Require exclusive live GAME owners and disjoint error storage. Hold the World
 * and local record across synchronous calls. Reached failure preserves every
 * sequence, acquisition, Registry, queue, RNG and descendant effect. In
 * particular, failed E insertion leaves successful F insertion intact.
 * Native admission does not overwrite earlier World diagnostics. Missing
 * concrete tick bodies fail explicitly at their reached entry.
 *
 * Existing F/E entry and allocation budgets remain separate. Record allocation
 * uses checked native allocation, including discarded/transient records; the
 * constructor's direct-byte count is not a combined dynamic scheduler limit.
 * No queue drain, later tick, global work limit or implicit save is added.
 * error may be NULL. */
mc_world_storage_status mc_world_runtime_schedule(mc_world_runtime *world, int32_t x, int32_t y,
                                                  int32_t z, int32_t block, int32_t delay,
                                                  mc_world_schedule_error *error);

#endif
