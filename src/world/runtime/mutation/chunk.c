#include "chunk_execution_internal.h"
#include "mutation_internal.h"
#include "world_execution_internal.h"

#include "../../../block/registry/runtime_internal.h"
#include "../../chunk/object_internal.h"
#include "../../storage/objects_internal.h"
#include "../construction_internal.h"
#include "../execution_children_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/dimension/world_provider.h"

static bool pending(const mc_world_runtime_execution_result *owned) {
    if (owned->root_pending)
        return true;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (owned->pending[i].used)
            return true;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (owned->closes[i].used)
            return true;
    return false;
}

static bool same_token(const mc_entity_tick_execution_world_token *left,
                       const mc_entity_tick_execution_world_token *right) {
    return left->owner == right->owner && left->slot == right->slot &&
           left->generation == right->generation;
}

static bool current_token(const mc_world_mutation_chunk_execution *frame,
                          const mc_entity_tick_execution_world_token *expected) {
    mc_entity_tick_execution_world_token current;
    return mc_entity_tick_execution_current_world(frame->execution, &current) &&
           same_token(&current, expected);
}

static bool scope_phase(const mc_world_mutation_chunk_execution *frame,
                        mc_entity_tick_execution_world_slot_phase *out) {
    const mc_entity_tick_execution_world_token *token = &frame->scope.token;
    if (!frame->scope.created || token->owner != frame->execution || token->generation == 0 ||
        token->slot >= MC_ENTITY_TICK_EXECUTION_WORLDS)
        return false;
    const mc_entity_tick_execution_world_slot *slot = &frame->execution->world_slots[token->slot];
    if (slot->generation != token->generation)
        return false;
    *out = slot->phase;
    return true;
}

static mc_world_storage_status note(mc_world_mutation_chunk_execution *frame,
                                    mc_world_storage_status status, const char *operation,
                                    mc_entity_tick_execution_site site) {
    mc_world_runtime_execution_result_note(frame->owned, status, operation, site);
    return status;
}

mc_entity_tick_execution_site
mc_world_mutation_chunk_site(const mc_world_mutation_chunk_execution *frame,
                             mc_world_mutation_chunk_body body, unsigned int pc) {
    const char *method = body == MC_WORLD_MUTATION_CHUNK_ID       ? "a"
                         : body == MC_WORLD_MUTATION_CHUNK_HEIGHT ? "g"
                                                                  : "f";
    return (mc_entity_tick_execution_site){
        frame->code_classes->side == MC_WORLD_INFO_CLIENT ? "lm" : "hi", method,
        body == MC_WORLD_MUTATION_CHUNK_ID ? "(IIII)Z" : "(III)V", (int)pc};
}

void mc_world_mutation_chunk_null(mc_world_mutation_chunk_execution *frame,
                                  mc_world_mutation_chunk_body body, unsigned int pc) {
    frame->source_null = true;
    frame->source_null_site = mc_world_mutation_chunk_site(frame, body, pc);
}

bool mc_world_mutation_chunk_capture(mc_world_mutation_chunk_execution *frame,
                                     mc_world_runtime *world, mc_entity_tick_execution_site site) {
    if ((frame->scope.created && !frame->scope.consumed) ||
        !current_token(frame, &frame->initiating_token) || pending(frame->owned) ||
        frame->owned->selection_ready || frame->owned->math_failed ||
        frame->owned->ownership_status != MC_WORLD_STORAGE_OK) {
        (void)note(frame, MC_WORLD_STORAGE_BUSY, "Chunk setter prior World work", site);
        return false;
    }
    mc_entity_tick_execution_world_token token;
    const mc_world_storage_status status =
        mc_entity_tick_execution_push_world(frame->execution, world, site, &token);
    if (note(frame, status, "Chunk setter evaluated World capture", site) != MC_WORLD_STORAGE_OK)
        return false;
    frame->scope = (mc_world_mutation_chunk_scope){.token = token, .site = site, .created = true};
    ++frame->world_captures;
    return true;
}

