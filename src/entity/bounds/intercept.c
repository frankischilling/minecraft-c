#include "../hit_result/internal.h"

#include <string.h>

struct mc_entity_bounds_intercept_call {
    mc_entity_classes *classes;
    mc_entity_bounds_intercept_phase phase;
    bool active, retiring, source_returned, start_held, end_held;
    const mc_entity_bounds *receiver;
    mc_entity_vector *start, *end, *generated[6], *locals[6], *nearest;
    int32_t face;
    double candidate_square, nearest_square;
    mc_entity_hit_result *candidate, *result;
    mc_entity_bounds_intercept_error error, cleanup;
};

static const char *descriptor(const mc_entity_bounds_intercept_call *call) {
    return call->classes->source == MC_ENTITY_CLIENT ? "(Lbt;Lbt;)Lvf;" : "(Lba;Lba;)Lnh;";
}

static mc_entity_hit_status fail(mc_entity_bounds_intercept_call *call, mc_entity_hit_status status,
                                 const char *operation, bool native_policy, bool entered,
                                 uint16_t pc) {
    call->error.status = status;
    call->error.operation = operation;
    call->error.native_policy = native_policy;
    call->error.site = (mc_entity_geometry_site){
        true, entered,          call->classes->source == MC_ENTITY_CLIENT ? "eq" : "cz",
        "a",  descriptor(call), pc};
    call->phase = MC_ENTITY_BOUNDS_INTERCEPT_FAILED;
    call->active = false;
    return status;
}

static bool member(const mc_entity_bounds *bounds, const mc_entity_vector *point,
                   unsigned int axis) {
    if (point == NULL)
        return false;
    if (axis == 0)
        return point->y >= bounds->min_y && point->y <= bounds->max_y &&
               point->z >= bounds->min_z && point->z <= bounds->max_z;
    if (axis == 1)
        return point->x >= bounds->min_x && point->x <= bounds->max_x &&
               point->z >= bounds->min_z && point->z <= bounds->max_z;
    return point->x >= bounds->min_x && point->x <= bounds->max_x && point->y >= bounds->min_y &&
           point->y <= bounds->max_y;
}

static mc_entity_hit_status execute(mc_entity_bounds_intercept_call *call) {
    static const uint16_t plane_pc[6] = {6, 16, 27, 38, 49, 60};
    static const uint16_t candidate_pc[6] = {149, 178, 208, 238, 268, 298};
    static const uint16_t nearest_pc[6] = {155, 184, 214, 244, 274, 304};
    static const int32_t faces[6] = {4, 5, 0, 1, 2, 3};
    if (call->receiver == NULL)
        return fail(call, MC_ENTITY_HIT_SOURCE_NULL_REFERENCE, "Bounds intercept null receiver",
                    false, false, 0);
    for (size_t index = 0; index < 6; ++index) {
        const mc_entity_bounds *bounds = call->receiver;
        const double coordinate = index == 0   ? bounds->min_x
                                  : index == 1 ? bounds->max_x
                                  : index == 2 ? bounds->min_y
                                  : index == 3 ? bounds->max_y
                                  : index == 4 ? bounds->min_z
                                               : bounds->max_z;
        mc_entity_geometry_status status;
        if (index < 2)
            status = mc_entity_vector_intersect_x(call->classes, call->start, call->end, coordinate,
                                                  &call->generated[index], &call->error.geometry);
        else if (index < 4)
            status = mc_entity_vector_intersect_y(call->classes, call->start, call->end, coordinate,
                                                  &call->generated[index], &call->error.geometry);
        else
            status = mc_entity_vector_intersect_z(call->classes, call->start, call->end, coordinate,
                                                  &call->generated[index], &call->error.geometry);
        if (status != MC_ENTITY_GEOMETRY_OK)
            return fail(call,
                        status == MC_ENTITY_GEOMETRY_SOURCE_NULL_REFERENCE
                            ? MC_ENTITY_HIT_SOURCE_NULL_REFERENCE
                            : MC_ENTITY_HIT_VECTOR_FAILED,
                        "Bounds intercept plane child", call->error.geometry.native_policy, true,
                        plane_pc[index]);
        call->locals[index] = call->generated[index];
    }
    for (size_t index = 0; index < 6; ++index)
        if (!member(call->receiver, call->locals[index], (unsigned int)(index / 2)))
            call->locals[index] = NULL;
    for (size_t index = 0; index < 6; ++index) {
        if (call->locals[index] == NULL)
            continue;
        if (call->nearest != NULL) {
            if (mc_entity_vector_distance_squared(call->classes, call->start, call->locals[index],
                                                  &call->candidate_square,
                                                  &call->error.geometry) != MC_ENTITY_GEOMETRY_OK)
                return fail(call, MC_ENTITY_HIT_VECTOR_FAILED, "Bounds candidate distance child",
                            call->error.geometry.native_policy, true, candidate_pc[index]);
            if (mc_entity_vector_distance_squared(call->classes, call->start, call->nearest,
                                                  &call->nearest_square,
                                                  &call->error.geometry) != MC_ENTITY_GEOMETRY_OK)
                return fail(call, MC_ENTITY_HIT_VECTOR_FAILED, "Bounds nearest distance child",
                            call->error.geometry.native_policy, true, nearest_pc[index]);
            if (!(call->candidate_square < call->nearest_square))
                continue;
        }
        call->nearest = call->locals[index];
    }
    if (call->nearest != NULL) {
        call->face = -1;
        for (size_t index = 0; index < 6; ++index)
            if (call->nearest == call->locals[index])
                call->face = faces[index];
        if (mc_entity_hit_result_allocate(call->classes, &call->candidate, &call->error.hit) !=
            MC_ENTITY_HIT_OK)
            return fail(call, MC_ENTITY_HIT_NATIVE_FAILED, "Bounds intercept NEW hit child", true,
                        true, 384);
        const mc_entity_hit_status status = mc_entity_hit_result_construct_block(
            call->candidate, 0, 0, 0, call->face, call->nearest, &call->error.hit);
        if (status != MC_ENTITY_HIT_OK)
            return fail(call, status, "Bounds intercept hit constructor child",
                        call->error.hit.native_policy, true, 395);
        call->result = call->candidate;
    }
    memset(&call->error, 0, sizeof(call->error));
    call->phase = MC_ENTITY_BOUNDS_INTERCEPT_RETURNED;
    call->source_returned = true;
    call->active = false;
    return MC_ENTITY_HIT_OK;
}

