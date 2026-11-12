#include "mutation_internal.h"
#include "world_execution_internal.h"

#include "../../../block/registry/runtime_internal.h"
#include "../../storage/objects_internal.h"
#include "../blocks_execution_internal.h"
#include "../construction_internal.h"
#include "../execution_children_internal.h"
#include "../plants_execution_internal.h"
#include "../solid_support_execution_internal.h"
#include "chunk_execution_internal.h"
#include "drops/execution_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"

#include <stdlib.h>

struct mc_world_mutation_execution_call {
    mc_world_mutation_execution_context context;
    mc_world_access_failure_lease *lease;
    mc_world_mutation_error error;
    mc_world_block_execution scalar;
    mc_world_notification_execution observer;
    mc_world_runtime_acquisition_error acquisition;
    mc_world_mutation_chunk_execution *chunk;
    mc_world_mutation_execution_call *child;
    mc_world_mutation_drop_execution *drop;
    mc_world_solid_support_execution *support;
    mc_world_plant_execution *plant;
    mc_world_storage_status support_status;
    mc_world_storage_status plant_status;
    bool active, close_done, support_returned, plant_returned;
};

static const mc_entity_tick_execution_site metadata_site = {"fd", "d", "(IIII)V", 0};
static const mc_entity_tick_execution_site block_site = {"fd", "f", "(IIII)Z", 0};

static bool records_quiet(const mc_world_runtime_execution_result *result) {
    if (result->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (result->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (result->closes[i].used)
            return false;
    return true;
}

static bool same_world(const mc_entity_tick_execution_world_token *a,
                       const mc_entity_tick_execution_world_token *b) {
    return a->owner == b->owner && a->slot == b->slot && a->generation == b->generation;
}

static bool current(const mc_world_mutation_execution_context *context, mc_world_runtime *world,
                    mc_world_access_failure_lease **expected_lease,
                    mc_entity_tick_execution_site site) {
    mc_world_runtime_execution_result *result = context->result;
    if (result->ownership_status != MC_WORLD_STORAGE_OK)
        return false;
    if (result->selection_ready || result->math_failed || !records_quiet(result)) {
        mc_world_runtime_execution_result_note(
            result, MC_WORLD_STORAGE_BUSY, "World mutation canonical result still in use", site);
        return false;
    }
    mc_entity_tick_execution_world_token token;
    if (!mc_entity_tick_execution_current_world(context->execution, &token)) {
        mc_world_runtime_execution_result_note(result, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                               "World mutation checked current scope", site);
        return false;
    }
    if (!same_world(&context->world, &token)) {
        mc_world_runtime_execution_result_note(result, MC_WORLD_STORAGE_BUSY,
                                               "World mutation exact evaluated scope", site);
        return false;
    }
    mc_world_access_failure_lease *lease = NULL;
    const mc_world_storage_status status =
        mc_world_execution_child_lease(context->execution, world, &lease);
    mc_world_runtime_execution_result_note(result, status, "World mutation checked World lease",
                                           site);
    if (status != MC_WORLD_STORAGE_OK)
        return false;
    if (*expected_lease != NULL && *expected_lease != lease) {
        mc_world_runtime_execution_result_note(result, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                               "World mutation exact producer lease changed", site);
        return false;
    }
    *expected_lease = lease;
    return true;
}

mc_registry_classes *
mc_world_mutation_execution_registry(const mc_world_mutation_execution_call *call) {
    return call->context.code_classes->registry;
}

mc_world_runtime_execution_result *
mc_world_mutation_execution_result_of(const mc_world_mutation_execution_call *call) {
    return call == NULL ? NULL : call->context.result;
}

bool mc_world_mutation_execution_current(const mc_world_mutation_execution_call *call,
                                         mc_world_runtime *world,
                                         mc_entity_tick_execution_site site) {
    if (call == NULL)
        return false;
    mc_world_access_failure_lease *lease = call->lease;
    return current(&call->context, world, &lease, site);
}

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
static bool acquisition_failure(mc_world_runtime *world, mc_chunk_acquire_status acquired,
                                const mc_world_runtime_acquisition_error *detail,
                                mc_world_mutation_error *frame) {
    frame->tile_entity = detail->tile_entity;
    frame->registry = detail->registry;
    frame->batch = detail->batch;
    frame->activation_bytecode = detail->activation_bytecode;
    frame->activation_world_bytecode = detail->activation_world_bytecode;
    frame->load_bytecode = detail->load_bytecode;
    frame->load_builder_bytecode = detail->load_builder_bytecode;
    frame->load_entity = detail->load_entity;
    frame->population = detail->population;
    if (detail->cause.status != MC_WORLD_STORAGE_OK)
        return storage_fail(world, frame, detail->cause);
    if (acquired == MC_CHUNK_ACQUIRE_LIMIT || acquired == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT)
        return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_LIMIT,
                                      "World mutation acquisition limit");
    return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "World mutation acquisition");
}

