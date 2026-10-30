#include "internal.h"

#include "../../../world/runtime/runtime_internal.h"
#include "mc/world/runtime/runtime.h"

#include <stdlib.h>
#include <string.h>

static bool ledger_failure(mc_client_tile_renderer_error *e) {
    e->world = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                        "LL/CV native World occurrence invariant", 0};
    tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_BUSY, "LL/CV native World occurrence invariant",
                       NULL, NULL, NULL, -1, true);
    return false;
}

static bool cv_ledger_valid(const mc_client_tile_renderer *r) {
    size_t count = 0, external = 0, current = 0;
    for (const mc_client_tile_renderer_cv *cv = r->cvs; cv != NULL; cv = cv->next) {
        if (count == r->live_cvs || cv->owner != r || cv->fields.c != cv->world_occurrence ||
            cv->current != (cv == r->current_cv) || (!cv->current && cv->external_holds == 0) ||
            cv->external_holds > SIZE_MAX - external)
            return false;
        ++count;
        external += cv->external_holds;
        if (cv->current)
            ++current;
    }
    return count == r->live_cvs && external == r->retained_cv_holds &&
           current == (r->current_cv == NULL ? 0u : 1u) &&
           r->piston_blocks == (r->current_cv == NULL ? NULL : &r->current_cv->fields);
}

static size_t world_occurrences(const mc_client_tile_renderer_dispatcher *m,
                                const mc_world_runtime *world) {
    size_t count = m->world_occurrence == world ? 1u : 0u;
    for (size_t i = 0; i < m->allocated_renderers; ++i) {
        const mc_client_tile_renderer *r = m->renderers[i];
        if (r != NULL)
            for (const mc_client_tile_renderer_cv *cv = r->cvs; cv != NULL; cv = cv->next)
                if (cv->world_occurrence == world)
                    ++count;
    }
    return count;
}

/* Release is void in the canonical World API. Admit the complete owned
 * occurrence inventory first, then exclusive releases cannot reach zero-count
 * misuse. A World can already be source-retired while its native holds end. */
static bool world_ledger_admit(mc_client_tile_renderer_dispatcher *m,
                               mc_client_tile_renderer_error *e) {
    if (m->fields.f != m->world_occurrence)
        return ledger_failure(e);
    for (size_t i = 0; i < m->allocated_renderers; ++i)
        if (m->renderers[i] != NULL && !cv_ledger_valid(m->renderers[i]))
            return ledger_failure(e);
    if (m->world_occurrence != NULL &&
        m->world_occurrence->persistent_bindings < world_occurrences(m, m->world_occurrence))
        return ledger_failure(e);
    for (size_t i = 0; i < m->allocated_renderers; ++i) {
        const mc_client_tile_renderer *r = m->renderers[i];
        if (r != NULL)
            for (const mc_client_tile_renderer_cv *cv = r->cvs; cv != NULL; cv = cv->next)
                if (cv->world_occurrence != NULL && cv->world_occurrence->persistent_bindings <
                                                        world_occurrences(m, cv->world_occurrence))
                    return ledger_failure(e);
    }
    return true;
}

static void cv_destroy(mc_client_tile_renderer_cv *cv) {
    mc_client_tile_renderer *r = cv->owner;
    mc_client_tile_renderer_cv **slot = &r->cvs;
    while (*slot != cv)
        slot = &(*slot)->next;
    *slot = cv->next;
    mc_world_runtime_binding_release(cv->world_occurrence);
    --r->live_cvs;
    r->bytes -= sizeof(*cv);
    free(cv);
}

static bool cv_allocate_failure(mc_client_tile_renderer_error *e, mc_client_model_status status) {
    e->model = (mc_client_model_error){.status = status, .source_class = "hy", .source_pc = 1};
    tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_MODEL_FAILED, "actual HY CV native allocation",
                       "ll", "a", "(Lfd;)V", 45, true);
    e->renderer_pc = 1;
    return false;
}

static bool world_retain_failure(mc_client_tile_renderer_error *e, mc_world_runtime *world,
                                 bool callback) {
    e->world = *mc_world_runtime_last_error(world);
    tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_WORLD_FAILED,
                       callback ? "HY CV constructor native World occurrence"
                                : "LL source f native World occurrence",
                       "ll", "a", "(Lfd;)V", callback ? 45 : 2, true);
    if (callback)
        e->renderer_pc = 6;
    return false;
}

