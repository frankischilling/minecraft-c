#include "../../../entity/entity_internal.h"
#include "../../display/source_surface.h"
#include "internal.h"

static void at(mc_client_model_runtime_error *error, const char *method, int32_t pc) {
    model_runtime_at(error, "o", method, "(FFFFFF)V", pc);
}

static void event_at(mc_client_model_runtime_error *error, int32_t pc) {
    model_runtime_at(error, "o", "a", "(Lls;FFF)V", pc);
}

static float *angle(mc_client_model_part *part, unsigned int index) {
    return index == 3 ? &part->d : index == 4 ? &part->e : &part->f;
}

static bool store(mc_client_model_runtime_error *error, mc_client_model_part *part,
                  unsigned int index, float value) {
    error->state.part = part;
    if (part == NULL)
        return model_runtime_null(error, "Wolf null captured pose store receiver");
    *angle(part, index) = value;
    ++error->state.pose_stores;
    return true;
}

static bool read(mc_client_model_runtime_error *error, mc_client_model_part *part,
                 unsigned int index, float *value) {
    error->state.part = part;
    if (part == NULL)
        return model_runtime_null(error, "Wolf null head pose read receiver");
    *value = *angle(part, index);
    return true;
}

static bool pivot(mc_client_model_runtime_error *error, mc_client_model_part *part, float x,
                  float y, float z) {
    error->state.part = part;
    ++error->state.pivot_calls;
    if (part == NULL)
        return model_runtime_null(error, "Wolf null PS pivot invocation receiver");
    /* Complete real PS.a(FFF), putfield2/7/12, return15. No GL/service/flag read. */
    part->a = x;
    ++error->state.pivot_stores;
    part->b = y;
    ++error->state.pivot_stores;
    part->c = z;
    ++error->state.pivot_stores;
    ++error->state.pivot_returns;
    return true;
}

bool model_runtime_o_pose(manager_model *model, const float values[6],
                          const mc_client_model_runtime_context *context,
                          mc_client_model_runtime_error *error) {
    (void)context;
    at(error, "b", 10);
    if (!model_runtime_call_push(error))
        return false;
    model_runtime_at(error, "ko", "b", "(FFFFFF)V", 0); /* Actual empty body. */
    model_runtime_call_pop(error);
    mc_client_model_part *part = model->fields.a;
    at(error, "b", 21);
    if (!model_runtime_fp(error))
        return false;
    float value = at_fdiv(values[4], 57.295776f);
    at(error, "b", 22);
    if (!store(error, part, 3, value))
        return false;
    part = model->fields.a;
    value = at_fdiv(values[3], 57.295776f);
    at(error, "b", 34);
    if (!store(error, part, 4, value))
        return false;
    mc_client_model_part **const targets[6] = {&model->fields.g, &model->fields.g,
                                               &model->fields.h, &model->fields.h,
                                               &model->fields.i, &model->fields.i};
    static const int32_t reads[6] = {45, 59, 73, 87, 101, 115};
    static const int32_t stores[6] = {48, 62, 76, 90, 104, 118};
    for (size_t i = 0; i < 6; ++i) {
        part = *targets[i];
        at(error, "b", reads[i]);
        const unsigned int index = (i & 1u) == 0 ? 4u : 3u;
        if (!read(error, model->fields.a, index, &value))
            return false;
        at(error, "b", stores[i]);
        if (!store(error, part, index, value))
            return false;
    }
    part = model->extra_j;
    at(error, "b", 126);
    if (!store(error, part, 3, values[2]))
        return false;
    at(error, "b", 129);
    return true;
}

bool model_runtime_o_render(mc_client_managed_renderer *owner, manager_model *model,
                            const float values[6], const mc_client_model_runtime_context *context,
                            mc_client_model_runtime_error *error) {
    at(error, "a", 10);
    if (!model_runtime_call_push(error))
        return false;
    model_runtime_at(error, "ko", "a", "(FFFFFF)V", 0); /* Actual empty body. */
    model_runtime_call_pop(error);
    at(error, "a", 23);
    if (!model_runtime_call_push(error) || !model_runtime_pose_body(model, values, context, error))
        return false;
    model_runtime_call_pop(error);
    mc_client_model_part **const slots[11] = {&model->fields.a, &model->fields.b, &model->fields.c,
                                              &model->fields.d, &model->fields.e, &model->fields.f,
                                              &model->fields.g, &model->fields.h, &model->fields.i,
                                              &model->extra_j,  &model->extra_k};
    static const int32_t pcs[11] = {32, 41, 50, 59, 68, 77, 86, 95, 104, 113, 122};
    for (size_t i = 0; i < 11; ++i) {
        mc_client_model_part *part = *slots[i];
        at(error, "a", pcs[i]);
        const bool rotated = i == 0 || (i >= 6 && i <= 9);
        if (!(rotated ? model_runtime_draw_part_rotated(owner, part, values[5], context, error)
                      : model_runtime_draw_part(owner, part, values[5], context, error)))
            return false;
    }
    at(error, "a", 125);
    return true;
}

