#include "../../entity/player/tick_math_internal.h"
#include "../application_state.h"
#include "../display/source_surface.h"
#include "../render_engine/bounds_draw.h"
#include "../render_engine/chunk_statistics.h"
#include "../render_engine/internal.h"
#include "mc/world/runtime/runtime.h"
#include "source_chunk_renderer_internal.h"

static const char *const CTOR = "(Lfd;Ljava/util/List;IIIII)V";

static bool fail(mc_client_source_dk *o, mc_client_render_global_error *e,
                 mc_client_render_global_status status, const char *op, const char *method,
                 const char *desc, unsigned int pc, bool native) {
    o->source_pc = pc;
    mc_render_global_source_fail(e, status, op, "dk", method, desc, pc, native);
    o->owner->failure = *e;
    o->failure = &o->owner->failure;
    return false;
}

static bool graphics(mc_client_source_dk *o, mc_client_render_global_error *e, const char *method,
                     const char *desc, unsigned int pc) {
    e->child = e->graphics.site;
    return fail(o, e,
                e->graphics.status == MC_CLIENT_GRAPHICS_ALLOCATION_FAILED
                    ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                : e->graphics.status == MC_CLIENT_GRAPHICS_LIMIT
                    ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                    : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                e->graphics.operation, method, desc, pc, e->graphics.native_policy);
}

static bool display(mc_client_source_dk *o, mc_client_render_global_error *e, unsigned int pc) {
    return fail(o, e,
                e->display.status == MC_CLIENT_DISPLAY_BUSY
                    ? MC_CLIENT_RENDER_GLOBAL_BUSY
                    : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                e->display.operation, "a", "(III)V", pc, true);
}

mc_client_source_dk *mc_source_dk_allocate(mc_client_render_global *n,
                                           mc_client_render_global_error *e) {
    const mc_client_graphics_site site = {"n", "a", "()V", 312};
    if (!mc_client_chunk_renderer_active_use(n->classes, site, &e->graphics)) {
        e->child = e->graphics.site;
        mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                                     e->graphics.operation, "n", "a", "()V", 312,
                                     e->graphics.native_policy);
        return NULL;
    }
    mc_client_source_dk *o = mc_render_global_allocate(n, sizeof(*o), site, e);
    if (o != NULL) {
        o->owner = n;
        o->next = n->chunk_owners;
        n->chunk_owners = o;
    }
    return o;
}

bool mc_source_dk_clear(mc_client_source_dk *o, mc_client_render_global_error *e) {
    if (o->fields.p == NULL)
        return fail(o, e, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual DK boolean array", "b",
                    "()V", 13, false);
    o->fields.p[0] = true;
    o->fields.p[1] = true;
    o->fields.o = false;
    o->fields.E = false;
    o->source_pc = 30;
    o->last_store_pc = 27;
    return true;
}

bool mc_source_dk_close(mc_client_source_dk *o, mc_client_render_global_error *e) {
    if (!mc_source_dk_clear(o, e)) {
        e->parent = (mc_client_graphics_site){"dk", "c", "()V", 1};
        o->owner->failure = *e;
        return false;
    }
    mc_world_runtime *previous = o->world_held ? o->fields.a : NULL;
    o->fields.a = NULL; /* Actual c() putfield6, after b()1. */
    o->world_held = false;
    o->last_store_pc = 6;
    o->source_pc = 9;
    mc_world_runtime_binding_release(previous);
    return true;
}

bool mc_source_dk_render_list(mc_client_source_dk *o, int32_t pass, int32_t *out,
                              mc_client_render_global_error *e) {
    if (o == NULL || out == NULL || e == NULL)
        return false; /* Internal entered-leaf argument contract. */
    if (!o->fields.o) {
        *out = -1;
        o->source_pc = 8;
        return true;
    }
    if (o->fields.p == NULL)
        return fail(o, e, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual DK pass array", "a",
                    "(I)I", 14, false);
    if (pass < 0 || pass >= 2) {
        mc_client_graphics_fail(&e->graphics, MC_CLIENT_GRAPHICS_ARRAY_INDEX,
                                "actual DK pass array index",
                                (mc_client_graphics_site){"dk", "a", "(I)I", 14}, false);
        e->graphics.actual_index = pass;
        e->child = e->graphics.site;
        return fail(o, e, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, "actual DK pass array index",
                    "a", "(I)I", 14, false);
    }
    if (o->fields.p[pass]) {
        *out = -1;
        o->source_pc = 26;
    } else {
        *out = mc_java_i32_add(o->fields.C, pass);
        o->source_pc = 24;
    }
    return true;
}