static bool acquire(mc_world_runtime *world, int32_t x, int32_t z, mc_chunk_state **out,
                    mc_world_mutation_error *frame, bool with_metadata,
                    mc_world_mutation_execution_call *owned) {
    frame->bytecode = 48;
    if (world->fields.v == NULL) {
        frame->provider_bytecode = 6;
        return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "World mutation provider receiver");
    }
    if (owned != NULL) {
        const mc_entity_tick_execution_site site = {"fd", with_metadata ? "a" : "c",
                                                    with_metadata ? "(IIIII)Z" : "(IIII)Z", 48};
        if (!mc_world_mutation_execution_current(owned, world, site))
            return false;
        mc_world_runtime_execution_result *result = owned->context.result;
        const mc_chunk_acquire_status status = mc_world_runtime_get_failure_in_execution(
            owned->context.execution, world, owned->context.code, mc_java_i32_shr(x, 4),
            mc_java_i32_shr(z, 4), out, &owned->acquisition, result);
        if (status != MC_CHUNK_ACQUIRE_OK)
            return acquisition_failure(world, status, &owned->acquisition, frame);
        if (result->ownership_status != MC_WORLD_STORAGE_OK)
            return false;
        if (!result->selection_ready || result->selection.status != MC_CHUNK_ACQUIRE_OK ||
            !records_quiet(result)) {
            mc_world_runtime_execution_result_note(result, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                                   "World mutation completed GET outcome", site);
            return false;
        }
        result->selection_ready = false;
        return true;
    }
    mc_world_runtime_acquisition_error detail;
    const mc_chunk_acquire_status acquired = mc_world_runtime_get_detailed(
        world, mc_java_i32_shr(x, 4), mc_java_i32_shr(z, 4), out, &detail);
    if (acquired == MC_CHUNK_ACQUIRE_OK)
        return true;
    return acquisition_failure(world, acquired, &detail, frame);
}

/* Enclosing caller holds the initiating World. Acquiring c(II) is still reached
 * even when the concrete setter will return false. The captured Chunk remains
 * leased across all synchronous descendants of that one concrete invocation. */
