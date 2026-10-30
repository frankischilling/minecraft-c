#include "mc/client/world/frustum_culling.h"

#include "../render/model_graph_internal.h"
#include "source_chunk_renderer_internal.h"

#include <emmintrin.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Source frustum wrapper arithmetic requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Source frustum wrapper arithmetic requires strict floating transformations"
#endif

struct mc_client_source_frustum {
    mc_client_source_frustum_state state;
    mc_client_culling_error failure;
    mc_client_render_classes *models;
};

struct mc_client_culling_call {
    mc_client_culling_state state;
    mc_client_culling_error failure;
    mc_client_render_global_error owner_error, cleanup;
};

static mc_client_frustum_status result(mc_client_culling_error *e, mc_client_frustum_status status,
                                       const char *operation, const char *type, const char *method,
                                       const char *descriptor, unsigned int pc, bool native) {
    if (e != NULL)
        *e = (mc_client_culling_error){.status = status,
                                       .operation = operation,
                                       .site = {type, method, descriptor, (int32_t)pc},
                                       .native_policy = native};
    return status;
}

static mc_client_frustum_status admission(mc_client_source_frustum *o, mc_client_culling_error *e) {
    if (o == NULL)
        return result(e, MC_CLIENT_FRUSTUM_SOURCE_NULL, "actual sr null receiver", "sr", "a",
                      "(DDDDDD)Z", 1, false);
    if (o->state.active || !o->state.constructed)
        return result(e, MC_CLIENT_FRUSTUM_BUSY, "sr active/failed native owner", NULL, NULL, NULL,
                      0, true);
    return MC_CLIENT_FRUSTUM_OK;
}

mc_client_frustum_status mc_client_source_frustum_begin(mc_client_render_classes *models,
                                                        mc_client_render_engine_classes *graphics,
                                                        mc_client_display *display,
                                                        mc_math_classes *math,
                                                        mc_client_source_frustum **out,
                                                        mc_client_culling_error *error) {
    if (models == NULL || out == NULL || *out != NULL)
        return result(error, MC_CLIENT_FRUSTUM_ARGUMENT, "sr native allocation/output", NULL, NULL,
                      NULL, 0, true);
    mc_client_source_frustum *o = calloc(1, sizeof(*o));
    if (o == NULL)
        return result(error, MC_CLIENT_FRUSTUM_ALLOCATION, "sr native allocation", NULL, NULL, NULL,
                      0, true);
    const mc_client_model_status held = mc_client_render_classes_retain(models, models->profile);
    if (held != MC_CLIENT_MODEL_OK) {
        free(o);
        result(error, MC_CLIENT_FRUSTUM_BUSY, "sr Models occurrence", NULL, NULL, NULL, 0, true);
        if (error != NULL)
            error->models = held;
        return MC_CLIENT_FRUSTUM_BUSY;
    }
    o->models = models;
    o->state.holds = 1;
    o->state.failure = &o->failure;
    o->state.active = true;
    o->state.source_pc = 5;
    *out = o;
    const mc_client_frustum_status status =
        mc_client_frustum_update(models, graphics, display, math, &o->state.a, &o->failure.frustum);
    if (status == MC_CLIENT_FRUSTUM_OK) {
        o->state.constructed = true;
        o->state.source_pc = 11;
    } else {
        const mc_client_frustum_error child = o->failure.frustum;
        result(&o->failure, status, child.operation, "sr", "<init>", "()V", 5, child.native_policy);
        o->failure.frustum = child;
        o->failure.child = child.site;
    }
    o->state.active = false;
    if (error != NULL)
        *error = o->failure;
    return status;
}

bool mc_client_source_frustum_inspect(const mc_client_source_frustum *o,
                                      mc_client_source_frustum_state *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = o->state;
    return true;
}

mc_client_frustum_status mc_client_source_frustum_recenter(mc_client_source_frustum *o, double x,
                                                           double y, double z,
                                                           mc_client_culling_error *e) {
    mc_client_frustum_status status = admission(o, e);
    if (status != MC_CLIENT_FRUSTUM_OK)
        return status;
    o->state.active = true;
    memcpy(&o->state.b, &x, sizeof x);
    memcpy(&o->state.c, &y, sizeof y);
    memcpy(&o->state.d, &z, sizeof z);
    o->state.source_pc = 16;
    o->state.active = false;
    return result(e, MC_CLIENT_FRUSTUM_OK, NULL, "sr", "a", "(DDD)V", 16, false);
}

