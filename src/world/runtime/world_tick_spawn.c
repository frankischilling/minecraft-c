#include "mc/world/runtime/spawning.h"

#include "../biome/registry_internal.h"
#include "construction_internal.h"
#include "mc/core/java/java.h"
#include "runtime_internal.h"

#include <string.h>

static mc_world_storage_status fail(mc_world_spawn_pick_error *e, mc_world_storage_status status,
                                    const char *operation, bool native, const char *exception) {
    e->status = status;
    e->operation = operation;
    e->native_policy = native;
    e->exception_class = exception;
    e->cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status pick(mc_world_runtime *w, const mc_biome *biome, int32_t category,
                                    mc_world_spawn_pick *out, mc_world_spawn_pick_error *e) {
    e->bytecode = 290;
    if (biome == NULL)
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn null Biome receiver", false,
                    "java/lang/NullPointerException");
    if (biome->classes != mc_world_runtime_registry(w))
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn Biome namespace", true,
                    NULL);
    const mc_biome_spawn_list *list = mc_biome_spawn_list_get(biome, category);
    mc_world_spawn_pick value = {0};
    if (list == NULL || mc_biome_spawn_list_size(list) == 0) {
        *out = value;
        return MC_WORLD_STORAGE_OK;
    }
    for (size_t i = 0; i < mc_biome_spawn_list_size(list); ++i) {
        const mc_biome_spawn_fields *entry =
            mc_biome_spawn_fields_get(mc_biome_spawn_list_at(list, i));
        e->bytecode = 351;
        if (entry == NULL)
            return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn null weighted entry",
                        false, "java/lang/NullPointerException");
        value.total_weight = mc_java_i32_add(value.total_weight, entry->b);
    }
    e->bytecode = 366;
    mc_java_random *random = w->fields.r;
    if (random == NULL)
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn null Random receiver",
                    false, "java/lang/NullPointerException");
    if (!mc_java_random_next_bounded(random, value.total_weight, &value.ticket))
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn nonpositive weight bound",
                    false, "java/lang/IllegalArgumentException");
    value.remainder = value.ticket;
    value.entry = mc_biome_spawn_list_at(list, 0);
    for (size_t i = 0; i < mc_biome_spawn_list_size(list); ++i) {
        const mc_biome_spawn *entry = mc_biome_spawn_list_at(list, i);
        const mc_biome_spawn_fields *fields = mc_biome_spawn_fields_get(entry);
        e->bytecode = 419;
        if (fields == NULL)
            return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn null weighted entry",
                        false, "java/lang/NullPointerException");
        value.remainder = mc_java_i32_sub(value.remainder, fields->b);
        if (value.remainder < 0) {
            value.entry = entry;
            value.index = i;
            break;
        }
    }
    *out = value;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_runtime_pick_spawn_entry(mc_world_runtime *w,
                                                          const mc_biome *biome, int32_t category,
                                                          mc_world_spawn_pick *out,
                                                          mc_world_spawn_pick_error *error) {
    mc_world_spawn_pick_error frame;
    memset(&frame, 0, sizeof frame);
    if (w == NULL || out == NULL)
        (void)fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World spawn pick arguments", true,
                   NULL);
    else if (w->construction.mode != MC_WORLD_RUNTIME_GAME || w->classes == NULL ||
             w->classes->side != MC_WORLD_INFO_CLIENT ||
             w->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn pick client GAME owner",
                   true, NULL);
    else if (mc_world_runtime_call_enter(w, &frame.cause) != MC_WORLD_STORAGE_OK) {
        frame.status = frame.cause.status;
        frame.operation = frame.cause.operation;
        frame.native_policy = true;
    } else {
        (void)pick(w, biome, category, out, &frame);
        w->last_error = frame.cause;
        mc_world_runtime_call_leave(w);
    }
    if (frame.status == MC_WORLD_STORAGE_OK)
        memset(&frame, 0, sizeof frame);
    if (error != NULL)
        *error = frame;
    return frame.status;
}
