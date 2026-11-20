#include "cx_internal.h"

#include <stdlib.h>

mc_client_cx_status cx_publish(const mc_client_cx_error *e, mc_client_cx_error *out) {
    if (out != NULL)
        *out = *e;
    return e->status;
}

mc_client_cx_status cx_fail(mc_client_cx_error *e, mc_client_cx_status status,
                            const char *operation, const char *method, unsigned int pc) {
    e->status = status;
    e->operation = operation;
    e->source_class = method == NULL ? NULL : "cx";
    e->source_method = method;
    e->source_descriptor = method == NULL ? NULL : method[0] == 'a' ? "()Z" : "()V";
    e->source_pc = pc;
    return status;
}

static bool cx_lock(mc_client_cx_classes *c, mc_client_cx_error *e) {
    if (c == NULL) {
        cx_fail(e, MC_CLIENT_CX_INVALID_ARGUMENT, "cx native namespace", NULL, 0);
        return false;
    }
    const int result = mtx_trylock(&c->mutex);
    if (result != thrd_success) {
        e->native_thread_error = result;
        cx_fail(e, result == thrd_busy ? MC_CLIENT_CX_BUSY : MC_CLIENT_CX_THREAD_FAILED,
                "cx native namespace monitor", NULL, 0);
        return false;
    }
    return true;
}

static void cx_drop_locked(mc_client_cx_classes *c) {
    const bool last = --c->holds == 0;
    (void)mtx_unlock(&c->mutex);
    if (last) {
        mtx_destroy(&c->mutex);
        free(c);
    }
}

static void cx_initialize_locked(mc_client_cx_classes *c) {
    if (c->phase == MC_CLIENT_CX_CLASS_COLD) {
        c->enabled = true; /* Actual cx.<clinit> putstatic1. */
        c->last_store = 1;
        c->phase = MC_CLIENT_CX_CLASS_READY; /* return4. */
    }
}

mc_client_cx_status mc_client_cx_classes_create(mc_client_cx_classes **out,
                                                mc_client_cx_error *error) {
    mc_client_cx_error e = {0};
    if (out == NULL)
        cx_fail(&e, MC_CLIENT_CX_INVALID_ARGUMENT, "cx namespace output", NULL, 0);
    else {
        mc_client_cx_classes *c = calloc(1, sizeof(*c));
        if (c == NULL)
            cx_fail(&e, MC_CLIENT_CX_ALLOCATION_FAILED, "cx namespace allocation", NULL, 0);
        else {
            const int result = mtx_init(&c->mutex, mtx_plain);
            if (result != thrd_success) {
                free(c);
                e.native_thread_error = result;
                cx_fail(&e, MC_CLIENT_CX_THREAD_FAILED, "cx namespace monitor allocation", NULL, 0);
            } else {
                c->holds = 1;
                *out = c;
            }
        }
    }
    return cx_publish(&e, error);
}

mc_client_cx_status mc_client_cx_classes_active_use(mc_client_cx_classes *c,
                                                    mc_client_cx_error *error) {
    mc_client_cx_error e = {0};
    if (cx_lock(c, &e)) {
        cx_initialize_locked(c);
        (void)mtx_unlock(&c->mutex);
    }
    return cx_publish(&e, error);
}

mc_client_cx_status mc_client_cx_classes_retain(mc_client_cx_classes *c,
                                                mc_client_cx_error *error) {
    mc_client_cx_error e = {0};
    if (cx_lock(c, &e)) {
        if (c->holds == SIZE_MAX)
            cx_fail(&e, MC_CLIENT_CX_LIMIT, "cx namespace hold limit", NULL, 0);
        else
            ++c->holds;
        (void)mtx_unlock(&c->mutex);
    }
    return cx_publish(&e, error);
}

mc_client_cx_status mc_client_cx_classes_release(mc_client_cx_classes **slot,
                                                 mc_client_cx_error *error) {
    mc_client_cx_error e = {0};
    if (slot == NULL)
        cx_fail(&e, MC_CLIENT_CX_INVALID_ARGUMENT, "cx namespace release slot", NULL, 0);
    else if (*slot != NULL && cx_lock(*slot, &e)) {
        mc_client_cx_classes *c = *slot;
        *slot = NULL;
        cx_drop_locked(c);
    }
    return cx_publish(&e, error);
}

