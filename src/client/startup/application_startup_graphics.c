#include "application_startup_graphics.h"
#include "application_owner_internal.h"
#include "application_startup_input.h"
#include "application_startup_internal.h"

#include <stdlib.h>
#include <string.h>

#define MC_CLASS "net/minecraft/client/Minecraft"

static mc_client_startup_status namespace_thread_failure(mc_client_application_startup *startup,
                                                         const char *operation, int code) {
    application_owner_fail(&startup->failure.owner, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                           operation, NULL, NULL, NULL, 0, code);
    return MC_CLIENT_STARTUP_OWNER_FAILED;
}

/* Associate the selected, already-required System output bootstrap with this
 * same native runtime/class scope. This default constructor has no source
 * output callback, write, flush, GetError or Minecraft clinit operation.
 * Its retained failed candidate is never replaced by a fresh successful one. */
static mc_client_startup_status system_output(mc_client_application_startup *startup,
                                              mc_client_startup_graphics_call *call) {
    mc_client_application_classes *classes = startup->app->source_namespace;
    const int locked = mtx_lock(&classes->mutex);
    if (locked != thrd_success)
        return namespace_thread_failure(startup, "startup System-output namespace lock", locked);
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    }
    if (!classes->system_output_attempted) {
        classes->system_output_attempted = true;
        if (mc_java_system_output_allocate(&classes->system_output,
                                           &classes->system_output_failure) == MC_JAVA_PRINT_OK)
            (void)mc_java_system_output_construct(classes->system_output,
                                                  &classes->system_output_failure);
    }
    call->namespace_error = classes->system_output_failure;
    call->namespace_identity = classes->system_output;
    const mc_client_startup_status status =
        call->namespace_error.status == MC_JAVA_PRINT_OK && call->namespace_identity != NULL
            ? MC_CLIENT_STARTUP_OK
            : MC_CLIENT_STARTUP_GRAPHICS_FAILED;
    const int unlocked = mtx_unlock(&classes->mutex);
    if (unlocked != thrd_success)
        return namespace_thread_failure(startup, "startup System-output namespace unlock",
                                        unlocked);
    return status;
}

static mc_client_startup_status label(mc_client_application_startup *startup,
                                      mc_client_startup_graphics_call *call, size_t index) {
    static const uint16_t before[] = {'P', 'r', 'e', ' ', 's', 't', 'a', 'r', 't', 'u', 'p'};
    static const uint16_t after[] = {'S', 't', 'a', 'r', 't', 'u', 'p'};
    const uint16_t *units = index == 0 ? before : after;
    const size_t length =
        index == 0 ? sizeof(before) / sizeof(before[0]) : sizeof(after) / sizeof(after[0]);
    mc_client_application_classes *classes = startup->app->source_namespace;
    const int locked = mtx_lock(&classes->mutex);
    if (locked != thrd_success)
        return namespace_thread_failure(startup, "startup GL-label namespace lock", locked);
    mc_client_startup_status status = MC_CLIENT_STARTUP_OK;
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY)
        status = MC_CLIENT_STARTUP_INVALID_PHASE;
    else if (classes->startup_gl_labels[index] == NULL) {
        startup->failure.player_string =
            mc_player_string_create(units, length, &classes->startup_gl_labels[index]);
        if (startup->failure.player_string != MC_PLAYER_OK)
            status = MC_CLIENT_STARTUP_STRING_FAILED;
    }
    if (status == MC_CLIENT_STARTUP_OK)
        call->labels[index] = classes->startup_gl_labels[index];
    const int unlocked = mtx_unlock(&classes->mutex);
    if (unlocked != thrd_success)
        return namespace_thread_failure(startup, "startup GL-label namespace unlock", unlocked);
    return status;
}

static void snapshot_diagnostic(mc_client_startup_graphics_call *call, size_t index) {
    mc_client_gl_output_state state;
    if (!mc_client_gl_output_inspect(call->diagnostics[index], &state) || state.error == NULL)
        return;
    mc_client_startup_diagnostic_snapshot *snapshot = &call->snapshots[index];
    snapshot->valid = true;
    snapshot->error = *state.error;
    snapshot->captured_receivers = state.captured_receivers;
    snapshot->normal_println_returns = state.normal_println_returns;
    for (size_t receiver = 0; receiver < 3; ++receiver)
        if (state.receivers[receiver] != NULL)
            snapshot->receiver_valid[receiver] = mc_java_print_stream_inspect(
                (mc_java_print_stream *)state.receivers[receiver], &snapshot->receivers[receiver]);
}

static mc_client_startup_status diagnostic(mc_client_application_startup *startup,
                                           mc_client_startup_graphics_call *call, size_t index) {
    startup->failure.caller.pc = index == 0 ? 395u : 464u;
    mc_client_startup_status status = label(startup, call, index);
    if (status != MC_CLIENT_STARTUP_OK)
        return application_startup_fail(&startup->failure, status,
                                        "actual Minecraft diagnostic argument literal", NULL, NULL,
                                        NULL, 0);
    startup->failure.caller.pc = index == 0 ? 397u : 466u;
    if (mc_client_gl_output_allocate(&call->diagnostics[index], &call->allocation_error) !=
        MC_CLIENT_GL_OUTPUT_OK)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_GRAPHICS_FAILED,
                                        "startup diagnostic native retained call allocation", NULL,
                                        NULL, NULL, 0);
    const mc_client_gl_output_status result = mc_client_gl_output_run(
        call->diagnostics[index], startup->display, call->namespace_identity, call->labels[index]);
    snapshot_diagnostic(call, index);
    if (result != MC_CLIENT_GL_OUTPUT_OK) {
        if (call->snapshots[index].valid) {
            const mc_client_gl_output_error *error = &call->snapshots[index].error;
            return application_startup_fail(
                &startup->failure, MC_CLIENT_STARTUP_GRAPHICS_FAILED, error->operation,
                error->site.class_name, error->site.method, error->site.descriptor, error->site.pc);
        }
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_GRAPHICS_FAILED,
                                        "startup diagnostic failed call inspection", NULL, NULL,
                                        NULL, 0);
    }
    ++call->diagnostic_calls_completed;
    call->last_pc = startup->failure.caller.pc;
    return MC_CLIENT_STARTUP_OK;
}

