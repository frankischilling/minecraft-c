#include "internal.h"

#include <string.h>

void mc_client_model_runtime_error_init(mc_client_model_runtime_error *error) {
    if (error == NULL)
        return;
    memset(error, 0, sizeof(*error));
    error->caller.pc = error->parent.pc = error->site.pc = error->child.pc = -1;
    error->state.site.pc = error->state.event_site.pc = error->state.pose_site.pc =
        error->state.render_site.pc = -1;
    error->math.source_pc = -1;
}

void model_runtime_at(mc_client_model_runtime_error *error, const char *cl, const char *method,
                      const char *descriptor, int32_t pc) {
    error->site = (mc_client_graphics_site){cl, method, descriptor, pc};
    error->state.site = error->site;
    if (strcmp(descriptor, "(Lls;FFF)V") == 0)
        error->state.event_site = error->site;
    else if (strcmp(method, "b") == 0)
        error->state.pose_site = error->site;
    else
        error->state.render_site = error->site;
}

bool model_runtime_fail(mc_client_model_runtime_error *error, mc_client_model_runtime_status status,
                        const char *operation) {
    if (error->status == MC_CLIENT_MODEL_RUNTIME_OK) {
        error->status = status;
        error->operation = operation;
        error->native_policy = !error->source_exception;
    }
    return false;
}

bool model_runtime_null(mc_client_model_runtime_error *error, const char *operation) {
    if (error->status == MC_CLIENT_MODEL_RUNTIME_OK) {
        error->source_exception = true;
        error->throwable = MC_CLIENT_MODEL_DRAW_SOURCE_EXCEPTION;
        error->exception_class = "java/lang/NullPointerException";
    }
    return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_SOURCE_EXCEPTION, operation);
}

bool model_runtime_admit(mc_client_managed_renderer *owner, manager_model *model,
                         mc_client_graphics_site caller, mc_client_model_runtime_error *error) {
    error->caller = error->site = error->state.site = caller;
    error->state.model = model;
    if (owner == NULL || owner->parent == NULL || owner->parent->classes == NULL)
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_ARGUMENT,
                                  "Model actual managed renderer owner");
    mc_client_render_manager *manager = owner->parent;
    mc_client_render_manager_classes *classes = manager->classes;
    if (classes->phase != MC_CLIENT_MANAGER_READY || classes->a != manager ||
        classes->models == NULL || classes->models->th != classes || classes->models->th_retired ||
        manager->holds == 0 || owner->holds == 0 || owner->phase != MC_CLIENT_MODEL_READY ||
        !owner->models_held)
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_STATE,
                                  "Model genuine READY renderer graph");
    bool canonical_renderer = false;
    if (manager->allocated_renderers > sizeof(manager->renderers) / sizeof(*manager->renderers))
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_STATE,
                                  "Model canonical renderer ledger extent");
    for (size_t i = 0; i < manager->allocated_renderers; ++i)
        if (manager->renderers[i] == owner) {
            canonical_renderer = true;
            break;
        }
    if (!canonical_renderer)
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_STATE,
                                  "Model canonical renderer ledger identity");
    if (model == NULL)
        return model_runtime_null(error, "Model null captured invocation receiver");
    bool owned = false;
    for (manager_model *at = owner->models; at != NULL; at = at->next)
        if (at == model) {
            owned = true;
            break;
        }
    if (!owned || model->phase != MC_CLIENT_MODEL_READY || model->source_class == NULL)
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_STATE,
                                  "Model actual constructed allocation provenance");
    error->state.runtime_class = model->source_class;
    return true;
}

bool model_runtime_quadruped(const manager_model *model) {
    return strcmp(model->source_class, "nn") == 0 || strcmp(model->source_class, "mw") == 0 ||
           strcmp(model->source_class, "ea") == 0;
}

bool model_runtime_fp(mc_client_model_runtime_error *error) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_STATE,
                              "Model strict floating controls");
}

