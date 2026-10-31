#include "internal.h"
#include "mc/core/java/java.h"
#include "nw_internal.h"
#include <stdlib.h>
#include <string.h>

static mc_client_graphics_site site(const char *method, const char *descriptor, int32_t pc) {
    return (mc_client_graphics_site){"nw", method, descriptor, pc};
}

static bool construct(mc_client_tessellator *o, mc_client_graphics_error *e) {
    mc_client_nw_fields *f = &o->fields;
    f->h = 0;
    o->last_store_pc = 6;
    f->l = false;
    o->last_store_pc = 11;
    f->m = false;
    o->last_store_pc = 16;
    f->n = false;
    o->last_store_pc = 21;
    f->o = 0;
    o->last_store_pc = 26;
    f->p = 0;
    o->last_store_pc = 31;
    f->q = false;
    o->last_store_pc = 36;
    f->w = false;
    o->last_store_pc = 41;
    f->x = false;
    o->last_store_pc = 46;
    f->z = 0;
    o->last_store_pc = 51;
    f->A = 10;
    o->last_store_pc = 57;
    f->B = 2097152;
    o->last_store_pc = 64;
    mc_client_direct_byte_buffer *bytes;
    if (!mc_client_ge_c(o->classes, &o->arena, mc_java_i32_mul(f->B, 4), site("<init>", "(I)V", 69),
                        &bytes, e))
        return false;
    f->d = bytes;
    o->last_store_pc = 72;
    mc_client_direct_int_buffer *ints;
    if (!mc_client_ge_int_view(&o->arena, bytes, site("<init>", "(I)V", 80), &ints, e))
        return false;
    f->e = ints;
    o->last_store_pc = 83;
    mc_client_direct_float_buffer *floats;
    if (!mc_client_ge_float_view(&o->arena, bytes, site("<init>", "(I)V", 91), &floats, e))
        return false;
    f->f = floats;
    o->last_store_pc = 94;
    int32_t *array = mc_client_render_arena_allocate(&o->arena, (size_t)f->B * sizeof(*array),
                                                     site("<init>", "(I)V", 99), e);
    if (array == NULL)
        return false;
    f->g = array;
    o->last_store_pc = 101;
    /* Actual c105 is false in this source initializer. No GL capability query
     * or source VBO generation is reached by the normal constructor. */
    if (o->classes->nw.c) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY,
                                "source nw nondefault VBO capability branch",
                                site("<init>", "(I)V", 111), false);
        return false;
    }
    f->x = false;
    o->last_store_pc = 125;
    return true;
}

bool mc_client_nw_active_use(mc_client_render_engine_classes *c, mc_client_graphics_site caller,
                             mc_client_tessellator **out, mc_client_graphics_error *e) {
    *e = (mc_client_graphics_error){0};
    e->entry = e->caller = caller;
    if (c == NULL || out == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT, "native nw class admission",
                                caller, true);
        return false;
    }
    if (c->nw.phase == MC_CLIENT_RENDER_ENGINE_READY) {
        *out = c->nw.a;
        return true;
    }
    if (c->nw.phase == MC_CLIENT_RENDER_ENGINE_FAILED) {
        *e = c->nw.failure;
        e->dependency_class_cause = e->class_cause;
        e->class_cause = (mc_client_graphics_cause){c->nw.failure.status, c->nw.failure.operation,
                                                    c->nw.failure.entry, c->nw.failure.site,
                                                    c->nw.failure.native_policy};
        e->entry = e->caller = caller;
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_CLASS_FAILED,
                                "previous actual nw class initialization failure", caller, false);
        return false;
    }
    if (c->active_source_calls != 0 || c->nw.phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING ||
        c->resource_retirement_started) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_BUSY, "native nw initializer active/retiring",
                                caller, true);
        return false;
    }
    if (mc_client_render_engine_classes_retain(c) != MC_CLIENT_RENDER_ENGINE_OK) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_LIMIT, "native nw initializer class hold",
                                caller, true);
        return false;
    }
    ++c->active_source_calls;
    c->nw.phase = MC_CLIENT_RENDER_ENGINE_CONSTRUCTING;
    c->nw.b = true;  /* actual <clinit>1 */
    c->nw.c = false; /* actual <clinit>5 */
    e->entry = e->caller = caller;
    mc_client_tessellator *candidate = calloc(1, sizeof(*candidate));
    if (candidate == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_ALLOCATION_FAILED,
                                "nw actual singleton native allocation", site("<clinit>", "()V", 8),
                                true);
    } else {
        c->nw.candidate = candidate;
        candidate->classes = c;
        candidate->arena.options = c->nw.options;
        candidate->last_store_pc = -1;
        if (construct(candidate, e)) {
            c->nw.a = candidate; /* actual static a17, only after ctor14 */
            c->nw.candidate = NULL;
            c->nw.phase = MC_CLIENT_RENDER_ENGINE_READY;
            *out = candidate;
        } else {
            e->entry = caller;
            e->parent = site("<clinit>", "()V", 14);
        }
    }
    const bool ok = c->nw.phase == MC_CLIENT_RENDER_ENGINE_READY;
    if (!ok) {
        c->nw.phase = MC_CLIENT_RENDER_ENGINE_FAILED;
        c->nw.failure = *e;
    }
    --c->active_source_calls;
    (void)mc_client_render_engine_classes_release(c);
    return ok;
}

bool mc_client_nw_inspect(const mc_client_render_engine_classes *c, mc_client_nw_state *out) {
    if (c == NULL || out == NULL)
        return false;
    const mc_client_tessellator *o = c->nw.a == NULL ? c->nw.candidate : c->nw.a;
    *out = (mc_client_nw_state){.phase = c->nw.phase,
                                .static_b = c->nw.b,
                                .static_c = c->nw.c,
                                .a = c->nw.a,
                                .failed_candidate = c->nw.candidate,
                                .failure = c->nw.failure,
                                .last_store_pc = o == NULL ? -1 : o->last_store_pc};
    if (o != NULL) {
        out->fields = o->fields;
        out->constructor_bytes = o->arena.bytes;
        out->constructor_allocations = o->arena.allocations;
    }
    return true;
}

void mc_client_nw_free_namespace(mc_client_render_engine_classes *c) {
    mc_client_tessellator *o = c->nw.a == NULL ? c->nw.candidate : c->nw.a;
    if (o != NULL) {
        mc_client_render_arena_free(&o->arena);
        free(o);
    }
    c->nw.a = c->nw.candidate = NULL;
}
