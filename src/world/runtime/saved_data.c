#include "mc/world/runtime/saved_data.h"

#include "runtime_internal.h"

static mc_world_storage_status fail(mc_world_storage_error *error, mc_world_storage_status status,
                                    const char *operation) {
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status enter(mc_world_runtime *world, mc_world_storage_error *error) {
    if (world == NULL)
        return fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World SavedData receiver");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World SavedData phase");
    return mc_world_runtime_call_enter(world, error);
}

static mc_world_storage_status finish(mc_world_runtime *world, mc_world_storage_status status,
                                      const mc_world_storage_error *cause,
                                      mc_world_storage_error *error) {
    if (status != MC_WORLD_STORAGE_OK)
        world->last_error = *cause;
    mc_world_runtime_call_leave(world);
    if (error != NULL)
        *error = *cause;
    return status;
}

mc_world_storage_status mc_world_runtime_saved_data_get(mc_world_runtime *world,
                                                        const mc_saved_data_class *type,
                                                        mc_world_info_name *key,
                                                        mc_saved_data_value **out,
                                                        mc_world_storage_error *error) {
    if (out == NULL)
        return fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World SavedData get output");
    mc_world_storage_error cause = {0};
    mc_world_storage_status status = enter(world, &cause);
    if (status != MC_WORLD_STORAGE_OK) {
        if (error != NULL)
            *error = cause;
        return status;
    }
    /* a(Class,String) reaches the actual z receiver at bytecode6. */
    status =
        world->fields.z == NULL
            ? fail(&cause, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World SavedData get z receiver")
            : mc_world_saved_data_get(world->fields.z, type, key, out, &cause);
    return finish(world, status, &cause, error);
}

mc_world_storage_status mc_world_runtime_saved_data_put(mc_world_runtime *world,
                                                        mc_world_info_name *key,
                                                        mc_saved_data_value *value,
                                                        mc_world_storage_error *error) {
    mc_world_storage_error cause = {0};
    mc_world_storage_status status = enter(world, &cause);
    if (status != MC_WORLD_STORAGE_OK) {
        if (error != NULL)
            *error = cause;
        return status;
    }
    /* a(String,SavedData) reaches the actual z receiver at bytecode6. */
    status = world->fields.z == NULL ? fail(&cause, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                            "World SavedData put z receiver")
                                     : mc_world_saved_data_put(world->fields.z, key, value, &cause);
    return finish(world, status, &cause, error);
}

mc_world_storage_status mc_world_runtime_saved_data_next_id(mc_world_runtime *world,
                                                            const mc_nbt_string *key, int32_t *out,
                                                            mc_world_storage_error *error) {
    if (out == NULL)
        return fail(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World SavedData ID output");
    mc_world_storage_error cause = {0};
    mc_world_storage_status status = enter(world, &cause);
    if (status != MC_WORLD_STORAGE_OK) {
        if (error != NULL)
            *error = cause;
        return status;
    }
    /* b(String) reaches the actual z receiver at bytecode5. */
    status = world->fields.z == NULL
                 ? fail(&cause, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World SavedData ID z receiver")
                 : mc_world_saved_data_next_id(world->fields.z, key, out, &cause);
    return finish(world, status, &cause, error);
}
