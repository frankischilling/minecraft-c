#include "tick_internal.h"

static mc_arrow_tick_error fresh_error(void) {
    mc_arrow_tick_error error = {0};
    error.site.pc = error.child_site.pc = -1;
    return error;
}

mc_arrow_tick_limits mc_arrow_tick_default_limits(void) {
    return (mc_arrow_tick_limits){
        .max_parent_bytes = 2u * 1024u * 1024u,
        .max_candidates = 1048576u,
        .max_angle_iterations = 1048576u,
        .base = {.max_particle_work = MC_ENTITY_TICK_DEFAULT_PARTICLE_WORK,
                 .material = {.max_cell_work = MC_WORLD_MATERIAL_MOVEMENT_DEFAULT_CELL_WORK}}};
}

static mc_arrow_tick_status admission(mc_arrow_tick_error *out, mc_arrow_tick_status status,
                                      const char *operation) {
    if (out != NULL) {
        *out = fresh_error();
        out->status = status;
        out->operation = operation;
        out->native_policy = status != MC_ARROW_TICK_OK;
        out->child =
            status == MC_ARROW_TICK_OK ? MC_ARROW_TICK_CHILD_NONE : MC_ARROW_TICK_CHILD_OWNER;
    }
    return status;
}

mc_arrow_tick_status mc_arrow_tick_begin(mc_entity *arrow, mc_world_runtime_classes *code,
                                         mc_math_classes *math, const mc_arrow_tick_limits *limits,
                                         mc_arrow_tick_call **out,
                                         mc_arrow_tick_error *admission_error) {
    if (out == NULL || *out != NULL || !mc_entity_has_arrow(arrow))
        return admission(admission_error, MC_ARROW_TICK_ARGUMENT, "Arrow tick native arguments");
    if (arrow->phase != MC_ENTITY_READY || arrow->holds == 0)
        return admission(admission_error, MC_ARROW_TICK_STATE, "Arrow tick held READY owner");
    if (arrow->arrow->tick_call != NULL || arrow->arrow->constructor_active ||
        arrow->arrow->heading_active)
        return admission(admission_error, MC_ARROW_TICK_BUSY,
                         "Arrow tick prior active/pending call");
    if (code != NULL && code->side != MC_WORLD_INFO_CLIENT)
        return admission(admission_error, MC_ARROW_TICK_ARGUMENT,
                         "Arrow client defining code owner");
    const mc_arrow_tick_limits selected = limits == NULL ? mc_arrow_tick_default_limits() : *limits;
    if (selected.max_parent_bytes < sizeof(mc_arrow_tick_call))
        return admission(admission_error, MC_ARROW_TICK_LIMIT,
                         "Arrow retained parent frame ceiling");
    if (code != NULL && code->native_borrows == SIZE_MAX)
        return admission(admission_error, MC_ARROW_TICK_LIMIT,
                         "Arrow defining class occurrence ceiling");
    const mc_entity_status held = mc_entity_retain(arrow);
    if (held != MC_ENTITY_OK)
        return admission(admission_error,
                         held == MC_ENTITY_LIMIT ? MC_ARROW_TICK_LIMIT : MC_ARROW_TICK_BUSY,
                         "Arrow tick receiver occurrence");
    mc_arrow_tick_call *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        mc_entity_release(arrow);
        return admission(admission_error, MC_ARROW_TICK_ALLOCATION,
                         "Arrow retained call allocation");
    }
    c->limits = selected;
    c->error = c->cleanup = fresh_error();
    c->state.receiver = arrow;
    c->state.phase = MC_ARROW_TICK_ACTIVE;
    c->state.active = true;
    c->state.failure = &c->error;
    c->state.cleanup = &c->cleanup;
    c->receiver_held = true;
    c->code_classes = code;
    if (code != NULL) {
        ++code->native_borrows;
        c->code_held = true;
        c->registry = code->registry; /* Defining namespace, never Arrow.world inference. */
    }
    /* Actual defining client Chunk code, independent of nullable/mutable
     * Arrow.world. c holds the real declaring code/Registry graph above. */
    c->query_code =
        (mc_world_runtime_execution_code){{"lm", "e", "()V", 6}, {"lm", "e", "()V", 33}};
    mc_world_runtime_execution_result_init(&c->query_result);
    mc_math_execution_init(&c->math, math);
    mc_entity_tick_execution_init(&c->execution, &c->math);
    arrow->arrow->tick_call = c; /* Borrowed backlink; c owns the independent Arrow hold. */
    *out = c;
    (void)admission(admission_error, MC_ARROW_TICK_OK, NULL);
    const bool returned = mc_arrow_tick_body(c);
    c->state.active = false;
    c->state.source_returned = returned;
    c->state.phase = returned ? MC_ARROW_TICK_RETURNED : MC_ARROW_TICK_FAILED;
    if (!returned && !c->failed)
        (void)mc_arrow_tick_fail(c, MC_ARROW_TICK_STATE, "Arrow body incomplete native failure",
                                 true, NULL);
    return returned ? MC_ARROW_TICK_OK : c->error.status;
}

