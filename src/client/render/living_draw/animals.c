#include "internal.h"

static const uint16_t saddle_units[] = {'/', 'm', 'o', 'b', '/', 's', 'a', 'd',
                                        'd', 'l', 'e', '.', 'p', 'n', 'g'};
static const mc_player_string saddle = {SIZE_MAX, sizeof(saddle_units) / sizeof(*saddle_units),
                                        saddle_units};

static bool cast_actor(ld_frame *f, mc_entity_class_kind kind, const char *operation) {
    if (f->actor == NULL || mc_entity_is_kind(f->actor, kind))
        return true;
    f->error->throwable = MC_CLIENT_LIVING_DRAW_EXCEPTION;
    f->error->source_exception = true;
    f->error->exception_class = "java/lang/ClassCastException";
    return ld_fail(f, MC_CLIENT_LIVING_DRAW_SOURCE_EXCEPTION, operation);
}

bool ld_pig_pass(ld_frame *f, int32_t pass, float partial, bool *out) {
    (void)partial;
    mc_client_living_draw_error *e = f->error;
    const mc_client_graphics_site parent = e->parent;
    e->parent = ld_site("gv", "a", "(Lls;DDDFF)V", 315);
    ld_at(f, "me", "a", "(Lls;IF)Z", 2);
    if (!cast_actor(f, MC_ENTITY_CLASS_KIND_PIG, "Pig pass actual WH checkcast"))
        return false;
    ld_at(f, "me", "a", "(Lwh;IF)Z", 3);
    if (!ld_texture(f, &saddle, 3))
        return false;
    ld_at(f, "me", "a", "(Lwh;IF)Z", 7);
    if (pass != 0) {
        *out = false;
    } else {
        ld_at(f, "me", "a", "(Lwh;IF)Z", 11);
        if (f->actor == NULL)
            return ld_null(f, "Pig pass null saddle invoke");
        if (!ld_entity(f, mc_entity_render_pig_saddled(f->actor, out, &e->detail.entity)))
            return false;
    }
    ld_at(f, "me", "a", "(Lwh;IF)Z", 22);
    e->parent = parent;
    return true;
}

bool ld_pass(ld_frame *f, int32_t pass, float partial, bool *out) {
    if (strcmp(f->renderer->source_class, "xy") == 0)
        return ld_sheep_pass(f, pass, partial, out);
    if (strcmp(f->renderer->source_class, "me") == 0)
        return ld_pig_pass(f, pass, partial, out);
    if (strcmp(f->renderer->source_class, "va") != 0 &&
        strcmp(f->renderer->source_class, "ip") != 0)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED, "Living actual concrete pass body");
    /* VA and IP inherit this actual two-instruction GV body. */
    ld_at(f, "gv", "a", "(Lls;IF)Z", 0);
    *out = false;
    ld_at(f, "gv", "a", "(Lls;IF)Z", 1);
    return true;
}

bool ld_age(ld_frame *f, float partial, float *out) {
    mc_client_living_draw_error *e = f->error;
    if (strcmp(f->renderer->source_class, "ip") != 0) {
        if (strcmp(f->renderer->source_class, "xy") != 0 &&
            strcmp(f->renderer->source_class, "me") != 0 &&
            strcmp(f->renderer->source_class, "va") != 0)
            return ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                           "Living actual concrete animation body");
        ld_at(f, "gv", "c", "(Lls;F)F", 1);
        if (f->actor == NULL)
            return ld_null(f, "Living age null tick field receiver");
        const int32_t ticks = f->actor->base.bt;
        ld_at(f, "gv", "c", "(Lls;F)F", 4);
        if (!ld_fp(f))
            return false;
        *out = at_fadd((float)ticks, partial);
        ld_at(f, "gv", "c", "(Lls;F)F", 7);
        return true;
    }
    const mc_client_graphics_site parent = e->parent;
    e->parent = ld_site("gv", "a", "(Lls;DDDFF)V", 147);
    ld_at(f, "ip", "c", "(Lls;F)F", 2);
    if (!cast_actor(f, MC_ENTITY_CLASS_KIND_CHICKEN, "Chicken age actual WW checkcast"))
        return false;
    ld_at(f, "ip", "a", "(Lww;F)F", 1);
    if (f->actor == NULL)
        return ld_null(f, "Chicken flap null first field receiver");
    if (!mc_entity_has_mob(f->actor))
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Chicken actual concrete field storage");
    const mc_mob_chicken_fields *fields = &f->actor->mob->chicken;
    const float first_g = fields->g;
    const float first_b = fields->b;
    const float first_g_again = fields->g;
    ld_at(f, "ip", "a", "(Lww;F)F", 12);
    if (!ld_fp(f))
        return false;
    const float first = at_fadd(first_g, at_fmul(at_fsub(first_b, first_g_again), partial));
    const float second_f = fields->f;
    const float second_c = fields->c;
    const float second_f_again = fields->f;
    const float second = at_fadd(second_f, at_fmul(at_fsub(second_c, second_f_again), partial));
    e->state.flap_first = first;
    e->state.flap_second = second;
    e->state.flap_arguments_read = true;
    ld_at(f, "ip", "a", "(Lww;F)F", 36);
    if (!ld_capture(f, mc_client_draw_capture_math(f->scope, &e->detail.capture)))
        return false;
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_MATH;
    e->child = ld_site("in", "a", "(F)F", 0);
    if (f->scope->model.math == NULL)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_UNIMPLEMENTED,
                       "Chicken flap reached actual Math namespace");
    const mc_math_class_status status =
        mc_math_classes_ensure(f->scope->model.math, &e->detail.math);
    if (status != MC_MATH_CLASS_OK) {
        if (e->detail.math.source_pc >= 0)
            e->child = ld_site("in", "<clinit>", "()V", e->detail.math.source_pc);
        return ld_fail(f,
                       status == MC_MATH_CLASS_LIMIT ? MC_CLIENT_LIVING_DRAW_LIMIT
                                                     : MC_CLIENT_LIVING_DRAW_CHILD_FAILED,
                       "Chicken flap actual Math active use");
    }
    if (!ld_fp(f))
        return false;
    const mc_math_trig *table = mc_math_classes_table(f->scope->model.math);
    if (table == NULL)
        return ld_fail(f, MC_CLIENT_LIVING_DRAW_STATE, "Chicken actual READY Math table");
    const float sine = mc_math_sin(table, first);
    e->state.flap_math_returned = true;
    *out = at_fmul(at_fadd(sine, 1.0f), second);
    e->selected_child = MC_CLIENT_LIVING_DRAW_CHILD_NONE;
    e->child = (mc_client_graphics_site){.pc = -1};
    e->parent = parent;
    ld_at(f, "ip", "a", "(Lww;F)F", 44);
    return true;
}
