#include "execution_internal.h"

#include "../../world/runtime/runtime_internal.h"

static mc_entity_tick_execution_error empty_error(void) {
    return (mc_entity_tick_execution_error){MC_WORLD_STORAGE_OK, NULL, {NULL, NULL, NULL, -1}};
}

static bool valid(const mc_entity_tick_execution *execution) {
    return execution != NULL && execution->identity == execution;
}

static mc_world_storage_status admission(mc_entity_tick_execution *execution,
                                         mc_world_storage_status status, const char *operation) {
    if (execution != NULL)
        execution->admission = (mc_entity_tick_execution_error){status, operation, execution->site};
    return status;
}

static mc_world_storage_status capture_failed(mc_entity_tick_execution *execution,
                                              mc_world_storage_status status, const char *operation,
                                              bool unprotected) {
    execution->capture = (mc_entity_tick_execution_error){status, operation, execution->site};
    if (execution->first.status == MC_WORLD_STORAGE_OK)
        execution->first = execution->capture;
    execution->capture_pending |= unprotected;
    execution->phase = MC_ENTITY_TICK_EXECUTION_FAILED;
    return status;
}

static mc_world_storage_status cleanup_failed(mc_entity_tick_execution *execution,
                                              mc_world_storage_status status,
                                              const char *operation) {
    execution->cleanup = (mc_entity_tick_execution_error){status, operation, execution->site};
    return status;
}

static mc_world_storage_status capture_admission(mc_entity_tick_execution *execution) {
    if (!valid(execution))
        return admission(execution, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                         "Entity execution capture owner identity");
    if (!execution->world_open || (execution->phase != MC_ENTITY_TICK_EXECUTION_OPEN &&
                                   execution->phase != MC_ENTITY_TICK_EXECUTION_FAILED))
        return admission(execution, MC_WORLD_STORAGE_BUSY,
                         "Entity execution capture outside current World call");
    execution->admission = empty_error();
    return MC_WORLD_STORAGE_OK;
}

void mc_entity_tick_execution_init(mc_entity_tick_execution *execution, mc_math_execution *math) {
    if (execution == NULL)
        return;
    execution->identity = execution;
    execution->math = math;
    execution->phase = MC_ENTITY_TICK_EXECUTION_OPEN;
    execution->world = NULL;
    execution->world_held = execution->world_open = execution->child_failed =
        execution->capture_pending = false;
    execution->loader_count = execution->receipt_count = execution->lease_count = 0;
    execution->current_lease = SIZE_MAX;
    execution->site = (mc_entity_tick_execution_site){NULL, NULL, NULL, -1};
    execution->first = execution->capture = execution->admission = execution->cleanup =
        empty_error();
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        execution->loader_originals[i] = NULL;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_RECEIPTS; ++i) {
        execution->receipts[i] = (mc_world_access_failure){0};
        mc_world_access_failure_lease_init(&execution->leases[i]);
    }
}

mc_world_storage_status mc_entity_tick_execution_begin_world(mc_entity_tick_execution *execution,
                                                             mc_world_runtime *world,
                                                             mc_entity_tick_execution_site site) {
    if (!valid(execution))
        return admission(execution, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                         "Entity execution World owner identity");
    if (execution->phase != MC_ENTITY_TICK_EXECUTION_OPEN || execution->world_open ||
        execution->world_held || execution->world != NULL || execution->current_lease != SIZE_MAX)
        return admission(execution, MC_WORLD_STORAGE_BUSY,
                         "Entity execution World call is not open");
    execution->admission = empty_error();
    execution->site = site;
    if (execution->lease_count >= MC_ENTITY_TICK_EXECUTION_RECEIPTS)
        return capture_failed(execution, MC_WORLD_STORAGE_LIMIT,
                              "Entity execution precharged lease capacity", false);
    if (world != NULL && !mc_world_runtime_binding_retain(world)) {
        const mc_world_storage_error *cause = mc_world_runtime_last_error(world);
        return capture_failed(execution, cause->status, cause->operation, false);
    }
    execution->world = world;
    execution->world_held = world != NULL;
    execution->world_open = true;
    execution->current_lease = execution->lease_count++;
    return MC_WORLD_STORAGE_OK;
}

mc_world_access_failure_lease *
mc_entity_tick_execution_world_lease(mc_entity_tick_execution *execution) {
    if (!valid(execution)) {
        (void)admission(execution, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                        "Entity execution World lease owner identity");
        return NULL;
    }
    if (execution->phase != MC_ENTITY_TICK_EXECUTION_OPEN || !execution->world_open ||
        execution->current_lease >= execution->lease_count ||
        execution->lease_count > MC_ENTITY_TICK_EXECUTION_RECEIPTS) {
        (void)admission(execution, MC_WORLD_STORAGE_BUSY,
                        "Entity execution World lease outside current call");
        return NULL;
    }
    execution->admission = empty_error();
    return &execution->leases[execution->current_lease];
}

