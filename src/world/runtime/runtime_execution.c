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

static bool same_site(const mc_entity_tick_execution_site *a,
                      const mc_entity_tick_execution_site *b) {
    return a->class_name == b->class_name && a->method == b->method &&
           a->descriptor == b->descriptor && a->pc == b->pc;
}

static bool world_value_valid(const mc_entity_tick_execution_world_token *world) {
    return world->owner != NULL && world->slot < MC_ENTITY_TICK_EXECUTION_WORLDS &&
           world->generation != 0;
}

static bool close_value_valid(const mc_world_runtime_execution_close *close, size_t slot) {
    return close->used && world_value_valid(&close->world) && close->world.slot == slot &&
           (close->kind == MC_WORLD_RUNTIME_EXECUTION_CLOSE_CHUNK_TILE ||
            close->kind == MC_WORLD_RUNTIME_EXECUTION_CLOSE_CHUNK_ENTITY) &&
           close->phase == MC_WORLD_RUNTIME_EXECUTION_CLOSE_LOCALS_DRAINED &&
           close->refused_status != MC_WORLD_STORAGE_OK && close->operation != NULL &&
           close->child_failed == (close->source_status != MC_WORLD_STORAGE_OK);
}

static bool same_close(const mc_world_runtime_execution_close *a,
                       const mc_world_runtime_execution_close *b) {
    return a->used == b->used && a->child_failed == b->child_failed && a->kind == b->kind &&
           a->phase == b->phase && same_world(&a->world, &b->world) &&
           a->source_status == b->source_status && a->refused_status == b->refused_status &&
           a->operation == b->operation && same_site(&a->site, &b->site);
}

static bool result_owner(const mc_entity_tick_execution_world_token *world,
                         const mc_entity_tick_execution **owner) {
    if (!world_value_valid(world) || (*owner != NULL && *owner != world->owner))
        return false;
    *owner = world->owner;
    return true;
}

static bool math_return_valid(const mc_world_runtime_execution_result *result) {
    return !result->math_failed ||
           (result->math_owner != NULL && result->math_status != MC_MATH_CLASS_OK);
}

static bool same_math_site(const mc_math_execution_site *a, const mc_math_execution_site *b) {
    return a->class_name == b->class_name && a->method == b->method &&
           a->descriptor == b->descriptor && a->pc == b->pc;
}

static bool same_math_failure(const mc_math_execution_failure *a,
                              const mc_math_execution_failure *b) {
    return same_math_site(&a->caller, &b->caller) && same_math_site(&a->child, &b->child) &&
           a->cause.status == b->cause.status && a->cause.operation == b->cause.operation &&
           a->cause.source_pc == b->cause.source_pc &&
           a->cause.initial_cause == b->cause.initial_cause;
}

static bool same_math_return(const mc_world_runtime_execution_result *a,
                             const mc_world_runtime_execution_result *b) {
    return a->math_owner == b->math_owner && a->math_status == b->math_status &&
           same_math_failure(&a->math_failure, &b->math_failure);
}

/* Ordinary VALUE validation only. No owner or diagnostic pointer is followed. */
static bool continuations_valid(const mc_world_runtime_execution_result *result,
                                const mc_entity_tick_execution **owner) {
    if (!math_return_valid(result) || (result->selection_ready && result->root_pending))
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i) {
        const mc_world_runtime_execution_pending *p = &result->pending[i];
        if (!p->used)
            continue;
        if (p->attempt.slot != i || p->attempt.generation == 0 || !p->outcome_valid ||
            (p->child_ready && p->child_pending) || !result_owner(&p->attempt.world, owner) ||
            (p->child_ready && !result_owner(&p->child.world, owner)) ||
            (p->child_pending && !result_owner(&p->child_attempt.world, owner)))
            return false;
    }
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (result->closes[i].used && (!close_value_valid(&result->closes[i], i) ||
                                       !result_owner(&result->closes[i].world, owner)))
            return false;
    if (result->selection_ready && !result_owner(&result->selection.world, owner))
        return false;
    if (result->root_pending &&
        (!result_owner(&result->root_attempt.world, owner) ||
         result->root_attempt.slot >= MC_ENTITY_TICK_EXECUTION_LOADERS ||
         !result->pending[result->root_attempt.slot].used ||
         !same_attempt(&result->root_attempt, &result->pending[result->root_attempt.slot].attempt)))
        return false;
    return true;
}

/* Called ONLY immediately after the genuine checked pop below returned
 * non-OK. The exact token came from this activation's successful push. Inspect
 * only stable owner/slot/generation/phase VALUES: no World/lease/producer is
 * dereferenced, even when a native binding counter makes ledger reads refuse.
 * A consumed non-OK pop is RETAINED/freed and must never authorize a retry. */
