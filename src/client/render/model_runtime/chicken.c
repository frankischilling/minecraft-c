#include "internal.h"

#include <string.h>

static void chicken_at(mc_client_model_runtime_error *error, const char *method, int32_t pc) {
    model_runtime_at(error, "td", method, "(FFFFFF)V", pc);
}

static bool store(mc_client_model_part *part, float value, unsigned int axis,
                  mc_client_model_runtime_error *error) {
    error->state.part = part;
    if (part == NULL)
        return model_runtime_null(error, "Chicken null captured pose part receiver");
    float *field = axis == 0 ? &part->d : axis == 1 ? &part->e : &part->f;
    memcpy(field, &value, sizeof(value));
    ++error->state.pose_stores;
    return true;
}

bool model_runtime_td_pose(manager_model *model, const float values[6],
                           const mc_client_model_runtime_context *context,
                           mc_client_model_runtime_error *error) {
    mc_client_model_part *part = model->fields.a;
    error->state.part = part;
    chicken_at(error, "b", 8);
    if (!model_runtime_fp(error))
        return false;
    const float pitch = at_fneg(at_fdiv(values[4], 57.295776f));
    chicken_at(error, "b", 10);
    if (!store(part, pitch, 0, error))
        return false;
    part = model->fields.a;
    error->state.part = part;
    chicken_at(error, "b", 21);
    if (!model_runtime_fp(error))
        return false;
    const float yaw = at_fdiv(values[3], 57.295776f);
    chicken_at(error, "b", 22);
    if (!store(part, yaw, 1, error))
        return false;

    mc_client_model_part **const copies[4] = {&model->fields.g, &model->fields.g, &model->fields.h,
                                              &model->fields.h};
    static const int32_t read_pcs[4] = {33, 47, 61, 75};
    static const int32_t store_pcs[4] = {36, 50, 64, 78};
    for (size_t i = 0; i < 4; ++i) {
        part = *copies[i];
        mc_client_model_part *head = model->fields.a;
        error->state.part = head;
        chicken_at(error, "b", read_pcs[i]);
        if (head == NULL)
            return model_runtime_null(error, "Chicken null head angle receiver");
        const unsigned int axis = (unsigned int)(i & 1u);
        float value;
        memcpy(&value, axis == 0 ? &head->d : &head->e, sizeof(value));
        chicken_at(error, "b", store_pcs[i]);
        if (!store(part, value, axis, error))
            return false;
    }
    part = model->fields.b;
    chicken_at(error, "b", 87);
    if (!store(part, 1.5707964f, 0, error))
        return false;

    mc_client_model_part **const legs[2] = {&model->fields.c, &model->fields.d};
    static const int32_t arithmetic_pcs[2] = {97, 116};
    static const int32_t math_pcs[2] = {98, 120};
    static const int32_t leg_store_pcs[2] = {106, 128};
    for (size_t i = 0; i < 2; ++i) {
        part = *legs[i];
        error->state.part = part;
        chicken_at(error, "b", arithmetic_pcs[i]);
        if (!model_runtime_fp(error))
            return false;
        float angle = at_fmul(values[0], .6662f);
        if (i == 1)
            angle = at_fadd(angle, 3.1415927f);
        chicken_at(error, "b", math_pcs[i]);
        if (!model_runtime_math(context, error))
            return false;
        const float cosine = mc_math_cos(mc_math_classes_table(context->math), angle);
        const float value = at_fmul(at_fmul(cosine, 1.4f), values[1]);
        chicken_at(error, "b", leg_store_pcs[i]);
        if (!store(part, value, 0, error))
            return false;
    }
    part = model->fields.e;
    chicken_at(error, "b", 136);
    if (!store(part, values[2], 2, error))
        return false;
    part = model->fields.f;
    const float opposite = at_fneg(values[2]);
    chicken_at(error, "b", 145);
    if (!store(part, opposite, 2, error))
        return false;
    chicken_at(error, "b", 148);
    return true;
}

bool model_runtime_td_render(mc_client_managed_renderer *owner, manager_model *model,
                             const float values[6], const mc_client_model_runtime_context *context,
                             mc_client_model_runtime_error *error) {
    chicken_at(error, "a", 10);
    if (!model_runtime_call_push(error) || !model_runtime_pose_body(model, values, context, error))
        return false;
    model_runtime_call_pop(error);
    /* A native fixed slot-address loop retains the eight distinct source
     * field reloads; it does not create a source PS array. */
    mc_client_model_part **const slots[8] = {&model->fields.a, &model->fields.g, &model->fields.h,
                                             &model->fields.b, &model->fields.c, &model->fields.d,
                                             &model->fields.e, &model->fields.f};
    static const int32_t pcs[8] = {19, 28, 37, 46, 55, 64, 73, 82};
    for (size_t i = 0; i < 8; ++i) {
        mc_client_model_part *part = *slots[i];
        chicken_at(error, "a", pcs[i]);
        if (!model_runtime_draw_part(owner, part, values[5], context, error))
            return false;
    }
    chicken_at(error, "a", 85);
    return true;
}
