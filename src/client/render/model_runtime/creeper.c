#include "internal.h"

static void creeper_at(mc_client_model_runtime_error *error, const char *method, int32_t pc) {
    model_runtime_at(error, "il", method, "(FFFFFF)V", pc);
}

static bool store(mc_client_model_part *part, float value, bool yaw,
                  mc_client_model_runtime_error *error) {
    error->state.part = part;
    if (part == NULL)
        return model_runtime_null(error, "Creeper null captured pose part receiver");
    if (yaw)
        part->e = value;
    else
        part->d = value;
    ++error->state.pose_stores;
    return true;
}

bool model_runtime_il_pose(manager_model *model, const float values[6],
                           const mc_client_model_runtime_context *context,
                           mc_client_model_runtime_error *error) {
    mc_client_model_part *part = model->fields.a;
    error->state.part = part;
    creeper_at(error, "b", 8);
    if (!model_runtime_fp(error))
        return false;
    float value = at_fdiv(values[3], 57.295776f);
    creeper_at(error, "b", 9);
    if (!store(part, value, true, error))
        return false;
    part = model->fields.a;
    error->state.part = part;
    creeper_at(error, "b", 20);
    if (!model_runtime_fp(error))
        return false;
    value = at_fdiv(values[4], 57.295776f);
    creeper_at(error, "b", 21);
    if (!store(part, value, false, error))
        return false;
    mc_client_model_part **const slots[4] = {&model->fields.d, &model->fields.e, &model->fields.f,
                                             &model->fields.g};
    static const int32_t arithmetic_pcs[4] = {31, 50, 72, 94};
    static const int32_t math_pcs[4] = {32, 54, 76, 95};
    static const int32_t store_pcs[4] = {40, 62, 84, 103};
    for (size_t i = 0; i < 4; ++i) {
        part = *slots[i];
        error->state.part = part;
        creeper_at(error, "b", arithmetic_pcs[i]);
        if (!model_runtime_fp(error))
            return false;
        float angle = at_fmul(values[0], .6662f);
        if (i == 1 || i == 2)
            angle = at_fadd(angle, 3.1415927f);
        creeper_at(error, "b", math_pcs[i]);
        float cosine;
        if (!model_runtime_math_value(context, MODEL_RUNTIME_COSINE, angle, &cosine, error))
            return false;
        value = at_fmul(at_fmul(cosine, 1.4f), values[1]);
        creeper_at(error, "b", store_pcs[i]);
        if (!store(part, value, false, error))
            return false;
    }
    creeper_at(error, "b", 106);
    return true;
}

bool model_runtime_il_render(mc_client_managed_renderer *owner, manager_model *model,
                             const float values[6], const mc_client_model_runtime_context *context,
                             mc_client_model_runtime_error *error) {
    creeper_at(error, "a", 10);
    if (!model_runtime_call_push(error) || !model_runtime_pose_body(model, values, context, error))
        return false;
    model_runtime_call_pop(error);
    /* IL.b is a real allocated part, but this source body never draws it. */
    mc_client_model_part **const slots[6] = {&model->fields.a, &model->fields.c, &model->fields.d,
                                             &model->fields.e, &model->fields.f, &model->fields.g};
    static const int32_t pcs[6] = {19, 28, 37, 46, 55, 64};
    for (size_t i = 0; i < 6; ++i) {
        mc_client_model_part *part = *slots[i];
        creeper_at(error, "a", pcs[i]);
        if (!model_runtime_draw_part(owner, part, values[5], context, error))
            return false;
    }
    creeper_at(error, "a", 67);
    return true;
}
