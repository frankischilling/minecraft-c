#include "tick_internal.h"

static mc_arrow_tick_status entity_status(mc_entity_status status) {
    return status == MC_ENTITY_LIMIT               ? MC_ARROW_TICK_LIMIT
           : status == MC_ENTITY_ALLOCATION_FAILED ? MC_ARROW_TICK_ALLOCATION
           : status == MC_ENTITY_BUSY              ? MC_ARROW_TICK_BUSY
                                                   : MC_ARROW_TICK_CHILD_FAILED;
}

static mc_arrow_tick_status world_status(mc_world_storage_status status) {
    return status == MC_WORLD_STORAGE_LIMIT               ? MC_ARROW_TICK_LIMIT
           : status == MC_WORLD_STORAGE_ALLOCATION_FAILED ? MC_ARROW_TICK_ALLOCATION
           : status == MC_WORLD_STORAGE_BUSY              ? MC_ARROW_TICK_BUSY
                                                          : MC_ARROW_TICK_CHILD_FAILED;
}

void mc_arrow_tick_at(mc_arrow_tick_call *c, unsigned int pc) {
    c->state.source_pc = pc;
}

void mc_arrow_tick_select(mc_arrow_tick_call *c, mc_arrow_tick_child child, unsigned int pc) {
    mc_arrow_tick_at(c, pc);
    c->work_tag = child;
    mc_math_execution_clear_current(&c->math);
    if (child == MC_ARROW_TICK_CHILD_BASE)
        mc_entity_tick_error_init(&c->work.base);
    else if (child == MC_ARROW_TICK_CHILD_BLOCK)
        mc_world_collision_error_init(&c->work.block);
    else if (child == MC_ARROW_TICK_CHILD_NEARBY)
        mc_world_nearby_error_init(&c->work.nearby);
    else if (child == MC_ARROW_TICK_CHILD_EFFECT)
        mc_entity_effect_error_init(&c->work.effect);
    else if (child == MC_ARROW_TICK_CHILD_ENVIRONMENT)
        mc_entity_environment_error_init(&c->work.environment);
    else if (child == MC_ARROW_TICK_CHILD_SOUND)
        mc_world_movement_error_init(&c->work.sound);
    else if (child == MC_ARROW_TICK_CHILD_PARTICLE)
        mc_world_mutation_error_init(&c->work.particle);
}

/* Exact known classes only. An arbitrary class string does not prove its
 * Throwable superclass. Typed Registry classification bypasses this table. */
static mc_arrow_tick_failure_kind named_kind(const char *name) {
    if (name == NULL)
        return MC_ARROW_TICK_FAILURE_UNRESOLVED;
    static const char *const exceptions[] = {"java/lang/NullPointerException",
                                             "java/lang/ClassCastException",
                                             "java/lang/IllegalStateException",
                                             "java/lang/IllegalArgumentException",
                                             "java/lang/IndexOutOfBoundsException",
                                             "java/lang/ArrayIndexOutOfBoundsException",
                                             "java/io/IOException",
                                             "java/io/EOFException",
                                             "java/io/UTFDataFormatException"};
    for (size_t i = 0; i < sizeof(exceptions) / sizeof(*exceptions); ++i)
        if (strcmp(name, exceptions[i]) == 0)
            return MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION;
    static const char *const errors[] = {"java/lang/ExceptionInInitializerError",
                                         "java/lang/NoClassDefFoundError"};
    for (size_t i = 0; i < sizeof(errors) / sizeof(*errors); ++i)
        if (strcmp(name, errors[i]) == 0)
            return MC_ARROW_TICK_FAILURE_SOURCE_ERROR;
    return MC_ARROW_TICK_FAILURE_UNRESOLVED;
}

bool mc_arrow_tick_fail_classified(mc_arrow_tick_call *c, mc_arrow_tick_status status,
                                   const char *operation, mc_arrow_tick_failure_kind kind,
                                   const char *exception) {
    if (!c->failed) {
        c->failed = true;
        c->error.status = status;
        c->error.operation = operation;
        c->error.classification = kind;
        c->error.native_policy = kind == MC_ARROW_TICK_FAILURE_NATIVE;
        c->error.exception_class = exception;
        c->error.source_error = kind == MC_ARROW_TICK_FAILURE_SOURCE_ERROR;
        c->error.source_exception = kind == MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION;
        c->error.site = (mc_arrow_site){"sl", "w_", "()V", (int32_t)c->state.source_pc};
        c->error.child = c->work_tag;
        /* Latch only. A callee B can still be current; never pop ambient state. */
    }
    return false;
}

bool mc_arrow_tick_fail(mc_arrow_tick_call *c, mc_arrow_tick_status status, const char *operation,
                        bool native, const char *exception) {
    return mc_arrow_tick_fail_classified(
        c, status, operation, native ? MC_ARROW_TICK_FAILURE_NATIVE : named_kind(exception),
        exception);
}

bool mc_arrow_tick_null(mc_arrow_tick_call *c, const char *operation) {
    return mc_arrow_tick_fail(c, MC_ARROW_TICK_SOURCE_EXCEPTION, operation, false,
                              "java/lang/NullPointerException");
}

bool mc_arrow_tick_fp(mc_arrow_tick_call *c, unsigned int pc) {
    mc_arrow_tick_at(c, pc);
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           mc_arrow_tick_fail(c, MC_ARROW_TICK_STATE, "Arrow reached strict floating environment",
                              true, NULL);
}

