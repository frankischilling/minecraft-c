#include "application_startup_internal.h"

mc_client_startup_status application_startup_fail(mc_client_startup_error *error,
                                                  mc_client_startup_status status,
                                                  const char *operation, const char *class_name,
                                                  const char *method, const char *descriptor,
                                                  unsigned int pc) {
    error->status = status;
    error->operation = operation;
    error->child = (mc_client_application_owner_site){class_name, method, descriptor, pc};
    return status;
}