static bool setup_operation(mc_client_display *display, size_t index,
                            mc_client_display_error *error) {
    /* Actual MC.a403..460, with no second GL error query or extra state reset. */
    switch (index) {
        case 0:
            return mc_client_display_source_texture_enable(display, true, error);
        case 1:
            return mc_client_display_source_shade_model(display, 0x1d01, error);
        case 2:
            return mc_client_display_source_clear_depth(display, 1.0, error);
        case 3:
            return mc_client_display_source_depth_test(display, true, error);
        case 4:
            return mc_client_display_source_depth_function(display, 0x0203, error);
        case 5:
            return mc_client_display_source_alpha_test(display, true, error);
        case 6: {
            const uint32_t bits = UINT32_C(0x3dcccccd);
            float reference;
            memcpy(&reference, &bits, sizeof(reference));
            return mc_client_display_source_alpha_function(display, 0x0204, reference, error);
        }
        case 7:
            return mc_client_display_source_cull_face(display, 0x0405, error);
        case 8:
            return mc_client_display_source_matrix_mode(display, 0x1701, error);
        case 9:
            return mc_client_display_source_load_identity(display, error);
        default:
            return mc_client_display_source_matrix_mode(display, 0x1700, error);
    }
}

mc_client_startup_status application_startup_graphics(mc_client_application_startup *startup) {
    if (startup == NULL || startup->app == NULL)
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    if (startup->graphics != NULL)
        return startup->graphics->active ? MC_CLIENT_STARTUP_BUSY : MC_CLIENT_STARTUP_INVALID_PHASE;
    client_application *app = startup->app;
    if (app->constructor_phase != MC_CLIENT_APP_READY || app->startup != startup ||
        startup->phase == MC_CLIENT_STARTUP_RETIRING || app->source_namespace == NULL ||
        startup->input == NULL || !startup->input->controllers_call_completed ||
        !startup->input->C_stored || app->C != startup->input->candidate)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "startup graphics actual prior input owners", NULL, NULL,
                                        NULL, 0);
    mc_client_startup_graphics_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                                        "startup graphics native invocation storage", NULL, NULL,
                                        NULL, 0);
    startup->graphics = call;
    startup->failure.graphics = call;
    const bool outer_active = startup->active;
    startup->active = call->active = true;
    startup->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 394};
    mc_client_startup_status status = system_output(startup, call);
    if (status != MC_CLIENT_STARTUP_OK) {
        status = application_startup_fail(&startup->failure, status,
                                          "startup canonical native System-output prerequisite",
                                          NULL, NULL, NULL, 0);
        goto done;
    }
    call->entered = true;
    status = diagnostic(startup, call, 0);
    if (status != MC_CLIENT_STARTUP_OK)
        goto done;
    static const unsigned int pcs[] = {403, 409, 413, 419, 425, 431, 439, 445, 451, 454, 460};
    for (size_t index = 0; index < sizeof(pcs) / sizeof(pcs[0]); ++index) {
        startup->failure.caller.pc = pcs[index];
        if (!setup_operation(startup->display, index, &call->display)) {
            status = application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_GRAPHICS_FAILED,
                                              call->display.operation, NULL, NULL, NULL, 0);
            goto done;
        }
        ++call->setup_calls_completed;
        call->last_pc = pcs[index];
    }
    call->setup_completed = true;
    status = diagnostic(startup, call, 1);
    if (status == MC_CLIENT_STARTUP_OK) {
        startup->failure.caller.pc = 470;
        status = application_startup_fail(
            &startup->failure, MC_CLIENT_STARTUP_UNIMPLEMENTED,
            "next unexecuted cx allocation; no later constructor/frame/tick entered", MC_CLASS, "a",
            "()V", 470);
    }
done:
    call->active = false;
    startup->active = outer_active;
    return status;
}

mc_client_startup_status
application_startup_graphics_retire(mc_client_application_startup *startup) {
    mc_client_startup_graphics_call *call = startup->graphics;
    startup->retirement.graphics = call;
    if (call == NULL || call->retired)
        return MC_CLIENT_STARTUP_OK;
    if (call->active)
        return MC_CLIENT_STARTUP_BUSY;
    /* Both successful and failed consumed-query calls retire before Display
     * and the owning class namespace. Cleanup never repeats a source call. */
    for (size_t remaining = 2; remaining != 0; --remaining) {
        const mc_client_gl_output_status status =
            mc_client_gl_output_retire(&call->diagnostics[remaining - 1], &call->cleanup_error);
        if (status != MC_CLIENT_GL_OUTPUT_OK)
            return status == MC_CLIENT_GL_OUTPUT_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                      : MC_CLIENT_STARTUP_GRAPHICS_FAILED;
    }
    call->namespace_identity = NULL;
    call->labels[0] = call->labels[1] = NULL;
    call->retired = true;
    return MC_CLIENT_STARTUP_OK;
}
