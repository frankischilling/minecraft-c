#include "active_internal.h"
#include "mc/world/runtime/placement.h"
#include <stdlib.h>

void mc_player_tick_error_init(mc_player_tick_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    mc_entity_tick_error_init(&e->child_error.environment);
    e->registry = e->child_error.environment.registry;
    e->writer.index = e->writer.text_offset = SIZE_MAX;
    e->statistic.writer.index = e->statistic.writer.text_offset = SIZE_MAX;
    e->input.named_index = -1;
}

bool mc_active_fail(mc_player_tick_error *e, mc_entity_status status, const char *op) {
    e->status = status;
    e->operation = op;
    return false;
}

static mc_entity_status tick_registry_status(mc_registry_status status) {
    return status == MC_REGISTRY_OK                  ? MC_ENTITY_OK
           : status == MC_REGISTRY_LIMIT             ? MC_ENTITY_LIMIT
           : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_REGISTRY_BUSY              ? MC_ENTITY_BUSY
                                                     : MC_ENTITY_STATE_ERROR;
}

static mc_entity_status tick_world_status(mc_world_storage_status status) {
    return status == MC_WORLD_STORAGE_OK                  ? MC_ENTITY_OK
           : status == MC_WORLD_STORAGE_LIMIT             ? MC_ENTITY_LIMIT
           : status == MC_WORLD_STORAGE_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_WORLD_STORAGE_BUSY              ? MC_ENTITY_BUSY
                                                          : MC_ENTITY_WORLD_FAILED;
}

mc_entity_status mc_active_sleep_status(mc_player_sleep_status status) {
    return status == MC_PLAYER_SLEEP_OK                  ? MC_ENTITY_OK
           : status == MC_PLAYER_SLEEP_LIMIT             ? MC_ENTITY_LIMIT
           : status == MC_PLAYER_SLEEP_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
           : status == MC_PLAYER_SLEEP_BUSY              ? MC_ENTITY_BUSY
                                                         : MC_ENTITY_STATE_ERROR;
}

static mc_entity_status tick_wake_cleanup_status(const mc_player_sleep_cleanup *cleanup) {
    if (cleanup->status != MC_PLAYER_SLEEP_CLEANUP_FAILED)
        return mc_active_sleep_status(cleanup->status);
    /* Earlier refused attempts may have populated other siblings. Only this
     * attempt's actual typed child selects the current cleanup cause. */
    switch (cleanup->child) {
        case MC_PLAYER_SLEEP_CLEANUP_WORLD:
        case MC_PLAYER_SLEEP_CLEANUP_QUERY:
            return cleanup->world.status == MC_WORLD_STORAGE_OK
                       ? MC_ENTITY_STATE_ERROR
                       : tick_world_status(cleanup->world.status);
        case MC_PLAYER_SLEEP_CLEANUP_REGISTRY:
            return cleanup->registry.status == MC_REGISTRY_OK
                       ? MC_ENTITY_STATE_ERROR
                       : tick_registry_status(cleanup->registry.status);
        case MC_PLAYER_SLEEP_CLEANUP_OCCUPANCY:
            return cleanup->occupancy == NULL ||
                           cleanup->occupancy->cleanup.status == MC_WORLD_STORAGE_OK
                       ? MC_ENTITY_STATE_ERROR
                       : tick_world_status(cleanup->occupancy->cleanup.status);
        case MC_PLAYER_SLEEP_CLEANUP_COORDINATE:
            return cleanup->coordinate == MC_PLAYER_BUSY    ? MC_ENTITY_BUSY
                   : cleanup->coordinate == MC_PLAYER_LIMIT ? MC_ENTITY_LIMIT
                                                            : MC_ENTITY_STATE_ERROR;
        default:
            return MC_ENTITY_STATE_ERROR;
    }
}

