#include "internal.h"

#include <stdlib.h>
#include <string.h>

mc_client_model_graph tile_renderer_graph(mc_client_tile_renderer *r) {
    return (mc_client_model_graph){r->parent->classes->models,
                                   &r->allocations,
                                   &r->bytes,
                                   r->max_bytes,
                                   &r->failure,
                                   NULL,
                                   NULL};
}

static mc_client_tile_renderer *renderer_allocate(mc_client_tile_renderer_dispatcher *m,
                                                  const char *source, int32_t pc) {
    mc_client_tile_renderer_classes *c = m->classes;
    if (m->allocated_renderers >= 3) {
        tile_renderer_fail(&c->failure, MC_CLIENT_TILE_RENDERER_INVALID_PHASE,
                           "LL constructor renderer allocation ledger", NULL, NULL, NULL, -1, true);
        return NULL;
    }
    mc_client_tile_renderer *r =
        tile_renderer_allocate(c, sizeof(*r), "ll", "<init>", "()V", pc, &c->failure);
    if (r == NULL)
        return NULL;
    r->parent = m;
    r->source_class = source;
    r->holds = 1;
    r->bytes = sizeof(*r);
    r->max_bytes = c->options.renderer_bytes;
    r->rf_pc = -1;
    m->renderers[m->allocated_renderers++] = r;
    r->failure.status = mc_client_render_classes_retain(c->models, c->profile);
    if (r->failure.status == MC_CLIENT_MODEL_OK) {
        r->models_held = true;
        if (r->bytes <= r->max_bytes)
            return r;
        r->failure.status = MC_CLIENT_MODEL_LIMIT;
    }
    c->failure.model = r->failure;
    tile_renderer_fail(&c->failure, MC_CLIENT_TILE_RENDERER_MODEL_FAILED,
                       "LL actual JE native RAW allocation", "ll", "<init>", "()V", pc, true);
    return NULL;
}

static bool sign_construct(mc_client_tile_renderer *r) {
    mc_client_model_graph g = tile_renderer_graph(r);
    mc_client_sign_model *model = r->sign_candidate;
    r->sign_phase = MC_CLIENT_MODEL_CONSTRUCTING;
    r->rf_pc = 5;
    mc_client_model_part *a = mc_client_model_graph_part_allocate(&g, "rf", 5);
    if (a == NULL)
        goto failed;
    mc_client_model_graph_part_construct(a, 0, 0); /* PS constructor11. */
    model->a = a;                                  /* RF.a14 before the first box child. */
    r->rf_pc = 33;
    if (!mc_client_model_graph_box(&g, a, -12.0f, -14.0f, -1.0f, 24, 12, 2, 0.0f))
        goto failed;
    r->rf_pc = 37;
    mc_client_model_part *b = mc_client_model_graph_part_allocate(&g, "rf", 37);
    if (b == NULL)
        goto failed;
    mc_client_model_graph_part_construct(b, 0, 14); /* PS constructor44. */
    model->b = b;                                   /* RF.b47 before the second box child. */
    r->rf_pc = 65;
    if (!mc_client_model_graph_box(&g, b, -1.0f, -2.0f, -1.0f, 2, 14, 2, 0.0f))
        goto failed;
    r->sign_phase = MC_CLIENT_MODEL_READY;
    r->rf_pc = 68;
    return true;
failed:
    r->sign_phase = MC_CLIENT_MODEL_FAILED;
    return false;
}