bool mc_arrow_tick_math_root(mc_arrow_tick_call *c, double value, unsigned int pc, float *out) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_MATH, pc);
    const mc_math_class_status status = mc_math_execution_require(
        &c->math, (mc_math_execution_site){"sl", "w_", "()V", (int)pc}, "a", "(D)F");
    if (status != MC_MATH_CLASS_OK) {
        c->error.math = mc_math_execution_cause(&c->math);
        c->error.child_site =
            (mc_arrow_site){c->math.first.child.class_name, c->math.first.child.method,
                            c->math.first.child.descriptor, c->math.first.child.pc};
        return mc_arrow_tick_fail(c,
                                  status == MC_MATH_CLASS_LIMIT ? MC_ARROW_TICK_LIMIT
                                  : status == MC_MATH_CLASS_ALLOCATION_FAILED
                                      ? MC_ARROW_TICK_ALLOCATION
                                  : status == MC_MATH_CLASS_BUSY ? MC_ARROW_TICK_BUSY
                                                                 : MC_ARROW_TICK_CHILD_FAILED,
                                  c->error.math->operation, true, NULL);
    }
    if (!mc_arrow_tick_fp(c, pc))
        return false;
    *out = at_root(value); /* Actual IN.a: double sqrt, then float narrowing. */
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    return true;
}

bool mc_arrow_tick_random(mc_arrow_tick_call *c, unsigned int pc, float *out) {
    mc_arrow_tick_at(c, pc);
    mc_java_random *const random = c->state.receiver->random; /* A fresh bs load. */
    if (random == NULL)
        return mc_arrow_tick_null(c, "Arrow null private Random receiver");
    *out = mc_java_random_next_float(random);
    if (c->state.random_calls != UINT_MAX)
        ++c->state.random_calls;
    return true;
}

bool mc_arrow_tick_world_begin(mc_arrow_tick_call *c, mc_world_runtime *world, unsigned int pc) {
    mc_arrow_tick_at(c, pc);
    if (c->direct_world_open || c->execution.world_open || c->query_live) {
        c->work_tag = MC_ARROW_TICK_CHILD_OWNER;
        c->error.execution = &c->execution;
        return mc_arrow_tick_fail(c, MC_ARROW_TICK_BUSY,
                                  "Arrow prior evaluated World/result is not drained", true, NULL);
    }
    const mc_world_storage_status status = mc_entity_tick_execution_push_world(
        &c->execution, world, (mc_entity_tick_execution_site){"sl", "w_", "()V", (int)pc},
        &c->direct_world);
    if (status != MC_WORLD_STORAGE_OK) {
        c->work_tag = MC_ARROW_TICK_CHILD_OWNER;
        c->error.execution = &c->execution;
        return mc_arrow_tick_fail(c, world_status(status),
                                  "Arrow actual evaluated World ownership admission", true, NULL);
    }
    c->direct_world_open = true;
    c->direct_world_site = (mc_entity_tick_execution_site){"sl", "w_", "()V", (int)pc};
    c->query_unwind = MC_ARROW_TICK_UNWIND_OWN_CLOSE;
    mc_world_runtime_execution_result_init(&c->query_result);
    c->query_invoked = false;
    c->query_status = MC_WORLD_STORAGE_OK; /* No return claimed until query_invoked. */
    memset(c->captured_worlds, 0, sizeof(c->captured_worlds));
    return true;
}

static bool registry_failed(mc_arrow_tick_call *c, const mc_registry_error *error) {
    mc_item_render_throwable kind = MC_ITEM_RENDER_NO_THROWABLE;
    const char *exception = NULL;
    mc_item_render_registry_classification(c->registry, error, &kind, &exception);
    c->error.registry = error;
    return mc_arrow_tick_fail_classified(
        c, error->status == MC_REGISTRY_LIMIT ? MC_ARROW_TICK_LIMIT : MC_ARROW_TICK_CHILD_FAILED,
        error->message,
        kind == MC_ITEM_RENDER_SOURCE_ERROR       ? MC_ARROW_TICK_FAILURE_SOURCE_ERROR
        : kind == MC_ITEM_RENDER_SOURCE_EXCEPTION ? MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION
                                                  : MC_ARROW_TICK_FAILURE_NATIVE,
        exception);
}

bool mc_arrow_tick_literal(mc_arrow_tick_call *c, const char *text, unsigned int pc,
                           const mc_registry_string **out) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_REGISTRY, pc);
    const mc_registry_status status =
        mc_registry_runtime_literal_ascii(c->registry, text, out, &c->registry_error);
    if (status != MC_REGISTRY_OK)
        return registry_failed(c, &c->registry_error);
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    return true;
}

static bool same_storage_cause(const mc_world_storage_error *a, const mc_world_storage_error *b) {
    return a->status == b->status && a->operation == b->operation &&
           a->system_error == b->system_error;
}

/* The active typed arm is chosen BEFORE this physical protection check.
 * Inspect only real owned nodes and compare all tuple VALUES. Never follow a
 * returned receipt pointer until its exact protected tuple has been found. */
static bool protected_exception(mc_arrow_tick_call *c, const mc_world_access_failure *active,
                                const mc_world_storage_error *active_cause, const char **name) {
    if (active->source_null_receiver)
        return active->receiver == NULL && active->receipt == NULL &&
               active->render_global == NULL &&
               mc_world_access_failure_source_exception(active, name);
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_RECEIPTS; ++i) {
        const mc_world_access_failure *owned = NULL;
        const mc_world_access_failure_lease *node = &c->execution.leases[i];
        if (c->execution.lease_used[i] && mc_world_access_failure_lease_inspect(node, &owned) &&
            owned->receiver == active->receiver && owned->receipt == active->receipt &&
            owned->render_global == active->render_global &&
            owned->source_null_receiver == active->source_null_receiver) {
            const mc_world_storage_error callback =
                mc_world_access_callback_error(node->callback_status, node->callback_error);
            if (same_storage_cause(&callback, active_cause))
                return mc_world_access_failure_source_exception(owned, name);
        }
    }
    return false;
}

