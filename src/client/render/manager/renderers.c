#include "internal.h"
#include <stdlib.h>
#include <string.h>

mc_client_model_graph manager_renderer_graph(mc_client_managed_renderer *r) {
    return (mc_client_model_graph){r->parent->classes->models,
                                   &r->allocations,
                                   &r->bytes,
                                   r->max_bytes,
                                   &r->failure,
                                   NULL,
                                   NULL};
}

static bool bw_construct(mc_client_managed_renderer *r) {
    mc_client_model_graph g = manager_renderer_graph(r);
    const mc_client_bw_slots slots = {&r->bw_model, &r->bw_blocks, &r->shadow_size,
                                      &r->shadow_opacity};
    return mc_client_model_graph_bw_construct(&g, &slots);
}

static bool gv_construct(mc_client_managed_renderer *r, manager_model *model, float shadow) {
    if (!bw_construct(r))
        return false;
    r->primary = model;      /* GV.e6, not a copied model. */
    r->shadow_size = shadow; /* GV.c11. */
    return true;
}

manager_model *manager_renderer_model(mc_client_managed_renderer *r, const char *cl,
                                      const char *descriptor, const char *caller, int32_t pc,
                                      float f, int32_t i) {
    mc_client_model_graph g = manager_renderer_graph(r);
    manager_model *model = manager_model_allocate(&g, cl, descriptor, caller, pc);
    if (model == NULL)
        return NULL;
    /* Native inspection chain also owns failed/unpublished argument models. */
    model->next = r->models;
    r->models = model;
    if (!manager_model_construct(&g, model, f, i))
        return NULL;
    return model;
}

mc_client_managed_renderer *manager_renderer_allocate(mc_client_render_manager *m, const char *cl,
                                                      int32_t pc) {
    mc_client_render_manager_classes *c = m->classes;
    if (m->allocated_renderers >= 28) {
        manager_fail(&c->failure, MC_CLIENT_MANAGER_INVALID_PHASE,
                     "manager actual constructor renderer bound", NULL, NULL, NULL, -1, true);
        return NULL;
    }
    mc_client_managed_renderer *r = manager_allocate(c, sizeof(*r), "th", pc);
    if (r == NULL)
        return NULL;
    r->parent = m;
    r->source_class = cl;
    r->holds = 1;
    r->max_bytes = c->options.renderer_bytes;
    r->bytes = sizeof(*r);
    m->renderers[m->allocated_renderers++] = r;
    if (strcmp(cl, "bb") == 0) {
        r->failure.status =
            mc_client_item_renderer_allocate(c->models, c->options.renderer_bytes, &r->bb);
        if (r->failure.status != MC_CLIENT_MODEL_OK)
            goto fail;
    } else {
        r->failure.status = mc_client_render_classes_retain(c->models, c->profile);
        if (r->failure.status != MC_CLIENT_MODEL_OK)
            goto fail;
        r->models_held = true;
        if (r->bytes > r->max_bytes) {
            r->failure.status = MC_CLIENT_MODEL_LIMIT;
            goto fail;
        }
    }
    return r;
fail:
    c->failure.model = r->failure;
    manager_fail(&c->failure, MC_CLIENT_MANAGER_MODEL_FAILED,
                 "actual renderer native RAW allocation", "th", "<init>", "()V", pc, true);
    return NULL;
}

