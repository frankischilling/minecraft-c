#include "mc/world/runtime/spawning.h"

#include "../biome/spawn_list_internal.h"
#include "construction_internal.h"
#include "mc/core/java/java.h"
#include "runtime_internal.h"

#include <string.h>

static mc_world_storage_status fail(mc_world_spawn_pick_error *e, mc_world_storage_status status,
                                    const char *operation, unsigned int pc, bool native,
                                    const char *exception) {
    memset(e, 0, sizeof(*e));
    e->status = status;
    e->operation = operation;
    e->bytecode = pc;
    e->native_policy = native;
    e->exception_class = exception;
    e->cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

static mc_world_storage_status pick(mc_world_runtime *w, const mc_biome *biome, int32_t category,
                                    mc_world_spawn_pick *out, mc_world_spawn_pick_error *e) {
    if (biome == NULL)
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn null Biome receiver", 290,
                    false, "java/lang/NullPointerException");
    if (biome->classes != mc_world_runtime_registry(w))
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn Biome namespace", 290, true,
                    NULL);
    const mc_biome_spawn_list *list = mc_biome_spawn_list_get(biome, category);
    mc_world_spawn_pick value = {0};
    if (list == NULL || list->data.count == 0) {
        *out = value;
        return MC_WORLD_STORAGE_OK;
    }
    /* The admitted owners are exclusive and the two loops and Random draw
     * introduce no callback that could change this List. */
    const size_t count = list->data.count;
    void *const *entries = list->data.elements;
    for (size_t i = 0; i < count; ++i) {
        const mc_biome_spawn *entry = entries[i];
        if (entry == NULL)
            return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn null weighted entry",
                        351, false, "java/lang/NullPointerException");
        value.total_weight = mc_java_i32_add(value.total_weight, entry->fields.b);
    }
    mc_java_random *random = w->fields.r;
    if (random == NULL)
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn null Random receiver", 366,
                    false, "java/lang/NullPointerException");
    if (!mc_java_random_next_bounded(random, value.total_weight, &value.ticket))
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn nonpositive weight bound",
                    366, false, "java/lang/IllegalArgumentException");
    value.remainder = value.ticket;
    value.entry = entries[0];
    for (size_t i = 0; i < count; ++i) {
        const mc_biome_spawn *entry = entries[i];
        if (entry == NULL)
            return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World spawn null weighted entry",
                        419, false, "java/lang/NullPointerException");
        value.remainder = mc_java_i32_sub(value.remainder, entry->fields.b);
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
    mc_world_storage_status status;
    if (w == NULL || out == NULL)
        status = fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World spawn pick arguments", 0,
                      true, NULL);
    else if (w->construction.mode != MC_WORLD_RUNTIME_GAME || w->classes == NULL ||
             w->classes->side != MC_WORLD_INFO_CLIENT ||
             w->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        status = fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                      "World spawn pick client GAME owner", 0, true, NULL);
    else {
        mc_world_storage_error cause;
        status = mc_world_runtime_call_enter(w, &cause);
        if (status != MC_WORLD_STORAGE_OK) {
            (void)fail(&frame, cause.status, cause.operation, 0, true, NULL);
            frame.cause = cause;
        } else {
            status = pick(w, biome, category, out, &frame);
            w->last_error =
                status == MC_WORLD_STORAGE_OK ? (mc_world_storage_error){0} : frame.cause;
            mc_world_runtime_call_leave(w);
        }
    }
    if (error != NULL) {
        if (status == MC_WORLD_STORAGE_OK)
            memset(error, 0, sizeof(*error));
        else
            *error = frame;
    }
    return status;
}
