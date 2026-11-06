#include "internal.h"

static void at(mc_client_model_runtime_error *error, int32_t pc) {
    model_runtime_at(error, "ej", "b", "(FFFFFF)V", pc);
}

static float *axis(mc_client_model_part *part, unsigned int index) {
    return index == 3 ? &part->d : index == 4 ? &part->e : &part->f;
}

static bool store(mc_client_model_runtime_error *error, mc_client_model_part *part,
                  unsigned int index, int32_t pc, float value) {
    at(error, pc);
    error->state.part = part;
    if (part == NULL)
        return model_runtime_null(error, "Zombie null captured pose store receiver");
    *axis(part, index) = value;
    ++error->state.pose_stores;
    return true;
}

static bool read(mc_client_model_runtime_error *error, mc_client_model_part *part,
                 unsigned int index, int32_t pc, float *value) {
    at(error, pc);
    error->state.part = part;
    if (part == NULL)
        return model_runtime_null(error, "Zombie null pose field receiver");
    *value = *axis(part, index);
    return true;
}

static bool math_value(mc_client_model_runtime_error *error,
                       const mc_client_model_runtime_context *context, model_runtime_math_kind kind,
                       int32_t pc, float value, float *out) {
    at(error, pc);
    return model_runtime_math_value(context, kind, value, out, error);
}

bool model_runtime_ej_pose(manager_model *model, const float v[6],
                           const mc_client_model_runtime_context *context,
                           mc_client_model_runtime_error *error) {
    at(error, 10);
    if (!model_runtime_call_push(error) || !model_runtime_fh_pose(model, v, context, error))
        return false;
    model_runtime_call_pop(error);
    float first, second, value, t;
    if (!math_value(error, context, MODEL_RUNTIME_SINE, 20, at_fmul(model->fields.m, 3.1415927f),
                    &first))
        return false;
    const float left = at_fsub(1.0f, model->fields.m);
    const float right = at_fsub(1.0f, model->fields.m);
    if (!math_value(error, context, MODEL_RUNTIME_SINE, 43,
                    at_fmul(at_fsub(1.0f, at_fmul(left, right)), 3.1415927f), &second) ||
        !store(error, model->fields.d, 5, 53, 0.0f) ||
        !store(error, model->fields.e, 5, 61, 0.0f) ||
        !store(error, model->fields.d, 4, 77, -at_fsub(.1f, at_fmul(first, .6f))) ||
        !store(error, model->fields.e, 4, 92, at_fsub(.1f, at_fmul(first, .6f))) ||
        !store(error, model->fields.d, 3, 101, -1.5707964f) ||
        !store(error, model->fields.e, 3, 110, -1.5707964f))
        return false;
    mc_client_model_part *part = model->fields.d;
    if (!read(error, part, 3, 118, &value) ||
        !store(error, part, 3, 133,
               at_fsub(value, at_fsub(at_fmul(first, 1.2f), at_fmul(second, .4f)))))
        return false;
    part = model->fields.e;
    if (!read(error, part, 3, 141, &value) ||
        !store(error, part, 3, 156,
               at_fsub(value, at_fsub(at_fmul(first, 1.2f), at_fmul(second, .4f)))))
        return false;
    part = model->fields.d;
    if (!read(error, part, 5, 164, &value) ||
        !math_value(error, context, MODEL_RUNTIME_COSINE, 171, at_fmul(v[2], .09f), &t) ||
        !store(error, part, 5, 181, at_fadd(value, at_fadd(at_fmul(t, .05f), .05f))))
        return false;
    part = model->fields.e;
    if (!read(error, part, 5, 189, &value) ||
        !math_value(error, context, MODEL_RUNTIME_COSINE, 196, at_fmul(v[2], .09f), &t) ||
        !store(error, part, 5, 206, at_fsub(value, at_fadd(at_fmul(t, .05f), .05f))))
        return false;
    part = model->fields.d;
    if (!read(error, part, 3, 214, &value) ||
        !math_value(error, context, MODEL_RUNTIME_SINE, 221, at_fmul(v[2], .067f), &t) ||
        !store(error, part, 3, 228, at_fadd(value, at_fmul(t, .05f))))
        return false;
    part = model->fields.e;
    if (!read(error, part, 3, 236, &value) ||
        !math_value(error, context, MODEL_RUNTIME_SINE, 243, at_fmul(v[2], .067f), &t) ||
        !store(error, part, 3, 250, at_fsub(value, at_fmul(t, .05f))))
        return false;
    at(error, 253);
    error->state.ej_pose_returned = true;
    return true;
}
