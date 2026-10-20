#include "../chunk/object_internal.h"
#include "activation_internal.h"

#include "construction_internal.h"
#include "entity_publication_internal.h"
#include "mc/world/runtime/tiles/tiles.h"
#include "runtime_internal.h"

static mc_world_storage_status result(mc_world_activation_error *error, mc_world_runtime *world,
                                      unsigned int bytecode, unsigned int world_bytecode,
                                      mc_world_storage_error cause)
{
    if (cause.status != MC_WORLD_STORAGE_OK && world != NULL)
        world->last_error = cause;
    if (error != NULL)
        *error = (mc_world_activation_error){.status = cause.status,
                                             .operation = cause.operation,
                                             .bytecode = bytecode,
                                             .world_bytecode = world_bytecode,
                                             .cause = cause,
                                             .tile = mc_live_tiles_empty_error(),
                                             .batch = mc_world_entity_batch_empty_error()};
    return cause.status;
}

static mc_world_storage_status failed(mc_world_activation_error *error, mc_world_runtime *world,
                                      unsigned int bytecode, unsigned int world_bytecode,
                                      const char *operation)
{
    return result(error, world, bytecode, world_bytecode,
                  (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation, 0});
}

static mc_world_storage_status completed(mc_world_activation_error *error)
{
    return result(error, NULL, 0, 0, (mc_world_storage_error){MC_WORLD_STORAGE_OK, NULL, 0});
}

static mc_world_storage_status fields_get(mc_world_chunk *chunk, mc_world_chunk_fields *fields,
                                          mc_world_activation_error *error)
{
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
                                           mc_world_activation_error *error)
{
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

mc_world_storage_status mc_world_runtime_register_chunk_tiles(mc_world_chunk *chunk,
                                                              mc_world_activation_error *error)
{
    mc_world_chunk_fields fields;
    mc_world_storage_status status = fields_get(chunk, &fields, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    /* values13 precedes invocation18. Its borrowed finite view is the actual
     * Map, not an allocated or shared stand-in collection. */
    if (fields.l == NULL)
        return failed(error, NULL, 13, 0, "Chunk tile Map values receiver is null");
    mc_world_runtime *world = fields.d;
    status = enter_world(world, 18, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;

    mc_live_tile_error detail = mc_live_tiles_empty_error();
    status = mc_live_tiles_chunk_register(chunk, &detail);
    if (status != MC_WORLD_STORAGE_OK) {
        result(error, world, detail.bytecode, detail.chunk_bytecode, detail.cause);
        if (error != NULL)
            error->tile = detail;
    }
    mc_world_runtime_call_leave(world);
    return status == MC_WORLD_STORAGE_OK ? completed(error) : status;
}

mc_world_storage_status mc_world_runtime_register_chunk_entities(mc_world_chunk *chunk,
                                                                 size_t slice,
                                                                 mc_world_activation_error *error)
{
    mc_world_chunk_fields fields;
    mc_world_storage_status status = fields_get(chunk, &fields, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    const mc_world_chunk_entity_list *source;
    status = mc_world_chunk_slice_at(fields.m, slice, &source);
    if (status != MC_WORLD_STORAGE_OK)
        return result(error, NULL, 41, 0,
                      (mc_world_storage_error){status, "Chunk registration entity slice", 0});
    mc_world_runtime *world = fields.d;
    status = enter_world(world, 42, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    /* b1/addAll5 completes before the subsequent live input size15. A NULL
     * slice is evaluated successfully at41 and fails only inside addAll. */
    mc_world_entity_batch_error detail = mc_world_entity_batch_empty_error();
    status = mc_world_runtime_entity_batch_held(world, source, false, &detail);
    if (status != MC_WORLD_STORAGE_OK) {
        result(error, world, 42, detail.publication.bytecode, detail.publication.cause);
        if (error != NULL)
            error->batch = detail;
    }
    mc_world_runtime_call_leave(world);
    return status == MC_WORLD_STORAGE_OK ? completed(error) : status;
}

mc_world_storage_status mc_world_runtime_invalidate_chunk_tiles(mc_world_chunk *chunk,
                                                                mc_world_activation_error *error)
{
    mc_world_chunk_fields fields;
    mc_world_storage_status status = fields_get(chunk, &fields, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    if (fields.l == NULL)
        return failed(error, NULL, 9, 0, "Chunk detach tile Map values receiver is null");
    /* Tile invalidation reads no World receiver, including a nonempty Map. */
    mc_live_tile_error detail = mc_live_tiles_empty_error();
    status = mc_live_tiles_chunk_invalidate(chunk, &detail);
    if (status != MC_WORLD_STORAGE_OK) {
        result(error, NULL, detail.bytecode, detail.chunk_bytecode, detail.cause);
        if (error != NULL)
            error->tile = detail;
        return status;
    }
    return completed(error);
}

mc_world_storage_status mc_world_runtime_defer_chunk_entities(mc_world_chunk *chunk, size_t slice,
                                                              mc_world_activation_error *error)
{
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
    /* The deferral callee reads D1, not activation's b or tile G/c. */
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