/* Base's outer projection may still be OK after its result-first SDK stop.
 * Select ONLY the actual reached/returned child and freshly supplied typed
 * frame, then prove its exact callback tuple/cause under a real owned lease.
 * No raw status, last-leaf field or ambient admission selects Source failure. */
bool mc_arrow_tick_base_source_failure(mc_arrow_tick_call *c, const char **operation,
                                       const char **exception) {
    if (c->work.base.source_exception && c->work.base.exception_class != NULL) {
        *operation = c->work.base.operation;
        *exception = c->work.base.exception_class;
        return true;
    }
    if (!c->base_execution.child_returned || c->base_execution.child_status == MC_WORLD_STORAGE_OK)
        return false;
    if (c->base_execution.child == MC_TICK_ARROW_BASE_SOUND &&
        c->work.base.world.status == c->base_execution.child_status &&
        protected_exception(c, &c->work.base.world.world_access_failure, &c->work.base.world.cause,
                            exception)) {
        *operation = c->work.base.world.operation;
        c->error.child_site = (mc_arrow_site){"fd", "a", "(Lsn;Ljava/lang/String;FF)V",
                                              (int32_t)c->work.base.world.bytecode};
        return true;
    }
    if (c->base_execution.child == MC_TICK_ARROW_BASE_PARTICLE &&
        c->work.base.world.presentation.status == c->base_execution.child_status &&
        protected_exception(c, &c->work.base.world.presentation.world_access_failure,
                            &c->work.base.world.presentation.cause, exception)) {
        *operation = c->work.base.world.presentation.operation;
        c->error.child_site =
            (mc_arrow_site){"fd", "a", "(Ljava/lang/String;DDDDDD)V",
                            (int32_t)c->work.base.world.presentation.world_access_bytecode};
        return true;
    }
    return false;
}

/* Explicit reached Chunk Entity invoke42 -> batch World34 -> WA29. Fresh
 * projections and exact first-cause identity select this ONE active tuple;
 * presence of some other receipt arm never selects a Throwable. Other Tile,
 * import/generation and opaque producer arms remain typed but UNRESOLVED. */
static mc_arrow_tick_failure_kind nearby_classification(mc_arrow_tick_call *c,
                                                        const mc_world_nearby_error *error,
                                                        const char **name) {
    const mc_world_entity_error *publication = &error->batch.publication;
    if (error->query.acquisition != MC_CHUNK_ACQUIRE_OK && error->activation_bytecode == 42 &&
        error->activation_world_bytecode == 34 && error->batch.index != SIZE_MAX &&
        publication->world_access_bytecode == 29 && publication->status != MC_WORLD_STORAGE_OK &&
        same_storage_cause(&publication->cause, &error->query.cause) &&
        protected_exception(c, &publication->world_access_failure, &publication->cause, name))
        return named_kind(*name);
    return MC_ARROW_TICK_FAILURE_UNRESOLVED;
}

static bool query_records_pending(const mc_world_runtime_execution_result *result) {
    if (result->root_pending)
        return true;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (result->pending[i].used)
            return true;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (result->closes[i].used)
            return true;
    return false;
}

static bool world_end(mc_arrow_tick_call *c) {
    if (!c->direct_world_open || (c->query_live && query_records_pending(&c->query_result))) {
        c->error.execution = &c->execution;
        c->error.query = c->query_live ? &c->query_result : NULL;
        return mc_arrow_tick_fail(c, MC_ARROW_TICK_CLEANUP_FAILED,
                                  "Arrow direct scope has unconsumed explicit query records", true,
                                  NULL);
    }
    const mc_world_storage_status status =
        mc_entity_tick_execution_pop_world(&c->execution, &c->direct_world, false);
    if (status != MC_WORLD_STORAGE_OK)
        mc_arrow_tick_own_pop_returned(c, status);
    if (c->query_live)
        mc_world_runtime_execution_result_note(
            &c->query_result, status, "Arrow scalar query own scope checked close",
            (mc_entity_tick_execution_site){"sl", "w_", "()V", (int)c->state.source_pc});
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.execution = &c->execution;
        c->error.query = c->query_live ? &c->query_result : NULL;
        return mc_arrow_tick_fail(c, MC_ARROW_TICK_CLEANUP_FAILED,
                                  "Arrow own evaluated scope checked close", true, NULL);
    }
    c->direct_world_open = false;
    c->query_live = false;
    c->query_unwind = MC_ARROW_TICK_UNWIND_QUIESCENT;
    return true;
}

static bool root_ioexception(const mc_world_source_chunk_error *error) {
    const mc_world_source_error *source = &error->source;
    return source->status == MC_WORLD_SOURCE_SOURCE_EXCEPTION && source->source_exception &&
           !source->native_policy && source->frame_count == 1 &&
           source->frames[0].class_name != NULL && source->frames[0].method != NULL &&
           source->frames[0].descriptor != NULL &&
           strcmp(source->frames[0].class_name, "ld") == 0 &&
           strcmp(source->frames[0].method, "a") == 0 &&
           strcmp(source->frames[0].descriptor, "(Lfd;II)Llm;") == 0 &&
           source->frames[0].pc == 18 && error->storage.status == MC_WORLD_STORAGE_INVALID_DATA &&
           error->input.source_fault == MC_NBT_SOURCE_ROOT &&
           error->input.parse.status == MC_NBT_ROOT_TYPE && error->input.source_class != NULL &&
           error->input.source_method != NULL && strcmp(error->input.source_class, "as") == 0 &&
           strcmp(error->input.source_method, "a(Ljava/io/DataInput;)Lnu;") == 0 &&
           error->input.source_pc == 23;
}

