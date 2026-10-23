#include "mutation_internal.h"

#include "../../../block/registry/runtime_internal.h"
#include "../../storage/objects_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"

#include <stdlib.h>

static bool in_domain(int32_t x, int32_t y, int32_t z) {
    return x >= -32000000 && x < 32000000 && z >= -32000000 && z <= 32000000 && y >= 0 && y < 128;
}

static mc_world_storage_status result(mc_world_mutation_error *out,
                                      mc_world_mutation_error *frame) {
    const mc_world_storage_status status = frame->status;
    if (out != NULL)
        *out = *frame;
    free(frame);
    return status;
}

static bool storage_fail(mc_world_runtime *world, mc_world_mutation_error *frame,
                         mc_world_storage_error cause) {
    frame->status = cause.status;
    frame->operation = cause.operation;
    frame->cause = cause;
    world->last_error = cause;
    return false;
}

static mc_world_storage_status admit(mc_world_runtime *world, bool has_out,
                                     mc_world_mutation_error *error) {
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    const char *operation = NULL;
    if (world == NULL || !has_out) {
        status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
        operation = "World mutation arguments";
    } else if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
               world->construction.phase == MC_WORLD_RUNTIME_RETIRED) {
        status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        operation = "World mutation phase";
    }
    /* Native admission changes only the returned diagnostic. */
    if (status != MC_WORLD_STORAGE_OK && error != NULL) {
        mc_world_mutation_error_init(error);
        error->status = status;
        error->operation = operation;
        error->cause = (mc_world_storage_error){status, operation, 0};
    }
    return status;
}

/* Diagnostic workspaces must not accumulate on the C stack during synchronous
 * neighbor recursion. Allocation is checked before any source mutation; each
 * invocation retains its own complete diagnostic until its descendants return. */
static mc_world_mutation_error *workspace(mc_world_runtime *world, mc_world_mutation_error *error) {
    mc_world_mutation_error *frame = malloc(sizeof(*frame));
    if (frame == NULL) {
        const mc_world_storage_error cause = {MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                              "World mutation diagnostic allocation", 0};
        mc_world_mutation_error_init(error);
        if (error != NULL) {
            error->status = cause.status;
            error->operation = cause.operation;
            error->cause = cause;
        }
        world->last_error = cause;
    } else {
        mc_world_mutation_error_init(frame);
    }
    return frame;
}

/* Return before a concrete setter can recurse, so acquisition diagnostics do
 * not remain in every ancestor's native stack frame. */
static bool acquire(mc_world_runtime *world, int32_t x, int32_t z, mc_chunk_state **out,
                    mc_world_mutation_error *frame) {
    frame->bytecode = 48;
    if (world->fields.v == NULL) {
        frame->provider_bytecode = 6;
        return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "World mutation provider receiver");
    }
    mc_world_runtime_acquisition_error detail;
    const mc_chunk_acquire_status acquired = mc_world_runtime_get_detailed(
        world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), out, &detail);
    if (acquired == MC_CHUNK_ACQUIRE_OK)
        return true;
    frame->tile = detail.tile;
    frame->registry = detail.registry;
    frame->batch = detail.batch;
    frame->activation_bytecode = detail.activation_bytecode;
    frame->activation_world_bytecode = detail.activation_world_bytecode;
    frame->load_bytecode = detail.load_bytecode;
    frame->load_builder_bytecode = detail.load_builder_bytecode;
    frame->load_entity = detail.load_entity;
    frame->population = detail.population;
    if (detail.cause.status != MC_WORLD_STORAGE_OK)
        return storage_fail(world, frame, detail.cause);
    if (acquired == MC_CHUNK_ACQUIRE_LIMIT || acquired == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT)
        return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_LIMIT,
                                      "World mutation acquisition limit");
    return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "World mutation acquisition");
}