bool mc_arrow_tick_inspect(const mc_arrow_tick_call *c, mc_arrow_tick_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    out->candidate = c->candidate;
    out->selected = c->selected;
    out->hurt_source = c->hurt_source;
    out->hurt_target = c->hurt_target;
    out->start = c->start;
    out->end = c->end;
    out->candidate_bounds = c->candidate_bounds;
    out->hit = c->hit;
    out->pending_hit = c->pending_hit;
    out->intercept_hit = c->intercept_hit;
    out->ray_call = c->ray;
    out->intercept_call = c->intercept;
    out->query = c->query_live ? &c->query_result : NULL;
    out->query_live = c->query_live;
    out->query_invoked = c->query_invoked;
    out->query_status = c->query_status;
    out->query_unwind = c->query_unwind;
    out->direct_world_open = c->direct_world_open;
    out->diagnostic_bytes = c->exception_storage_bytes;
    return true;
}

static mc_arrow_tick_status cleanup_fail(mc_arrow_tick_call *c, mc_arrow_tick_child child,
                                         const char *operation) {
    c->cleanup.status = MC_ARROW_TICK_CLEANUP_FAILED;
    c->cleanup.operation = operation;
    c->cleanup.native_policy = true;
    c->cleanup.site = (mc_arrow_site){NULL, "native_retire", NULL, -1};
    c->cleanup.child = child;
    return c->cleanup.status;
}

static bool cleanup_vector(mc_arrow_tick_call *c, mc_entity_vector **owned) {
    if (*owned == NULL)
        return true;
    const mc_entity_status status = mc_entity_vector_release(c->state.receiver->classes, *owned);
    if (status != MC_ENTITY_OK) {
        c->cleanup.entity = &c->retire_entity_error;
        c->retire_entity_error =
            (mc_entity_error){.status = status, .operation = "Arrow remaining vector occurrence"};
        (void)cleanup_fail(c, MC_ARROW_TICK_CHILD_VECTOR, c->retire_entity_error.operation);
        return false;
    }
    *owned = NULL;
    return true;
}

static bool cleanup_bounds(mc_arrow_tick_call *c, mc_entity_bounds **owned,
                           mc_entity_classes *classes) {
    if (*owned == NULL)
        return true;
    const mc_entity_status status = mc_entity_bounds_release(classes, *owned);
    if (status != MC_ENTITY_OK) {
        c->retire_entity_error =
            (mc_entity_error){.status = status, .operation = "Arrow remaining bounds occurrence"};
        c->cleanup.entity = &c->retire_entity_error;
        (void)cleanup_fail(c, MC_ARROW_TICK_CHILD_BOUNDS, c->retire_entity_error.operation);
        return false;
    }
    *owned = NULL;
    return true;
}

static bool cleanup_hit(mc_arrow_tick_call *c, mc_entity_hit_result **owned) {
    if (*owned == NULL)
        return true;
    const mc_entity_hit_status status = mc_entity_hit_result_release(owned, &c->retire_hit_error);
    if (status == MC_ENTITY_HIT_OK)
        return true;
    c->cleanup.hit = &c->retire_hit_error;
    (void)cleanup_fail(c, MC_ARROW_TICK_CHILD_HIT, "Arrow remaining VF occurrence");
    return false;
}

static void clear_world_borrows(mc_arrow_tick_call *c) {
    c->error.base = NULL;
    c->error.block = NULL;
    c->error.nearby = NULL;
    c->error.sound = NULL;
    c->error.particle = NULL;
    if (c->error.loader != NULL && c->error.registry == &c->error.loader->registry)
        c->error.registry = NULL; /* This selected loader's typed borrow expires with it. */
    c->error.loader = NULL;
}

