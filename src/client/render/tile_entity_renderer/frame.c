#include "internal.h"

#include "../../../entity/player/tick_math_internal.h"
#include "../../gui/bitmap_font.h"

#include <stdlib.h>
#include <string.h>

static const char frame_descriptor[] = "(Lfd;Lji;Lsj;Lls;F)V";

static bool frame_fail(mc_client_tile_renderer_error *e, mc_client_tile_renderer_status status,
                       const char *operation, int32_t pc, bool native) {
    tile_renderer_fail(e, status, operation, "ll", "a", frame_descriptor, pc, native);
    e->frame_pc = pc;
    return false;
}

static bool frame_fp(mc_client_tile_renderer_error *e, int32_t pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    return frame_fail(e, MC_CLIENT_TILE_RENDERER_UNSUPPORTED_FP,
                      "LL frame strict floating environment", pc, true);
}

/* Measured LL cases13/14 preserve the current operand's payload through SUB,
 * MUL and ADD; cases17..19 preserve/quiet the partial payload and widen it.
 * OpenJDK17u tag jdk-17.0.17+10, templateTable_x86.cpp fop2/dop2, keeps the
 * right source operand as SSE destination for ADD/MUL, left for SUB. Explicit
 * selection prevents a compiler commuting the intrinsic's NaN operands.
 * https://github.com/openjdk/jdk17u/blob/jdk-17.0.17%2B10/src/hotspot/cpu/x86/templateTable_x86.cpp
 * No Java/JDK body is copied here. Floating exception flags are not a value
 * parity claim; controls are checked and no MXCSR bit is repaired or reset. */
static bool frame_float_nan_admit(float left, float right, int32_t pc,
                                  mc_client_tile_renderer_error *e) {
    uint32_t a, b;
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    if ((a & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000) &&
        (b & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000) && ((a & b) & UINT32_C(0x00400000)) == 0)
        return frame_fail(e, MC_CLIENT_TILE_RENDERER_UNSUPPORTED_NUMERIC,
                          "LL frame unmeasured two-NaN signaling operand", pc, true);
    return true;
}

static bool frame_double_nan_admit(double left, double right, int32_t pc,
                                   mc_client_tile_renderer_error *e) {
    uint64_t a, b;
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    if ((a & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000) &&
        (b & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000) &&
        ((a & b) & UINT64_C(0x0008000000000000)) == 0)
        return frame_fail(e, MC_CLIENT_TILE_RENDERER_UNSUPPORTED_NUMERIC,
                          "LL frame unmeasured two-NaN signaling operand", pc, true);
    return true;
}

static bool frame_nan_float(float left, float right, bool prefer_right, float *out) {
    uint32_t a, b;
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    const bool an = (a & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000);
    const bool bn = (b & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000);
    if (!an && !bn)
        return false;
    uint32_t result = (prefer_right ? (bn ? b : a) : (an ? a : b)) | UINT32_C(0x00400000);
    memcpy(out, &result, sizeof(result));
    return true;
}

static bool frame_nan_double(double left, double right, bool prefer_right, double *out) {
    uint64_t a, b;
    memcpy(&a, &left, sizeof(a));
    memcpy(&b, &right, sizeof(b));
    const bool an = (a & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000);
    const bool bn = (b & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000);
    if (!an && !bn)
        return false;
    uint64_t result = (prefer_right ? (bn ? b : a) : (an ? a : b)) | UINT64_C(0x0008000000000000);
    memcpy(out, &result, sizeof(result));
    return true;
}

static float frame_fsub(float a, float b) {
    float result;
    return frame_nan_float(a, b, false, &result) ? result : at_fsub(a, b);
}

static float frame_fmul(float a, float b) {
    float result;
    return frame_nan_float(a, b, true, &result) ? result : at_fmul(a, b);
}

static float frame_fadd(float a, float b) {
    float result;
    return frame_nan_float(a, b, true, &result) ? result : at_fadd(a, b);
}

static double frame_dsub(double a, double b) {
    double result;
    return frame_nan_double(a, b, false, &result) ? result : at_sub(a, b);
}

static double frame_dmul(double a, double b) {
    double result;
    return frame_nan_double(a, b, true, &result) ? result : at_mul(a, b);
}

static double frame_dadd(double a, double b) {
    double result;
    return frame_nan_double(a, b, true, &result) ? result : at_add(a, b);
}

static double frame_widen(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    if ((bits & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000)) {
        const uint64_t widened = ((uint64_t)(bits & UINT32_C(0x80000000)) << 32) |
                                 UINT64_C(0x7ff8000000000000) |
                                 ((uint64_t)(bits & UINT32_C(0x007fffff)) << 29);
        double result;
        memcpy(&result, &widened, sizeof(result));
        return result;
    }
    return at_widen(value);
}

