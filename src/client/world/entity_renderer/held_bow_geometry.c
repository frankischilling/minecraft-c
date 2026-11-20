#include "client/render/living_draw_internal.h"
#include "entity/living/travel_math_internal.h"
#include "held_bow_geometry_internal.h"

#include <string.h>

static void at(mc_held_bow_error *e, unsigned int pc) {
    e->state.source_pc = pc;
    e->site = (mc_client_graphics_site){"ra", "a", "(Lls;Liz;)V", (int32_t)pc};
}

static bool failed(mc_held_bow_error *e, mc_held_bow_status status, const char *operation) {
    e->status = status;
    e->operation = operation;
    return false;
}

static bool fp(mc_held_bow_error *e, unsigned int pc) {
    at(e, pc);
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           failed(e, MC_HELD_BOW_FP, "Bow reached strict floating controls");
}

static bool work(mc_held_bow_error *e) {
    return e->native_scope == NULL ||
           mc_client_draw_work_step(e->native_scope, &e->capture_failure) ||
           failed(e, MC_HELD_BOW_CAPTURE_FAILED, e->capture_failure.operation);
}

static float i2f(int32_t value) {
    return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), value));
}

static double f2d(float value) {
    return _mm_cvtsd_f64(_mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(value)));
}

static bool nw_receiver(mc_held_bow_error *e) {
    if (e->state.captured_nw != NULL)
        return true;
    e->source_null_invocation = true;
    return failed(e, MC_HELD_BOW_NULL_NW, "Bow actual captured NW null invocation");
}

static bool begin(mc_held_bow_error *e, unsigned int batch, unsigned int pc) {
    e->state.batch = batch;
    e->state.strip = -1;
    at(e, pc);
    ++e->state.begin_attempts;
    if (!work(e))
        return false;
    if (!nw_receiver(e))
        return false;
    if (!mc_client_nw_begin_quads(e->state.captured_nw, &e->graphics))
        return failed(e, MC_HELD_BOW_GRAPHICS, "Bow NW.begin child");
    ++e->state.begin_returns;
    return true;
}

static bool normal(mc_held_bow_error *e, float x, float y, float z, unsigned int pc) {
    at(e, pc);
    ++e->state.normal_attempts;
    if (!work(e))
        return false;
    if (!nw_receiver(e))
        return false;
    if (!mc_client_nw_normal(e->state.captured_nw, x, y, z, &e->graphics))
        return failed(e, MC_HELD_BOW_GRAPHICS, "Bow NW.normal child");
    ++e->state.normal_returns;
    return true;
}

static bool vertex(mc_held_bow_error *e, float x, float y, float z, float u, float v,
                   unsigned int pc) {
    /* Five distinct f2d operations follow the source's operand order. Source
     * dconst0/1 and widened float0/1 have identical exact finite values here. */
    e->state.last_vertex[0] = f2d(x);
    e->state.last_vertex[1] = f2d(y);
    e->state.last_vertex[2] = f2d(z);
    e->state.last_vertex[3] = f2d(u);
    e->state.last_vertex[4] = f2d(v);
    at(e, pc);
    ++e->state.vertex_attempts;
    if (!work(e))
        return false;
    if (!nw_receiver(e))
        return false;
    const double *p = e->state.last_vertex;
    if (!mc_client_nw_vertex_uv(e->state.captured_nw, p[0], p[1], p[2], p[3], p[4], &e->graphics))
        return failed(e, MC_HELD_BOW_GRAPHICS, "Bow NW.vertex child");
    ++e->state.vertex_returns;
    return true;
}

static bool draw(mc_held_bow_error *e, unsigned int pc) {
    at(e, pc);
    ++e->state.draw_attempts;
    if (!work(e))
        return false;
    if (!nw_receiver(e))
        return false;
    mc_held_bow_batch_receipt *receipt = &e->batches[e->state.batch];
    receipt->attempted = true;
    (void)mc_client_nw_fields_get(e->state.captured_nw, &receipt->before);
    const bool returned = mc_client_nw_draw(e->state.captured_nw, e->state.display, &e->graphics);
    receipt->returned = returned;
    if (returned)
        ++e->state.draw_returns;
    (void)mc_client_nw_fields_get(e->state.captured_nw, &receipt->after);
    const mc_client_nw_fields *before = &receipt->before;
    if (before->g != NULL && before->o >= 0 && before->o <= before->B && before->o <= 768) {
        receipt->word_count = (size_t)before->o;
        memcpy(receipt->words, before->g, receipt->word_count * sizeof(*receipt->words));
        receipt->captured = true;
    }
    if (!returned)
        return failed(e, MC_HELD_BOW_GRAPHICS, "Bow NW.draw child");
    if (!receipt->captured) {
        e->capture_failure.status = MC_CLIENT_MANAGER_LIMIT;
        e->capture_failure.native_policy = true;
        e->capture_failure.operation = "Bow reached batch receipt extent";
        return failed(e, MC_HELD_BOW_CAPTURE_FAILED, e->capture_failure.operation);
    }
    return true;
}