bool manager_renderer_construct(mc_client_managed_renderer *r, manager_model *first,
                                manager_model *second, float shadow, float scale, int32_t icon) {
    mc_client_model_graph g = manager_renderer_graph(r);
    const char *cl = r->source_class;
    r->phase = MC_CLIENT_MODEL_CONSTRUCTING;
    r->active = true;
    if (strcmp(cl, "bb") == 0) {
        if (mc_client_item_renderer_construct(r->bb, &r->failure) != MC_CLIENT_MODEL_OK)
            goto failed;
    } else if (strcmp(cl, "yx") == 0) {
        manager_model *a = manager_renderer_model(r, "sc", "()V", "yx", 1, 0.0f, 0);
        if (a == NULL || !gv_construct(r, a, 1.0f))
            goto failed;
        manager_model *b = manager_renderer_model(r, "sc", "()V", "yx", 13, 0.0f, 0);
        if (b == NULL)
            goto failed;
        r->secondary = b; /* Inherited GV.a(KO) putfield f2. */
    } else if (strcmp(cl, "m") == 0) {
        manager_model *a = manager_renderer_model(r, "il", "()V", "m", 1, 0.0f, 0);
        if (a == NULL || !gv_construct(r, a, 0.5f))
            goto failed;
        manager_model *b = manager_renderer_model(r, "il", "(F)V", "m", 14, 2.0f, 0);
        if (b == NULL)
            goto failed;
        r->own_model = b; /* Actual fconst_2 at18, m.a22. */
    } else if (strcmp(cl, "ds") == 0) {
        manager_model *a = manager_renderer_model(r, "fh", "(F)V", "ds", 1, 0.0f, 0);
        if (a == NULL || !gv_construct(r, a, 0.5f))
            goto failed;
        r->own_model = r->primary; /* DS.a22 aliases GV.e. */
        manager_model *b = manager_renderer_model(r, "fh", "(F)V", "ds", 26, 1.0f, 0);
        if (b == NULL)
            goto failed;
        r->g = b; /* DS.g34. */
        manager_model *h = manager_renderer_model(r, "fh", "(F)V", "ds", 38, 0.5f, 0);
        if (h == NULL)
            goto failed;
        r->h = h; /* DS.h47. */
    } else if (strcmp(cl, "pq") == 0) {
        manager_model *a = manager_renderer_model(r, "lo", "()V", "pq", 1, 0.0f, 0);
        if (a == NULL || !gv_construct(r, a, 0.5f))
            goto failed;
    } else if (strcmp(cl, "tb") == 0 || strcmp(cl, "fe") == 0) {
        if (!bw_construct(r))
            goto failed;
        r->shadow_size = 0.5f; /* c7 before child new11. */
        manager_model *a =
            manager_renderer_model(r, strcmp(cl, "tb") == 0 ? "nq" : "ez", "()V", cl, 11, 0.0f, 0);
        if (a == NULL)
            goto failed;
        r->own_model = a; /* a18. */
    } else if (strcmp(cl, "on") == 0 || strcmp(cl, "gn") == 0) {
        if (!bw_construct(r))
            goto failed;
        r->own_blocks = mc_client_model_graph_blocks(&g, cl, 5);
        if (r->own_blocks == NULL)
            goto failed;
        r->shadow_size = 0.5f; /* c18, after distinct CV a12. */
    } else if (strcmp(cl, "dy") == 0) {
        if (!bw_construct(r))
            goto failed;
        mc_java_random *random = mc_client_model_graph_allocate(&g, sizeof(*random), "dy", 5);
        if (random == NULL)
            goto failed;
        const mc_java_random_profile_status status =
            mc_java_random_construct_default(r->parent->classes->profile, random);
        if (status != MC_JAVA_RANDOM_PROFILE_OK) {
            r->failure = (mc_client_model_error){.status = MC_CLIENT_MODEL_RANDOM_FAILED,
                                                 .source_class = "dy",
                                                 .source_pc = 9,
                                                 .random = status};
            goto failed;
        }
        r->random = random; /* DY.a12 after real no-argument constructor. */
    } else if (strcmp(cl, "dg") == 0) {
        if (!bw_construct(r))
            goto failed;
        r->icon = icon; /* DG.a6. */
    } else if (strcmp(cl, "mb") == 0 || strcmp(cl, "mc") == 0 || strcmp(cl, "kl") == 0 ||
               strcmp(cl, "pl") == 0 || strcmp(cl, "pi") == 0) {
        if (!bw_construct(r))
            goto failed;
    } else {
        if (strcmp(cl, "me") != 0 && strcmp(cl, "xy") != 0 && strcmp(cl, "va") != 0 &&
            strcmp(cl, "we") != 0 && strcmp(cl, "ip") != 0 && strcmp(cl, "v") != 0 &&
            strcmp(cl, "mj") != 0 && strcmp(cl, "yg") != 0 && strcmp(cl, "es") != 0 &&
            strcmp(cl, "gv") != 0) {
            r->failure = (mc_client_model_error){
                .status = MC_CLIENT_MODEL_INVALID_ARGUMENT, .source_class = cl, .source_pc = 0};
            goto failed;
        }
        /* Actual remaining direct GV subclass constructors. Both original
         * arguments were fully constructed before entering this receiver. */
        float actual_shadow = shadow;
        if (strcmp(cl, "yg") == 0)
            actual_shadow = shadow * scale;
        if (!gv_construct(r, first, actual_shadow))
            goto failed;
        if (strcmp(cl, "me") == 0 || strcmp(cl, "xy") == 0)
            r->secondary = second;
        else if (strcmp(cl, "mj") == 0)
            r->own_model = second;
        else if (strcmp(cl, "v") == 0)
            r->own_model = first;
        else if (strcmp(cl, "yg") == 0)
            r->scale = scale;
        /* VA/WE/IP/ES/GV have no further constructor stores. */
    }
    r->phase = MC_CLIENT_MODEL_READY;
    r->active = false;
    return true;
failed:
    r->phase = MC_CLIENT_MODEL_FAILED;
    r->active = false;
    return false;
}