static mc_arrow_tick_failure_kind query_classification(mc_arrow_tick_call *c,
                                                       const mc_world_nearby_error *lower,
                                                       const char **exception) {
    if (c->query_result.selection_ready) {
        mc_entity_tick_execution_loader_view selected = {0};
        if (!mc_entity_tick_execution_loader_read(&c->execution, &c->query_result.selection,
                                                  &selected) ||
            selected.status == MC_CHUNK_ACQUIRE_OK)
            return MC_ARROW_TICK_FAILURE_UNRESOLVED;
        if (selected.failure != NULL) {
            /* Explicit actual selected loader, never an ambient last error or
             * a different World's direct pointer. Child status need not equal
             * the enclosing acquisition status. Match its real current cause. */
            if (lower->query.source_chunk != selected.failure ||
                !same_storage_cause(&selected.failure->storage, &lower->query.cause))
                return MC_ARROW_TICK_FAILURE_UNRESOLVED;
            c->error.loader = selected.failure;
            if (root_ioexception(selected.failure)) {
                *exception = "java/io/IOException"; /* Verified original as constructor23. */
                c->error.child_site = (mc_arrow_site){"as", "a", "(Ljava/io/DataInput;)Lnu;", 23};
                return MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION;
            }
            if (selected.failure->registry.status != MC_REGISTRY_OK) {
                mc_world_runtime *producer = NULL;
                if (!mc_entity_tick_execution_read_world(
                        &c->execution, &c->query_result.selection.world, &producer) ||
                    producer == NULL || producer->classes == NULL)
                    return MC_ARROW_TICK_FAILURE_UNRESOLVED;
                mc_item_render_throwable kind = MC_ITEM_RENDER_NO_THROWABLE;
                /* The explicit selected loader's actual World supplies its
                 * retained declaring Registry, never Arrow.world or a search
                 * for a convenient namespace. This is pure typed inspection. */
                mc_item_render_registry_classification(
                    producer->classes->registry, &selected.failure->registry, &kind, exception);
                if (kind != MC_ITEM_RENDER_NO_THROWABLE) {
                    c->error.registry = &selected.failure->registry;
                    return kind == MC_ITEM_RENDER_SOURCE_ERROR
                               ? MC_ARROW_TICK_FAILURE_SOURCE_ERROR
                               : MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION;
                }
                return MC_ARROW_TICK_FAILURE_UNRESOLVED;
            }
            if (selected.failure->source.source_exception &&
                !selected.failure->source.native_policy &&
                selected.failure->source.exception_class != NULL) {
                *exception = selected.failure->source.exception_class;
                return named_kind(*exception);
            }
            /* Exact selected loader Native policy is explicit, not inferred
             * from INVALID_DATA or a classless source parse/compression code. */
            return selected.failure->source.native_policy &&
                           !selected.failure->source.source_exception
                       ? MC_ARROW_TICK_FAILURE_NATIVE
                       : MC_ARROW_TICK_FAILURE_UNRESOLVED;
        }
    }
    const mc_arrow_tick_failure_kind active = nearby_classification(c, lower, exception);
    if (active != MC_ARROW_TICK_FAILURE_UNRESOLVED || *exception != NULL)
        return active;
    /* Known owned native preflight / actual successful GET bookkeeping arm.
     * A reached unclassified Source failure stays UNRESOLVED independently. */
    if (c->query_result.ownership_status != MC_WORLD_STORAGE_OK &&
        lower->query.acquisition == MC_CHUNK_ACQUIRE_OK)
        return MC_ARROW_TICK_FAILURE_NATIVE;
    return MC_ARROW_TICK_FAILURE_UNRESOLVED;
}

bool mc_arrow_tick_block_value(mc_arrow_tick_call *c, mc_world_runtime *world, int32_t x, int32_t y,
                               int32_t z, bool metadata, unsigned int pc, int32_t *out) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_BLOCK, pc);
    c->query_live = true;
    c->error.query = &c->query_result;
    c->error.execution = &c->execution;
    if (world == NULL)
        return mc_arrow_tick_null(c, "Arrow null block-value World receiver");
    c->query_invoked = true;
    c->query_status = mc_world_collision_block_value_in_execution(
        &c->execution, world, c->query_code, x, y, z, metadata, out, &c->work.block,
        &c->query_result);
    if (c->query_status != MC_WORLD_STORAGE_OK ||
        c->query_result.ownership_status != MC_WORLD_STORAGE_OK) {
        c->error.block = &c->work.block;
        c->error.child_site = (mc_arrow_site){"fd", metadata ? "e" : "a", "(III)I",
                                              (int32_t)c->work.block.nearby.query.bytecode};
        const char *exception = NULL;
        const mc_arrow_tick_failure_kind kind =
            query_classification(c, &c->work.block.nearby, &exception);
        const mc_arrow_tick_status status =
            world_status(c->query_status != MC_WORLD_STORAGE_OK ? c->query_status
                                                                : c->query_result.ownership_status);
        return mc_arrow_tick_fail_classified(c, status, "Arrow actual World block value", kind,
                                             exception);
    }
    if (!world_end(c))
        return false;
    c->error.query = NULL;
    c->error.execution = NULL;
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    return true;
}