/* Enclosing caller holds the initiating World. Acquiring c(II) is still reached
 * even when the concrete setter will return false. The captured Chunk remains
 * leased across all synchronous descendants of that one concrete invocation. */
static bool quiet(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t id,
                  bool with_metadata, int32_t metadata, bool *out, mc_world_mutation_error *frame) {
    mc_chunk_state *state = NULL;
    if (!acquire(world, x, z, &state, frame))
        return false;
    frame->bytecode = with_metadata ? 68u : 66u;
    if (state->blank) {
        *out = true; /* Actual Blank override returns true without mutation. */
        frame->bytecode = 0;
        return true;
    }

    mc_world_store *store = world->options.store;
    mc_world_chunk *chunk = mc_world_store_chunk_object(store, state);
    mc_world_chunk_fields fields;
    if (chunk == NULL || mc_world_chunk_fields_get(chunk, &fields) != MC_WORLD_STORAGE_OK ||
        fields.d == NULL || fields.d->construction.mode != MC_WORLD_RUNTIME_GAME ||
        fields.d->construction.phase == MC_WORLD_RUNTIME_RETIRED || fields.d->fields.w != store)
        return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "World mutation concrete Chunk binding");

    mc_world_storage_error cause = {0};
    if (mc_world_runtime_call_enter(fields.d, &cause) != MC_WORLD_STORAGE_OK)
        return storage_fail(world, frame, cause);
    if (mc_world_store_lease_chunk(store, state, &cause) != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_call_leave(fields.d);
        return storage_fail(world, frame, cause);
    }
    const int32_t local_x = (int32_t)((uint32_t)x & 15u);
    const int32_t local_z = (int32_t)((uint32_t)z & 15u);
    const bool completed =
        with_metadata ? mc_world_mutation_chunk_block_metadata(chunk, local_x, y, local_z, id,
                                                               metadata, out, frame)
                      : mc_world_mutation_chunk_block(chunk, local_x, y, local_z, id, out, frame);
    (void)mc_world_store_release_chunk(store, state, NULL);
    mc_world_runtime_call_leave(fields.d);
    if (!completed)
        world->last_error = frame->cause;
    else
        frame->bytecode = 0;
    return completed;
}

static mc_world_storage_status set(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                   int32_t id, bool with_metadata, int32_t metadata, bool notify,
                                   bool *out, mc_world_mutation_error *error) {
    const mc_world_storage_status admission = admit(world, out != NULL, error);
    if (admission != MC_WORLD_STORAGE_OK)
        return admission;
    if (!in_domain(x, y, z)) {
        *out = false;
        mc_world_mutation_error_init(error);
        return MC_WORLD_STORAGE_OK;
    }
    mc_world_mutation_error *frame = workspace(world, error);
    if (frame == NULL)
        return MC_WORLD_STORAGE_ALLOCATION_FAILED;
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_call_enter(world, &cause) != MC_WORLD_STORAGE_OK) {
        frame->status = cause.status;
        frame->operation = cause.operation;
        frame->cause = cause;
        return result(error, frame);
    }
    bool changed = false;
    if (notify)
        frame->wrapper_bytecode = with_metadata ? 8u : 6u;
    bool completed = quiet(world, x, y, z, id, with_metadata, metadata, &changed, frame);
    if (completed && changed && notify) {
        frame->wrapper_bytecode = with_metadata ? 20u : 18u;
        completed = mc_world_mutation_observe_cell(world, x, y, z, frame) &&
                    mc_world_mutation_neighbors(world, x, y, z, id, frame);
    }
    if (completed) {
        *out = changed;
        mc_world_mutation_error_init(frame);
    }
    mc_world_runtime_call_leave(world);
    return result(error, frame);
}

mc_world_storage_status mc_world_runtime_block_set_quiet(mc_world_runtime *world, int32_t x,
                                                         int32_t y, int32_t z, int32_t id,
                                                         bool *out,
                                                         mc_world_mutation_error *error) {
    return set(world, x, y, z, id, false, 0, false, out, error);
}