/* Each operation rounds at the actual JVM width, including endpoint partials,
 * subnormal values and intermediate overflow. No algebraic shortcut or FMA. */
static bool interpolate_float(float previous, float current, float partial, int32_t sub_pc,
                              int32_t mul_pc, int32_t add_pc, float *out,
                              mc_client_tile_renderer_error *e) {
    if (!frame_fp(e, sub_pc) || !frame_float_nan_admit(current, previous, sub_pc, e))
        return false;
    const float difference = frame_fsub(current, previous);
    if (!frame_fp(e, mul_pc) || !frame_float_nan_admit(difference, partial, mul_pc, e))
        return false;
    const float scaled = frame_fmul(difference, partial);
    if (!frame_fp(e, add_pc) || !frame_float_nan_admit(previous, scaled, add_pc, e))
        return false;
    const float value = frame_fadd(previous, scaled);
    *out = value;
    return true;
}

static bool interpolate_double(double previous, double current, float partial, int32_t sub_pc,
                               int32_t widen_pc, int32_t mul_pc, int32_t add_pc, double *out,
                               mc_client_tile_renderer_error *e) {
    if (!frame_fp(e, sub_pc) || !frame_double_nan_admit(current, previous, sub_pc, e))
        return false;
    const double difference = frame_dsub(current, previous);
    if (!frame_fp(e, widen_pc))
        return false;
    const double widened = frame_widen(partial);
    if (!frame_fp(e, mul_pc) || !frame_double_nan_admit(difference, widened, mul_pc, e))
        return false;
    const double scaled = frame_dmul(difference, widened);
    if (!frame_fp(e, add_pc) || !frame_double_nan_admit(previous, scaled, add_pc, e))
        return false;
    const double value = frame_dadd(previous, scaled);
    *out = value;
    return true;
}

static bool ledger_failure(mc_client_tile_renderer_error *e) {
    tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_BUSY, "LL frame occurrence invariant", NULL, NULL,
                       NULL, -1, true);
    return false;
}

static bool frame_ledger_admit(const mc_client_tile_renderer_dispatcher *m,
                               mc_client_tile_renderer_error *e) {
    size_t engines = 0, fonts = 0;
    bool engine_found = m->fields.e == NULL, font_found = m->fields.n == NULL;
    for (const tile_renderer_engine_occurrence *p = m->engines; p != NULL; p = p->next) {
        if (engines == m->retained_engines || p->value == NULL)
            return ledger_failure(e);
        ++engines;
        if (p->value == m->fields.e)
            engine_found = true;
    }
    for (const tile_renderer_font_occurrence *p = m->fonts; p != NULL; p = p->next) {
        if (fonts == m->retained_fonts || p->value == NULL)
            return ledger_failure(e);
        ++fonts;
        if (p->value == m->fields.n)
            font_found = true;
    }
    if (engines != m->retained_engines || fonts != m->retained_fonts || !engine_found ||
        !font_found || m->fields.g != m->living_occurrence)
        return ledger_failure(e);
    return true;
}

static bool capture_engine(mc_client_tile_renderer_dispatcher *m, mc_client_render_engine *value,
                           mc_client_tile_renderer_error *e) {
    if (value == NULL)
        return true;
    for (tile_renderer_engine_occurrence *p = m->engines; p != NULL; p = p->next)
        if (p->value == value)
            return true;
    if (m->retained_engines == SIZE_MAX)
        return frame_fail(e, MC_CLIENT_TILE_RENDERER_LIMIT, "LL native Engine occurrence bound", 15,
                          true);
    tile_renderer_engine_occurrence *p =
        tile_renderer_allocate(m->classes, sizeof(*p), "ll", "a", frame_descriptor, 15, e);
    if (p == NULL) {
        e->frame_pc = 15;
        return false;
    }
    e->engine_status = mc_client_render_engine_retain(value);
    if (e->engine_status != MC_CLIENT_RENDER_ENGINE_OK) {
        m->classes->bytes -= sizeof(*p);
        free(p); /* Unpublished native capture only; no source field was stored. */
        return frame_fail(e, MC_CLIENT_TILE_RENDERER_ENGINE_FAILED,
                          "LL actual Engine native occurrence", 15, true);
    }
    p->value = value;
    p->next = m->engines;
    m->engines = p;
    ++m->retained_engines;
    return true;
}

