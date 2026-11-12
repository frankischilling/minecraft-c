#include "application_startup_resources.h"
#include "application_owner_internal.h"
#include "application_startup_internal.h"

#include <stdlib.h>

#define MC_CLASS "net/minecraft/client/Minecraft"

static mc_client_startup_status download_failure(mc_client_application_startup *s) {
    const mc_client_download_error *cause = &s->resources->download;
    return application_startup_fail(
        &s->failure,
        cause->status == MC_CLIENT_DOWNLOAD_UNIMPLEMENTED ? MC_CLIENT_STARTUP_UNIMPLEMENTED
                                                          : MC_CLIENT_STARTUP_OWNER_FAILED,
        cause->operation, cause->site.owner, cause->site.method, cause->site.descriptor,
        cause->site.pc < 0 ? 0u : (unsigned int)cause->site.pc);
}

static mc_client_startup_status post_startup(mc_client_application_startup *s) {
    mc_client_startup_resources_call *call = s->resources;
    mc_client_application_classes *classes = s->app->source_namespace;
    s->failure.caller.pc = 691;
    if (mtx_lock(&classes->mutex) != thrd_success)
        return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_OWNER_FAILED,
                                        "Post startup literal namespace lock", NULL, NULL, NULL, 0);
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "Post startup live class namespace", NULL, NULL, NULL, 0);
    }
    mc_player_status literal = MC_PLAYER_OK;
    if (classes->startup_post_label == NULL) {
        static const uint16_t units[] = {'P', 'o', 's', 't', ' ', 's',
                                         't', 'a', 'r', 't', 'u', 'p'};
        literal = mc_player_string_create(units, sizeof(units) / sizeof(units[0]),
                                          &classes->startup_post_label);
    }
    mc_player_string *label = classes->startup_post_label;
    if (mtx_unlock(&classes->mutex) != thrd_success)
        return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_OWNER_FAILED,
                                        "Post startup literal namespace unlock", NULL, NULL, NULL,
                                        0);
    if (literal != MC_PLAYER_OK)
        return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_STRING_FAILED,
                                        "Post startup literal", MC_CLASS, "a", "()V", 691);
    s->failure.caller.pc = 693;
    if (mc_client_gl_output_allocate(&call->diagnostic, &call->graphics) !=
            MC_CLIENT_GL_OUTPUT_OK ||
        mc_client_gl_output_run(call->diagnostic, s->display, classes->system_output, label) !=
            MC_CLIENT_GL_OUTPUT_OK) {
        mc_client_gl_output_state state;
        if (call->diagnostic != NULL && mc_client_gl_output_inspect(call->diagnostic, &state))
            call->graphics = *state.error;
        return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_GRAPHICS_FAILED,
                                        "actual Post startup GL diagnostic", MC_CLASS, "c",
                                        "(Ljava/lang/String;)V", call->graphics.site.pc);
    }
    call->post_returned = true;
    return application_startup_gui_ingame(s);
}

mc_client_startup_status application_startup_resources(mc_client_application_startup *s) {
    if (s == NULL || s->app == NULL || s->resources != NULL || s->app->U != NULL)
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    mc_client_startup_resources_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                                        "resource startup call storage", NULL, NULL, NULL, 0);
    s->resources = call;
    call->active = true;
    s->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 664};
    mc_client_startup_status status;
    if (mc_client_download_classes_create(s->services.files,
                                          s->app->source_namespace->system_output, &call->classes,
                                          &call->download) != MC_CLIENT_DOWNLOAD_OK ||
        mc_client_download_thread_allocate(call->classes, NULL, &call->candidate,
                                           &call->download) != MC_CLIENT_DOWNLOAD_OK) {
        status = download_failure(s);
        goto done;
    }
    s->failure.caller.pc = 673;
    mc_client_download_status result = mc_client_download_thread_construct(
        call->candidate, s->app->game_directory, s->app, &call->download);
    if (result == MC_CLIENT_DOWNLOAD_OK) {
        call->constructed = true;
        s->app->U = call->candidate; /* Actual source publication only after cz returns. */
        call->U_stored = true;
        s->last_startup_store = 676;
        s->failure.caller.pc = 683;
        result = mc_client_download_thread_start(s->app->U, &call->download);
        call->started = result == MC_CLIENT_DOWNLOAD_OK;
    }
    /* Only an actual source Exception enters the empty catch at689. Native
     * allocation/transport admission and unimplemented dependencies remain stops. */
    if (result == MC_CLIENT_DOWNLOAD_SOURCE_EXCEPTION)
        call->caught_exception = true;
    else if (result != MC_CLIENT_DOWNLOAD_OK) {
        status = download_failure(s);
        goto done;
    }
    status = post_startup(s);
done:
    call->active = false;
    return status;
}

mc_client_startup_status application_startup_resources_retire(mc_client_application_startup *s) {
    mc_client_startup_resources_call *call = s->resources;
    if (call == NULL)
        return s->app->U == NULL ? MC_CLIENT_STARTUP_OK : MC_CLIENT_STARTUP_BUSY;
    if (call->active)
        return MC_CLIENT_STARTUP_BUSY;
    if (call->retired)
        return MC_CLIENT_STARTUP_OK;
    if (call->U_stored ? s->app->U != call->candidate : s->app->U != NULL)
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    if (call->candidate != NULL) {
        const mc_client_download_status result =
            mc_client_download_thread_retire(&call->candidate, &call->cleanup);
        if (result != MC_CLIENT_DOWNLOAD_OK)
            return result == MC_CLIENT_DOWNLOAD_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                     : MC_CLIENT_STARTUP_OWNER_FAILED;
    }
    if (call->U_stored) {
        s->app->U = NULL; /* End native view before any borrowed startup dependency. */
        call->U_stored = false;
    }
    if (mc_client_download_classes_release(&call->classes, &call->cleanup) != MC_CLIENT_DOWNLOAD_OK)
        return MC_CLIENT_STARTUP_OWNER_FAILED;
    const mc_client_gl_output_status graphics =
        mc_client_gl_output_retire(&call->diagnostic, &call->graphics_cleanup);
    if (graphics != MC_CLIENT_GL_OUTPUT_OK)
        return graphics == MC_CLIENT_GL_OUTPUT_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                    : MC_CLIENT_STARTUP_GRAPHICS_FAILED;
    call->retired = true;
    return MC_CLIENT_STARTUP_OK;
}
