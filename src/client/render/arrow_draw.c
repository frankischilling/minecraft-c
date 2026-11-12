#include "arrow_draw_internal.h"
#include "entity/arrow/internal.h"
#include "living_draw/internal.h"

static const char arrow_descriptor[] = "(Lsl;DDDFF)V";
static const uint16_t arrow_units[] = {'/', 'i', 't', 'e', 'm', '/', 'a', 'r',
                                       'r', 'o', 'w', 's', '.', 'p', 'n', 'g'};
static const mc_player_string arrow_texture = {SIZE_MAX, sizeof arrow_units / sizeof *arrow_units,
                                               arrow_units};

static void arrow_at(ld_frame *f, int32_t pc) {
    ld_at(f, "mc", "a", arrow_descriptor, pc);
}

static bool arrow_rotate(ld_frame *f, int32_t pc, float angle, float x, float y, float z) {
    arrow_at(f, pc);
    return ld_display(f, mc_client_display_source_rotate(f->scope->model.display, angle, x, y, z,
                                                         &f->error->detail.display));
}

static bool arrow_normal(ld_frame *f, int32_t pc, float x, float y, float z) {
    arrow_at(f, pc);
    if (!ld_display(f, mc_client_display_source_normal(f->scope->model.display, x, y, z,
                                                       &f->error->detail.display)))
        return false;
    ++f->error->state.arrow.normals;
    return true;
}

static bool arrow_begin(ld_frame *f, mc_client_tessellator *nw, size_t quad, int32_t pc) {
    mc_client_arrow_draw_state *s = &f->error->state.arrow;
    arrow_at(f, pc);
    ++s->begin_attempts;
    /* The getstatic at88 can successfully return NULL. Its first source
     * dereference is244, after rescale, shake, transforms and current normal. */
    if (nw == NULL)
        return ld_null(f, "Arrow null captured NW begin receiver");
    if (!ld_graphics(f, mc_client_nw_begin_quads(nw, &f->error->detail.graphics)))
        return false;
    s->begin_returned[quad] = true;
    ++s->begins;
    return true;
}

static bool arrow_vertex(ld_frame *f, mc_client_tessellator *nw, int32_t pc, double x, double y,
                         double z, float u, float v) {
    mc_client_arrow_draw_state *s = &f->error->state.arrow;
    arrow_at(f, pc);
    if (!ld_fp(f) || !ld_work(f))
        return false;
    const double uu = at_widen(u), vv = at_widen(v);
    s->last_vertex_arguments[0] = x;
    s->last_vertex_arguments[1] = y;
    s->last_vertex_arguments[2] = z;
    s->last_vertex_arguments[3] = uu;
    s->last_vertex_arguments[4] = vv;
    s->last_vertex_pc = pc;
    s->last_vertex_returned = false;
    ++s->vertex_attempts;
    if (!ld_graphics(f, mc_client_nw_vertex_uv(nw, x, y, z, uu, vv, &f->error->detail.graphics)))
        return false;
    s->last_vertex_returned = true;
    ++s->vertices;
    return true;
}

static bool arrow_draw(ld_frame *f, mc_client_tessellator *nw, size_t quad, int32_t pc) {
    mc_client_arrow_draw_state *s = &f->error->state.arrow;
    mc_client_nw_fields fields;
    arrow_at(f, pc);
    if (!mc_client_nw_fields_get(nw, &fields))
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Arrow captured NW native storage");
    s->nw_words_identity[quad] = (uintptr_t)fields.g;
    s->nw_words_before_draw[quad] = fields.o;
    s->nw_vertices_before_draw[quad] = fields.h;
    s->nw_mode[quad] = fields.r;
    s->nw_writing_before_draw[quad] = fields.w;
    ++s->draw_attempts;
    const bool ok = mc_client_nw_draw(nw, f->scope->model.display, &f->error->detail.graphics);
    if (mc_client_nw_fields_get(nw, &fields))
        s->nw_writing_after_draw[quad] = fields.w;
    if (!ld_graphics(f, ok))
        return false;
    s->draw_returned[quad] = true;
    ++s->draws;
    return true;
}