static bool quiet(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t id,
                  bool with_metadata, int32_t metadata, bool *out, mc_world_mutation_error *frame,
                  mc_world_mutation_execution_call *owned) {
    mc_chunk_state *state = NULL;
    if (!acquire(world, x, z, &state, frame, with_metadata, owned))
        return false;
    frame->bytecode = with_metadata ? 68u : 66u;
    if (state->blank) {
        *out = true; /* Actual Blank override returns true without mutation. */
        frame->bytecode = 0;
        return true;
    }

    mc_world_store *store = world->options.store;
    mc_world_chunk *chunk = mc_world_store_chunk_object(store, state);
    const int32_t local_x = (int32_t)((uint32_t)x & 15u);
    const int32_t local_z = (int32_t)((uint32_t)z & 15u);
    if (owned != NULL) {
        const mc_entity_tick_execution_site site = {"fd", "c", "(IIII)Z", 66};
        if (chunk == NULL) {
            mc_world_runtime_execution_result_note(owned->context.result,
                                                   MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                                   "World mutation selected Chunk identity", site);
            return false;
        }
        owned->chunk = calloc(1, sizeof(*owned->chunk));
        if (owned->chunk == NULL) {
            mc_world_runtime_execution_result_note(owned->context.result,
                                                   MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                                   "World retained Chunk mutation frame", site);
            return false;
        }
        mc_world_mutation_chunk_execution_init(owned->chunk, owned->context.execution,
                                               owned->context.code, owned->context.code_classes,
                                               mc_world_mutation_execution_registry(owned), site,
                                               owned->context.world, owned->context.result);
        mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
        const bool completed = mc_world_mutation_chunk_block_in_execution(
            owned->chunk, world, store, state, chunk, local_x, y, local_z, id, out, frame, &raw);
        if (completed)
            frame->bytecode = 0;
        else if (raw != MC_WORLD_STORAGE_OK)
            world->last_error = frame->cause;
        return completed;
    }
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

static bool enter(mc_world_runtime *world, mc_world_mutation_error *frame,
                  mc_world_mutation_execution_call *owned, mc_entity_tick_execution_site site) {
    mc_world_storage_error cause = {0};
    const mc_world_storage_status status = mc_world_runtime_call_enter(world, &cause);
    if (status != MC_WORLD_STORAGE_OK) {
        if (owned != NULL) {
            mc_world_runtime_execution_result_note(owned->context.result, status, cause.operation,
                                                   site);
            return false;
        }
        frame->status = cause.status;
        frame->operation = cause.operation;
        frame->cause = cause;
        return false;
    }
    return true;
}

static bool notify_cell(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                        mc_world_mutation_error *frame, mc_world_mutation_execution_call *owned) {
    if (owned == NULL)
        return mc_world_mutation_observe_cell(world, x, y, z, frame);
    const mc_entity_tick_execution_site site = {"fd", "g", "(IIII)V", 4};
    if (!mc_world_mutation_execution_current(owned, world, site))
        return false;
    mc_world_notification_execution_init_borrowed(&owned->observer, owned->context.execution, site,
                                                  owned->context.result);
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    return mc_world_mutation_observe_cell_in_execution(&owned->observer, world, x, y, z, frame,
                                                       &raw);
}

static bool neighbors(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t id,
                      mc_world_mutation_error *frame, mc_world_mutation_execution_call *owned) {
    return owned == NULL
               ? mc_world_mutation_neighbors(world, x, y, z, id, frame)
               : mc_world_mutation_neighbors_in_execution(owned, world, x, y, z, id, frame);
}

/* fd.g invokes the cell observer at 4, then ordered neighbors at 13. */
static bool notify_and_neighbors(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                 int32_t id, mc_world_mutation_error *frame,
                                 mc_world_mutation_execution_call *owned) {
    return notify_cell(world, x, y, z, frame, owned) && neighbors(world, x, y, z, id, frame, owned);
}

static bool set_body(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t id,
                     bool with_metadata, int32_t metadata, bool notify, bool *out,
                     mc_world_mutation_error *frame, mc_world_mutation_execution_call *owned) {
    if (!enter(world, frame, owned, block_site))
        return false;
    bool changed = false;
    if (notify)
        frame->wrapper_bytecode = with_metadata ? 8u : 6u;
    bool completed = quiet(world, x, y, z, id, with_metadata, metadata, &changed, frame, owned);
    if (completed && changed && notify) {
        frame->wrapper_bytecode = with_metadata ? 20u : 18u;
        completed = notify_and_neighbors(world, x, y, z, id, frame, owned);
    }
    if (completed) {
        *out = changed;
        mc_world_mutation_error_init(frame);
    }
    mc_world_runtime_call_leave(world);
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
    (void)set_body(world, x, y, z, id, with_metadata, metadata, notify, out, frame, NULL);
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

static bool scalar_result(mc_world_mutation_execution_call *owned, mc_world_runtime *world,
                          bool completed, mc_world_storage_status raw, mc_world_storage_error cause,
                          mc_world_mutation_error *frame) {
    if (completed)
        return true;
    if (owned->scalar.get_returned && owned->scalar.get_status != MC_CHUNK_ACQUIRE_OK)
        return acquisition_failure(world, owned->scalar.get_status, &owned->scalar.acquisition,
                                   frame);
    if (raw != MC_WORLD_STORAGE_OK)
        return storage_fail(world, frame, cause);
    return false;
}

static void prepare_scalar(mc_world_mutation_execution_call *owned,
                           mc_entity_tick_execution_site site) {
    owned->scalar = (mc_world_block_execution){.execution = owned->context.execution,
                                               .code = owned->context.code,
                                               .receiver_site = site,
                                               .owned = owned->context.result};
}

bool mc_world_mutation_execution_get(mc_world_mutation_execution_call *owned,
                                     mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                     bool metadata, int32_t *out, mc_world_mutation_error *frame,
                                     mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(owned, world, site))
        return false;
    prepare_scalar(owned, site);
    mc_world_storage_error cause = {0};
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    const bool completed = metadata ? mc_world_runtime_metadata_get_in_execution(
                                          &owned->scalar, world, x, y, z, out, &cause, &raw)
                                    : mc_world_runtime_block_get_in_execution(
                                          &owned->scalar, world, x, y, z, out, &cause, &raw);
    return scalar_result(owned, world, completed, raw, cause, frame);
}

static bool metadata_quiet(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                           int32_t metadata, bool *changed, mc_world_mutation_error *frame,
                           mc_world_mutation_execution_call *owned) {
    mc_world_storage_error cause = {0};
    if (owned == NULL) {
        const mc_world_storage_status raw =
            mc_world_runtime_metadata_set_quiet(world, x, y, z, metadata, changed, &cause);
        return raw == MC_WORLD_STORAGE_OK || storage_fail(world, frame, cause);
    }
    const mc_entity_tick_execution_site site = {"fd", "d", "(IIII)V", 6};
    if (!mc_world_mutation_execution_current(owned, world, site))
        return false;
    prepare_scalar(owned, site);
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    const bool completed = mc_world_runtime_metadata_set_quiet_in_execution(
        &owned->scalar, world, x, y, z, metadata, changed, &cause, &raw);
    return scalar_result(owned, world, completed, raw, cause, frame);
}

static bool metadata_body(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                          int32_t metadata, mc_world_mutation_error *frame,
                          mc_world_mutation_execution_call *owned) {
    if (!enter(world, frame, owned, metadata_site))
        return false;
    bool completed = false;
    bool changed;
    frame->wrapper_bytecode = 6;
    if (!metadata_quiet(world, x, y, z, metadata, &changed, frame, owned))
        goto leave;
    if (changed) {
        int32_t id;
        frame->wrapper_bytecode = 16;
        if (owned == NULL) {
            mc_world_storage_error cause = {0};
            if (mc_world_runtime_block_get(world, x, y, z, &id, &cause) != MC_WORLD_STORAGE_OK) {
                storage_fail(world, frame, cause);
                goto leave;
            }
        } else {
            const mc_entity_tick_execution_site site = {"fd", "d", "(IIII)V", 16};
            if (!mc_world_mutation_execution_get(owned, world, x, y, z, false, &id, frame, site))
                goto leave;
        }
        mc_registry_classes *classes = owned == NULL ? mc_world_runtime_registry(world)
                                                     : mc_world_mutation_execution_registry(owned);
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
        if (!(observe ? notify_and_neighbors(world, x, y, z, id, frame, owned)
                      : neighbors(world, x, y, z, id, frame, owned)))
            goto leave;
    }
    completed = true;
    mc_world_mutation_error_init(frame);
leave:
    mc_world_runtime_call_leave(world);
    return completed;
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
    (void)metadata_body(world, x, y, z, metadata, frame, NULL);
    return result(error, frame);
}

static bool admit_owned(const mc_world_mutation_execution_context *context, mc_world_runtime *world,
                        bool has_out, mc_world_mutation_execution_call **slot,
                        const mc_world_storage_status *raw, const mc_world_mutation_error *out,
                        mc_world_access_failure_lease **lease, mc_entity_tick_execution_site site) {
    if (context == NULL || context->result == NULL)
        return false;
    mc_world_runtime_execution_result *result = context->result;
    if (context->execution == NULL || context->execution->identity != context->execution ||
        context->code_classes == NULL || world == NULL || !has_out || slot == NULL ||
        *slot != NULL || raw == NULL || out == NULL) {
        mc_world_runtime_execution_result_note(result, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "World mutation execution and fresh outputs", site);
        return false;
    }
    if (!current(context, world, lease, site))
        return false;
    const mc_world_storage_status status = admit(world, true, NULL);
    mc_world_runtime_execution_result_note(result, status, "World mutation runtime phase", site);
    return status == MC_WORLD_STORAGE_OK;
}

static mc_world_mutation_execution_call *
start_owned(const mc_world_mutation_execution_context *context,
            mc_world_mutation_execution_call **slot, mc_world_access_failure_lease *lease,
            mc_entity_tick_execution_site site) {
    mc_world_mutation_execution_call *call = calloc(1, sizeof(*call));
    if (call == NULL) {
        mc_world_runtime_execution_result_note(context->result, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                               "World retained mutation invocation", site);
        return NULL;
    }
    call->context = *context;
    call->lease = lease;
    call->active = true;
    mc_world_mutation_error_init(&call->error);
    *slot = call;
    return call;
}

static bool finish_owned(mc_world_mutation_execution_call **slot, bool completed,
                         mc_world_storage_status *raw, mc_world_mutation_error *out,
                         mc_entity_tick_execution_site site, bool project_success) {
    mc_world_mutation_execution_call *call = *slot;
    call->active = false;
    *raw = call->error.status;
    if (!completed || project_success)
        *out = call->error;
    if (!completed)
        return false;
    mc_world_runtime_execution_result *result = call->context.result;
    const mc_world_storage_status status = mc_world_mutation_execution_retire(slot);
    mc_world_runtime_execution_result_note(result, status, "World completed mutation retirement",
                                           site);
    return status == MC_WORLD_STORAGE_OK;
}

bool mc_world_runtime_metadata_set_in_execution(const mc_world_mutation_execution_context *context,
                                                mc_world_runtime *world, int32_t x, int32_t y,
                                                int32_t z, int32_t metadata,
                                                mc_world_mutation_execution_call **slot,
                                                mc_world_storage_status *raw,
                                                mc_world_mutation_error *out) {
    mc_world_access_failure_lease *lease = NULL;
    if (!admit_owned(context, world, true, slot, raw, out, &lease, metadata_site))
        return false;
    if (!in_domain(x, y, z)) {
        *raw = MC_WORLD_STORAGE_OK;
        mc_world_mutation_error_init(out);
        return true;
    }
    mc_world_mutation_execution_call *call = start_owned(context, slot, lease, metadata_site);
    if (call == NULL)
        return false;
    const bool completed = metadata_body(world, x, y, z, metadata, &call->error, call);
    return finish_owned(slot, completed, raw, out, metadata_site, true);
}

static bool block_set_owned(const mc_world_mutation_execution_context *context,
                            mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t id,
                            bool *out, mc_world_mutation_execution_call **slot,
                            mc_world_storage_status *raw, mc_world_mutation_error *error,
                            bool project_success) {
    mc_world_access_failure_lease *lease = NULL;
    if (!admit_owned(context, world, out != NULL, slot, raw, error, &lease, block_site))
        return false;
    if (!in_domain(x, y, z)) {
        *raw = MC_WORLD_STORAGE_OK;
        *out = false;
        if (project_success)
            mc_world_mutation_error_init(error);
        return true;
    }
    mc_world_mutation_execution_call *call = start_owned(context, slot, lease, block_site);
    if (call == NULL)
        return false;
    bool changed = false;
    const bool completed =
        set_body(world, x, y, z, id, false, 0, true, &changed, &call->error, call);
    if (!finish_owned(slot, completed, raw, error, block_site, project_success))
        return false;
    *out = changed;
    return true;
}

bool mc_world_runtime_block_set_in_execution(const mc_world_mutation_execution_context *context,
                                             mc_world_runtime *world, int32_t x, int32_t y,
                                             int32_t z, int32_t id, bool *out,
                                             mc_world_mutation_execution_call **slot,
                                             mc_world_storage_status *raw,
                                             mc_world_mutation_error *error) {
    return block_set_owned(context, world, x, y, z, id, out, slot, raw, error, true);
}

bool mc_world_mutation_execution_remove(mc_world_mutation_execution_call *call,
                                        mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                        mc_world_mutation_error *error,
                                        mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    bool ignored;
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    return block_set_owned(&call->context, world, x, y, z, 0, &ignored, &call->child, &raw, error,
                           false);
}

bool mc_world_mutation_execution_owns_world(const mc_world_mutation_execution_call *call,
                                            const mc_entity_tick_execution_world_token *world) {
    if (call == NULL || world == NULL || call->close_done)
        return false;
    return mc_world_mutation_drop_owns_world(call->drop, world) ||
           mc_world_mutation_execution_owns_world(call->child, world) ||
           (call->chunk != NULL &&
            mc_world_mutation_chunk_execution_owns_world(call->chunk, world));
}

bool mc_world_mutation_execution_source_null(const mc_world_mutation_execution_call *call,
                                             mc_entity_tick_execution_site *site) {
    if (call == NULL || call->active || site == NULL)
        return false;
    if (mc_world_mutation_execution_source_null(call->child, site))
        return true;
    if (call->chunk == NULL || !call->chunk->source_null)
        return false;
    *site = call->chunk->source_null_site;
    return true;
}

const mc_world_notification_execution *
mc_world_mutation_execution_failure_observer(const mc_world_mutation_execution_call *call) {
    if (call == NULL || call->active)
        return NULL;
    const mc_world_notification_execution *child =
        mc_world_mutation_execution_failure_observer(call->child);
    if (child != NULL)
        return child;
    if (call->chunk != NULL) {
        child = mc_world_mutation_chunk_failure_observer(call->chunk);
        if (child != NULL)
            return child;
    }
    return call->observer.entered && !call->observer.completed ? &call->observer : NULL;
}

mc_world_storage_status
mc_world_mutation_execution_native_close(mc_world_mutation_execution_call *call) {
    if (call == NULL || call->close_done)
        return MC_WORLD_STORAGE_OK;
    if (call->active || !records_quiet(call->context.result))
        return MC_WORLD_STORAGE_BUSY;
    mc_world_storage_status status = mc_world_mutation_execution_native_close(call->child);
    if (status == MC_WORLD_STORAGE_OK && call->chunk != NULL)
        status = mc_world_mutation_chunk_native_close(call->chunk);
    if (status == MC_WORLD_STORAGE_OK)
        status = mc_world_mutation_drop_native_close(call->drop);
    call->close_done = status == MC_WORLD_STORAGE_OK;
    return status;
}

mc_world_storage_status
mc_world_mutation_execution_retire(mc_world_mutation_execution_call **slot) {
    if (slot == NULL || *slot == NULL)
        return MC_WORLD_STORAGE_OK;
    mc_world_mutation_execution_call *call = *slot;
    const mc_world_storage_status closed = mc_world_mutation_execution_native_close(call);
    if (closed != MC_WORLD_STORAGE_OK)
        return closed;
    const mc_world_storage_status child = mc_world_mutation_execution_retire(&call->child);
    if (child != MC_WORLD_STORAGE_OK)
        return child;
    const mc_world_storage_status drop = mc_world_mutation_drop_retire(&call->drop);
    if (drop != MC_WORLD_STORAGE_OK)
        return drop;
    free(call->chunk);
    free(call->support);
    free(call->plant);
    free(call);
    *slot = NULL;
    return MC_WORLD_STORAGE_OK;
}

bool mc_world_mutation_execution_support(mc_world_mutation_execution_call *call,
                                         mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                         bool *out, mc_world_storage_status *raw,
                                         const mc_world_solid_support_execution **detail,
                                         mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    if (call->support != NULL || out == NULL || raw == NULL || detail == NULL) {
        mc_world_runtime_execution_result_note(call->context.result, MC_WORLD_STORAGE_BUSY,
                                               "World support requires fresh caller work", site);
        return false;
    }
    call->support = calloc(1, sizeof(*call->support));
    if (call->support == NULL) {
        mc_world_runtime_execution_result_note(call->context.result,
                                               MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                               "World retained support frame", site);
        return false;
    }
    mc_world_solid_support_execution_init(call->support, call->context.execution,
                                          call->context.code_classes, call->context.code, site,
                                          call->context.result);
    const bool completed = mc_world_solid_support_detailed_held_in_execution(
        call->support, world, x, y, z, out, &call->support_status);
    call->support_returned = true;
    *raw = call->support_status;
    if (!completed) {
        *detail = call->support;
        return false;
    }
    free(call->support);
    call->support = NULL;
    call->support_returned = false;
    *detail = NULL;
    return true;
}

const mc_world_solid_support_execution *
mc_world_mutation_execution_failed_support(const mc_world_mutation_execution_call *call,
                                           mc_world_storage_status *raw) {
    if (call == NULL || call->active || raw == NULL)
        return NULL;
    const mc_world_solid_support_execution *child =
        mc_world_mutation_execution_failed_support(call->child, raw);
    if (child != NULL)
        return child;
    if (call->support == NULL || !call->support_returned)
        return NULL;
    *raw = call->support_status;
    return call->support;
}

bool mc_world_mutation_execution_plant_survives(mc_world_mutation_execution_call *call,
                                                mc_world_runtime *world, const mc_block *block,
                                                int32_t x, int32_t y, int32_t z, bool *out,
                                                mc_world_storage_status *raw,
                                                const mc_world_plant_execution **detail,
                                                mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    if (call->plant != NULL || out == NULL || raw == NULL || detail == NULL) {
        mc_world_runtime_execution_result_note(call->context.result, MC_WORLD_STORAGE_BUSY,
                                               "Plant survival requires fresh caller work", site);
        return false;
    }
    call->plant = calloc(1, sizeof(*call->plant));
    if (call->plant == NULL) {
        mc_world_runtime_execution_result_note(call->context.result,
                                               MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                               "World retained plant frame", site);
        return false;
    }
    mc_world_plant_execution_init(call->plant, call->context.execution, call->context.code_classes,
                                  call->context.code, call->context.world, site,
                                  call->context.result);
    const bool completed = mc_world_runtime_flower_survives_in_execution(
        call->plant, world, block, x, y, z, out, &call->plant_status);
    call->plant_returned = true;
    *raw = call->plant_status;
    if (!completed) {
        *detail = call->plant;
        return false;
    }
    free(call->plant);
    call->plant = NULL;
    call->plant_returned = false;
    *detail = NULL;
    return true;
}

const mc_world_plant_execution *
mc_world_mutation_execution_failed_plant(const mc_world_mutation_execution_call *call,
                                         mc_world_storage_status *raw) {
    if (call == NULL || call->active || raw == NULL)
        return NULL;
    const mc_world_plant_execution *child =
        mc_world_mutation_execution_failed_plant(call->child, raw);
    if (child != NULL)
        return child;
    if (call->plant == NULL || !call->plant_returned)
        return NULL;
    *raw = call->plant_status;
    return call->plant;
}

bool mc_world_mutation_execution_drop(mc_world_mutation_execution_call *call,
                                      mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, int32_t metadata,
                                      mc_world_mutation_error *error,
                                      mc_entity_tick_execution_site site) {
    if (!mc_world_mutation_execution_current(call, world, site))
        return false;
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    const bool completed = mc_world_mutation_drop_in_execution(
        &call->context, world, block, x, y, z, metadata, &call->drop, error, &raw);
    if (!completed)
        return false;
    const mc_world_storage_status status = mc_world_mutation_drop_retire(&call->drop);
    mc_world_runtime_execution_result_note(call->context.result, status,
                                           "Block completed drop retirement", site);
    return status == MC_WORLD_STORAGE_OK;
}

const mc_world_mutation_drop_execution *
mc_world_mutation_execution_failed_drop(const mc_world_mutation_execution_call *call) {
    if (call == NULL || call->active)
        return NULL;
    const mc_world_mutation_drop_execution *child =
        mc_world_mutation_execution_failed_drop(call->child);
    return child != NULL ? child : call->drop;
}
