#include "internal.h"

#include <string.h>

void mc_entity_hit_error_clear(mc_entity_hit_error *error) {
    if (error != NULL)
        memset(error, 0, sizeof(*error));
}

mc_entity_hit_status mc_entity_hit_fail(mc_entity_hit_error *error, mc_entity_hit_status status,
                                        const char *operation, bool native_policy,
                                        mc_entity_classes *classes, const char *method,
                                        const char *descriptor, bool entered, uint16_t pc) {
    if (error != NULL) {
        error->status = status;
        error->operation = operation;
        error->native_policy = native_policy;
        error->site = (mc_entity_geometry_site){
            method != NULL,
            entered,
            classes == NULL || classes->source == MC_ENTITY_CLIENT ? "vf" : "nh",
            method,
            descriptor,
            pc};
    }
    return status;
}

mc_entity_hit_status mc_entity_hit_result_allocate(mc_entity_classes *classes,
                                                   mc_entity_hit_result **out,
                                                   mc_entity_hit_error *error) {
    mc_entity_hit_error_clear(error);
    if (classes == NULL || out == NULL)
        return mc_entity_hit_fail(error, MC_ENTITY_HIT_INVALID_ARGUMENT, "Hit native allocation",
                                  true, classes, NULL, NULL, false, 0);
    if (classes->live_hit_results == SIZE_MAX) {
        if (error != NULL)
            error->native.status = MC_ENTITY_LIMIT;
        return mc_entity_hit_fail(error, MC_ENTITY_HIT_NATIVE_FAILED, "Hit native live count", true,
                                  classes, NULL, NULL, false, 0);
    }
    mc_entity_hit_result *result = mc_entity_class_memory_allocate(
        classes, sizeof(*result), error == NULL ? NULL : &error->native, "Hit native NEW", 0);
    if (result == NULL)
        return mc_entity_hit_fail(error, MC_ENTITY_HIT_NATIVE_FAILED, "Hit native NEW", true,
                                  classes, NULL, NULL, false, 0);
    memset(result, 0, sizeof(*result));
    result->classes = classes;
    result->holds = 1;
    ++classes->live_hit_results;
    *out = result;
    return MC_ENTITY_HIT_OK;
}

static mc_entity_hit_status finish(mc_entity_hit_result *raw, mc_entity_hit_status status,
                                   mc_entity_hit_error *error) {
    raw->phase =
        status == MC_ENTITY_HIT_OK ? MC_ENTITY_HIT_READY : MC_ENTITY_HIT_FAILED_CONSTRUCTION;
    raw->active = false;
    if (error != NULL)
        *error = raw->construction_error;
    return status;
}

static mc_entity_hit_status start(mc_entity_hit_result *raw, const char *descriptor,
                                  mc_entity_hit_error *error) {
    mc_entity_hit_error_clear(error);
    if (raw == NULL)
        return mc_entity_hit_fail(error, MC_ENTITY_HIT_SOURCE_NULL_REFERENCE,
                                  "Hit constructor null receiver", false, NULL, "<init>",
                                  descriptor, false, 0);
    if (raw->phase != MC_ENTITY_HIT_RAW || raw->active || raw->retiring)
        return mc_entity_hit_fail(error, MC_ENTITY_HIT_INVALID_PHASE,
                                  "Hit native constructor phase", true, raw->classes, "<init>",
                                  descriptor, false, 0);
    raw->active = true;
    raw->phase = MC_ENTITY_HIT_CONSTRUCTING;
    mc_entity_hit_error_clear(&raw->construction_error);
    mc_entity_hit_status status =
        mc_entity_hit_kind_ensure(raw->classes, &raw->construction_error.kind);
    if (status != MC_ENTITY_HIT_OK) {
        mc_entity_hit_fail(&raw->construction_error, status, "Hit kind active use", true,
                           raw->classes, "<init>", descriptor, true, 5);
        return finish(raw, status, error);
    }
    return MC_ENTITY_HIT_OK;
}

mc_entity_hit_status mc_entity_hit_result_construct_block(mc_entity_hit_result *raw, int32_t x,
                                                          int32_t y, int32_t z, int32_t face,
                                                          const mc_entity_vector *point,
                                                          mc_entity_hit_error *error) {
    const char *descriptor =
        raw == NULL || raw->classes->source == MC_ENTITY_CLIENT ? "(IIIILbt;)V" : "(IIIILba;)V";
    mc_entity_hit_status status = start(raw, descriptor, error);
    if (status != MC_ENTITY_HIT_OK)
        return status;
    raw->fields.a = raw->classes->hit_kinds->a;
    raw->fields.b = x;
    raw->fields.c = y;
    raw->fields.d = z;
    raw->fields.e = face;
    if (point == NULL) {
        status = mc_entity_hit_fail(&raw->construction_error, MC_ENTITY_HIT_SOURCE_NULL_REFERENCE,
                                    "Hit point null source getfield", false, raw->classes, "<init>",
                                    descriptor, true, 35);
    } else {
        const double px = point->x, py = point->y, pz = point->z;
        if (mc_entity_vector_pool_get(raw->classes, px, py, pz, &raw->fields.f,
                                      &raw->construction_error.vector) != MC_ENTITY_OK)
            status = mc_entity_hit_fail(&raw->construction_error, MC_ENTITY_HIT_VECTOR_FAILED,
                                        "Hit pooled point copy", true, raw->classes, "<init>",
                                        descriptor, true, 48);
    }
    return finish(raw, status, error);
}