static bool arrow_body(ld_frame *f, double x, double y, double z, float partial) {
    mc_client_living_draw_error *e = f->error;
    mc_client_arrow_draw_state *s = &e->state.arrow;
    arrow_at(f, 1);
    if (f->actor == NULL)
        return ld_null(f, "Arrow null previous-yaw receiver");
    if (!mc_entity_has_arrow(f->actor))
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Arrow actual canonical SL storage");
    s->guard_yaw = f->actor->base.aU;
    s->previous_yaw_read = true;
    arrow_at(f, 5);
    if (!ld_fp(f))
        return false;
    if (s->guard_yaw == 0.0f) {
        arrow_at(f, 10);
        s->guard_pitch = f->actor->base.aV;
        s->previous_pitch_read = true;
        arrow_at(f, 14);
        if (s->guard_pitch == 0.0f) {
            arrow_at(f, 18);
            s->early_returned = s->returned = e->state.returned = true;
            return true;
        }
    }
    arrow_at(f, 22);
    if (!ld_texture(f, &arrow_texture, 22))
        return false;
    s->texture_returned = true;
    arrow_at(f, 25);
    if (!ld_capture(f, mc_client_draw_capture_graphics(f->scope, &e->detail.capture)) ||
        !ld_display(f, mc_client_display_source_push(f->scope->model.display, &e->detail.display)))
        return false;
    s->matrix_pushed = true;
    arrow_at(f, 29);
    if (!ld_fp(f))
        return false;
    s->position[0] = at_narrow(x);
    s->position[1] = at_narrow(y);
    s->position[2] = at_narrow(z);
    arrow_at(f, 36);
    if (!ld_display(f, mc_client_display_source_translate(f->scope->model.display, s->position[0],
                                                          s->position[1], s->position[2],
                                                          &e->detail.display)))
        return false;
    s->position_translated = true;
    arrow_at(f, 40);
    s->yaw_reads[0] = f->actor->base.aU;
    arrow_at(f, 44);
    s->yaw_reads[1] = f->actor->base.yaw;
    arrow_at(f, 48);
    s->yaw_reads[2] = f->actor->base.aU;
    arrow_at(f, 51);
    if (!ld_fp(f))
        return false;
    s->yaw = at_fsub(
        at_fadd(s->yaw_reads[0], at_fmul(at_fsub(s->yaw_reads[1], s->yaw_reads[2]), partial)),
        90.0f);
    if (!arrow_rotate(f, 62, s->yaw, 0, 1, 0))
        return false;
    s->yaw_rotated = true;
    arrow_at(f, 66);
    s->pitch_reads[0] = f->actor->base.aV;
    arrow_at(f, 70);
    s->pitch_reads[1] = f->actor->base.aT;
    arrow_at(f, 74);
    s->pitch_reads[2] = f->actor->base.aV;
    arrow_at(f, 77);
    if (!ld_fp(f))
        return false;
    s->pitch =
        at_fadd(s->pitch_reads[0], at_fmul(at_fsub(s->pitch_reads[1], s->pitch_reads[2]), partial));
    if (!arrow_rotate(f, 85, s->pitch, 0, 0, 1))
        return false;
    s->pitch_rotated = true;
    arrow_at(f, 88);
    mc_client_tessellator *nw = NULL;
    if (!ld_graphics(f, mc_client_nw_active_use(f->scope->model.graphics, e->site, &nw,
                                                &e->detail.graphics)))
        return false;
    s->nw_identity = (uintptr_t)nw;
    s->nw_captured = true;
    arrow_at(f, 113);
    if (!ld_fp(f))
        return false;
    s->shaft_uv[0] = 0.0f;
    s->shaft_uv[1] = 0.5f;
    s->shaft_uv[2] = at_fdiv(0.0f, 32.0f);
    s->shaft_uv[3] = at_fdiv(5.0f, 32.0f);
    s->end_uv[0] = 0.0f;
    s->end_uv[1] = 0.15625f;
    s->end_uv[2] = at_fdiv(5.0f, 32.0f);
    s->end_uv[3] = at_fdiv(10.0f, 32.0f);
    s->scale = 0.05625f; /* Source ldc raw binary32 0x3d666666. */
    arrow_at(f, 169);
    if (!ld_display(f, mc_client_display_source_rescale_normal(f->scope->model.display, true,
                                                               &e->detail.display)))
        return false;
    s->rescale_enabled = true;
    arrow_at(f, 173);
    s->shake = f->actor->arrow->fields.b;
    s->shake_read = true;
    arrow_at(f, 176);
    if (!ld_fp(f))
        return false;
    s->shake_remaining = at_fsub((float)s->shake, partial);
    if (s->shake_remaining > 0.0f) {
        arrow_at(f, 193);
        s->shake_argument = at_fmul(s->shake_remaining, 3.0f);
        arrow_at(f, 194);
        if (!ld_float_math(f, "a", s->shake_argument, &s->shake_sine))
            return false;
        s->shake_math_returned = true;
        arrow_at(f, 200);
        if (!ld_fp(f))
            return false;
        s->shake_rotation = at_fmul(at_fneg(s->shake_sine), s->shake_remaining);
        if (!arrow_rotate(f, 208, s->shake_rotation, 0, 0, 1))
            return false;
        s->shake_rotated = true;
    }
    if (!arrow_rotate(f, 216, 45, 1, 0, 0))
        return false;
    s->fixed_rotated = true;
    arrow_at(f, 225);
    if (!ld_display(f, mc_client_display_source_scale(f->scope->model.display, at_widen(s->scale),
                                                      at_widen(s->scale), at_widen(s->scale),
                                                      &e->detail.display)))
        return false;
    s->scaled = true;
    arrow_at(f, 232);
    if (!ld_display(f, mc_client_display_source_translate(f->scope->model.display, -4, 0, 0,
                                                          &e->detail.display)))
        return false;
    s->tail_translated = true;
    if (!arrow_normal(f, 239, s->scale, 0, 0) || !arrow_begin(f, nw, 0, 244) ||
        !arrow_vertex(f, nw, 264, -7, -2, -2, s->end_uv[0], s->end_uv[2]) ||
        !arrow_vertex(f, nw, 284, -7, -2, 2, s->end_uv[1], s->end_uv[2]) ||
        !arrow_vertex(f, nw, 304, -7, 2, 2, s->end_uv[1], s->end_uv[3]) ||
        !arrow_vertex(f, nw, 324, -7, 2, -2, s->end_uv[0], s->end_uv[3]) ||
        !arrow_draw(f, nw, 0, 329))
        return false;
    if (!arrow_normal(f, 337, at_fneg(s->scale), 0, 0) || !arrow_begin(f, nw, 1, 342) ||
        !arrow_vertex(f, nw, 362, -7, 2, -2, s->end_uv[0], s->end_uv[2]) ||
        !arrow_vertex(f, nw, 382, -7, 2, 2, s->end_uv[1], s->end_uv[2]) ||
        !arrow_vertex(f, nw, 402, -7, -2, 2, s->end_uv[1], s->end_uv[3]) ||
        !arrow_vertex(f, nw, 422, -7, -2, -2, s->end_uv[0], s->end_uv[3]) ||
        !arrow_draw(f, nw, 1, 427))
        return false;
    for (int32_t i = 0;; ++i) {
        ++s->loop_tests;
        if (i >= 4)
            break;
        if (!arrow_rotate(f, 444, 90, 1, 0, 0))
            return false;
        ++s->shaft_rotations;
        if (!arrow_normal(f, 451, 0, 0, s->scale) || !arrow_begin(f, nw, (size_t)i + 2, 456) ||
            !arrow_vertex(f, nw, 474, -8, -2, 0, s->shaft_uv[0], s->shaft_uv[2]) ||
            !arrow_vertex(f, nw, 492, 8, -2, 0, s->shaft_uv[1], s->shaft_uv[2]) ||
            !arrow_vertex(f, nw, 510, 8, 2, 0, s->shaft_uv[1], s->shaft_uv[3]) ||
            !arrow_vertex(f, nw, 528, -8, 2, 0, s->shaft_uv[0], s->shaft_uv[3]) ||
            !arrow_draw(f, nw, (size_t)i + 2, 533))
            return false;
    }
    arrow_at(f, 544);
    if (!ld_display(f, mc_client_display_source_rescale_normal(f->scope->model.display, false,
                                                               &e->detail.display)))
        return false;
    s->rescale_disabled = e->state.rescale_disabled = true;
    arrow_at(f, 547);
    if (!ld_display(f, mc_client_display_source_pop(f->scope->model.display, &e->detail.display)))
        return false;
    s->matrix_popped = e->state.matrix_popped = true;
    arrow_at(f, 550);
    s->returned = e->state.returned = true;
    return true;
}

