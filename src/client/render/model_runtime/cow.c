#include "internal.h"

#include <string.h>

static void cow_at(mc_client_model_runtime_error *error, const char *method, int32_t pc) {
    model_runtime_at(error, "hh", method, "(FFFFFF)V", pc);
}

bool model_runtime_hh_pose(manager_model *model, const float values[6],
                           const mc_client_model_runtime_context *context,
                           mc_client_model_runtime_error *error) {
    cow_at(error, "b", 10);
    if (!model_runtime_call_push(error) || !model_runtime_nn_pose(model, values, context, error))
        return false;
    model_runtime_call_pop(error);

    mc_client_model_part **const targets[4] = {&model->fields.b, &model->fields.b, &model->fields.c,
                                               &model->fields.c};
    static const int32_t read_pcs[4] = {21, 35, 49, 63};
    static const int32_t store_pcs[4] = {24, 38, 52, 66};
    for (size_t i = 0; i < 4; ++i) {
        /* Each target is captured before the fresh source head receiver.
         * A null target therefore fails only after that head angle read. */
        mc_client_model_part *target = *targets[i];
        mc_client_model_part *head = model->fields.d;
        error->state.part = head;
        cow_at(error, "b", read_pcs[i]);
        if (head == NULL)
            return model_runtime_null(error, "Cow null head angle receiver");
        float value;
        const bool yaw = (i & 1u) == 0;
        memcpy(&value, yaw ? &head->e : &head->d, sizeof(value));
        error->state.part = target;
        cow_at(error, "b", store_pcs[i]);
        if (target == NULL)
            return model_runtime_null(error, "Cow null captured horn angle receiver");
        memcpy(yaw ? &target->e : &target->d, &value, sizeof(value));
        ++error->state.pose_stores;
    }
    cow_at(error, "b", 69);
    return true;
}

bool model_runtime_hh_render(mc_client_managed_renderer *owner, manager_model *model,
                             const float values[6], const mc_client_model_runtime_context *context,
                             mc_client_model_runtime_error *error) {
    cow_at(error, "a", 10);
    if (!model_runtime_call_push(error) ||
        !model_runtime_nn_render(owner, model, values, context, error))
        return false;
    model_runtime_call_pop(error);
    /* HH's special NN.a call includes the one virtual HH.b pose. Only after
     * its six inherited parts return do the horns and udder draw here. */
    mc_client_model_part **const slots[3] = {&model->fields.b, &model->fields.c, &model->fields.a};
    static const int32_t pcs[3] = {19, 28, 37};
    for (size_t i = 0; i < 3; ++i) {
        mc_client_model_part *part = *slots[i];
        cow_at(error, "a", pcs[i]);
        if (!model_runtime_draw_part(owner, part, values[5], context, error))
            return false;
    }
    cow_at(error, "a", 40);
    return true;
}
