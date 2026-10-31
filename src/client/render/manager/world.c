#include "../../../world/runtime/runtime_internal.h"
#include "internal.h"
#include "mc/world/runtime/runtime.h"

bool manager_world_ledger_admit(const mc_client_render_manager *owner,
                                mc_client_manager_error *error) {
    if (owner->fields.g == owner->world_occurrence &&
        (owner->world_occurrence == NULL || owner->world_occurrence->persistent_bindings != 0))
        return true;
    manager_fail(error, MC_CLIENT_MANAGER_BUSY, "TH native World occurrence invariant", NULL, NULL,
                 NULL, -1, true);
    return false;
}

/* Entered by either actual source overload. The caller owns both TH guards
 * when callbacks can occur; this store itself calls no source child. */
bool manager_world_store_entered(mc_client_render_manager *owner, mc_world_runtime *world,
                                 mc_client_manager_error *error) {
    if (!manager_world_ledger_admit(owner, error))
        return false;
    if (world != NULL && !mc_world_runtime_binding_retain(world)) {
        error->world = *mc_world_runtime_last_error(world);
        manager_fail(error, MC_CLIENT_MANAGER_WORLD_FAILED, "TH captured World native occurrence",
                     NULL, NULL, NULL, -1, true);
        return false;
    }
    manager_frame_invalidate(owner); /* Before the old diagnostic World can end. */
    mc_world_runtime *previous = owner->world_occurrence;
    owner->fields.g = world; /* Actual g2 in both source overloads. */
    owner->world_occurrence = world;
    mc_world_runtime_binding_release(previous);
    return true;
}

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
    if (!manager_world_store_entered(owner, world, error)) {
        /* Preserve this older native API's established failure classification. */
        if (error->status == MC_CLIENT_MANAGER_WORLD_FAILED)
            error->status = MC_CLIENT_MANAGER_BUSY;
        return error->status;
    }
    return MC_CLIENT_MANAGER_OK;
}

mc_client_manager_status mc_client_render_manager_detach_world(mc_client_render_manager *owner,
                                                               mc_world_runtime *expected,
                                                               mc_client_manager_error *error) {
    if (error == NULL)
        return MC_CLIENT_MANAGER_INVALID_ARGUMENT;
    if (admit(owner, error) != MC_CLIENT_MANAGER_OK)
        return error->status;
    if (!manager_world_ledger_admit(owner, error))
        return error->status;
    if (expected != NULL && owner->world_occurrence == expected) {
        if (owner->fields.g != expected)
            return manager_fail(error, MC_CLIENT_MANAGER_BUSY,
                                "TH source World changed outside native occurrence", NULL, NULL,
                                NULL, -1, true);
        manager_frame_invalidate(owner);
        owner->fields.g = NULL; /* Consumed native view, no source method credited. */
        owner->world_occurrence = NULL;
        mc_world_runtime_binding_release(expected);
    }
    return MC_CLIENT_MANAGER_OK;
}