static bool arrow_entered(mc_client_draw_scope *scope, mc_client_managed_renderer *renderer,
                          mc_entity *actor, double x, double y, double z, float yaw, float partial,
                          mc_client_graphics_site caller, mc_client_living_draw_error *e,
                          bool bridge) {
    (void)yaw; /* MC never reads argument8. */
    if (e == NULL)
        return false;
    mc_client_living_draw_error_init(e); /* Also clears a reused child's Throwable/detail. */
    e->caller = caller;
    ld_frame f = {scope, renderer, actor, e};
    if (scope == NULL || renderer == NULL || renderer->source_class == NULL ||
        strcmp(renderer->source_class, "mc") != 0)
        return ld_fail(&f, MC_CLIENT_LIVING_DRAW_ARGUMENT, "Arrow draw native scope/MC renderer");
    e->state.arrow.entered = true;
    e->state.arrow.receiver_identity = (uintptr_t)renderer;
    e->state.arrow.actor_identity = (uintptr_t)actor;
    if (bridge) {
        ld_at(&f, "mc", "a", "(Lsn;DDDFF)V", 2);
        if (!ld_cast_actor(&f, MC_ENTITY_CLASS_KIND_ARROW, "Arrow bridge actual SL checkcast"))
            return false;
        e->state.arrow.bridge_cast_returned = true;
        e->parent = ld_site("mc", "a", "(Lsn;DDDFF)V", 14);
    }
    /* No catch/finally in MC. Failure keeps every reached texture, selected
     * matrix, normal, rescale and NW effect; native retirement never undoes it. */
    return arrow_body(&f, x, y, z, partial);
}

bool mc_client_arrow_draw_entered(mc_client_draw_scope *scope, mc_client_managed_renderer *renderer,
                                  mc_entity *actor, double x, double y, double z, float yaw,
                                  float partial, mc_client_graphics_site caller,
                                  mc_client_living_draw_error *e) {
    return arrow_entered(scope, renderer, actor, x, y, z, yaw, partial, caller, e, true);
}

bool mc_client_arrow_draw_typed_entered(mc_client_draw_scope *scope,
                                        mc_client_managed_renderer *renderer, mc_entity *actor,
                                        double x, double y, double z, float yaw, float partial,
                                        mc_client_graphics_site caller,
                                        mc_client_living_draw_error *e) {
    return arrow_entered(scope, renderer, actor, x, y, z, yaw, partial, caller, e, false);
}