bool mc_source_dk_empty(mc_client_source_dk *o, bool *out, mc_client_render_global_error *e) {
    if (o == NULL || out == NULL || e == NULL)
        return false;
    if (!o->fields.E) {
        *out = false;
        o->source_pc = 8;
        return true;
    }
    if (o->fields.p == NULL)
        return fail(o, e, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE, "actual DK empty pass array", "e",
                    "()Z", 14, false);
    *out = o->fields.p[0] && o->fields.p[1];
    o->source_pc = 32;
    return true;
}

bool mc_source_dk_call_bounds(mc_client_source_dk *o, mc_client_render_global_error *e) {
    if (o == NULL || e == NULL)
        return false;
    const int32_t id = mc_java_i32_add(o->fields.C, 2);
    o->source_pc = 6;
    if (!mc_client_display_list_call(o->owner->display, id, &e->display))
        return fail(o, e,
                    e->display.status == MC_CLIENT_DISPLAY_BUSY ? MC_CLIENT_RENDER_GLOBAL_BUSY
                    : e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP
                        ? MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
                    : e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL
                        ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
                        : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                    e->display.operation, "d", "()V", 6, true);
    o->source_pc = 9;
    return true;
}

void mc_source_dk_dirty(mc_client_source_dk *o) {
    o->fields.u = true;
    o->last_store_pc = 2;
    o->source_pc = 5;
}