mc_client_cx_status mc_client_cx_classes_inspect(mc_client_cx_classes *c,
                                                 mc_client_cx_class_state *out,
                                                 mc_client_cx_error *error) {
    mc_client_cx_error e = {0};
    if (out == NULL)
        cx_fail(&e, MC_CLIENT_CX_INVALID_ARGUMENT, "cx namespace inspection output", NULL, 0);
    else if (cx_lock(c, &e)) {
        *out = (mc_client_cx_class_state){c->phase, c->holds, c->enabled, c->last_store};
        (void)mtx_unlock(&c->mutex);
    }
    return cx_publish(&e, error);
}

mc_client_cx_status mc_client_cx_allocate(mc_client_cx_classes *c, mc_client_source_cx **out,
                                          mc_client_cx_error *error) {
    mc_client_cx_error e = {0};
    if (out == NULL)
        cx_fail(&e, MC_CLIENT_CX_INVALID_ARGUMENT, "cx RAW instance output", NULL, 0);
    else if (cx_lock(c, &e)) {
        cx_initialize_locked(c); /* Actual new active use, before allocation. */
        if (c->holds == SIZE_MAX)
            cx_fail(&e, MC_CLIENT_CX_LIMIT, "cx instance namespace hold limit", NULL, 0);
        else {
            mc_client_source_cx *o = calloc(1, sizeof(*o));
            if (o == NULL)
                cx_fail(&e, MC_CLIENT_CX_ALLOCATION_FAILED, "cx RAW instance allocation", NULL, 0);
            else {
                o->classes = c;
                o->holds = 1;
                ++c->holds;
                *out = o;
            }
        }
        (void)mtx_unlock(&c->mutex);
    }
    return cx_publish(&e, error);
}

bool cx_instance_lock(mc_client_source_cx *o, mc_client_cx_error *e) {
    if (o != NULL)
        return cx_lock(o->classes, e);
    cx_fail(e, MC_CLIENT_CX_INVALID_ARGUMENT, "cx native receiver", NULL, 0);
    return false;
}

mc_client_cx_status mc_client_cx_construct(mc_client_source_cx *o, mc_client_cx_error *error) {
    mc_client_cx_error e = {0};
    if (cx_instance_lock(o, &e)) {
        if (o->phase != MC_CLIENT_CX_RAW)
            cx_fail(&e, MC_CLIENT_CX_INVALID_PHASE, "cx constructor requires RAW", NULL, 0);
        else {
            o->constructor_pc = 1; /* Actual Object.<init>()V. */
            o->phase = MC_CLIENT_CX_READY;
            o->constructor_pc = 4;
            e.source_class = "cx";
            e.source_method = "<init>";
            e.source_descriptor = "()V";
            e.source_pc = 4;
            e.source_returned = true;
        }
        (void)mtx_unlock(&o->classes->mutex);
    }
    return cx_publish(&e, error);
}

mc_client_cx_status mc_client_cx_retain(mc_client_source_cx *o, mc_client_cx_error *error) {
    mc_client_cx_error e = {0};
    if (cx_instance_lock(o, &e)) {
        if (o->holds == SIZE_MAX)
            cx_fail(&e, MC_CLIENT_CX_LIMIT, "cx instance hold limit", NULL, 0);
        else
            ++o->holds;
        (void)mtx_unlock(&o->classes->mutex);
    }
    return cx_publish(&e, error);
}

mc_client_cx_status mc_client_cx_release(mc_client_source_cx **slot, mc_client_cx_error *error) {
    mc_client_cx_error e = {0};
    if (slot == NULL)
        cx_fail(&e, MC_CLIENT_CX_INVALID_ARGUMENT, "cx instance release slot", NULL, 0);
    else if (*slot != NULL && cx_instance_lock(*slot, &e)) {
        mc_client_source_cx *o = *slot;
        mc_client_cx_classes *c = o->classes;
        *slot = NULL;
        if (--o->holds == 0) {
            free(o);
            cx_drop_locked(c);
        } else
            (void)mtx_unlock(&c->mutex);
    }
    return cx_publish(&e, error);
}

mc_client_cx_status mc_client_cx_inspect(mc_client_source_cx *o, mc_client_cx_state *out,
                                         mc_client_cx_error *error) {
    mc_client_cx_error e = {0};
    if (out == NULL)
        cx_fail(&e, MC_CLIENT_CX_INVALID_ARGUMENT, "cx instance inspection output", NULL, 0);
    else if (cx_instance_lock(o, &e)) {
        *out = (mc_client_cx_state){o->phase, o->holds, o->constructor_pc};
        (void)mtx_unlock(&o->classes->mutex);
    }
    return cx_publish(&e, error);
}