mc_entity_hit_status mc_entity_bounds_intercept_begin(mc_entity_classes *classes,
                                                      const mc_entity_bounds *receiver,
                                                      mc_entity_vector *start,
                                                      mc_entity_vector *end,
                                                      mc_entity_bounds_intercept_call **out,
                                                      mc_entity_error *admission_error) {
    if (admission_error != NULL)
        memset(admission_error, 0, sizeof(*admission_error));
    if (classes == NULL || out == NULL || *out != NULL) {
        mc_entity_error_set(admission_error, MC_ENTITY_INVALID_ARGUMENT,
                            "Bounds intercept native arguments", 0);
        return MC_ENTITY_HIT_INVALID_ARGUMENT;
    }
    if (classes->live_hit_calls == SIZE_MAX) {
        mc_entity_error_set(admission_error, MC_ENTITY_LIMIT, "Bounds intercept native call count",
                            0);
        return MC_ENTITY_HIT_NATIVE_FAILED;
    }
    mc_entity_bounds_intercept_call *call = mc_entity_class_memory_allocate(
        classes, sizeof(*call), admission_error, "Bounds intercept native call frame", 0);
    if (call == NULL)
        return MC_ENTITY_HIT_NATIVE_FAILED;
    memset(call, 0, sizeof(*call));
    call->classes = classes;
    call->receiver = receiver;
    call->start = start;
    call->end = end;
    call->active = true;
    ++classes->live_hit_calls;
    *out = call;
    if (start != NULL) {
        call->error.native.status = mc_entity_vector_retain(classes, start);
        if (call->error.native.status != MC_ENTITY_OK)
            return fail(call, MC_ENTITY_HIT_NATIVE_FAILED,
                        "Bounds intercept native start occurrence", true, false, 0);
        call->start_held = true;
    }
    if (end != NULL) {
        call->error.native.status = mc_entity_vector_retain(classes, end);
        if (call->error.native.status != MC_ENTITY_OK)
            return fail(call, MC_ENTITY_HIT_NATIVE_FAILED, "Bounds intercept native end occurrence",
                        true, false, 0);
        call->end_held = true;
    }
    return execute(call);
}

