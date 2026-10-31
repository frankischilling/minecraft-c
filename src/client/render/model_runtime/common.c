#include "internal.h"
#include "mc/core/math/strict.h"

#include <string.h>

void mc_client_model_runtime_error_init(mc_client_model_runtime_error *error) {
    if (error == NULL)
        return;
    memset(error, 0, sizeof(*error));
    error->caller.pc = error->parent.pc = error->site.pc = error->child.pc = -1;
    error->state.site.pc = error->state.event_site.pc = error->state.pose_site.pc =
        error->state.render_site.pc = -1;
    error->state.nn_pose_site.pc = error->state.nn_render_site.pc = -1;
    for (size_t i = 0; i < sizeof(error->ancestors) / sizeof(*error->ancestors); ++i)
        error->ancestors[i].pc = -1;
    error->state.fh_pose_site.pc = error->state.fh_render_site.pc = error->state.ej_pose_site.pc =
        -1;
    error->math.source_pc = -1;
    mc_entity_render_error_init(&error->entity);
    error->state.array_index = error->state.array_length = -1;
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
    if (strcmp(cl, "nn") == 0) {
        if (strcmp(method, "b") == 0)
            error->state.nn_pose_site = error->site;
        else if (strcmp(method, "a") == 0 && strcmp(descriptor, "(FFFFFF)V") == 0)
            error->state.nn_render_site = error->site;
    }
    if (strcmp(cl, "fh") == 0) {
        if (strcmp(method, "b") == 0)
            error->state.fh_pose_site = error->site;
        else if (strcmp(method, "a") == 0)
            error->state.fh_render_site = error->site;
    } else if (strcmp(cl, "ej") == 0 && strcmp(method, "b") == 0)
        error->state.ej_pose_site = error->site;
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
    error->native_owner = owner;
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
           strcmp(model->source_class, "ea") == 0 || strcmp(model->source_class, "eh") == 0;
}

static bool supported(const manager_model *model) {
    return model_runtime_quadruped(model) || strcmp(model->source_class, "hh") == 0 ||
           strcmp(model->source_class, "td") == 0 || strcmp(model->source_class, "sc") == 0 ||
           strcmp(model->source_class, "no") == 0 || strcmp(model->source_class, "wn") == 0 ||
           strcmp(model->source_class, "il") == 0 || model_runtime_biped(model) ||
           strcmp(model->source_class, "o") == 0;
}

bool model_runtime_fp(mc_client_model_runtime_error *error) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_STATE,
                              "Model strict floating controls");
}

static bool math_enter(const mc_client_model_runtime_context *context, model_runtime_math_kind kind,
                       mc_client_model_runtime_error *error) {
    ++error->state.math_calls;
    const bool sine = kind == MODEL_RUNTIME_SINE;
    const bool square_root = kind == MODEL_RUNTIME_SQUARE_ROOT;
    if (sine)
        ++error->state.sine_calls;
    else if (square_root)
        ++error->state.square_root_calls;
    else
        ++error->state.cosine_calls;
    error->selected_child = MC_CLIENT_MODEL_RUNTIME_CHILD_MATH;
    error->child = (mc_client_graphics_site){"in",
                                             sine          ? "a"
                                             : square_root ? "c"
                                                           : "b",
                                             "(F)F", -1};
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

bool model_runtime_math(const mc_client_model_runtime_context *context,
                        mc_client_model_runtime_error *error) {
    return math_enter(context, MODEL_RUNTIME_COSINE, error);
}

bool model_runtime_math_value(const mc_client_model_runtime_context *context,
                              model_runtime_math_kind kind, float value, float *out,
                              mc_client_model_runtime_error *error) {
    if (kind == MODEL_RUNTIME_ABSOLUTE) {
        ++error->state.math_calls;
        ++error->state.absolute_calls;
        error->selected_child = MC_CLIENT_MODEL_RUNTIME_CHILD_MATH;
        error->child = (mc_client_graphics_site){"java/lang/Math", "abs", "(F)F", 0};
        if (!model_runtime_fp(error))
            return false;
        /* JDK17 Math.abs(F): fcmpg/ifgt, otherwise 0f-value. This preserves
         * the source signed-zero/NaN behavior without a host fabsf substitute. */
        *out = value <= 0.0f ? at_fsub(0.0f, value) : value;
        error->selected_child = MC_CLIENT_MODEL_RUNTIME_CHILD_NONE;
        error->child = (mc_client_graphics_site){.pc = -1};
        return true;
    }
    const bool sine = kind == MODEL_RUNTIME_SINE;
    if (!math_enter(context, kind, error))
        return false;
    *out = kind == MODEL_RUNTIME_SQUARE_ROOT ? at_narrow(mc_math_strict_sqrt(at_widen(value)))
           : sine ? mc_math_sin(mc_math_classes_table(context->math), value)
                  : mc_math_cos(mc_math_classes_table(context->math), value);
    return true;
}

static bool unsupported(manager_model *model, const char *method, const char *descriptor,
                        mc_client_model_runtime_error *error) {
    error->child = (mc_client_graphics_site){model->source_class, method, descriptor, -1};
    return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_UNIMPLEMENTED,
                              "Model concrete virtual family is not connected");
}

