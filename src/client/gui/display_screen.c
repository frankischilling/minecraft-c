#include "display_screen.h"
#include "../application_state.h"
#include "loading_internal.h"

#include <stdlib.h>

#define MC_CLASS "net/minecraft/client/Minecraft"

static mc_client_display_screen_status fail(mc_client_display_screen_call *call,
                                            mc_client_display_screen_status status,
                                            const char *operation, unsigned int pc) {
    call->status = status;
    call->operation = operation;
    call->site = (mc_client_screen_site){MC_CLASS, "a", "(Lda;)V", pc};
    return status;
}

static bool menu(mc_client_display_screen_call *call, mc_client_source_da *screen, unsigned int pc,
                 bool *out) {
    *out = false; /* Source instanceof null is false. */
    if (screen == NULL)
        return true;
    mc_client_screen_state state;
    if (mc_client_screen_inspect(screen, &state, &call->screen) != MC_CLIENT_SCREEN_OK) {
        fail(call, MC_CLIENT_DISPLAY_SCREEN_CHILD_FAILED, "source screen dispatch", pc);
        call->child = call->screen.site;
        return false;
    }
    *out = state.is_menu;
    return true;
}

static mc_client_display_screen_status run(mc_client_display_screen_call *call,
                                           mc_client_screen_classes *classes) {
    client_application *app = call->app;
    call->old_screen = app->r;
    if (call->old_screen != NULL) {
        bool ignored;
        if (!menu(call, call->old_screen, 4, &ignored))
            return call->status;
        /* The implemented da/fu classes cannot be ce. Both inherit da.h()V,
         * whose bytecode is the literal return0, with no source field access. */
        call->old_closed = true;
    }
    bool is_menu;
    if (!menu(call, call->target, 26, &is_menu))
        return call->status;
    if (is_menu) {
        call->stats_receiver = app->stats_writer; /* Actual I33, then invokevirtual36. */
        if (call->stats_receiver == NULL)
            return fail(call, MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE, "actual xi.b null receiver",
                        36);
        /* xi.b()V has a literal return0. It does not flush/service its worker. */
        call->stats_b_returned = true;
    }
    call->stats_receiver = app->stats_writer; /* Actual independent I40 read. */
    if (call->stats_receiver == NULL)
        return fail(call, MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE, "actual xi.c null receiver", 43);
    if (mc_client_stats_writer_save(call->stats_receiver, &call->statistics_report,
                                    &call->statistics) != MC_CLIENT_STATS_OK) {
        call->child =
            (mc_client_screen_site){call->statistics.class_name, call->statistics.method,
                                    call->statistics.descriptor, call->statistics.bytecode};
        return fail(call,
                    call->statistics.status == MC_CLIENT_STATS_UNIMPLEMENTED_DEPENDENCY
                        ? MC_CLIENT_DISPLAY_SCREEN_UNIMPLEMENTED
                        : MC_CLIENT_DISPLAY_SCREEN_CHILD_FAILED,
                    call->statistics.operation, 43);
    }
    call->stats_c_returned = true;
    if (call->target == NULL && app->active_world == NULL) {
        if (mc_client_screen_menu_allocate(classes, NULL, &call->fallback, &call->screen) !=
                MC_CLIENT_SCREEN_OK ||
            mc_client_screen_menu_construct(call->fallback, &call->screen) != MC_CLIENT_SCREEN_OK) {
            call->child = call->screen.site;
            return fail(call, MC_CLIENT_DISPLAY_SCREEN_CHILD_FAILED,
                        "actual fallback fu construction", 61);
        }
        call->target = mc_client_screen_menu_base(call->fallback);
    } else if (call->target == NULL) {
        /* Preserve the actual h73/Y76 dependency; no fabricated Player health
         * or death-screen object substitutes for that source branch. */
        return fail(call, MC_CLIENT_DISPLAY_SCREEN_UNIMPLEMENTED,
                    "World null-screen Player/death branch", 76);
    }
    if (!menu(call, call->target, 91, &is_menu))
        return call->status;
    if (is_menu) {
        if (app->v == NULL)
            return fail(call, MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE, "actual uq.b null receiver",
                        101);
        if (mc_client_gui_ingame_clear_messages(app->v, &call->gui) != MC_CLIENT_GUI_INGAME_OK) {
            call->child = (mc_client_screen_site){call->gui.site.class_name, call->gui.site.method,
                                                  call->gui.site.descriptor, call->gui.site.pc};
            return fail(call, MC_CLIENT_DISPLAY_SCREEN_CHILD_FAILED, "actual uq.b message clear",
                        101);
        }
        call->messages_cleared = true;
    }
    app->r = call->target; /* Actual putfield106, before focus/qq/initialization. */
    call->r_stored = true;
    if (call->target == NULL)
        return fail(call, MC_CLIENT_DISPLAY_SCREEN_UNIMPLEMENTED,
                    "actual Minecraft.g focus acquisition", 165);
    /* Actual h(): a false N returns7 before any Player/MouseHelper operation. */
    if (app->N) {
        if (app->h != NULL) {
            call->child = (mc_client_screen_site){MC_CLASS, "h", "()V", 19};
            return fail(call, MC_CLIENT_DISPLAY_SCREEN_UNIMPLEMENTED,
                        "actual LocalPlayer key release", 114);
        }
        app->N = false; /* Actual h24 survives the later reached child stop. */
        if (app->C == NULL) {
            call->child = (mc_client_screen_site){MC_CLASS, "h", "()V", 31};
            return fail(call, MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE,
                        "actual MouseHelper null receiver", 114);
        }
        const mc_client_mouse_helper_status released =
            mc_client_mouse_helper_ungrab(app->C, NULL, &call->mouse);
        if (released != MC_CLIENT_MOUSE_HELPER_OK) {
            call->child = (mc_client_screen_site){
                call->mouse.site.class_name, call->mouse.site.method, call->mouse.site.descriptor,
                call->mouse.site.pc < 0 ? 0u : (unsigned int)call->mouse.site.pc};
            return fail(call,
                        released == MC_CLIENT_MOUSE_HELPER_NULL_REFERENCE
                            ? MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE
                        : released == MC_CLIENT_MOUSE_HELPER_UNIMPLEMENTED_DEPENDENCY
                            ? MC_CLIENT_DISPLAY_SCREEN_UNIMPLEMENTED
                            : MC_CLIENT_DISPLAY_SCREEN_CHILD_FAILED,
                        call->mouse.operation, 114);
        }
    }
    call->focus_released = true;
    if (mc_client_loading_resolution(app, &call->resolution, &call->resolution_error, 133) !=
        MC_CLIENT_LOADING_OK) {
        call->child = (mc_client_screen_site){
            call->resolution_error.child.class_name, call->resolution_error.child.method,
            call->resolution_error.child.descriptor, (unsigned int)call->resolution_error.child.pc};
        return fail(call, MC_CLIENT_DISPLAY_SCREEN_CHILD_FAILED, "actual qq constructor", 133);
    }
    call->resolution_returned = true;
    if (mc_client_screen_initialize(call->target, app, call->resolution.width,
                                    call->resolution.height,
                                    &call->initialization) != MC_CLIENT_SCREEN_OK) {
        call->child = call->initialization.entry;
        return fail(call, MC_CLIENT_DISPLAY_SCREEN_CHILD_FAILED, "actual screen initialization",
                    153);
    }
    call->initialized = true;
    app->w = false;
    call->w_stored = true;
    call->source_returned = true;
    call->site = (mc_client_screen_site){MC_CLASS, "a", "(Lda;)V", 168};
    return MC_CLIENT_DISPLAY_SCREEN_OK;
}

