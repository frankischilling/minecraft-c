#include "biped_internal.h"
#include <stdlib.h>

static mc_client_model_status admission(mc_client_biped_error *e, mc_client_model_status status,
                                        const char *operation) {
    if (e != NULL)
        *e = (mc_client_biped_error){.status = status,
                                     .operation = operation,
                                     .part = 0,
                                     .face = -1,
                                     .vertex = -1,
                                     .uv_copy = -1,
                                     .native_policy = true};
    return status;
}

static mc_client_model_graph graph(mc_client_biped_owner *o) {
    return (mc_client_model_graph){o->classes,   &o->allocations,   &o->bytes,
                                   o->max_bytes, &o->model_failure, &o->progress.failure,
                                   &o->progress};
}

mc_client_model_status mc_client_biped_allocate(mc_client_render_classes *classes, size_t max_bytes,
                                                mc_client_biped_owner **out,
                                                mc_client_biped_error *error) {
    if (classes == NULL || out == NULL)
        return admission(error, MC_CLIENT_MODEL_INVALID_ARGUMENT,
                         "Biped native allocation arguments");
    if (max_bytes == 0)
        max_bytes = 1024u * 1024u;
    if (max_bytes < sizeof(mc_client_biped_owner))
        return admission(error, MC_CLIENT_MODEL_LIMIT, "Biped native owner byte bound");
    mc_client_biped_owner *o = calloc(1, sizeof(*o));
    if (o == NULL)
        return admission(error, MC_CLIENT_MODEL_ALLOCATION_FAILED, "Biped native owner allocation");
    const mc_client_model_status held = mc_client_render_classes_retain(classes, classes->profile);
    if (held != MC_CLIENT_MODEL_OK) {
        free(o);
        return admission(error, held, "Biped actual class hold");
    }
    o->classes = classes;
    o->max_bytes = max_bytes;
    o->bytes = sizeof(*o);
    o->holds = 1;
    o->progress.failure.face = o->progress.failure.vertex = o->progress.failure.uv_copy = -1;
    o->progress.last_root_store.pc = -1;
    *out = o;
    return admission(error, MC_CLIENT_MODEL_OK, NULL);
}

mc_client_model_status mc_client_biped_retain(mc_client_biped_owner *o) {
    if (o == NULL)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    if (o->phase == MC_CLIENT_BIPED_RETIRING)
        return MC_CLIENT_MODEL_INVALID_PHASE;
    if (o->holds == SIZE_MAX)
        return MC_CLIENT_MODEL_LIMIT;
    ++o->holds;
    return MC_CLIENT_MODEL_OK;
}

mc_client_model_status mc_client_biped_construct_zero(mc_client_biped_owner *o,
                                                      mc_client_biped_error *error) {
    if (o == NULL)
        return admission(error, MC_CLIENT_MODEL_INVALID_ARGUMENT,
                         "Biped native constructor receiver");
    if (o->active || o->phase != MC_CLIENT_BIPED_RAW)
        return admission(error, o->active ? MC_CLIENT_MODEL_BUSY : MC_CLIENT_MODEL_INVALID_PHASE,
                         "Biped one-shot constructor admission");
    if (o->holds == SIZE_MAX)
        return admission(error, MC_CLIENT_MODEL_LIMIT, "Biped native per-call hold");
    ++o->holds;
    o->active = true;
    o->phase = MC_CLIENT_BIPED_CONSTRUCTING;
    mc_client_model_graph ledger = graph(o);
    const bool ok = mc_client_model_graph_fill_zero(&ledger, &o->fields);
    o->phase = ok ? MC_CLIENT_BIPED_READY : MC_CLIENT_BIPED_FAILED;
    o->progress.failure.model = o->model_failure;
    const mc_client_model_status status = ok ? MC_CLIENT_MODEL_OK : o->model_failure.status;
    o->active = false;
    --o->holds;
    if (error != NULL)
        *error = ok ? (mc_client_biped_error){0} : o->progress.failure;
    return status;
}

bool mc_client_biped_inspect(const mc_client_biped_owner *o, mc_client_biped_state *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = o->progress;
    out->phase = o->phase;
    out->fields = o->phase == MC_CLIENT_BIPED_RETIRING ? NULL : &o->fields;
    out->holds = o->holds;
    out->allocation_bytes = o->bytes;
    out->allocations = 0;
    for (const model_allocation *a = o->allocations; a != NULL; a = a->next)
        ++out->allocations;
    return true;
}

mc_client_model_status mc_client_biped_release(mc_client_biped_owner **slot,
                                               mc_client_biped_error *error) {
    if (slot == NULL)
        return admission(error, MC_CLIENT_MODEL_INVALID_ARGUMENT, "Biped native release slot");
    mc_client_biped_owner *o = *slot;
    if (o == NULL)
        return admission(error, MC_CLIENT_MODEL_OK, NULL);
    if (o->active || o->phase == MC_CLIENT_BIPED_CONSTRUCTING)
        return admission(error, MC_CLIENT_MODEL_BUSY, "Biped active owner release");
    if (o->holds > 1) {
        --o->holds;
        *slot = NULL;
        return admission(error, MC_CLIENT_MODEL_OK, NULL);
    }
    o->phase = MC_CLIENT_BIPED_RETIRING;
    mc_client_model_graph ledger = graph(o);
    mc_client_model_error cleanup = {0};
    const mc_client_model_status retired = mc_client_model_graph_retire(&ledger, &cleanup);
    if (retired != MC_CLIENT_MODEL_OK) {
        admission(error, retired, "Biped native graph retirement");
        if (error != NULL)
            error->retirement = cleanup;
        return retired;
    }
    const mc_client_model_status released = mc_client_render_classes_release(o->classes);
    if (released != MC_CLIENT_MODEL_OK)
        return admission(error, MC_CLIENT_MODEL_BUSY, "Biped native class hold retirement");
    free(o);
    *slot = NULL;
    return admission(error, MC_CLIENT_MODEL_OK, NULL);
}