bool model_runtime_call_push(mc_client_model_runtime_error *error) {
    if (error->ancestor_count >= sizeof(error->ancestors) / sizeof(*error->ancestors))
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_LIMIT,
                                  "Model finite source frame bound");
    error->ancestors[error->ancestor_count++] = error->site;
    error->parent = error->site;
    return true;
}

void model_runtime_call_pop(mc_client_model_runtime_error *error) {
    if (error->ancestor_count == 0)
        return;
    error->ancestors[--error->ancestor_count] = (mc_client_graphics_site){.pc = -1};
    error->parent = error->ancestor_count == 0 ? (mc_client_graphics_site){.pc = -1}
                                               : error->ancestors[error->ancestor_count - 1u];
}

bool model_runtime_pose_body(manager_model *model, const float values[6],
                             const mc_client_model_runtime_context *context,
                             mc_client_model_runtime_error *error) {
    error->state.pose_returned = false;
    bool ok;
    if (strcmp(model->source_class, "hh") == 0)
        ok = model_runtime_hh_pose(model, values, context, error);
    else if (strcmp(model->source_class, "td") == 0)
        ok = model_runtime_td_pose(model, values, context, error);
    else if (strcmp(model->source_class, "sc") == 0)
        ok = model_runtime_sc_pose(model, values, context, error);
    else if (strcmp(model->source_class, "no") == 0)
        ok = model_runtime_no_pose(model, values, context, error);
    else if (strcmp(model->source_class, "wn") == 0)
        ok = model_runtime_wn_pose(model, values, context, error);
    else if (strcmp(model->source_class, "il") == 0)
        ok = model_runtime_il_pose(model, values, context, error);
    else if (strcmp(model->source_class, "o") == 0)
        ok = model_runtime_o_pose(model, values, context, error);
    else if (strcmp(model->source_class, "ej") == 0 || strcmp(model->source_class, "lc") == 0)
        ok = model_runtime_ej_pose(model, values, context, error);
    else if (strcmp(model->source_class, "fh") == 0)
        ok = model_runtime_fh_pose(model, values, context, error);
    else if (model_runtime_quadruped(model))
        ok = model_runtime_nn_pose(model, values, context, error);
    else
        return unsupported(model, "b", "(FFFFFF)V", error);
    if (ok)
        error->state.pose_returned = true;
    return ok;
}

