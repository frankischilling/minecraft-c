#include "mc/world/runtime/auxiliary.h"

#include "construction_internal.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/mutations.h"
#include "runtime_internal.h"
#include "world_access_internal.h"

#include <stdlib.h>

static mc_world_storage_status fail(mc_world_auxiliary_error *e, mc_world_storage_status status,
                                    const char *operation, unsigned int pc, bool native) {
    e->status = status;
    e->operation = operation;
    e->source_pc = pc;
    e->native_policy = native;
    e->cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static bool enter(mc_world_runtime *world, mc_world_auxiliary_error *e, const char *descriptor) {
    e->method = "a";
    e->descriptor = descriptor;
    if (world == NULL || world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED ||
        world->classes->side != MC_WORLD_INFO_CLIENT) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World auxiliary client GAME admission", 0,
             true);
        return false;
    }
    const mc_world_storage_status status = mc_world_runtime_call_enter(world, &e->cause);
    if (status != MC_WORLD_STORAGE_OK) {
        fail(e, status, e->cause.operation, 0, true);
        return false;
    }
    return true;
}

static mc_world_storage_status dispatch(mc_world_runtime *world, mc_entity *player, int32_t effect,
                                        int32_t x, int32_t y, int32_t z, int32_t data,
                                        mc_world_auxiliary_error *e) {
    for (size_t index = 0;; ++index) {
        mc_world_access *access = NULL;
        bool present = false;
        if (!mc_world_access_next(world, index, 9, 23, 28, &access, &present, &e->world_access_pc,
                                  &e->cause))
            return fail(e, e->cause.status, e->cause.operation, e->world_access_pc, true);
        if (!present)
            return MC_WORLD_STORAGE_OK;
        e->world_access_pc = 40;
        const mc_world_storage_status status = mc_world_access_call_auxiliary(
            access, world, player, effect, x, y, z, data, &e->cause, &e->world_access_failure);
        mc_world_access_release_held(access);
        if (status == MC_WORLD_STORAGE_OK)
            continue;
        const mc_world_storage_error cause = mc_world_access_callback_error(status, e->cause);
        fail(e, status, cause.operation, 40, access != NULL);
        e->cause = cause;
        if (access == NULL) {
            e->source_exception = true;
            e->exception_class = "java/lang/NullPointerException";
        } else if (mc_world_access_failure_source_exception(&e->world_access_failure,
                                                            &e->exception_class)) {
            e->source_exception = true;
            e->native_policy = false;
        }
        return status;
    }
}

mc_world_storage_status mc_world_runtime_auxiliary_effect(mc_world_runtime *world,
                                                          mc_entity *player, int32_t effect,
                                                          int32_t x, int32_t y, int32_t z,
                                                          int32_t data,
                                                          mc_world_auxiliary_error *e) {
    if (e == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (!enter(world, e, "(Lgs;IIIII)V"))
        return e->status;
    const mc_world_storage_status status = dispatch(world, player, effect, x, y, z, data, e);
    if (status == MC_WORLD_STORAGE_OK) {
        e->source_pc = 51;
        e->returned = true;
    } else
        world->last_error = e->cause;
    mc_world_runtime_call_leave(world);
    return status;
}

mc_world_storage_status mc_world_runtime_extinguish_fire(mc_world_runtime *world, mc_entity *player,
                                                         int32_t x, int32_t y, int32_t z,
                                                         int32_t face, bool *out,
                                                         mc_world_auxiliary_error *e) {
    if (e == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (!enter(world, e, "(Lgs;IIII)V"))
        return e->status;
    if (face == 0)
        y = mc_java_i32_sub(y, 1);
    if (face == 1)
        y = mc_java_i32_add(y, 1);
    if (face == 2)
        z = mc_java_i32_sub(z, 1);
    if (face == 3)
        z = mc_java_i32_add(z, 1);
    if (face == 4)
        x = mc_java_i32_sub(x, 1);
    if (face == 5)
        x = mc_java_i32_add(x, 1);
    int32_t id = 0;
    e->source_pc = 58;
    mc_world_storage_status status = mc_world_runtime_block_get(world, x, y, z, &id, &e->query);
    if (status != MC_WORLD_STORAGE_OK) {
        fail(e, status, e->query.operation, 58, true);
        goto done;
    }
    const mc_block *fire = NULL;
    e->source_pc = 61;
    if (mc_registry_block_static_get(world->classes->registry, "as", &fire, &e->registry) !=
        MC_REGISTRY_OK) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "extinguish actual Block.as active use", 61,
             true);
        goto done;
    }
    const mc_block_fields *fields = mc_block_fields_get(fire);
    if (fields == NULL) {
        fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "extinguish actual Fire ID receiver", 64,
             false);
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        goto done;
    }
    if (id == fields->bn) {
        e->source_pc = 80;
        status = dispatch(world, player, 1004, x, y, z, 0, e);
        if (status != MC_WORLD_STORAGE_OK) {
            e->source_pc = 80;
            goto done;
        }
        e->source_pc = 89;
        e->mutation = calloc(1, sizeof(*e->mutation));
        if (e->mutation == NULL) {
            fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED, "extinguish reached mutation diagnostic",
                 89, true);
            goto done;
        }
        bool changed = false;
        status = mc_world_runtime_block_set(world, x, y, z, 0, &changed, e->mutation);
        if (status != MC_WORLD_STORAGE_OK) {
            fail(e, status, e->mutation->operation, 89, true);
            goto done;
        }
        free(e->mutation);
        e->mutation = NULL;
        *out = true;
    } else
        *out = false;
    e->source_pc = 93;
    e->returned = true;
done:
    if (e->status != MC_WORLD_STORAGE_OK)
        world->last_error = e->cause;
    mc_world_runtime_call_leave(world);
    return e->status;
}

bool mc_world_auxiliary_error_retire(mc_world_auxiliary_error *e) {
    if (e != NULL) {
        free(e->mutation);
        *e = (mc_world_auxiliary_error){0};
    }
    return true;
}
