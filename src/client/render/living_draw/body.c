#include "internal.h"

static void gv(ld_frame *f, int32_t pc) {
    ld_at(f, "gv", "a", "(Lls;DDDFF)V", pc);
}

static bool model_cell(ld_frame *f, manager_model *model, int32_t pc) {
    gv(f, pc);
    if (model == NULL)
        return ld_null(f, "Living draw null model field receiver");
    for (manager_model *p = f->renderer->models; p != NULL; p = p->next)
        if (p == model)
            return true;
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Living draw model allocation provenance");
}

static bool rotate_actor(ld_frame *f, float age, float body, float partial) {
    (void)age;
    mc_client_living_draw_error *e = f->error;
    ld_at(f, "gv", "a", "(Lls;FFF)V", 7);
    if (!ld_fp(f) || !ld_display(f, mc_client_display_source_rotate(
                                        f->scope->model.display, at_fsub(180.0f, body), 0.0f, 1.0f,
                                        0.0f, &e->detail.display)))
        return false;
    mc_living_entity_fields *ls = mc_entity_living_fields(f->actor);
    ld_at(f, "gv", "a", "(Lls;FFF)V", 14);
    if (ls == NULL)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Living draw actual LS rotation storage");
    if (ls->ad > 0) {
        if (!ld_fp(f))
            return false;
        float value = at_fmul(at_fdiv(at_fsub(at_fadd((float)ls->ad, partial), 1.0f), 20.0f), 1.6f);
        ld_at(f, "gv", "a", "(Lls;FFF)V", 37);
        if (f->scope->model.math == NULL)
            return ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                           "Living death rotation actual Math namespace");
        e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_MATH;
        if (mc_math_classes_ensure(f->scope->model.math, &e->detail.math) != MC_MATH_CLASS_OK)
            return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                           "Living death rotation Math active use");
        value = at_root(at_widen(value));
        if (value > 1.0f)
            value = 1.0f;
        ld_at(f, "gv", "a", "(Lls;FFF)V", 63);
        if (!ld_display(f, mc_client_display_source_rotate(f->scope->model.display,
                                                           at_fmul(value, 90.0f), 0.0f, 0.0f, 1.0f,
                                                           &e->detail.display)))
            return false;
    }
    return true;
}

