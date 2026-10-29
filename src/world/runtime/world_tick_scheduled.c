#include "mc/world/runtime/world_tick.h"

#include "../../block/registry/runtime_internal.h"
#include "construction_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"
#include "mutation/mutation_internal.h"
#include "presence_internal.h"
#include "runtime_internal.h"

#include <stdlib.h>
#include <string.h>

static bool fail(mc_world_runtime *w, mc_world_scheduled_drain_error *e,
                 mc_world_storage_status status, const char *operation) {
    e->call.status = status;
    e->call.operation = operation;
    e->call.cause = (mc_world_storage_error){status, operation, 0};
    e->native_policy = true;
    w->last_error = e->call.cause;
    return false;
}

static bool cause(mc_world_runtime *w, mc_world_scheduled_drain_error *e,
                  mc_world_storage_error error) {
    (void)fail(w, e, error.status, error.operation);
    w->last_error = e->call.cause = error;
    return false;
}

static bool ordered(mc_world_runtime *w, mc_world_scheduled_drain_error *e,
                    mc_scheduled_ordered_status status, unsigned int pc) {
    e->call.bytecode = pc;
    e->call.ordered = status;
    if (status == MC_SCHEDULED_ORDERED_OK)
        return true;
    if (w->fields.E == NULL) {
        (void)fail(w, e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "World scheduled drain null ordered queue");
        e->native_policy = false;
        e->exception_class = "java/lang/NullPointerException";
        return false;
    }
    return fail(w, e,
                status == MC_SCHEDULED_ORDERED_HOLD_LIMIT || status == MC_SCHEDULED_ORDERED_LIMIT
                    ? MC_WORLD_STORAGE_LIMIT
                    : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                "World scheduled drain reached ordered queue");
}

static bool membership(mc_world_runtime *w, mc_world_scheduled_drain_error *e,
                       mc_scheduled_membership_status status, unsigned int pc) {
    e->call.bytecode = pc;
    e->call.membership = status;
    if (status == MC_SCHEDULED_MEMBERSHIP_OK)
        return true;
    if (w->fields.F == NULL) {
        (void)fail(w, e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "World scheduled drain null membership queue");
        e->native_policy = false;
        e->exception_class = "java/lang/NullPointerException";
        return false;
    }
    return fail(w, e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                "World scheduled drain reached membership queue");
}

static bool tick(mc_world_runtime *w, mc_scheduled_tick *record,
                 mc_world_scheduled_drain_error *e) {
    mc_scheduled_tick_fields f;
    (void)mc_scheduled_tick_fields_get(record, &f);
    bool present;
    mc_world_presence_error presence = {0};
    e->call.bytecode = 159;
    if (mc_world_runtime_region_present_held(w, mc_java_i32_sub(f.x, 8), mc_java_i32_sub(f.y, 8),
                                             mc_java_i32_sub(f.z, 8), mc_java_i32_add(f.x, 8),
                                             mc_java_i32_add(f.y, 8), mc_java_i32_add(f.z, 8),
                                             &present, &presence) != MC_WORLD_STORAGE_OK) {
        e->call.presence_bytecode = presence.bytecode;
        e->call.provider_bytecode = presence.provider_bytecode;
        return cause(w, e, presence.cause);
    }
    if (!present)
        return true;
    (void)mc_scheduled_tick_fields_get(record, &f);
    int32_t current;
    mc_world_storage_error error = {0};
    e->call.bytecode = 181;
    if (mc_world_runtime_block_get(w, f.x, f.y, f.z, &current, &error) != MC_WORLD_STORAGE_OK)
        return cause(w, e, error);
    (void)mc_scheduled_tick_fields_get(record, &f);
    if (current != f.block || current <= 0)
        return true;

    mc_registry_error registry = mc_world_mutation_empty_registry();
    mc_registry_block_m_view view;
    mc_registry_classes *classes = mc_world_runtime_registry(w);
    e->call.bytecode = 201;
    if (classes == NULL) {
        registry.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        registry.message = "World scheduled drain Registry binding";
    } else if (mc_registry_block_m_get(classes, &view, &registry) == MC_REGISTRY_OK) {
        const mc_block *block;
        e->call.bytecode = 206;
        if (mc_registry_block_m_at(&view, current, &block, &registry) == MC_REGISTRY_OK) {
            (void)mc_scheduled_tick_fields_get(record, &f);
            mc_java_random *random = w->fields.r;
            e->call.bytecode = 227;
            if (mc_world_mutation_block_tick(w, block, f.x, f.y, f.z, random, &e->call.tick))
                return true;
            e->call.registry = e->call.tick.registry;
            return cause(w, e, e->call.tick.cause);
        }
    }
    e->call.registry = registry;
    e->call.status = mc_world_runtime_registry_result(
        w, registry.status, &registry, "World scheduled drain Block.m access", &e->call.cause);
    e->call.operation = e->call.cause.operation;
    return false;
}

