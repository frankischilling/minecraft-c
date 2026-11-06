#include "../render/model_graph_internal.h"
#include "source_inventory_screen_internal.h"

static bool fail(mc_client_screen_error *e, mc_client_screen_status status, const char *operation,
                 const char *owner, const char *method, const char *descriptor, unsigned int pc,
                 bool native) {
    mc_screen_fail(e, status, operation, owner, method, descriptor, pc);
    e->native_policy = native;
    return false;
}

static mc_client_screen_status model_status(mc_client_model_status status) {
    return status == MC_CLIENT_MODEL_BUSY                ? MC_CLIENT_SCREEN_BUSY
           : status == MC_CLIENT_MODEL_LIMIT             ? MC_CLIENT_SCREEN_LIMIT
           : status == MC_CLIENT_MODEL_ALLOCATION_FAILED ? MC_CLIENT_SCREEN_ALLOCATION_FAILED
                                                         : MC_CLIENT_SCREEN_APP_FAILED;
}

bool mc_screen_inventory_active_locked(mc_client_screen_classes *c,
                                       mc_client_render_classes *models,
                                       mc_client_screen_error *e) {
    if (c->retiring || models == NULL || models->profile != c->profile ||
        (c->inventory_models != NULL && c->inventory_models != models))
        return fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "ID actual model namespace/profile", NULL,
                    NULL, NULL, 0, true);
    if (c->inventory_phase == MC_CLIENT_SCREEN_CLASS_READY)
        return true;
    if (c->inventory_phase == MC_CLIENT_SCREEN_CLASS_INITIALIZING)
        return fail(e, MC_CLIENT_SCREEN_BUSY, "ID active native initializer", NULL, NULL, NULL, 0,
                    true);
    if (c->inventory_phase == MC_CLIENT_SCREEN_CLASS_FAILED) {
        *e = c->inventory_failure;
        e->first_class_status = e->status;
        e->first_class_site = e->site;
        e->status = MC_CLIENT_SCREEN_CLASS_ERRONEOUS;
        return false;
    }
    c->inventory_phase = MC_CLIENT_SCREEN_CLASS_INITIALIZING;
    c->inventory_models = models;
    c->inventory_pc = 0;
    mc_client_model_status status =
        mc_client_item_renderer_allocate(models, 0, &c->inventory_pending);
    if (status == MC_CLIENT_MODEL_OK) {
        c->inventory_pc = 4;
        status =
            mc_client_item_renderer_construct(c->inventory_pending, &c->inventory_detail.model);
    }
    if (status != MC_CLIENT_MODEL_OK) {
        c->inventory_detail.model.status = status;
        fail(e, model_status(status), "ID actual distinct BB initializer", "id", "<clinit>", "()V",
             c->inventory_pc, true);
        e->inventory = &c->inventory_detail;
        c->inventory_failure = *e;
        c->inventory_phase = MC_CLIENT_SCREEN_CLASS_FAILED;
        return false;
    }
    c->inventory_renderer = c->inventory_pending;
    c->inventory_pending = NULL;
    c->inventory_store_pc = 7;
    c->inventory_pc = 10;
    c->inventory_phase = MC_CLIENT_SCREEN_CLASS_READY;
    return true;
}

mc_client_screen_status mc_client_screen_inventory_active_use(mc_client_screen_classes *c,
                                                              mc_client_render_classes *models,
                                                              mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (mc_screen_lock(c, &e)) {
        if (mc_screen_inventory_active_locked(c, models, &e))
            e.source_returned = true;
        mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

bool mc_screen_inventory_classes_cleanup_locked(mc_client_screen_classes *c,
                                                mc_client_screen_error *e) {
    mc_client_model_status status = mc_client_item_renderer_free(&c->inventory_renderer);
    if (status == MC_CLIENT_MODEL_OK)
        status = mc_client_item_renderer_free(&c->inventory_pending);
    if (status != MC_CLIENT_MODEL_OK) {
        c->inventory_detail.cleanup_model = status;
        e->inventory = &c->inventory_detail;
        return fail(e, model_status(status), "ID distinct BB native retirement", NULL, NULL, NULL,
                    0, true);
    }
    c->inventory_models = NULL;
    return true;
}

mc_client_item_renderer *mc_client_screen_inventory_item_renderer(mc_client_screen_classes *c) {
    return c == NULL || c->retiring ? NULL : c->inventory_renderer;
}

mc_client_screen_status
mc_client_screen_inventory_class_inspect(mc_client_screen_classes *c,
                                         mc_client_inventory_class_state *out,
                                         mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (out == NULL)
        fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "ID native class inspection", NULL, NULL, NULL,
             0, true);
    else if (mc_screen_lock(c, &e)) {
        *out = (mc_client_inventory_class_state){
            c->inventory_phase, c->inventory_renderer, c->inventory_pending, c->inventory_models,
            c->inventory_pc,    c->inventory_store_pc, &c->inventory_failure};
        mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}