static bool bind_living(mc_client_tile_renderer_dispatcher *m, mc_entity *value,
                        mc_client_tile_renderer_error *e) {
    if (value != NULL) {
        mc_entity_snapshot state;
        e->entity_status = mc_entity_get_snapshot(value, &state);
        const mc_entity_class_literal *living = NULL;
        bool matches = false;
        if (e->entity_status == MC_ENTITY_OK)
            e->entity_status = mc_entity_class_literal_get(
                m->classes->models->entities, MC_ENTITY_CLASS_KIND_LIVING_ENTITY, &living);
        if (e->entity_status == MC_ENTITY_OK)
            e->entity_status =
                mc_entity_class_literal_is_assignable_from(living, state.class_literal, &matches);
        if (e->entity_status != MC_ENTITY_OK)
            return frame_fail(e, MC_CLIENT_TILE_RENDERER_ENTITY_FAILED,
                              "LL canonical Living Class admission", 21, true);
        if (!matches || state.source != MC_ENTITY_CLIENT) {
            e->entity_status = MC_ENTITY_INVALID_ARGUMENT;
            return frame_fail(e, MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT,
                              "LL Living from the same actual Entity namespace", 21, true);
        }
        e->entity_status = mc_entity_retain(value);
        if (e->entity_status != MC_ENTITY_OK)
            return frame_fail(e, MC_CLIENT_TILE_RENDERER_ENTITY_FAILED,
                              "LL actual Living native occurrence", 21, true);
    }
    mc_entity *previous = m->living_occurrence;
    m->fields.g = value; /* Actual g21 after native retain, before old occurrence ends. */
    m->living_occurrence = value;
    m->last_frame_pc = 21;
    mc_entity_release(previous);
    return true;
}