bool manager_renderer_retire(mc_client_managed_renderer **slot, mc_client_manager_error *e) {
    if (*slot == NULL)
        return true;
    mc_client_managed_renderer *r = *slot;
    if (r->holds != 1 || r->active || r->phase == MC_CLIENT_MODEL_CONSTRUCTING) {
        manager_fail(e, MC_CLIENT_MANAGER_BUSY, "actual renderer external hold/operation", NULL,
                     NULL, NULL, -1, true);
        return false;
    }
    if (r->bb != NULL) {
        e->model.status = mc_client_item_renderer_free(&r->bb);
        if (e->model.status != MC_CLIENT_MODEL_OK)
            goto busy;
    }
    mc_client_model_graph g = manager_renderer_graph(r);
    if (mc_client_model_graph_retire(&g, &e->model) != MC_CLIENT_MODEL_OK)
        goto busy;
    if (r->models_held) {
        e->model.status = mc_client_render_classes_release(r->parent->classes->models);
        if (e->model.status != MC_CLIENT_MODEL_OK)
            goto busy;
        r->models_held = false;
    }
    r->parent->classes->bytes -= sizeof(*r);
    free(r);
    *slot = NULL;
    return true;
busy:
    manager_fail(e, MC_CLIENT_MANAGER_BUSY, "actual renderer graph retirement", NULL, NULL, NULL,
                 -1, true);
    return false;
}

mc_client_manager_status mc_client_managed_renderer_retain(mc_client_managed_renderer *r,
                                                           mc_client_manager_error *e) {
    mc_client_manager_error local;
    manager_error_init(&local);
    if (r == NULL)
        manager_fail(&local, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "renderer hold receiver", NULL,
                     NULL, NULL, -1, true);
    else if (r->active || r->parent->classes->active)
        manager_fail(&local, MC_CLIENT_MANAGER_BUSY, "renderer active operation", NULL, NULL, NULL,
                     -1, true);
    else if (r->parent->classes->phase == MC_CLIENT_MANAGER_RETIRING)
        manager_fail(&local, MC_CLIENT_MANAGER_INVALID_PHASE, "renderer terminal hold", NULL, NULL,
                     NULL, -1, true);
    else if (r->holds == SIZE_MAX)
        manager_fail(&local, MC_CLIENT_MANAGER_LIMIT, "renderer hold limit", NULL, NULL, NULL, -1,
                     true);
    else
        ++r->holds;
    if (e != NULL)
        *e = local;
    return local.status;
}

mc_client_manager_status mc_client_managed_renderer_release(mc_client_managed_renderer **slot,
                                                            mc_client_manager_error *e) {
    mc_client_manager_error local;
    manager_error_init(&local);
    if (slot == NULL)
        manager_fail(&local, MC_CLIENT_MANAGER_INVALID_ARGUMENT, "renderer occurrence slot", NULL,
                     NULL, NULL, -1, true);
    else if (*slot != NULL) {
        mc_client_managed_renderer *r = *slot;
        if (r->active || r->parent->classes->active || r->holds <= 1)
            manager_fail(&local, MC_CLIENT_MANAGER_BUSY, "renderer parent owns final occurrence",
                         NULL, NULL, NULL, -1, true);
        else {
            --r->holds;
            *slot = NULL;
        }
    }
    if (e != NULL)
        *e = local;
    return local.status;
}

bool mc_client_managed_renderer_inspect(const mc_client_managed_renderer *r,
                                        mc_client_managed_renderer_state *s) {
    if (r == NULL || s == NULL || r->active || r->parent->classes->active ||
        r->parent->classes->phase == MC_CLIENT_MANAGER_RETIRING)
        return false;
    *s = (mc_client_managed_renderer_state){
        .source_class = r->source_class,
        .identity = strcmp(r->source_class, "bb") == 0 ? (const void *)r->bb : (const void *)r,
        .phase = r->phase,
        .holds = r->holds,
        .allocation_bytes = r->bytes,
        .manager = r->manager,
        .bw_model = r->bw_model,
        .bw_blocks = r->bw_blocks,
        .own_blocks = r->own_blocks,
        .primary = r->primary,
        .secondary = r->secondary,
        .own_model = r->own_model,
        .g = r->g,
        .h = r->h,
        .random = r->random,
        .shadow_size = r->shadow_size,
        .shadow_opacity = r->shadow_opacity,
        .scale = r->scale,
        .icon = r->icon,
        .failure = &r->failure};
    if (r->bb != NULL) {
        const mc_client_item_renderer_fields *f = mc_client_item_renderer_fields_get(r->bb);
        s->bw_model = f->base_model;
        s->bw_blocks = f->base_blocks;
        s->own_blocks = f->e;
        s->shadow_size = f->shadow_size;
        s->shadow_opacity = f->shadow_opacity;
        s->random = f->f;
        s->manager = f->render_manager;
        s->allocation_bytes = mc_client_item_renderer_allocation_bytes(r->bb);
    }
    return true;
}
