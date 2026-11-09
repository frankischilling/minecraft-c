#include "../chunk/object_internal.h"
#include "activation_entity_execution_internal.h"
#include "activation_execution_internal.h"
#include "activation_internal.h"
#include "activation_tile_execution_internal.h"

#include "construction_internal.h"
#include "entity_publication_detailed_execution_internal.h"
#include "entity_publication_execution_internal.h"
#include "entity_publication_internal.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"
#include "runtime_internal.h"
#include "tile_entities/execution_internal.h"

static mc_world_storage_status result(mc_world_activation_error *error, mc_world_runtime *world,
                                      unsigned int bytecode, unsigned int world_bytecode,
                                      mc_world_storage_error cause) {
    if (cause.status != MC_WORLD_STORAGE_OK && world != NULL)
        world->last_error = cause;
    if (error != NULL)
        *error = (mc_world_activation_error){.status = cause.status,
                                             .operation = cause.operation,
                                             .bytecode = bytecode,
                                             .world_bytecode = world_bytecode,
                                             .cause = cause,
                                             .tile_entity = mc_live_tile_entities_empty_error(),
                                             .batch = mc_world_entity_batch_empty_error()};
    return cause.status;
}

static mc_world_storage_status failed(mc_world_activation_error *error, mc_world_runtime *world,
                                      unsigned int bytecode, unsigned int world_bytecode,
                                      const char *operation) {
    return result(error, world, bytecode, world_bytecode,
                  (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation, 0});
}

static mc_world_storage_status completed(mc_world_activation_error *error) {
    return result(error, NULL, 0, 0, (mc_world_storage_error){MC_WORLD_STORAGE_OK, NULL, 0});
}

static mc_world_storage_status fields_get(mc_world_chunk *chunk, mc_world_chunk_fields *fields,
                                          mc_world_activation_error *error) {
    if (chunk != NULL && !mc_world_chunk_source_enabled(chunk))
        return failed(error, NULL, 0, 0, "Chunk source operations are disabled");
    mc_world_storage_status status = mc_world_chunk_fields_get(chunk, fields);
    if (status != MC_WORLD_STORAGE_OK)
        return result(error, NULL, 0, 0,
                      (mc_world_storage_error){status, "Chunk activation owner", 0});
    return MC_WORLD_STORAGE_OK;
}

/* The receiver has already been evaluated in the Chunk expression. Enter only
 * at its actual World invocation, not while evaluating l or m. Success owns
 * one hold; every caller releases it after its concrete callee finishes. */
static mc_world_storage_status enter_world(mc_world_runtime *world, unsigned int bytecode,
                                           mc_world_activation_error *error) {
    if (world == NULL)
        return failed(error, NULL, bytecode, 0, "Chunk World receiver is null");
    mc_world_storage_error cause = {0};
    mc_world_storage_status status = mc_world_runtime_call_enter(world, &cause);
    if (status != MC_WORLD_STORAGE_OK)
        return result(error, world, bytecode, 0, cause);
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED) {
        status = failed(error, world, bytecode, 0, "Chunk requires live GAME World receiver");
        mc_world_runtime_call_leave(world);
        return status;
    }
    return MC_WORLD_STORAGE_OK;
}

/* Checked native scope bookkeeping only. First reached Source status/cause
 * stays in error even if closing B also refuses. No source operation retries. */
static mc_world_storage_status close_tile_scope(mc_entity_tick_execution *execution,
                                                const mc_entity_tick_execution_world_token *token,
                                                mc_world_storage_status source_status,
                                                mc_entity_tick_execution_site site,
                                                mc_world_activation_error *error,
                                                mc_world_runtime_execution_result *owned) {
    if (execution == NULL)
        return source_status;
    const mc_world_storage_status status = mc_world_runtime_execution_close_scope(
        execution, owned, MC_WORLD_RUNTIME_EXECUTION_CLOSE_CHUNK_TILE, token, source_status,
        "Chunk TileEntity activation owned scope cleanup", site);
    if (status == MC_WORLD_STORAGE_OK)
        return source_status;
    if (source_status != MC_WORLD_STORAGE_OK)
        return source_status;
    return result(
        error, NULL, 0, 0,
        (mc_world_storage_error){status, "Chunk TileEntity activation owned scope cleanup", 0});
}

