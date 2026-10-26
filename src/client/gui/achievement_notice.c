#include "mc/client/gui/achievement_notice.h"
#include "../achievements/description_registry.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "millis.h"
#include <stdlib.h>

struct mc_client_achievement_notice {
    mc_client_render_classes *models;
    mc_registry_classes *registry;
    mc_client_model_phase phase;
    mc_client_achievement_notice_fields fields;
    mc_client_description_result description;
    mc_client_item_renderer *item; /* Native retention of unpublished child. */
};

static mc_client_notice_status result(mc_client_notice_error *e, mc_client_notice_status s,
                                      int32_t pc) {
    if (e != NULL)
        *e = (mc_client_notice_error){.status = s,
                                      .source_class = s == MC_CLIENT_NOTICE_OK ? NULL : "vu",
                                      .source_pc = pc,
                                      .notice_pc = pc};
    return s;
}

mc_client_notice_status mc_client_achievement_notice_allocate(mc_client_render_classes *models,
                                                              mc_registry_classes *registry,
                                                              mc_java_random_profile *profile,
                                                              mc_client_achievement_notice **out,
                                                              mc_client_notice_error *error) {
    if (models == NULL || registry == NULL || profile == NULL || out == NULL)
        return result(error, MC_CLIENT_NOTICE_INVALID_ARGUMENT, -1);
    mc_client_achievement_notice *p = calloc(1, sizeof(*p));
    if (p == NULL)
        return result(error, MC_CLIENT_NOTICE_ALLOCATION_FAILED, -1);
    if (mc_client_render_classes_retain(models, profile) != MC_CLIENT_MODEL_OK) {
        free(p);
        return result(error, MC_CLIENT_NOTICE_INVALID_ARGUMENT, -1);
    }
    mc_registry_error cause = {0};
    if (mc_registry_classes_retain(registry, MC_REGISTRY_CLIENT, profile, &cause) !=
        MC_REGISTRY_OK) {
        (void)mc_client_render_classes_release(models);
        free(p);
        result(error, MC_CLIENT_NOTICE_REGISTRY_FAILED, -1);
        if (error != NULL)
            error->registry = cause;
        return MC_CLIENT_NOTICE_REGISTRY_FAILED;
    }
    p->models = models;
    p->registry = registry;
    *out = p;
    return result(error, MC_CLIENT_NOTICE_OK, -1);
}

mc_client_notice_status mc_client_achievement_notice_construct(mc_client_achievement_notice *p,
                                                               client_application *app,
                                                               mc_client_notice_error *error) {
    if (p == NULL)
        return result(error, MC_CLIENT_NOTICE_INVALID_ARGUMENT, -1);
    if (p->phase != MC_CLIENT_MODEL_RAW)
        return result(error, MC_CLIENT_NOTICE_INVALID_PHASE, -1);
    p->phase = MC_CLIENT_MODEL_CONSTRUCTING;
    p->fields.k = 0.0f;
    p->fields.a = app;
    mc_client_model_error cause = {0};
    mc_client_model_status status = mc_client_item_renderer_allocate(p->models, 0, &p->item);
    const int32_t failure_pc = status == MC_CLIENT_MODEL_OK ? 14 : 10;
    if (status == MC_CLIENT_MODEL_OK)
        status = mc_client_item_renderer_construct(p->item, &cause);
    if (status != MC_CLIENT_MODEL_OK) {
        p->phase = MC_CLIENT_MODEL_FAILED;
        result(error, MC_CLIENT_NOTICE_MODEL_FAILED, failure_pc);
        if (error != NULL) {
            error->model = cause;
            error->model.status = status;
        }
        return MC_CLIENT_NOTICE_MODEL_FAILED;
    }
    p->fields.h = p->item;
    p->phase = MC_CLIENT_MODEL_READY;
    return result(error, MC_CLIENT_NOTICE_OK, -1);
}

const mc_client_achievement_notice_fields *
mc_client_achievement_notice_fields_get(const mc_client_achievement_notice *p) {
    return p == NULL ? NULL : &p->fields;
}

mc_client_model_phase mc_client_achievement_notice_phase(const mc_client_achievement_notice *p) {
    return p == NULL ? MC_CLIENT_MODEL_FAILED : p->phase;
}

static mc_client_notice_status admitted(mc_client_achievement_notice *p,
                                        const mc_statistic *achievement,
                                        mc_client_notice_error *error) {
    if (p == NULL)
        return result(error, MC_CLIENT_NOTICE_INVALID_ARGUMENT, -1);
    if (p->phase != MC_CLIENT_MODEL_READY)
        return result(error, MC_CLIENT_NOTICE_INVALID_PHASE, -1);
    mc_registry_error cause = {0};
    if (mc_client_achievement_admit(p->registry, achievement, &cause) != MC_REGISTRY_OK) {
        result(error, MC_CLIENT_NOTICE_REGISTRY_FAILED, -1);
        if (error != NULL)
            error->registry = cause;
        return MC_CLIENT_NOTICE_REGISTRY_FAILED;
    }
    return MC_CLIENT_NOTICE_OK;
}

