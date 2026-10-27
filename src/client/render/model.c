#include "model_graph_internal.h"
#include <stdlib.h>
#include <string.h>

struct mc_client_item_renderer {
    mc_client_render_classes *classes;
    mc_client_model_phase phase;
    mc_client_item_renderer_fields fields;
    model_allocation *allocations;
    size_t bytes, max_bytes;
    mc_client_model_error failure;
};

static mc_client_model_status fail(mc_client_item_renderer *o, mc_client_model_status status,
                                   const char *source, int32_t pc) {
    o->failure = (mc_client_model_error){.status = status, .source_class = source, .source_pc = pc};
    return status;
}

static mc_client_model_graph graph(mc_client_item_renderer *o) {
    return (mc_client_model_graph){
        o->classes, &o->allocations, &o->bytes, o->max_bytes, &o->failure, NULL, NULL};
}

static void *allocate(mc_client_item_renderer *o, size_t bytes, const char *source, int32_t pc) {
    mc_client_model_graph ledger = graph(o);
    return mc_client_model_graph_allocate(&ledger, bytes, source, pc);
}

static mc_client_biped_model *biped(mc_client_item_renderer *o) {
    mc_client_biped_model *m = allocate(o, sizeof(*m), "bw", 5);
    if (m == NULL)
        return NULL;
    mc_client_model_graph ledger = graph(o);
    return mc_client_model_graph_fill_zero(&ledger, m) ? m : NULL;
}

static mc_client_render_blocks *blocks(mc_client_item_renderer *o, const char *source, int32_t pc) {
    if (!o->classes->cv_ready) {
        o->classes->cv_a = true;
        o->classes->cv_ready = true;
    }
    mc_client_render_blocks *p = allocate(o, sizeof(*p), source, pc);
    if (p != NULL) {
        p->d = -1;
        p->e = false;
        p->f = false;
        p->b = true;
        p->g = p->h = p->i = p->j = p->k = p->l = 0;
        p->O = 1;
    }
    return p;
}

mc_client_model_status mc_client_render_classes_create(mc_entity_classes *entities,
                                                       mc_java_random_profile *profile,
                                                       mc_client_render_classes **out) {
    if (entities == NULL || profile == NULL || out == NULL)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    mc_client_render_classes *p = calloc(1, sizeof(*p));
    if (p == NULL)
        return MC_CLIENT_MODEL_ALLOCATION_FAILED;
    if (mc_entity_classes_retain(entities, MC_ENTITY_CLIENT, profile) != MC_ENTITY_OK) {
        free(p);
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    }
    p->entities = entities;
    p->profile = profile;
    *out = p;
    return MC_CLIENT_MODEL_OK;
}

mc_client_model_status mc_client_render_classes_free(mc_client_render_classes *p) {
    if (p == NULL)
        return MC_CLIENT_MODEL_OK;
    if (p->owners != 0)
        return MC_CLIENT_MODEL_BUSY;
    if (mc_entity_classes_release(p->entities) != MC_ENTITY_OK)
        return MC_CLIENT_MODEL_BUSY;
    free(p);
    return MC_CLIENT_MODEL_OK;
}

mc_client_model_status mc_client_render_classes_retain(mc_client_render_classes *p,
                                                       mc_java_random_profile *profile) {
    if (p == NULL || p->profile != profile)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    if (p->owners == SIZE_MAX)
        return MC_CLIENT_MODEL_LIMIT;
    ++p->owners;
    return MC_CLIENT_MODEL_OK;
}

mc_client_model_status mc_client_render_classes_release(mc_client_render_classes *p) {
    if (p == NULL || p->owners == 0)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    --p->owners;
    return MC_CLIENT_MODEL_OK;
}

bool mc_client_render_blocks_static_get(const mc_client_render_classes *p, bool *ready, bool *a) {
    if (p == NULL || ready == NULL || a == NULL)
        return false;
    *ready = p->cv_ready;
    *a = p->cv_a;
    return true;
}

