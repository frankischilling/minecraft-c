#include "application_startup_effects.h"
#include "../application_access.h"
#include "../texture_effects/lava_construction.h"
#include "application_owner_internal.h"
#include "application_startup_graphics.h"
#include "application_startup_internal.h"

#include <stdlib.h>

#define MC_CLASS "net/minecraft/client/Minecraft"

static mc_client_startup_status failure(mc_client_application_startup *startup,
                                        mc_client_startup_status status, const char *operation,
                                        const char *child, const char *method,
                                        const char *descriptor, unsigned int pc) {
    return application_startup_fail(&startup->failure, status, operation, child, method, descriptor,
                                    pc);
}

static mc_client_startup_status cx_namespace(mc_client_application_startup *startup,
                                             mc_client_startup_effects_call *call) {
    mc_client_application_classes *classes = startup->app->source_namespace;
    const int locked = mtx_lock(&classes->mutex);
    if (locked != thrd_success) {
        call->cx.status = MC_CLIENT_CX_THREAD_FAILED;
        call->cx.operation = "MC cx association native lock";
        call->cx.native_thread_error = locked;
        return failure(startup, MC_CLIENT_STARTUP_CX_FAILED, call->cx.operation, NULL, NULL, NULL,
                       0);
    }
    mc_client_startup_status status = MC_CLIENT_STARTUP_OK;
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        status = MC_CLIENT_STARTUP_INVALID_PHASE;
    } else {
        if (!classes->startup_cx_attempted) {
            classes->startup_cx_attempted = true;
            (void)mc_client_cx_classes_create(&classes->startup_cx_classes,
                                              &classes->cx_service_failure);
        }
        call->cx_classes = classes->startup_cx_classes;
        call->cx = classes->cx_service_failure;
        if (call->cx_classes == NULL || call->cx.status != MC_CLIENT_CX_OK)
            status = MC_CLIENT_STARTUP_CX_FAILED;
    }
    const int unlocked = mtx_unlock(&classes->mutex);
    if (unlocked != thrd_success) {
        call->cx.status = MC_CLIENT_CX_THREAD_FAILED;
        call->cx.operation = "MC cx association native unlock";
        call->cx.native_thread_error = unlocked;
        status = MC_CLIENT_STARTUP_CX_FAILED;
    }
    if (status != MC_CLIENT_STARTUP_OK)
        return failure(startup, status, "MC cold cx namespace association", NULL, NULL, NULL, 0);
    return status;
}

static mc_client_startup_status construct_cx(mc_client_application_startup *startup,
                                             mc_client_startup_effects_call *call) {
    startup->failure.caller.pc = 470;
    mc_client_startup_status status = cx_namespace(startup, call);
    if (status != MC_CLIENT_STARTUP_OK)
        return status;
    if (mc_client_cx_allocate(call->cx_classes, &call->cx_candidate, &call->cx) != MC_CLIENT_CX_OK)
        return failure(startup, MC_CLIENT_STARTUP_CX_FAILED, call->cx.operation,
                       call->cx.source_class, call->cx.source_method, call->cx.source_descriptor,
                       call->cx.source_pc);
    call->last_pc = 470;
    startup->failure.caller.pc = 474;
    if (mc_client_cx_construct(call->cx_candidate, &call->cx) != MC_CLIENT_CX_OK)
        return failure(startup, MC_CLIENT_STARTUP_CX_FAILED, call->cx.operation,
                       call->cx.source_class, call->cx.source_method, call->cx.source_descriptor,
                       call->cx.source_pc);
    call->last_pc = 474;
    startup->failure.caller.pc = 477;
    if (startup->app->S != NULL)
        return failure(startup, MC_CLIENT_STARTUP_INVALID_PHASE,
                       "native S publication refuses an unrelated owned source view", NULL, NULL,
                       NULL, 0);
    startup->app->S = call->cx_candidate; /* Actual putfield477, same occurrence. */
    startup->last_startup_store = 477;
    call->S_stored = true;
    call->last_pc = 477;
    return MC_CLIENT_STARTUP_OK;
}

