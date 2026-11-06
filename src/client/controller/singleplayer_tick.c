#include "mc/client/controller/singleplayer_tick.h"

#include "../application_state.h"
#include "mc/client/sound/sound_manager.h"
#include "singleplayer_internal.h"

#include <stdlib.h>
#include <string.h>

struct mc_singleplayer_tick_call {
    mc_singleplayer_tick_state state;
    mc_singleplayer_tick_error error;
    mc_sound_error sound;
    mc_sound_manager_report sound_report;
};

static mc_singleplayer_tick_status result(mc_singleplayer_tick_error *out,
                                          mc_singleplayer_tick_status status, const char *operation,
                                          bool native_policy) {
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->status = status;
        out->operation = operation;
        out->native_policy = native_policy;
    }
    return status;
}

static mc_singleplayer_tick_status admission(mc_client_singleplayer_controller *c,
                                             mc_singleplayer_tick_error *error) {
    if (c == NULL)
        return result(error, MC_SINGLEPLAYER_TICK_INVALID_ARGUMENT,
                      "singleplayer tick/reset native receiver", true);
    if (c->fields.entered)
        return result(error, MC_SINGLEPLAYER_TICK_BUSY,
                      "singleplayer tick/reset entered controller", true);
    if (c->fields.phase != MC_SINGLEPLAYER_CONTROLLER_READY)
        return result(error, MC_SINGLEPLAYER_TICK_INVALID_PHASE,
                      "singleplayer tick/reset READY controller", true);
    return MC_SINGLEPLAYER_TICK_OK;
}

static void source_result(mc_singleplayer_tick_call *call, mc_singleplayer_tick_status status,
                          const char *operation, unsigned int pc, bool native_policy) {
    call->state.status = status;
    call->state.source_pc = pc;
    (void)result(&call->error, status, operation, native_policy);
    call->error.site = (mc_player_site){"os", "c", "()V", pc};
}

mc_singleplayer_tick_status
mc_client_singleplayer_controller_tick_begin(mc_client_singleplayer_controller *c,
                                             mc_singleplayer_tick_call **out,
                                             mc_singleplayer_tick_error *error) {
    if (out == NULL || *out != NULL)
        return result(error, MC_SINGLEPLAYER_TICK_INVALID_ARGUMENT,
                      "singleplayer tick owned call output", true);
    mc_singleplayer_tick_status status = admission(c, error);
    if (status != MC_SINGLEPLAYER_TICK_OK)
        return status;
    if (c->fields.native_holds == SIZE_MAX)
        return result(error, MC_SINGLEPLAYER_TICK_LIMIT,
                      "singleplayer tick controller occurrence limit", true);
    mc_singleplayer_tick_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return result(error, MC_SINGLEPLAYER_TICK_ALLOCATION_FAILED,
                      "singleplayer tick retained diagnostic allocation", true);
    const mc_singleplayer_controller_status held = mc_client_singleplayer_controller_retain(c);
    if (held != MC_SINGLEPLAYER_CONTROLLER_OK) {
        free(call);
        status = result(error, MC_SINGLEPLAYER_TICK_CONTROLLER_FAILED,
                        "singleplayer tick controller occurrence", true);
        if (error != NULL)
            error->controller = held;
        return status;
    }
    call->state.controller = c;
    call->state.error = &call->error;
    call->state.active = true;
    c->fields.entered = true;
    *out = call;

    /* Actual f2 -> g5. Avoid any arithmetic or NaN canonicalization. */
    memcpy(&c->fields.g, &c->fields.f, sizeof(c->fields.g));
    call->state.g_stored = true;
    call->state.source_pc = 5;
    call->state.app = c->fields.a; /* Actual getfield ob.a9. */
    if (call->state.app == NULL)
        source_result(call, MC_SINGLEPLAYER_TICK_SOURCE_NULL_REFERENCE,
                      "actual os.c null App at B read", 12, false);
    else {
        call->state.sound_manager = call->state.app->B; /* Actual getfield App.B12. */
        if (call->state.sound_manager == NULL)
            source_result(call, MC_SINGLEPLAYER_TICK_SOURCE_NULL_REFERENCE,
                          "actual os.c null SoundManager invocation", 15, false);
        else {
            call->state.source_pc = 15;
            call->state.sound_called = true;
            call->state.sound = &call->sound;
            call->state.sound_report = &call->sound_report;
            call->sound.source_pc = -1;
            call->sound_report.source_pc = -1;
            const mc_sound_status sound_status = mc_sound_manager_music_tick(
                call->state.sound_manager, &call->sound_report, &call->sound);
            if (sound_status != MC_SOUND_OK) {
                source_result(call, MC_SINGLEPLAYER_TICK_SOUND_FAILED,
                              "actual os.c SoundManager music tick", 15,
                              sound_status != MC_SOUND_SOURCE_EXCEPTION);
                call->error.sound = &call->sound;
                call->error.sound_report = &call->sound_report;
                if (call->sound.source_pc >= 0)
                    call->error.child =
                        (mc_player_site){"yo", "c", "()V", (unsigned int)call->sound.source_pc};
            } else {
                call->state.sound_returned = true;
                call->state.source_returned = true;
                source_result(call, MC_SINGLEPLAYER_TICK_OK, NULL, 18, false);
            }
        }
    }
    call->state.active = false;
    c->fields.entered = false;
    if (error != NULL)
        *error = call->error;
    return call->state.status;
}

