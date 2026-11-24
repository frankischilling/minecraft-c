#include "internal.h"

static void squid_at(mc_client_model_runtime_error *error, const char *method, int32_t pc) {
    model_runtime_at(error, "wn", method, "(FFFFFF)V", pc);
}

static bool array_read(manager_model *model, bool pose, bool element, int32_t index,
                       manager_part_array **out, int32_t *out_length,
                       mc_client_model_runtime_error *error) {
    manager_part_array *array = model->array_b; /* Actual fresh source b. */
    error->state.part_array = array;
    error->state.array_index = index;
    error->state.array_length = -1;
    error->state.array_role =
        pose      ? element ? MC_CLIENT_MODEL_ARRAY_POSE_ELEMENT : MC_CLIENT_MODEL_ARRAY_POSE_LENGTH
        : element ? MC_CLIENT_MODEL_ARRAY_RENDER_ELEMENT
                  : MC_CLIENT_MODEL_ARRAY_RENDER_LENGTH;
    squid_at(error, pose ? "b" : "a", pose ? element ? 19 : 9 : element ? 41 : 31);
    if (array == NULL)
        return model_runtime_null(error, element ? "Squid null captured tentacle array aaload"
                                                 : "Squid null tentacle array length receiver");
    size_t capacity;
    if (!manager_part_array_native_extent(error->native_owner, array, &capacity))
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_STATE,
                                  "Squid actual typed model-array allocation provenance");
    const int32_t length = array->length;
    error->state.array_length = length;
    if (!element)
        ++error->state.array_length_reads;
    /* A genuine Java array length is immutable. Native allocation extent is
     * separate ledger metadata, never inferred from this source field. */
    if (length < 0 || (size_t)length != capacity)
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_STATE,
                                  "Squid immutable source array/native extent mismatch");
    if (element && (index < 0 || index >= length)) {
        error->source_exception = true;
        error->throwable = MC_CLIENT_MODEL_DRAW_SOURCE_EXCEPTION;
        error->exception_class = "java/lang/ArrayIndexOutOfBoundsException";
        return model_runtime_fail(error, MC_CLIENT_MODEL_RUNTIME_SOURCE_EXCEPTION,
                                  "Squid actual tentacle array index");
    }
    *out = array;
    *out_length = length;
    return true;
}

bool model_runtime_wn_pose(manager_model *model, const float values[6],
                           const mc_client_model_runtime_context *context,
                           mc_client_model_runtime_error *error) {
    (void)context;
    int32_t index = 0;
    for (;;) {
        manager_part_array *array;
        int32_t length;
        if (!array_read(model, true, false, index, &array, &length, error))
            return false;
        if (index >= length)
            break;
        if (!array_read(model, true, true, index, &array, &length, error))
            return false;
        mc_client_model_part *part = array->elements[(size_t)index];
        ++error->state.array_element_reads;
        error->state.part = part;
        squid_at(error, "b", 21);
        if (part == NULL)
            return model_runtime_null(error, "Squid null captured tentacle pose receiver");
        part->d = values[2];
        ++error->state.pose_stores;
        /* A valid array's nonnegative int length bounds this increment. */
        ++index;
    }
    squid_at(error, "b", 30);
    return true;
}

bool model_runtime_wn_render(mc_client_managed_renderer *owner, manager_model *model,
                             const float values[6], const mc_client_model_runtime_context *context,
                             mc_client_model_runtime_error *error) {
    squid_at(error, "a", 10);
    if (!model_runtime_call_push(error) || !model_runtime_pose_body(model, values, context, error))
        return false;
    model_runtime_call_pop(error);
    mc_client_model_part *part = model->fields.a;
    squid_at(error, "a", 19);
    if (!model_runtime_draw_part(owner, part, values[5], context, error))
        return false;
    int32_t index = 0;
    for (;;) {
        manager_part_array *array;
        int32_t length;
        if (!array_read(model, false, false, index, &array, &length, error))
            return false;
        if (index >= length)
            break;
        if (!array_read(model, false, true, index, &array, &length, error))
            return false;
        part = array->elements[(size_t)index];
        ++error->state.array_element_reads;
        squid_at(error, "a", 44);
        if (!model_runtime_draw_part(owner, part, values[5], context, error))
            return false;
        ++index;
    }
    squid_at(error, "a", 53);
    return true;
}
