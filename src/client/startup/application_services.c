#include "application_services.h"
#include "application_owner_internal.h"

#include <assert.h>

static mc_java_print_status failed(mc_java_print_error *error, mc_java_print_status status,
                                   const char *operation, int native_error) {
    *error = (mc_java_print_error){.status = status,
                                   .operation = operation,
                                   .system_error = native_error,
                                   .native_policy = true};
    return status;
}

mc_java_print_status
mc_client_application_classes_output_service(mc_client_application_classes *classes,
                                             mc_java_system_output **out,
                                             mc_java_print_error *error) {
    if (error == NULL)
        return MC_JAVA_PRINT_INVALID_ARGUMENT;
    *error = (mc_java_print_error){0};
    if (classes == NULL || out == NULL)
        return failed(error, MC_JAVA_PRINT_INVALID_ARGUMENT, "MC System-output service arguments",
                      0);
    const int locked = mtx_lock(&classes->mutex);
    if (locked != thrd_success)
        return failed(error, MC_JAVA_PRINT_NATIVE_FAILURE, "MC System-output service lock", locked);
    mc_java_print_status status;
    mc_java_system_output *retained = NULL;
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY)
        status =
            failed(error, MC_JAVA_PRINT_INVALID_PHASE, "MC System-output service class phase", 0);
    else if (!classes->system_output_attempted)
        status = failed(error, MC_JAVA_PRINT_INVALID_PHASE,
                        "MC System-output service not bootstrapped", 0);
    else if (classes->system_output_failure.status != MC_JAVA_PRINT_OK) {
        *error = classes->system_output_failure;
        status = error->status;
    } else if (classes->system_output == NULL) {
        status = failed(error, MC_JAVA_PRINT_INVALID_PHASE,
                        "MC System-output service missing after bootstrap", 0);
    } else {
        status = mc_java_system_output_retain(classes->system_output, error);
        if (status == MC_JAVA_PRINT_OK)
            retained = classes->system_output;
    }
    const int unlocked = mtx_unlock(&classes->mutex);
    assert(unlocked == thrd_success); /* Admitted live-mutex ownership invariant. */
    (void)unlocked;
    if (status == MC_JAVA_PRINT_OK)
        *out = retained;
    return status;
}
