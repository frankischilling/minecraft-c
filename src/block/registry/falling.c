#include "mc/block/registry/falling.h"

#include "registry_internal.h"

static mc_registry_status result(mc_registry_error *error, mc_registry_status status,
                                 const char *message) {
    if (error != NULL)
        *error = (mc_registry_error){.status = status,
                                     .message = message,
                                     .initializer_pc = -1,
                                     .block_id = -1,
                                     .constructor_class = NULL,
                                     .constructor_pc = -1,
                                     .profile_status = MC_JAVA_RANDOM_PROFILE_OK,
                                     .active_initializer_class = NULL,
                                     .active_initializer_pc = -1,
                                     .item_argument = -1};
    return status;
}

static mc_registry_status access(mc_registry_classes *classes, bool store, bool value, bool *out,
                                 mc_registry_error *error) {
    if (classes == NULL || (!store && out == NULL))
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "falling flag owner/output");
    if (classes->entered != 0 || classes->blocks_phase == MC_REGISTRY_INITIALIZING)
        return result(error, MC_REGISTRY_BUSY, "falling flag Registry entered");
    ++classes->entered;
    /* Initialize the DECLARING gk/ei class, including its actual superclass.
     * Do not merely initialize Block or reset a flag before the real clinit. */
    const mc_registry_status status = mc_registry_block_class_initialize(classes, MC_BLOCK_SAND);
    if (status == MC_REGISTRY_OK) {
        if (store)
            classes->gk_a = value;
        else
            *out = classes->gk_a;
    }
    --classes->entered;
    if (status != MC_REGISTRY_OK) {
        if (error != NULL)
            *error = classes->last_error;
        return status;
    }
    return result(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_falling_flag_get(mc_registry_classes *classes, bool *out,
                                                mc_registry_error *error) {
    return access(classes, false, false, out, error);
}

mc_registry_status mc_registry_falling_flag_set(mc_registry_classes *classes, bool value,
                                                mc_registry_error *error) {
    return access(classes, true, value, NULL, error);
}