bool mc_active_wake_retire(mc_player_tick_error *e) {
    if (e->child != MC_PLAYER_TICK_CHILD_WAKE)
        return true;
    e->cleanup = (mc_player_tick_cleanup){0};
    if (mc_player_sleep_retire(&e->child_error.wake.call, &e->cleanup.wake) != MC_PLAYER_SLEEP_OK) {
        mc_player_sleep_state state;
        if (mc_player_sleep_inspect(e->child_error.wake.call, &state) && state.failure != NULL)
            e->child_error.wake.error = *state.failure;
        e->cleanup.child = MC_PLAYER_TICK_CLEANUP_WAKE;
        e->cleanup.status = tick_wake_cleanup_status(&e->cleanup.wake);
        e->cleanup.operation = e->cleanup.wake.operation;
        return false;
    }
    /* The value source cause survives, but these exact producer borrows end
     * with the child. Do this before independent parent Registry retirement. */
    e->child_error.wake.error.acquisition = NULL;
    e->child_error.wake.error.occupancy = NULL;
    e->child_error.wake.error.bed = NULL;
    e->child_error.wake.error.world_sleep = NULL;
    e->child_error.wake.error.position = NULL;
    return true;
}

bool mc_player_tick_error_retire(mc_player_tick_error *e) {
    if (e == NULL)
        return false;
    e->cleanup = (mc_player_tick_cleanup){0};
    if (!mc_active_wake_retire(e))
        return false;
    if (e->retained_registry != NULL) {
        const mc_registry_status status =
            mc_registry_classes_release(e->retained_registry, &e->cleanup.registry);
        if (status != MC_REGISTRY_OK) {
            e->cleanup.child = MC_PLAYER_TICK_CLEANUP_REGISTRY;
            e->cleanup.status = tick_registry_status(status);
            e->cleanup.operation = "Active Player retained entry Registry retirement";
            return false;
        }
        e->retained_registry = NULL;
    }
    e->cleanup = (mc_player_tick_cleanup){0};
    return true;
}

bool mc_active_fp(mc_player_tick_error *e) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           mc_active_fail(e, MC_ENTITY_UNSUPPORTED_FP, "Active Player tick FP admission");
}

bool mc_active_work(mc_player_tick_error *e) {
    if (e->animation_work == e->max_animation_work)
        return mc_active_fail(e, MC_ENTITY_LIMIT, "Active Player pending animation loop ceiling");
    ++e->animation_work;
    return true;
}

bool mc_active_registry(mc_active_scope *s, mc_player_tick_error *e) {
    return (s->registry != NULL && s->registry->entities == s->player->classes) ||
           mc_active_fail(e, MC_ENTITY_STATE_ERROR, "Active Player actual code namespace");
}

bool mc_active_registry_result(mc_player_tick_error *e, mc_registry_status status) {
    if (status == MC_REGISTRY_OK)
        return true;
    return mc_active_fail(e,
                          status == MC_REGISTRY_LIMIT               ? MC_ENTITY_LIMIT
                          : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_ENTITY_ALLOCATION_FAILED
                          : status == MC_REGISTRY_BUSY              ? MC_ENTITY_BUSY
                                                                    : MC_ENTITY_STATE_ERROR,
                          "Active Player Registry child");
}

bool mc_active_world_result(mc_player_tick_error *e, mc_world_storage_status status,
                            const char *op) {
    return status == MC_WORLD_STORAGE_OK ||
           mc_active_fail(e,
                          status == MC_WORLD_STORAGE_LIMIT ? MC_ENTITY_LIMIT
                          : status == MC_WORLD_STORAGE_ALLOCATION_FAILED
                              ? MC_ENTITY_ALLOCATION_FAILED
                          : status == MC_WORLD_STORAGE_BUSY ? MC_ENTITY_BUSY
                                                            : MC_ENTITY_WORLD_FAILED,
                          op);
}
#ifdef MC_PLAYER_TICK_TEST_ALLOCATOR
extern void *mc_player_tick_test_allocate(size_t bytes);
extern void mc_player_tick_test_free(void *memory);
#define diagnostic_allocate mc_player_tick_test_allocate
#define diagnostic_free mc_player_tick_test_free
#else
#define diagnostic_allocate malloc
#define diagnostic_free free
#endif

static mc_entity_status rejected(mc_player_tick_error *out, mc_entity_status status,
                                 const char *operation) {
    if (out != NULL) {
        mc_player_tick_error_init(out);
        out->status = status;
        out->operation = operation;
    }
    return status;
}

