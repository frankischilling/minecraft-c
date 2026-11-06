#include "internal.h"

#include <string.h>

static void at(mc_client_model_runtime_error *error, int32_t pc) {
    model_runtime_at(error, "fh", "b", "(FFFFFF)V", pc);
}

static float *axis(mc_client_model_part *part, unsigned int index) {
    switch (index) {
        case 0:
            return &part->a;
        case 1:
            return &part->b;
        case 2:
            return &part->c;
        case 3:
            return &part->d;
        case 4:
            return &part->e;
        default:
            return &part->f;
    }
}

static bool read(mc_client_model_runtime_error *error, mc_client_model_part *part,
                 unsigned int index, int32_t pc, float *out) {
    at(error, pc);
    error->state.part = part;
    if (part == NULL)
        return model_runtime_null(error, "Biped null pose field receiver");
    memcpy(out, axis(part, index), sizeof(*out));
    return true;
}

static bool store(mc_client_model_runtime_error *error, mc_client_model_part *part,
                  unsigned int index, int32_t pc, float value) {
    at(error, pc);
    error->state.part = part;
    if (part == NULL)
        return model_runtime_null(error, "Biped null captured pose store receiver");
    memcpy(axis(part, index), &value, sizeof(value));
    ++error->state.pose_stores;
    return true;
}

static bool math_value(mc_client_model_runtime_error *error,
                       const mc_client_model_runtime_context *context, model_runtime_math_kind kind,
                       int32_t pc, float value, float *out) {
    at(error, pc);
    return model_runtime_math_value(context, kind, value, out, error);
}

bool model_runtime_biped(const manager_model *model) {
    return strcmp(model->source_class, "fh") == 0 || strcmp(model->source_class, "ej") == 0 ||
           strcmp(model->source_class, "lc") == 0;
}