static bool renderer_construct(mc_client_tile_renderer *r, int32_t ll_pc) {
    mc_client_tile_renderer_classes *c = r->parent->classes;
    mc_client_model_graph g = tile_renderer_graph(r);
    int32_t renderer_pc = 1; /* All three invoke the empty JE constructor1. */
    r->phase = MC_CLIENT_MODEL_CONSTRUCTING;
    r->active = true;
    if (strcmp(r->source_class, "po") == 0) {
        renderer_pc = 5;
        r->sign_candidate = mc_client_model_graph_allocate(&g, sizeof(*r->sign_candidate), "po", 5);
        if (r->sign_candidate == NULL)
            goto failed;
        renderer_pc = 9;
        if (!sign_construct(r))
            goto failed;
        r->sign_model = r->sign_candidate; /* PO.b12 only after RF returned68. */
    } else if (strcmp(r->source_class, "ag") == 0) {
        renderer_pc = 5;
        r->spawner_map_candidate =
            mc_client_model_graph_allocate(&g, sizeof(*r->spawner_map_candidate), "ag", 5);
        if (r->spawner_map_candidate == NULL)
            goto failed;
        /* Genuine no-argument HashMap constructor9 leaves an empty Map with
         * no backing table; AG.b12 publishes this distinct source identity. */
        r->spawner_map = r->spawner_map_candidate;
    }
    /* HY has no CV allocation or store in its constructor; b stays NULL. */
    r->phase = MC_CLIENT_MODEL_READY;
    r->active = false;
    return true;
failed:
    r->phase = MC_CLIENT_MODEL_FAILED;
    r->active = false;
    c->failure.model = r->failure;
    tile_renderer_fail(&c->failure, MC_CLIENT_TILE_RENDERER_MODEL_FAILED,
                       "LL actual JE constructor child", "ll", "<init>", "()V", ll_pc, true);
    c->failure.renderer_pc = renderer_pc;
    c->failure.rf_pc = r->rf_pc;
    return false;
}

bool tile_renderer_construct(mc_client_tile_renderer_dispatcher *m) {
    mc_client_tile_renderer_classes *c = m->classes;
    m->active = true;
    m->map_candidate =
        tile_renderer_allocate(c, sizeof(*m->map_candidate), "ll", "<init>", "()V", 5, &c->failure);
    if (m->map_candidate == NULL)
        goto failed;
    /* HashMap constructor9 completes before source m12 publication. */
    m->map = m->map_candidate;
    m->fields.m = m->map;
    m->last_constructor_pc = 12;
    static const mc_tile_entity_class_kind keys[3] = {MC_TILE_ENTITY_CLASS_KIND_SIGN,
                                                      MC_TILE_ENTITY_CLASS_KIND_SPAWNER,
                                                      MC_TILE_ENTITY_CLASS_KIND_PISTON};
    static const char *const sources[3] = {"po", "ag", "hy"};
    static const int32_t literal_pc[3] = {19, 38, 57};
    static const int32_t allocation_pc[3] = {21, 40, 59};
    static const int32_t constructor_pc[3] = {25, 44, 63};
    static const int32_t put_pc[3] = {28, 47, 66};
    for (size_t i = 0; i < 3; ++i) {
        const mc_tile_entity_class_literal *key = NULL;
        c->failure.tile_entity = mc_tile_entity_class_literal_get(c->tile_classes, keys[i], &key);
        if (c->failure.tile_entity != MC_TILE_ENTITY_OK) {
            tile_renderer_fail(&c->failure, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                               "LL passive canonical Class literal", "ll", "<init>", "()V",
                               literal_pc[i], true);
            goto failed;
        }
        mc_client_tile_renderer *r = renderer_allocate(m, sources[i], allocation_pc[i]);
        if (r == NULL)
            goto failed;
        m->last_constructor_pc = constructor_pc[i];
        if (!renderer_construct(r, constructor_pc[i]))
            goto failed;
        if (!tile_renderer_map_put(m, key, r, put_pc[i], true, &c->failure))
            goto failed;
    }
    /* Traverse THIS actual Map's values, including occurrence multiplicity.
     * Native bucket order is not asserted to equal a JVM identity-hash order. */
    for (size_t i = 0; i < m->map->capacity; ++i)
        for (tile_renderer_map_node *p = m->map->buckets[i]; p != NULL; p = p->next) {
            p->value->manager = m; /* Actual JE.a(LL) source putfield a2. */
            ++m->backpointers;
            m->last_constructor_pc = 108;
        }
    m->last_constructor_pc = 114;
    m->active = false;
    return true;
failed:
    m->active = false;
    return false;
}

bool tile_renderer_object_retire(mc_client_tile_renderer **slot, mc_client_tile_renderer_error *e) {
    if (*slot == NULL)
        return true;
    mc_client_tile_renderer *r = *slot;
    if (r->holds != 1 || r->active || r->phase == MC_CLIENT_MODEL_CONSTRUCTING) {
        tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_BUSY, "JE external hold/operation", NULL,
                           NULL, NULL, -1, true);
        return false;
    }
    mc_client_model_graph g = tile_renderer_graph(r);
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
    tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_BUSY, "JE actual graph retirement", NULL, NULL,
                       NULL, -1, true);
    return false;
}