static bool piston_change_world(mc_client_tile_renderer *r, mc_world_runtime *world,
                                mc_client_tile_renderer_error *e) {
    /* HY's new instruction1 actively uses CV before native allocation. */
    if (!mc_client_render_blocks_active_use(r->parent->classes->models, &e->model)) {
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_MODEL_FAILED, "HY actual CV class active use",
                           "ll", "a", "(Lfd;)V", 45, true);
        e->renderer_pc = 1;
        return false;
    }
    if (r->bytes > r->max_bytes || sizeof(mc_client_tile_renderer_cv) > r->max_bytes - r->bytes ||
        r->live_cvs == SIZE_MAX)
        return cv_allocate_failure(e, MC_CLIENT_MODEL_LIMIT);
    mc_client_tile_renderer_cv *cv = calloc(1, sizeof(*cv));
    if (cv == NULL)
        return cv_allocate_failure(e, MC_CLIENT_MODEL_ALLOCATION_FAILED);
    r->bytes += sizeof(*cv);
    /* Native admission before entering constructor6, not a fabricated Java
     * exception or a claim that source CV.c61 already executed on failure. */
    if (world != NULL && !mc_world_runtime_binding_retain(world)) {
        r->bytes -= sizeof(*cv);
        free(cv); /* Unpublished, unreachable native allocation only. */
        return world_retain_failure(e, world, true);
    }
    cv->owner = r;
    cv->world_occurrence = world;
    mc_client_render_blocks_construct_fields(&cv->fields, world, true); /* CV c61, return64. */
    mc_client_tile_renderer_cv *previous = r->current_cv;
    cv->next = r->cvs;
    r->cvs = cv;
    ++r->live_cvs;
    cv->current = true;
    r->current_cv = cv;
    r->piston_blocks = &cv->fields; /* Actual HY.b9 after the complete constructor6. */
    if (previous != NULL) {
        previous->current = false;
        if (previous->external_holds == 0)
            cv_destroy(previous); /* End old field occurrence AFTER new b9. */
    }
    return true;
}

static bool renderer_change_world(mc_client_tile_renderer *r, mc_world_runtime *world,
                                  mc_client_tile_renderer_error *e) {
    if (strcmp(r->source_class, "hy") == 0)
        return piston_change_world(r, world, e);
    if (strcmp(r->source_class, "po") == 0 || strcmp(r->source_class, "ag") == 0)
        return true; /* Actual inherited JE.a(fd) body: return0. */
    tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_INVALID_PHASE, "LL actual JE World callback type",
                       "ll", "a", "(Lfd;)V", 45, true);
    return false;
}

bool tile_renderer_world_change_admit(mc_client_tile_renderer_dispatcher *m,
                                      mc_world_runtime *world, mc_client_tile_renderer_error *e) {
    if (world != NULL && (world->classes != m->classes->world_classes ||
                          world->construction.mode != MC_WORLD_RUNTIME_GAME)) {
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                           "LL nullable GAME World from the same class namespace", NULL, NULL, NULL,
                           -1, true);
        return false;
    }
    return world_ledger_admit(m, e);
}