bool mc_arrow_tick_block_get(mc_arrow_tick_call *c, int32_t id, unsigned int pc,
                             const mc_block **out) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_REGISTRY, pc);
    const mc_registry_status status =
        mc_registry_block_get(c->registry, id, out, &c->registry_error);
    if (status != MC_REGISTRY_OK)
        return registry_failed(c, &c->registry_error);
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    return true;
}

bool mc_arrow_tick_block_shape(mc_arrow_tick_call *c, mc_world_runtime *world,
                               const mc_block *block, int32_t x, int32_t y, int32_t z,
                               bool collision_box, unsigned int pc) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_BLOCK, pc);
    if (block == NULL)
        return mc_arrow_tick_null(c, "Arrow null Block virtual receiver");
    c->query_live = true;
    c->error.query = &c->query_result;
    c->error.execution = &c->execution;
    c->query_invoked = true; /* Actual C child invocation, not a Source-return witness. */
    /* Real retained defining graphs of this client caller, not World/Chunk fields. */
    const mc_world_collision_block_execution_code code = {
        c->query_code, c->registry, c->code_classes, {"lm", "d", "(III)Low;", 67}};
    const bool completed =
        collision_box ? mc_world_collision_block_box_in_classes_in_execution(
                            &c->execution, c->code_classes, world, code, block, x, y, z,
                            &c->block_box, &c->work.block, &c->query_status, &c->query_result)
                      : mc_world_collision_block_state_shape_in_classes_in_execution(
                            &c->execution, c->code_classes, world, code, block, x, y, z,
                            &c->work.block, &c->query_status, &c->query_result);
    if (!completed) {
        c->error.block = &c->work.block;
        c->error.child_site = (mc_arrow_site){c->work.block.block_class, collision_box ? "e" : "a",
                                              collision_box ? "(Lfd;III)Leq;" : "(Lxp;III)V",
                                              (int32_t)c->work.block.block_bytecode};
        const char *exception = NULL;
        const mc_arrow_tick_failure_kind kind =
            query_classification(c, &c->work.block.nearby, &exception);
        const mc_world_storage_status status = c->query_status != MC_WORLD_STORAGE_OK
                                                   ? c->query_status
                                                   : c->query_result.ownership_status;
        return mc_arrow_tick_fail_classified(c, world_status(status),
                                             "Arrow actual Block shape child", kind, exception);
    }
    if (collision_box && c->block_box != NULL)
        c->block_box_classes = c->code_classes->entities; /* Actual getter factory owner. */
    if (!world_end(c))
        return false;
    c->error.query = NULL;
    c->error.execution = NULL;
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    return true;
}

bool mc_arrow_tick_vector_drop(mc_arrow_tick_call *c, mc_entity_vector **slot) {
    if (*slot == NULL)
        return true;
    const mc_entity_status status = mc_entity_vector_release(c->state.receiver->classes, *slot);
    if (status != MC_ENTITY_OK) {
        c->work_tag = MC_ARROW_TICK_CHILD_VECTOR;
        c->entity_error = (mc_entity_error){.status = status,
                                            .operation = "Arrow actual vector occurrence release"};
        c->error.entity = &c->entity_error;
        return mc_arrow_tick_fail(c, MC_ARROW_TICK_CLEANUP_FAILED, c->entity_error.operation, true,
                                  NULL);
    }
    *slot = NULL;
    return true;
}

bool mc_arrow_tick_vector_new(mc_arrow_tick_call *c, double x, double y, double z, unsigned int pc,
                              mc_entity_vector **slot) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_VECTOR, pc);
    mc_entity_vector *next = NULL;
    const mc_entity_status status =
        mc_entity_vector_pool_get(c->state.receiver->classes, x, y, z, &next, &c->vector_error);
    if (status != MC_ENTITY_OK) {
        c->error.vector = &c->vector_error;
        return mc_arrow_tick_fail(c, entity_status(status), c->vector_error.operation, true, NULL);
    }
    c->replaced_vector = *slot;
    *slot = next; /* Actual source local/operand replacement before native cleanup. */
    if (!mc_arrow_tick_vector_drop(c, &c->replaced_vector))
        return false;
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    return true;
}

bool mc_arrow_tick_bounds_drop(mc_arrow_tick_call *c, mc_entity_bounds **slot) {
    if (*slot == NULL)
        return true;
    mc_entity_classes *owner =
        slot == &c->block_box ? c->block_box_classes : c->state.receiver->classes;
    const mc_entity_status status = mc_entity_bounds_release(owner, *slot);
    if (status != MC_ENTITY_OK) {
        c->work_tag = MC_ARROW_TICK_CHILD_BOUNDS;
        c->entity_error = (mc_entity_error){.status = status,
                                            .operation = "Arrow actual bounds occurrence release"};
        c->error.entity = &c->entity_error;
        return mc_arrow_tick_fail(c, MC_ARROW_TICK_CLEANUP_FAILED, c->entity_error.operation, true,
                                  NULL);
    }
    *slot = NULL;
    return true;
}

bool mc_arrow_tick_entity_drop(mc_entity **slot) {
    mc_entity *value = *slot;
    *slot = NULL;
    mc_entity_release(value);
    return true;
}