static mc_world_storage_status pop_scope(mc_world_mutation_chunk_execution *frame) {
    mc_world_mutation_chunk_scope *scope = &frame->scope;
    const mc_world_storage_status status =
        mc_entity_tick_execution_pop_world(frame->execution, &scope->token, scope->failed);
    mc_entity_tick_execution_world_slot_phase phase;
    scope->consumed =
        scope_phase(frame, &phase) && (phase == MC_ENTITY_TICK_EXECUTION_WORLD_FREE ||
                                       phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED);
    (void)note(frame, status, "Chunk setter checked World close", scope->site);
    if (status == MC_WORLD_STORAGE_OK && !scope->consumed)
        return note(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "Chunk setter checked World close consumption", scope->site);
    return status;
}

bool mc_world_mutation_chunk_close(mc_world_mutation_chunk_execution *frame, bool completed) {
    mc_world_mutation_chunk_scope *scope = &frame->scope;
    if (completed &&
        (pending(frame->owned) || frame->owned->selection_ready || frame->owned->math_failed ||
         frame->owned->ownership_status != MC_WORLD_STORAGE_OK)) {
        (void)note(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "Chunk setter completed child result not quiet", scope->site);
        completed = false;
    }
    scope->attempted = true;
    scope->failed = !completed;
    scope->initial_close_status = pop_scope(frame);
    return scope->initial_close_status == MC_WORLD_STORAGE_OK && completed;
}

bool mc_world_mutation_chunk_execution_owns_world(
    const mc_world_mutation_chunk_execution *frame,
    const mc_entity_tick_execution_world_token *token) {
    mc_entity_tick_execution_world_slot_phase phase;
    return frame != NULL && frame->execution != NULL && token != NULL && !frame->scope.consumed &&
           same_token(&frame->scope.token, token) && scope_phase(frame, &phase) &&
           phase == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN;
}

const mc_world_notification_execution *
mc_world_mutation_chunk_failure_observer(const mc_world_mutation_chunk_execution *frame) {
    return frame != NULL && !frame->active && frame->height_observer.entered &&
                   !frame->height_observer.completed
               ? &frame->height_observer
               : NULL;
}