static mc_client_startup_status bind_sound(mc_client_application_startup *startup,
                                           mc_client_startup_effects_call *call) {
    call->sound = startup->app->B; /* Actual B481, before z485. */
    call->sound_settings = startup->app->settings;
    startup->failure.caller.pc = 488;
    call->sound_entered = true;
    if (call->sound == NULL) {
        call->null_sound_receiver = true;
        return failure(startup, MC_CLIENT_STARTUP_SOUND_FAILED,
                       "actual null B receiver at MC invoke488", MC_CLASS, "a", "()V", 488);
    }
    const mc_sound_status result = mc_sound_manager_bind(call->sound, call->sound_settings, NULL,
                                                         &call->sound_report, &call->sound_error);
    /* bind can retain z before a native setup failure. Inspect the live real
     * manager now; do not infer that failure rolled this source store back. */
    mc_sound_manager_state state;
    if ((result == MC_SOUND_OK || call->sound_report.source_pc != 0) &&
        mc_sound_manager_read_state(call->sound, &state) && state.settings == call->sound_settings)
        call->sound_settings_borrow = true;
    if (result != MC_SOUND_OK)
        return failure(
            startup, MC_CLIENT_STARTUP_SOUND_FAILED, call->sound_error.operation, "yo", "a",
            "(Lkv;)V",
            call->sound_report.source_pc < 0 ? 0u : (unsigned int)call->sound_report.source_pc);
    call->sound_returned = true;
    call->last_pc = 488;
    /* Native option-action wiring only after the real source bind. Neither
     * this hook nor its later detach replays setup/save/source cleanup. */
    if (mc_game_settings_attach_sound_manager(call->sound_settings, call->sound,
                                              &call->sound_hook_error) != MC_GAME_SETTINGS_OK)
        return failure(startup, MC_CLIENT_STARTUP_SOUND_FAILED, call->sound_hook_error.operation,
                       NULL, NULL, NULL, 0);
    call->sound_hook_attached = true;
    return MC_CLIENT_STARTUP_OK;
}

static void snapshot(mc_client_startup_effect_call *entry) {
    mc_texture_effect_state state;
    if (mc_client_texture_effect_inspect(entry->argument, &state)) {
        entry->snapshot_valid = true;
        entry->observed_phase = state.phase;
        entry->observed_holds = state.native_holds;
        entry->observed_counter = state.counter;
        entry->observed_last_store = state.last_store_pc;
    }
}