static bool body_try(ld_frame *f, double x, double y, double z, float partial) {
    mc_client_living_draw_error *e = f->error;
    mc_client_living_draw_state *s = &e->state;
    mc_living_entity_fields *ls = mc_entity_living_fields(f->actor);
    gv(f, 77);
    if (ls == NULL)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Living draw represented LS fields");
    if (!ld_fp(f))
        return false;
    s->body_yaw = at_fadd(ls->I, at_fmul(at_fsub(ls->H, ls->I), partial));
    s->head_yaw = at_fadd(f->actor->base.aU,
                          at_fmul(at_fsub(f->actor->base.yaw, f->actor->base.aU), partial));
    s->pitch =
        at_fadd(f->actor->base.aV, at_fmul(at_fsub(f->actor->base.aT, f->actor->base.aV), partial));
    ld_at(f, "gv", "b", "(Lls;DDD)V", 8);
    if (!ld_display(f, mc_client_display_source_translate(f->scope->model.display, at_narrow(x),
                                                          at_narrow(y), at_narrow(z),
                                                          &e->detail.display)))
        return false;
    gv(f, 147);
    if (!ld_age(f, partial, &s->age))
        return false;
    if (!rotate_actor(f, s->age, s->body_yaw, partial))
        return false;
    s->scale = .0625f;
    gv(f, 169);
    if (!ld_display(f, mc_client_display_source_rescale_normal(f->scope->model.display, true,
                                                               &e->detail.display)))
        return false;
    gv(f, 177);
    if (!ld_display(f, mc_client_display_source_scale(f->scope->model.display, -1.0, -1.0, 1.0,
                                                      &e->detail.display)))
        return false;
    /* Actual base GV.a(LS,F) invoked184 returns0, reading no actor or service. */
    gv(f, 197);
    if (!ld_display(
            f, mc_client_display_source_translate(f->scope->model.display, 0.0f,
                                                  at_fsub(at_fmul(-24.0f, s->scale), .0078125f),
                                                  0.0f, &e->detail.display)))
        return false;
    ls = mc_entity_living_fields(f->actor);
    gv(f, 201);
    s->limb_amount = at_fadd(ls->ak, at_fmul(at_fsub(ls->al, ls->ak), partial));
    s->limb_swing = at_fsub(ls->am, at_fmul(ls->al, at_fsub(1.0f, partial)));
    if (s->limb_amount > 1.0f)
        s->limb_amount = 1.0f;
    gv(f, 247);
    if (!ld_skin(f))
        return false;
    gv(f, 261);
    if (!ld_display(f, mc_client_display_source_alpha_test(f->scope->model.display, true,
                                                           &e->detail.display)))
        return false;
    gv(f, 275);
    if (!ld_model(f, mc_client_model_event_entered(f->renderer, f->renderer->primary, f->actor,
                                                   s->limb_swing, s->limb_amount, partial,
                                                   &f->scope->model, e->site, &e->detail.model)))
        return false;
    s->primary_event = e->detail.model.state;
    s->primary_event_returned = true;
    gv(f, 297);
    if (!ld_model(f, mc_client_model_render_entered(
                         f->renderer, f->renderer->primary, s->limb_swing, s->limb_amount, s->age,
                         at_fsub(s->head_yaw, s->body_yaw), s->pitch, s->scale, &f->scope->model,
                         e->site, &e->detail.model)))
        return false;
    s->primary_render = e->detail.model.state;
    s->primary_render_returned = true;
    for (int32_t pass = 0; pass < 4; ++pass) {
        gv(f, 315);
        s->pass = pass;
        if (!ld_work(f))
            return false;
        s->pass_attempted[(size_t)pass] = true;
        bool selected;
        if (!ld_pass(f, pass, partial, &selected))
            return false;
        s->pass_result[(size_t)pass] = selected;
        if (!selected)
            continue;
        gv(f, 340);
        if (!ld_model(f, mc_client_model_render_entered(
                             f->renderer, f->renderer->secondary, s->limb_swing, s->limb_amount,
                             s->age, at_fsub(s->head_yaw, s->body_yaw), s->pitch, s->scale,
                             &f->scope->model, e->site, &e->detail.model)))
            return false;
        s->pass_render[(size_t)pass] = e->detail.model.state;
        s->pass_render_returned[(size_t)pass] = true;
        gv(f, 346);
        if (!ld_display(f, mc_client_display_source_blend_enable(f->scope->model.display, false,
                                                                 &e->detail.display)))
            return false;
        gv(f, 352);
        if (!ld_display(f, mc_client_display_source_alpha_test(f->scope->model.display, true,
                                                               &e->detail.display)))
            return false;
    }
    /* Actual base held helper365 is return0; it does not query a held Stack. */
    gv(f, 371);
    float brightness;
    if (!ld_brightness(f, partial, &brightness))
        return false;
    (void)brightness; /* Actual base color-multiplier382 returns0, ignoring its operands. */
    ls = mc_entity_living_fields(f->actor);
    gv(f, 400);
    if (ls->aa > 0 || ls->ad > 0) {
        gv(f, 416);
        if (!ld_display(f, mc_client_display_source_texture_enable(f->scope->model.display, false,
                                                                   &e->detail.display)))
            return false;
        gv(f, 422);
        if (!ld_display(f, mc_client_display_source_alpha_test(f->scope->model.display, false,
                                                               &e->detail.display)))
            return false;
        gv(f, 428);
        if (!ld_display(f, mc_client_display_source_blend_enable(f->scope->model.display, true,
                                                                 &e->detail.display)))
            return false;
        gv(f, 437);
        if (!ld_display(f, mc_client_display_source_blend_function(f->scope->model.display, 770,
                                                                   771, &e->detail.display)))
            return false;
        gv(f, 443);
        if (!ld_display(f, mc_client_display_source_depth_function(f->scope->model.display, 514,
                                                                   &e->detail.display)))
            return false;
        /* A newly supported GL_EQUAL backend still needs its separate overlay
         * cohort; never silently skip those source draws. */
        gv(f, 447);
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                       "Living actual hurt/death overlay child");
    }
    gv(f, 737);
    if (!ld_display(f, mc_client_display_source_rescale_normal(f->scope->model.display, false,
                                                               &e->detail.display)))
        return false;
    s->rescale_disabled = true;
    return true;
}

static bool tail(ld_frame *f, double x, double y, double z) {
    mc_client_living_draw_error *e = f->error;
    gv(f, 753);
    if (!ld_display(f, mc_client_display_source_cull_test(f->scope->model.display, true,
                                                          &e->detail.display)))
        return false;
    e->state.cull_enabled = true;
    gv(f, 756);
    if (!ld_display(f, mc_client_display_source_pop(f->scope->model.display, &e->detail.display)))
        return false;
    e->state.matrix_popped = true;
    ld_at(f, "gv", "a", "(Lls;DDD)V", 0);
    bool debug;
    if (!mc_client_draw_debug_flag(f->scope, &debug, &e->detail.capture)) {
        e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_CAPTURE;
        e->child = ld_site(e->detail.capture.source_class, e->detail.capture.source_method,
                           e->detail.capture.source_descriptor, e->detail.capture.pc);
        if (e->detail.capture.status == MC_CLIENT_MANAGER_NULL_REFERENCE &&
            !e->detail.capture.native_policy)
            return ld_null(f, "Living debug actual Settings receiver");
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_CHILD_FAILED, e->detail.capture.operation);
    }
    e->state.debug_checked = true;
    e->state.debug = debug;
    if (debug) {
        (void)x;
        (void)y;
        (void)z;
        ld_at(f, "gv", "a", "(Lls;DDD)V", 12);
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                       "Living debug Integer.toString label child");
    }
    gv(f, 769);
    e->state.returned = true;
    return true;
}

