#include "mc/world/runtime/scheduling.h"

#include "../../block/registry/runtime_internal.h"
#include "construction_internal.h"
#include "execution_children_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"
#include "mutation/mutation_internal.h"
#include "presence_internal.h"
#include "runtime_internal.h"
#include "scheduling_execution_internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void error_init(mc_world_schedule_error *error) {
    if (error == NULL)
        return;
    const size_t tick_start = offsetof(mc_world_schedule_error, tick);
    const size_t tick_end = tick_start + sizeof(error->tick);
    memset(error, 0, tick_start);
    memset((unsigned char *)error + tick_end, 0, sizeof(*error) - tick_end);
    mc_world_mutation_error_init(&error->tick);
    error->registry = mc_world_mutation_empty_registry();
}

static mc_world_storage_status admission(mc_world_schedule_error *error,
                                         mc_world_storage_status status, const char *operation) {
    error_init(error);
    if (error != NULL) {
        error->status = status;
        error->operation = operation;
        error->cause = (mc_world_storage_error){status, operation, 0};
    }
    return status;
}

static bool storage_fail(mc_world_runtime *world, mc_world_schedule_error *frame,
                         mc_world_storage_error cause) {
    frame->status = cause.status;
    frame->operation = cause.operation;
    frame->cause = cause;
    world->last_error = cause;
    return false;
}

static bool fail(mc_world_runtime *world, mc_world_schedule_error *frame,
                 mc_world_storage_status status, const char *operation) {
    return storage_fail(world, frame, (mc_world_storage_error){status, operation, 0});
}

static bool record_fail(mc_world_runtime *world, mc_world_schedule_error *frame,
                        const char *operation) {
    mc_world_storage_status status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (frame->record == MC_SCHEDULED_TICK_ALLOCATION_FAILED)
        status = MC_WORLD_STORAGE_ALLOCATION_FAILED;
    else if (frame->record == MC_SCHEDULED_TICK_LIMIT)
        status = MC_WORLD_STORAGE_LIMIT;
    return fail(world, frame, status, operation);
}

static bool membership_fail(mc_world_runtime *world, mc_world_schedule_error *frame,
                            const char *operation) {
    mc_world_storage_status status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (frame->membership == MC_SCHEDULED_MEMBERSHIP_ALLOCATION_FAILED)
        status = MC_WORLD_STORAGE_ALLOCATION_FAILED;
    else if (frame->membership == MC_SCHEDULED_MEMBERSHIP_LIMIT ||
             frame->membership == MC_SCHEDULED_MEMBERSHIP_HOLD_LIMIT)
        status = MC_WORLD_STORAGE_LIMIT;
    return fail(world, frame, status, operation);
}

static bool ordered_fail(mc_world_runtime *world, mc_world_schedule_error *frame,
                         const char *operation) {
    mc_world_storage_status status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (frame->ordered == MC_SCHEDULED_ORDERED_ALLOCATION_FAILED)
        status = MC_WORLD_STORAGE_ALLOCATION_FAILED;
    else if (frame->ordered == MC_SCHEDULED_ORDERED_LIMIT ||
             frame->ordered == MC_SCHEDULED_ORDERED_HOLD_LIMIT)
        status = MC_WORLD_STORAGE_LIMIT;
    return fail(world, frame, status, operation);
}

static bool registry_fail(mc_world_runtime *world, mc_world_schedule_error *frame,
                          const mc_registry_error *cause, const char *operation) {
    frame->registry = *cause;
    frame->status =
        mc_world_runtime_registry_result(world, cause->status, cause, operation, &frame->cause);
    frame->operation = frame->cause.operation;
    return false;
}

static bool radius_present(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                           unsigned int call_pc, bool *out, mc_world_schedule_error *frame) {
    const int32_t min_x = mc_java_i32_sub(x, 8);
    const int32_t min_y = mc_java_i32_sub(y, 8);
    const int32_t min_z = mc_java_i32_sub(z, 8);
    const int32_t max_x = mc_java_i32_add(x, 8);
    const int32_t max_y = mc_java_i32_add(y, 8);
    const int32_t max_z = mc_java_i32_add(z, 8);
    mc_world_presence_error presence = {0};
    frame->bytecode = call_pc;
    if (mc_world_runtime_region_present_held(world, min_x, min_y, min_z, max_x, max_y, max_z, out,
                                             &presence) != MC_WORLD_STORAGE_OK) {
        frame->presence_bytecode = presence.bytecode;
        frame->provider_bytecode = presence.provider_bytecode;
        return storage_fail(world, frame, presence.cause);
    }
    return true;
}