mc_entity_hit_status mc_entity_hit_result_construct_entity(mc_entity_hit_result *raw,
                                                           mc_entity *target,
                                                           mc_entity_hit_error *error) {
    const char *descriptor =
        raw == NULL || raw->classes->source == MC_ENTITY_CLIENT ? "(Lsn;)V" : "(Llq;)V";
    if (raw != NULL && target != NULL && target->classes != raw->classes) {
        mc_entity_hit_error_clear(error);
        return mc_entity_hit_fail(error, MC_ENTITY_HIT_INVALID_ARGUMENT,
                                  "Hit target native class namespace", true, raw->classes, "<init>",
                                  descriptor, false, 0);
    }
    mc_entity_hit_status status = start(raw, descriptor, error);
    if (status != MC_ENTITY_HIT_OK)
        return status;
    raw->fields.a = raw->classes->hit_kinds->b;
    if (target != NULL) {
        raw->construction_error.native.status = mc_entity_retain(target);
        if (raw->construction_error.native.status != MC_ENTITY_OK) {
            status = mc_entity_hit_fail(&raw->construction_error, MC_ENTITY_HIT_NATIVE_FAILED,
                                        "Hit target native occurrence", true, raw->classes,
                                        "<init>", descriptor, true, 13);
            return finish(raw, status, error);
        }
    }
    raw->fields.g = target;
    if (target == NULL) {
        status = mc_entity_hit_fail(&raw->construction_error, MC_ENTITY_HIT_SOURCE_NULL_REFERENCE,
                                    "Hit target null source getfield", false, raw->classes,
                                    "<init>", descriptor, true, 18);
    } else {
        const double x = target->base.x, y = target->base.y, z = target->base.z;
        if (mc_entity_vector_pool_get(raw->classes, x, y, z, &raw->fields.f,
                                      &raw->construction_error.vector) != MC_ENTITY_OK)
            status = mc_entity_hit_fail(&raw->construction_error, MC_ENTITY_HIT_VECTOR_FAILED,
                                        "Hit target pooled point copy", true, raw->classes,
                                        "<init>", descriptor, true, 29);
    }
    return finish(raw, status, error);
}

mc_entity_hit_status mc_entity_hit_result_retain(mc_entity_hit_result *result,
                                                 mc_entity_hit_error *error) {
    mc_entity_hit_error_clear(error);
    if (result == NULL || result->retiring || result->holds == 0)
        return mc_entity_hit_fail(error, MC_ENTITY_HIT_INVALID_ARGUMENT, "Hit native retain", true,
                                  result == NULL ? NULL : result->classes, NULL, NULL, false, 0);
    if (result->holds == SIZE_MAX) {
        if (error != NULL)
            error->native.status = MC_ENTITY_LIMIT;
        return mc_entity_hit_fail(error, MC_ENTITY_HIT_NATIVE_FAILED, "Hit native retain overflow",
                                  true, result->classes, NULL, NULL, false, 0);
    }
    ++result->holds;
    return MC_ENTITY_HIT_OK;
}

mc_entity_hit_status mc_entity_hit_result_release(mc_entity_hit_result **owned,
                                                  mc_entity_hit_error *error) {
    mc_entity_hit_error_clear(error);
    if (owned == NULL)
        return mc_entity_hit_fail(error, MC_ENTITY_HIT_INVALID_ARGUMENT, "Hit native release slot",
                                  true, NULL, NULL, NULL, false, 0);
    mc_entity_hit_result *result = *owned;
    if (result == NULL)
        return MC_ENTITY_HIT_OK;
    if (result->active)
        return mc_entity_hit_fail(error, MC_ENTITY_HIT_BUSY, "Hit active native release", true,
                                  result->classes, NULL, NULL, false, 0);
    if (result->holds > 1) {
        --result->holds;
        *owned = NULL;
        return MC_ENTITY_HIT_OK;
    }
    result->retiring = true;
    if (result->fields.f != NULL) {
        const mc_entity_status status = mc_entity_vector_release(result->classes, result->fields.f);
        if (status != MC_ENTITY_OK) {
            if (error != NULL)
                error->native.status = status;
            return mc_entity_hit_fail(error, MC_ENTITY_HIT_NATIVE_FAILED,
                                      "Hit native vector cleanup", true, result->classes, NULL,
                                      NULL, false, 0);
        }
        result->fields.f = NULL;
    }
    mc_entity_release(result->fields.g);
    result->fields.g = NULL;
    mc_entity_classes *classes = result->classes;
    --classes->live_hit_results;
    mc_entity_class_memory_retire(classes, result, sizeof(*result));
    *owned = NULL;
    return MC_ENTITY_HIT_OK;
}

mc_entity_hit_status mc_entity_hit_result_inspect(const mc_entity_hit_result *result,
                                                  mc_entity_hit_result_view *out) {
    if (result == NULL || out == NULL)
        return MC_ENTITY_HIT_INVALID_ARGUMENT;
    *out = (mc_entity_hit_result_view){result->classes, result->phase, result->fields,
                                       result->holds, &result->construction_error};
    return MC_ENTITY_HIT_OK;
}
