#include "active_internal.h"
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
                            mc_player_tick_error *out) {
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
                             .registry = c == NULL ? NULL : c->travel.code_registry};
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
    if (scope.registry_held && !mc_registry_runtime_borrow_release(scope.registry))
        mc_active_fail(e, MC_ENTITY_STATE_ERROR, "Active Player Registry borrow retirement");
    mc_entity_release(p);
    const mc_entity_status result = e->status;
    if (result == MC_ENTITY_OK)
        mc_player_tick_error_init(e);
    if (out != NULL)
        *out = *e;
    diagnostic_free(e);
    return result;
}

mc_entity_status mc_local_player_action_tick(mc_entity *p, const mc_player_action_context *c,
                                             const mc_player_tick_limits *l,
                                             mc_player_tick_error *e) {
    return run(p, c, l, 0, e);
}

mc_entity_status mc_living_tick_body(mc_entity *p, const mc_player_action_context *c,
                                     const mc_player_tick_limits *l, mc_player_tick_error *e) {
    return run(p, c, l, 1, e);
}

mc_entity_status mc_player_tick(mc_entity *p, const mc_player_action_context *c,
                                const mc_player_tick_limits *l, mc_player_tick_error *e) {
    return run(p, c, l, 2, e);
}