bool mc_client_living_draw_entered(mc_client_draw_scope *scope, mc_client_managed_renderer *r,
                                   mc_entity *actor, double x, double y, double z, float yaw,
                                   float partial, mc_client_graphics_site caller,
                                   mc_client_living_draw_error *e) {
    (void)yaw;
    if (e == NULL)
        return false;
    mc_client_living_draw_error_init(e);
    e->caller = caller;
    ld_frame f = {scope, r, actor, e};
    if (scope == NULL || r == NULL)
        return ld_fail(&f, MC_CLIENT_LIVING_DRAW_ARGUMENT, "Living draw native scope/renderer");
    const bool cow = strcmp(r->source_class, "va") == 0;
    const bool chicken = strcmp(r->source_class, "ip") == 0;
    if (strcmp(r->source_class, "xy") != 0 && strcmp(r->source_class, "me") != 0 && !cow &&
        !chicken) {
        ld_at(&f, r->source_class, "a", "(Lsn;DDDFF)V", 0);
        return ld_fail(&f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                       "Living actual concrete renderer body");
    }
    if (cow || chicken) {
        ld_at(&f, r->source_class, "a", "(Lsn;DDDFF)V", 2);
        if (actor != NULL && !mc_entity_is_kind(actor, cow ? MC_ENTITY_CLASS_KIND_COW
                                                           : MC_ENTITY_CLASS_KIND_CHICKEN)) {
            e->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
            e->source_exception = true;
            e->exception_class = "java/lang/ClassCastException";
            return ld_fail(&f, MC_CLIENT_LIVING_DRAW_SOURCE_EXCEPTION,
                           "Living actual concrete renderer checkcast");
        }
    }
    gv(&f, 0);
    if (!ld_display(&f, mc_client_display_source_push(scope->model.display, &e->detail.display)))
        return false;
    gv(&f, 6);
    if (!ld_display(&f, mc_client_display_source_cull_test(scope->model.display, false,
                                                           &e->detail.display)))
        return false;
    manager_model *primary = r->primary;
    gv(&f, 17);
    if (actor == NULL)
        return ld_null(&f, "Living swing null actor invoke");
    float swing;
    if (!ld_entity(&f, mc_entity_render_swing(actor, partial, &swing, &e->detail.entity)))
        return false;
    if (!model_cell(&f, primary, 20))
        return false;
    primary->fields.m = swing;
    if (r->secondary != NULL) {
        manager_model *secondary = r->secondary;
        if (!model_cell(&f, secondary, 41))
            return false;
        secondary->fields.m = r->primary->fields.m;
    }
    primary = r->primary;
    gv(&f, 49);
    bool riding;
    if (!ld_entity(&f, mc_entity_render_riding(actor, &riding, &e->detail.entity)))
        return false;
    if (!model_cell(&f, primary, 52))
        return false;
    primary->fields.n = riding;
    if (r->secondary != NULL) {
        manager_model *secondary = r->secondary;
        if (!model_cell(&f, secondary, 73))
            return false;
        secondary->fields.n = r->primary->fields.n;
    }
    if (!body_try(&f, x, y, z, partial)) {
        if (e->throwable == MC_CLIENT_LIVING_DRAW_EXCEPTION) {
            e->caught.active = true;
            e->caught.operation = e->operation;
            e->caught.exception_class = e->exception_class;
            e->caught.site = e->site;
            e->caught.child = e->child;
            e->caught.selected_child = e->selected_child;
            e->caught.detail = e->detail;
            memset(&e->detail, 0, sizeof(e->detail));
            e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_NONE;
            e->throwable = MC_CLIENT_LIVING_DRAW_NO_THROWABLE;
            e->source_exception = false;
            e->exception_class = NULL;
            gv(&f, 747);
            e->child = ld_site("java/lang/Exception", "printStackTrace", "()V", 0);
            return ld_fail(&f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                           "Living caught Exception.printStackTrace child");
        }
        return false;
    }
    return tail(&f, x, y, z);
}