static bool drain(mc_world_runtime *w, bool force, bool *pending,
                  mc_world_scheduled_drain_error *e) {
    int32_t size, fsize;
    if (!ordered(w, e, mc_scheduled_ordered_size(w->fields.E, &size), 4))
        return false;
    e->initial_count = size;
    if (!membership(w, e, mc_scheduled_membership_size(w->fields.F, &fsize), 13))
        return false;
    if (size != fsize) {
        e->call.bytecode = 30;
        (void)fail(w, e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "TickNextTick list out of synch");
        e->native_policy = false;
        e->exception_class = "java/lang/IllegalStateException";
        return false;
    }
    if (size > 1000)
        size = 1000;
    for (int32_t i = 0; i < size; ++i) {
        mc_scheduled_tick *record = NULL;
        if (!ordered(w, e, mc_scheduled_ordered_first(w->fields.E, &record), 53))
            return false;
        bool ok = false;
        ++e->iterations;
        if (!force) {
            mc_scheduled_tick_fields f;
            (void)mc_scheduled_tick_fields_get(record, &f);
            int64_t now;
            e->call.bytecode = 74;
            if (mc_world_info_time_get(w->fields.x, &now) != MC_WORLD_INFO_OK) {
                (void)fail(w, e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "World scheduled drain Info time receiver");
                e->native_policy = false;
                e->exception_class = "java/lang/NullPointerException";
                goto release;
            }
            if (f.due_time > now) {
                mc_scheduled_tick_release(record);
                break;
            }
        }
        bool removed;
        if (!ordered(w, e, mc_scheduled_ordered_remove(w->fields.E, record, &removed), 90))
            goto release;
        if (!membership(w, e, mc_scheduled_membership_remove(w->fields.F, record, &removed), 100))
            goto release;
        ++e->removed;
        ok = tick(w, record, e);
    release:
        mc_scheduled_tick_release(record);
        if (!ok)
            return false;
    }
    if (!ordered(w, e, mc_scheduled_ordered_size(w->fields.E, &size), 240))
        return false;
    *pending = size != 0;
    e->call.bytecode = 251;
    return true;
}

mc_world_storage_status mc_world_runtime_drain_scheduled(mc_world_runtime *w, bool force,
                                                         bool *pending,
                                                         mc_world_scheduled_drain_error *error) {
    mc_world_storage_error admission = {0};
    if (w == NULL || pending == NULL)
        admission = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                             "World scheduled drain arguments", 0};
    else if (w->construction.mode != MC_WORLD_RUNTIME_GAME || w->classes == NULL ||
             w->classes->side != MC_WORLD_INFO_CLIENT ||
             w->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        admission = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                             "World scheduled drain actual client GAME owner", 0};
    mc_world_scheduled_drain_error *frame = calloc(1, sizeof(*frame));
    if (frame == NULL) {
        if (error != NULL) {
            memset(error, 0, sizeof(*error));
            error->call.status = MC_WORLD_STORAGE_ALLOCATION_FAILED;
            error->call.operation = "World scheduled drain diagnostic allocation";
            error->call.cause =
                (mc_world_storage_error){error->call.status, error->call.operation, 0};
        }
        return MC_WORLD_STORAGE_ALLOCATION_FAILED;
    }
    mc_world_mutation_error_init(&frame->call.tick);
    frame->call.registry = mc_world_mutation_empty_registry();
    if (admission.status == MC_WORLD_STORAGE_OK)
        (void)mc_world_runtime_call_enter(w, &admission);
    if (admission.status != MC_WORLD_STORAGE_OK) {
        frame->call.status = admission.status;
        frame->call.operation = admission.operation;
        frame->call.cause = admission;
        frame->native_policy = true;
    } else {
        bool result;
        if (drain(w, force, &result, frame)) {
            *pending = result;
            frame->call.status = MC_WORLD_STORAGE_OK;
            frame->call.operation = NULL;
            frame->call.cause = (mc_world_storage_error){0};
            w->last_error = frame->call.cause;
        }
        mc_world_runtime_call_leave(w);
    }
    const mc_world_storage_status status = frame->call.status;
    if (error != NULL)
        *error = *frame;
    free(frame);
    return status;
}