static float back(float thickness) {
    return mc_travel_fsub(0.0f, thickness);
}

static void initialize(mc_client_draw_scope *scope, int32_t icon, mc_client_tessellator *nw,
                       mc_client_render_engine_classes *graphics, mc_client_display *display,
                       mc_held_bow_error *e) {
    memset(e, 0, sizeof(*e));
    e->site.pc = -1;
    e->state.strip = -1;
    e->state.icon = icon;
    e->state.captured_nw = nw;
    e->state.graphics = graphics;
    e->state.display = display;
    e->native_scope = scope;
}

static bool coordinates(mc_held_bow_error *e) {
    /* Positive divisor16 cannot trigger C/Java INT_MIN/-1 overflow. C17 signed
     * / and % truncate toward zero exactly as Java idiv/irem. Multiplication
     * still uses the established Java wrapping kernel. Never floor-mod icons. */
    e->state.remainder16 = e->state.icon % 16;
    e->state.column_pixels = mc_java_i32_mul(e->state.remainder16, 16);
    if (!fp(e, 143))
        return false;
    e->state.u0 = mc_travel_fdiv(mc_travel_fadd(i2f(e->state.column_pixels), 0.0f), 256.0f);
    at(e, 149);
    e->state.u1 = mc_travel_fdiv(mc_travel_fadd(i2f(e->state.column_pixels), 15.99f), 256.0f);
    at(e, 166);
    e->state.quotient16 = e->state.icon / 16;
    e->state.row_pixels = mc_java_i32_mul(e->state.quotient16, 16);
    e->state.v0 = mc_travel_fdiv(mc_travel_fadd(i2f(e->state.row_pixels), 0.0f), 256.0f);
    at(e, 182);
    e->state.v1 = mc_travel_fdiv(mc_travel_fadd(i2f(e->state.row_pixels), 15.99f), 256.0f);
    at(e, 199);
    e->state.span = 1.0f;
    return true;
}

/* Same entered NW groups for both RA callers. The CPU component entry below
 * supplies an actual selected group, without executing GL or inventing draw. */
static bool emit_batch(mc_held_bow_error *e, unsigned int batch) {
    const float u0 = e->state.u0, u1 = e->state.u1, v0 = e->state.v0, v1 = e->state.v1;
    const float span = e->state.span, thickness = e->state.thickness;
    if (batch == 0)
        return begin(e, 0, 268) && normal(e, 0, 0, 1, 275) && vertex(e, 0, 0, 0, u1, v1, 288) &&
               vertex(e, span, 0, 0, u0, v1, 303) && vertex(e, span, 1, 0, u0, v0, 318) &&
               vertex(e, 0, 1, 0, u1, v0, 331);
    if (batch == 1)
        return begin(e, 1, 339) && normal(e, 0, 0, -1, 347) &&
               vertex(e, 0, 1, back(thickness), u1, v0, 364) &&
               vertex(e, span, 1, back(thickness), u0, v0, 383) &&
               vertex(e, span, 0, back(thickness), u0, v1, 402) &&
               vertex(e, 0, 0, back(thickness), u1, v1, 419);
    static const unsigned int begins[] = {427, 558, 691, 828};
    static const unsigned int normals[] = {435, 565, 698, 836};
    static const unsigned int conversions[] = {450, 580, 713, 851};
    static const unsigned int vertices[4][4] = {
        {495, 510, 525, 544}, {628, 642, 657, 677}, {757, 774, 795, 814}, {894, 909, 928, 949}};
    const size_t group = batch - 2u;
    if (!begin(e, (unsigned int)group + 2u, begins[group]))
        return false;
    const float nx = group == 0 ? -1.0f : group == 1 ? 1.0f : 0.0f;
    const float ny = group == 2 ? 1.0f : group == 3 ? -1.0f : 0.0f;
    if (!normal(e, nx, ny, 0, normals[group]))
        return false;
    for (int32_t strip = 0; strip < 16; ++strip) {
        e->state.strip = strip;
        at(e, conversions[group]);
        if (!work(e))
            return false;
        if (!fp(e, conversions[group]))
            return false;
        const float fraction = mc_travel_fdiv(i2f(strip), 16.0f);
        e->state.fraction = fraction;
        const float hi = group < 2 ? u1 : v1, lo = group < 2 ? u0 : v0;
        const float difference = mc_travel_fsub(lo, hi);
        const float product = mc_travel_fmul(difference, fraction);
        const float interpolated = mc_travel_fadd(hi, product);
        const float uv = mc_travel_fsub(interpolated, .001953125f);
        e->state.strip_uv = uv;
        float position = mc_travel_fmul(span, fraction);
        if (group == 1 || group == 2)
            position = mc_travel_fadd(position, .0625f);
        e->state.strip_position = position;
        const unsigned int *p = vertices[group];
        if (group == 0) {
            if (!vertex(e, position, 0, back(thickness), uv, v1, p[0]) ||
                !vertex(e, position, 0, 0, uv, v1, p[1]) ||
                !vertex(e, position, 1, 0, uv, v0, p[2]) ||
                !vertex(e, position, 1, back(thickness), uv, v0, p[3]))
                return false;
        } else if (group == 1) {
            if (!vertex(e, position, 1, back(thickness), uv, v0, p[0]) ||
                !vertex(e, position, 1, 0, uv, v0, p[1]) ||
                !vertex(e, position, 0, 0, uv, v1, p[2]) ||
                !vertex(e, position, 0, back(thickness), uv, v1, p[3]))
                return false;
        } else if (group == 2) {
            if (!vertex(e, 0, position, 0, u1, uv, p[0]) ||
                !vertex(e, span, position, 0, u0, uv, p[1]) ||
                !vertex(e, span, position, back(thickness), u0, uv, p[2]) ||
                !vertex(e, 0, position, back(thickness), u1, uv, p[3]))
                return false;
        } else {
            if (!vertex(e, span, position, 0, u0, uv, p[0]) ||
                !vertex(e, 0, position, 0, u1, uv, p[1]) ||
                !vertex(e, 0, position, back(thickness), u1, uv, p[2]) ||
                !vertex(e, span, position, back(thickness), u0, uv, p[3]))
                return false;
        }
    }
    return true;
}

