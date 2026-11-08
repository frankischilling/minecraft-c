#ifndef MC_WORLD_RUNTIME_EXECUTION_CHILDREN_INTERNAL_H
#define MC_WORLD_RUNTIME_EXECUTION_CHILDREN_INTERNAL_H

#include "../../entity/tick/execution_internal.h"
#include "mc/world/runtime/movement.h"
#include "runtime_internal.h"

/* Native preflight only. The parent already owns the exact evaluated nullable
 * World in a live scope. No source call/getter, new hold, real receipt release,
 * context retirement or Math use occurs here. A rejected node is NEVER passed
 * as NULL to select a legacy call. All storage is live/exclusive/disjoint. */
static inline mc_world_storage_status
mc_world_execution_child_lease(mc_entity_tick_execution *execution, mc_world_runtime *world,
                               mc_world_access_failure_lease **out) {
    const char *operation = NULL;
    mc_world_storage_status status;
    if (execution == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (execution->identity != execution) {
        status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
        operation = "World execution child owner identity";
    } else if (!execution->world_open || execution->world != world ||
               execution->world_held != (world != NULL) ||
               (world != NULL && world->persistent_bindings == 0)) {
        status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        operation = "World execution child evaluated receiver ownership";
    } else {
        mc_world_access_failure_lease *node = mc_entity_tick_execution_world_lease(execution);
        if (node == NULL)
            return execution->admission.status;
        const mc_world_access_failure *held = NULL;
        if (mc_world_access_failure_lease_inspect(node, &held)) {
            status = MC_WORLD_STORAGE_BUSY;
            operation = "World execution child already holds a failure lease";
        } else {
            /* HELD is rejected above. On IDLE this is an idempotent no-op;
             * ARMED/invalid refuses without consuming any actual occurrence. */
            mc_world_storage_error cause = {0};
            status = mc_world_access_failure_lease_release(node, &cause);
            if (status == MC_WORLD_STORAGE_OK) {
                *out = node;
                return status;
            }
            operation = cause.operation;
        }
    }
    execution->admission = (mc_entity_tick_execution_error){status, operation, execution->site};
    return status;
}

/* Actual entered bodies shared with the public NULL-context legacy routes.
 * The caller keeps arguments, code and source graph live/exclusive and supplies
 * fresh disjoint error storage (or NULL). Valid failure owns its exact producer
 * through execution even with NULL error. World invocation-NULL attribution
 * remains at the source caller; these C children preserve native World argument
 * admission and never invent a NULL-World WorldAccess receipt.
 * On return the PARENT closes the scope and later clears expired typed views
 * before checked execution retirement. These children do neither operation. */
mc_world_storage_status mc_world_runtime_entity_sound_in_execution(mc_entity_tick_execution *,
                                                                   mc_world_runtime *, mc_entity *,
                                                                   const mc_registry_string *,
                                                                   float, float,
                                                                   mc_world_movement_error *);
mc_world_storage_status mc_world_runtime_particle_in_execution(mc_entity_tick_execution *,
                                                               mc_world_runtime *,
                                                               const mc_registry_string *, double,
                                                               double, double, double, double,
                                                               double, mc_world_mutation_error *);

#endif