bool mc_source_dk_position(mc_client_source_dk *o, int32_t x, int32_t y, int32_t z,
                           mc_client_render_global_error *e) {
    mc_client_source_dk_fields *f = &o->fields;
    if (x == f->c && y == f->d && z == f->e) {
        o->source_pc = 24;
        return true;
    }
    if (!mc_source_dk_clear(o, e)) {
        e->parent = (mc_client_graphics_site){"dk", "a", "(III)V", 26};
        o->owner->failure = *e;
        return false;
    }
    f->c = x;
    o->last_store_pc = 31;
    f->d = y;
    o->last_store_pc = 36;
    f->e = z;
    o->last_store_pc = 41;
    int32_t half;
    (void)mc_java_i32_div(f->f, 2, &half);
    f->q = mc_java_i32_add(x, half);
    (void)mc_java_i32_div(f->g, 2, &half);
    f->r = mc_java_i32_add(y, half);
    (void)mc_java_i32_div(f->h, 2, &half);
    f->s = mc_java_i32_add(z, half);
    f->l = (int32_t)((uint32_t)x & 1023u);
    f->m = y;
    f->n = (int32_t)((uint32_t)z & 1023u);
    f->i = mc_java_i32_sub(x, f->l);
    f->j = mc_java_i32_sub(y, f->m);
    f->k = mc_java_i32_sub(z, f->n);
    o->last_store_pc = 130;
    if (!mc_render_global_fp(o->owner, "a", 139)) {
        e->graphics = o->owner->failure.graphics;
        return fail(o, e, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP,
                    "DK position strict floating environment", "a", "(III)V", 139, true);
    }
    mc_entity_classes *classes = o->owner->fields.t->entities;
    const double min_x = at_widen(at_fsub((float)x, 6.0f));
    const double min_y = at_widen(at_fsub((float)y, 6.0f));
    const double min_z = at_widen(at_fsub((float)z, 6.0f));
    const double max_x = at_widen(at_fadd((float)mc_java_i32_add(x, f->f), 6.0f));
    const double max_y = at_widen(at_fadd((float)mc_java_i32_add(y, f->g), 6.0f));
    const double max_z = at_widen(at_fadd((float)mc_java_i32_add(z, f->h), 6.0f));
    mc_entity_bounds *bounds = NULL;
    if (mc_entity_bounds_create(classes, min_x, min_y, min_z, max_x, max_y, max_z, &bounds,
                                &e->bounds) != MC_ENTITY_OK)
        return fail(o, e, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, e->bounds.operation, "a",
                    "(III)V", 189, true);
    mc_entity_bounds *previous = f->v;
    f->v = bounds;
    o->last_store_pc = 192;
    if (previous != NULL) {
        o->displaced_bounds = previous;
        if (mc_entity_bounds_release(classes, previous) != MC_ENTITY_OK)
            return fail(o, e, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                        "DK displaced bounds native retirement", "a", "(III)V", 192, true);
        o->displaced_bounds = NULL;
    }
    const int32_t id = mc_java_i32_add(f->C, 2);
    o->source_pc = 204;
    if (!mc_client_display_lists_begin(o->owner->display, id, &e->display))
        return display(o, e, 204);
    o->list_open = true;
    o->owner->open_list = id;
    if (!mc_render_global_fp(o->owner, "a", 211))
        return fail(o, e, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP,
                    "DK bounding list strict floating environment", "a", "(III)V", 211, true);
    if (mc_entity_bounds_pool_get(classes, at_widen(at_fsub((float)f->l, 6.0f)),
                                  at_widen(at_fsub((float)f->m, 6.0f)),
                                  at_widen(at_fsub((float)f->n, 6.0f)),
                                  at_widen(at_fadd((float)mc_java_i32_add(f->l, f->f), 6.0f)),
                                  at_widen(at_fadd((float)mc_java_i32_add(f->m, f->g), 6.0f)),
                                  at_widen(at_fadd((float)mc_java_i32_add(f->n, f->h), 6.0f)),
                                  &o->pending_pooled_bounds, &e->bounds) != MC_ENTITY_OK)
        return fail(o, e, MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED, e->bounds.operation, "a",
                    "(III)V", 276, true);
    o->source_pc = 279;
    if (!mc_client_render_bounds_draw(o->owner->classes, o->owner->display,
                                      o->pending_pooled_bounds, &e->graphics))
        return graphics(o, e, "a", "(III)V", 279);
    if (!mc_client_display_lists_end(o->owner->display, &e->display))
        return display(o, e, 282);
    o->list_open = false;
    o->owner->open_list = 0;
    if (mc_entity_bounds_release(classes, o->pending_pooled_bounds) != MC_ENTITY_OK)
        return fail(o, e, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                    "DK local pooled bounds native retirement", "a", "(III)V", 282, true);
    o->pending_pooled_bounds = NULL;
    mc_source_dk_dirty(o);
    o->source_pc = 289;
    return true;
}

bool mc_source_dk_construct(mc_client_source_dk *o, mc_world_runtime *world,
                            mc_client_render_list *list, int32_t x, int32_t y, int32_t z,
                            int32_t size, int32_t base, mc_client_render_global_error *e) {
    o->phase = MC_CLIENT_RENDER_GLOBAL_CONSTRUCTING;
    o->fields.C = -1;
    o->last_store_pc = 6;
    o->fields.o = false;
    o->last_store_pc = 11;
    o->source_pc = 16;
    o->fields.p = mc_render_global_allocate(o->owner, 2u * sizeof(bool),
                                            (mc_client_graphics_site){"dk", "<init>", CTOR, 16}, e);
    if (o->fields.p == NULL)
        goto failed;
    o->last_store_pc = 18;
    o->fields.x = true;
    o->last_store_pc = 23;
    o->fields.E = false;
    o->last_store_pc = 28;
    o->source_pc = 32;
    o->fields.B = mc_render_global_allocate(o->owner, sizeof(*o->fields.B),
                                            (mc_client_graphics_site){"dk", "<init>", CTOR, 32}, e);
    if (o->fields.B == NULL)
        goto failed;
    o->last_store_pc = 39;
    o->source_pc = 44;
    if (world != NULL && !mc_world_runtime_binding_retain(world)) {
        (void)fail(o, e, MC_CLIENT_RENDER_GLOBAL_BUSY, "DK World native occurrence", "<init>", CTOR,
                   44, true);
        goto failed;
    }
    o->fields.a = world;
    o->world_held = world != NULL;
    o->last_store_pc = 44;
    o->fields.F = list;
    o->last_store_pc = 49;
    o->fields.h = size;
    o->fields.g = size;
    o->fields.f = size;
    o->last_store_pc = 65;
    const int32_t square = mc_java_i32_mul(size, size);
    const int32_t sum = mc_java_i32_add(mc_java_i32_add(square, square), square);
    if (!mc_render_global_fp(o->owner, "<init>", 98)) {
        (void)fail(o, e, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP,
                   "DK radius strict floating environment", "<init>", CTOR, 98, true);
        goto failed;
    }
    const double widened = at_widen((float)sum);
    const double root = _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(widened)));
    volatile float radius = at_narrow(root) / 2.0f;
    o->fields.t = radius;
    o->last_store_pc = 104;
    o->fields.C = base;
    o->last_store_pc = 110;
    o->fields.c = -999;
    o->last_store_pc = 117;
    if (!mc_source_dk_position(o, x, y, z, e))
        goto failed;
    o->fields.u = false;
    o->last_store_pc = 131;
    o->source_pc = 134;
    o->constructor_returned = true;
    o->phase = MC_CLIENT_RENDER_GLOBAL_READY;
    return true;