bool mc_arrow_tick_hit_drop(mc_arrow_tick_call *c, mc_entity_hit_result **slot) {
    if (mc_entity_hit_result_release(slot, &c->hit_cleanup) == MC_ENTITY_HIT_OK)
        return true;
    c->work_tag = MC_ARROW_TICK_CHILD_HIT;
    c->error.hit = &c->hit_cleanup;
    return mc_arrow_tick_fail(c, MC_ARROW_TICK_CLEANUP_FAILED, "Arrow actual VF occurrence release",
                              true, NULL);
}

/* Only these exact real static Ray names are normalized. Arbitrary names
 * retain their original text and unresolved Throwable classification. */
static const char *ray_known_exception(const char *name) {
    if (name == NULL)
        return NULL;
    if (strcmp(name, "java.lang.NullPointerException") == 0)
        return "java/lang/NullPointerException";
    if (strcmp(name, "java.lang.ArrayIndexOutOfBoundsException") == 0)
        return "java/lang/ArrayIndexOutOfBoundsException";
    return name;
}

bool mc_arrow_tick_ray(mc_arrow_tick_call *c, mc_world_runtime *world) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_RAY, 451);
    c->query_live = true;
    c->error.query = &c->query_result;
    c->error.execution = &c->execution;
    if (world == NULL)
        return mc_arrow_tick_null(c, "Arrow null captured World ray receiver");
    const mc_world_collision_block_execution_code code = {
        c->query_code, c->registry, c->code_classes, {"lm", "d", "(III)Low;", 67}};
    mc_world_raytrace_status status = MC_WORLD_RAYTRACE_OK; /* No return claimed yet. */
    c->query_invoked = true; /* Actual child invocation, not Source completion. */
    const bool completed = mc_world_raytrace_flags_begin_in_execution(
        &c->execution, c->code_classes, world, code, c->start, c->end, false, true, &c->ray,
        &status, &c->query_result);
    mc_world_raytrace_state view = {0};
    mc_world_raytrace_execution_state owned_view = {0};
    if (c->ray != NULL && (!mc_world_raytrace_inspect(c->ray, &view) ||
                           !mc_world_raytrace_execution_inspect(c->ray, &owned_view)))
        return mc_arrow_tick_fail(c, MC_ARROW_TICK_STATE, "Arrow actual owned ray view", true,
                                  NULL);
    if (!completed || !view.source_returned) {
        c->error.ray = view.failure;
        const char *exception = NULL;
        mc_arrow_tick_failure_kind kind = MC_ARROW_TICK_FAILURE_UNRESOLVED;
        const char *operation = "Arrow actual World ray native entry";
        if (view.failure != NULL) {
            c->error.child_site =
                (mc_arrow_site){view.failure->site.class_name, view.failure->site.method,
                                view.failure->site.descriptor, view.failure->site.pc};
            if (view.failure->operation != NULL)
                operation = view.failure->operation;
        }
        if (owned_view.math_failed) {
            c->ray_math_error = owned_view.math_error;
            c->error.math = &c->ray_math_error;
            c->error.child_site =
                (mc_arrow_site){owned_view.math_child.class_name, owned_view.math_child.method,
                                owned_view.math_child.descriptor, owned_view.math_child.pc};
            /* Explicit current callee outcome, never a test of old math.failed. */
            kind = MC_ARROW_TICK_FAILURE_NATIVE;
            operation = c->ray_math_error.operation;
        } else if (owned_view.query_failed && owned_view.collision != NULL) {
            /* Active scratch is Ray-owned, not the Arrow block union. Classify
             * the explicit selected loader/receipt before Ray retirement. */
            kind = query_classification(c, &owned_view.collision->nearby, &exception);
        } else if (view.failure != NULL && view.failure->registry != NULL) {
            mc_item_render_throwable registry_kind = MC_ITEM_RENDER_NO_THROWABLE;
            /* Copy actual typed VALUE before the Ray frame expires. */
            c->registry_error = *view.failure->registry;
            c->error.registry = &c->registry_error;
            mc_item_render_registry_classification(c->code_classes->registry, &c->registry_error,
                                                   &registry_kind, &exception);
            kind = registry_kind == MC_ITEM_RENDER_SOURCE_ERROR ? MC_ARROW_TICK_FAILURE_SOURCE_ERROR
                   : registry_kind == MC_ITEM_RENDER_SOURCE_EXCEPTION
                       ? MC_ARROW_TICK_FAILURE_SOURCE_EXCEPTION
                       : MC_ARROW_TICK_FAILURE_NATIVE;
        } else if (view.failure != NULL && view.failure->source_exception &&
                   !view.failure->native_policy) {
            exception = ray_known_exception(view.failure->exception_class);
            kind = named_kind(exception);
        } else if ((view.failure != NULL && view.failure->native_policy) ||
                   c->query_result.ownership_status != MC_WORLD_STORAGE_OK || c->ray == NULL) {
            kind = MC_ARROW_TICK_FAILURE_NATIVE;
        }
        if (c->query_result.ownership_status != MC_WORLD_STORAGE_OK &&
            (view.failure == NULL || view.failure->operation == NULL))
            operation = c->query_result.ownership_error.operation;
        return mc_arrow_tick_fail_classified(c, MC_ARROW_TICK_CHILD_FAILED, operation, kind,
                                             exception);
    }
    if (view.result != NULL) {
        const mc_entity_hit_status retained =
            mc_entity_hit_result_retain(view.result, &c->hit_error);
        if (retained != MC_ENTITY_HIT_OK) {
            c->error.hit = &c->hit_error;
            return mc_arrow_tick_fail(c, MC_ARROW_TICK_CHILD_FAILED,
                                      "Arrow returned ray VF occurrence", true, NULL);
        }
        c->hit = view.result;
    }
    const mc_world_raytrace_status retired = mc_world_raytrace_retire(&c->ray);
    if (retired != MC_WORLD_RAYTRACE_OK) {
        mc_world_raytrace_state remaining = {0};
        if (c->ray != NULL && mc_world_raytrace_inspect(c->ray, &remaining))
            c->error.ray = remaining.cleanup;
        return mc_arrow_tick_fail(c, MC_ARROW_TICK_CLEANUP_FAILED,
                                  "Arrow returned ray checked retirement", true, NULL);
    }
    if (!world_end(c))
        return false;
    c->error.query = NULL;
    c->error.execution = NULL;
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    return true;
}