static bool event_store(mc_client_model_runtime_error *error, mc_client_model_part *part,
                        unsigned int index, int32_t pc, float value) {
    event_at(error, pc);
    return store(error, part, index, value);
}

static bool event_pivot(mc_client_model_runtime_error *error, mc_client_model_part *part,
                        int32_t pc, float x, float y, float z) {
    event_at(error, pc);
    return pivot(error, part, x, y, z);
}

bool model_runtime_o_event(manager_model *model, mc_entity *actor, const float v[3],
                           const mc_client_model_runtime_context *context,
                           mc_client_model_runtime_error *error) {
    event_at(error, 1);
    if (actor != NULL && !mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_WOLF)) {
        error->source_exception = true;
        error->throwable = MC_CLIENT_MODEL_DRAW_SOURCE_EXCEPTION;
        error->exception_class = "java/lang/ClassCastException";
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_SOURCE_EXCEPTION,
                                  "Wolf actual GI event checkcast");
    }
    event_at(error, 8);
    if (actor == NULL)
        return model_runtime_null(error, "Wolf null GI angry invocation receiver");
    bool flag;
    if (!model_runtime_entity_finish(
            context, mc_entity_render_wolf_angry(actor, &flag, &error->entity), error))
        return false;
    mc_client_model_part *part = model->extra_j;
    float value;
    if (flag) {
        if (!event_store(error, part, 4, 19, 0.0f))
            return false;
    } else {
        event_at(error, 32);
        if (!model_runtime_fp(error))
            return false;
        const float operand = at_fmul(v[0], .6662f);
        event_at(error, 33);
        if (!model_runtime_math_value(context, MODEL_RUNTIME_COSINE, operand, &value, error) ||
            !event_store(error, part, 4, 41, at_fmul(at_fmul(value, 1.4f), v[1])))
            return false;
    }
    event_at(error, 46);
    if (!model_runtime_entity_finish(
            context, mc_entity_render_wolf_sitting(actor, &flag, &error->entity), error))
        return false;
    if (flag) {
        if (!event_pivot(error, model->extra_k, 62, -1.0f, 16.0f, -3.0f) ||
            !event_store(error, model->extra_k, 3, 71, 1.2566371f) ||
            !event_store(error, model->extra_k, 4, 79, 0.0f) ||
            !event_pivot(error, model->fields.b, 90, 0.0f, 18.0f, 0.0f) ||
            !event_store(error, model->fields.b, 3, 99, .7853982f) ||
            !event_pivot(error, model->extra_j, 112, -1.0f, 21.0f, 6.0f) ||
            !event_pivot(error, model->fields.c, 124, -2.5f, 22.0f, 2.0f) ||
            !event_store(error, model->fields.c, 3, 133, 4.712389f) ||
            !event_pivot(error, model->fields.d, 145, .5f, 22.0f, 2.0f) ||
            !event_store(error, model->fields.d, 3, 154, 4.712389f) ||
            !event_store(error, model->fields.e, 3, 163, 5.811947f) ||
            !event_pivot(error, model->fields.e, 176, -2.49f, 17.0f, -4.0f) ||
            !event_store(error, model->fields.f, 3, 185, 5.811947f) ||
            !event_pivot(error, model->fields.f, 198, .51f, 17.0f, -4.0f))
            return false;
    } else {
        if (!event_pivot(error, model->fields.b, 212, 0.0f, 14.0f, 2.0f) ||
            !event_store(error, model->fields.b, 3, 221, 1.5707964f) ||
            !event_pivot(error, model->extra_k, 234, -1.0f, 14.0f, -3.0f))
            return false;
        part = model->extra_k;
        event_at(error, 245);
        if (!read(error, model->fields.b, 3, &value) || !event_store(error, part, 3, 248, value) ||
            !event_pivot(error, model->extra_j, 261, -1.0f, 12.0f, 8.0f) ||
            !event_pivot(error, model->fields.c, 274, -2.5f, 16.0f, 7.0f) ||
            !event_pivot(error, model->fields.d, 287, .5f, 16.0f, 7.0f) ||
            !event_pivot(error, model->fields.e, 300, -2.5f, 16.0f, -4.0f) ||
            !event_pivot(error, model->fields.f, 313, .5f, 16.0f, -4.0f))
            return false;
        mc_client_model_part **const legs[4] = {&model->fields.c, &model->fields.d,
                                                &model->fields.e, &model->fields.f};
        static const int32_t arithmetic_pcs[4] = {323, 342, 364, 386};
        static const int32_t math_pcs[4] = {324, 346, 368, 387},
                             store_pcs[4] = {332, 354, 376, 395};
        for (size_t i = 0; i < 4; ++i) {
            part = *legs[i];
            event_at(error, arithmetic_pcs[i]);
            if (!model_runtime_fp(error))
                return false;
            float operand = at_fmul(v[0], .6662f);
            if (i == 1 || i == 2)
                operand = at_fadd(operand, 3.1415927f);
            event_at(error, math_pcs[i]);
            if (!model_runtime_math_value(context, MODEL_RUNTIME_COSINE, operand, &value, error) ||
                !event_store(error, part, 3, store_pcs[i], at_fmul(at_fmul(value, 1.4f), v[1])))
                return false;
        }
    }
    float head, shake;
    event_at(error, 402);
    if (!model_runtime_entity_finish(
            context, mc_entity_render_wolf_head(actor, v[2], &head, &error->entity), error))
        return false;
    event_at(error, 410);
    if (!model_runtime_entity_finish(
            context,
            mc_entity_render_wolf_shake(actor, context ? context->math : NULL, v[2], 0.0f, &shake,
                                        &error->entity),
            error))
        return false;
    value = at_fadd(head, shake);
    if (!event_store(error, model->fields.a, 5, 422, value) ||
        !event_store(error, model->fields.g, 5, 431, value) ||
        !event_store(error, model->fields.h, 5, 440, value) ||
        !event_store(error, model->fields.i, 5, 449, value))
        return false;
    mc_client_model_part **const shake_parts[3] = {&model->extra_k, &model->fields.b,
                                                   &model->extra_j};
    static const float offsets[3] = {-.08f, -.16f, -.2f};
    static const int32_t calls[3] = {462, 478, 494}, stores[3] = {465, 481, 497};
    for (size_t i = 0; i < 3; ++i) {
        part = *shake_parts[i]; /* Captured BEFORE the actual GI.a(FF) invocation. */
        event_at(error, calls[i]);
        if (!model_runtime_entity_finish(
                context,
                mc_entity_render_wolf_shake(actor, context ? context->math : NULL, v[2], offsets[i],
                                            &value, &error->entity),
                error) ||
            !event_store(error, part, 5, stores[i], value))
            return false;
    }
    event_at(error, 502);
    if (!model_runtime_entity_finish(
            context, mc_entity_render_wolf_wet(actor, &flag, &error->entity), error))
        return false;
    if (flag) {
        float brightness, wet_scale;
        event_at(error, 512);
        if (!model_runtime_entity_finish(
                context,
                mc_entity_render_brightness(actor, context ? context->math : NULL, v[2],
                                            &brightness, &error->entity),
                error))
            return false;
        event_at(error, 519);
        if (!model_runtime_entity_finish(
                context, mc_entity_render_wolf_wet_scale(actor, v[2], &wet_scale, &error->entity),
                error))
            return false;
        value = at_fmul(brightness, wet_scale);
        event_at(error, 531);
        mc_client_display *display = context ? context->display : NULL;
        if (!mc_client_display_source_color(display, value, value, value, 1.0f, &error->display)) {
            error->selected_child = MC_CLIENT_MODEL_RUNTIME_CHILD_DISPLAY;
            error->child =
                (mc_client_graphics_site){"org/lwjgl/opengl/GL11", "glColor3f", "(FFF)V", -1};
            return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_DISPLAY_FAILED,
                                      error->display.operation);
        }
    }
    event_at(error, 534);
    return true;
}