static bool unwind_same_token(const mc_entity_tick_execution_world_token *a,
                              const mc_entity_tick_execution_world_token *b) {
    return a->owner == b->owner && a->slot == b->slot && a->generation == b->generation;
}

static bool unwind_has_records(const mc_arrow_tick_call *c) {
    if (c->query_result.root_pending)
        return true;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (c->query_result.pending[i].used)
            return true;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (c->query_result.closes[i].used)
            return true;
    return false;
}

static mc_arrow_tick_status unwind_refused(mc_arrow_tick_call *c, mc_world_storage_status status,
                                           const char *operation) {
    c->query_unwind = MC_ARROW_TICK_UNWIND_REFUSED;
    if (c->query_live)
        mc_world_runtime_execution_result_note(
            &c->query_result, status, operation,
            (mc_entity_tick_execution_site){"sl", "w_", "()V", (int)c->state.source_pc});
    (void)cleanup_fail(c, MC_ARROW_TICK_CHILD_OWNER, operation);
    c->cleanup.ownership_status = status;
    c->cleanup.execution = &c->execution;
    c->cleanup.query = c->query_live ? &c->query_result : NULL;
    return c->cleanup.status;
}

/* Native-only protected diagnostic copy. All real producers/children remain
 * held: this runs before ANY cleanup can consume one. No Source or class use,
 * callback, getter, field repair or guessed name lifetime is introduced. */
static mc_arrow_tick_status snapshot_exception(mc_arrow_tick_call *c, bool *copied) {
    *copied = false;
    if (c->exception_storage != NULL) {
        if (c->error.exception_class == c->exception_storage && c->exception_storage_bytes != 0 &&
            c->limits.max_parent_bytes >= sizeof(*c) &&
            c->exception_storage_bytes <= c->limits.max_parent_bytes - sizeof(*c))
            return MC_ARROW_TICK_OK;
        return unwind_refused(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                              "Arrow owned diagnostic class-name provenance");
    }
    if (c->error.exception_class == NULL)
        return MC_ARROW_TICK_OK;
    if (c->limits.max_parent_bytes < sizeof(*c))
        return unwind_refused(c, MC_WORLD_STORAGE_LIMIT,
                              "Arrow protected diagnostic parent byte ceiling");
    const size_t available = c->limits.max_parent_bytes - sizeof(*c);
    const char *const protected_name = c->error.exception_class;
    size_t length = 0;
    while (length < available && protected_name[length] != '\0')
        ++length;
    /* available includes NUL; subtraction above and this strict inequality
     * prove length+1 and frame+bytes do not overflow or exceed the fixed cap. */
    if (length == available)
        return unwind_refused(c, MC_WORLD_STORAGE_LIMIT,
                              "Arrow protected diagnostic class-name byte ceiling");
    const size_t bytes = length + 1u;
    char *const owned = malloc(bytes);
    if (owned == NULL)
        return unwind_refused(c, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                              "Arrow protected diagnostic class-name allocation");
    memcpy(owned, protected_name, bytes);
    c->exception_storage = owned;
    c->exception_storage_bytes = bytes;
    c->error.exception_class = owned; /* Exact same text, now frame-owned. */
    c->query_unwind = MC_ARROW_TICK_UNWIND_DIAGNOSTIC;
    *copied = true;
    return MC_ARROW_TICK_OK;
}

/* Refresh only an actual completed selection after native finish. Scalar
 * classification/Source firstcause never changes during cleanup. No loader
 * raw pending view is re-adopted or read under some other World. */
static bool unwind_selected_loader(mc_arrow_tick_call *c) {
    if (!c->query_live || !c->query_result.selection_ready)
        return true;
    mc_entity_tick_execution_loader_view view = {0};
    if (!mc_entity_tick_execution_loader_read(&c->execution, &c->query_result.selection, &view)) {
        (void)unwind_refused(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "Arrow completed explicit loader selection read");
        return false;
    }
    if (c->work_tag == MC_ARROW_TICK_CHILD_BLOCK) {
        c->work.block.nearby.query.source_chunk = view.failure;
        c->error.loader = view.failure;
    } else if (c->work_tag == MC_ARROW_TICK_CHILD_RAY) {
        /* Ray scratch already drained. Keep only the checked neutral copy. */
        c->error.loader = view.failure;
    } else if (c->work_tag == MC_ARROW_TICK_CHILD_NEARBY) {
        c->work.nearby.query.source_chunk = view.failure;
        c->error.loader = view.failure;
    } else if (c->work_tag == MC_ARROW_TICK_CHILD_BASE &&
               c->base_execution.child == MC_TICK_ARROW_BASE_ENVIRONMENT) {
        c->work.base.environment.material.block.nearby.query.source_chunk = view.failure;
        c->error.loader = view.failure;
    }
    return true;
}