static bool part_failed(mc_client_model_runtime_error *error, bool quadruped) {
    error->selected_child = MC_CLIENT_MODEL_RUNTIME_CHILD_PART;
    error->child = error->part.site;
    error->throwable = error->part.throwable;
    if (error->part.throwable == MC_CLIENT_MODEL_DRAW_SOURCE_ERROR &&
        error->part.exception_class != NULL) {
        error->exception_class = error->part.exception_class;
        model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_PART_FAILED,
                           "Model actual PS source Error");
        error->native_policy = false;
        return false;
    }
    /* Only the producer's named classification may enter GV's Exception catch.
     * The complete producer, including an unfinished list, stays unchanged. */
    if (error->part.throwable == MC_CLIENT_MODEL_DRAW_SOURCE_EXCEPTION &&
        error->part.exception_class != NULL) {
        error->source_exception = true;
        error->exception_class = error->part.exception_class;
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_SOURCE_EXCEPTION,
                                  quadruped ? "Quadruped actual PS source Exception"
                                            : "Model actual PS source Exception");
    }
    if (error->part.throwable != MC_CLIENT_MODEL_DRAW_NO_THROWABLE) {
        error->throwable = MC_CLIENT_MODEL_DRAW_UNCLASSIFIED_THROWABLE;
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_UNIMPLEMENTED,
                                  quadruped ? "Quadruped unclassified PS Throwable boundary"
                                            : "Model unclassified PS Throwable boundary");
    }
    return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_PART_FAILED,
                              quadruped ? "Quadruped actual PS draw child"
                                        : "Model actual PS draw child");
}

bool model_runtime_draw_part(mc_client_managed_renderer *owner, mc_client_model_part *part,
                             float scale, const mc_client_model_runtime_context *context,
                             mc_client_model_runtime_error *error) {
    const bool quadruped = strcmp(error->site.class_name, "nn") == 0;
    error->state.part = part;
    ++error->state.part_calls;
    ++error->state.part_a_calls;
    if (part == NULL)
        return model_runtime_null(error, quadruped ? "Quadruped null PS invocation receiver"
                                                   : "Model null PS invocation receiver");
    if (context == NULL || context->entities != owner->parent->classes->models->entities)
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_STATE,
                                  quadruped ? "Quadruped actual PS Entity namespace"
                                            : "Model actual PS Entity namespace");
    const bool ok = mc_client_model_part_draw(part, context->graphics, context->entities,
                                              context->display, scale, &error->part);
    error->state.faces += error->part.faces;
    error->state.vertices += error->part.vertices;
    if (error->part.compiled)
        ++error->state.compiled_parts;
    if (!ok)
        return part_failed(error, quadruped);
    ++error->state.part_returns;
    ++error->state.part_a_returns;
    return true;
}

static bool other_part(mc_client_managed_renderer *owner, mc_client_model_part *part, float scale,
                       bool attachment, const mc_client_model_runtime_context *context,
                       mc_client_model_runtime_error *error) {
    error->state.part = part;
    ++error->state.part_calls;
    if (attachment)
        ++error->state.part_c_calls;
    else
        ++error->state.part_b_calls;
    if (part == NULL)
        return model_runtime_null(error, "Model null PS invocation receiver");
    /* A hidden/disabled PS and a compiled attachment need no new Entity/NW
     * service. Caller still holds the same genuine complete renderer graph. */
    if (!part->i && part->h && !part->n && context != NULL && context->entities != NULL &&
        context->entities != owner->parent->classes->models->entities)
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_STATE,
                                  "Model actual PS Entity namespace");
    mc_client_render_engine_classes *graphics = context ? context->graphics : NULL;
    mc_entity_classes *entities = context ? context->entities : NULL;
    mc_client_display *display = context ? context->display : NULL;
    const bool ok = attachment ? mc_client_model_part_attach(part, graphics, entities, display,
                                                             scale, &error->part)
                               : mc_client_model_part_draw_rotated(part, graphics, entities,
                                                                   display, scale, &error->part);
    error->state.faces += error->part.faces;
    error->state.vertices += error->part.vertices;
    if (error->part.compiled)
        ++error->state.compiled_parts;
    if (!ok)
        return part_failed(error, false);
    ++error->state.part_returns;
    if (attachment)
        ++error->state.part_c_returns;
    else
        ++error->state.part_b_returns;
    return true;
}

bool model_runtime_draw_part_rotated(mc_client_managed_renderer *owner, mc_client_model_part *part,
                                     float scale, const mc_client_model_runtime_context *context,
                                     mc_client_model_runtime_error *error) {
    return other_part(owner, part, scale, false, context, error);
}

bool model_runtime_attach_part(mc_client_managed_renderer *owner, mc_client_model_part *part,
                               float scale, const mc_client_model_runtime_context *context,
                               mc_client_model_runtime_error *error) {
    return other_part(owner, part, scale, true, context, error);
}

