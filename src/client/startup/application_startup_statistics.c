#include "application_startup_statistics.h"
#include "application_owner_internal.h"
#include "application_startup_internal.h"

#include <stdlib.h>

#define MC_CLASS "net/minecraft/client/Minecraft"

static bool bind_classes(mc_client_application_classes *classes,
                         mc_client_startup_stats_call *call) {
    if (mtx_lock(&classes->mutex) != thrd_success) {
        call->classes_error.status = MC_CLIENT_STATS_THREAD_FAILED;
        call->classes_error.operation = "startup xi literal namespace lock";
        return false;
    }
    bool ready = !classes->native_retiring && classes->phase == MC_CLIENT_APPLICATION_CLASS_READY;
    if (!ready) {
        call->classes_error.status = MC_CLIENT_STATS_INVALID_PHASE;
        call->classes_error.operation = "startup actual Minecraft namespace phase";
    } else if (classes->startup_stats_classes == NULL) {
        /* Cold native scope only. The actual stats String is resolved later
         * by xi at36, after its reached raw File allocation at31. */
        ready = mc_client_stats_classes_create(0, &classes->startup_stats_classes,
                                               &call->classes_error) == MC_CLIENT_STATS_OK;
    }
    call->classes = classes->startup_stats_classes;
    if (mtx_unlock(&classes->mutex) != thrd_success) {
        call->classes_error.status = MC_CLIENT_STATS_THREAD_FAILED;
        call->classes_error.operation = "startup xi literal namespace unlock";
        return false;
    }
    return ready;
}

mc_client_startup_status application_startup_statistics(mc_client_application_startup *startup) {
    if (startup == NULL || startup->app == NULL)
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    if (startup->statistics != NULL)
        return startup->statistics->active ? MC_CLIENT_STARTUP_BUSY
                                           : MC_CLIENT_STARTUP_INVALID_PHASE;
    client_application *app = startup->app;
    if (app->constructor_phase != MC_CLIENT_APP_READY || app->source_namespace == NULL ||
        startup->phase == MC_CLIENT_STARTUP_RETIRING || app->startup != startup ||
        app->stats_writer != NULL || app->stats_writer_retained != NULL ||
        app->stats_writer_constructing || app->statistics_retiring || app->statistics_retired)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "startup statistics live actual owners", NULL, NULL, NULL,
                                        0);
    startup->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 324};
    mc_client_startup_stats_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                                        "startup statistics native call storage", NULL, NULL, NULL,
                                        0);
    startup->statistics = call;
    startup->failure.statistics = call;
    const bool outer_active = startup->active;
    startup->active = call->active = true;
    call->last_pc = 324;
    if (!bind_classes(app->source_namespace, call)) {
        call->active = false;
        startup->active = outer_active;
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_STATISTICS_FAILED,
                                        call->classes_error.operation, NULL, NULL, NULL, 0);
    }
    call->entered = true;
    call->last_pc = 325;
    startup->failure.caller.pc = 325;
    /* Canonical helper performs NEW325, live k330/Z334, xi337, I340,
     * then the actual ep343/kh346/351/ny354 sequence. No worker observation,
     * service or join is inserted between construction and callback install. */
    const mc_client_statistics_owner_status status =
        mc_client_application_construct_statistics_file(app, call->classes, startup->services.files,
                                                        NULL, &call->report, &call->failure);
    call->candidate = app->stats_writer_retained;
    call->published = app->stats_writer;
    if (call->published != NULL && call->published == call->candidate) {
        call->I_stored = true;
        call->last_pc = 340;
        startup->last_startup_store = 340;
    }
    call->active = false;
    startup->active = outer_active;
    if (status != MC_CLIENT_STATISTICS_OWNER_OK) {
        const mc_client_statistics_owner_error *error = &call->failure;
        if (error->site.class_name != NULL)
            startup->failure.caller = (mc_client_application_owner_site){
                error->site.class_name, error->site.method, error->site.descriptor, error->site.pc};
        const mc_client_stats_error *child = &error->writer;
        if (status == MC_CLIENT_STATISTICS_OWNER_NOTICE_FAILED)
            return application_startup_fail(
                &startup->failure, MC_CLIENT_STARTUP_STATISTICS_FAILED, error->operation,
                error->notice.source_class, "a", "()V",
                error->notice.source_pc < 0 ? 0u : (unsigned int)error->notice.source_pc);
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_STATISTICS_FAILED,
                                        error->operation, child->class_name, child->method,
                                        child->descriptor, child->bytecode);
    }
    call->callback_installed = call->source_calls_completed = true;
    call->last_pc = 354;
    startup->failure.status = MC_CLIENT_STARTUP_OK;
    startup->failure.operation = NULL;
    startup->failure.child = (mc_client_application_owner_site){0};
    return MC_CLIENT_STARTUP_OK;
}

mc_client_startup_status
application_startup_statistics_retire(mc_client_application_startup *startup) {
    mc_client_startup_stats_call *call = startup->statistics;
    startup->retirement.statistics = call;
    if (call == NULL || call->retired)
        return MC_CLIENT_STARTUP_OK;
    if (call->active)
        return MC_CLIENT_STARTUP_BUSY;
    if (!call->entered) {
        call->retired = true; /* Only cold native class association was attempted. */
        return MC_CLIENT_STARTUP_OK;
    }
    client_application *app = startup->app;
    const bool externally_retired =
        app->statistics_retired && app->stats_writer == NULL && app->stats_writer_retained == NULL;
    if (!externally_retired &&
        (app->stats_writer != call->published || app->stats_writer_retained != call->candidate))
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    const mc_client_statistics_owner_status status =
        mc_client_application_retire_statistics(app, &call->retirement);
    /* A consumed writer stays consumed even when retirement reports a prior
     * close error. No borrowed File-state view or old identity is dereferenced. */
    call->candidate = app->stats_writer_retained;
    call->published = app->stats_writer;
    if (status != MC_CLIENT_STATISTICS_OWNER_OK)
        return status == MC_CLIENT_STATISTICS_OWNER_BUSY ||
                       call->retirement.writer.status == MC_CLIENT_STATS_BUSY
                   ? MC_CLIENT_STARTUP_BUSY
                   : MC_CLIENT_STARTUP_STATISTICS_FAILED;
    call->retired = true;
    return MC_CLIENT_STARTUP_OK;
}
