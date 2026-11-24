#include "execution_internal.h"

#include "mc/entity/entity.h"
#include <stdlib.h>

static bool pending(const mc_world_runtime_execution_result *r) {
    if (r->root_pending)
        return true;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (r->pending[i].used)
            return true;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (r->closes[i].used)
            return true;
    return false;
}

static bool same(const mc_entity_tick_execution_world_token *a,
                 const mc_entity_tick_execution_world_token *b) {
    return a->owner == b->owner && a->slot == b->slot && a->generation == b->generation;
}

bool mc_world_mutation_drop_result_quiet(const mc_world_runtime_execution_result *r) {
    return r != NULL && r->ownership_status == MC_WORLD_STORAGE_OK && !r->selection_ready &&
           !r->math_failed && !pending(r);
}

static bool current(const mc_world_mutation_drop_execution *call,
                    const mc_entity_tick_execution_world_token *token) {
    mc_entity_tick_execution_world_token actual;
    return mc_entity_tick_execution_current_world(call->context.execution, &actual) &&
           same(&actual, token);
}

static bool phase(const mc_world_mutation_drop_execution *call,
                  const mc_entity_drop_world_execution_view *scope,
                  mc_entity_tick_execution_world_slot_phase *out) {
    const mc_entity_tick_execution *execution = call->context.execution;
    const mc_entity_tick_execution_world_token *token = &scope->world;
    if (!scope->created || token->owner != execution || token->generation == 0 ||
        token->slot >= MC_ENTITY_TICK_EXECUTION_WORLDS ||
        execution->world_slots[token->slot].generation != token->generation)
        return false;
    *out = execution->world_slots[token->slot].phase;
    return true;
}

static mc_world_storage_status note(mc_world_mutation_drop_execution *call,
                                    mc_world_storage_status status, const char *operation,
                                    mc_entity_tick_execution_site site) {
    mc_world_runtime_execution_result_note(call->context.result, status, operation, site);
    return status;
}

static void observe_scope(mc_world_mutation_drop_execution *call,
                          mc_entity_drop_world_execution_view *scope) {
    mc_entity_tick_execution_world_slot_phase value;
    if (phase(call, scope, &value)) {
        scope->open = value == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN;
        scope->consumed = value == MC_ENTITY_TICK_EXECUTION_WORLD_FREE ||
                          value == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED;
    }
}

bool mc_world_mutation_drop_capture(mc_world_mutation_drop_execution *call, mc_world_runtime *world,
                                    mc_entity_tick_execution_site site,
                                    mc_entity_drop_world_execution_view *scope) {
    mc_world_runtime_execution_result *r = call->context.result;
    if (!current(call, &call->context.world) || r->selection_ready || r->math_failed ||
        pending(r) || r->ownership_status != MC_WORLD_STORAGE_OK ||
        (scope->created && !scope->consumed)) {
        (void)note(call, MC_WORLD_STORAGE_BUSY, "Block drop prior canonical work or World", site);
        return false;
    }
    mc_entity_tick_execution_world_token token;
    const mc_world_storage_status status =
        mc_entity_tick_execution_push_world(call->context.execution, world, site, &token);
    if (note(call, status, "Block drop actual World capture", site) != MC_WORLD_STORAGE_OK)
        return false;
    *scope = (mc_entity_drop_world_execution_view){.world = token, .created = true, .open = true};
    return true;
}

bool mc_world_mutation_drop_close(mc_world_mutation_drop_execution *call,
                                  mc_entity_drop_world_execution_view *scope,
                                  mc_entity_tick_execution_site site, bool completed) {
    mc_world_runtime_execution_result *r = call->context.result;
    if (completed && (r->ownership_status != MC_WORLD_STORAGE_OK || r->selection_ready ||
                      r->math_failed || pending(r))) {
        (void)note(call, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "Block drop completed child result not quiet", site);
        completed = false;
    }
    scope->locals_drained = true;
    scope->close_attempted = true;
    scope->failed = !completed;
    scope->initial_close_status =
        mc_entity_tick_execution_pop_world(call->context.execution, &scope->world, scope->failed);
    observe_scope(call, scope);
    (void)note(call, scope->initial_close_status, "Block drop checked World close", site);
    return completed && scope->initial_close_status == MC_WORLD_STORAGE_OK;
}

bool mc_world_mutation_drop_owns_world(const mc_world_mutation_drop_execution *call,
                                       const mc_entity_tick_execution_world_token *world) {
    if (call == NULL || world == NULL || call->active)
        return false;
    const mc_entity_drop_world_execution_view *scopes[] = {&call->publication_scope,
                                                           &call->constructor_scope};
    for (size_t i = 0; i < 2; ++i) {
        mc_entity_tick_execution_world_slot_phase value;
        const mc_entity_drop_world_execution_view *scope = scopes[i];
        if (!scope->consumed && same(world, &scope->world) && phase(call, scope, &value) &&
            value == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN)
            return true;
    }
    return false;
}

static mc_world_storage_status close_scope(mc_world_mutation_drop_execution *call,
                                           mc_entity_drop_world_execution_view *scope,
                                           mc_entity_tick_execution_site site) {
    if (!scope->created || scope->consumed)
        return MC_WORLD_STORAGE_OK;
    if (!scope->close_attempted || !scope->locals_drained)
        return note(call, MC_WORLD_STORAGE_BUSY, "Block drop Native close lacks recorded return",
                    site);
    mc_entity_tick_execution_world_slot_phase value;
    if (!phase(call, scope, &value))
        return note(call, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Block drop Native token identity",
                    site);
    if (value == MC_ENTITY_TICK_EXECUTION_WORLD_FREE ||
        value == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED) {
        scope->open = false;
        scope->consumed = true;
        return MC_WORLD_STORAGE_OK;
    }
    if (value != MC_ENTITY_TICK_EXECUTION_WORLD_OPEN || !current(call, &scope->world))
        return note(call, MC_WORLD_STORAGE_BUSY, "Block drop Native close awaits exact token",
                    site);
    scope->native_close_attempted = true;
    scope->native_close_status =
        mc_entity_tick_execution_pop_world(call->context.execution, &scope->world, scope->failed);
    observe_scope(call, scope);
    return note(call, scope->native_close_status, "Block drop recorded Native World close", site);
}

mc_world_storage_status
mc_world_mutation_drop_native_close(mc_world_mutation_drop_execution *call) {
    if (call == NULL)
        return MC_WORLD_STORAGE_OK;
    if (call->active || pending(call->context.result))
        return MC_WORLD_STORAGE_BUSY;
    mc_world_storage_status status =
        close_scope(call, &call->publication_scope, call->publication_site);
    if (status == MC_WORLD_STORAGE_OK)
        status = close_scope(call, &call->constructor_scope, call->constructor_site);
    return status;
}

mc_world_storage_status mc_world_mutation_drop_retire(mc_world_mutation_drop_execution **slot) {
    if (slot == NULL || *slot == NULL)
        return MC_WORLD_STORAGE_OK;
    mc_world_mutation_drop_execution *call = *slot;
    const mc_world_storage_status status = mc_world_mutation_drop_native_close(call);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    mc_entity_release(call->entity);
    mc_item_stack_release(call->stack);
    free(call);
    *slot = NULL;
    return MC_WORLD_STORAGE_OK;
}
