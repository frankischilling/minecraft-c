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
    error->state.pose_returned = true;
    return true;
}

static bool part_failed(mc_client_model_runtime_error *error) {
    error->selected_child = MC_CLIENT_MODEL_RUNTIME_CHILD_PART;
    error->child = error->part.site;
    error->throwable = error->part.throwable;
    /* This is the producer's explicit classification, not an inference from
     * native_policy. An unknown Throwable cannot enter GV's Exception catch. */
    if (error->part.throwable == MC_CLIENT_MODEL_DRAW_SOURCE_EXCEPTION &&
        error->part.exception_class != NULL) {
        error->source_exception = true;
        error->exception_class = error->part.exception_class;
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_SOURCE_EXCEPTION,
                                  "Quadruped actual PS source Exception");
    }
    if (error->part.throwable != MC_CLIENT_MODEL_DRAW_NO_THROWABLE) {
        error->throwable = MC_CLIENT_MODEL_DRAW_UNCLASSIFIED_THROWABLE;
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_UNIMPLEMENTED,
                                  "Quadruped unclassified PS Throwable boundary");
    }
    return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_PART_FAILED,
                              "Quadruped actual PS draw child");
}

bool model_runtime_nn_render(mc_client_managed_renderer *owner, manager_model *model,
                             const float values[6], const mc_client_model_runtime_context *context,
                             mc_client_model_runtime_error *error) {
    render_at(error, 10);
    error->parent = error->site;
    if (!model_runtime_nn_pose(model, values, context, error))
        return false;
    error->parent = (mc_client_graphics_site){.pc = -1};
    mc_client_model_part **const slots[6] = {&model->fields.d, &model->fields.e, &model->fields.f,
                                             &model->fields.g, &model->fields.h, &model->fields.i};
    static const int32_t pcs[6] = {19, 28, 37, 46, 55, 64};
    for (size_t i = 0; i < 6; ++i) {
        mc_client_model_part *part = *slots[i];
        error->state.part = part;
        render_at(error, pcs[i]);
        ++error->state.part_calls;
        if (part == NULL)
            return model_runtime_null(error, "Quadruped null PS invocation receiver");
        if (context == NULL || context->entities != owner->parent->classes->models->entities)
            return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_STATE,
                                      "Quadruped actual PS Entity namespace");
        const bool ok = mc_client_model_part_draw(part, context->graphics, context->entities,
                                                  context->display, values[5], &error->part);
        error->state.faces += error->part.faces;
        error->state.vertices += error->part.vertices;
        if (error->part.compiled)
            ++error->state.compiled_parts;
        if (!ok)
            return part_failed(error);
        ++error->state.part_returns;
    }
    render_at(error, 67);
    return true;
}