static mc_world_storage_status register_chunk_tile_entities_run(
    mc_world_chunk *chunk, mc_world_activation_error *error, mc_entity_tick_execution *execution,
    mc_world_runtime_execution_code code, mc_world_runtime_execution_result *owned) {
    if (execution != NULL &&
        (execution->identity != execution || execution->phase != MC_ENTITY_TICK_EXECUTION_OPEN)) {
        const mc_world_storage_status status = execution->identity != execution
                                                   ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                   : MC_WORLD_STORAGE_BUSY;
        execution->admission = (mc_entity_tick_execution_error){
            status, "Chunk TileEntity activation execution owner admission", execution->site};
        mc_world_runtime_execution_result_note(
            owned, status, "Chunk TileEntity activation execution owner admission",
            code.tile_receiver_site);
        return result(error, NULL, 0, 0,
                      (mc_world_storage_error){
                          status, "Chunk TileEntity activation execution owner admission", 0});
    }
    mc_world_chunk_fields fields;
    mc_world_storage_status status = fields_get(chunk, &fields, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    /* Actual nullable d6 is evaluated before l10/values13. Native retention
     * adds no eager Source invocation, NULL diagnosis or class active use. */
    mc_world_runtime *world = fields.d;
    mc_entity_tick_execution_world_token token = {0};
    if (execution != NULL) {
        status =
            mc_entity_tick_execution_push_world(execution, world, code.tile_receiver_site, &token);
        if (status != MC_WORLD_STORAGE_OK) {
            mc_world_runtime_execution_result_note(
                owned, status, "Chunk TileEntity activation evaluated World ownership",
                code.tile_receiver_site);
            return result(error, NULL, 0, 0,
                          (mc_world_storage_error){
                              status, "Chunk TileEntity activation evaluated World ownership", 0});
        }
    }
    /* values13 still precedes invocation18, including when evaluated B=NULL. */
    if (fields.l == NULL) {
        status = failed(error, NULL, 13, 0, "Chunk TileEntity Map values receiver is null");
        return close_tile_scope(execution, &token, status, code.tile_receiver_site, error, owned);
    }
    status = enter_world(world, 18, error);
    if (status != MC_WORLD_STORAGE_OK)
        return close_tile_scope(execution, &token, status, code.tile_receiver_site, error, owned);

    mc_live_tile_entity_error detail = mc_live_tile_entities_empty_error();
    status = execution == NULL ? mc_live_tile_entities_chunk_register(chunk, &detail)
                               : mc_live_tile_entities_chunk_register_in_execution(
                                     execution, world, code, chunk, &detail, owned);
    if (status != MC_WORLD_STORAGE_OK) {
        result(error, world, detail.bytecode, detail.chunk_bytecode, detail.cause);
        if (error != NULL)
            error->tile_entity = detail;
    }
    mc_world_runtime_call_leave(world);
    status = close_tile_scope(execution, &token, status, code.tile_receiver_site, error, owned);
    return status == MC_WORLD_STORAGE_OK ? completed(error) : status;
}

mc_world_storage_status
mc_world_runtime_register_chunk_tile_entities(mc_world_chunk *chunk,
                                              mc_world_activation_error *error) {
    return register_chunk_tile_entities_run(chunk, error, NULL,
                                            (mc_world_runtime_execution_code){0}, NULL);
}

mc_world_storage_status mc_world_runtime_register_chunk_tile_entities_in_execution(
    mc_entity_tick_execution *execution, mc_world_chunk *chunk,
    mc_world_runtime_execution_code code, mc_world_activation_error *error,
    mc_world_runtime_execution_result *owned) {
    if (owned != NULL)
        mc_world_runtime_execution_result_init(owned);
    if (execution == NULL || owned == NULL) {
        mc_world_runtime_execution_result_note(
            owned, MC_WORLD_STORAGE_INVALID_ARGUMENT,
            "Chunk TileEntity activation execution/result required", code.tile_receiver_site);
        return result(
            error, NULL, 0, 0,
            (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                     "Chunk TileEntity activation execution/result required", 0});
    }
    return register_chunk_tile_entities_run(chunk, error, execution, code, owned);
}

/* Native checked scope coordination only. Source failure status/cause/typed
 * batch prefix has already been selected by the one actual entered body and
 * remains primary if this close also refuses. A genuine non-consuming close
 * publishes its exact created token after local native calls have drained;
 * callers retry that explicit capsule, never this Source body. */
static mc_world_storage_status close_entity_scope(mc_entity_tick_execution *execution,
                                                  const mc_entity_tick_execution_world_token *token,
                                                  mc_world_storage_status source_status,
                                                  mc_world_activation_error *error,
                                                  mc_world_runtime_execution_result *native,
                                                  mc_entity_tick_execution_site site) {
    if (execution == NULL)
        return source_status;
    const mc_world_storage_status status = mc_world_runtime_execution_close_scope(
        execution, native, MC_WORLD_RUNTIME_EXECUTION_CLOSE_CHUNK_ENTITY, token, source_status,
        "Chunk Entity activation owned scope cleanup", site);
    /* The actual checked pop published its own refused close VALUE before
     * this local frame leaves. Earlier Source failure remains primary. */
    if (status == MC_WORLD_STORAGE_OK || source_status != MC_WORLD_STORAGE_OK)
        return source_status;
    /* Native layer attribution. Detailed current admission/cleanup remains in
     * execution; do not choose an old neutral status by matching its number. */
    return result(
        error, NULL, 0, 0,
        (mc_world_storage_error){status, "Chunk Entity activation owned scope cleanup", 0});
}

static mc_world_storage_status
register_chunk_entities_run(mc_world_chunk *chunk, size_t slice, mc_world_activation_error *error,
                            mc_entity_tick_execution *execution,
                            mc_entity_tick_execution_site receiver_site,
                            mc_world_runtime_execution_result *native) {
    if (execution != NULL &&
        (execution->identity != execution || execution->phase != MC_ENTITY_TICK_EXECUTION_OPEN)) {
        const mc_world_storage_status status = execution->identity != execution
                                                   ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                   : MC_WORLD_STORAGE_BUSY;
        execution->admission = (mc_entity_tick_execution_error){
            status, "Chunk Entity activation execution owner admission", execution->site};
        mc_world_runtime_execution_result_note(
            native, status, "Chunk Entity activation execution owner admission", receiver_site);
        return result(error, NULL, 0, 0,
                      (mc_world_storage_error){
                          status, "Chunk Entity activation execution owner admission", 0});
    }
    mc_world_chunk_fields fields;
    mc_world_storage_status status = fields_get(chunk, &fields, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    /* Actual nullable d33 is evaluated BEFORE m/slice41. Only native ownership
     * is acquired here; NULL World invocation remains delayed until42. */
    mc_world_runtime *world = fields.d;
    mc_entity_tick_execution_world_token token = {0};
    if (execution != NULL) {
        status = mc_entity_tick_execution_push_world(execution, world, receiver_site, &token);
        if (status != MC_WORLD_STORAGE_OK) {
            mc_world_runtime_execution_result_note(
                native, status, "Chunk Entity activation evaluated World ownership", receiver_site);
            return result(error, NULL, 0, 0,
                          (mc_world_storage_error){
                              status, "Chunk Entity activation evaluated World ownership", 0});
        }
    }
    const mc_world_chunk_entity_list *source;
    status = mc_world_chunk_slice_at(fields.m, slice, &source);
    if (status != MC_WORLD_STORAGE_OK) {
        status = result(error, NULL, 41, 0,
                        (mc_world_storage_error){status, "Chunk registration entity slice", 0});
        return close_entity_scope(execution, &token, status, error, native, receiver_site);
    }
    status = enter_world(world, 42, error);
    if (status != MC_WORLD_STORAGE_OK)
        return close_entity_scope(execution, &token, status, error, native, receiver_site);
    /* b1/addAll5 completes before the subsequent live input size15. A NULL
     * slice is evaluated successfully at41 and fails only inside addAll. */
    mc_world_entity_batch_error detail = mc_world_entity_batch_empty_error();
    status = execution == NULL ? mc_world_runtime_entity_batch_held(world, source, false, &detail)
             : native == NULL
                 ? mc_world_runtime_entity_batch_held_in_execution(execution, world, source, false,
                                                                   &detail)
                 : mc_world_runtime_entity_batch_held_detailed_in_execution(
                       execution, world, source, false, receiver_site, &detail, native);
    if (status != MC_WORLD_STORAGE_OK) {
        result(error, world, 42, detail.publication.bytecode, detail.publication.cause);
        if (error != NULL)
            error->batch = detail;
    }
    mc_world_runtime_call_leave(world);
    status = close_entity_scope(execution, &token, status, error, native, receiver_site);
    return status == MC_WORLD_STORAGE_OK ? completed(error) : status;
}

mc_world_storage_status mc_world_runtime_register_chunk_entities(mc_world_chunk *chunk,
                                                                 size_t slice,
                                                                 mc_world_activation_error *error) {
    return register_chunk_entities_run(chunk, slice, error, NULL,
                                       (mc_entity_tick_execution_site){NULL, NULL, NULL, -1}, NULL);
}

mc_world_storage_status mc_world_runtime_register_chunk_entities_in_execution(
    mc_entity_tick_execution *execution, mc_world_chunk *chunk, size_t slice,
    mc_entity_tick_execution_site evaluated_receiver_site, mc_world_activation_error *error) {
    if (execution == NULL)
        return result(error, NULL, 0, 0,
                      (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "Chunk Entity activation execution is required", 0});
    return register_chunk_entities_run(chunk, slice, error, execution, evaluated_receiver_site,
                                       NULL);
}

mc_world_storage_status mc_world_runtime_register_chunk_entities_detailed_in_execution(
    mc_entity_tick_execution *execution, mc_world_chunk *chunk, size_t slice,
    mc_world_runtime_execution_code code, mc_world_activation_error *error,
    mc_world_runtime_execution_result *native) {
    mc_world_runtime_execution_result_init(native);
    if (execution == NULL || native == NULL) {
        mc_world_runtime_execution_result_note(
            native, MC_WORLD_STORAGE_INVALID_ARGUMENT,
            "Chunk Entity activation execution/result is required", code.entity_receiver_site);
        return result(
            error, NULL, 0, 0,
            (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                     "Chunk Entity activation execution/result is required", 0});
    }
    return register_chunk_entities_run(chunk, slice, error, execution, code.entity_receiver_site,
                                       native);
}

mc_world_storage_status
mc_world_runtime_invalidate_chunk_tile_entities(mc_world_chunk *chunk,
                                                mc_world_activation_error *error) {
    mc_world_chunk_fields fields;
    mc_world_storage_status status = fields_get(chunk, &fields, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (fields.l == NULL)
        return failed(error, NULL, 9, 0, "Chunk detach TileEntity Map values receiver is null");
    /* TileEntity invalidation reads no World receiver, including a nonempty Map. */
    mc_live_tile_entity_error detail = mc_live_tile_entities_empty_error();
    status = mc_live_tile_entities_chunk_invalidate(chunk, &detail);
    if (status != MC_WORLD_STORAGE_OK) {
        result(error, NULL, detail.bytecode, detail.chunk_bytecode, detail.cause);
        if (error != NULL)
            error->tile_entity = detail;
        return status;
    }
    return completed(error);
}

mc_world_storage_status mc_world_runtime_defer_chunk_entities(mc_world_chunk *chunk, size_t slice,
                                                              mc_world_activation_error *error) {
    mc_world_chunk_fields fields;
    mc_world_storage_status status = fields_get(chunk, &fields, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    const mc_world_chunk_entity_list *source;
    status = mc_world_chunk_slice_at(fields.m, slice, &source);
    if (status != MC_WORLD_STORAGE_OK)
        return result(error, NULL, 66, 0,
                      (mc_world_storage_error){status, "Chunk deferral entity slice", 0});
    mc_world_runtime *world = fields.d;
    status = enter_world(world, 67, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    /* The deferral callee reads D1, not activation's b or TileEntity G/c. */
    mc_world_entity_batch_error detail = mc_world_entity_batch_empty_error();
    status = mc_world_runtime_entity_batch_held(world, source, true, &detail);
    if (status != MC_WORLD_STORAGE_OK) {
        result(error, world, 67, detail.publication.bytecode, detail.publication.cause);
        if (error != NULL)
            error->batch = detail;
    }
    mc_world_runtime_call_leave(world);
    return status == MC_WORLD_STORAGE_OK ? completed(error) : status;
}