failed:
    o->owner->failure = *e;
    o->failure = &o->owner->failure;
    o->phase = MC_CLIENT_RENDER_GLOBAL_FAILED;
    return false;
}

static mc_client_render_global_status retire_bounds(mc_client_source_dk *o, mc_entity_bounds **slot,
                                                    const char *operation,
                                                    mc_client_render_global_error *e) {
    if (*slot == NULL)
        return MC_CLIENT_RENDER_GLOBAL_OK;
    client_application *app = o->owner->fields.t;
    if (app == NULL || app->entities == NULL) {
        e->bounds.status = MC_ENTITY_INVALID_PHASE;
        return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED,
                                            "DK retained bounds require actual Entities owner",
                                            NULL, NULL, NULL, 0, true);
    }
    e->bounds.status = mc_entity_bounds_release(app->entities, *slot);
    if (e->bounds.status != MC_ENTITY_OK)
        return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_CLEANUP_FAILED, operation,
                                            NULL, NULL, NULL, 0, true);
    *slot = NULL;
    return MC_CLIENT_RENDER_GLOBAL_OK;
}

mc_client_render_global_status mc_source_dk_native_retire(mc_client_source_dk *o,
                                                          mc_client_render_global_error *e) {
    if (retire_bounds(o, &o->displaced_bounds, "DK displaced bounds native cleanup", e) !=
        MC_CLIENT_RENDER_GLOBAL_OK)
        return e->status;
    if (o->list_open) {
        if (!mc_client_display_lists_abort(o->owner->display, mc_java_i32_add(o->fields.C, 2),
                                           &e->cleanup_display))
            return mc_render_global_source_fail(e, MC_CLIENT_RENDER_GLOBAL_BUSY,
                                                "DK pending list native retirement", NULL, NULL,
                                                NULL, 0, true);
        o->list_open = false;
        o->owner->open_list = 0;
    }
    if (retire_bounds(o, &o->pending_pooled_bounds, "DK local bounds native cleanup", e) !=
        MC_CLIENT_RENDER_GLOBAL_OK)
        return e->status;
    if (retire_bounds(o, &o->fields.v, "DK field bounds native cleanup", e) !=
        MC_CLIENT_RENDER_GLOBAL_OK)
        return e->status;
    if (o->world_held) {
        mc_world_runtime_binding_release(o->fields.a);
        o->world_held = false;
        o->fields.a = NULL; /* Consumed native view, no source c() credited. */
    }
    o->phase = MC_CLIENT_RENDER_GLOBAL_RETIRING;
    return MC_CLIENT_RENDER_GLOBAL_OK;
}

bool mc_client_source_dk_inspect(const mc_client_source_dk *o, mc_client_source_dk_state *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = (mc_client_source_dk_state){o->phase,         o->fields,
                                       o->owner,         o->source_pc,
                                       o->last_store_pc, o->constructor_returned,
                                       o->list_open,     o->pending_pooled_bounds,
                                       o->failure};
    return true;
}
