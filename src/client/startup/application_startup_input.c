#include "application_startup_input.h"
#include "../display/sdk_display_internal.h"
#include "application_startup_internal.h"

#include <stdlib.h>

#define MC_CLASS "net/minecraft/client/Minecraft"

mc_client_startup_status application_startup_input(mc_client_application_startup *startup) {
    if (startup == NULL || startup->app == NULL)
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    if (startup->input != NULL)
        return startup->input->active ? MC_CLIENT_STARTUP_BUSY : MC_CLIENT_STARTUP_INVALID_PHASE;
    client_application *app = startup->app;
    if (app->constructor_phase != MC_CLIENT_APP_READY || app->startup != startup ||
        startup->phase == MC_CLIENT_STARTUP_RETIRING || app->C != NULL ||
        startup->engine_classes == NULL || !mc_client_display_sdk_live(startup->display))
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "startup input live actual owners", NULL, NULL, NULL, 0);
    startup->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 362};
    mc_client_startup_input_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                                        "startup input native call storage", NULL, NULL, NULL, 0);
    startup->input = call;
    startup->failure.input = call;
    const bool outer_active = startup->active;
    startup->active = call->active = true;
    call->last_pc = 362;
    mc_client_sdk_input *input = mc_client_display_inputs(startup->display);
    if (mc_client_sdk_keyboard_create(input, &call->input) != MC_CLIENT_SDK_INPUT_OK)
        goto input_failure;
    call->keyboard_call_completed = true;
    startup->failure.caller.pc = call->last_pc = 365;
    if (mc_client_sdk_mouse_create(input, &call->input) != MC_CLIENT_SDK_INPUT_OK)
        goto input_failure;
    call->mouse_call_completed = true;
    startup->failure.caller.pc = call->last_pc = 369;
    if (mc_client_mouse_helper_allocate(startup->engine_classes, NULL, &call->candidate,
                                        &call->mouse_helper) != MC_CLIENT_MOUSE_HELPER_OK)
        goto helper_failure;
    mc_client_source_canvas *component = app->m; /* Actual live m374, after NEW369. */
    startup->failure.caller.pc = call->last_pc = 377;
    if (mc_client_mouse_helper_construct(call->candidate, component, startup->display, input,
                                         &call->mouse_helper) != MC_CLIENT_MOUSE_HELPER_OK)
        goto helper_failure;
    app->C = call->candidate; /* Actual putfield380, only after constructor return. */
    call->C_stored = true;
    call->last_pc = 380;
    startup->last_startup_store = 380;
    call->active = false;
    startup->active = outer_active;
    startup->failure.caller.pc = 383;
    return application_startup_fail(
        &startup->failure, MC_CLIENT_STARTUP_UNIMPLEMENTED,
        "next unexecuted Controllers.create; no handler/frame/tick body entered", MC_CLASS, "a",
        "()V", 383);
input_failure:
    call->active = false;
    startup->active = outer_active;
    return application_startup_fail(
        &startup->failure, MC_CLIENT_STARTUP_INPUT_FAILED, call->input.operation,
        call->input.site.class_name, call->input.site.method, call->input.site.descriptor,
        call->input.site.pc < 0 ? 0u : (unsigned int)call->input.site.pc);
helper_failure:
    call->active = false;
    startup->active = outer_active;
    return application_startup_fail(
        &startup->failure, MC_CLIENT_STARTUP_INPUT_FAILED, call->mouse_helper.operation,
        call->mouse_helper.site.class_name, call->mouse_helper.site.method,
        call->mouse_helper.site.descriptor,
        call->mouse_helper.site.pc < 0 ? 0u : (unsigned int)call->mouse_helper.site.pc);
}

mc_client_startup_status application_startup_input_retire(mc_client_application_startup *startup) {
    mc_client_startup_input_call *call = startup->input;
    startup->retirement.input = call;
    if (call == NULL || call->retired)
        return MC_CLIENT_STARTUP_OK;
    if (call->active)
        return MC_CLIENT_STARTUP_BUSY;
    client_application *app = startup->app;
    if ((call->C_stored && app->C != call->candidate) || (!call->C_stored && app->C != NULL))
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    const mc_client_mouse_helper_status status =
        mc_client_mouse_helper_release(&call->candidate, &call->retirement);
    if (call->candidate == NULL && call->C_stored) {
        app->C = NULL; /* Native source view ended; no source putfield is invented. */
        call->C_stored = false;
    }
    if (status != MC_CLIENT_MOUSE_HELPER_OK)
        return status == MC_CLIENT_MOUSE_HELPER_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                     : MC_CLIENT_STARTUP_INPUT_FAILED;
    call->retired = true;
    return MC_CLIENT_STARTUP_OK;
}
