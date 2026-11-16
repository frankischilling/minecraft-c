/* The actual lazy Chunk.d67 scope survives a failed child or checked pop.
 * Canonical acquisition records remain copyable VALUES in their caller. */
struct mc_live_tile_entity_query_execution_call {
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_result *owned;
    mc_entity_tick_execution_world_token world;
    mc_entity_tick_execution_site site;
    mc_world_mutation_execution_call *addition;
    mc_world_mutation_error *error;
    bool active, created, consumed, source_completed, addition_invoked;
    mc_world_storage_status initial_close, native_close;
};

static bool live_lazy_same(const mc_entity_tick_execution_world_token *a,
                           const mc_entity_tick_execution_world_token *b) {
    return a->owner == b->owner && a->slot == b->slot && a->generation == b->generation;
}

static bool live_lazy_quiet(const mc_world_runtime_execution_result *owned) {
    if (owned->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (owned->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (owned->closes[i].used)
            return false;
    return true;
}

static mc_world_storage_status live_lazy_pop(mc_live_tile_entity_query_execution_call *call) {
    if (!call->created || call->consumed)
        return MC_WORLD_STORAGE_OK;
    const mc_world_storage_status status =
        mc_entity_tick_execution_pop_world(call->execution, &call->world, !call->source_completed);
    /* Inspect only this actual pop's returned token VALUE poststate. A non-OK
     * close may already have consumed the scope while retaining its producer. */
    const mc_entity_tick_execution_world_token *token = &call->world;
    if (call->execution->identity == call->execution && token->owner == call->execution &&
        token->slot < MC_ENTITY_TICK_EXECUTION_WORLDS && token->generation != 0 &&
        call->execution->world_slots[token->slot].generation == token->generation) {
        const mc_entity_tick_execution_world_slot_phase phase =
            call->execution->world_slots[token->slot].phase;
        call->consumed = phase == MC_ENTITY_TICK_EXECUTION_WORLD_FREE ||
                         phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED;
    }
    if (status == MC_WORLD_STORAGE_OK && !call->consumed)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    return status;
}

bool mc_live_tile_entity_query_execution_owns_world(
    const mc_live_tile_entity_query_execution_call *call,
    const mc_entity_tick_execution_world_token *token) {
    return call != NULL && token != NULL &&
           ((call->created && !call->consumed && live_lazy_same(&call->world, token)) ||
            mc_world_mutation_execution_owns_world(call->addition, token));
}

const mc_world_mutation_error *
mc_live_tile_entity_query_execution_failure(const mc_live_tile_entity_query_execution_call *call) {
    return call != NULL && !call->active && call->addition_invoked && !call->source_completed
               ? call->error
               : NULL;
}

bool mc_live_tile_entity_query_execution_source_null(
    const mc_live_tile_entity_query_execution_call *call, mc_entity_tick_execution_site *site) {
    return call != NULL && !call->active &&
           mc_world_mutation_execution_source_null(call->addition, site);
}

mc_world_storage_status
mc_live_tile_entity_query_execution_native_close(mc_live_tile_entity_query_execution_call *call) {
    if (call == NULL)
        return MC_WORLD_STORAGE_OK;
    if (call->active || !live_lazy_quiet(call->owned))
        return MC_WORLD_STORAGE_BUSY;
    mc_world_storage_status status = mc_world_mutation_execution_native_close(call->addition);
    if (status == MC_WORLD_STORAGE_OK)
        status = live_lazy_pop(call);
    call->native_close = status;
    return status;
}

mc_world_storage_status
mc_live_tile_entity_query_execution_retire(mc_live_tile_entity_query_execution_call **slot) {
    if (slot == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (*slot == NULL)
        return MC_WORLD_STORAGE_OK;
    mc_live_tile_entity_query_execution_call *call = *slot;
    mc_world_storage_status status = mc_live_tile_entity_query_execution_native_close(call);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    status = mc_world_mutation_execution_retire(&call->addition);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    free(call->error);
    free(call);
    *slot = NULL;
    return MC_WORLD_STORAGE_OK;
}

static bool live_lazy_begin(const live_query_execution *query, mc_world_runtime *world,
                            mc_entity_tick_execution_world_token *token,
                            mc_live_tile_entity_error *error) {
    mc_live_tile_entity_query_execution_call *call = calloc(1, sizeof(*call));
    if (call == NULL) {
        live_fail(error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                  "TileEntity lazy Block retained owner allocation");
        mc_world_runtime_execution_result_note(query->owned, error->status, error->operation,
                                               query->receiver_site);
        return false;
    }
    call->execution = query->execution;
    call->owned = query->owned;
    call->site = query->receiver_site;
    call->active = true;
    *query->lazy_call = call;
    const mc_world_storage_status status = mc_entity_tick_execution_push_world(
        query->execution, world, query->receiver_site, &call->world);
    call->created = status == MC_WORLD_STORAGE_OK;
    if (!call->created) {
        call->active = false;
        mc_world_runtime_execution_result_note(query->owned, status,
                                               "TileEntity lazy Block evaluated World ownership",
                                               query->receiver_site);
        live_fail(error, status, "TileEntity lazy Block evaluated World ownership");
        return false;
    }
    *token = call->world;
    return true;
}

static void live_lazy_project(const mc_world_mutation_error *child,
                              mc_live_tile_entity_error *error) {
    error->status = child->status;
    error->operation = child->operation;
    error->cause = child->cause;
    error->registry = child->registry;
    error->entity = child->entity;
    error->publication = child->publication;
    error->batch = child->batch;
    error->tile_entity = child->tile_entity.tile_entity;
    error->load_entity = child->load_entity;
    error->activation_bytecode = child->activation_bytecode;
    error->activation_world_bytecode = child->activation_world_bytecode;
    error->load_bytecode = child->load_bytecode;
    error->load_builder_bytecode = child->load_builder_bytecode;
    error->provider_bytecode =
        child->provider_bytecode != 0 ? child->provider_bytecode : child->query.provider_bytecode;
    error->world_access_failure = child->world_access_failure;
    error->world_access_bytecode = child->world_access_bytecode;
    error->caller_bytecode = child->hook_bytecode;
    error->hook_bytecode =
        child->cause_hook_bytecode != 0 ? child->cause_hook_bytecode : child->hook_bytecode;
}

static bool live_lazy_furnace(const live_query_execution *query, mc_world_runtime *world,
                              const mc_block *block, int32_t x, int32_t y, int32_t z,
                              mc_live_tile_entity_error *error) {
    mc_live_tile_entity_query_execution_call *call = *query->lazy_call;
    const mc_world_mutation_execution_context context = {query->execution, query->code_classes,
                                                         query->code, query->owned, call->world};
    call->error = calloc(1, sizeof(*call->error));
    if (call->error == NULL) {
        live_fail(error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                  "TileEntity lazy Furnace retained child diagnostics");
        mc_world_runtime_execution_result_note(query->owned, error->status, error->operation,
                                               query->receiver_site);
        return false;
    }
    mc_world_mutation_error_init(call->error);
    call->addition_invoked = true;
    const bool completed = mc_world_mutation_execution_added_with_tile_bindings(
        &context, query->registry, query->factory_classes, world, block, x, y, z, &call->addition,
        call->error, query->receiver_site);
    if (!completed)
        live_lazy_project(call->error, error);
    return completed;
}