static mc_arrow_tick_status native_unwind_step(mc_arrow_tick_call *c, bool *quiescent) {
    if (c->state.active || c->ray != NULL || c->intercept != NULL || c->effect_cleanup_owned)
        return unwind_refused(c, MC_WORLD_STORAGE_BUSY,
                              "Arrow entered child must drain before explicit World unwind");
    if (!c->direct_world_open && !unwind_has_records(c)) {
        if (c->execution.world_open)
            return unwind_refused(c, MC_WORLD_STORAGE_BUSY,
                                  "Arrow unrecorded descendant World remains open");
        c->query_unwind = MC_ARROW_TICK_UNWIND_QUIESCENT;
        *quiescent = true; /* Scope/attempt quiescence, not producer retirement. */
        return MC_ARROW_TICK_OK;
    }
    mc_entity_tick_execution_world_token current = {0};
    if (!mc_entity_tick_execution_current_world(&c->execution, &current))
        return unwind_refused(
            c, c->execution.world_open ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : MC_WORLD_STORAGE_BUSY,
            "Arrow explicit unwind checked current World");
    const bool own = c->direct_world_open && unwind_same_token(&current, &c->direct_world);
    /* Direct sparse lookup by the CURRENT checked token, not an owner/error
     * history scan. Only the callee's actual returned matching capsule acts. */
    mc_world_runtime_execution_close *close = &c->query_result.closes[current.slot];
    const bool published = close->used && unwind_same_token(&current, &close->world);
    if ((!own && !published) || (own && close->used))
        return unwind_refused(c, MC_WORLD_STORAGE_BUSY,
                              "Arrow current descendant lacks its exact published close");
    if (!unwind_same_token(&c->captured_worlds[current.slot], &current)) {
        const mc_world_access_failure *view = NULL;
        mc_world_access_failure_lease *node = &c->execution.leases[current.slot];
        mc_world_storage_status status;
        const char *operation;
        if (mc_world_access_failure_lease_inspect(node, &view)) {
            status = mc_entity_tick_execution_capture_receipt(&c->execution, view);
            operation = "Arrow exact current protected failure receipt capture";
        } else {
            /* Checked IDLE no-op; ARMED/malformed refuses without consumption.
             * No unprotected raw receipt is inspected or manufactured. */
            mc_world_storage_error cause = {0};
            status = mc_world_access_failure_lease_release(node, &cause);
            operation = cause.operation == NULL ? "Arrow exact current unused lease admission"
                                                : cause.operation;
        }
        if (status != MC_WORLD_STORAGE_OK)
            return unwind_refused(c, status, operation);
        c->captured_worlds[current.slot] = current;
        c->query_unwind = MC_ARROW_TICK_UNWIND_CAPTURE;
        *quiescent = false;
        return MC_ARROW_TICK_OK;
    }
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (c->query_result.pending[i].used &&
            unwind_same_token(&current, &c->query_result.pending[i].attempt.world)) {
            const mc_world_storage_status status =
                mc_world_runtime_execution_finish_pending(&c->execution, &c->query_result);
            if (status != MC_WORLD_STORAGE_OK)
                return unwind_refused(c, status, "Arrow actual current pending completion");
            if (!unwind_selected_loader(c))
                return MC_ARROW_TICK_CLEANUP_FAILED;
            c->query_unwind = MC_ARROW_TICK_UNWIND_FINISH;
            *quiescent = false;
            return MC_ARROW_TICK_OK;
        }
    if (published) {
        const mc_world_storage_status status =
            mc_world_runtime_execution_retry_close(&c->execution, &c->query_result, close);
        if (status != MC_WORLD_STORAGE_OK)
            return unwind_refused(c, status, "Arrow actual current published child close");
        c->query_unwind = MC_ARROW_TICK_UNWIND_CHILD_CLOSE;
        *quiescent = false;
        return MC_ARROW_TICK_OK;
    }
    if (!own || unwind_has_records(c))
        return unwind_refused(c, MC_WORLD_STORAGE_BUSY,
                              "Arrow own close awaits all actual pending/close records");
    if (!unwind_selected_loader(c))
        return MC_ARROW_TICK_CLEANUP_FAILED;
    const mc_world_storage_status status =
        mc_entity_tick_execution_pop_world(&c->execution, &c->direct_world, c->failed);
    if (status != MC_WORLD_STORAGE_OK)
        mc_arrow_tick_own_pop_returned(c, status);
    if (status != MC_WORLD_STORAGE_OK)
        return unwind_refused(c, status, "Arrow own exact evaluated World close");
    c->direct_world_open = false;
    if (c->execution.world_open)
        return unwind_refused(c, MC_WORLD_STORAGE_BUSY,
                              "Arrow unexpected open scope after own exact close");
    c->query_unwind = MC_ARROW_TICK_UNWIND_QUIESCENT;
    *quiescent = true;
    return MC_ARROW_TICK_OK;
}