static mc_client_startup_status registration(mc_client_application_startup *startup,
                                             mc_client_startup_effects_call *call, size_t index) {
    static const unsigned int receiver_pc[] = {492, 503, 514, 528, 543, 558, 572, 586, 601};
    static const unsigned int new_pc[] = {0, 0, 517, 531, 546, 561, 575, 589, 604};
    static const unsigned int ctor_pc[] = {0, 0, 521, 536, 551, 565, 579, 594, 609};
    static const unsigned int invoke_pc[] = {499, 510, 524, 539, 554, 568, 582, 597, 612};
    static const mc_texture_effect_kind kinds[] = {
        MC_TEXTURE_EFFECT_LAVA,         MC_TEXTURE_EFFECT_WATER, MC_TEXTURE_EFFECT_PORTAL,
        MC_TEXTURE_EFFECT_COMPASS,      MC_TEXTURE_EFFECT_WATCH, MC_TEXTURE_EFFECT_FLOWING_WATER,
        MC_TEXTURE_EFFECT_FLOWING_LAVA, MC_TEXTURE_EFFECT_FIRE,  MC_TEXTURE_EFFECT_FIRE};
    mc_client_startup_effect_call *entry = &call->effects[index];
    entry->receiver_pc = receiver_pc[index];
    entry->new_pc = new_pc[index];
    entry->constructor_pc = ctor_pc[index];
    entry->invocation_pc = invoke_pc[index];
    entry->kind = kinds[index];
    startup->failure.caller.pc = entry->receiver_pc;
    /* Capture p BEFORE evaluating each argument/new/constructor. Its native
     * hold protects this exact receiver even if a lower provider rebinds p. */
    entry->engine = startup->app->render_engine;
    if (entry->engine != NULL) {
        entry->engine_hold_error = mc_client_render_engine_retain(entry->engine);
        if (entry->engine_hold_error != MC_CLIENT_RENDER_ENGINE_OK) {
            entry->engine = NULL; /* A failed retain never owned an occurrence. */
            return failure(startup, MC_CLIENT_STARTUP_RENDER_ENGINE_FAILED,
                           "startup actual captured Engine native hold", NULL, NULL, NULL, 0);
        }
    }
    entry->receiver_captured = true;
    if (index == 0) {
        entry->argument = mc_client_lava_texture_as_effect(startup->app->ae); /* ae496 */
    } else if (index == 1) {
        entry->argument = mc_client_water_texture_as_effect(startup->app->water_effect); /* ad507 */
    } else {
        const mc_texture_effect_services services = {.registry = startup->app->registry,
                                                     .profile = startup->app->profile,
                                                     .trig = startup->services.texture_effect_trig,
                                                     .files = startup->services.class_files,
                                                     .resources =
                                                         startup->services.texture_resources};
        startup->failure.caller.pc = entry->new_pc;
        const mc_texture_effect_status allocated = mc_client_texture_effect_allocate(
            &services, entry->kind, &startup->options.texture_effects, &entry->owned,
            &entry->construction);
        if (allocated != MC_TEXTURE_EFFECT_OK)
            return failure(startup, MC_CLIENT_STARTUP_EFFECTS_FAILED, entry->construction.operation,
                           NULL, NULL, NULL, 0);
        entry->allocated = true;
        entry->argument = entry->owned;
        startup->failure.caller.pc = entry->constructor_pc;
        const mc_texture_effect_status constructed = mc_client_texture_effect_construct(
            entry->owned, startup->app, index == 8 ? 1 : 0, &entry->construction);
        snapshot(entry);
        if (constructed != MC_TEXTURE_EFFECT_OK)
            return failure(startup, MC_CLIENT_STARTUP_EFFECTS_FAILED, entry->construction.operation,
                           entry->construction.source_class, entry->construction.source_method,
                           entry->construction.source_descriptor,
                           entry->construction.source_pc < 0
                               ? 0u
                               : (unsigned int)entry->construction.source_pc);
        entry->constructed = true;
        call->last_pc = entry->constructor_pc;
    }
    startup->failure.caller.pc = entry->invocation_pc;
    entry->null_receiver = entry->engine == NULL;
    if (entry->null_receiver)
        return failure(startup, MC_CLIENT_STARTUP_EFFECTS_FAILED,
                       "actual null p receiver after source argument evaluation", MC_CLASS, "a",
                       "()V", entry->invocation_pc);
    /* Same constructor-owned ae. Native bind has no Math/FP activation or
     * source store. It is not attempted before a null invoke receiver. A null
     * ae still reaches actual g.append(NULL) then aw.a NPE12 below. */
    if (index == 0 && startup->app->ae != NULL) {
        if (mc_client_lava_texture_bind_trig(startup->app->ae,
                                             startup->services.texture_effect_trig,
                                             &call->lava) != MC_LAVA_TEXTURE_OK)
            return failure(startup, MC_CLIENT_STARTUP_EFFECTS_FAILED, call->lava.operation, NULL,
                           NULL, NULL, 0);
        call->lava_trig_bound = true;
    }
    entry->registration_entered = true;
    const bool registered = mc_client_render_engine_register_effect(
        entry->engine, entry->argument, startup->options.maximum_effect_registrations,
        &entry->registration);
    snapshot(entry);
    if (!registered) {
        const mc_texture_effect_error *child = &entry->registration.child;
        if (child->source_class != NULL)
            return failure(startup, MC_CLIENT_STARTUP_EFFECTS_FAILED, entry->registration.operation,
                           child->source_class, child->source_method, child->source_descriptor,
                           child->source_pc < 0 ? 0u : (unsigned int)child->source_pc);
        return failure(startup, MC_CLIENT_STARTUP_EFFECTS_FAILED, entry->registration.operation,
                       "ji", "a", "(Law;)V",
                       entry->registration.ji_pc < 0 ? 0u
                                                     : (unsigned int)entry->registration.ji_pc);
    }
    ++call->registrations_completed;
    call->last_pc = entry->invocation_pc;
    return MC_CLIENT_STARTUP_OK;
}

mc_client_startup_status application_startup_effects(mc_client_application_startup *startup) {
    if (startup == NULL || startup->app == NULL)
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    if (startup->effects != NULL)
        return startup->effects->active ? MC_CLIENT_STARTUP_BUSY : MC_CLIENT_STARTUP_INVALID_PHASE;
    if (startup->app->constructor_phase != MC_CLIENT_APP_READY ||
        startup->app->startup != startup || startup->phase == MC_CLIENT_STARTUP_RETIRING ||
        startup->app->source_namespace == NULL || startup->graphics == NULL ||
        !startup->graphics->setup_completed || startup->graphics->diagnostic_calls_completed != 2)
        return failure(startup, MC_CLIENT_STARTUP_INVALID_PHASE,
                       "startup actual completed graphics prefix required", NULL, NULL, NULL, 0);
    startup->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 470};
    mc_client_startup_effects_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return failure(startup, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                       "startup bounded native effect call storage", NULL, NULL, NULL, 0);
    startup->effects = call;
    startup->failure.effects = call;
    const bool outer_active = startup->active;
    startup->active = call->active = call->entered = true;
    mc_client_startup_status status = construct_cx(startup, call);
    if (status == MC_CLIENT_STARTUP_OK)
        status = bind_sound(startup, call);
    for (size_t index = 0; status == MC_CLIENT_STARTUP_OK && index < MC_CLIENT_STARTUP_EFFECT_CALLS;
         ++index)
        status = registration(startup, call, index);
    if (status == MC_CLIENT_STARTUP_OK)
        status = application_startup_rendering(startup);
    call->active = false;
    startup->active = outer_active;
    return status;
}