bool mc_arrow_tick_nearby(mc_arrow_tick_call *c, mc_world_runtime *world) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_NEARBY, 564);
    c->query_live = true;
    c->error.query = &c->query_result;
    c->error.execution = &c->execution;
    if (world == NULL)
        return mc_arrow_tick_null(c, "Arrow null captured nearby World receiver");
    c->query_invoked = true; /* Actual C entry; not proof that GET113 was reached. */
    const bool completed = mc_world_runtime_nearby_entities_in_execution(
        &c->execution, world, c->query_code, c->state.receiver, c->query_bounds, &c->nearby,
        &c->work.nearby, &c->query_status, &c->query_result);
    if (!completed) {
        c->error.nearby = &c->work.nearby;
        c->error.child_site = (mc_arrow_site){"fd", "b", "(Lsn;Leq;)Ljava/util/List;",
                                              (int32_t)c->work.nearby.query.bytecode};
        const mc_math_class_error *math = mc_math_execution_cause(&c->math);
        if (math != NULL && math->status != MC_MATH_CLASS_OK) {
            c->error.math = math;
            const mc_math_execution_site child =
                c->math.failed ? c->math.first.child : c->math.current.child;
            c->error.child_site =
                (mc_arrow_site){child.class_name, child.method, child.descriptor, child.pc};
            return mc_arrow_tick_fail(
                c,
                math->status == MC_MATH_CLASS_LIMIT               ? MC_ARROW_TICK_LIMIT
                : math->status == MC_MATH_CLASS_ALLOCATION_FAILED ? MC_ARROW_TICK_ALLOCATION
                : math->status == MC_MATH_CLASS_BUSY              ? MC_ARROW_TICK_BUSY
                                                                  : MC_ARROW_TICK_CHILD_FAILED,
                math->operation, true, NULL);
        }
        const char *exception = NULL;
        const mc_arrow_tick_failure_kind kind =
            query_classification(c, &c->work.nearby, &exception);
        const mc_world_storage_status status = c->query_status != MC_WORLD_STORAGE_OK
                                                   ? c->query_status
                                                   : c->query_result.ownership_status;
        return mc_arrow_tick_fail_classified(c, world_status(status), "Arrow actual nearby query",
                                             kind, exception);
    }
    if (!world_end(c))
        return false;
    c->error.query = NULL;
    c->error.execution = NULL;
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    return true;
}

bool mc_arrow_tick_intercept(mc_arrow_tick_call *c) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_INTERCEPT, 656);
    const mc_entity_hit_status status =
        mc_entity_bounds_intercept_begin(c->state.receiver->classes, c->candidate_bounds, c->start,
                                         c->end, &c->intercept, &c->entity_error);
    mc_entity_bounds_intercept_view view = {0};
    if (c->intercept != NULL &&
        mc_entity_bounds_intercept_inspect(c->intercept, &view) != MC_ENTITY_HIT_OK)
        return mc_arrow_tick_fail(c, MC_ARROW_TICK_STATE, "Arrow actual intercept view", true,
                                  NULL);
    if (status != MC_ENTITY_HIT_OK || !view.source_returned) {
        c->error.intercept = view.error;
        c->error.entity = &c->entity_error;
        return mc_arrow_tick_fail(c, MC_ARROW_TICK_CHILD_FAILED,
                                  view.error == NULL ? "Arrow actual intercept admission"
                                                     : view.error->operation,
                                  view.error == NULL || view.error->native_policy,
                                  view.error == NULL ? NULL
                                  : view.error->status == MC_ENTITY_HIT_SOURCE_NULL_REFERENCE
                                      ? "java/lang/NullPointerException"
                                      : NULL);
    }
    if (view.result != NULL) {
        const mc_entity_hit_status retained =
            mc_entity_hit_result_retain(view.result, &c->hit_error);
        if (retained != MC_ENTITY_HIT_OK) {
            c->error.hit = &c->hit_error;
            return mc_arrow_tick_fail(c, MC_ARROW_TICK_CHILD_FAILED,
                                      "Arrow intercept returned VF occurrence", true, NULL);
        }
    }
    c->replaced_hit = c->intercept_hit;
    c->intercept_hit = view.result; /* Actual astore12@659, including NULL. */
    mc_arrow_tick_at(c, 659);
    if (!mc_arrow_tick_hit_drop(c, &c->replaced_hit))
        return false;
    const mc_entity_hit_status retired =
        mc_entity_bounds_intercept_release(&c->intercept, &c->hit_cleanup);
    if (retired != MC_ENTITY_HIT_OK) {
        c->error.hit = &c->hit_cleanup;
        return mc_arrow_tick_fail(c, MC_ARROW_TICK_CLEANUP_FAILED,
                                  "Arrow intercept checked retirement", true, NULL);
    }
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    return true;
}

