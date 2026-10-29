#include "mc/world/runtime/brightness.h"

#include "construction_internal.h"
#include "runtime_internal.h"

#include <string.h>

static mc_world_brightness_error empty_error(void) {
    return (mc_world_brightness_error){.query = {.registry = {.initializer_pc = -1,
                                                              .block_id = -1,
                                                              .constructor_pc = -1,
                                                              .active_initializer_pc = -1,
                                                              .item_argument = -1}}};
}

static mc_world_storage_status fail(mc_world_brightness_error *error,
                                    mc_world_storage_status status, const char *operation,
                                    unsigned int bytecode, bool native_policy) {
    error->status = status;
    error->operation = operation;
    error->bytecode = bytecode;
    error->native_policy = native_policy;
    error->cause = (mc_world_storage_error){status, operation, 0};
    return status;
}

mc_world_storage_status mc_world_runtime_brightness(mc_world_runtime *world, int32_t x, int32_t y,
                                                    int32_t z, float *out,
                                                    mc_world_brightness_error *error) {
    mc_world_brightness_error frame = empty_error();
    if (world == NULL || out == NULL) {
        (void)fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World brightness arguments", 0,
                   true);
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED ||
        world->classes->side != MC_WORLD_INFO_CLIENT) {
        (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "World brightness client GAME admission", 0, true);
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    const mc_world_storage_status entered = mc_world_runtime_call_enter(world, &frame.cause);
    if (entered != MC_WORLD_STORAGE_OK) {
        (void)fail(&frame, entered, frame.cause.operation, 0, true);
        if (error != NULL)
            *error = frame;
        return frame.status;
    }
    mc_world_provider *captured = world->fields.t; /* Actual getfield1. */
    bool held = false;
    if (captured == NULL) {
        frame.source_exception = MC_WORLD_BRIGHTNESS_SOURCE_NULL_DIMENSION;
        (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "actual World brightness null Dimension receiver", 4, false);
        goto done;
    }
    frame.provider = mc_world_provider_retain(captured);
    if (frame.provider != MC_WORLD_PROVIDER_OK) {
        (void)fail(&frame,
                   frame.provider == MC_WORLD_PROVIDER_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                                             : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "World brightness captured Dimension native hold", 4, true);
        goto done;
    }
    held = true;
    mc_world_provider_fields fields;
    frame.provider = mc_world_provider_fields_get(captured, &fields);
    if (frame.provider != MC_WORLD_PROVIDER_OK) {
        (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "World brightness captured Dimension fields", 4, true);
        goto done;
    }
    const float *const table = fields.f; /* Actual getfield4; NULL is not read yet. */
    frame.query_reached = true;
    frame.bytecode = 11;
    frame.status = mc_world_runtime_block_light(world, x, y, z, &frame.light, &frame.query);
    if (frame.status != MC_WORLD_STORAGE_OK) {
        frame.operation = frame.query.operation;
        frame.cause = frame.query.cause;
        goto done;
    }
    if (table == NULL) {
        frame.source_exception = MC_WORLD_BRIGHTNESS_SOURCE_NULL_TABLE;
        (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "actual World brightness null float array", 14, false);
        goto done;
    }
    if (frame.light < 0 || frame.light >= 16) {
        frame.source_exception = MC_WORLD_BRIGHTNESS_SOURCE_INDEX;
        (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                   "actual World brightness float array index", 14, false);
        goto done;
    }
    memcpy(out, &table[frame.light], sizeof(*out));
    frame.bytecode = 15;

done:
    if (held)
        mc_world_provider_release(captured);
    if (frame.status != MC_WORLD_STORAGE_OK)
        world->last_error = frame.cause;
    mc_world_runtime_call_leave(world);
    if (error != NULL)
        *error = frame;
    return frame.status;
}