mc_client_startup_status
application_startup_effects_retire(mc_client_application_startup *startup) {
    mc_client_startup_effects_call *call = startup->effects;
    startup->retirement.effects = call;
    if (call != NULL && call->active)
        return MC_CLIENT_STARTUP_BUSY;
    if (call != NULL && !call->retired) {
        /* End the canonical g occurrences on every actually captured receiver.
         * Duplicate receiver holds remain valid until after the App gate. */
        for (size_t i = 0; i < MC_CLIENT_STARTUP_EFFECT_CALLS; ++i) {
            mc_client_startup_effect_call *entry = &call->effects[i];
            if (entry->engine != NULL && !entry->container_retired) {
                if (!mc_client_render_engine_retire_effects(entry->engine, &entry->cleanup))
                    return entry->cleanup.status == MC_TEXTURE_EFFECT_BUSY
                               ? MC_CLIENT_STARTUP_BUSY
                               : MC_CLIENT_STARTUP_EFFECTS_FAILED;
                entry->container_retired = true;
            }
        }
        for (size_t i = 0; i < MC_CLIENT_STARTUP_EFFECT_CALLS; ++i) {
            mc_client_startup_effect_call *entry = &call->effects[i];
            /* Clear the borrowed argument before a possibly final consumption;
             * scalar snapshots/full causes contain no dangling owner views. */
            entry->argument = NULL;
            const mc_texture_effect_status released =
                mc_client_texture_effect_release(&entry->owned, &entry->retirement);
            if (released != MC_TEXTURE_EFFECT_OK)
                return released == MC_TEXTURE_EFFECT_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                          : MC_CLIENT_STARTUP_EFFECTS_FAILED;
        }
    }
    /* Even a caller with no own record can have an externally held genuine
     * compass/watch. Keep all App/Settings/resources live until it ends. */
    if (mc_client_application_effect_borrows_pending(startup->app))
        return MC_CLIENT_STARTUP_BUSY;
    if (call == NULL || call->retired)
        return MC_CLIENT_STARTUP_OK;
    if ((call->S_stored && startup->app->S != call->cx_candidate) ||
        (!call->S_stored && startup->app->S != NULL))
        return MC_CLIENT_STARTUP_INVALID_PHASE;
    const mc_client_cx_status cx = mc_client_cx_release(&call->cx_candidate, &call->cx_cleanup);
    if (call->cx_candidate == NULL && call->S_stored) {
        startup->app->S = NULL; /* Native view ended; no source putfield. */
        call->S_stored = false;
    }
    if (cx != MC_CLIENT_CX_OK)
        return cx == MC_CLIENT_CX_BUSY ? MC_CLIENT_STARTUP_BUSY : MC_CLIENT_STARTUP_CX_FAILED;
    if (call->sound_hook_attached) {
        if (startup->app->settings != call->sound_settings || startup->app->B != call->sound)
            return MC_CLIENT_STARTUP_INVALID_PHASE;
        if (mc_game_settings_attach_sound_manager(call->sound_settings, NULL,
                                                  &call->sound_hook_error) != MC_GAME_SETTINGS_OK)
            return call->sound_hook_error.status == MC_GAME_SETTINGS_BUSY
                       ? MC_CLIENT_STARTUP_BUSY
                       : MC_CLIENT_STARTUP_SOUND_FAILED;
        call->sound_hook_attached = false;
    }
    if (call->sound_settings_borrow) {
        if (startup->app->settings != call->sound_settings || startup->app->B != call->sound)
            return MC_CLIENT_STARTUP_INVALID_PHASE;
        if (mc_sound_manager_detach_settings(call->sound, call->sound_settings,
                                             &call->sound_cleanup) != MC_SOUND_OK)
            return call->sound_cleanup.status == MC_SOUND_BUSY ? MC_CLIENT_STARTUP_BUSY
                                                               : MC_CLIENT_STARTUP_SOUND_FAILED;
        call->sound_settings_borrow = false;
    }
    call->sound = NULL;
    call->sound_settings = NULL;
    for (size_t i = 0; i < MC_CLIENT_STARTUP_EFFECT_CALLS; ++i) {
        mc_client_startup_effect_call *entry = &call->effects[i];
        if (entry->engine != NULL) {
            entry->engine_release_error = mc_client_render_engine_release(entry->engine);
            if (entry->engine_release_error != MC_CLIENT_RENDER_ENGINE_OK)
                return entry->engine_release_error == MC_CLIENT_RENDER_ENGINE_BUSY
                           ? MC_CLIENT_STARTUP_BUSY
                           : MC_CLIENT_STARTUP_RENDER_ENGINE_FAILED;
            entry->engine = NULL;
        }
    }
    call->cx_classes = NULL;
    call->retired = true;
    return MC_CLIENT_STARTUP_OK;
}