mc_client_display_screen_status mc_client_display_screen_run(client_application *app,
                                                             mc_client_screen_classes *classes,
                                                             mc_client_source_da *argument,
                                                             mc_client_display_screen_call **out) {
    if (app == NULL || classes == NULL || out == NULL || *out != NULL)
        return MC_CLIENT_DISPLAY_SCREEN_ARGUMENT;
    mc_client_display_screen_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return MC_CLIENT_DISPLAY_SCREEN_ALLOCATION;
    *out = call;
    call->app = app;
    call->argument = call->target = argument;
    call->active = true;
    const mc_client_display_screen_status result = run(call, classes);
    call->active = false;
    return result;
}

mc_client_display_screen_status
mc_client_display_screen_retire(mc_client_display_screen_call **slot) {
    if (slot == NULL)
        return MC_CLIENT_DISPLAY_SCREEN_ARGUMENT;
    mc_client_display_screen_call *call = *slot;
    if (call == NULL)
        return MC_CLIENT_DISPLAY_SCREEN_OK;
    if (call->active)
        return MC_CLIENT_DISPLAY_SCREEN_BUSY;
    if (call->fallback != NULL) {
        mc_client_source_da *base = mc_client_screen_menu_base(call->fallback);
        const mc_client_screen_status status =
            mc_client_screen_menu_retire(&call->fallback, &call->screen);
        if (status != MC_CLIENT_SCREEN_OK)
            return status == MC_CLIENT_SCREEN_BUSY ? MC_CLIENT_DISPLAY_SCREEN_BUSY
                                                   : MC_CLIENT_DISPLAY_SCREEN_CHILD_FAILED;
        if (call->r_stored && call->app->r == base)
            call->app->r = NULL; /* Native view ends; no source r rollback. */
    }
    free(call);
    *slot = NULL;
    return MC_CLIENT_DISPLAY_SCREEN_OK;
}