/* The caller owns the fresh record throughout this branch. It has not escaped
 * into a collection, so its coordinate/key reads cannot be invalidated by a
 * synchronous World call. Each snapshot below is a total read of that live
 * owner at the next source use, not an extra failure/preflight boundary. */
static bool immediate(mc_world_runtime *world, mc_scheduled_tick *record,
                      mc_world_schedule_error *frame, mc_world_schedule_execution *owned) {
    mc_scheduled_tick_fields fields;
    (void)mc_scheduled_tick_fields_get(record, &fields);
    bool present;
    if (!radius_present(world, fields.x, fields.y, fields.z, 74, &present, frame))
        return false;
    if (!present)
        return true;

    (void)mc_scheduled_tick_fields_get(record, &fields);
    int32_t current;
    mc_world_storage_error cause = {0};
    frame->bytecode = 96;
    if (owned == NULL) {
        if (mc_world_runtime_block_get(world, fields.x, fields.y, fields.z, &current, &cause) !=
            MC_WORLD_STORAGE_OK)
            return storage_fail(world, frame, cause);
    } else {
        owned->lower = (mc_world_block_execution){.execution = owned->execution,
                                                  .code = owned->code,
                                                  .receiver_site = {"fd", "c", "(IIIII)V", 96},
                                                  .owned = owned->owned};
        const bool completed = mc_world_runtime_block_get_in_execution(
            &owned->lower, world, fields.x, fields.y, fields.z, &current, &owned->lower_cause,
            &owned->lower_status);
        owned->native_stopped =
            owned->lower.native_stopped || owned->owned->ownership_status != MC_WORLD_STORAGE_OK;
        if (!completed) {
            if (!owned->native_stopped)
                return storage_fail(world, frame, owned->lower_cause);
            return false;
        }
    }
    (void)mc_scheduled_tick_fields_get(record, &fields);
    if (current != fields.block || current <= 0)
        return true;

    mc_registry_classes *classes = mc_world_runtime_registry(world);
    mc_registry_error registry = mc_world_mutation_empty_registry();
    mc_registry_block_m_view array;
    frame->bytecode = 116;
    if (classes == NULL) {
        registry.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        registry.message = "World scheduled tick Registry binding";
        return registry_fail(world, frame, &registry, registry.message);
    }
    if (mc_registry_block_m_get(classes, &array, &registry) != MC_REGISTRY_OK)
        return registry_fail(world, frame, &registry, "World scheduled tick Block.m active use");
    const mc_block *block;
    frame->bytecode = 121;
    if (mc_registry_block_m_at(&array, current, &block, &registry) != MC_REGISTRY_OK)
        return registry_fail(world, frame, &registry, "World scheduled tick Block.m element");

    (void)mc_scheduled_tick_fields_get(record, &fields);
    mc_java_random *random = world->fields.r; /* Source getfield139, after m access. */
    frame->bytecode = 142;
    if (owned != NULL) {
        owned->native_stopped = true;
        mc_world_runtime_execution_result_note(
            owned->owned, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
            "Immediate scheduled virtual Block tick requires retained execution",
            (mc_entity_tick_execution_site){"fd", "c", "(IIIII)V", 142});
        return false;
    }
    if (!mc_world_mutation_block_tick(world, block, fields.x, fields.y, fields.z, random,
                                      &frame->tick)) {
        frame->registry = frame->tick.registry;
        return storage_fail(world, frame, frame->tick.cause);
    }
    return true;
}

