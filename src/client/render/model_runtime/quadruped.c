#include "internal.h"

static void pose_at(mc_client_model_runtime_error *error, int32_t pc) {
    model_runtime_at(error, "nn", "b", "(FFFFFF)V", pc);
}

static void render_at(mc_client_model_runtime_error *error, int32_t pc) {
    model_runtime_at(error, "nn", "a", "(FFFFFF)V", pc);
}

static bool store_angle(mc_client_model_part *part, float value, bool yaw,
                        mc_client_model_runtime_error *error) {
    error->state.part = part;
    if (part == NULL)
        return model_runtime_null(error, "Quadruped null captured pose part receiver");
    if (yaw)
        part->e = value;
    else
        part->d = value;
    ++error->state.pose_stores;
    return true;
}

bool model_runtime_nn_pose(manager_model *model, const float values[6],
                           const mc_client_model_runtime_context *context,
                           mc_client_model_runtime_error *error) {
    mc_client_model_part *part = model->fields.d;
    error->state.part = part;
    pose_at(error, 8);
    if (!model_runtime_fp(error))
        return false;
    const float pitch = at_fdiv(values[4], 57.295776f);
    pose_at(error, 9);
    if (!store_angle(part, pitch, false, error))
        return false;
    part = model->fields.d; /* Actual fresh head receiver at13. */
    pose_at(error, 20);
    if (!model_runtime_fp(error))
        return false;
    const float yaw = at_fdiv(values[3], 57.295776f);
    pose_at(error, 21);
    if (!store_angle(part, yaw, true, error))
        return false;
    part = model->fields.e;
    pose_at(error, 30);
    if (!store_angle(part, 1.5707964f, false, error))
        return false;

    mc_client_model_part **const slots[4] = {&model->fields.f, &model->fields.g, &model->fields.h,
                                             &model->fields.i};
    static const int32_t arithmetic_pcs[4] = {40, 59, 81, 103};
    static const int32_t math_pcs[4] = {41, 63, 85, 104};
    static const int32_t store_pcs[4] = {49, 71, 93, 112};
    for (size_t i = 0; i < 4; ++i) {
        /* The source captures this PS before the argument arithmetic/IN call.
         * NULL fails at the later putfield, after reached Math side effects. */
        part = *slots[i];
        error->state.part = part;
        pose_at(error, arithmetic_pcs[i]);
        if (!model_runtime_fp(error))
            return false;
        float angle = at_fmul(values[0], .6662f);
        if (i == 1 || i == 2)
            angle = at_fadd(angle, 3.1415927f);
        pose_at(error, math_pcs[i]);
        if (!model_runtime_math(context, error))
            return false;
        const float cosine = mc_math_cos(mc_math_classes_table(context->math), angle);
        const float value = at_fmul(at_fmul(cosine, 1.4f), values[1]);
        pose_at(error, store_pcs[i]);
        if (!store_angle(part, value, false, error))
            return false;
    }
    pose_at(error, 115);
    error->state.nn_pose_returned = true;
    return true;
}

bool model_runtime_nn_render(mc_client_managed_renderer *owner, manager_model *model,
                             const float values[6], const mc_client_model_runtime_context *context,
                             mc_client_model_runtime_error *error) {
    render_at(error, 10);
    if (!model_runtime_call_push(error) || !model_runtime_pose_body(model, values, context, error))
        return false;
    model_runtime_call_pop(error);
    mc_client_model_part **const slots[6] = {&model->fields.d, &model->fields.e, &model->fields.f,
                                             &model->fields.g, &model->fields.h, &model->fields.i};
    static const int32_t pcs[6] = {19, 28, 37, 46, 55, 64};
    for (size_t i = 0; i < 6; ++i) {
        mc_client_model_part *part = *slots[i];
        render_at(error, pcs[i]);
        if (!model_runtime_draw_part(owner, part, values[5], context, error))
            return false;
    }
    render_at(error, 67);
    error->state.nn_render_returned = true;
    return true;
}