mc_client_notice_status mc_client_achievement_notice_remind(mc_client_achievement_notice *p,
                                                            const mc_statistic *achievement,
                                                            mc_client_notice_error *error) {
    mc_client_notice_status status = admitted(p, achievement, error);
    if (status != MC_CLIENT_NOTICE_OK)
        return status;
    if (achievement == NULL)
        return result(error, MC_CLIENT_NOTICE_SOURCE_EXCEPTION, 2);
    p->fields.d = mc_statistic_fields_get(achievement)->f;
    mc_client_description_result description = {0};
    status = mc_client_achievement_description(p->registry, achievement, &description, error);
    if (status != MC_CLIENT_NOTICE_OK) {
        if (error != NULL)
            error->notice_pc = 10;
        return status;
    }
    p->fields.e = description.string;
    mc_client_description_result old_description = p->description;
    p->description = description;
    mc_client_description_result_free(&old_description);
    int64_t now;
    if (mc_platform_current_time_millis(&now) != MC_PLATFORM_MILLIS_OK)
        return result(error, MC_CLIENT_NOTICE_CLOCK_FAILED, 17);
    p->fields.g = mc_java_i64_sub(now, INT64_C(2500));
    p->fields.f = achievement;
    p->fields.i = true;
    return result(error, MC_CLIENT_NOTICE_OK, -1);
}

mc_client_notice_status mc_client_achievement_notice_complete(mc_client_achievement_notice *p,
                                                              const mc_statistic *achievement,
                                                              mc_client_notice_error *error) {
    mc_client_notice_status status = admitted(p, achievement, error);
    if (status != MC_CLIENT_NOTICE_OK)
        return status;
    const mc_registry_string *title = NULL;
    mc_registry_error cause = {0};
    if (mc_client_achievement_get_title(p->registry, &title, &cause) != MC_REGISTRY_OK) {
        result(error, MC_CLIENT_NOTICE_REGISTRY_FAILED, 3);
        if (error != NULL)
            error->registry = cause;
        return MC_CLIENT_NOTICE_REGISTRY_FAILED;
    }
    p->fields.d = title;
    if (achievement == NULL)
        return result(error, MC_CLIENT_NOTICE_SOURCE_EXCEPTION, 11);
    p->fields.e = mc_statistic_fields_get(achievement)->f;
    mc_client_description_result_free(&p->description);
    int64_t now;
    if (mc_platform_current_time_millis(&now) != MC_PLATFORM_MILLIS_OK)
        return result(error, MC_CLIENT_NOTICE_CLOCK_FAILED, 18);
    p->fields.g = now;
    p->fields.f = achievement;
    p->fields.i = false;
    return result(error, MC_CLIENT_NOTICE_OK, -1);
}

mc_client_notice_status mc_client_achievement_notice_free(mc_client_achievement_notice **out,
                                                          mc_client_notice_error *error) {
    if (out == NULL)
        return result(error, MC_CLIENT_NOTICE_INVALID_ARGUMENT, -1);
    mc_client_achievement_notice *p = *out;
    if (p == NULL)
        return result(error, MC_CLIENT_NOTICE_OK, -1);
    if (p->phase == MC_CLIENT_MODEL_CONSTRUCTING)
        return result(error, MC_CLIENT_NOTICE_BUSY, -1);
    mc_registry_error cause = {0};
    if (mc_client_notice_registry_retire_admit(p->registry, &cause) != MC_REGISTRY_OK) {
        result(error, MC_CLIENT_NOTICE_REGISTRY_FAILED, -1);
        if (error != NULL)
            error->registry = cause;
        return MC_CLIENT_NOTICE_REGISTRY_FAILED;
    }
    if (mc_client_item_renderer_free(&p->item) != MC_CLIENT_MODEL_OK)
        return result(error, MC_CLIENT_NOTICE_BUSY, -1);
    mc_client_description_result_free(&p->description);
    if (mc_registry_classes_release(p->registry, &cause) != MC_REGISTRY_OK) {
        result(error, MC_CLIENT_NOTICE_REGISTRY_FAILED, -1);
        if (error != NULL)
            error->registry = cause;
        return MC_CLIENT_NOTICE_REGISTRY_FAILED;
    }
    (void)mc_client_render_classes_release(p->models);
    free(p);
    *out = NULL;
    return result(error, MC_CLIENT_NOTICE_OK, -1);
}

mc_client_notice_status mc_client_achievement_notice_draw(mc_client_achievement_notice *p,
                                                          mc_client_notice_error *error) {
    if (p == NULL)
        return result(error, MC_CLIENT_NOTICE_INVALID_ARGUMENT, -1);
    return result(error, MC_CLIENT_NOTICE_UNIMPLEMENTED_DEPENDENCY, 0);
}
