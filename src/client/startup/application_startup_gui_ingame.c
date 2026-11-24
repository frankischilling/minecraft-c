#include "application_startup_gui_ingame.h"
#include "application_owner_internal.h"
#include "application_startup_internal.h"
#include "application_startup_resources.h"

#include <stdlib.h>

#define MC_CLASS "net/minecraft/client/Minecraft"

static mc_client_startup_status failure(mc_client_application_startup *s) {
    const mc_client_gui_ingame_error *e = &s->gui_ingame->error;
    return application_startup_fail(
        &s->failure,
        e->status == MC_CLIENT_GUI_INGAME_BUSY ? MC_CLIENT_STARTUP_BUSY
                                               : MC_CLIENT_STARTUP_OWNER_FAILED,
        e->operation, e->site.class_name, e->site.method, e->site.descriptor, e->site.pc);
}

static mc_client_startup_status bind_classes(mc_client_application_startup *s) {
    mc_client_startup_gui_ingame_call *call = s->gui_ingame;
    mc_client_application_classes *classes = s->app->source_namespace;
    if (mtx_lock(&classes->mutex) != thrd_success)
        return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_OWNER_FAILED,
                                        "GuiIngame namespace association lock", NULL, NULL, NULL,
                                        0);
    mc_client_startup_status status = MC_CLIENT_STARTUP_OK;
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY ||
        s->app->render_classes == NULL || s->app->profile == NULL)
        status = application_startup_fail(&s->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                          "GuiIngame actual App model/profile namespace", NULL,
                                          NULL, NULL, 0);
    else if (classes->startup_gui_ingame_classes == NULL) {
        /* Cold native binding has no uq/bb/Random source effects. Actual NEW697
         * below performs the canonical first active use before RAW allocation. */
        if (mc_client_gui_ingame_classes_create(s->app->render_classes, s->app->profile, 0,
                                                &classes->startup_gui_ingame_classes,
                                                &call->error) != MC_CLIENT_GUI_INGAME_OK)
            status = failure(s);
        else {
            classes->startup_gui_ingame_models = s->app->render_classes;
            classes->startup_gui_ingame_profile = s->app->profile;
        }
    } else if (classes->startup_gui_ingame_models != s->app->render_classes ||
               classes->startup_gui_ingame_profile != s->app->profile)
        status = application_startup_fail(&s->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                          "GuiIngame same actual class model/profile association",
                                          NULL, NULL, NULL, 0);
    call->classes = classes->startup_gui_ingame_classes;
    if (mtx_unlock(&classes->mutex) != thrd_success)
        return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_OWNER_FAILED,
                                        "GuiIngame namespace association unlock", NULL, NULL, NULL,
                                        0);
    return status;
}

mc_client_startup_status application_startup_gui_ingame(mc_client_application_startup *s) {
    if (s == NULL || s->app == NULL || s->gui_ingame != NULL || s->app->v != NULL ||
        s->app->startup != s || s->app->constructor_phase != MC_CLIENT_APP_READY ||
        s->phase == MC_CLIENT_STARTUP_RETIRING || s->resources == NULL ||
        !s->resources->post_returned)
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    s->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 697};
    mc_client_startup_gui_ingame_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                                        "GuiIngame retained startup call storage", NULL, NULL, NULL,
                                        0);
    s->gui_ingame = call;
    call->active = true;
    mc_client_startup_status status = bind_classes(s);
    if (status != MC_CLIENT_STARTUP_OK)
        goto done;
    call->last_pc = 697;
    if (mc_client_gui_ingame_allocate(call->classes, NULL, &call->candidate, &call->error) !=
        MC_CLIENT_GUI_INGAME_OK) {
        status = failure(s);
        goto done;
    }
    call->allocated = true;
    s->failure.caller.pc = 702;
    if (mc_client_gui_ingame_construct(call->candidate, s->app, &call->error) !=
        MC_CLIENT_GUI_INGAME_OK) {
        status = failure(s);
        goto done;
    }
    call->constructor_returned = true;
    call->last_pc = 702;
    s->failure.caller.pc = 705;
    if (s->app->v != NULL) {
        status =
            application_startup_fail(&s->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                     "GuiIngame foreign v publication guard", NULL, NULL, NULL, 0);
        goto done;
    }
    s->app->v = call->candidate;
    call->v_stored = true;
    call->last_pc = 705;
    s->last_startup_store = 705;
    status = application_startup_menu(s);

done:
    call->active = false;
    return status;
}

mc_client_startup_status application_startup_gui_ingame_retire(mc_client_application_startup *s) {
    mc_client_startup_gui_ingame_call *call = s->gui_ingame;
    if (call == NULL)
        return s->app->v == NULL ? MC_CLIENT_STARTUP_OK : MC_CLIENT_STARTUP_BUSY;
    if (call->active)
        return MC_CLIENT_STARTUP_BUSY;
    if (call->retired)
        return s->app->v == NULL ? MC_CLIENT_STARTUP_OK : MC_CLIENT_STARTUP_INVALID_PHASE;
    if (call->v_stored ? s->app->v != call->candidate : s->app->v != NULL)
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    const mc_client_gui_ingame_status status =
        mc_client_gui_ingame_retire(&call->candidate, &call->cleanup);
    if (status != MC_CLIENT_GUI_INGAME_OK)
        return status == MC_CLIENT_GUI_INGAME_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                   : MC_CLIENT_STARTUP_OWNER_FAILED;
    if (call->v_stored) {
        s->app->v = NULL; /* End native view after the owned occurrence was consumed. */
        call->v_stored = false;
    }
    call->classes = NULL;
    call->retired = true;
    return MC_CLIENT_STARTUP_OK;
}