mc_client_model_status mc_client_item_renderer_allocate(mc_client_render_classes *classes,
                                                        size_t max_bytes,
                                                        mc_client_item_renderer **out) {
    if (classes == NULL || out == NULL)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    if (max_bytes == 0)
        max_bytes = 1024u * 1024u;
    if (max_bytes < sizeof(mc_client_item_renderer) || classes->owners == SIZE_MAX)
        return MC_CLIENT_MODEL_LIMIT;
    mc_client_item_renderer *p = calloc(1, sizeof(*p));
    if (p == NULL)
        return MC_CLIENT_MODEL_ALLOCATION_FAILED;
    p->classes = classes;
    p->max_bytes = max_bytes;
    p->bytes = sizeof(*p);
    ++classes->owners;
    *out = p;
    return MC_CLIENT_MODEL_OK;
}

mc_client_model_status mc_client_item_renderer_construct(mc_client_item_renderer *o,
                                                         mc_client_model_error *error) {
    if (o == NULL)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    if (o->phase != MC_CLIENT_MODEL_RAW)
        return MC_CLIENT_MODEL_INVALID_PHASE;
    o->phase = MC_CLIENT_MODEL_CONSTRUCTING;
    mc_client_biped_model *model = biped(o);
    if (model == NULL)
        goto failure;
    o->fields.base_model = model;
    o->fields.base_blocks = blocks(o, "bw", 16);
    if (o->fields.base_blocks == NULL)
        goto failure;
    o->fields.shadow_size = 0.0f;
    o->fields.shadow_opacity = 1.0f;
    o->fields.e = blocks(o, "bb", 5);
    if (o->fields.e == NULL)
        goto failure;
    mc_java_random *random = allocate(o, sizeof(*random), "bb", 16);
    if (random == NULL)
        goto failure;
    const mc_java_random_profile_status status =
        mc_java_random_construct_default(o->classes->profile, random);
    if (status != MC_JAVA_RANDOM_PROFILE_OK) {
        (void)fail(o, MC_CLIENT_MODEL_RANDOM_FAILED, "bb", 20);
        o->failure.random = status;
        goto failure;
    }
    o->fields.f = random;
    o->fields.a = true;
    o->fields.shadow_size = 0.15f;
    o->fields.shadow_opacity = 0.75f;
    o->phase = MC_CLIENT_MODEL_READY;
    if (error != NULL)
        *error = (mc_client_model_error){0};
    return MC_CLIENT_MODEL_OK;
failure:
    o->phase = MC_CLIENT_MODEL_FAILED;
    if (error != NULL)
        *error = o->failure;
    return o->failure.status;
}

mc_client_model_phase mc_client_item_renderer_phase(const mc_client_item_renderer *p) {
    return p == NULL ? MC_CLIENT_MODEL_FAILED : p->phase;
}

const mc_client_item_renderer_fields *
mc_client_item_renderer_fields_get(const mc_client_item_renderer *p) {
    return p == NULL ? NULL : &p->fields;
}

size_t mc_client_item_renderer_allocation_bytes(const mc_client_item_renderer *p) {
    return p == NULL ? 0 : p->bytes;
}

mc_client_model_status mc_client_item_renderer_free(mc_client_item_renderer **out) {
    if (out == NULL)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    mc_client_item_renderer *o = *out;
    if (o == NULL)
        return MC_CLIENT_MODEL_OK;
    if (o->phase == MC_CLIENT_MODEL_CONSTRUCTING)
        return MC_CLIENT_MODEL_BUSY;
    for (model_allocation *a = o->allocations; a != NULL; a = a->next)
        if (a->vector != NULL) {
            if (mc_entity_vector_release(o->classes->entities, a->vector) != MC_ENTITY_OK)
                return MC_CLIENT_MODEL_BUSY;
            a->vector = NULL;
        }
    model_allocation *a = o->allocations;
    while (a != NULL) {
        model_allocation *next = a->next;
        free(a);
        a = next;
    }
    --o->classes->owners;
    free(o);
    *out = NULL;
    return MC_CLIENT_MODEL_OK;
}

mc_client_model_status mc_client_item_renderer_draw(mc_client_item_renderer *p,
                                                    mc_client_model_error *error) {
    if (p == NULL)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    if (error != NULL)
        *error = (mc_client_model_error){.status = MC_CLIENT_MODEL_UNIMPLEMENTED_DEPENDENCY,
                                         .source_class = "bb",
                                         .source_pc = 0};
    return MC_CLIENT_MODEL_UNIMPLEMENTED_DEPENDENCY;
}
