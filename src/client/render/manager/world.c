#include "internal.h"
#include "mc/world/runtime/runtime.h"

static mc_client_manager_status admit(mc_client_render_manager *owner,
                                      mc_client_manager_error *error) {
    manager_error_init(error);
    if (owner == NULL)
        return manager_fail(error, MC_CLIENT_MANAGER_NULL_REFERENCE, "actual th.a(fd) receiver",
                            "th", "a", "(Lfd;)V", 2, false);
    if (owner->active || owner->classes->active || owner->classes->phase != MC_CLIENT_MANAGER_READY)
        return manager_fail(error, MC_CLIENT_MANAGER_BUSY, "TH World native operation admission",
                            NULL, NULL, NULL, -1, true);
    return MC_CLIENT_MANAGER_OK;
}

mc_client_manager_status mc_client_render_manager_change_world(mc_client_render_manager *owner,
                                                               mc_world_runtime *world,
                                                               mc_client_manager_error *error) {
    if (error == NULL)
        return MC_CLIENT_MANAGER_INVALID_ARGUMENT;
    if (admit(owner, error) != MC_CLIENT_MANAGER_OK)
        return error->status;
    if (world != NULL && !mc_world_runtime_binding_retain(world))
        return manager_fail(error, MC_CLIENT_MANAGER_BUSY, "TH captured World native occurrence",
                            NULL, NULL, NULL, -1, true);
    mc_world_runtime *previous = owner->world_occurrence;
    owner->fields.g = world; /* Actual th.a(fd) putfield2. */
    owner->world_occurrence = world;
    mc_world_runtime_binding_release(previous);
    return MC_CLIENT_MANAGER_OK;
}

mc_client_manager_status mc_client_render_manager_detach_world(mc_client_render_manager *owner,
                                                               mc_world_runtime *expected,
                                                               mc_client_manager_error *error) {
    if (error == NULL)
        return MC_CLIENT_MANAGER_INVALID_ARGUMENT;
    if (admit(owner, error) != MC_CLIENT_MANAGER_OK)
        return error->status;
    if (expected != NULL && owner->world_occurrence == expected) {
        if (owner->fields.g != expected)
            return manager_fail(error, MC_CLIENT_MANAGER_BUSY,
                                "TH source World changed outside native occurrence", NULL, NULL,
                                NULL, -1, true);
        owner->fields.g = NULL; /* Consumed native view, no source method credited. */
        owner->world_occurrence = NULL;
        mc_world_runtime_binding_release(expected);
    }
    return MC_CLIENT_MANAGER_OK;
}