static bool created_scope_still_open(const mc_entity_tick_execution *execution,
                                     const mc_entity_tick_execution_world_token *token) {
    return execution != NULL && execution->identity == execution && token != NULL &&
           token->owner == execution && token->slot < MC_ENTITY_TICK_EXECUTION_WORLDS &&
           token->generation != 0 &&
           execution->world_slots[token->slot].generation == token->generation &&
           execution->world_slots[token->slot].phase == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN;
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

mc_world_storage_status mc_world_runtime_execution_result_note_math(
    mc_world_runtime_execution_result *result, const mc_math_execution *owner,
    mc_math_class_status status, const mc_math_execution_failure *failure) {
    if (result == NULL || owner == NULL || status == MC_MATH_CLASS_OK || failure == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (!math_return_valid(result))
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (result->math_failed) {
        return result->math_owner == owner && result->math_status == status &&
                       same_math_failure(&result->math_failure, failure)
                   ? MC_WORLD_STORAGE_OK
                   : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
    result->math_owner = owner;
    result->math_status = status;
    result->math_failure = *failure;
    result->math_failed = true;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status
mc_world_runtime_execution_result_forward(mc_world_runtime_execution_result *parent,
                                          const mc_world_runtime_execution_result *child) {
    if (parent == NULL || child == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    const mc_entity_tick_execution *owner = NULL;
    if (!continuations_valid(parent, &owner) || !continuations_valid(child, &owner))
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (parent == child)
        return MC_WORLD_STORAGE_OK;
    if (parent->math_failed && child->math_failed && !same_math_return(parent, child))
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (child->pending[i].used && parent->pending[i].used &&
            !same_pending(&parent->pending[i], &child->pending[i]))
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (child->closes[i].used && parent->closes[i].used &&
            !same_close(&parent->closes[i], &child->closes[i]))
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if ((parent->selection_ready && child->root_pending) ||
        (parent->root_pending && child->selection_ready) ||
        (parent->selection_ready && child->selection_ready &&
         !same_selection(&parent->selection, &child->selection)) ||
        (parent->root_pending && child->root_pending &&
         !same_attempt(&parent->root_attempt, &child->root_attempt)))
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    /* BOTH ledgers, actual Math proofs and selectors precede ANY mutation. */
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (child->pending[i].used)
            parent->pending[i] = child->pending[i];
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (child->closes[i].used)
            parent->closes[i] = child->closes[i];
    if (child->ownership_status != MC_WORLD_STORAGE_OK &&
        parent->ownership_status == MC_WORLD_STORAGE_OK) {
        parent->ownership_status = child->ownership_status;
        parent->ownership_error = child->ownership_error;
    }
    if (child->selection_ready) {
        parent->selection_ready = true;
        parent->selection = child->selection;
    }
    if (child->root_pending) {
        parent->root_pending = true;
        parent->root_attempt = child->root_attempt;
    }
    if (child->math_failed) {
        parent->math_failed = true;
        parent->math_owner = child->math_owner;
        parent->math_status = child->math_status;
        parent->math_failure = child->math_failure;
    }
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

mc_world_storage_status mc_world_runtime_execution_close_scope(
    mc_entity_tick_execution *execution, mc_world_runtime_execution_result *result,
    mc_world_runtime_execution_close_kind kind, const mc_entity_tick_execution_world_token *token,
    mc_world_storage_status source_status, const char *operation,
    mc_entity_tick_execution_site site) {
    const bool child_failed = source_status != MC_WORLD_STORAGE_OK;
    const mc_world_storage_status status =
        mc_entity_tick_execution_pop_world(execution, token, child_failed);
    mc_world_runtime_execution_result_note(result, status, operation, site);
    if (status == MC_WORLD_STORAGE_OK || result == NULL ||
        !created_scope_still_open(execution, token))
        return status;
    const mc_world_runtime_execution_close close = {
        .used = true,
        .child_failed = child_failed,
        .kind = kind,
        .phase = MC_WORLD_RUNTIME_EXECUTION_CLOSE_LOCALS_DRAINED,
        .world = *token,
        .source_status = source_status,
        .refused_status = status,
        .operation = operation,
        .site = site};
    const mc_entity_tick_execution *owner = execution;
    if (!close_value_valid(&close, token->slot) || !continuations_valid(result, &owner) ||
        (result->closes[token->slot].used && !same_close(&result->closes[token->slot], &close)))
        return refused(execution, result, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Activation close continuation result contract");
    result->closes[token->slot] = close;
    return status;
}

mc_world_storage_status
mc_world_runtime_execution_retry_close(mc_entity_tick_execution *execution,
                                       mc_world_runtime_execution_result *result,
                                       const mc_world_runtime_execution_close *continuation) {
    if (execution == NULL || execution->identity != execution || result == NULL ||
        continuation == NULL)
        return refused(execution, result, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                       "Activation close continuation owner/result/value required");
    /* Supports a pointer into this result without reading it after the clear. */
    const mc_world_runtime_execution_close close = *continuation;
    const mc_entity_tick_execution *owner = execution;
    if (!world_value_valid(&close.world) || close.world.owner != execution ||
        !close_value_valid(&close, close.world.slot) || !continuations_valid(result, &owner) ||
        !result->closes[close.world.slot].used ||
        !same_close(&result->closes[close.world.slot], &close))
        return refused(execution, result, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Activation close continuation exact published drained value");
    mc_entity_tick_execution_world_token current;
    if (!mc_entity_tick_execution_current_world(execution, &current))
        return refused(execution, result,
                       execution->world_open ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                                             : MC_WORLD_STORAGE_BUSY,
                       "Activation close continuation checked current World");
    if (!same_world(&current, &close.world))
        return refused(execution, result, MC_WORLD_STORAGE_BUSY,
                       "Activation close continuation requires exact current token");
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (result->pending[i].used && same_world(&result->pending[i].attempt.world, &close.world))
            return refused(execution, result, MC_WORLD_STORAGE_BUSY,
                           "Activation close continuation actual attempt must finish first");
    const mc_world_storage_status status =
        mc_entity_tick_execution_pop_world(execution, &close.world, close.child_failed);
    mc_world_runtime_execution_result_note(result, status, close.operation, close.site);
    if (status == MC_WORLD_STORAGE_OK)
        result->closes[close.world.slot] = (mc_world_runtime_execution_close){0};
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
