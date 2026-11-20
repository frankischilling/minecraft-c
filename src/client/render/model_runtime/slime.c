#include "internal.h"

static void slime_at(mc_client_model_runtime_error *error, const char *method, int32_t pc) {
    model_runtime_at(error, "no", method, "(FFFFFF)V", pc);
}

bool model_runtime_no_pose(manager_model *model, const float values[6],
                           const mc_client_model_runtime_context *context,
                           mc_client_model_runtime_error *error) {
    (void)model;
    (void)values;
    (void)context;
    /* The actual NO override is one return; persistent PS pose is untouched. */
    slime_at(error, "b", 0);
    return true;
}

bool model_runtime_no_render(mc_client_managed_renderer *owner, manager_model *model,
                             const float values[6], const mc_client_model_runtime_context *context,
                             mc_client_model_runtime_error *error) {
    slime_at(error, "a", 10);
    if (!model_runtime_call_push(error) || !model_runtime_pose_body(model, values, context, error))
        return false;
    model_runtime_call_pop(error);
    mc_client_model_part *part = model->fields.a;
    slime_at(error, "a", 19);
    if (!model_runtime_draw_part(owner, part, values[5], context, error))
        return false;
    /* This is the one source guard. A null b suppresses c/d even if present;
     * a nonnull b is freshly reloaded for the invocation at35. */
    slime_at(error, "a", 26);
    if (model->fields.b != NULL) {
        part = model->fields.b;
        slime_at(error, "a", 35);
        if (!model_runtime_draw_part(owner, part, values[5], context, error))
            return false;
        part = model->fields.c;
        slime_at(error, "a", 44);
        if (!model_runtime_draw_part(owner, part, values[5], context, error))
            return false;
        part = model->fields.d;
        slime_at(error, "a", 53);
        if (!model_runtime_draw_part(owner, part, values[5], context, error))
            return false;
    }
    slime_at(error, "a", 56);
    return true;
}