static mc_entity_status run(mc_entity *p, const mc_player_action_context *c,
                            const mc_player_tick_limits *limits, int body,
                            bool can_transfer_children, mc_player_tick_error *out) {
    if (can_transfer_children && out == NULL)
        return rejected(out, MC_ENTITY_INVALID_ARGUMENT,
                        "Active Player retained child output destination");
    if (p == NULL)
        return rejected(out, MC_ENTITY_INVALID_ARGUMENT, "Active Player receiver");
    if (p->phase != MC_ENTITY_READY)
        return rejected(out, MC_ENTITY_INVALID_PHASE, "Active Player READY receiver");
    if (!mc_entity_is_kind(p, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) || p->local_player == NULL)
        return rejected(out, MC_ENTITY_STATE_ERROR, "Active Player canonical dc owner");
    const mc_entity_status held = mc_entity_retain(p);
    if (held != MC_ENTITY_OK)
        return rejected(out, held, "Active Player native hold");
    /* A complete source call nests already-large independent child frames.
     * Keep this fixed-size native scratch private to the call and off the
     * source thread stack. Caller output is never the working state. */
    mc_player_tick_error *e = diagnostic_allocate(sizeof(*e));
    if (e == NULL) {
        mc_entity_release(p);
        return rejected(out, MC_ENTITY_ALLOCATION_FAILED,
                        "Active Player diagnostic scratch allocation");
    }
    mc_player_tick_error_init(e);
    mc_active_scope scope = {.player = p,
                             .context = c,
                             .limits = limits,
                             .registry = c == NULL ? NULL : c->travel.code_registry,
                             .can_transfer_children = can_transfer_children};
    e->max_animation_work =
        limits == NULL ? MC_PLAYER_TICK_DEFAULT_ANIMATION_WORK : limits->max_animation_work;
    if (scope.registry != NULL) {
        const mc_registry_status borrowed = mc_registry_classes_retain(
            scope.registry, MC_REGISTRY_CLIENT, p->classes->profile, &e->registry);
        scope.registry_held = borrowed == MC_REGISTRY_OK;
        (void)mc_active_registry_result(e, borrowed);
    }
    if (e->status == MC_ENTITY_OK) {
        if (body == 0)
            (void)mc_active_local(&scope, e);
        else if (body == 1)
            (void)mc_active_living(&scope, e);
        else
            (void)mc_active_player(&scope, e);
    }
    if (scope.registry_held) {
        if (scope.can_transfer_children && e->child == MC_PLAYER_TICK_CHILD_WAKE) {
            /* Retained source/cleanup failure owns this independent entry
             * occurrence too. Never replace the first cause while unwinding. */
            e->retained_registry = scope.registry;
        } else if (!mc_registry_runtime_borrow_release(scope.registry)) {
            e->cleanup =
                (mc_player_tick_cleanup){.status = MC_ENTITY_STATE_ERROR,
                                         .operation = "Active Player Registry borrow retirement",
                                         .child = MC_PLAYER_TICK_CLEANUP_REGISTRY};
            (void)mc_registry_runtime_borrow_release_admit(scope.registry, &e->cleanup.registry);
            if (scope.can_transfer_children)
                e->retained_registry = scope.registry;
            if (e->status == MC_ENTITY_OK)
                mc_active_fail(e, e->cleanup.status, e->cleanup.operation);
        }
        scope.registry_held = false;
    }
    mc_entity_release(p);
    const mc_entity_status result = e->status;
    if (result == MC_ENTITY_OK)
        mc_player_tick_error_init(e);
    if (out != NULL)
        *out = *e; /* Sole transfer; the private scratch ends without release. */
    diagnostic_free(e);
    return result;
}

mc_entity_status mc_local_player_action_tick(mc_entity *p, const mc_player_action_context *c,
                                             const mc_player_tick_limits *l,
                                             mc_player_tick_error *e) {
    return run(p, c, l, 0, e != NULL, e);
}

mc_entity_status mc_living_tick_body(mc_entity *p, const mc_player_action_context *c,
                                     const mc_player_tick_limits *l, mc_player_tick_error *e) {
    return run(p, c, l, 1, e != NULL, e);
}

mc_entity_status mc_player_tick(mc_entity *p, const mc_player_action_context *c,
                                const mc_player_tick_limits *l, mc_player_tick_error *e) {
    return run(p, c, l, 2, e != NULL, e);
}

mc_entity_status mc_player_tick_with_child_output(mc_entity *p, const mc_player_action_context *c,
                                                  const mc_player_tick_limits *l,
                                                  bool can_transfer_children,
                                                  mc_player_tick_error *e) {
    return run(p, c, l, 2, can_transfer_children, e);
}