mc_client_frustum_status mc_client_source_frustum_box(mc_client_source_frustum *o, double x0,
                                                      double y0, double z0, double x1, double y1,
                                                      double z1, bool *out,
                                                      mc_client_culling_error *e) {
    if (out == NULL)
        return result(e, MC_CLIENT_FRUSTUM_ARGUMENT, "sr disjoint box output", NULL, NULL, NULL, 0,
                      true);
    mc_client_frustum_status status = admission(o, e);
    if (status != MC_CLIENT_FRUSTUM_OK)
        return status;
    o->state.active = true;
    const mc_client_source_dr *d = o->state.a; /* Actual getfield1 before arguments. */
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        o->state.active = false;
        return result(e, MC_CLIENT_FRUSTUM_UNSUPPORTED_FP, "sr reached strict FP admission", "sr",
                      "a", "(DDDDDD)Z", 9, true);
    }
    const double input[] = {x0, y0, z0, x1, y1, z1};
    double relative[6];
    for (size_t i = 0; i < 6; ++i) {
        const double origin = i % 3 == 0 ? o->state.b : i % 3 == 1 ? o->state.c : o->state.d;
        relative[i] = _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(input[i]), _mm_set_sd(origin)));
    }
    mc_client_frustum_error child;
    status = mc_client_frustum_box(d, relative[0], relative[1], relative[2], relative[3],
                                   relative[4], relative[5], out, &child);
    o->state.source_pc = status == MC_CLIENT_FRUSTUM_OK ? 47 : 44;
    o->state.active = false;
    result(e, status, child.operation, "sr", "a", "(DDDDDD)Z", o->state.source_pc,
           child.native_policy);
    if (e != NULL) {
        e->frustum = child;
        e->child = child.site;
    }
    return status;
}

mc_client_frustum_status mc_client_source_frustum_bounds(mc_client_source_frustum *o,
                                                         const mc_entity_bounds *box, bool *out,
                                                         mc_client_culling_error *e) {
    if (o == NULL)
        return result(e, MC_CLIENT_FRUSTUM_SOURCE_NULL, "actual sr null receiver", "sr", "a",
                      "(Leq;)Z", 0, false);
    if (box == NULL)
        return result(e, MC_CLIENT_FRUSTUM_SOURCE_NULL, "actual sr null bounds", "sr", "a",
                      "(Leq;)Z", 2, false);
    return mc_client_source_frustum_box(o, box->min_x, box->min_y, box->min_z, box->max_x,
                                        box->max_y, box->max_z, out, e);
}

mc_client_frustum_status mc_client_source_frustum_retire(mc_client_source_frustum **slot,
                                                         mc_client_culling_error *e) {
    if (slot == NULL)
        return result(e, MC_CLIENT_FRUSTUM_ARGUMENT, "sr retirement slot", NULL, NULL, NULL, 0,
                      true);
    mc_client_source_frustum *o = *slot;
    if (o == NULL)
        return result(e, MC_CLIENT_FRUSTUM_OK, NULL, NULL, NULL, NULL, 0, false);
    if (o->state.active)
        return result(e, MC_CLIENT_FRUSTUM_BUSY, "sr active retirement", NULL, NULL, NULL, 0, true);
    if (o->state.holds > 1) {
        --o->state.holds;
        *slot = NULL;
    } else {
        if (mc_client_render_classes_release(o->models) != MC_CLIENT_MODEL_OK)
            return result(e, MC_CLIENT_FRUSTUM_BUSY, "sr Models release", NULL, NULL, NULL, 0,
                          true);
        free(o);
        *slot = NULL;
    }
    return result(e, MC_CLIENT_FRUSTUM_OK, NULL, NULL, NULL, NULL, 0, false);
}

static bool cull_fail(mc_client_culling_call *c, mc_client_frustum_status status,
                      const char *operation, unsigned int pc, bool native) {
    result(&c->failure, status, operation, "n", "a", "(Lyn;F)V", pc, native);
    c->state.source_pc = pc;
    return false;
}

