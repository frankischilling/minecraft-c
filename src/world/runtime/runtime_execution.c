#include "runtime_execution_internal.h"

static bool same_world(const mc_entity_tick_execution_world_token *a,
                       const mc_entity_tick_execution_world_token *b) {
    return a->owner == b->owner && a->slot == b->slot && a->generation == b->generation;
}

static bool same_attempt(const mc_entity_tick_execution_loader_attempt *a,
                         const mc_entity_tick_execution_loader_attempt *b) {
    return same_world(&a->world, &b->world) && a->slot == b->slot && a->generation == b->generation;
}

static bool same_selection(const mc_entity_tick_execution_loader_selection *a,
                           const mc_entity_tick_execution_loader_selection *b) {
    return same_world(&a->world, &b->world) && a->generation == b->generation &&
           a->status == b->status;
}

static bool same_pending(const mc_world_runtime_execution_pending *a,
                         const mc_world_runtime_execution_pending *b) {
    return a->outcome_valid == b->outcome_valid && same_attempt(&a->attempt, &b->attempt) &&
           a->actual_status == b->actual_status && a->direct_failure == b->direct_failure &&
           a->child_ready == b->child_ready && a->child_pending == b->child_pending &&
           (!a->child_ready || same_selection(&a->child, &b->child)) &&
           (!a->child_pending || same_attempt(&a->child_attempt, &b->child_attempt));
}

void mc_world_runtime_execution_result_init(mc_world_runtime_execution_result *result) {
    if (result != NULL)
        *result = (mc_world_runtime_execution_result){0};
}

void mc_world_runtime_execution_result_note(mc_world_runtime_execution_result *result,
                                            mc_world_storage_status status, const char *operation,
                                            mc_entity_tick_execution_site site) {
    if (result != NULL && status != MC_WORLD_STORAGE_OK &&
        result->ownership_status == MC_WORLD_STORAGE_OK) {
        result->ownership_status = status;
        result->ownership_error = (mc_entity_tick_execution_error){status, operation, site};
    }
}

mc_world_storage_status
mc_world_runtime_execution_result_forward(mc_world_runtime_execution_result *parent,
                                          const mc_world_runtime_execution_result *child) {
    if (parent == NULL || child == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (parent == child)
        return MC_WORLD_STORAGE_OK;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (child->pending[i].used &&
            (child->pending[i].attempt.slot != i ||
             (parent->pending[i].used && !same_pending(&parent->pending[i], &child->pending[i]))))
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    /* Validate all slots before copying any ordinary VALUE continuation. */
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (child->pending[i].used)
            parent->pending[i] = child->pending[i];
    if (child->ownership_status != MC_WORLD_STORAGE_OK &&
        parent->ownership_status == MC_WORLD_STORAGE_OK) {
        parent->ownership_status = child->ownership_status;
        parent->ownership_error = child->ownership_error;
    }
    parent->selection_ready = child->selection_ready;
    parent->selection = child->selection;
    parent->root_pending = child->root_pending;
    parent->root_attempt = child->root_attempt;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status refused(mc_entity_tick_execution *execution,
                                       mc_world_runtime_execution_result *result,
                                       mc_world_storage_status status, const char *operation) {
    const mc_entity_tick_execution_site site =
        execution == NULL ? (mc_entity_tick_execution_site){NULL, NULL, NULL, -1} : execution->site;
    if (execution != NULL)
        execution->admission = (mc_entity_tick_execution_error){status, operation, site};
    mc_world_runtime_execution_result_note(result, status, operation, site);
    return status;
}

mc_world_storage_status
mc_world_runtime_execution_finish_pending(mc_entity_tick_execution *execution,
                                          mc_world_runtime_execution_result *result) {
    if (execution == NULL || result == NULL || execution->identity != execution)
        return refused(execution, result, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                       "Acquisition continuation owner identity");
    mc_entity_tick_execution_world_token current;
    if (!mc_entity_tick_execution_current_world(execution, &current))
        return refused(execution, result,
                       execution->world_open ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                                             : MC_WORLD_STORAGE_BUSY,
                       "Acquisition continuation requires exact current World");
    size_t index = MC_ENTITY_TICK_EXECUTION_LOADERS;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i) {
        const mc_world_runtime_execution_pending *p = &result->pending[i];
        if (!p->used || !same_world(&p->attempt.world, &current))
            continue;
        if (index != MC_ENTITY_TICK_EXECUTION_LOADERS || p->attempt.slot != i)
            return refused(execution, result, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Acquisition continuation exact reserved slot");
        index = i;
    }
    if (index == MC_ENTITY_TICK_EXECUTION_LOADERS)
        return refused(execution, result, MC_WORLD_STORAGE_BUSY,
                       "Acquisition continuation World has no returned attempt");
    mc_world_runtime_execution_pending *p = &result->pending[index];
    if (!p->outcome_valid || (p->child_ready && p->child_pending))
        return refused(execution, result, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Acquisition continuation actual outcome");
    if (p->child_pending)
        return refused(execution, result, MC_WORLD_STORAGE_BUSY,
                       "Acquisition continuation actual child still pending");
    if (p->child_ready) {
        mc_entity_tick_execution_loader_view view;
        if (p->actual_status == MC_CHUNK_ACQUIRE_OK ||
            !mc_entity_tick_execution_loader_read(execution, &p->child, &view) ||
            view.status == MC_CHUNK_ACQUIRE_OK)
            return refused(execution, result, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Acquisition continuation explicit failed child selection");
    }
    mc_entity_tick_execution_loader_selection direct;
    const mc_world_storage_status status =
        mc_entity_tick_execution_loader_finish(execution, &p->attempt, p->actual_status,
                                               p->child_ready ? NULL : p->direct_failure, &direct);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_execution_result_note(
            result, status, "Acquisition checked native completion", execution->site);
        return status;
    }
    const mc_entity_tick_execution_loader_selection selected = p->child_ready ? p->child : direct;
    const mc_entity_tick_execution_loader_attempt finished = p->attempt;
    *p = (mc_world_runtime_execution_pending){0};
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i) {
        mc_world_runtime_execution_pending *parent = &result->pending[i];
        if (parent->used && parent->child_pending &&
            same_attempt(&parent->child_attempt, &finished)) {
            parent->child_pending = false;
            /* A completed successful get can precede a native outer refusal;
             * its NONE/OK token must never become a failed child selection.
             * The parent direct view was deliberately NULL for this explicit
             * child relation, so it now finishes with its own actual NONE. */
            parent->child_ready = selected.status != MC_CHUNK_ACQUIRE_OK;
            parent->child =
                parent->child_ready ? selected : (mc_entity_tick_execution_loader_selection){0};
        }
    }
    if (result->root_pending && same_attempt(&result->root_attempt, &finished)) {
        result->root_pending = false;
        result->selection_ready = true;
        result->selection = selected;
    }
    return MC_WORLD_STORAGE_OK;
}
