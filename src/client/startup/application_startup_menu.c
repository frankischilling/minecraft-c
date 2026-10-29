#include "application_startup_menu.h"
#include "application_owner_internal.h"
#include "application_startup_gui_ingame.h"
#include "application_startup_internal.h"

#include <stdlib.h>

#define MC_CLASS "net/minecraft/client/Minecraft"

static mc_client_startup_status screen_failure(mc_client_application_startup *s) {
    const mc_client_screen_error *e = &s->menu->error;
    return application_startup_fail(
        &s->failure,
        e->status == MC_CLIENT_SCREEN_BUSY ? MC_CLIENT_STARTUP_BUSY
                                           : MC_CLIENT_STARTUP_OWNER_FAILED,
        e->operation, e->site.class_name, e->site.method, e->site.descriptor, e->site.pc);
}

static mc_client_startup_status bind_classes(mc_client_application_startup *s) {
    mc_client_application_classes *classes = s->app->source_namespace;
    if (mtx_lock(&classes->mutex) != thrd_success)
        return MC_CLIENT_STARTUP_OWNER_FAILED;
    mc_client_startup_status status = MC_CLIENT_STARTUP_OK;
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY)
        status = MC_CLIENT_STARTUP_INVALID_PHASE;
    else if (classes->startup_screen_classes == NULL) {
        if (mc_client_screen_classes_create(s->app->profile, s->services.texture_resources,
                                            s->app->registry, &classes->startup_screen_classes,
                                            &s->menu->error) != MC_CLIENT_SCREEN_OK)
            status = screen_failure(s);
        else {
            classes->startup_screen_profile = s->app->profile;
            classes->startup_screen_resources = s->services.texture_resources;
            classes->startup_screen_registry = s->app->registry;
        }
    } else if (classes->startup_screen_profile != s->app->profile ||
               classes->startup_screen_resources != s->services.texture_resources ||
               classes->startup_screen_registry != s->app->registry)
        status = MC_CLIENT_STARTUP_INVALID_PHASE;
    if (status == MC_CLIENT_STARTUP_OK)
        s->menu->classes = classes->startup_screen_classes;
    if (mtx_unlock(&classes->mutex) != thrd_success)
        status = MC_CLIENT_STARTUP_OWNER_FAILED;
    if (status != MC_CLIENT_STARTUP_OK && s->failure.status == MC_CLIENT_STARTUP_OK)
        return application_startup_fail(&s->failure, status, "actual screen namespace association",
                                        NULL, NULL, NULL, 0);
    return status;
}

mc_client_startup_status application_startup_menu(mc_client_application_startup *s) {
    if (s == NULL || s->app == NULL || s->menu != NULL || s->gui_ingame == NULL ||
        !s->gui_ingame->v_stored || s->app->startup != s || s->phase == MC_CLIENT_STARTUP_RETIRING)
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    if (s->app->ab != NULL) {
        s->failure.caller.pc = 716;
        return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_UNIMPLEMENTED,
                                        "actual connecting-screen constructor", MC_CLASS, "a",
                                        "()V", 716);
    }
    s->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 739};
    mc_client_startup_menu_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return application_startup_fail(&s->failure, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                                        "startup menu call storage", NULL, NULL, NULL, 0);
    s->menu = call;
    call->active = true;
    mc_client_startup_status status = bind_classes(s);
    if (status != MC_CLIENT_STARTUP_OK)
        goto done;
    if (mc_client_screen_menu_allocate(call->classes, NULL, &call->candidate, &call->error) !=
        MC_CLIENT_SCREEN_OK) {
        status = screen_failure(s);
        goto done;
    }
    call->allocated = true;
    call->last_pc = 739;
    s->failure.caller.pc = 743;
    if (mc_client_screen_menu_construct(call->candidate, &call->error) != MC_CLIENT_SCREEN_OK) {
        status = screen_failure(s);
        goto done;
    }
    call->constructed = true;
    call->last_pc = 743;
    s->failure.caller.pc = 746;
    const mc_client_display_screen_status displayed = mc_client_display_screen_run(
        s->app, call->classes, mc_client_screen_menu_base(call->candidate), &call->display);
    call->r_stored = call->display != NULL && call->display->r_stored;
    if (displayed != MC_CLIENT_DISPLAY_SCREEN_OK) {
        const mc_client_screen_site child = call->display == NULL ? (mc_client_screen_site){0}
                                            : call->display->child.class_name != NULL
                                                ? call->display->child
                                                : call->display->site;
        status = application_startup_fail(
            &s->failure,
            displayed == MC_CLIENT_DISPLAY_SCREEN_UNIMPLEMENTED ? MC_CLIENT_STARTUP_UNIMPLEMENTED
                                                                : MC_CLIENT_STARTUP_OWNER_FAILED,
            call->display == NULL ? "displayGuiScreen native call storage"
                                  : call->display->operation,
            child.class_name, child.method, child.descriptor, child.pc);
        goto done;
    }
    call->last_pc = 749;
    call->source_startup_returned = true;
    s->failure.caller = (mc_client_application_owner_site){MC_CLASS, "run", "()V", 32};
    status = application_startup_fail(&s->failure, MC_CLIENT_STARTUP_UNIMPLEMENTED,
                                      "next actual run clock/frame loop is not connected", MC_CLASS,
                                      "run", "()V", 32);
done:
    call->active = false;
    return status;
}

mc_client_startup_status application_startup_menu_retire(mc_client_application_startup *s) {
    mc_client_startup_menu_call *call = s->menu;
    if (call == NULL)
        return s->app->r == NULL ? MC_CLIENT_STARTUP_OK : MC_CLIENT_STARTUP_BUSY;
    if (call->active)
        return MC_CLIENT_STARTUP_BUSY;
    if (call->retired)
        return s->app->r == NULL ? MC_CLIENT_STARTUP_OK : MC_CLIENT_STARTUP_INVALID_PHASE;
    mc_client_source_da *base = mc_client_screen_menu_base(call->candidate);
    if (call->r_stored ? s->app->r != base : s->app->r != NULL)
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    if (call->candidate != NULL) {
        mc_client_screen_state state;
        if (mc_client_screen_inspect(base, &state, &call->cleanup) != MC_CLIENT_SCREEN_OK ||
            state.holds != 1)
            return MC_CLIENT_STARTUP_BUSY;
    }
    const mc_client_display_screen_status displayed =
        mc_client_display_screen_retire(&call->display);
    if (displayed != MC_CLIENT_DISPLAY_SCREEN_OK)
        return displayed == MC_CLIENT_DISPLAY_SCREEN_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                          : MC_CLIENT_STARTUP_OWNER_FAILED;
    const mc_client_screen_status status =
        mc_client_screen_menu_retire(&call->candidate, &call->cleanup);
    if (status != MC_CLIENT_SCREEN_OK)
        return status == MC_CLIENT_SCREEN_BUSY ? MC_CLIENT_STARTUP_BUSY
                                               : MC_CLIENT_STARTUP_OWNER_FAILED;
    if (call->r_stored) {
        s->app->r = NULL; /* Native view ended after actual owner consumption. */
        call->r_stored = false;
    }
    call->classes = NULL;
    call->retired = true;
    return MC_CLIENT_STARTUP_OK;
}