/* The admitted caller holds both LL active guards through this exact body. */
bool tile_renderer_world_change_entered(mc_client_tile_renderer_dispatcher *m,
                                        mc_world_runtime *world, mc_client_tile_renderer_error *e) {
    if (world != NULL && !mc_world_runtime_binding_retain(world))
        return world_retain_failure(e, world, false);
    mc_world_runtime *previous = m->world_occurrence;
    m->fields.f = world; /* Actual LL.a(fd) putfield f2 BEFORE Map callbacks. */
    m->world_occurrence = world;
    mc_world_runtime_binding_release(previous);
    for (size_t i = 0; i < m->map->capacity; ++i)
        for (tile_renderer_map_node *node = m->map->buckets[i]; node != NULL; node = node->next) {
            mc_client_tile_renderer *r = node->value;
            if (r == NULL)
                continue; /* Actual null JE skip40. */
            r->active = true;
            const bool ok = renderer_change_world(r, world, e);
            r->active = false;
            if (!ok)
                return false;
        }
    return true;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_change_world(mc_client_tile_renderer_dispatcher *m, mc_world_runtime *world,
                                     mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (m == NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_NULL_REFERENCE, "actual LL.a(fd) receiver",
                           "ll", "a", "(Lfd;)V", 2, false);
    else if (tile_renderer_passive_admit(m, &e)) {
        if (m->classes->phase != MC_CLIENT_TILE_RENDERER_READY || m->classes->a != m ||
            m->map == NULL)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_PHASE,
                               "LL World source receiver requires completed initialization", NULL,
                               NULL, NULL, -1, true);
        else if (tile_renderer_world_change_admit(m, world, &e)) {
            m->active = m->classes->active = true;
            (void)tile_renderer_world_change_entered(m, world, &e);
            m->active = m->classes->active = false;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_cv_retain_current(mc_client_tile_renderer *r,
                                          mc_client_tile_renderer_cv **out,
                                          mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (r == NULL || out == NULL || strcmp(r->source_class, "hy") != 0)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                           "HY current CV native handle/output", NULL, NULL, NULL, -1, true);
    else if (r->active)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_BUSY, "HY active CV handle", NULL, NULL,
                           NULL, -1, true);
    else if (tile_renderer_passive_admit(r->parent, &e)) {
        if (!cv_ledger_valid(r))
            (void)ledger_failure(&e);
        else if (r->current_cv == NULL)
            *out = NULL;
        else if (r->current_cv->external_holds == SIZE_MAX || r->retained_cv_holds == SIZE_MAX)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_LIMIT, "HY external CV hold bound", NULL,
                               NULL, NULL, -1, true);
        else {
            ++r->current_cv->external_holds;
            ++r->retained_cv_holds;
            *out = r->current_cv;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_cv_release(mc_client_tile_renderer_cv **slot,
                                   mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (slot == NULL)
        tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                           "HY CV native occurrence slot", NULL, NULL, NULL, -1, true);
    else if (*slot != NULL) {
        mc_client_tile_renderer_cv *cv = *slot;
        mc_client_tile_renderer *r = cv->owner;
        if (r->active)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_BUSY, "HY active CV release", NULL, NULL,
                               NULL, -1, true);
        else if (tile_renderer_passive_admit(r->parent, &e)) {
            if (cv->external_holds == 0 || r->retained_cv_holds == 0)
                tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                                   "HY CV release requires an external occurrence", NULL, NULL,
                                   NULL, -1, true);
            else if (world_ledger_admit(r->parent, &e)) {
                --cv->external_holds;
                --r->retained_cv_holds;
                *slot = NULL;
                if (!cv->current && cv->external_holds == 0)
                    cv_destroy(cv);
            }
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool mc_client_tile_renderer_cv_inspect(const mc_client_tile_renderer_cv *cv,
                                        mc_client_tile_renderer_cv_state *s) {
    if (cv == NULL || s == NULL || cv->owner->active || cv->owner->parent->active ||
        cv->owner->parent->classes->active ||
        cv->owner->parent->classes->phase == MC_CLIENT_TILE_RENDERER_RETIRING)
        return false;
    bool ready = false, a = false;
    if (!cv_ledger_valid(cv->owner) ||
        !mc_client_render_blocks_static_get(cv->owner->parent->classes->models, &ready, &a))
        return false;
    *s = (mc_client_tile_renderer_cv_state){.owner = cv->owner,
                                            .fields = &cv->fields,
                                            .identity = &cv->fields,
                                            .world_occurrence = cv->world_occurrence,
                                            .external_holds = cv->external_holds,
                                            .allocation_bytes = sizeof(*cv),
                                            .current = cv->current,
                                            .static_ready = ready,
                                            .static_a = a};
    return true;
}

bool tile_renderer_world_terminal_admit(mc_client_tile_renderer_dispatcher *m,
                                        mc_client_tile_renderer_error *e) {
    for (size_t i = 0; i < m->allocated_renderers; ++i)
        if (m->renderers[i] != NULL && m->renderers[i]->retained_cv_holds != 0) {
            tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_BUSY,
                               "LL retained CV owners block class retirement", NULL, NULL, NULL, -1,
                               true);
            return false;
        }
    return world_ledger_admit(m, e);
}

bool tile_renderer_world_retire(mc_client_tile_renderer_dispatcher *m,
                                mc_client_tile_renderer_error *e) {
    if (!tile_renderer_world_terminal_admit(m, e))
        return false;
    /* Source views have ended. Drain the already-admitted native occurrences,
     * clearing consumed native pointers; no Java reset/callback is credited. */
    for (size_t i = 0; i < m->allocated_renderers; ++i) {
        mc_client_tile_renderer *r = m->renderers[i];
        if (r == NULL)
            continue;
        r->current_cv = NULL;
        r->piston_blocks = NULL;
        while (r->cvs != NULL)
            cv_destroy(r->cvs);
    }
    mc_world_runtime_binding_release(m->world_occurrence);
    m->world_occurrence = NULL;
    m->fields.f = NULL;
    return true;
}