static mc_world_storage_status release_lease(mc_world_mutation_chunk_execution *frame) {
    if (frame->lease_held) {
        mc_world_storage_error cause = {0};
        const mc_world_storage_status status =
            mc_world_store_release_chunk(frame->store, frame->state, &cause);
        if (note(frame, status, cause.operation, frame->caller_site) != MC_WORLD_STORAGE_OK)
            return status;
        frame->lease_held = false;
    }
    if (frame->binding_held) {
        mc_world_runtime_call_leave(frame->binding_world);
        frame->binding_held = false;
    }
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status
mc_world_mutation_chunk_native_close(mc_world_mutation_chunk_execution *frame) {
    if (frame == NULL || frame->execution == NULL ||
        frame->execution->identity != frame->execution || frame->owned == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (frame->active || pending(frame->owned))
        return note(frame, MC_WORLD_STORAGE_BUSY, "Chunk setter Native close awaits child work",
                    frame->caller_site);
    mc_world_mutation_chunk_scope *scope = &frame->scope;
    if (scope->created && !scope->consumed) {
        mc_entity_tick_execution_world_slot_phase phase;
        if (!scope->attempted || !scope_phase(frame, &phase))
            return note(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "Chunk setter Native close lacks its recorded token", scope->site);
        if (phase == MC_ENTITY_TICK_EXECUTION_WORLD_OPEN) {
            if (!current_token(frame, &scope->token))
                return note(frame, MC_WORLD_STORAGE_BUSY,
                            "Chunk setter Native close awaits its exact current token",
                            scope->site);
            const mc_world_storage_status status = pop_scope(frame);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
        } else if (phase == MC_ENTITY_TICK_EXECUTION_WORLD_FREE ||
                   phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED)
            scope->consumed = true;
        else
            return note(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "Chunk setter Native close token phase", scope->site);
    }
    if ((frame->lease_held || frame->binding_held) &&
        !current_token(frame, &frame->initiating_token))
        return note(frame, MC_WORLD_STORAGE_BUSY,
                    "Chunk setter Native release awaits its initiating World", frame->caller_site);
    return release_lease(frame);
}

static bool initial(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z, int32_t *height,
                    int32_t *old, mc_world_mutation_error *error) {
    if (!mc_world_chunk_source_enabled(chunk))
        return mc_world_mutation_fail(NULL, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk mutation source is disabled");
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    const int32_t column = mc_java_i32_shl(z, 4) | x;
    error->chunk_bytecode = 14;
    if (fields.h == NULL || column < 0 || (uint32_t)column >= MC_CHUNK_HEIGHTMAP_BYTES)
        return mc_world_mutation_fail(fields.d, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk setter old height element");
    *height = (int32_t)fields.h[(size_t)column];

    (void)mc_world_chunk_fields_get(chunk, &fields);
    const int32_t cell = mc_java_i32_shl(x, 11) | mc_java_i32_shl(z, 7) | y;
    error->chunk_bytecode = 36;
    if (fields.b == NULL || cell < 0 || (uint32_t)cell >= MC_CHUNK_BLOCKS)
        return mc_world_mutation_fail(fields.d, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk setter old block element");
    *old = (int32_t)fields.b[(size_t)cell];
    return true;
}

static bool put(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z, int32_t id, unsigned int pc,
                mc_world_mutation_error *error) {
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    const int32_t cell = mc_java_i32_shl(x, 11) | mc_java_i32_shl(z, 7) | y;
    error->chunk_bytecode = pc;
    if (fields.b == NULL || cell < 0 || (uint32_t)cell >= MC_CHUNK_BLOCKS)
        return mc_world_mutation_fail(fields.d, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk setter block store");
    fields.b[(size_t)cell] = (uint8_t)((uint32_t)id & 255u);
    return true;
}

static bool metadata(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z, int32_t value,
                     unsigned int pc, mc_world_mutation_error *error) {
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    error->chunk_bytecode = pc;
    if (!mc_world_mutation_nibble_set(fields.e, x, y, z, value, error)) {
        if (fields.d != NULL)
            fields.d->last_error = error->cause;
        return false;
    }
    return true;
}

static bool hook(mc_world_chunk *chunk, int32_t id, int32_t x, int32_t y, int32_t z, bool added,
                 unsigned int capture_pc, unsigned int element_pc, unsigned int invoke_pc,
                 mc_world_mutation_error *error, mc_world_mutation_chunk_execution *execution) {
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    mc_world_runtime *world = execution == NULL ? fields.d : execution->initiating_world;
    mc_registry_classes *classes =
        execution == NULL ? mc_world_runtime_registry(world) : execution->registry;
    mc_registry_error cause = mc_world_mutation_empty_registry();
    mc_registry_block_m_view array;
    error->chunk_bytecode = capture_pc;
    if (classes == NULL) {
        cause.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        cause.message = "Chunk setter Registry binding";
        return mc_world_mutation_registry_fail(world, error, &cause, cause.message);
    }
    if (mc_registry_block_m_get(classes, &array, &cause) != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, error, &cause,
                                               "Chunk setter Block.m active use");
    const mc_block *block;
    error->chunk_bytecode = element_pc;
    if (mc_registry_block_m_at(&array, id, &block, &cause) != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, error, &cause,
                                               "Chunk setter Block.m element");
    (void)mc_world_chunk_fields_get(chunk, &fields);
    if (execution != NULL) {
        const unsigned int world_pc = added ? 245u : 109u;
        error->chunk_bytecode = world_pc;
        if (!mc_world_mutation_chunk_capture(
                execution, fields.d,
                mc_world_mutation_chunk_site(execution, MC_WORLD_MUTATION_CHUNK_ID, world_pc)))
            return false;
        error->chunk_bytecode = invoke_pc;
        const mc_entity_tick_execution_site site =
            mc_world_mutation_chunk_site(execution, MC_WORLD_MUTATION_CHUNK_ID, invoke_pc);
        if (block == NULL)
            mc_world_mutation_chunk_null(execution, MC_WORLD_MUTATION_CHUNK_ID, invoke_pc);
        const bool completed = added ? mc_world_mutation_block_added_in_execution(
                                           fields.d, block, x, y, z, error, execution->owned, site)
                                     : mc_world_mutation_block_removed_in_execution(
                                           fields.d, block, x, y, z, error, execution->owned, site);
        return mc_world_mutation_chunk_close(execution, completed);
    }
    error->chunk_bytecode = invoke_pc;
    return added ? mc_world_mutation_block_added(fields.d, block, x, y, z, error)
                 : mc_world_mutation_block_removed(fields.d, block, x, y, z, error);
}

static bool height(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z, int32_t id,
                   int32_t old_height, unsigned int capture_pc, unsigned int element_pc,
                   unsigned int raise_pc, unsigned int lower_pc, mc_world_mutation_error *error,
                   mc_world_mutation_chunk_execution *execution) {
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    const int32_t narrowed = (int32_t)((uint32_t)id & 255u);
    int32_t opacity;
    const bool opacity_read =
        execution == NULL
            ? mc_world_mutation_q(fields.d, narrowed, &error->chunk_bytecode, capture_pc,
                                  element_pc, &opacity, error)
            : mc_world_mutation_chunk_q(execution->initiating_world, execution->registry, narrowed,
                                        &error->chunk_bytecode, capture_pc, element_pc, &opacity,
                                        error);
    if (!opacity_read)
        return false;
    const bool raises = opacity != 0 && y >= old_height;
    if (!raises && !(opacity == 0 && y == mc_java_i32_sub(old_height, 1)))
        return true;
    error->chunk_bytecode = raises ? raise_pc : lower_pc;
    const int32_t requested = raises ? mc_java_i32_add(y, 1) : y;
    return execution == NULL
               ? mc_world_mutation_height(chunk, x, requested, z, error)
               : mc_world_mutation_height_in_execution(execution, chunk, x, requested, z, error);
}

static bool submit(mc_world_chunk *chunk, mc_light_kind kind, int32_t x, int32_t y, int32_t z,
                   unsigned int world_pc, unsigned int type_pc, unsigned int call_pc,
                   mc_world_mutation_error *error, mc_world_mutation_chunk_execution *execution) {
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    if (execution != NULL) {
        error->chunk_bytecode = world_pc;
        if (!mc_world_mutation_chunk_capture(
                execution, fields.d,
                mc_world_mutation_chunk_site(execution, MC_WORLD_MUTATION_CHUNK_ID, world_pc)))
            return false;
    }
    const mc_light_bounds bounds = {x, y, z, x, y, z};
    return mc_world_mutation_submit_scoped(execution, fields.d, kind, &bounds,
                                           MC_WORLD_MUTATION_CHUNK_ID, &error->chunk_bytecode,
                                           type_pc, call_pc, error);
}

static bool chunk_block(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z, int32_t id,
                        bool *out, mc_world_mutation_error *error,
                        mc_world_mutation_chunk_execution *execution) {
    int32_t old_height, old;
    if (!initial(chunk, x, y, z, &old_height, &old, error))
        return false;
    if (old == id) {
        error->chunk_bytecode = 0;
        *out = false;
        return true;
    }
    mc_world_chunk_fields fields;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    const int32_t world_x = mc_java_i32_add(mc_java_i32_mul(fields.j, 16), x);
    const int32_t world_z = mc_java_i32_add(mc_java_i32_mul(fields.k, 16), z);
    if (!put(chunk, x, y, z, id, 96, error))
        return false;
    if (old != 0 && !hook(chunk, old, world_x, y, world_z, false, 102, 107, 117, error, execution))
        return false;
    if (!metadata(chunk, x, y, z, 0, 128, error) ||
        !height(chunk, x, y, z, id, old_height, 131, 140, 156, 174, error, execution) ||
        !submit(chunk, MC_LIGHT_SKY, world_x, y, world_z, 178, 181, 194, error, execution) ||
        !submit(chunk, MC_LIGHT_BLOCK, world_x, y, world_z, 198, 201, 214, error, execution))
        return false;
    error->chunk_bytecode = 220;
    const bool column_completed =
        execution == NULL ? mc_world_mutation_column(chunk, x, z, error)
                          : mc_world_mutation_column_in_execution(execution, chunk, x, z, error);
    if (!column_completed)
        return false;
    if (id != 0) {
        (void)mc_world_chunk_fields_get(chunk, &fields);
        error->chunk_bytecode = 232;
        if (execution != NULL && fields.d == NULL) {
            mc_world_mutation_chunk_null(execution, MC_WORLD_MUTATION_CHUNK_ID, 232);
            return mc_world_mutation_fail(execution->initiating_world, error,
                                          MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Chunk setter current World remote field receiver");
        }
        if (!fields.d->fields.B &&
            !hook(chunk, id, world_x, y, world_z, true, 238, 243, 253, error, execution))
            return false;
    }
    mc_world_chunk_state(chunk)->dirty = true; /* o258 */
    error->chunk_bytecode = 0;
    *out = true;
    return true;
}

bool mc_world_mutation_chunk_block(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z,
                                   int32_t id, bool *out, mc_world_mutation_error *error) {
    return chunk_block(chunk, x, y, z, id, out, error, NULL);
}

void mc_world_mutation_chunk_execution_init(mc_world_mutation_chunk_execution *frame,
                                            mc_entity_tick_execution *execution,
                                            mc_world_runtime_execution_code code,
                                            mc_world_runtime_classes *code_classes,
                                            mc_registry_classes *registry,
                                            mc_entity_tick_execution_site caller_site,
                                            mc_entity_tick_execution_world_token initiating_token,
                                            mc_world_runtime_execution_result *owned) {
    if (frame != NULL)
        *frame = (mc_world_mutation_chunk_execution){.execution = execution,
                                                     .code = code,
                                                     .code_classes = code_classes,
                                                     .registry = registry,
                                                     .caller_site = caller_site,
                                                     .initiating_token = initiating_token,
                                                     .owned = owned};
}

static bool enter_chunk(mc_world_mutation_chunk_execution *frame, mc_world_runtime *world,
                        mc_world_store *store, mc_chunk_state *state, mc_world_chunk *chunk) {
    if (frame->owned->ownership_status != MC_WORLD_STORAGE_OK || frame->owned->selection_ready ||
        frame->owned->math_failed || pending(frame->owned) ||
        !current_token(frame, &frame->initiating_token)) {
        (void)note(frame, MC_WORLD_STORAGE_BUSY, "Chunk setter exact initiating World/result",
                   frame->caller_site);
        return false;
    }
    mc_world_access_failure_lease *unused = NULL;
    mc_world_storage_status status =
        mc_world_execution_child_lease(frame->execution, world, &unused);
    if (note(frame, status, "Chunk setter evaluated initiating World ownership",
             frame->caller_site) != MC_WORLD_STORAGE_OK)
        return false;

    mc_world_chunk_fields fields;
    if (world->options.store != store || state->blank ||
        mc_world_store_chunk_object(store, state) != chunk ||
        mc_world_chunk_fields_get(chunk, &fields) != MC_WORLD_STORAGE_OK ||
        (fields.d != NULL && (fields.d->construction.mode != MC_WORLD_RUNTIME_GAME ||
                              fields.d->construction.phase == MC_WORLD_RUNTIME_RETIRED ||
                              fields.d->fields.w != store))) {
        (void)note(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Chunk setter concrete Store binding",
                   frame->caller_site);
        return false;
    }

    mc_world_storage_error cause = {0};
    if (fields.d != NULL) {
        status = mc_world_runtime_call_enter(fields.d, &cause);
        if (note(frame, status, cause.operation, frame->caller_site) != MC_WORLD_STORAGE_OK)
            return false;
        frame->binding_held = true;
    }
    frame->binding_world = fields.d;
    frame->initiating_world = world;
    frame->store = store;
    frame->state = state;
    frame->chunk = chunk;
    status = mc_world_store_lease_chunk(store, state, &cause);
    if (note(frame, status, cause.operation, frame->caller_site) != MC_WORLD_STORAGE_OK) {
        (void)release_lease(frame);
        return false;
    }
    frame->lease_held = true;
    return true;
}

bool mc_world_mutation_chunk_block_in_execution(mc_world_mutation_chunk_execution *frame,
                                                mc_world_runtime *world, mc_world_store *store,
                                                mc_chunk_state *state, mc_world_chunk *chunk,
                                                int32_t x, int32_t y, int32_t z, int32_t id,
                                                bool *out, mc_world_mutation_error *error,
                                                mc_world_storage_status *raw) {
    if (frame == NULL)
        return false;
    if (frame->execution == NULL || frame->execution->identity != frame->execution ||
        frame->owned == NULL || frame->registry == NULL || frame->code_classes == NULL ||
        frame->entered || frame->active || world == NULL || store == NULL || state == NULL ||
        chunk == NULL || out == NULL || error == NULL || raw == NULL) {
        (void)note(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Chunk setter execution arguments",
                   frame->caller_site);
        return false;
    }
    if (!enter_chunk(frame, world, store, state, chunk))
        return false;

    frame->entered = frame->active = true;
    bool changed = false;
    const bool returned = chunk_block(chunk, x, y, z, id, &changed, error, frame);
    frame->source_returned = returned;
    frame->changed = returned && changed;
    *raw = returned ? MC_WORLD_STORAGE_OK : error->status;
    if ((!frame->scope.created || frame->scope.consumed) && !pending(frame->owned)) {
        if (current_token(frame, &frame->initiating_token))
            (void)release_lease(frame);
        else
            (void)note(frame, MC_WORLD_STORAGE_BUSY,
                       "Chunk setter release awaits its initiating World", frame->caller_site);
    }
    frame->active = false;
    frame->completed = returned && !frame->lease_held && !frame->binding_held &&
                       frame->owned->ownership_status == MC_WORLD_STORAGE_OK;
    if (frame->completed)
        *out = changed;
    return frame->completed;
}

bool mc_world_mutation_chunk_block_metadata(mc_world_chunk *chunk, int32_t x, int32_t y, int32_t z,
                                            int32_t id, int32_t value, bool *out,
                                            mc_world_mutation_error *error) {
    int32_t old_height, old;
    if (!initial(chunk, x, y, z, &old_height, &old, error))
        return false;
    mc_world_chunk_fields fields;
    if (old == id) {
        int32_t previous;
        (void)mc_world_chunk_fields_get(chunk, &fields);
        error->chunk_bytecode = 57;
        if (!mc_world_mutation_nibble_get(fields.e, x, y, z, &previous, error)) {
            fields.d->last_error = error->cause;
            return false;
        }
        if (previous == value) {
            error->chunk_bytecode = 0;
            *out = false;
            return true;
        }
    }
    (void)mc_world_chunk_fields_get(chunk, &fields);
    const int32_t world_x = mc_java_i32_add(mc_java_i32_mul(fields.j, 16), x);
    const int32_t world_z = mc_java_i32_add(mc_java_i32_mul(fields.k, 16), z);
    if (!put(chunk, x, y, z, id, 111, error))
        return false;
    if (old != 0) {
        (void)mc_world_chunk_fields_get(chunk, &fields);
        error->chunk_bytecode = 121;
        if (!fields.d->fields.B &&
            !hook(chunk, old, world_x, y, world_z, false, 127, 132, 142, error, NULL))
            return false;
    }
    if (!metadata(chunk, x, y, z, value, 154, error))
        return false;
    (void)mc_world_chunk_fields_get(chunk, &fields);
    mc_world_provider_fields dimension;
    error->chunk_bytecode = 164;
    if (mc_world_provider_fields_get(fields.d->fields.t, &dimension) != MC_WORLD_PROVIDER_OK)
        return mc_world_mutation_fail(fields.d, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Chunk setter current Dimension");
    if (!dimension.e &&
        (!height(chunk, x, y, z, id, old_height, 170, 179, 195, 213, error, NULL) ||
         !submit(chunk, MC_LIGHT_SKY, world_x, y, world_z, 217, 220, 233, error, NULL)))
        return false;
    if (!submit(chunk, MC_LIGHT_BLOCK, world_x, y, world_z, 237, 240, 253, error, NULL))
        return false;
    error->chunk_bytecode = 259;
    if (!mc_world_mutation_column(chunk, x, z, error) ||
        !metadata(chunk, x, y, z, value, 271, error))
        return false;
    if (id != 0 && !hook(chunk, id, world_x, y, world_z, true, 279, 284, 294, error, NULL))
        return false;
    mc_world_chunk_state(chunk)->dirty = true; /* o299 */
    error->chunk_bytecode = 0;
    *out = true;
    return true;
}