bool model_runtime_math(const mc_client_model_runtime_context *context,
                        mc_client_model_runtime_error *error) {
    ++error->state.math_calls;
    error->selected_child = MC_CLIENT_MODEL_RUNTIME_CHILD_MATH;
    error->child = (mc_client_graphics_site){"in", "b", "(F)F", -1};
    if (context == NULL || context->math == NULL)
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_MATH_FAILED,
                                  "Model actual Math namespace");
    const mc_math_class_status status = mc_math_classes_ensure(context->math, &error->math);
    if (status != MC_MATH_CLASS_OK) {
        error->child = (mc_client_graphics_site){"in", "<clinit>", "()V", error->math.source_pc};
        return model_runtime_fail(
            error,
            status == MC_MATH_CLASS_LIMIT               ? MC_CLIENT_MODEL_RUNTIME_LIMIT
            : status == MC_MATH_CLASS_ALLOCATION_FAILED ? MC_CLIENT_MODEL_RUNTIME_ALLOCATION
            : status == MC_MATH_CLASS_BUSY              ? MC_CLIENT_MODEL_RUNTIME_BUSY
                                                        : MC_CLIENT_MODEL_RUNTIME_MATH_FAILED,
            "Model reached Math active use");
    }
    if (!model_runtime_fp(error))
        return false;
    error->selected_child = MC_CLIENT_MODEL_RUNTIME_CHILD_NONE;
    error->child = (mc_client_graphics_site){.pc = -1};
    return true;
}

static bool unsupported(manager_model *model, const char *method, const char *descriptor,
                        mc_client_model_runtime_error *error) {
    error->child = (mc_client_graphics_site){model->source_class, method, descriptor, -1};
    return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_UNIMPLEMENTED,
                              "Model concrete virtual family is not connected");
}

bool mc_client_model_event_entered(mc_client_managed_renderer *owner, manager_model *model,
                                   mc_entity *actor, float limb_swing, float limb_amount,
                                   float partial, const mc_client_model_runtime_context *context,
                                   mc_client_graphics_site caller,
                                   mc_client_model_runtime_error *error) {
    if (error == NULL)
        return false;
    mc_client_model_runtime_error_init(error);
    error->state.arguments[0] = limb_swing;
    error->state.arguments[1] = limb_amount;
    error->state.arguments[2] = partial;
    error->state.argument_count = 3;
    error->state.actor_identity = (uintptr_t)actor;
    if (!model_runtime_admit(owner, model, caller, error))
        return false;
    if (!model_runtime_quadruped(model))
        return unsupported(model, "a", "(Lls;FFF)V", error);
    (void)context;
    /* All selected classes inherit the actual empty KO entity event. Neither
     * the actor, source KO fields nor any native service is read here. */
    model_runtime_at(error, "ko", "a", "(Lls;FFF)V", 0);
    error->state.event_returned = error->state.returned = true;
    return true;
}

static void arguments(mc_client_model_runtime_error *error, const float values[6]) {
    memcpy(error->state.arguments, values, sizeof(error->state.arguments));
    error->state.argument_count = 6;
}

bool mc_client_model_pose_entered(mc_client_managed_renderer *owner, manager_model *model,
                                  float limb_swing, float limb_amount, float age, float yaw,
                                  float pitch, float scale,
                                  const mc_client_model_runtime_context *context,
                                  mc_client_graphics_site caller,
                                  mc_client_model_runtime_error *error) {
    if (error == NULL)
        return false;
    mc_client_model_runtime_error_init(error);
    const float values[6] = {limb_swing, limb_amount, age, yaw, pitch, scale};
    arguments(error, values);
    if (!model_runtime_admit(owner, model, caller, error))
        return false;
    if (!model_runtime_quadruped(model))
        return unsupported(model, "b", "(FFFFFF)V", error);
    if (!model_runtime_nn_pose(model, values, context, error))
        return false;
    error->state.returned = true;
    return true;
}

bool mc_client_model_render_entered(mc_client_managed_renderer *owner, manager_model *model,
                                    float limb_swing, float limb_amount, float age, float yaw,
                                    float pitch, float scale,
                                    const mc_client_model_runtime_context *context,
                                    mc_client_graphics_site caller,
                                    mc_client_model_runtime_error *error) {
    if (error == NULL)
        return false;
    mc_client_model_runtime_error_init(error);
    const float values[6] = {limb_swing, limb_amount, age, yaw, pitch, scale};
    arguments(error, values);
    if (!model_runtime_admit(owner, model, caller, error))
        return false;
    if (!model_runtime_quadruped(model))
        return unsupported(model, "a", "(FFFFFF)V", error);
    if (!model_runtime_nn_render(owner, model, values, context, error))
        return false;
    error->state.returned = true;
    return true;
}