static bool capture_font(mc_client_tile_renderer_dispatcher *m, mc_client_bitmap_font *value,
                         mc_client_tile_renderer_error *e) {
    if (value == NULL)
        return true;
    for (tile_renderer_font_occurrence *p = m->fonts; p != NULL; p = p->next)
        if (p->value == value)
            return true;
    if (m->retained_fonts == SIZE_MAX)
        return frame_fail(e, MC_CLIENT_TILE_RENDERER_LIMIT, "LL native Font occurrence bound", 26,
                          true);
    tile_renderer_font_occurrence *p =
        tile_renderer_allocate(m->classes, sizeof(*p), "ll", "a", frame_descriptor, 26, e);
    if (p == NULL) {
        e->frame_pc = 26;
        return false;
    }
    const mc_client_bitmap_font_status status = mc_client_bitmap_font_retain(value);
    e->font_status = (int)status;
    if (status != MC_CLIENT_BITMAP_FONT_OK) {
        m->classes->bytes -= sizeof(*p);
        free(p);
        return frame_fail(e, MC_CLIENT_TILE_RENDERER_FONT_FAILED,
                          "LL actual BitmapFont native occurrence", 26, true);
    }
    p->value = value;
    p->next = m->fonts;
    m->fonts = p;
    ++m->retained_fonts;
    return true;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_bind_frame(mc_client_tile_renderer_dispatcher *m, mc_world_runtime *world,
                                   mc_client_render_engine *engine, mc_client_bitmap_font *font,
                                   mc_entity *living, float partial,
                                   mc_client_tile_renderer_error *error) {
    mc_client_tile_renderer_error e;
    tile_renderer_error_init(&e);
    if (m == NULL)
        (void)frame_fail(&e, MC_CLIENT_TILE_RENDERER_NULL_REFERENCE, "actual LL frame receiver", 1,
                         false);
    else if (tile_renderer_passive_admit(m, &e)) {
        if (m->classes->phase != MC_CLIENT_TILE_RENDERER_READY || m->classes->a != m ||
            m->map == NULL)
            tile_renderer_fail(&e, MC_CLIENT_TILE_RENDERER_INVALID_PHASE,
                               "LL frame requires completed class initialization", NULL, NULL, NULL,
                               -1, true);
        else if (frame_ledger_admit(m, &e)) {
            m->active = m->classes->active = true;
            m->last_frame_pc = -1;
            if (m->fields.f != world) { /* Actual reference comparison5. */
                e.frame_pc = 10;
                if (!tile_renderer_world_change_admit(m, world, &e) ||
                    !tile_renderer_world_change_entered(m, world, &e))
                    goto leave;
                m->last_frame_pc = 10;
            }
            if (!capture_engine(m, engine, &e))
                goto leave;
            m->fields.e = engine;
            m->last_frame_pc = 15;
            if (!bind_living(m, living, &e) || !capture_font(m, font, &e))
                goto leave;
            m->fields.n = font;
            m->last_frame_pc = 26;
            if (living == NULL) {
                (void)frame_fail(&e, MC_CLIENT_TILE_RENDERER_NULL_REFERENCE,
                                 "actual null Living field receiver", 32, false);
                goto leave;
            }
            mc_entity_snapshot state;
            e.entity_status = mc_entity_get_snapshot(living, &state);
            if (e.entity_status != MC_ENTITY_OK) {
                (void)frame_fail(&e, MC_CLIENT_TILE_RENDERER_ENTITY_FAILED,
                                 "LL actual Living base fields", 32, true);
                goto leave;
            }
            const mc_entity_base_fields *f = &state.base;
            float angle;
            double position;
            if (!interpolate_float(f->aU, f->yaw, partial, 45, 48, 49, &angle, &e))
                goto leave;
            m->fields.h = angle;
            m->last_frame_pc = 50;
            if (!interpolate_float(f->aV, f->aT, partial, 69, 72, 73, &angle, &e))
                goto leave;
            m->fields.i = angle;
            m->last_frame_pc = 74;
            if (!interpolate_double(f->bl, f->x, partial, 93, 96, 97, 98, &position, &e))
                goto leave;
            m->fields.j = position;
            m->last_frame_pc = 99;
            if (!interpolate_double(f->bm, f->y, partial, 118, 121, 122, 123, &position, &e))
                goto leave;
            m->fields.k = position;
            m->last_frame_pc = 124;
            if (!interpolate_double(f->bn, f->z, partial, 143, 146, 147, 148, &position, &e))
                goto leave;
            m->fields.l = position;
            m->last_frame_pc = e.frame_pc = 152;
        leave:
            m->active = m->classes->active = false;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool tile_renderer_frame_terminal_admit(mc_client_tile_renderer_dispatcher *m,
                                        mc_client_tile_renderer_error *e) {
    if (!frame_ledger_admit(m, e))
        return false;
    for (tile_renderer_font_occurrence *p = m->fonts; p != NULL; p = p->next) {
        mc_client_bitmap_font_state state;
        if (!mc_client_bitmap_font_inspect(p->value, &state) || state.holds == 0)
            return ledger_failure(e);
        if (state.active || state.phase == MC_CLIENT_BITMAP_FONT_CONSTRUCTING) {
            e->font_status = (int)MC_CLIENT_BITMAP_FONT_BUSY;
            tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_BUSY,
                               "LL retained Font operation blocks retirement", NULL, NULL, NULL, -1,
                               true);
            return false;
        }
    }
    for (tile_renderer_engine_occurrence *p = m->engines; p != NULL; p = p->next) {
        mc_client_render_engine_state state;
        if (!mc_client_render_engine_inspect(p->value, &state) || state.native_holds == 0)
            return ledger_failure(e);
        if (state.phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING) {
            e->engine_status = MC_CLIENT_RENDER_ENGINE_BUSY;
            tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_BUSY,
                               "LL retained Engine constructor blocks retirement", NULL, NULL, NULL,
                               -1, true);
            return false;
        }
    }
    return true;
}

/* Called with LL RETIRING and both active guards held, including every real
 * Font stream closer. Successful slot consumption is never repeated on retry. */
bool tile_renderer_frame_retire(mc_client_tile_renderer_dispatcher *m,
                                mc_client_tile_renderer_error *e) {
    while (m->fonts != NULL) {
        tile_renderer_font_occurrence *p = m->fonts;
        const bool current = m->fields.n == p->value;
        mc_client_bitmap_font_error child = {0};
        const mc_client_bitmap_font_status status =
            mc_client_bitmap_font_release(&p->value, &child);
        e->font_status = (int)status;
        if (status != MC_CLIENT_BITMAP_FONT_OK) {
            tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_FONT_FAILED,
                               "LL retained BitmapFont occurrence retirement", NULL, NULL, NULL, -1,
                               true);
            return false;
        }
        if (current)
            m->fields.n = NULL; /* Native source views have already ended. */
        m->fonts = p->next;
        --m->retained_fonts;
        m->classes->bytes -= sizeof(*p);
        free(p);
    }
    while (m->engines != NULL) {
        tile_renderer_engine_occurrence *p = m->engines;
        const bool current = m->fields.e == p->value;
        e->engine_status = mc_client_render_engine_release(p->value);
        if (e->engine_status != MC_CLIENT_RENDER_ENGINE_OK) {
            tile_renderer_fail(e, MC_CLIENT_TILE_RENDERER_ENGINE_FAILED,
                               "LL retained Engine occurrence retirement", NULL, NULL, NULL, -1,
                               true);
            return false;
        }
        if (current)
            m->fields.e = NULL;
        p->value = NULL;
        m->engines = p->next;
        --m->retained_engines;
        m->classes->bytes -= sizeof(*p);
        free(p);
    }
    mc_entity *living = m->living_occurrence;
    m->living_occurrence = NULL;
    m->fields.g = NULL;
    mc_entity_release(living);
    return true;
}