mc_entity_hit_status mc_entity_bounds_intercept_inspect(const mc_entity_bounds_intercept_call *call,
                                                        mc_entity_bounds_intercept_view *out) {
    if (call == NULL || out == NULL)
        return MC_ENTITY_HIT_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->phase = call->phase;
    out->active = call->active;
    out->retiring = call->retiring;
    out->source_returned = call->source_returned;
    out->status = call->error.status;
    out->error = &call->error;
    out->cleanup = &call->cleanup;
    out->receiver = call->receiver;
    out->start = call->start;
    out->end = call->end;
    for (size_t index = 0; index < 6; ++index) {
        out->generated[index] = call->generated[index];
        out->locals[index] = call->locals[index];
    }
    out->nearest = call->nearest;
    out->face = call->face;
    out->candidate = call->candidate;
    out->result = call->result;
    return MC_ENTITY_HIT_OK;
}

static mc_entity_hit_status cleanup_vector(mc_entity_bounds_intercept_call *call,
                                           mc_entity_vector **slot, mc_entity_hit_error *error) {
    mc_entity_vector *vector = *slot;
    if (vector == NULL)
        return MC_ENTITY_HIT_OK;
    const mc_entity_status status = mc_entity_vector_release(call->classes, vector);
    if (status != MC_ENTITY_OK) {
        call->cleanup.native.status = status;
        call->cleanup.status = MC_ENTITY_HIT_NATIVE_FAILED;
        call->cleanup.native_policy = true;
        call->cleanup.operation = "Bounds intercept native vector cleanup";
        mc_entity_hit_fail(error, MC_ENTITY_HIT_NATIVE_FAILED, call->cleanup.operation, true,
                           call->classes, NULL, NULL, false, 0);
        if (error != NULL)
            error->native.status = status;
        return MC_ENTITY_HIT_NATIVE_FAILED;
    }
    *slot = NULL;
    for (size_t index = 0; index < 6; ++index)
        if (call->locals[index] == vector)
            call->locals[index] = NULL;
    if (call->nearest == vector)
        call->nearest = NULL;
    return MC_ENTITY_HIT_OK;
}

mc_entity_hit_status mc_entity_bounds_intercept_release(mc_entity_bounds_intercept_call **owned,
                                                        mc_entity_hit_error *error) {
    mc_entity_hit_error_clear(error);
    if (owned == NULL)
        return mc_entity_hit_fail(error, MC_ENTITY_HIT_INVALID_ARGUMENT,
                                  "Bounds intercept native release slot", true, NULL, NULL, NULL,
                                  false, 0);
    mc_entity_bounds_intercept_call *call = *owned;
    if (call == NULL)
        return MC_ENTITY_HIT_OK;
    if (call->active)
        return mc_entity_hit_fail(error, MC_ENTITY_HIT_BUSY,
                                  "Bounds intercept active native release", true, call->classes,
                                  NULL, NULL, false, 0);
    call->retiring = true;
    call->phase = MC_ENTITY_BOUNDS_INTERCEPT_RETIRING;
    memset(&call->cleanup, 0, sizeof(call->cleanup));
    if (mc_entity_hit_result_release(&call->candidate, &call->cleanup.hit) != MC_ENTITY_HIT_OK) {
        call->cleanup.status = call->cleanup.hit.status;
        call->cleanup.operation = "Bounds intercept native hit cleanup";
        call->cleanup.native_policy = true;
        if (error != NULL)
            *error = call->cleanup.hit;
        return call->cleanup.status;
    }
    call->result = NULL;
    for (size_t index = 0; index < 6; ++index)
        if (cleanup_vector(call, &call->generated[index], error) != MC_ENTITY_HIT_OK)
            return MC_ENTITY_HIT_NATIVE_FAILED;
    if (call->start_held) {
        if (cleanup_vector(call, &call->start, error) != MC_ENTITY_HIT_OK)
            return MC_ENTITY_HIT_NATIVE_FAILED;
        call->start_held = false;
    }
    if (call->end_held) {
        if (cleanup_vector(call, &call->end, error) != MC_ENTITY_HIT_OK)
            return MC_ENTITY_HIT_NATIVE_FAILED;
        call->end_held = false;
    }
    mc_entity_classes *classes = call->classes;
    --classes->live_hit_calls;
    mc_entity_class_memory_retire(classes, call, sizeof(*call));
    *owned = NULL;
    return MC_ENTITY_HIT_OK;
}
