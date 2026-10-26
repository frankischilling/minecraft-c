#include "entity_tick_internal.h"

bool wt_present(wt_scope *s, int32_t x, int32_t z, unsigned int pc, bool *out) {
    s->error.bytecode = pc;
    s->error.provider_bytecode = 6;
    mc_world_storage_error cause = {0};
    const mc_world_storage_status status =
        mc_world_runtime_chunk_provider_contains(s->world, x, z, out, &cause);
    if (status != MC_WORLD_STORAGE_OK)
        return wt_cause(s, cause);
    s->error.provider_bytecode = 0;
    return true;
}

bool wt_get(wt_scope *s, int32_t x, int32_t z, unsigned int pc, mc_chunk_state **out) {
    s->error.bytecode = pc;
    s->error.child = MC_WORLD_ENTITY_TICK_CHILD_ACQUISITION;
    const mc_chunk_acquire_status status =
        mc_world_runtime_get_detailed(s->world, x, z, out, &s->error.child_error.acquisition);
    if (status != MC_CHUNK_ACQUIRE_OK) {
        const mc_world_storage_error cause = s->error.child_error.acquisition.cause;
        if (cause.status != MC_WORLD_STORAGE_OK)
            return wt_cause(s, cause);
        return wt_fail(s,
                       status == MC_CHUNK_ACQUIRE_LIMIT || status == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT
                           ? MC_WORLD_STORAGE_LIMIT
                           : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World actual acquiring Chunk child");
    }
    s->error.child = MC_WORLD_ENTITY_TICK_CHILD_NONE;
    return true;
}

static mc_world_chunk *lease(wt_scope *s, mc_chunk_state *state, unsigned int pc) {
    s->error.bytecode = pc;
    if (state == NULL) {
        wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World acquired null Chunk receiver");
        return NULL;
    }
    mc_world_chunk *chunk = mc_world_store_chunk_object(s->world->fields.w, state);
    if (chunk == NULL) {
        wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World actual normal Chunk header");
        return NULL;
    }
    mc_world_storage_error cause = {0};
    if (mc_world_store_lease_chunk(s->world->fields.w, state, &cause) != MC_WORLD_STORAGE_OK) {
        wt_cause(s, cause);
        return NULL;
    }
    return chunk;
}

bool wt_chunk_remove(wt_scope *s, mc_chunk_state *state, mc_entity *entity, bool by_entity,
                     int32_t slice, unsigned int pc) {
    s->error.bytecode = pc;
    if (state == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World null removing Chunk receiver");
    if (state->blank)
        return true; /* Both actual li overloads are literal return. */
    mc_world_chunk *chunk = lease(s, state, pc);
    if (chunk == NULL)
        return false;
    bool ok = false;
    if (by_entity) {
        s->error.chunk_bytecode = 3;
        if (entity == NULL) {
            wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk.b null Entity slice receiver");
            goto done;
        }
        slice = entity->base.bH;
    }
    {
        const mc_world_storage_status status = mc_world_chunk_remove_entity(
            chunk, entity, slice, &s->error.loop_work, s->error.max_loop_work, &s->error.chunk);
        if (status == MC_WORLD_STORAGE_OK) {
            s->error.chunk_bytecode = 0;
            ok = true;
        } else {
            s->error.chunk_bytecode = s->error.chunk.bytecode;
            s->error.entity = s->error.chunk.entity;
            wt_cause(s, s->error.chunk.cause);
        }
    }
done:
    (void)mc_world_store_release_chunk(s->world->fields.w, state, NULL);
    return ok;
}

bool wt_chunk_add(wt_scope *s, mc_chunk_state *state, mc_entity *entity, unsigned int pc) {
    s->error.bytecode = pc;
    if (state == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World null adding Chunk receiver");
    if (state->blank)
        return true;
    mc_world_chunk *chunk = lease(s, state, pc);
    if (chunk == NULL)
        return false;
    const mc_world_storage_status status =
        mc_world_chunk_insert_entity(chunk, entity, &s->error.chunk);
    (void)mc_world_store_release_chunk(s->world->fields.w, state, NULL);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    s->error.chunk_bytecode = s->error.chunk.bytecode;
    s->error.entity = s->error.chunk.entity;
    return wt_cause(s, s->error.chunk.cause);
}
