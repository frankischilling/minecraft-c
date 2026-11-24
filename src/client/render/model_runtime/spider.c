#include "internal.h"

static void spider_at(mc_client_model_runtime_error *error, const char *method, int32_t pc) {
    model_runtime_at(error, "sc", method, "(FFFFFF)V", pc);
}

static bool store(mc_client_model_part *part, float value, unsigned int axis,
                  mc_client_model_runtime_error *error) {
    error->state.part = part;
    if (part == NULL)
        return model_runtime_null(error, "Spider null captured pose part receiver");
    float *field = axis == 0 ? &part->d : axis == 1 ? &part->e : &part->f;
    *field = value;
    ++error->state.pose_stores;
    return true;
}

bool model_runtime_sc_pose(manager_model *model, const float values[6],
                           const mc_client_model_runtime_context *context,
                           mc_client_model_runtime_error *error) {
    mc_client_model_part *part = model->fields.a;
    error->state.part = part;
    spider_at(error, "b", 8);
    if (!model_runtime_fp(error))
        return false;
    float value = at_fdiv(values[3], 57.295776f);
    spider_at(error, "b", 9);
    if (!store(part, value, 1, error))
        return false;
    part = model->fields.a;
    error->state.part = part;
    spider_at(error, "b", 20);
    if (!model_runtime_fp(error))
        return false;
    value = at_fdiv(values[4], 57.295776f);
    spider_at(error, "b", 21);
    if (!store(part, value, 0, error))
        return false;

    /* These are native addresses of the eleven individual source fields,
     * never a source array or a copied set of part receivers. */
    mc_client_model_part **const legs[8] = {&model->fields.d, &model->fields.e, &model->fields.f,
                                            &model->fields.g, &model->fields.h, &model->fields.i,
                                            &model->extra_j,  &model->extra_k};
    static const int32_t roll_pcs[8] = {35, 44, 57, 69, 82, 94, 104, 113};
    const float p = .7853982f;
    for (size_t i = 0; i < 8; ++i) {
        part = *legs[i];
        value = (i & 1u) == 0 ? at_fneg(p) : p;
        if (i >= 2 && i <= 5)
            value = at_fmul(value, .74f);
        spider_at(error, "b", roll_pcs[i]);
        if (!store(part, value, 2, error))
            return false;
    }
    static const int32_t yaw_pcs[8] = {135, 150, 164, 179, 194, 208, 223, 237};
    const float q = .3926991f, negative_zero = -0.0f;
    for (size_t i = 0; i < 8; ++i) {
        part = *legs[i];
        const bool negate = i == 1 || i == 3 || i == 4 || i == 6;
        value = at_fmul(negate ? at_fneg(q) : q, i < 2 || i > 5 ? 2.0f : 1.0f);
        value = (i & 1u) == 0 ? at_fadd(value, negative_zero) : at_fsub(value, negative_zero);
        spider_at(error, "b", yaw_pcs[i]);
        if (!store(part, value, 1, error))
            return false;
    }
    static const float phases[4] = {0.0f, 3.1415927f, 1.5707964f, 4.712389f};
    static const int32_t cosine_pcs[4] = {248, 268, 288, 308};
    static const int32_t sine_pcs[4] = {325, 345, 365, 385};
    static const int32_t abs_pcs[4] = {331, 351, 371, 391};
    float yaw[4], roll[4];
    for (size_t i = 0; i < 4; ++i) {
        const float angle = at_fadd(at_fmul(at_fmul(values[0], .6662f), 2.0f), phases[i]);
        spider_at(error, "b", cosine_pcs[i]);
        float cosine;
        if (!model_runtime_math_value(context, MODEL_RUNTIME_COSINE, angle, &cosine, error))
            return false;
        yaw[i] = at_fmul(at_fneg(at_fmul(cosine, .4f)), values[1]);
    }
    for (size_t i = 0; i < 4; ++i) {
        const float angle = at_fadd(at_fmul(values[0], .6662f), phases[i]);
        spider_at(error, "b", sine_pcs[i]);
        float sine, absolute;
        if (!model_runtime_math_value(context, MODEL_RUNTIME_SINE, angle, &sine, error))
            return false;
        value = at_fmul(sine, .4f);
        spider_at(error, "b", abs_pcs[i]);
        if (!model_runtime_math_value(context, MODEL_RUNTIME_ABSOLUTE, value, &absolute, error))
            return false;
        roll[i] = at_fmul(absolute, values[1]);
    }
    static const int32_t read_pcs[2][8] = {{403, 417, 432, 446, 461, 475, 490, 504},
                                           {519, 533, 548, 562, 577, 591, 606, 620}};
    static const int32_t write_pcs[2][8] = {{409, 424, 438, 453, 467, 482, 496, 511},
                                            {525, 540, 554, 569, 583, 598, 612, 627}};
    for (size_t axis = 0; axis < 2; ++axis)
        for (size_t i = 0; i < 8; ++i) {
            part = *legs[i];
            error->state.part = part;
            spider_at(error, "b", read_pcs[axis][i]);
            if (part == NULL)
                return model_runtime_null(error, "Spider null animated angle read receiver");
            const float old = axis == 0 ? part->e : part->f;
            float delta = axis == 0 ? yaw[i / 2u] : roll[i / 2u];
            if ((i & 1u) != 0)
                delta = at_fneg(delta);
            value = at_fadd(old, delta);
            spider_at(error, "b", write_pcs[axis][i]);
            if (!store(part, value, axis == 0 ? 1u : 2u, error))
                return false;
        }
    spider_at(error, "b", 630);
    return true;
}

bool model_runtime_sc_render(mc_client_managed_renderer *owner, manager_model *model,
                             const float values[6], const mc_client_model_runtime_context *context,
                             mc_client_model_runtime_error *error) {
    spider_at(error, "a", 10);
    if (!model_runtime_call_push(error) || !model_runtime_pose_body(model, values, context, error))
        return false;
    model_runtime_call_pop(error);
    mc_client_model_part **const slots[11] = {&model->fields.a, &model->fields.b, &model->fields.c,
                                              &model->fields.d, &model->fields.e, &model->fields.f,
                                              &model->fields.g, &model->fields.h, &model->fields.i,
                                              &model->extra_j,  &model->extra_k};
    static const int32_t pcs[11] = {19, 28, 37, 46, 55, 64, 73, 82, 91, 100, 109};
    for (size_t i = 0; i < 11; ++i) {
        mc_client_model_part *part = *slots[i];
        spider_at(error, "a", pcs[i]);
        if (!model_runtime_draw_part(owner, part, values[5], context, error))
            return false;
    }
    spider_at(error, "a", 112);
    return true;
}
