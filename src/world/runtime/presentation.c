#include "mc/world/runtime/presentation.h"

#include "construction_internal.h"
#include "mutation/mutation_internal.h"
#include "observers_internal.h"

#include <stddef.h>
#include <stdint.h>

static mc_world_storage_status presentation_result(mc_world_mutation_error *out,
                                                   const mc_world_mutation_error *frame) {
    if (out != NULL)
        *out = *frame;
    return frame->status;
}

static bool presentation_admit(mc_world_runtime *world, mc_world_mutation_error *frame) {
    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
    const char *operation = NULL;
    if (world == NULL) {
        status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
        operation = "World presentation arguments";
    } else if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
               world->construction.phase == MC_WORLD_RUNTIME_RETIRED) {
        status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        operation = "World presentation phase";
    }
    if (status == MC_WORLD_STORAGE_OK)
        return true;

    /* Native admission has no reached source frame and changes no retained
     * World diagnostic. RAW/constructing/failed GAME prefixes remain admitted. */
    frame->status = status;
    frame->operation = operation;
    frame->cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool presentation_failure(mc_world_runtime *world, mc_world_mutation_error *frame,
                                 mc_world_storage_error cause) {
    frame->status = cause.status;
    frame->operation = cause.operation;
    frame->cause = cause;
    world->last_error = cause;
    return false;
}

static bool presentation_enter(mc_world_runtime *world, mc_world_mutation_error *frame) {
    if (!presentation_admit(world, frame))
        return false;

    mc_world_storage_error cause = {0};
    if (mc_world_runtime_call_enter(world, &cause) != MC_WORLD_STORAGE_OK) {
        frame->status = cause.status;
        frame->operation = cause.operation;
        frame->cause = cause;
        return false;
    }
    return true;
}

static mc_world_storage_status presentation_finish(mc_world_runtime *world, bool completed,
                                                   mc_world_mutation_error *frame,
                                                   mc_world_mutation_error *error) {
    if (completed)
        *frame = mc_world_mutation_empty_error();
    mc_world_runtime_call_leave(world);
    return presentation_result(error, frame);
}

bool mc_world_mutation_sound(mc_world_runtime *world, double x, double y, double z,
                             const mc_registry_string *name, float volume, float pitch,
                             mc_world_mutation_error *error) {
    /* Preserve the original argument order and values. This body neither
     * dereferences the String nor draws Random; caller-side fizz draws have
     * already happened. A callback can edit u before the next size9/get23. */
    for (size_t index = 0;; ++index) {
        mc_world_observer *observer = NULL;
        bool present = false;
        mc_world_storage_error cause = {0};
        if (!mc_world_observer_next(world, index, 9, 23, 28, &observer, &present,
                                    &error->observer_bytecode, &cause))
            return presentation_failure(world, error, cause);
        if (!present) {
            error->observer_bytecode = 0;
            return true;
        }
        error->observer_bytecode = 41;
        mc_world_storage_status status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        if (observer != NULL && observer->callbacks.sound != NULL)
            status = observer->callbacks.sound(observer->context, world, name, x, y, z, volume,
                                               pitch, &cause);
        else
            cause.operation = "World sound observer invocation";
        mc_world_observer_release_held(observer);
        if (status != MC_WORLD_STORAGE_OK)
            return presentation_failure(world, error,
                                        mc_world_observer_callback_error(status, cause));
    }
}

bool mc_world_mutation_particle(mc_world_runtime *world, const mc_registry_string *name, double x,
                                double y, double z, double velocity_x, double velocity_y,
                                double velocity_z, mc_world_mutation_error *error) {
    /* No Math.random, default-Random construction or velocity adjustment is
     * performed here. Every caller draw retains its original interleaving. */
    for (size_t index = 0;; ++index) {
        mc_world_observer *observer = NULL;
        bool present = false;
        mc_world_storage_error cause = {0};
        if (!mc_world_observer_next(world, index, 9, 23, 28, &observer, &present,
                                    &error->observer_bytecode, &cause))
            return presentation_failure(world, error, cause);
        if (!present) {
            error->observer_bytecode = 0;
            return true;
        }
        error->observer_bytecode = 43;
        mc_world_storage_status status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        if (observer != NULL && observer->callbacks.particle != NULL)
            status = observer->callbacks.particle(observer->context, world, name, x, y, z,
                                                  velocity_x, velocity_y, velocity_z, &cause);
        else
            cause.operation = "World particle observer invocation";
        mc_world_observer_release_held(observer);
        if (status != MC_WORLD_STORAGE_OK)
            return presentation_failure(world, error,
                                        mc_world_observer_callback_error(status, cause));
    }
}

mc_world_storage_status mc_world_runtime_sound(mc_world_runtime *world, double x, double y,
                                               double z, const mc_registry_string *name,
                                               float volume, float pitch,
                                               mc_world_mutation_error *error) {
    mc_world_mutation_error frame = mc_world_mutation_empty_error();
    if (!presentation_enter(world, &frame))
        return presentation_result(error, &frame);
    const bool completed = mc_world_mutation_sound(world, x, y, z, name, volume, pitch, &frame);
    return presentation_finish(world, completed, &frame, error);
}

mc_world_storage_status mc_world_runtime_particle(mc_world_runtime *world,
                                                  const mc_registry_string *name, double x,
                                                  double y, double z, double velocity_x,
                                                  double velocity_y, double velocity_z,
                                                  mc_world_mutation_error *error) {
    mc_world_mutation_error frame = mc_world_mutation_empty_error();
    if (!presentation_enter(world, &frame))
        return presentation_result(error, &frame);
    const bool completed = mc_world_mutation_particle(world, name, x, y, z, velocity_x, velocity_y,
                                                      velocity_z, &frame);
    return presentation_finish(world, completed, &frame, error);
}