/* Actual invoke741 is reached only after the complete Source argument prefix,
 * including current Living c737, was evaluated/held by tick_body. The required
 * full victim damage join is still missing. Stop here without entering a
 * callee, inventing its Boolean or acquiring victim cleanup. Air/block paths
 * never reach this dependency. This is NOT a substitute damage implementation. */
bool mc_arrow_tick_hurt(mc_arrow_tick_call *c, mc_entity *target, mc_entity *source, bool *out) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_EFFECT, 741);
    (void)source;
    (void)out;
    if (target == NULL)
        return mc_arrow_tick_null(c, "Arrow null actual victim invocation receiver");
    return mc_arrow_tick_fail(c, MC_ARROW_TICK_CHILD_FAILED,
                              "Arrow actual victim damage dependency; callee not entered", true,
                              NULL);
}

bool mc_arrow_tick_sound(mc_arrow_tick_call *c, mc_world_runtime *world,
                         const mc_registry_string *name, float pitch, unsigned int pc) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_SOUND, pc);
    if (world == NULL)
        return mc_arrow_tick_null(c, "Arrow null captured impact sound World receiver");
    const mc_world_storage_status status = mc_world_runtime_entity_sound_detailed_in_execution(
        &c->execution, world, c->state.receiver, name, 1.0f, pitch, c->direct_world_site,
        &c->work.sound, &c->query_result);
    /* Source first only with this actual typed protected callback proof.
     * query_result retains the immediate first ownership error independently. */
    const char *source_exception = NULL;
    if (status != MC_WORLD_STORAGE_OK &&
        protected_exception(c, &c->work.sound.world_access_failure, &c->work.sound.cause,
                            &source_exception)) {
        c->error.sound = &c->work.sound;
        c->error.query = &c->query_result;
        c->error.execution = &c->execution;
        return mc_arrow_tick_fail_classified(c, world_status(status), c->work.sound.operation,
                                             named_kind(source_exception), source_exception);
    }
    if (c->query_result.ownership_status != MC_WORLD_STORAGE_OK) {
        c->error.sound = c->execution.capture_pending ? NULL : &c->work.sound;
        c->error.query = &c->query_result;
        c->error.execution = &c->execution;
        return mc_arrow_tick_fail(c, world_status(c->query_result.ownership_status),
                                  c->query_result.ownership_error.operation, true, NULL);
    }
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.sound = c->execution.capture_pending ? NULL : &c->work.sound;
        const char *exception = NULL;
        if (!c->execution.capture_pending)
            (void)protected_exception(c, &c->work.sound.world_access_failure, &c->work.sound.cause,
                                      &exception);
        return mc_arrow_tick_fail_classified(c, world_status(status), c->work.sound.operation,
                                             exception == NULL ? MC_ARROW_TICK_FAILURE_UNRESOLVED
                                                               : named_kind(exception),
                                             exception);
    }
    if (!world_end(c))
        return false;
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    return true;
}

bool mc_arrow_tick_particle(mc_arrow_tick_call *c, mc_world_runtime *world,
                            const mc_registry_string *name, double x, double y, double z, double vx,
                            double vy, double vz) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_PARTICLE, 1459);
    if (world == NULL)
        return mc_arrow_tick_null(c, "Arrow null captured bubble World receiver");
    const mc_world_storage_status status = mc_world_runtime_particle_detailed_in_execution(
        &c->execution, world, name, x, y, z, vx, vy, vz, c->direct_world_site, &c->work.particle,
        &c->query_result);
    /* Source first only with this actual typed protected callback proof.
     * query_result retains the immediate first ownership error independently. */
    const char *source_exception = NULL;
    if (status != MC_WORLD_STORAGE_OK &&
        protected_exception(c, &c->work.particle.world_access_failure, &c->work.particle.cause,
                            &source_exception)) {
        c->error.particle = &c->work.particle;
        c->error.query = &c->query_result;
        c->error.execution = &c->execution;
        return mc_arrow_tick_fail_classified(c, world_status(status), c->work.particle.operation,
                                             named_kind(source_exception), source_exception);
    }
    if (c->query_result.ownership_status != MC_WORLD_STORAGE_OK) {
        c->error.particle = c->execution.capture_pending ? NULL : &c->work.particle;
        c->error.query = &c->query_result;
        c->error.execution = &c->execution;
        return mc_arrow_tick_fail(c, world_status(c->query_result.ownership_status),
                                  c->query_result.ownership_error.operation, true, NULL);
    }
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.particle = c->execution.capture_pending ? NULL : &c->work.particle;
        const char *exception = NULL;
        if (!c->execution.capture_pending)
            (void)protected_exception(c, &c->work.particle.world_access_failure,
                                      &c->work.particle.cause, &exception);
        return mc_arrow_tick_fail_classified(c, world_status(status), c->work.particle.operation,
                                             exception == NULL ? MC_ARROW_TICK_FAILURE_UNRESOLVED
                                                               : named_kind(exception),
                                             exception);
    }
    ++c->state.bubble_calls;
    if (!world_end(c))
        return false;
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    return true;
}

bool mc_arrow_tick_dead(mc_arrow_tick_call *c, unsigned int pc) {
    mc_arrow_tick_select(c, MC_ARROW_TICK_CHILD_EFFECT, pc);
    const mc_entity_status status = mc_entity_effect_dead(c->state.receiver, &c->work.effect);
    if (status != MC_ENTITY_OK) {
        c->error.effect = &c->work.effect;
        return mc_arrow_tick_fail(c, entity_status(status), c->work.effect.operation, true, NULL);
    }
    c->work_tag = MC_ARROW_TICK_CHILD_NONE;
    return true;
}