bool entity_renderer_held_item_geometry_batch(int32_t icon, unsigned int batch,
                                              mc_client_tessellator *nw,
                                              mc_client_render_engine_classes *graphics,
                                              mc_held_bow_error *e) {
    if (e == NULL)
        return false;
    initialize(NULL, icon, nw, graphics, NULL, e);
    if (graphics == NULL || batch >= 6)
        return failed(e, MC_HELD_BOW_ARGUMENT, "Held Item entered geometry group/context");
    if (!coordinates(e))
        return false;
    e->state.thickness = .0625f;
    return emit_batch(e, batch);
}

bool entity_renderer_held_bow_draw(mc_client_draw_scope *scope, int32_t icon,
                                   mc_client_tessellator *nw,
                                   mc_client_render_engine_classes *graphics,
                                   mc_client_display *display, mc_held_bow_error *e) {
    if (e == NULL)
        return false;
    initialize(scope, icon, nw, graphics, display, e);
    /* Parent owns the real context tuple and its complete retained graph. */
    if (graphics == NULL || display == NULL)
        return failed(e, MC_HELD_BOW_ARGUMENT, "Bow native context tuple required");
    if (!coordinates(e))
        return false;
#define DISPLAY(pc, expression)                                                                    \
    do {                                                                                           \
        at(e, pc);                                                                                 \
        if (!work(e))                                                                              \
            return false;                                                                          \
        if (!(expression))                                                                         \
            return failed(e, MC_HELD_BOW_DISPLAY, "Bow Display child");                            \
    } while (0)
    DISPLAY(213, mc_client_display_source_rescale_normal(display, true, &e->display));
    e->state.rescale_enabled = true;
    DISPLAY(223, mc_client_display_source_translate(display, -0.0f, -.3f, 0.0f, &e->display));
    DISPLAY(236, mc_client_display_source_scale(display, 1.5, 1.5, 1.5, &e->display));
    DISPLAY(244, mc_client_display_source_rotate(display, 50.0f, 0.0f, 1.0f, 0.0f, &e->display));
    DISPLAY(252, mc_client_display_source_rotate(display, 335.0f, 0.0f, 0.0f, 1.0f, &e->display));
    DISPLAY(260, mc_client_display_source_translate(display, -.9375f, -.0625f, 0.0f, &e->display));
    e->state.thickness = .0625f;
    static const unsigned int draws[] = {335, 423, 554, 687, 824, 959};
    for (unsigned int batch = 0; batch < 6; ++batch)
        if (!emit_batch(e, batch) || !draw(e, draws[batch]))
            return false;
    /* Unreachable after any failed begin/normal/vertex/draw. No finally-style
     * state repair is supplied. Parent still owns RA's push0 and this pop967. */
    DISPLAY(964, mc_client_display_source_rescale_normal(display, false, &e->display));
    e->state.rescale_disabled = true;
    DISPLAY(967, mc_client_display_source_pop(display, &e->display));
    e->state.pop_returned = e->state.fragment_completed = true;
#undef DISPLAY
    return true;
}
