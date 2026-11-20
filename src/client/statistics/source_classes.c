#include "internal.h"
#include <stdlib.h>

static mc_client_stats_status class_result(mc_client_stats_error *out,
                                           const mc_client_stats_error *error) {
    if (out != NULL)
        *out = *error;
    return error->status;
}

mc_client_stats_status mc_client_stats_classes_create(size_t maximum, mc_client_stats_classes **out,
                                                      mc_client_stats_error *error) {
    mc_client_stats_error cause;
    stats_error_clear(&cause);
    if (out == NULL)
        stats_fail(&cause, MC_CLIENT_STATS_INVALID_ARGUMENT, "xi literal scope output", NULL, NULL,
                   NULL, 0);
    else {
        mc_client_stats_classes *classes = calloc(1, sizeof(*classes));
        if (classes == NULL)
            stats_fail(&cause, MC_CLIENT_STATS_ALLOCATION_FAILED, "xi cold literal scope", NULL,
                       NULL, NULL, 0);
        else {
            classes->holds = 1;
            classes->max_string_bytes = maximum == 0 ? 1048576u : maximum;
            *out = classes;
        }
    }
    return class_result(error, &cause);
}

mc_client_stats_status mc_client_stats_classes_retain(mc_client_stats_classes *classes,
                                                      mc_client_stats_error *error) {
    mc_client_stats_error cause;
    stats_error_clear(&cause);
    if (classes == NULL)
        stats_fail(&cause, MC_CLIENT_STATS_INVALID_ARGUMENT, "xi literal scope retain", NULL, NULL,
                   NULL, 0);
    else if (classes->active || classes->retiring)
        stats_fail(&cause, MC_CLIENT_STATS_BUSY, "xi active/terminal literal scope", NULL, NULL,
                   NULL, 0);
    else if (classes->holds == SIZE_MAX)
        stats_fail(&cause, MC_CLIENT_STATS_LIMIT, "xi literal scope count", NULL, NULL, NULL, 0);
    else
        ++classes->holds;
    return class_result(error, &cause);
}

static mc_client_stats_status release_scope(mc_client_stats_classes **slot, bool strict,
                                            mc_client_stats_error *error) {
    mc_client_stats_error cause;
    stats_error_clear(&cause);
    if (slot == NULL)
        stats_fail(&cause, MC_CLIENT_STATS_INVALID_ARGUMENT, "xi literal scope slot", NULL, NULL,
                   NULL, 0);
    else if (*slot != NULL) {
        mc_client_stats_classes *classes = *slot;
        if (classes->active || classes->retiring || (strict && classes->holds != 1))
            stats_fail(&cause, MC_CLIENT_STATS_BUSY, "xi literal scope retained/active", NULL, NULL,
                       NULL, 0);
        else if (classes->holds > 1) {
            --classes->holds;
            *slot = NULL;
        } else {
            classes->active = true;
            classes->retiring = true;
            mc_world_info_name_release(classes->stats_literal);
            free(classes);
            *slot = NULL;
        }
    }
    return class_result(error, &cause);
}

mc_client_stats_status mc_client_stats_classes_release(mc_client_stats_classes **slot,
                                                       mc_client_stats_error *error) {
    return release_scope(slot, false, error);
}

mc_client_stats_status mc_client_stats_classes_retire(mc_client_stats_classes **slot,
                                                      mc_client_stats_error *error) {
    return release_scope(slot, true, error);
}

mc_client_stats_status stats_classes_literal(mc_client_stats_classes *classes,
                                             mc_world_info_name **out,
                                             mc_client_stats_error *error) {
    mc_client_stats_error cause;
    stats_error_clear(&cause);
    if (classes == NULL || out == NULL)
        stats_fail(&cause, MC_CLIENT_STATS_INVALID_ARGUMENT, "xi literal native arguments", "xi",
                   "<init>", "(Lgr;Ljava/io/File;)V", 36);
    else if (classes->active || classes->retiring)
        stats_fail(&cause, MC_CLIENT_STATS_BUSY, "xi literal native reentry/terminal", "xi",
                   "<init>", "(Lgr;Ljava/io/File;)V", 36);
    else if (classes->failed)
        cause = classes->failure;
    else {
        classes->active = true;
        if (classes->stats_literal == NULL) {
            static const uint16_t units[] = {'s', 't', 'a', 't', 's'};
            const mc_world_info_status status = mc_world_info_name_copy(
                (mc_nbt_string){units, 5}, classes->max_string_bytes, &classes->stats_literal);
            if (status != MC_WORLD_INFO_OK) {
                stats_fail(
                    &cause,
                    status == MC_WORLD_INFO_LIMIT               ? MC_CLIENT_STATS_LIMIT
                    : status == MC_WORLD_INFO_ALLOCATION_FAILED ? MC_CLIENT_STATS_ALLOCATION_FAILED
                                                                : MC_CLIENT_STATS_INVALID_PHASE,
                    "xi ldc stats String resolution", "xi", "<init>", "(Lgr;Ljava/io/File;)V", 36);
                classes->failed = true; /* Native immutable-scope failure, not Java clinit. */
            }
        }
        if (cause.status == MC_CLIENT_STATS_OK) {
            const mc_world_info_status held = mc_world_info_name_retain(classes->stats_literal);
            if (held != MC_WORLD_INFO_OK)
                stats_fail(&cause, MC_CLIENT_STATS_LIMIT, "xi literal native occurrence", "xi",
                           "<init>", "(Lgr;Ljava/io/File;)V", 36);
            else
                *out = classes->stats_literal;
        }
        if (cause.status != MC_CLIENT_STATS_OK)
            classes->failure = cause;
        classes->active = false;
    }
    return class_result(error, &cause);
}

bool mc_client_stats_classes_read_state(const mc_client_stats_classes *classes,
                                        mc_client_stats_classes_state *out) {
    if (classes == NULL || out == NULL)
        return false;
    *out = (mc_client_stats_classes_state){
        classes->holds,   classes->active,   classes->stats_literal != NULL,
        classes->failed,  classes->retiring, classes->stats_literal,
        &classes->failure};
    return true;
}