static mc_client_source_dk *get(mc_client_culling_call *c, unsigned int pc) {
    mc_client_render_global *n = c->state.owner;
    if (n->fields.o == NULL) {
        cull_fail(c, MC_CLIENT_FRUSTUM_SOURCE_NULL, "actual N null DK array", pc, false);
        return NULL;
    }
    if (c->state.index < 0 || (size_t)c->state.index >= n->o_length) {
        cull_fail(c, MC_CLIENT_FRUSTUM_CHILD_FAILED, "actual N DK array index", pc, false);
        return NULL;
    }
    mc_client_source_dk *dk = n->fields.o[c->state.index];
    if (dk == NULL) {
        cull_fail(c, MC_CLIENT_FRUSTUM_SOURCE_NULL, "actual N null DK receiver",
                  pc == 16   ? 17
                  : pc == 28 ? 29
                             : 54,
                  false);
        return NULL;
    }
    bool owned = false;
    for (mc_client_source_dk *p = n->chunk_owners; p != NULL; p = p->next)
        if (p == dk) {
            owned = true;
            break;
        }
    if (!owned) {
        cull_fail(c, MC_CLIENT_FRUSTUM_ARGUMENT, "N DK native occurrence provenance", pc, true);
        return NULL;
    }
    c->state.chunk = dk;
    return dk;
}

static bool cull_body(mc_client_culling_call *c) {
    mc_client_render_global *n = c->state.owner;
    for (c->state.index = 0;; c->state.index = mc_java_i32_add(c->state.index, 1)) {
        if (n->fields.o == NULL)
            return cull_fail(c, MC_CLIENT_FRUSTUM_SOURCE_NULL, "actual N null DK array length", 7,
                             false);
        if (n->o_length > INT32_MAX)
            return cull_fail(c, MC_CLIENT_FRUSTUM_LIMIT, "N DK array native extent", 7, true);
        if ((size_t)c->state.index >= n->o_length)
            break;
        mc_client_source_dk *dk = get(c, 16);
        if (dk == NULL)
            return false;
        ++c->state.visited;
        if (dk->fields.E) {
            if (dk->fields.p == NULL) {
                cull_fail(c, MC_CLIENT_FRUSTUM_SOURCE_NULL, "actual dk.e null pass array", 17,
                          false);
                c->failure.child = (mc_client_graphics_site){"dk", "e", "()Z", 14};
                return false;
            }
            if (dk->fields.p[0] && dk->fields.p[1]) {
                ++c->state.empty_skips;
                continue;
            }
        }
        dk = get(c, 28);
        if (dk == NULL)
            return false;
        if (dk->fields.o && (((uint32_t)mc_java_i32_add(c->state.index, n->fields.j) & 15u) != 0)) {
            ++c->state.throttle_skips;
            continue;
        }
        dk = get(c, 52);
        if (dk == NULL)
            return false;
        ++c->state.tested;
        bool visible;
        mc_client_culling_error child;
        if (c->state.frustum == NULL) {
            cull_fail(c, MC_CLIENT_FRUSTUM_SOURCE_NULL, "actual dk.a null frustum receiver", 54,
                      false);
            c->failure.child = (mc_client_graphics_site){"dk", "a", "(Lyn;)V", 6};
            return false;
        }
        const mc_client_frustum_status status =
            mc_client_source_frustum_bounds(c->state.frustum, dk->fields.v, &visible, &child);
        if (status != MC_CLIENT_FRUSTUM_OK) {
            cull_fail(c, status, child.operation, 54, child.native_policy);
            c->failure.child = child.site;
            c->failure.frustum = child.frustum;
            return false;
        }
        dk->fields.o = visible;
        dk->last_store_pc = 11;
        dk->source_pc = 14;
        ++c->state.stores;
    }
    n->fields.j = mc_java_i32_add(n->fields.j, 1);
    c->state.final_j = n->fields.j;
    c->state.source_pc = 73;
    c->state.returned = true;
    return true;
}

