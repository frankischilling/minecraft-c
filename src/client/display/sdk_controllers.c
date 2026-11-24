#include "sdk_controllers_internal.h"
#include "sdk_controllers_native.h"

#include <SDL3/SDL_init.h>
#include <stdlib.h>

#define CONTROLLERS "org/lwjgl/input/Controllers"

static mc_client_sdk_empty_list *list_allocate(mc_client_sdk_input_error *e, const char *method,
                                               int32_t pc, const char *operation) {
    mc_client_sdk_empty_list *list = calloc(1, sizeof(*list));
    if (list == NULL)
        mc_client_sdk_input_fail(e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED, operation, CONTROLLERS,
                                 method, "()V", pc, true);
    return list;
}

/* One canonical body for the existing class cells. The actual ArrayList
 * headers are independent; no returned/local list replaces either field. */
static bool active_use(mc_client_sdk_input *o, mc_client_sdk_input_error *e) {
    if (o->controllers == NULL) {
        mc_client_sdk_empty_list *list =
            list_allocate(e, "<clinit>", 0, "actual Controllers first ArrayList");
        if (list == NULL)
            return false;
        o->controllers = list;
        o->controllers_last_clinit_store = 7;
    }
    if (o->controller_events == NULL) {
        mc_client_sdk_empty_list *list =
            list_allocate(e, "<clinit>", 10, "actual Controllers second ArrayList");
        if (list == NULL)
            return false;
        o->controller_events = list;
        o->controllers_last_clinit_store = 17;
    }
    return true;
}

static void attempt_free(mc_client_sdk_controller_attempt **slot) {
    mc_client_sdk_controller_attempt *attempt = *slot;
    if (attempt == NULL)
        return;
    free(attempt->iterator);
    if (attempt->filtered != NULL) {
        free(attempt->filtered->entries);
        free(attempt->filtered);
    }
    if (attempt->returned != NULL) {
        free(attempt->returned->entries);
        free(attempt->returned);
    }
    free(attempt);
    *slot = NULL;
}

mc_client_sdk_input_status mc_client_sdk_controllers_is_created(mc_client_sdk_input *o,
                                                                bool *result,
                                                                mc_client_sdk_input_error *out) {
    mc_client_sdk_input_error e = {0};
    if (result == NULL || !mc_client_sdk_input_enter(o, CONTROLLERS, "isCreated", "()Z", &e)) {
        if (result == NULL)
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT,
                                     "Controllers native output", CONTROLLERS, "isCreated", "()Z",
                                     -1, true);
        if (out != NULL)
            *out = e;
        return e.status;
    }
    if (active_use(o, &e))
        *result = o->controllers_created;
    return mc_client_sdk_input_leave(o, &e, out);
}

mc_client_sdk_input_status mc_client_sdk_controllers_create(mc_client_sdk_input *o,
                                                            mc_client_sdk_input_error *out) {
    mc_client_sdk_input_error e = {0};
    if (!mc_client_sdk_input_enter(o, CONTROLLERS, "create", "()V", &e)) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    if (!active_use(o, &e))
        goto done;
    if (o->controllers_created) {
        o->controllers_last_create_pc = 6;
        goto done;
    }
    /* Invocation bookkeeping is native and separately owned. It is not the
     * Java returned Controller[], filtered ArrayList or source publication. */
    if (o->controller_attempt != NULL) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_INVALID_PHASE,
                                 "Controllers retained native invocation", CONTROLLERS, "create",
                                 "()V", -1, true);
        goto done;
    }
    o->controller_attempt = calloc(1, sizeof(*o->controller_attempt));
    if (o->controller_attempt == NULL) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
                                 "Controllers native invocation storage", CONTROLLERS, "create",
                                 "()V", -1, true);
        goto done;
    }
    if (o->controller_environment == NULL &&
        mc_client_sdk_controller_environment_allocate(&o->controller_environment, &e) !=
            MC_CLIENT_SDK_INPUT_OK)
        goto done;
    o->controllers_last_create_pc = 7;
    if (mc_client_sdk_controller_environment_discover(o->controller_environment, &e) !=
        MC_CLIENT_SDK_INPUT_OK)
        goto done;
    if (o->controller_environment->count != 0) {
        /* Actual discovery is retained. No completed Java Controller[]/peer
         * is fabricated from ID/count/name-only SDL data. This native failure
         * is not the Source Throwable/LWJGLException/MC391 catch path. */
        mc_client_sdk_input_fail(
            &e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE,
            "Controllers nonempty target/component/event/rumbler mapping required", CONTROLLERS,
            "create", "()V", 12, true);
        if (o->controller_environment->failure.status == MC_CLIENT_SDK_INPUT_OK)
            o->controller_environment->failure = e;
        goto done;
    }
    mc_client_sdk_controller_attempt *attempt = o->controller_attempt;
    attempt->returned = calloc(1, sizeof(*attempt->returned));
    if (attempt->returned == NULL) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
                                 "Controllers real zero-length returned array owner", CONTROLLERS,
                                 "create", "()V", 12, true);
        goto done;
    }
    o->controllers_last_create_pc = 12;
    attempt->filtered =
        list_allocate(&e, "create", 16, "Controllers actual temporary filtering ArrayList");
    if (attempt->filtered == NULL)
        goto done;
    o->controllers_last_create_pc = 20;
    if (attempt->returned->count != 0) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE,
                                 "Controllers concrete target type/filter adapter required",
                                 CONTROLLERS, "create", "()V", 48, true);
        goto done;
    }
    /* The real returned array has length0. No type query, leaf constructor,
     * global append or controllerCount update is reached by this profile. */
    attempt->iterator = calloc(1, sizeof(*attempt->iterator));
    if (attempt->iterator == NULL) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
                                 "Controllers actual empty filtered-list cursor", CONTROLLERS,
                                 "create", "()V", 92, true);
        goto done;
    }
    attempt->iterator->list = attempt->filtered;
    while (attempt->iterator->index < attempt->iterator->list->count) {
        mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE,
                                 "Controllers actual target/leaf construction required",
                                 CONTROLLERS, "create", "()V", 118, true);
        goto done;
    }
    o->controllers_last_create_pc = 102;
    o->controllers_created = true; /* Only actual source putstatic125. */
    o->controllers_last_create_pc = 125;
    attempt_free(&o->controller_attempt);
    o->controllers_last_create_pc = 143;