bool model_runtime_fh_pose(manager_model *model, const float v[6],
                           const mc_client_model_runtime_context *context,
                           mc_client_model_runtime_error *error) {
    mc_client_model_part *part = model->fields.a;
    float value, t, swing, easing, pitch_factor;
    at(error, 8);
    if (!model_runtime_fp(error) || !store(error, part, 4, 9, at_fdiv(v[3], 57.295776f)))
        return false;
    part = model->fields.a;
    if (!store(error, part, 3, 21, at_fdiv(v[4], 57.295776f)))
        return false;
    part = model->fields.b;
    if (!read(error, model->fields.a, 4, 32, &value) || !store(error, part, 4, 35, value))
        return false;
    part = model->fields.b;
    if (!read(error, model->fields.a, 3, 46, &value) || !store(error, part, 3, 49, value))
        return false;
    part = model->fields.d;
    if (!math_value(error, context, MODEL_RUNTIME_COSINE, 63,
                    at_fadd(at_fmul(v[0], .6662f), 3.1415927f), &value) ||
        !store(error, part, 3, 73, at_fmul(at_fmul(at_fmul(value, 2.0f), v[1]), .5f)))
        return false;
    part = model->fields.e;
    if (!math_value(error, context, MODEL_RUNTIME_COSINE, 84, at_fmul(v[0], .6662f), &value) ||
        !store(error, part, 3, 94, at_fmul(at_fmul(at_fmul(value, 2.0f), v[1]), .5f)) ||
        !store(error, model->fields.d, 5, 102, 0.0f) ||
        !store(error, model->fields.e, 5, 110, 0.0f))
        return false;
    part = model->fields.f;
    if (!math_value(error, context, MODEL_RUNTIME_COSINE, 121, at_fmul(v[0], .6662f), &value) ||
        !store(error, part, 3, 129, at_fmul(at_fmul(value, 1.4f), v[1])))
        return false;
    part = model->fields.g;
    if (!math_value(error, context, MODEL_RUNTIME_COSINE, 143,
                    at_fadd(at_fmul(v[0], .6662f), 3.1415927f), &value) ||
        !store(error, part, 3, 151, at_fmul(at_fmul(value, 1.4f), v[1])) ||
        !store(error, model->fields.f, 4, 159, 0.0f) ||
        !store(error, model->fields.g, 4, 167, 0.0f))
        return false;
    if (model->fields.n) {
        part = model->fields.d;
        if (!read(error, part, 3, 182, &value) ||
            !store(error, part, 3, 188, at_fadd(value, -.62831855f)))
            return false;
        part = model->fields.e;
        if (!read(error, part, 3, 196, &value) ||
            !store(error, part, 3, 202, at_fadd(value, -.62831855f)) ||
            !store(error, model->fields.f, 3, 211, -1.2566371f) ||
            !store(error, model->fields.g, 3, 220, -1.2566371f) ||
            !store(error, model->fields.f, 4, 229, .31415927f) ||
            !store(error, model->fields.g, 4, 238, -.31415927f))
            return false;
    }
    if (model->fields.j) {
        part = model->fields.e;
        if (!read(error, model->fields.e, 3, 256, &value) ||
            !store(error, part, 3, 265, at_fsub(at_fmul(value, .5f), .31415927f)))
            return false;
    }
    if (model->fields.k) {
        part = model->fields.d;
        if (!read(error, model->fields.d, 3, 283, &value) ||
            !store(error, part, 3, 292, at_fsub(at_fmul(value, .5f), .31415927f)))
            return false;
    }
    if (!store(error, model->fields.d, 4, 300, 0.0f) ||
        !store(error, model->fields.e, 4, 308, 0.0f))
        return false;
    /* fcmpl318: NaN takes the same skip branch as <= -9990. */
    if (model->fields.m > -9990.0f) {
        swing = model->fields.m;
        part = model->fields.c;
        if (!math_value(error, context, MODEL_RUNTIME_SQUARE_ROOT, 333, swing, &value) ||
            !math_value(error, context, MODEL_RUNTIME_SINE, 341,
                        at_fmul(at_fmul(value, 3.1415927f), 2.0f), &value) ||
            !store(error, part, 4, 347, at_fmul(value, .2f)))
            return false;
        part = model->fields.d;
        if (!read(error, model->fields.c, 4, 358, &value) ||
            !math_value(error, context, MODEL_RUNTIME_SINE, 361, value, &value) ||
            !store(error, part, 2, 367, at_fmul(value, 5.0f)))
            return false;
        part = model->fields.d;
        if (!read(error, model->fields.c, 4, 378, &value) ||
            !math_value(error, context, MODEL_RUNTIME_COSINE, 381, value, &value) ||
            !store(error, part, 0, 388, at_fmul(-value, 5.0f)))
            return false;
        part = model->fields.e;
        if (!read(error, model->fields.c, 4, 399, &value) ||
            !math_value(error, context, MODEL_RUNTIME_SINE, 402, value, &value) ||
            !store(error, part, 2, 409, at_fmul(-value, 5.0f)))
            return false;
        part = model->fields.e;
        if (!read(error, model->fields.c, 4, 420, &value) ||
            !math_value(error, context, MODEL_RUNTIME_COSINE, 423, value, &value) ||
            !store(error, part, 0, 429, at_fmul(value, 5.0f)))
            return false;
        part = model->fields.d;
        if (!read(error, part, 4, 437, &value) || !read(error, model->fields.c, 4, 444, &t) ||
            !store(error, part, 4, 448, at_fadd(value, t)))
            return false;
        part = model->fields.e;
        if (!read(error, part, 4, 456, &value) || !read(error, model->fields.c, 4, 463, &t) ||
            !store(error, part, 4, 467, at_fadd(value, t)))
            return false;
        part = model->fields.e;
        if (!read(error, part, 3, 475, &value) || !read(error, model->fields.c, 4, 482, &t) ||
            !store(error, part, 3, 486, at_fadd(value, t)))
            return false;
        swing = at_fsub(1.0f, model->fields.m);
        swing = at_fmul(swing, swing);
        swing = at_fmul(swing, swing);
        swing = at_fsub(1.0f, swing);
        if (!math_value(error, context, MODEL_RUNTIME_SINE, 522, at_fmul(swing, 3.1415927f),
                        &easing) ||
            !math_value(error, context, MODEL_RUNTIME_SINE, 534,
                        at_fmul(model->fields.m, 3.1415927f), &value) ||
            !read(error, model->fields.a, 3, 541, &t))
            return false;
        pitch_factor = at_fmul(at_fmul(value, -at_fsub(t, .7f)), .75f);
        part = model->fields.d;
        if (!read(error, part, 3, 559, &value) ||
            !store(error, part, 3, 576,
                   at_narrow(at_sub(at_widen(value), at_add(at_mul(at_widen(easing), 1.2),
                                                            at_widen(pitch_factor))))))
            return false;
        part = model->fields.d;
        if (!read(error, part, 4, 584, &value) || !read(error, model->fields.c, 4, 591, &t) ||
            !store(error, part, 4, 597, at_fadd(value, at_fmul(t, 2.0f))))
            return false;
        part = model->fields.d;
        if (!math_value(error, context, MODEL_RUNTIME_SINE, 611,
                        at_fmul(model->fields.m, 3.1415927f), &value) ||
            !store(error, part, 5, 617, at_fmul(value, -.4f)))
            return false;
    }
    if (model->fields.l) {
        if (!store(error, model->fields.c, 3, 633, .5f))
            return false;
        part = model->fields.f;
        if (!read(error, part, 3, 641, &value) || !store(error, part, 3, 646, at_fsub(value, 0.0f)))
            return false;
        part = model->fields.g;
        if (!read(error, part, 3, 654, &value) || !store(error, part, 3, 659, at_fsub(value, 0.0f)))
            return false;
        part = model->fields.d;
        if (!read(error, part, 3, 667, &value) || !store(error, part, 3, 673, at_fadd(value, .4f)))
            return false;
        part = model->fields.e;
        if (!read(error, part, 3, 681, &value) ||
            !store(error, part, 3, 687, at_fadd(value, .4f)) ||
            !store(error, model->fields.f, 2, 696, 4.0f) ||
            !store(error, model->fields.g, 2, 705, 4.0f) ||
            !store(error, model->fields.f, 1, 714, 9.0f) ||
            !store(error, model->fields.g, 1, 723, 9.0f) ||
            !store(error, model->fields.a, 1, 731, 1.0f))
            return false;
    } else if (!store(error, model->fields.c, 3, 742, 0.0f) ||
               !store(error, model->fields.f, 2, 750, 0.0f) ||
               !store(error, model->fields.g, 2, 758, 0.0f) ||
               !store(error, model->fields.f, 1, 767, 12.0f) ||
               !store(error, model->fields.g, 1, 776, 12.0f) ||
               !store(error, model->fields.a, 1, 784, 0.0f))
        return false;
    part = model->fields.d;
    if (!read(error, part, 5, 792, &value) ||
        !math_value(error, context, MODEL_RUNTIME_COSINE, 799, at_fmul(v[2], .09f), &t) ||
        !store(error, part, 5, 809, at_fadd(value, at_fadd(at_fmul(t, .05f), .05f))))
        return false;
    part = model->fields.e;
    if (!read(error, part, 5, 817, &value) ||
        !math_value(error, context, MODEL_RUNTIME_COSINE, 824, at_fmul(v[2], .09f), &t) ||
        !store(error, part, 5, 834, at_fsub(value, at_fadd(at_fmul(t, .05f), .05f))))
        return false;
    part = model->fields.d;
    if (!read(error, part, 3, 842, &value) ||
        !math_value(error, context, MODEL_RUNTIME_SINE, 849, at_fmul(v[2], .067f), &t) ||
        !store(error, part, 3, 856, at_fadd(value, at_fmul(t, .05f))))
        return false;
    part = model->fields.e;
    if (!read(error, part, 3, 864, &value) ||
        !math_value(error, context, MODEL_RUNTIME_SINE, 871, at_fmul(v[2], .067f), &t) ||
        !store(error, part, 3, 878, at_fsub(value, at_fmul(t, .05f))))
        return false;
    at(error, 881);
    error->state.fh_pose_returned = true;
    return true;
}

bool model_runtime_fh_render(mc_client_managed_renderer *owner, manager_model *model,
                             const float values[6], const mc_client_model_runtime_context *context,
                             mc_client_model_runtime_error *error) {
    model_runtime_at(error, "fh", "a", "(FFFFFF)V", 10);
    if (!model_runtime_call_push(error) || !model_runtime_pose_body(model, values, context, error))
        return false;
    model_runtime_call_pop(error);
    /* Native slot references are not an invented Source part array. LC's
     * displaced constructor limbs and current h/i are intentionally untouched. */
    mc_client_model_part **const slots[7] = {&model->fields.a, &model->fields.c, &model->fields.d,
                                             &model->fields.e, &model->fields.f, &model->fields.g,
                                             &model->fields.b};
    static const int32_t pcs[7] = {19, 28, 37, 46, 55, 64, 73};
    for (size_t i = 0; i < 7; ++i) {
        mc_client_model_part *part = *slots[i];
        model_runtime_at(error, "fh", "a", "(FFFFFF)V", pcs[i]);
        if (!model_runtime_draw_part(owner, part, values[5], context, error))
            return false;
    }
    model_runtime_at(error, "fh", "a", "(FFFFFF)V", 76);
    error->state.fh_render_returned = true;
    return true;
}