mc_world_storage_status
mc_entity_tick_execution_copy_loader(mc_entity_tick_execution *execution,
                                     const mc_world_source_chunk_error **selected_slot) {
    const mc_world_storage_status admitted = capture_admission(execution);
    if (admitted != MC_WORLD_STORAGE_OK)
        return admitted;
    if (selected_slot == NULL)
        return capture_failed(execution, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                              "Entity execution loader selection slot", true);
    const mc_world_source_chunk_error *const borrowed = *selected_slot;
    if (borrowed == NULL)
        return MC_WORLD_STORAGE_OK;
    if (!execution->world_held || execution->world == NULL)
        return capture_failed(execution, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution loader producer is not held", true);
    if (execution->loader_count > MC_ENTITY_TICK_EXECUTION_LOADERS)
        return capture_failed(execution, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution loader ownership extent", true);
    for (size_t i = 0; i < execution->loader_count; ++i) {
        if (borrowed == &execution->loaders[i] || borrowed == execution->loader_originals[i]) {
            *selected_slot = &execution->loaders[i];
            return MC_WORLD_STORAGE_OK;
        }
    }
    if (execution->loader_count == MC_ENTITY_TICK_EXECUTION_LOADERS)
        return capture_failed(execution, MC_WORLD_STORAGE_LIMIT,
                              "Entity execution precharged loader capacity", true);
    const size_t index = execution->loader_count;
    execution->loaders[index] = *borrowed;
    execution->loader_originals[index] = borrowed;
    ++execution->loader_count;
    *selected_slot = &execution->loaders[index];
    return MC_WORLD_STORAGE_OK;
}

static bool same_failure(const mc_world_access_failure *a, const mc_world_access_failure *b) {
    return a->receiver == b->receiver && a->render_global == b->render_global &&
           a->receipt == b->receipt && a->source_null_receiver == b->source_null_receiver;
}

static const mc_world_access_failure *protected_failure(const mc_entity_tick_execution *execution,
                                                        const mc_world_access_failure *borrowed) {
    for (size_t i = 0; i < execution->lease_count; ++i) {
        const mc_world_access_failure *candidate = NULL;
        if (mc_world_access_failure_lease_inspect(&execution->leases[i], &candidate) &&
            same_failure(candidate, borrowed))
            return candidate;
    }
    return NULL;
}

mc_world_storage_status
mc_entity_tick_execution_capture_receipt(mc_entity_tick_execution *execution,
                                         const mc_world_access_failure *borrowed) {
    const mc_world_storage_status admitted = capture_admission(execution);
    if (admitted != MC_WORLD_STORAGE_OK)
        return admitted;
    if (borrowed == NULL || (borrowed->receiver == NULL && borrowed->render_global == NULL &&
                             borrowed->receipt == NULL))
        return MC_WORLD_STORAGE_OK; /* Empty native failure or value-only source NULL. */
    if (borrowed->source_null_receiver || borrowed->receipt == NULL ||
        execution->lease_count > MC_ENTITY_TICK_EXECUTION_RECEIPTS ||
        execution->receipt_count > MC_ENTITY_TICK_EXECUTION_RECEIPTS)
        return capture_failed(execution, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                              "Entity execution concrete receipt shape", true);

    const mc_world_access_failure *protected = protected_failure(execution, borrowed);
    if (protected == NULL)
        return capture_failed(execution, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution receipt lacks protected handoff", true);
    for (size_t i = 0; i < execution->receipt_count; ++i)
        if (same_failure(&execution->receipts[i], protected))
            return MC_WORLD_STORAGE_OK;
    if (execution->receipt_count == MC_ENTITY_TICK_EXECUTION_RECEIPTS)
        return capture_failed(execution, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution selected receipt occurrence invariant", false);
    execution->receipts[execution->receipt_count] = *protected;
    ++execution->receipt_count;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status release_world(mc_entity_tick_execution *execution) {
    if (!execution->world_held) {
        if (execution->world != NULL)
            return cleanup_failed(execution, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "Entity execution unowned World slot");
        return MC_WORLD_STORAGE_OK;
    }
    if (execution->world == NULL || execution->world->persistent_bindings == 0)
        return cleanup_failed(execution, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution World binding ownership");
    mc_world_runtime_binding_release(execution->world);
    execution->world_held = false;
    execution->world = NULL;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_entity_tick_execution_end_world(mc_entity_tick_execution *execution,
                                                           bool child_failed) {
    if (!valid(execution))
        return admission(execution, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                         "Entity execution World end owner identity");
    if (!execution->world_open || (execution->phase != MC_ENTITY_TICK_EXECUTION_OPEN &&
                                   execution->phase != MC_ENTITY_TICK_EXECUTION_FAILED))
        return admission(execution, MC_WORLD_STORAGE_BUSY,
                         "Entity execution World end outside current call");
    execution->admission = execution->cleanup = empty_error();
    if (execution->current_lease >= execution->lease_count ||
        execution->lease_count > MC_ENTITY_TICK_EXECUTION_RECEIPTS) {
        return cleanup_failed(execution, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution current lease ownership extent");
    }
    const bool failed = child_failed || execution->phase == MC_ENTITY_TICK_EXECUTION_FAILED ||
                        execution->capture_pending;
    mc_world_access_failure_lease *const lease = &execution->leases[execution->current_lease];
    const mc_world_access_failure *held = NULL;
    const bool lease_held = mc_world_access_failure_lease_inspect(lease, &held);
    /* Validate an unused node before closing its source scope. In particular,
     * ARMED callback reentry must refuse without destroying the outer call's
     * exact current slot, World right, phase or first failure. */
    if (!lease_held) {
        mc_world_storage_error cause = {0};
        const mc_world_storage_status status = mc_world_access_failure_lease_release(lease, &cause);
        if (status != MC_WORLD_STORAGE_OK)
            return cleanup_failed(execution, status, cause.operation);
    }
    execution->world_open = false;
    execution->current_lease = SIZE_MAX;
    execution->child_failed |= failed;
    if (failed)
        execution->phase = MC_ENTITY_TICK_EXECUTION_FAILED;
    if (!lease_held)
        --execution->lease_count;
    else if (!failed) {
        execution->phase = MC_ENTITY_TICK_EXECUTION_FAILED;
        execution->child_failed = true;
        return cleanup_failed(execution, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution returned child retained a failure lease");
    }
    if (failed)
        return MC_WORLD_STORAGE_OK;
    const mc_world_storage_status status = release_world(execution);
    if (status != MC_WORLD_STORAGE_OK)
        execution->phase = MC_ENTITY_TICK_EXECUTION_FAILED;
    return status;
}

mc_world_storage_status
mc_entity_tick_execution_release_receipts(mc_entity_tick_execution *execution) {
    if (execution == NULL)
        return MC_WORLD_STORAGE_OK;
    if (!valid(execution))
        return admission(execution, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                         "Entity execution receipt cleanup owner identity");
    if (execution->phase == MC_ENTITY_TICK_EXECUTION_RETIRED)
        return MC_WORLD_STORAGE_OK;
    execution->cleanup = empty_error();
    if (execution->world_open)
        return cleanup_failed(execution, MC_WORLD_STORAGE_BUSY,
                              "Entity execution World call is still entered");
    if (execution->capture_pending)
        return cleanup_failed(execution, MC_WORLD_STORAGE_BUSY,
                              "Entity execution unprotected capture integration is open");
    if (execution->receipt_count > MC_ENTITY_TICK_EXECUTION_RECEIPTS ||
        execution->lease_count > MC_ENTITY_TICK_EXECUTION_RECEIPTS ||
        execution->loader_count > MC_ENTITY_TICK_EXECUTION_LOADERS ||
        execution->current_lease != SIZE_MAX)
        return cleanup_failed(execution, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Entity execution receipt ownership extent");
    execution->phase = MC_ENTITY_TICK_EXECUTION_RETIRING;
    for (size_t i = 0; i < execution->receipt_count; ++i)
        if (protected_failure(execution, &execution->receipts[i]) == NULL)
            return cleanup_failed(execution, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "Entity execution selected receipt lease ownership");
    while (execution->receipt_count != 0)
        execution->receipts[--execution->receipt_count] = (mc_world_access_failure){0};
    while (execution->lease_count != 0) {
        mc_world_storage_error cause = {0};
        const mc_world_storage_status status = mc_world_access_failure_lease_release(
            &execution->leases[execution->lease_count - 1], &cause);
        if (status != MC_WORLD_STORAGE_OK)
            return cleanup_failed(execution, status, cause.operation);
        --execution->lease_count;
    }
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_entity_tick_execution_retire(mc_entity_tick_execution *execution) {
    const mc_world_storage_status status = mc_entity_tick_execution_release_receipts(execution);
    if (status != MC_WORLD_STORAGE_OK || execution == NULL ||
        execution->phase == MC_ENTITY_TICK_EXECUTION_RETIRED)
        return status;
    const mc_world_storage_status released = release_world(execution);
    if (released != MC_WORLD_STORAGE_OK)
        return released;
    for (size_t i = 0; i < execution->loader_count && i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        execution->loader_originals[i] = NULL;
    execution->loader_count = 0;
    execution->math = NULL; /* Unused/expired borrowed Math is never dereferenced. */
    execution->phase = MC_ENTITY_TICK_EXECUTION_RETIRED;
    return MC_WORLD_STORAGE_OK;
}