static bool deferred(mc_world_runtime *world, mc_scheduled_tick *record, int32_t x, int32_t y,
                     int32_t z, int32_t block, int32_t delay, mc_world_schedule_error *frame) {
    bool present;
    if (!radius_present(world, x, y, z, 171, &present, frame))
        return false;
    if (!present)
        return true;

    if (block > 0) {
        /* This is the reached current metadata time only, not a store snapshot
         * or a preflight of unrelated Info fields. Nonpositive IDs skip it. */
        const int64_t signed_delay = (int64_t)delay;
        frame->bytecode = 191;
        int64_t time;
        if (mc_world_info_time_get(world->fields.x, &time) != MC_WORLD_INFO_OK)
            return fail(world, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "World scheduled tick Info time receiver");
        const int64_t due = mc_java_i64_add(signed_delay, time);
        frame->bytecode = 195;
        frame->record = mc_scheduled_tick_time_set(record, due);
        if (frame->record != MC_SCHEDULED_TICK_OK)
            return record_fail(world, frame, "World scheduled tick due time");
    }

    bool contained;
    frame->bytecode = 205;
    frame->membership = mc_scheduled_membership_contains(world->fields.F, record, &contained);
    if (frame->membership != MC_SCHEDULED_MEMBERSHIP_OK)
        return membership_fail(world, frame, "World scheduled tick membership contains");
    if (contained)
        return true;

    /* Keep the two live receiver loads and calls separate. Ordinary add false
     * is discarded; a later E failure retains the F publication and sequence. */
    bool added;
    frame->bytecode = 219;
    frame->membership = mc_scheduled_membership_add(world->fields.F, record, &added);
    if (frame->membership != MC_SCHEDULED_MEMBERSHIP_OK)
        return membership_fail(world, frame, "World scheduled tick membership add");
    frame->bytecode = 231;
    frame->ordered = mc_scheduled_ordered_add(world->fields.E, record, &added);
    if (frame->ordered != MC_SCHEDULED_ORDERED_OK)
        return ordered_fail(world, frame, "World scheduled tick ordered add");
    return true;
}