mc_arrow_tick_status mc_arrow_tick_native_unwind_step(mc_arrow_tick_call *c, bool *quiescent) {
    if (c == NULL || quiescent == NULL)
        return MC_ARROW_TICK_ARGUMENT;
    if (c->state.active || c->state.retiring)
        return MC_ARROW_TICK_BUSY;
    c->cleanup = fresh_error();
    bool copied = false;
    const mc_arrow_tick_status diagnostic = snapshot_exception(c, &copied);
    if (diagnostic != MC_ARROW_TICK_OK)
        return diagnostic; /* Caller output and all owners remain untouched. */
    if (copied) {
        *quiescent = false; /* This owned diagnostic copy was the ONE action. */
        return MC_ARROW_TICK_OK;
    }
    return native_unwind_step(c, quiescent);
}

mc_arrow_tick_status mc_arrow_tick_retire(mc_arrow_tick_call **slot) {
    if (slot == NULL)
        return MC_ARROW_TICK_ARGUMENT;
    mc_arrow_tick_call *c = *slot;
    if (c == NULL)
        return MC_ARROW_TICK_OK;
    if (c->state.active || c->state.retiring)
        return MC_ARROW_TICK_BUSY;
    c->state.phase = MC_ARROW_TICK_RETIRING;
    c->state.retiring = true;
    c->cleanup = fresh_error();
    mc_arrow_tick_status status = MC_ARROW_TICK_CLEANUP_FAILED;
    /* Snapshot BEFORE effect/ray/intercept/local cleanup as well as neutral
     * lease release. Any one of those children may own this arbitrary name. */
    bool copied = false;
    if (snapshot_exception(c, &copied) != MC_ARROW_TICK_OK)
        goto done;

    /* Keep Arrow's protected WA captures while a victim-owned child releases
     * its own captures. Existing child cleanup does not retire the producer. */
    if (c->effect_cleanup_owned) {
        c->error.effect = NULL; /* Subsequent cleanup may consume nested borrows. */
        if (!mc_entity_effect_error_retire(&c->work.effect)) {
            c->cleanup.effect = &c->work.effect;
            (void)cleanup_fail(c, MC_ARROW_TICK_CHILD_EFFECT,
                               "Arrow newly acquired victim child checked retirement");
            goto done;
        }
        c->effect_cleanup_owned = false;
        c->state.hurt_cleanup_acquired = false;
    }
    if (c->intercept != NULL) {
        c->error.intercept = NULL;
        if (mc_entity_bounds_intercept_release(&c->intercept, &c->retire_hit_error) !=
            MC_ENTITY_HIT_OK) {
            c->cleanup.hit = &c->retire_hit_error;
            (void)cleanup_fail(c, MC_ARROW_TICK_CHILD_INTERCEPT,
                               "Arrow intercept checked retirement");
            goto done;
        }
    }
    /* The interceptor borrows the exact expanded AABB until release succeeds. */
    if (!cleanup_bounds(c, &c->replaced_bounds, c->state.receiver->classes) ||
        !cleanup_bounds(c, &c->candidate_bounds, c->state.receiver->classes))
        goto done;
    if (c->ray != NULL) {
        c->error.ray = NULL;
        if (mc_world_raytrace_retire(&c->ray) != MC_WORLD_RAYTRACE_OK) {
            mc_world_raytrace_state state = {0};
            if (mc_world_raytrace_inspect(c->ray, &state))
                c->cleanup.ray = state.cleanup;
            (void)cleanup_fail(c, MC_ARROW_TICK_CHILD_RAY, "Arrow World ray checked retirement");
            goto done;
        }
    }
    if (!cleanup_hit(c, &c->replaced_hit) || !cleanup_hit(c, &c->intercept_hit) ||
        !cleanup_hit(c, &c->old_hit) || !cleanup_hit(c, &c->pending_hit) ||
        !cleanup_hit(c, &c->hit))
        goto done;
    if (!cleanup_bounds(c, &c->block_box, c->block_box_classes) ||
        !cleanup_bounds(c, &c->swept, c->state.receiver->classes) ||
        !cleanup_bounds(c, &c->query_bounds, c->state.receiver->classes))
        goto done;
    if (!cleanup_vector(c, &c->replaced_vector) || !cleanup_vector(c, &c->point) ||
        !cleanup_vector(c, &c->start) || !cleanup_vector(c, &c->end))
        goto done;
    mc_world_entity_list_view_release(&c->nearby);
    c->error.effect = NULL;
    mc_arrow_tick_entity_drop(&c->candidate);
    mc_arrow_tick_entity_drop(&c->selected);
    mc_arrow_tick_entity_drop(&c->hurt_source);
    mc_arrow_tick_entity_drop(&c->hurt_target);

    /* Only explicit participating records or our own token authorize unwind.
     * No Math/producer retirement occurs while one native step still refuses. */
    bool quiescent = false;
    for (size_t i = 0;
         i < 2u * MC_ENTITY_TICK_EXECUTION_WORLDS + MC_ENTITY_TICK_EXECUTION_LOADERS + 2u; ++i) {
        if (native_unwind_step(c, &quiescent) != MC_ARROW_TICK_OK)
            goto done;
        if (quiescent)
            break;
    }
    if (!quiescent) {
        (void)unwind_refused(c, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "Arrow bounded explicit native unwind did not drain");
        goto done;
    }
    /* Victim/ray cleanup and all native scopes are drained. Expire typed
     * outward views BEFORE checked producer release. A real capture_pending
     * or lease refusal occurs through this API while Math is still live. */
    clear_world_borrows(c);
    const mc_world_storage_status producers =
        mc_entity_tick_execution_release_receipts(&c->execution);
    if (producers != MC_WORLD_STORAGE_OK) {
        (void)unwind_refused(c, producers,
                             "Arrow protected producer checked release before Math retirement");
        goto done;
    }
    c->error.math = NULL;
    if (mc_math_execution_retire(&c->math) != MC_MATH_CLASS_OK) {
        c->cleanup.math = &c->math.cleanup;
        (void)cleanup_fail(c, MC_ARROW_TICK_CHILD_MATH, "Arrow Math context checked retirement");
        goto done;
    }
    clear_world_borrows(c);
    const mc_world_storage_status owners = mc_entity_tick_execution_retire(&c->execution);
    if (owners != MC_WORLD_STORAGE_OK) {
        (void)unwind_refused(c, owners,
                             "Arrow evaluated World/producer context checked retirement");
        goto done;
    }
    mc_entity *receiver = c->state.receiver;
    if (!c->receiver_held || !mc_entity_has_arrow(receiver) || receiver->arrow->tick_call != c) {
        (void)cleanup_fail(c, MC_ARROW_TICK_CHILD_OWNER, "Arrow borrowed backlink provenance");
        goto done;
    }
    if (c->code_held) {
        if (c->code_classes == NULL || c->code_classes->native_borrows == 0) {
            (void)cleanup_fail(c, MC_ARROW_TICK_CHILD_OWNER,
                               "Arrow defining class occurrence provenance");
            goto done;
        }
        c->error.registry = NULL;
        --c->code_classes->native_borrows;
        c->code_held = false;
        c->code_classes = NULL;
        c->registry = NULL;
    }
    receiver->arrow->tick_call = NULL;
    c->state.receiver = NULL;
    c->receiver_held = false;
    *slot = NULL;
    free(c->exception_storage); /* Only after ALL checked parent retirement succeeds. */
    free(c);
    mc_entity_release(receiver);
    return MC_ARROW_TICK_OK;

done:
    c->state.retiring = false;
    status =
        c->cleanup.status == MC_ARROW_TICK_OK ? MC_ARROW_TICK_CLEANUP_FAILED : c->cleanup.status;
    return status;
}
