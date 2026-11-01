#include "render_internal.h"

#include "../../entity/entity_internal.h"
#include "../../entity/particles/particle_internal.h"
#include "../render/interpolation_internal.h"
#include "../render_engine/nw.h"
#include "mc/core/java/java.h"
#include <stdlib.h>
#include <string.h>

typedef struct mc_effect_item_draw {
    mc_entity_render_error brightness;
    mc_world_source_chunk_error source_chunk;
    mc_world_access_failure receipt;
} mc_effect_item_draw;

static bool item_failed(mc_client_effect_draw_call *c, unsigned int invocation,
                        mc_effect_draw_status status, const char *operation, unsigned int pc,
                        bool native) {
    mc_effect_draw_fail(c, status, operation, invocation, native);
    c->state.particle_pc = pc;
    c->failure.child = (mc_client_graphics_site){
        mc_entity_is_kind(c->current, MC_ENTITY_CLASS_KIND_DIGGING_PARTICLE) ? "qm" : "pb", "a",
        "(Lnw;FFFFFF)V", (int32_t)pc};
    if (!native && status == MC_EFFECT_DRAW_SOURCE_NULL) {
        c->failure.source_exception = true;
        c->failure.exception_class = "java/lang/NullPointerException";
        c->failure.throwable = MC_PARTICLE_SOURCE_EXCEPTION;
    }
    return false;
}

static bool item_fp(mc_client_effect_draw_call *c, unsigned int invocation, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           item_failed(c, invocation, MC_EFFECT_DRAW_UNSUPPORTED_FP,
                       "PB reached strict floating controls", pc, true);
}

/* These views point to the actual producer. Copy only the reached loader
 * value; the retained call independently owns the two pre-query World slots. */
static bool brightness_capture(mc_client_effect_draw_call *c) {
    if (c->item == NULL)
        return true;
    mc_entity_render_error *e = &c->item->brightness;
    const mc_world_source_chunk_error **source = NULL;
    const mc_world_access_failure *receipt = NULL;
    if (e->selected_child == MC_ENTITY_RENDER_CHILD_BRIGHTNESS)
        source = &e->brightness.query.source_chunk;
    else if (e->selected_child == MC_ENTITY_RENDER_CHILD_AREA) {
        source = &e->area.query.source_chunk;
        receipt = &e->area.world_access_failure;
    }
    if (source != NULL && *source != NULL && *source != &c->item->source_chunk) {
        c->item->source_chunk = **source;
        *source = &c->item->source_chunk;
    }
    if (receipt != NULL && receipt->receipt != NULL && c->item->receipt.receipt == NULL) {
        const mc_world_storage_status status =
            mc_world_access_failure_retain(receipt, &c->item->receipt);
        if (status != MC_WORLD_STORAGE_OK) {
            if (c->cleanup.receipt_status == MC_WORLD_STORAGE_OK)
                c->cleanup.receipt_status = status;
            return false;
        }
    }
    return true;
}

bool mc_effect_draw_item_capture(mc_client_effect_draw_call *c) {
    return brightness_capture(c);
}

bool mc_effect_draw_item_discharge(mc_client_effect_draw_call *c) {
    if (c->item == NULL)
        return true;
    if (!brightness_capture(c) ||
        mc_entity_render_worlds_release(&c->item->brightness) != MC_ENTITY_OK)
        return false;
    mc_world_access_failure_release(&c->item->receipt);
    return true;
}

void mc_effect_draw_item_free(mc_client_effect_draw_call *c) {
    if (c->item == NULL)
        return;
    c->state.owner->allocation_bytes -= sizeof(*c->item);
    free(c->item);
    c->item = NULL;
}

static bool item_brightness(mc_client_effect_draw_call *c, unsigned int invocation) {
    if (!mc_effect_draw_capture_math(c, 139))
        return item_failed(c, invocation, c->failure.status, c->failure.operation, 139, true);
    c->failure.entity_render = &c->item->brightness;
    const bool returned = mc_entity_render_brightness(c->current, c->math, c->state.partial,
                                                      &c->state.brightness, &c->item->brightness);
    if (!returned) {
        const mc_entity_render_error *e = &c->item->brightness;
        item_failed(c, invocation, MC_EFFECT_DRAW_CHILD_FAILED, e->operation, 139,
                    e->native_policy);
        c->failure.source_exception = e->source_exception;
        c->failure.exception_class = e->exception_class;
        c->failure.throwable =
            e->throwable == MC_ENTITY_RENDER_SOURCE_EXCEPTION ? MC_PARTICLE_SOURCE_EXCEPTION
            : e->throwable == MC_ENTITY_RENDER_SOURCE_ERROR   ? MC_PARTICLE_SOURCE_ERROR
            : e->throwable == MC_ENTITY_RENDER_UNCLASSIFIED_THROWABLE
                ? MC_PARTICLE_UNCLASSIFIED_THROWABLE
                : MC_PARTICLE_NO_THROWABLE;
        if (!brightness_capture(c)) {
            const mc_effect_draw_status status =
                c->cleanup.receipt_status == MC_WORLD_STORAGE_LIMIT ? MC_EFFECT_DRAW_LIMIT
                : c->cleanup.receipt_status == MC_WORLD_STORAGE_ALLOCATION_FAILED
                    ? MC_EFFECT_DRAW_ALLOCATION
                    : MC_EFFECT_DRAW_CLEANUP_FAILED;
            c->cleanup.status = status;
            c->cleanup.operation = "PB brightness producer occurrence capture";
            c->cleanup.native_policy = true;
            /* Native ownership admission takes precedence over the child's
             * Exception. Its complete typed Source cause stays in item. */
            item_failed(c, invocation, status, c->cleanup.operation, 139, true);
            c->failure.receipt_status = c->cleanup.receipt_status;
            c->failure.source_exception = false;
            c->failure.exception_class = NULL;
            c->failure.throwable = MC_PARTICLE_NO_THROWABLE;
        }
        return false;
    }
    c->state.brightness_returned = true;
    return true;
}