mc_world_storage_status mc_world_runtime_block_metadata_set_quiet(mc_world_runtime *world,
                                                                  int32_t x, int32_t y, int32_t z,
                                                                  int32_t id, int32_t metadata,
                                                                  bool *out,
                                                                  mc_world_mutation_error *error) {
    return set(world, x, y, z, id, true, metadata, false, out, error);
}

mc_world_storage_status mc_world_runtime_block_set(mc_world_runtime *world, int32_t x, int32_t y,
                                                   int32_t z, int32_t id, bool *out,
                                                   mc_world_mutation_error *error) {
    return set(world, x, y, z, id, false, 0, true, out, error);
}

mc_world_storage_status mc_world_runtime_block_metadata_set(mc_world_runtime *world, int32_t x,
                                                            int32_t y, int32_t z, int32_t id,
                                                            int32_t metadata, bool *out,
                                                            mc_world_mutation_error *error) {
    return set(world, x, y, z, id, true, metadata, true, out, error);
}

mc_world_storage_status mc_world_runtime_metadata_set(mc_world_runtime *world, int32_t x, int32_t y,
                                                      int32_t z, int32_t metadata,
                                                      mc_world_mutation_error *error) {
    const mc_world_storage_status admission = admit(world, true, error);
    if (admission != MC_WORLD_STORAGE_OK)
        return admission;
    if (!in_domain(x, y, z)) {
        mc_world_mutation_error_init(error);
        return MC_WORLD_STORAGE_OK;
    }
    mc_world_mutation_error *frame = workspace(world, error);
    if (frame == NULL)
        return MC_WORLD_STORAGE_ALLOCATION_FAILED;
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_call_enter(world, &cause) != MC_WORLD_STORAGE_OK) {
        frame->status = cause.status;
        frame->operation = cause.operation;
        frame->cause = cause;
        return result(error, frame);
    }
    bool changed;
    frame->wrapper_bytecode = 6;
    if (mc_world_runtime_metadata_set_quiet(world, x, y, z, metadata, &changed, &cause) !=
        MC_WORLD_STORAGE_OK) {
        storage_fail(world, frame, cause);
        goto leave;
    }
    if (changed) {
        int32_t id;
        frame->wrapper_bytecode = 16;
        if (mc_world_runtime_block_get(world, x, y, z, &id, &cause) != MC_WORLD_STORAGE_OK) {
            storage_fail(world, frame, cause);
            goto leave;
        }
        mc_registry_classes *classes = mc_world_runtime_registry(world);
        mc_registry_block_t_view array;
        mc_registry_error registry = mc_world_mutation_empty_registry();
        frame->wrapper_bytecode = 21;
        if (classes == NULL) {
            registry.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
            registry.message = "World metadata Registry binding";
            mc_world_mutation_registry_fail(world, frame, &registry, registry.message);
            goto leave;
        }
        if (mc_registry_block_t_get(classes, &array, &registry) != MC_REGISTRY_OK) {
            mc_world_mutation_registry_fail(world, frame, &registry,
                                            "World metadata Block.t active use");
            goto leave;
        }
        bool observe;
        frame->wrapper_bytecode = 30;
        if (mc_registry_block_t_at(&array, (int32_t)((uint32_t)id & 255u), &observe, &registry) !=
            MC_REGISTRY_OK) {
            mc_world_mutation_registry_fail(world, frame, &registry,
                                            "World metadata Block.t element");
            goto leave;
        }
        frame->wrapper_bytecode = observe ? 40u : 52u;
        if (observe && !mc_world_mutation_observe_cell(world, x, y, z, frame))
            goto leave;
        if (!mc_world_mutation_neighbors(world, x, y, z, id, frame))
            goto leave;
    }
    mc_world_mutation_error_init(frame);
leave:
    mc_world_runtime_call_leave(world);
    return result(error, frame);
}
