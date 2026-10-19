#include "mc/world/runtime/scheduling.h"

#include "../../block/registry/runtime_internal.h"
#include "construction_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"
#include "mutation/mutation_internal.h"
#include "presence_internal.h"
#include "runtime_internal.h"

#include <stddef.h>

static mc_world_schedule_error empty_error(void)
{
    const mc_world_mutation_error tick = mc_world_mutation_empty_error();
    return (mc_world_schedule_error){.registry = tick.registry, .tick = tick};
}

static mc_world_storage_status result(mc_world_schedule_error *out,
                                      const mc_world_schedule_error *frame)
{
    if (out != NULL)
        *out = *frame;
    return frame->status;
}

static bool storage_fail(mc_world_runtime *world, mc_world_schedule_error *frame,
                         mc_world_storage_error cause)
{
    frame->status = cause.status;
    frame->operation = cause.operation;
    frame->cause = cause;
    world->last_error = cause;
    return false;
}

static bool fail(mc_world_runtime *world, mc_world_schedule_error *frame,
                 mc_world_storage_status status, const char *operation)
{
    return storage_fail(world, frame, (mc_world_storage_error){status, operation, 0});
}

static bool record_fail(mc_world_runtime *world, mc_world_schedule_error *frame,
                        const char *operation)
{
    mc_world_storage_status status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (frame->record == MC_SCHEDULED_TICK_ALLOCATION_FAILED)
        status = MC_WORLD_STORAGE_ALLOCATION_FAILED;
    else if (frame->record == MC_SCHEDULED_TICK_LIMIT)
        status = MC_WORLD_STORAGE_LIMIT;
    return fail(world, frame, status, operation);
}

static bool membership_fail(mc_world_runtime *world, mc_world_schedule_error *frame,
                            const char *operation)
{
    mc_world_storage_status status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (frame->membership == MC_SCHEDULED_MEMBERSHIP_ALLOCATION_FAILED)
        status = MC_WORLD_STORAGE_ALLOCATION_FAILED;
    else if (frame->membership == MC_SCHEDULED_MEMBERSHIP_LIMIT ||
             frame->membership == MC_SCHEDULED_MEMBERSHIP_HOLD_LIMIT)
        status = MC_WORLD_STORAGE_LIMIT;
    return fail(world, frame, status, operation);
}

static bool ordered_fail(mc_world_runtime *world, mc_world_schedule_error *frame,
                         const char *operation)
{
    mc_world_storage_status status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (frame->ordered == MC_SCHEDULED_ORDERED_ALLOCATION_FAILED)
        status = MC_WORLD_STORAGE_ALLOCATION_FAILED;
    else if (frame->ordered == MC_SCHEDULED_ORDERED_LIMIT ||
             frame->ordered == MC_SCHEDULED_ORDERED_HOLD_LIMIT)
        status = MC_WORLD_STORAGE_LIMIT;
    return fail(world, frame, status, operation);
}

static bool registry_fail(mc_world_runtime *world, mc_world_schedule_error *frame,
                          const mc_registry_error *cause, const char *operation)
{
    frame->registry = *cause;
    frame->status =
        mc_world_runtime_registry_result(world, cause->status, cause, operation, &frame->cause);
    frame->operation = frame->cause.operation;
    return false;
}

static bool radius_present(mc_world_runtime *world, int32_t x, int32_t y, int32_t z,
                           unsigned int call_pc, bool *out, mc_world_schedule_error *frame)
{
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
                      mc_world_schedule_error *frame)
{
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
    if (mc_world_runtime_block_get(world, fields.x, fields.y, fields.z, &current, &cause) !=
        MC_WORLD_STORAGE_OK)
        return storage_fail(world, frame, cause);
    (void)mc_scheduled_tick_fields_get(record, &fields);
    if (current != fields.block || current <= 0)
        return true;

    mc_registry_classes *classes = mc_world_runtime_registry(world);
    mc_registry_error registry = empty_error().registry;
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
    if (!mc_world_mutation_block_tick(world, block, fields.x, fields.y, fields.z, random,
                                      &frame->tick)) {
        frame->registry = frame->tick.registry;
        return storage_fail(world, frame, frame->tick.cause);
    }
    return true;
}

static bool deferred(mc_world_runtime *world, mc_scheduled_tick *record, int32_t x, int32_t y,
                     int32_t z, int32_t block, int32_t delay, mc_world_schedule_error *frame)
{
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
                          int32_t delay, mc_world_schedule_error *frame)
{
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
    const bool completed = world->fields.a ? immediate(world, record, frame)
                                           : deferred(world, record, x, y, z, block, delay, frame);
    mc_scheduled_tick_release(record);
    return completed;
}

mc_world_storage_status mc_world_runtime_schedule_class_get(const mc_world_runtime_classes *classes,
                                                            mc_world_schedule_class_state *out)
{
    if (classes == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = (mc_world_schedule_class_state){.initialized = classes->scheduled_initialized,
                                           .next = classes->scheduled_sequence.next};
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_runtime_schedule(mc_world_runtime *world, int32_t x, int32_t y,
                                                  int32_t z, int32_t block, int32_t delay,
                                                  mc_world_schedule_error *error)
{
    mc_world_schedule_error frame = empty_error();
    /* Native admission does not alter existing World diagnostics or execute
     * scheduled-record class initialization, allocation or counter effects. */
    if (world == NULL) {
        frame.status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
        frame.operation = "World scheduled tick arguments";
        frame.cause = (mc_world_storage_error){frame.status, frame.operation, 0};
        return result(error, &frame);
    }
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED) {
        frame.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        frame.operation = "World scheduled tick phase";
        frame.cause = (mc_world_storage_error){frame.status, frame.operation, 0};
        return result(error, &frame);
    }
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_call_enter(world, &cause) != MC_WORLD_STORAGE_OK) {
        frame.status = cause.status;
        frame.operation = cause.operation;
        frame.cause = cause;
        return result(error, &frame);
    }
    if (schedule_held(world, x, y, z, block, delay, &frame))
        frame = empty_error();
    mc_world_runtime_call_leave(world);
    return result(error, &frame);
}