mc_client_frustum_status mc_client_render_global_cull_begin(mc_client_render_global *n,
                                                            mc_client_source_frustum *frustum,
                                                            float partial,
                                                            mc_client_culling_call **out,
                                                            mc_client_culling_error *e) {
    (void)partial;
    if (n == NULL || out == NULL || *out != NULL)
        return result(e, MC_CLIENT_FRUSTUM_ARGUMENT, "N cull native owner/output", NULL, NULL, NULL,
                      0, true);
    mc_client_render_global_world_state attachment;
    const bool blocked_attachment =
        n->world_call != NULL &&
        (!mc_client_render_global_world_inspect(n->world_call, &attachment) || attachment.active ||
         !attachment.source_returned);
    if (n->active || n->phase == MC_CLIENT_RENDER_GLOBAL_RETIRING || blocked_attachment ||
        n->sky_call != NULL || n->cull_call != NULL || n->update_call != NULL ||
        n->submit_call != NULL || n->entities_call != NULL ||
        (frustum != NULL && (frustum->state.active || !frustum->state.constructed)))
        return result(e, MC_CLIENT_FRUSTUM_BUSY, "N cull active/failed native owner", NULL, NULL,
                      NULL, 0, true);
    if (frustum != NULL && frustum->state.holds == SIZE_MAX)
        return result(e, MC_CLIENT_FRUSTUM_LIMIT, "N cull sr occurrence ceiling", NULL, NULL, NULL,
                      0, true);
    mc_client_culling_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return result(e, MC_CLIENT_FRUSTUM_ALLOCATION, "N cull call allocation", NULL, NULL, NULL,
                      0, true);
    if (mc_client_render_global_retain(n, &c->owner_error) != MC_CLIENT_RENDER_GLOBAL_OK) {
        free(c);
        return result(e, MC_CLIENT_FRUSTUM_BUSY, "N cull owner occurrence", NULL, NULL, NULL, 0,
                      true);
    }
    if (frustum != NULL)
        ++frustum->state.holds;
    c->state.owner = n;
    c->state.frustum = frustum;
    c->state.initial_j = c->state.final_j = n->fields.j;
    c->state.failure = &c->failure;
    n->cull_call = c;
    *out = c;
    c->state.active = n->active = true;
    (void)cull_body(c);
    c->state.active = n->active = false;
    if (e != NULL)
        *e = c->failure;
    return c->failure.status;
}

bool mc_client_render_global_cull_inspect(const mc_client_culling_call *c,
                                          mc_client_culling_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    return true;
}

mc_client_frustum_status mc_client_render_global_cull_retire(mc_client_culling_call **slot,
                                                             mc_client_culling_error *e) {
    if (slot == NULL)
        return result(e, MC_CLIENT_FRUSTUM_ARGUMENT, "N cull retirement slot", NULL, NULL, NULL, 0,
                      true);
    mc_client_culling_call *c = *slot;
    if (c == NULL)
        return result(e, MC_CLIENT_FRUSTUM_OK, NULL, NULL, NULL, NULL, 0, false);
    if (c->state.active || c->state.owner->active)
        return result(e, MC_CLIENT_FRUSTUM_BUSY, "N cull active retirement", NULL, NULL, NULL, 0,
                      true);
    if (mc_client_source_frustum_retire(&c->state.frustum, e) != MC_CLIENT_FRUSTUM_OK)
        return e == NULL ? MC_CLIENT_FRUSTUM_BUSY : e->status;
    mc_client_render_global *owner = c->state.owner;
    if (owner->cull_call != c)
        return result(e, MC_CLIENT_FRUSTUM_BUSY, "N cull pending occurrence changed", NULL, NULL,
                      NULL, 0, true);
    owner->cull_call = NULL;
    if (mc_client_render_global_release(&c->state.owner, &c->cleanup) !=
        MC_CLIENT_RENDER_GLOBAL_OK) {
        owner->cull_call = c;
        return result(e, MC_CLIENT_FRUSTUM_BUSY, "N cull occurrence release", NULL, NULL, NULL, 0,
                      true);
    }
    free(c);
    *slot = NULL;
    return result(e, MC_CLIENT_FRUSTUM_OK, NULL, NULL, NULL, NULL, 0, false);
}