static bool item_graphics(mc_client_effect_draw_call *c, unsigned int invocation, bool ok,
                          unsigned int pc) {
    if (ok)
        return true;
    const mc_client_graphics_error *g = &c->failure.graphics;
    item_failed(c, invocation, MC_EFFECT_DRAW_CHILD_FAILED, g->operation, pc, g->native_policy);
    if (!g->native_policy) {
        const char *name = g->status == MC_CLIENT_GRAPHICS_NULL_REFERENCE
                               ? "java/lang/NullPointerException"
                           : g->status == MC_CLIENT_GRAPHICS_ARRAY_INDEX
                               ? "java/lang/ArrayIndexOutOfBoundsException"
                           : g->status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT
                               ? "java/lang/IllegalArgumentException"
                           : g->status == MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE
                               ? "java/lang/IllegalStateException"
                               : NULL;
        c->failure.exception_class = name;
        c->failure.source_exception = name != NULL;
        c->failure.throwable =
            name == NULL ? MC_PARTICLE_UNCLASSIFIED_THROWABLE : MC_PARTICLE_SOURCE_EXCEPTION;
    }
    return false;
}

bool mc_effect_draw_item(mc_client_effect_draw_call *c, unsigned int invocation) {
    if (c->item == NULL) {
        mc_client_effect_renderer *owner = c->state.owner;
        if (owner->allocation_bytes > owner->max_allocation_bytes ||
            sizeof(*c->item) > owner->max_allocation_bytes - owner->allocation_bytes)
            return item_failed(c, invocation, MC_EFFECT_DRAW_LIMIT,
                               "PB reached draw scratch ceiling", 0, true);
        c->item = calloc(1, sizeof(*c->item));
        if (c->item == NULL)
            return item_failed(c, invocation, MC_EFFECT_DRAW_ALLOCATION,
                               "PB reached draw scratch allocation", 0, true);
        owner->allocation_bytes += sizeof(*c->item);
        mc_entity_render_error_init(&c->item->brightness);
    }
    mc_particle_fields *f = &c->current->particle->fields;
    mc_effect_draw_state *s = &c->state;
    s->item_particle_returned = false;
    s->item_vertices_returned = 0;
    s->brightness_returned = s->color_set = false;
    const int32_t icon_x = f->b % 16;
    if (!item_fp(c, invocation, 7))
        return false;
    s->item_u0 = at_fdiv(at_fadd((float)icon_x, at_fdiv(f->c, 4.0f)), 16.0f);
    s->item_u1 = at_fadd(s->item_u0, .015609375f);
    const int32_t icon_y = f->b / 16;
    s->item_v0 = at_fdiv(at_fadd((float)icon_y, at_fdiv(f->d, 4.0f)), 16.0f);
    s->item_v1 = at_fadd(s->item_v0, .015609375f);
    s->item_half_size = at_fmul(.1f, f->g);

    static const int32_t sub[3] = {77, 101, 125}, widen[3] = {79, 103, 127};
    static const int32_t mul[3] = {80, 104, 128}, add[3] = {81, 105, 129};
    static const unsigned int origin_pc[3] = {82, 106, 130}, narrow_pc[3] = {86, 110, 134};
    for (size_t axis = 0; axis < 3; ++axis) {
        const mc_entity_base_fields *base = &c->current->base;
        const double previous = axis == 0 ? base->aJ : axis == 1 ? base->aK : base->aL;
        const double current = axis == 0 ? base->x : axis == 1 ? base->y : base->z;
        const double previous_again = axis == 0 ? base->aJ : axis == 1 ? base->aK : base->aL;
        mc_client_interpolation_error arithmetic;
        /* Fresh source previous operands surround the current coordinate read. */
        if (!item_fp(c, invocation, (unsigned int)sub[axis]))
            return false;
        double coordinate;
        if (!mc_client_interpolation_double_nan_admit(current, previous_again, sub[axis],
                                                      &arithmetic))
            return item_failed(c, invocation, MC_EFFECT_DRAW_UNSUPPORTED_NUMERIC,
                               "PB interpolation unmeasured signaling operands",
                               (unsigned int)sub[axis], true);
        const double delta = mc_client_interpolation_dsub(current, previous_again);
        const double partial = mc_client_interpolation_widen(s->partial);
        if (!mc_client_interpolation_double_nan_admit(delta, partial, mul[axis], &arithmetic))
            return item_failed(c, invocation, MC_EFFECT_DRAW_UNSUPPORTED_NUMERIC,
                               "PB interpolation unmeasured signaling operands",
                               (unsigned int)mul[axis], true);
        const double product = mc_client_interpolation_dmul(delta, partial);
        if (!mc_client_interpolation_double_nan_admit(previous, product, add[axis], &arithmetic))
            return item_failed(c, invocation, MC_EFFECT_DRAW_UNSUPPORTED_NUMERIC,
                               "PB interpolation unmeasured signaling operands",
                               (unsigned int)add[axis], true);
        coordinate = mc_client_interpolation_dadd(previous, product);
        (void)widen[axis]; /* Actual f2d argument is performed above before dmul. */
        mc_particle_origin origin;
        if (mc_entity_particle_origin_read(c->state.owner->classes, &origin, &c->failure.entity) !=
            MC_ENTITY_OK)
            return item_failed(c, invocation, MC_EFFECT_DRAW_CHILD_FAILED,
                               c->failure.entity.operation, origin_pc[axis], true);
        const double offset = axis == 0 ? origin.l : axis == 1 ? origin.m : origin.n;
        if (!mc_client_interpolation_double_nan_admit(coordinate, offset,
                                                      (int32_t)narrow_pc[axis] - 1, &arithmetic))
            return item_failed(c, invocation, MC_EFFECT_DRAW_UNSUPPORTED_NUMERIC,
                               "PB origin subtraction unmeasured signaling operands",
                               narrow_pc[axis] - 1u, true);
        s->item_center[axis] = at_narrow(mc_client_interpolation_dsub(coordinate, offset));
        s->particle_pc = narrow_pc[axis];
    }
    s->draw_x = at_widen(s->item_center[0]);
    s->draw_y = at_widen(s->item_center[1]);
    s->draw_z = at_widen(s->item_center[2]);
    if (!item_brightness(c, invocation))
        return false;
    s->item_color[0] = at_fmul(s->brightness, f->i);
    s->item_color[1] = at_fmul(s->brightness, f->j);
    s->item_color[2] = at_fmul(s->brightness, f->k);
    if (c->nw == NULL)
        return item_failed(c, invocation, MC_EFFECT_DRAW_SOURCE_NULL, "PB null NW color receiver",
                           166, false);
    if (!item_graphics(c, invocation,
                       mc_client_nw_color_float(c->nw, s->item_color[0], s->item_color[1],
                                                s->item_color[2], &c->failure.graphics),
                       166))
        return false;
    s->color_set = true;
    static const unsigned int calls[4] = {214, 262, 310, 358};
    for (size_t i = 0; i < 4; ++i) {
        const bool right = i >= 2, top = i == 1 || i == 2;
        const float size = s->item_half_size;
        float x = right ? at_fadd(s->item_center[0], at_fmul(s->cos_yaw, size))
                        : at_fsub(s->item_center[0], at_fmul(s->cos_yaw, size));
        x = top ? at_fadd(x, at_fmul(s->horizontal_y, size))
                : at_fsub(x, at_fmul(s->horizontal_y, size));
        const float y = top ? at_fadd(s->item_center[1], at_fmul(s->cos_pitch, size))
                            : at_fsub(s->item_center[1], at_fmul(s->cos_pitch, size));
        float z = right ? at_fadd(s->item_center[2], at_fmul(s->sin_yaw, size))
                        : at_fsub(s->item_center[2], at_fmul(s->sin_yaw, size));
        z = top ? at_fadd(z, at_fmul(s->horizontal_x, size))
                : at_fsub(z, at_fmul(s->horizontal_x, size));
        const float u = right ? s->item_u1 : s->item_u0;
        const float v = top ? s->item_v0 : s->item_v1;
        const float values[5] = {x, y, z, u, v};
        memcpy(s->item_vertices[i], values, sizeof(values));
        if (!item_graphics(c, invocation,
                           mc_client_nw_vertex_uv(c->nw, at_widen(x), at_widen(y), at_widen(z),
                                                  at_widen(u), at_widen(v), &c->failure.graphics),
                           calls[i]))
            return false;
        ++s->item_vertices_returned;
        s->particle_pc = calls[i];
    }
    s->particle_pc = 361;
    s->item_particle_returned = true;
    /* Completed PB diagnostics are passive scalar values. Release pre-query
     * World occurrences before this working child may be reused for another PB. */
    if (mc_entity_render_worlds_release(&c->item->brightness) != MC_ENTITY_OK)
        return item_failed(c, invocation, MC_EFFECT_DRAW_CLEANUP_FAILED,
                           "PB completed brightness World occurrence retirement", 361, true);
    c->failure.entity_render = NULL;
    return true;
}