done:
    return mc_client_sdk_input_leave(o, &e, out);
}

mc_client_sdk_input_status mc_client_sdk_controllers_poll(mc_client_sdk_input *o,
                                                          mc_client_sdk_input_error *out) {
    mc_client_sdk_input_error e = {0};
    if (!mc_client_sdk_input_enter(o, CONTROLLERS, "poll", "()V", &e)) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    if (active_use(o, &e)) {
        /* Actual list-size test6 precedes each getController13/poll16. There
         * is no source created guard and no SDL update/event pump here. */
        if (o->controllers->count != 0) {
            mc_client_sdk_input_fail(&e, MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE,
                                     "Controllers real physical Controller.poll body required",
                                     CONTROLLERS, "poll", "()V", 16, true);
        }
    }
    return mc_client_sdk_input_leave(o, &e, out);
}

bool mc_client_sdk_controllers_inspect(const mc_client_sdk_input *o,
                                       mc_client_sdk_controllers_state *out) {
    if (o == NULL || out == NULL || !SDL_IsMainThread())
        return false;
    const mc_client_sdk_controller_environment *env = o->controller_environment;
    const mc_client_sdk_controller_attempt *attempt = o->controller_attempt;
    *out = (mc_client_sdk_controllers_state){
        .controllers_identity = o->controllers,
        .events_identity = o->controller_events,
        .event_identity = o->controller_event,
        .controllers_count = o->controllers == NULL ? 0 : o->controllers->count,
        .events_count = o->controller_events == NULL ? 0 : o->controller_events->count,
        .controller_count = o->controller_count,
        .last_clinit_store_pc = o->controllers_last_clinit_store,
        .last_create_pc = o->controllers_last_create_pc,
        .created = o->controllers_created,
        .create_attempt_retained = attempt != NULL,
        .returned_array_identity = attempt == NULL ? NULL : attempt->returned,
        .filtered_list_identity = attempt == NULL ? NULL : attempt->filtered,
        .iterator_identity = attempt == NULL ? NULL : attempt->iterator,
        .environment_identity = env,
        .environment_phase = env == NULL ? MC_CLIENT_SDK_CONTROLLER_ENV_COLD : env->phase,
        .native_active = env != NULL && env->active,
        .native_retired = o->controllers_native_retired,
        .subsystem_reference_owned = env != NULL && env->reference_owned,
        .initialization_attempted = env != NULL && env->initialization_attempted,
        .enumeration_returned = env != NULL && env->enumeration_returned,
        .native_count = env == NULL ? 0 : env->count,
        .native_ids = env == NULL ? NULL : env->ids,
        .init_calls = env == NULL ? 0 : env->init_calls,
        .enumeration_calls = env == NULL ? 0 : env->enumeration_calls,
        .quit_calls = env == NULL ? 0 : env->quit_calls,
        .configured_backend_mask = mc_client_sdk_controller_native_backends(),
        .environment_failure = env == NULL ? NULL : &env->failure};
    return true;
}

mc_client_sdk_input_status
mc_client_sdk_controllers_native_cleanup(mc_client_sdk_input *o, mc_client_sdk_input_error *error) {
    /* No peer has been published by this increment. Refuse an unimplemented
     * nonempty owner rather than freeing a target/event through void slots. */
    if ((o->controllers != NULL && o->controllers->count != 0) ||
        (o->controller_events != NULL && o->controller_events->count != 0) ||
        o->controller_event != NULL)
        return mc_client_sdk_input_fail(
            error, MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE,
            "Controllers physical wrapper/event native retirement required", NULL, NULL, NULL, -1,
            true);
    const mc_client_sdk_input_status status =
        mc_client_sdk_controller_environment_retire(&o->controller_environment, o->events, error);
    if (status != MC_CLIENT_SDK_INPUT_OK)
        return status;
    attempt_free(&o->controller_attempt);
    o->controllers_native_retired = true;
    return MC_CLIENT_SDK_INPUT_OK;
}
