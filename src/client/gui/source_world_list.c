#include "source_world_list.h"
#include "../application_state.h"
#include "mc/core/java/java.h"
#include "source_world_selection_internal.h"
#include <xmmintrin.h>

static bool fail(mc_client_world_list_error *e, mc_client_screen_status status,
                 const char *operation, const char *owner, const char *method,
                 const char *descriptor, unsigned int pc) {
    mc_screen_fail(&e->screen, status, operation, owner, method, descriptor, pc);
    e->screen.native_policy = status != MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
                              status != MC_CLIENT_SCREEN_SOURCE_ARITHMETIC;
    return false;
}

static bool lg_fail(mc_client_world_list_error *e, mc_client_screen_status status,
                    const char *operation, unsigned int pc) {
    return fail(e, status, operation, "lg", "a", "(IIF)V", pc);
}

static bool fp(mc_client_world_list_error *e, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           lg_fail(e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                   "world list strict binary32 control state", pc);
}

static float addf(float a, float b) {
    volatile float v = a + b;
    return v;
}

static float subf(float a, float b) {
    volatile float v = a - b;
    return v;
}

mc_client_source_world_list *mc_screen_world_list_allocate_locked(mc_client_source_rq *rq,
                                                                  mc_client_screen_error *e) {
    if (rq == NULL) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "MG native allocation owner", NULL,
                       NULL, NULL, 0);
        return NULL;
    }
    mc_client_source_world_list *list = mc_screen_allocate(&rq->base.memory, sizeof(*list));
    if (list == NULL) {
        mc_screen_fail(e, rq->base.memory.allocation_status, "MG native new storage", "rq", "b",
                       "()V", 39);
        return NULL;
    }
    list->native_owner = rq;
    return list;
}

bool mc_screen_world_list_construct_locked(mc_client_source_world_list *list,
                                           mc_client_screen_error *e) {
    if (list == NULL || list->native_owner == NULL || list->phase != MC_CLIENT_SCREEN_RAW) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_INVALID_PHASE, "MG raw constructor owner", "mg",
                       "<init>", "(Lrq;)V", 0);
        return false;
    }
    list->phase = MC_CLIENT_SCREEN_CONSTRUCTING;
    list->a = list->native_owner; /* Actual mg2, before LG constructor29. */
    mc_client_source_da *base = &list->a->base;
    mc_client_world_list_fields *f = &list->fields;
    f->k = -2.0f;
    f->n = -1;
    f->o = 0;
    f->p = true;
    f->a = base->fields.b;
    f->b = base->fields.c;
    f->c = base->fields.d;
    f->h = 32;
    f->i = mc_java_i32_sub(base->fields.d, 64);
    f->j = 36;
    f->e = 0;
    f->d = base->fields.c;
    list->last_pc = 32;
    list->phase = MC_CLIENT_SCREEN_READY;
    return true;
}

bool mc_screen_world_list_buttons_locked(mc_client_source_world_list *list,
                                         mc_client_screen_list *unused, int32_t up, int32_t down,
                                         mc_client_screen_error *e) {
    (void)unused; /* The actual List argument is never read by lg.a(List,II). */
    if (list == NULL || list->phase != MC_CLIENT_SCREEN_READY || list->retired || list->active) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_INVALID_PHASE, "LG scroll button registration owner",
                       "lg", "a", "(Ljava/util/List;II)V", 0);
        return false;
    }
    list->fields.f = up;
    list->fields.g = down;
    list->last_pc = 10;
    return true;
}

bool mc_screen_world_list_release_locked(mc_client_source_world_list **slot,
                                         mc_client_screen_error *e) {
    if (slot == NULL) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "MG native retirement slot", NULL,
                       NULL, NULL, 0);
        return false;
    }
    if (*slot != NULL && (*slot)->active) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "MG draw remains active", NULL, NULL, NULL, 0);
        return false;
    }
    if (*slot != NULL)
        (*slot)->retired = true;
    *slot = NULL; /* Retained RQ arena still owns source field/history storage. */
    return true;
}

bool mc_screen_world_list_count(mc_client_source_world_list *list, int32_t *out,
                                mc_client_world_list_error *e) {
    if (list->a == NULL || list->a->fields.n == NULL)
        return fail(e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "MG actual rq.n List.size receiver",
                    "mg", "a", "()I", 7);
    const size_t n = mc_client_save_list_size(list->a->fields.n);
    if (n > INT32_MAX)
        return fail(e, MC_CLIENT_SCREEN_LIMIT, "MG native List.size range", "mg", "a", "()I", 7);
    *out = (int32_t)n;
    return true;
}

int32_t mc_screen_world_list_maximum(mc_client_source_world_list *list, int32_t n) {
    const mc_client_world_list_fields *f = &list->fields;
    return mc_java_i32_sub(mc_java_i32_mul(n, 36), mc_java_i32_sub(mc_java_i32_sub(f->i, f->h), 4));
}

void mc_screen_world_list_clamp(mc_client_source_world_list *list, int32_t n) {
    int32_t limit = mc_screen_world_list_maximum(list, n);
    if (limit < 0)
        limit /= 2;
    if (list->fields.m < 0)
        list->fields.m = 0;
    if (list->fields.m > (float)limit)
        list->fields.m = (float)limit; /* Source negative short-list quirk. */
}

mc_client_screen_status mc_client_world_list_button(mc_client_source_world_list *list,
                                                    mc_client_source_ke *button,
                                                    mc_client_screen_error *error) {
    mc_client_world_list_error e = {0};
    if (list == NULL || list->a == NULL) {
        mc_screen_fail(&e.screen, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "LG button owner", NULL, NULL,
                       NULL, 0);
        return mc_screen_publish(&e.screen, error);
    }
    mc_client_screen_classes *classes = list->a->base.classes;
    if (!mc_screen_lock(classes, &e.screen))
        return mc_screen_publish(&e.screen, error);
    if (list->active || list->retired || list->phase != MC_CLIENT_SCREEN_READY)
        mc_screen_fail(&e.screen, MC_CLIENT_SCREEN_BUSY, "LG button draw/retirement active", NULL,
                       NULL, NULL, 0);
    else if (button == NULL) {
        mc_screen_fail(&e.screen, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "LG actual ke.g receiver",
                       "lg", "a", "(Lke;)V", 1);
        e.screen.native_policy = false;
    } else if (button->fields.g &&
               (button->fields.f == list->fields.f || button->fields.f == list->fields.g)) {
        int32_t n;
        if (fp(&e, 34)) {
            const float step = (float)(mc_java_i32_mul(list->fields.j, 2) / 3);
            list->fields.m = button->fields.f == list->fields.f ? subf(list->fields.m, step)
                                                                : addf(list->fields.m, step);
            list->fields.k = -2;
            if (mc_screen_world_list_count(list, &n, &e))
                mc_screen_world_list_clamp(list, n);
        }
    }
    if (e.screen.status == MC_CLIENT_SCREEN_OK)
        e.screen.source_returned = true;
    mtx_unlock(&classes->mutex);
    return mc_screen_publish(&e.screen, error);
}