static bool schedule_held(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t block,
                          int32_t delay, mc_world_schedule_error *frame,
                          mc_world_schedule_execution *owned) {
    frame->bytecode = 0;
    mc_world_runtime_classes *classes = world->classes;
    if (classes == NULL)
        return fail(world, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World scheduled tick class owner");
    if (!classes->scheduled_initialized) {
        /* qy/ki.<clinit> is a separate first-active-use zero store. Its lifetime
         * is above Worlds and is never reset by another World constructor. */
        classes->scheduled_sequence.next = 0;
        classes->scheduled_initialized = true;
    }
    mc_scheduled_tick *record = NULL;
    frame->record = mc_scheduled_tick_create(&classes->scheduled_sequence, x, y, z, block, &record);
    if (frame->record != MC_SCHEDULED_TICK_OK)
        return record_fail(world, frame, "World scheduled tick record allocation");

    /* Original flag19 is read only after allocation/constructor0/9, including
     * requests that later prove absent, duplicate, mismatched or nonpositive. */
    const bool completed = world->fields.a ? immediate(world, record, frame, owned)
                                           : deferred(world, record, x, y, z, block, delay, frame);
    if (owned != NULL && !completed)
        owned->record = record;
    else
        mc_scheduled_tick_release(record);
    return completed;
}

mc_world_storage_status mc_world_runtime_schedule_class_get(const mc_world_runtime_classes *classes,
                                                            mc_world_schedule_class_state *out) {
    if (classes == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = (mc_world_schedule_class_state){.initialized = classes->scheduled_initialized,
                                           .next = classes->scheduled_sequence.next};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status admit_schedule(mc_world_runtime *world,
                                              mc_world_schedule_error *error) {
    /* Native admission does not alter existing World diagnostics or execute
     * scheduled-record class initialization, allocation or counter effects. */
    if (world == NULL)
        return admission(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                         "World scheduled tick arguments");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return admission(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World scheduled tick phase");
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status schedule(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                                        int32_t block, int32_t delay,
                                        mc_world_schedule_error *frame) {
    error_init(frame);
    mc_world_storage_error cause = {0};
    const mc_world_storage_status status = mc_world_runtime_call_enter(world, &cause);
    if (status != MC_WORLD_STORAGE_OK) {
        frame->status = cause.status;
        frame->operation = cause.operation;
        frame->cause = cause;
        return status;
    }
    const bool completed = schedule_held(world, x, y, z, block, delay, frame, NULL);
    mc_world_runtime_call_leave(world);
    return completed ? MC_WORLD_STORAGE_OK : frame->status;
}

mc_world_storage_status mc_world_runtime_schedule(mc_world_runtime *world, int32_t x, int32_t y,
                                                  int32_t z, int32_t block, int32_t delay,
                                                  mc_world_schedule_error *error) {
    mc_world_storage_status status = admit_schedule(world, error);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    /* Immediate ticks can synchronously schedule more fluid work. Retain each
     * complete diagnostic off the native stack until its descendants return. */
    mc_world_schedule_error *frame = malloc(sizeof(*frame));
    if (frame == NULL)
        return admission(error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                         "World scheduled tick diagnostic allocation");
    status = schedule(world, x, y, z, block, delay, frame);
    if (status == MC_WORLD_STORAGE_OK)
        error_init(error);
    else if (error != NULL)
        *error = *frame;
    free(frame);
    return status;
}

mc_world_storage_status mc_world_runtime_schedule_in_place(mc_world_runtime *world, int32_t x,
                                                           int32_t y, int32_t z, int32_t block,
                                                           int32_t delay,
                                                           mc_world_schedule_error *frame) {
    if (frame == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    const mc_world_storage_status status = admit_schedule(world, frame);
    return status == MC_WORLD_STORAGE_OK ? schedule(world, x, y, z, block, delay, frame) : status;
}

void mc_world_schedule_execution_init(mc_world_schedule_execution *frame,
                                      mc_entity_tick_execution *execution,
                                      mc_world_runtime_classes *code_classes,
                                      mc_world_runtime_execution_code code,
                                      mc_entity_tick_execution_world_token world,
                                      mc_entity_tick_execution_site receiver_site,
                                      mc_world_runtime_execution_result *owned) {
    if (frame != NULL)
        *frame = (mc_world_schedule_execution){.execution = execution,
                                               .code_classes = code_classes,
                                               .code = code,
                                               .world = world,
                                               .receiver_site = receiver_site,
                                               .owned = owned};
}

static bool schedule_records_quiet(const mc_world_runtime_execution_result *owned) {
    if (owned->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (owned->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (owned->closes[i].used)
            return false;
    return true;
}

bool mc_world_runtime_schedule_in_execution(mc_world_schedule_execution *frame,
                                            mc_world_runtime *world, int32_t x, int32_t y,
                                            int32_t z, int32_t block, int32_t delay,
                                            mc_world_storage_status *raw) {
    if (frame == NULL)
        return false;
    const char *operation = NULL;
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    if (frame->execution == NULL || frame->execution->identity != frame->execution ||
        frame->code_classes == NULL || frame->owned == NULL || world == NULL || raw == NULL ||
        frame->entered) {
        status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
        operation = "Scheduled execution/fresh outputs required";
    } else if (frame->owned->ownership_status != MC_WORLD_STORAGE_OK ||
               frame->owned->selection_ready || frame->owned->math_failed ||
               !schedule_records_quiet(frame->owned)) {
        status = MC_WORLD_STORAGE_BUSY;
        operation = "Scheduled quiet canonical result required";
    } else {
        mc_entity_tick_execution_world_token token;
        mc_world_access_failure_lease *lease = NULL;
        if (!mc_entity_tick_execution_current_world(frame->execution, &token) ||
            token.owner != frame->world.owner || token.slot != frame->world.slot ||
            token.generation != frame->world.generation) {
            status = MC_WORLD_STORAGE_BUSY;
            operation = "Scheduled exact evaluated World token";
        } else if ((status = mc_world_execution_child_lease(frame->execution, world, &lease)) !=
                   MC_WORLD_STORAGE_OK)
            operation = "Scheduled evaluated World ownership";
        else if (world->classes != frame->code_classes ||
                 world->construction.mode != MC_WORLD_RUNTIME_GAME ||
                 world->construction.phase == MC_WORLD_RUNTIME_RETIRED) {
            status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
            operation = "Scheduled same defining GAME owner";
        }
    }
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_execution_result_note(frame->owned, status, operation,
                                               frame->receiver_site);
        return false;
    }
    mc_world_storage_error cause = {0};
    status = mc_world_runtime_call_enter(world, &cause);
    if (status != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_execution_result_note(frame->owned, status, cause.operation,
                                               frame->receiver_site);
        return false;
    }
    frame->entered = frame->active = true;
    error_init(&frame->detail);
    const bool completed = schedule_held(world, x, y, z, block, delay, &frame->detail, frame);
    if (completed)
        error_init(&frame->detail);
    mc_world_runtime_call_leave(world);
    frame->active = false;
    frame->status = frame->detail.status;
    *raw = frame->status;
    frame->completed = completed && !frame->native_stopped &&
                       frame->owned->ownership_status == MC_WORLD_STORAGE_OK &&
                       !frame->owned->selection_ready && schedule_records_quiet(frame->owned);
    return frame->completed;
}

mc_world_storage_status mc_world_schedule_execution_retire(mc_world_schedule_execution **slot) {
    if (slot == NULL || *slot == NULL)
        return MC_WORLD_STORAGE_OK;
    mc_world_schedule_execution *frame = *slot;
    if (frame->active || !schedule_records_quiet(frame->owned))
        return MC_WORLD_STORAGE_BUSY;
    mc_scheduled_tick_release(frame->record);
    free(frame);
    *slot = NULL;
    return MC_WORLD_STORAGE_OK;
}