bool mc_client_singleplayer_controller_tick_inspect(const mc_singleplayer_tick_call *call,
                                                    mc_singleplayer_tick_state *out) {
    if (call == NULL || out == NULL)
        return false;
    *out = call->state;
    return true;
}

mc_singleplayer_tick_status
mc_client_singleplayer_controller_tick_retire(mc_singleplayer_tick_call **slot,
                                              mc_singleplayer_tick_error *error) {
    if (slot == NULL)
        return result(error, MC_SINGLEPLAYER_TICK_INVALID_ARGUMENT,
                      "singleplayer tick retirement slot", true);
    mc_singleplayer_tick_call *call = *slot;
    if (call == NULL)
        return result(error, MC_SINGLEPLAYER_TICK_OK, NULL, false);
    if (call->state.active || call->state.controller->fields.entered)
        return result(error, MC_SINGLEPLAYER_TICK_BUSY,
                      "singleplayer tick retirement during entered operation", true);
    const mc_singleplayer_controller_status released =
        mc_client_singleplayer_controller_release(call->state.controller);
    if (released != MC_SINGLEPLAYER_CONTROLLER_OK) {
        const mc_singleplayer_tick_status status =
            result(error, MC_SINGLEPLAYER_TICK_CONTROLLER_FAILED,
                   "singleplayer tick controller occurrence release", true);
        if (error != NULL)
            error->controller = released;
        return status;
    }
    free(call);
    *slot = NULL;
    return result(error, MC_SINGLEPLAYER_TICK_OK, NULL, false);
}

mc_singleplayer_tick_status
mc_client_singleplayer_controller_reset(mc_client_singleplayer_controller *c,
                                        mc_singleplayer_tick_error *error) {
    const mc_singleplayer_tick_status status = admission(c, error);
    if (status != MC_SINGLEPLAYER_TICK_OK)
        return status;
    c->fields.entered = true;
    c->fields.f = 0.0f; /* Actual fconst_0/putfield2. */
    c->fields.i = 0;    /* Actual iconst_0/putfield7. */
    c->fields.entered = false;
    (void)result(error, MC_SINGLEPLAYER_TICK_OK, NULL, false);
    if (error != NULL)
        error->site = (mc_player_site){"os", "a", "()V", 10};
    return MC_SINGLEPLAYER_TICK_OK;
}