bool model_runtime_entity_finish(const mc_client_model_runtime_context *context, bool ok,
                                 mc_client_model_runtime_error *error) {
    /* This is the concrete Root-owned native bridge, never a callback. Even
     * failed/scalar children reach it before their owning error may be reused. */
    const bool adopted =
        mc_client_model_runtime_entity_adopt(context, &error->entity, &error->capture_failure);
    ++error->state.entity_leaf_calls;
    error->state.math_calls += error->entity.sine_calls;
    error->state.sine_calls += error->entity.sine_calls;
    if (ok && adopted) {
        error->selected_child = MC_CLIENT_MODEL_RUNTIME_CHILD_NONE;
        error->child = (mc_client_graphics_site){.pc = -1};
        return true;
    }
    error->selected_child = MC_CLIENT_MODEL_RUNTIME_CHILD_ENTITY;
    const mc_entity_render_site site = error->entity.site;
    error->child =
        (mc_client_graphics_site){site.class_name, site.method, site.descriptor, site.pc};
    if (!adopted) {
        /* Earlier Source causes/owning slots remain intact in entity. */
        error->source_exception = false;
        error->exception_class = NULL;
        error->throwable = MC_CLIENT_MODEL_DRAW_NO_THROWABLE;
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_CAPTURE_FAILED,
                                  error->capture_failure.operation);
    }
    error->source_exception = error->entity.source_exception;
    error->exception_class = error->entity.exception_class;
    switch (error->entity.throwable) {
        case MC_ENTITY_RENDER_SOURCE_EXCEPTION:
            error->throwable = MC_CLIENT_MODEL_DRAW_SOURCE_EXCEPTION;
            break;
        case MC_ENTITY_RENDER_SOURCE_ERROR:
            error->throwable = MC_CLIENT_MODEL_DRAW_SOURCE_ERROR;
            break;
        case MC_ENTITY_RENDER_UNCLASSIFIED_THROWABLE:
            error->throwable = MC_CLIENT_MODEL_DRAW_UNCLASSIFIED_THROWABLE;
            break;
        default:
            error->throwable = MC_CLIENT_MODEL_DRAW_NO_THROWABLE;
            break;
    }
    model_runtime_fail(error,
                       error->source_exception ? MC_CLIENT_MODEL_RUNTIME_SOURCE_EXCEPTION
                                               : MC_CLIENT_MODEL_RUNTIME_ENTITY_FAILED,
                       error->entity.operation);
    error->native_policy = error->entity.native_policy;
    return false;
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
    if (!supported(model))
        return unsupported(model, "a", "(Lls;FFF)V", error);
    if (strcmp(model->source_class, "o") == 0) {
        const float values[3] = {limb_swing, limb_amount, partial};
        if (!model_runtime_o_event(model, actor, values, context, error))
            return false;
        error->state.event_returned = error->state.returned = true;
        return true;
    }
    /* All other selected classes inherit the actual empty KO entity event. Neither
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
    if (!model_runtime_pose_body(model, values, context, error))
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
    if (!supported(model))
        return unsupported(model, "a", "(FFFFFF)V", error);
    const bool ok = strcmp(model->source_class, "hh") == 0
                        ? model_runtime_hh_render(owner, model, values, context, error)
                    : strcmp(model->source_class, "td") == 0
                        ? model_runtime_td_render(owner, model, values, context, error)
                    : strcmp(model->source_class, "sc") == 0
                        ? model_runtime_sc_render(owner, model, values, context, error)
                    : strcmp(model->source_class, "no") == 0
                        ? model_runtime_no_render(owner, model, values, context, error)
                    : strcmp(model->source_class, "wn") == 0
                        ? model_runtime_wn_render(owner, model, values, context, error)
                    : strcmp(model->source_class, "il") == 0
                        ? model_runtime_il_render(owner, model, values, context, error)
                    : strcmp(model->source_class, "o") == 0
                        ? model_runtime_o_render(owner, model, values, context, error)
                    : model_runtime_biped(model)
                        ? model_runtime_fh_render(owner, model, values, context, error)
                        : model_runtime_nn_render(owner, model, values, context, error);
    if (!ok)
        return false;
    error->state.returned = true;
    return true;
}
