#include "internal.h"

static void gv(ld_frame *f, int32_t pc) {
    ld_at(f, "gv", "a", "(Lls;DDDFF)V", pc);
}

static bool overlay_pass(ld_frame *f, int32_t pass, float partial, bool *out) {
    mc_client_living_draw_error *e = f->error;
    const mc_client_graphics_site parent = e->parent;
    e->parent = e->site; /* Actual GV.a506/666, not its ordinary pass315. */
    if (strcmp(f->renderer->source_class, "m") == 0) {
        /* The M.b override returns false. It must not enter the powered M.a
         * pass, including its texture/matrix side effects. */
        ld_at(f, "m", "b", "(Lls;IF)Z", 2);
        if (!ld_cast_actor(f, MC_ENTITY_CLASS_KIND_CREEPER, "Creeper overlay actual GB checkcast"))
            return false;
        ld_at(f, "m", "b", "(Lgb;IF)Z", 1);
        *out = false;
    } else {
        /* Actual inherited GV.b forwards to the virtual a at4. Predicates
         * execute anew; a previously selected pass is never replayed. */
        ld_at(f, "gv", "b", "(Lls;IF)Z", 4);
        e->parent = e->site;
        if (!ld_pass(f, pass, partial, out))
            return false;
        ld_at(f, "gv", "b", "(Lls;IF)Z", 7);
    }
    e->parent = parent;
    return true;
}

static bool overlay_color(ld_frame *f, const float rgba[4], int32_t pc) {
    gv(f, pc);
    return ld_display(f,
                      mc_client_display_source_color(f->scope->model.display, rgba[0], rgba[1],
                                                     rgba[2], rgba[3], &f->error->detail.display));
}

static bool overlay_model(ld_frame *f, manager_model *model, int32_t pc,
                          mc_client_model_runtime_state *receipt, bool *attempted) {
    mc_client_living_draw_error *e = f->error;
    const mc_client_living_draw_state *s = &e->state;
    gv(f, pc == 488 ? 483 : pc == 540 ? 535 : pc == 648 ? 643 : 697);
    if (!ld_fp(f))
        return false;
    const float yaw = at_fsub(s->head_yaw, s->body_yaw);
    gv(f, pc);
    if (attempted != NULL)
        *attempted = true;
    if (!ld_model(f, mc_client_model_render_entered(f->renderer, model, s->limb_swing,
                                                    s->limb_amount, s->age, yaw, s->pitch, s->scale,
                                                    &f->scope->model, e->site, &e->detail.model)))
        return false;
    *receipt = e->detail.model.state;
    return true;
}

static bool redraw(ld_frame *f, mc_client_living_overlay_state *o, float partial, bool hurt) {
    o->entered = true;
    if (!overlay_color(f, o->rgba, hurt ? 466 : 626))
        return false;
    if (!overlay_model(f, f->renderer->primary, hurt ? 488 : 648, &o->primary,
                       &o->primary_attempted))
        return false;
    o->primary_returned = true;
    for (int32_t pass = 0; pass < 4; ++pass) {
        gv(f, hurt ? 506 : 666);
        if (!ld_work(f))
            return false;
        o->pass_attempted[(size_t)pass] = true;
        bool selected;
        if (!overlay_pass(f, pass, partial, &selected))
            return false;
        o->pass_result[(size_t)pass] = selected;
        if (!selected)
            continue;
        /* A predicate can change the current color. Source restores the
         * overlay color before each selected secondary model invocation. */
        if (!overlay_color(f, o->rgba, hurt ? 518 : 680))
            return false;
        if (!overlay_model(f, f->renderer->secondary, hurt ? 540 : 702,
                           &o->pass_render[(size_t)pass], NULL))
            return false;
        o->pass_render_returned[(size_t)pass] = true;
    }
    o->returned = true;
    return true;
}

bool ld_overlays(ld_frame *f, float brightness, int32_t color, float partial) {
    mc_client_living_draw_error *e = f->error;
    mc_client_living_draw_state *s = &e->state;
    const uint32_t bits = (uint32_t)color;
    const uint32_t alpha = bits >> 24;
    const mc_living_entity_fields *ls = mc_entity_living_fields(f->actor);
    gv(f, 400);
    if (alpha == 0 && ls->aa <= 0 && ls->ad <= 0)
        return true;
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
    if (!ld_display(f, mc_client_display_source_blend_function(f->scope->model.display, 770, 771,
                                                               &e->detail.display)))
        return false;
    gv(f, 443);
    if (!ld_display(f, mc_client_display_source_depth_function(f->scope->model.display, 514,
                                                               &e->detail.display)))
        return false;
    gv(f, 447);
    ls = mc_entity_living_fields(f->actor); /* Actual fresh reads after GL calls. */
    if (ls->aa > 0 || ls->ad > 0) {
        s->hurt_overlay.rgba[0] = brightness;
        s->hurt_overlay.rgba[1] = s->hurt_overlay.rgba[2] = 0.0f;
        s->hurt_overlay.rgba[3] = .4f;
        if (!redraw(f, &s->hurt_overlay, partial, true))
            return false;
    }
    if (alpha > 0) {
        gv(f, 573);
        if (!ld_fp(f))
            return false;
        static const unsigned int shifts[] = {16, 8, 0, 24};
        for (size_t i = 0; i < 4; ++i)
            s->color_overlay.rgba[i] = at_fdiv((float)((bits >> shifts[i]) & 255u), 255.0f);
        if (!redraw(f, &s->color_overlay, partial, false))
            return false;
    }
    gv(f, 714);
    if (!ld_display(f, mc_client_display_source_depth_function(f->scope->model.display, 515,
                                                               &e->detail.display)))
        return false;
    gv(f, 720);
    if (!ld_display(f, mc_client_display_source_blend_enable(f->scope->model.display, false,
                                                             &e->detail.display)))
        return false;
    gv(f, 726);
    if (!ld_display(f, mc_client_display_source_alpha_test(f->scope->model.display, true,
                                                           &e->detail.display)))
        return false;
    gv(f, 732);
    if (!ld_display(f, mc_client_display_source_texture_enable(f->scope->model.display, true,
                                                               &e->detail.display)))
        return false;
    s->overlay_state_restored = true;
    return true;
}
