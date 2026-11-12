#include "mc/world/runtime/presentation.h"

#include "construction_internal.h"
#include "execution_children_internal.h"
#include "mutation/mutation_internal.h"
#include "presentation_execution_internal.h"
#include "runtime_execution_internal.h"
#include "world_access_internal.h"

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

static bool presentation_quiet(const mc_world_runtime_execution_result *owned) {
    if (owned->selection_ready || owned->root_pending)
        return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_LOADERS; ++i)
        if (owned->pending[i].used)
            return false;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (owned->closes[i].used)
            return false;
    return true;
}

static bool presentation_note(mc_world_presentation_execution *frame,
                              mc_world_storage_status status, const char *operation) {
    mc_world_runtime_execution_result_note(frame->owned, status, operation, frame->receiver_site);
    return false;
}

static bool presentation_current(mc_world_presentation_execution *frame, mc_world_runtime *world) {
    if (frame->owned->ownership_status != MC_WORLD_STORAGE_OK)
        return false;
    mc_entity_tick_execution_world_token current;
    if (!mc_entity_tick_execution_current_world(frame->execution, &current))
        return presentation_note(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "World presentation checked current scope");
    if (current.owner != frame->world.owner || current.slot != frame->world.slot ||
        current.generation != frame->world.generation)
        return presentation_note(frame, MC_WORLD_STORAGE_BUSY,
                                 "World presentation exact evaluated scope changed");
    mc_world_access_failure_lease *lease = NULL;
    const mc_world_storage_status status =
        mc_world_execution_child_lease(frame->execution, world, &lease);
    if (status != MC_WORLD_STORAGE_OK)
        return presentation_note(frame, status, "World presentation evaluated scope admission");
    if (lease != frame->lease)
        return presentation_note(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "World presentation exact producer lease changed");
    return true;
}

static void presentation_callback(mc_world_presentation_execution *frame,
                                  mc_world_storage_status status, mc_world_mutation_error *error,
                                  const char *operation) {
    if (frame == NULL)
        return;
    frame->callback_status = status;
    if (frame->callbacks_returned != SIZE_MAX)
        ++frame->callbacks_returned;
    if (status != MC_WORLD_STORAGE_OK) {
        const mc_world_storage_status captured = mc_entity_tick_execution_capture_receipt(
            frame->execution, &error->world_access_failure);
        mc_world_runtime_execution_result_note(frame->owned, captured, operation,
                                               frame->receiver_site);
        if (captured != MC_WORLD_STORAGE_OK && frame->execution->capture_pending)
            error->world_access_failure = (mc_world_access_failure){0};
    }
}

static bool sound_body(mc_world_runtime *world, double x, double y, double z,
                       const mc_registry_string *name, float volume, float pitch,
                       mc_world_mutation_error *error, mc_world_presentation_execution *execution) {
    /* Preserve the original argument order and values. This body neither
     * dereferences the String nor draws Random; caller-side fizz draws have
     * already happened. A callback can edit u before the next size9/get23. */
    for (size_t index = 0;; ++index) {
        if (execution != NULL && !presentation_current(execution, world))
            return false;
        mc_world_access *world_access = NULL;
        bool present = false;
        mc_world_storage_error cause = {0};
        if (!mc_world_access_next(world, index, 9, 23, 28, &world_access, &present,
                                  &error->world_access_bytecode, &cause))
            return presentation_failure(world, error, cause);
        if (!present) {
            error->world_access_bytecode = 0;
            return true;
        }
        error->world_access_bytecode = 41;
        const mc_world_storage_status status =
            execution == NULL
                ? mc_world_access_call_sound(world_access, world, name, x, y, z, volume, pitch,
                                             &cause, &error->world_access_failure)
                : mc_world_access_call_sound_with_lease(&world_access, world, name, x, y, z, volume,
                                                        pitch, execution->lease, &cause,
                                                        &error->world_access_failure);
        presentation_callback(execution, status, error,
                              "World coordinate sound owned receipt capture");
        mc_world_access_release_held(world_access);
        if (status != MC_WORLD_STORAGE_OK)
            return presentation_failure(world, error,
                                        mc_world_access_callback_error(status, cause));
    }
}

bool mc_world_mutation_sound(mc_world_runtime *world, double x, double y, double z,
                             const mc_registry_string *name, float volume, float pitch,
                             mc_world_mutation_error *error) {
    return sound_body(world, x, y, z, name, volume, pitch, error, NULL);
}

static bool particle_body(mc_world_runtime *world, const mc_registry_string *name, double x,
                          double y, double z, double velocity_x, double velocity_y,
                          double velocity_z, mc_world_mutation_error *error,
                          mc_world_presentation_execution *execution) {
    /* No Math.random, default-Random construction or velocity adjustment is
     * performed here. Every caller draw retains its original interleaving. */
    for (size_t index = 0;; ++index) {
        if (execution != NULL && !presentation_current(execution, world))
            return false;
        mc_world_access *world_access = NULL;
        bool present = false;
        mc_world_storage_error cause = {0};
        if (!mc_world_access_next(world, index, 9, 23, 28, &world_access, &present,
                                  &error->world_access_bytecode, &cause))
            return presentation_failure(world, error, cause);
        if (!present) {
            error->world_access_bytecode = 0;
            return true;
        }
        error->world_access_bytecode = 43;
        const mc_world_storage_status status =
            execution == NULL
                ? mc_world_access_call_particle(world_access, world, name, x, y, z, velocity_x,
                                                velocity_y, velocity_z, &cause,
                                                &error->world_access_failure)
                : mc_world_access_call_particle_with_lease(
                      &world_access, world, name, x, y, z, velocity_x, velocity_y, velocity_z,
                      execution->lease, &cause, &error->world_access_failure);
        presentation_callback(execution, status, error, "World particle owned receipt capture");
        mc_world_access_release_held(world_access);
        if (status != MC_WORLD_STORAGE_OK)
            return presentation_failure(world, error,
                                        mc_world_access_callback_error(status, cause));
    }
}

bool mc_world_mutation_particle(mc_world_runtime *world, const mc_registry_string *name, double x,
                                double y, double z, double velocity_x, double velocity_y,
                                double velocity_z, mc_world_mutation_error *error) {
    return particle_body(world, name, x, y, z, velocity_x, velocity_y, velocity_z, error, NULL);
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

static mc_world_storage_status particle_run(mc_world_runtime *world, const mc_registry_string *name,
                                            double x, double y, double z, double velocity_x,
                                            double velocity_y, double velocity_z,
                                            mc_world_mutation_error *error) {
    mc_world_mutation_error frame = mc_world_mutation_empty_error();
    if (!presentation_enter(world, &frame))
        return presentation_result(error, &frame);
    const bool completed =
        particle_body(world, name, x, y, z, velocity_x, velocity_y, velocity_z, &frame, NULL);
    return presentation_finish(world, completed, &frame, error);
}

mc_world_storage_status mc_world_runtime_particle(mc_world_runtime *world,
                                                  const mc_registry_string *name, double x,
                                                  double y, double z, double velocity_x,
                                                  double velocity_y, double velocity_z,
                                                  mc_world_mutation_error *error) {
    return particle_run(world, name, x, y, z, velocity_x, velocity_y, velocity_z, error);
}

void mc_world_presentation_execution_init(mc_world_presentation_execution *frame,
                                          mc_entity_tick_execution *execution,
                                          mc_entity_tick_execution_site receiver_site,
                                          mc_world_runtime_execution_result *owned) {
    if (frame != NULL)
        *frame = (mc_world_presentation_execution){
            .execution = execution, .owned = owned, .receiver_site = receiver_site};
    mc_world_runtime_execution_result_init(owned);
}

static bool presentation_owned_enter(mc_world_presentation_execution *frame,
                                     mc_world_runtime *world, mc_world_mutation_error *error,
                                     mc_world_storage_status *status) {
    if (frame == NULL)
        return false;
    if (frame->execution == NULL || frame->execution->identity != frame->execution ||
        frame->owned == NULL || world == NULL || error == NULL || status == NULL || frame->entered)
        return presentation_note(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                 "World presentation execution/fresh outputs required");
    if (frame->owned->ownership_status != MC_WORLD_STORAGE_OK || !presentation_quiet(frame->owned))
        return presentation_note(frame, MC_WORLD_STORAGE_BUSY,
                                 "World presentation fresh result required");
    if (!mc_entity_tick_execution_current_world(frame->execution, &frame->world))
        return presentation_note(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                 "World presentation evaluated scope required");
    const mc_world_storage_status admitted =
        mc_world_execution_child_lease(frame->execution, world, &frame->lease);
    if (admitted != MC_WORLD_STORAGE_OK)
        return presentation_note(frame, admitted, "World presentation evaluated scope admission");
    frame->entered = true;
    return true;
}

static bool presentation_owned_finish(mc_world_presentation_execution *frame, bool completed,
                                      mc_world_mutation_error *error,
                                      mc_world_storage_status *status) {
    *status = completed ? MC_WORLD_STORAGE_OK : error->status;
    frame->completed = completed && frame->owned->ownership_status == MC_WORLD_STORAGE_OK;
    return frame->completed;
}

bool mc_world_mutation_sound_in_execution(mc_world_presentation_execution *frame,
                                          mc_world_runtime *world, double x, double y, double z,
                                          const mc_registry_string *name, float volume, float pitch,
                                          mc_world_mutation_error *error,
                                          mc_world_storage_status *status) {
    if (!presentation_owned_enter(frame, world, error, status))
        return false;
    return presentation_owned_finish(
        frame, sound_body(world, x, y, z, name, volume, pitch, error, frame), error, status);
}

bool mc_world_mutation_particle_in_execution(mc_world_presentation_execution *frame,
                                             mc_world_runtime *world,
                                             const mc_registry_string *name, double x, double y,
                                             double z, double velocity_x, double velocity_y,
                                             double velocity_z, mc_world_mutation_error *error,
                                             mc_world_storage_status *status) {
    if (!presentation_owned_enter(frame, world, error, status))
        return false;
    return presentation_owned_finish(
        frame,
        particle_body(world, name, x, y, z, velocity_x, velocity_y, velocity_z, error, frame),
        error, status);
}

bool mc_world_runtime_sound_in_execution(mc_world_presentation_execution *frame,
                                         mc_world_runtime *world, double x, double y, double z,
                                         const mc_registry_string *name, float volume, float pitch,
                                         mc_world_mutation_error *error,
                                         mc_world_storage_status *status) {
    if (!presentation_owned_enter(frame, world, error, status))
        return false;
    *error = mc_world_mutation_empty_error();
    if (!presentation_enter(world, error))
        return presentation_owned_finish(frame, false, error, status);
    const bool completed = sound_body(world, x, y, z, name, volume, pitch, error, frame);
    (void)presentation_finish(world, completed, error, error);
    return presentation_owned_finish(frame, completed, error, status);
}

bool mc_world_runtime_particle_frame_in_execution(
    mc_world_presentation_execution *frame, mc_world_runtime *world, const mc_registry_string *name,
    double x, double y, double z, double velocity_x, double velocity_y, double velocity_z,
    mc_world_mutation_error *error, mc_world_storage_status *status) {
    if (!presentation_owned_enter(frame, world, error, status))
        return false;
    *error = mc_world_mutation_empty_error();
    if (!presentation_enter(world, error))
        return presentation_owned_finish(frame, false, error, status);
    const bool completed =
        particle_body(world, name, x, y, z, velocity_x, velocity_y, velocity_z, error, frame);
    (void)presentation_finish(world, completed, error, error);
    return presentation_owned_finish(frame, completed, error, status);
}

static mc_world_storage_status
particle_owned_run(mc_entity_tick_execution *execution, mc_world_runtime *world,
                   const mc_registry_string *name, double x, double y, double z, double velocity_x,
                   double velocity_y, double velocity_z, mc_world_mutation_error *error,
                   mc_world_runtime_execution_result *native,
                   mc_entity_tick_execution_site receiver_site) {
    /* Preserve this older adapter's admission ordering and exact operation
     * identity, including evaluated nullable/mismatched World scopes. */
    mc_world_access_failure_lease *lease = NULL;
    const mc_world_storage_status admitted =
        mc_world_execution_child_lease(execution, world, &lease);
    if (admitted != MC_WORLD_STORAGE_OK) {
        mc_world_runtime_execution_result_note(
            native, admitted, "World particle actual owned admission", receiver_site);
        if (error != NULL) {
            mc_world_mutation_error_init(error);
            error->status = error->cause.status = admitted;
            error->operation = error->cause.operation =
                execution == NULL ? "World execution child context is required"
                                  : execution->admission.operation;
        }
        return admitted;
    }
    /* Old callers supply an already-initialized result, which must never be
     * reset here. This adapter frame contains only values/borrows; no future
     * cleanup or error points to its local storage. The bare adapter instead
     * uses a fresh local result solely to expose native-only refusal. */
    mc_world_runtime_execution_result local_native;
    if (native == NULL)
        mc_world_runtime_execution_result_init(&local_native);
    mc_world_runtime_execution_result *owned = native == NULL ? &local_native : native;
    mc_world_presentation_execution frame = {.execution = execution,
                                             .owned = owned,
                                             .receiver_site = receiver_site,
                                             .lease = lease,
                                             .entered = true};
    mc_world_mutation_error local_error = mc_world_mutation_empty_error();
    mc_world_mutation_error *out = error == NULL ? &local_error : error;
    mc_world_storage_status raw = MC_WORLD_STORAGE_OK;
    if (!mc_entity_tick_execution_current_world(execution, &frame.world)) {
        (void)presentation_note(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                "World particle checked current scope");
        mc_world_mutation_error_init(out);
        out->status = out->cause.status = owned->ownership_status;
        out->operation = out->cause.operation = owned->ownership_error.operation;
        return owned->ownership_status;
    }
    *out = mc_world_mutation_empty_error();
    if (!presentation_enter(world, out))
        return presentation_result(error, out);
    const bool completed =
        particle_body(world, name, x, y, z, velocity_x, velocity_y, velocity_z, out, &frame);
    raw = presentation_finish(world, completed, out, out);
    if (!completed && raw == MC_WORLD_STORAGE_OK && native == NULL) {
        out->status = out->cause.status = owned->ownership_status;
        out->operation = out->cause.operation = owned->ownership_error.operation;
        return owned->ownership_status;
    }
    return raw;
}

mc_world_storage_status
mc_world_runtime_particle_in_execution(mc_entity_tick_execution *execution, mc_world_runtime *world,
                                       const mc_registry_string *name, double x, double y, double z,
                                       double velocity_x, double velocity_y, double velocity_z,
                                       mc_world_mutation_error *error) {
    return particle_owned_run(execution, world, name, x, y, z, velocity_x, velocity_y, velocity_z,
                              error, NULL, (mc_entity_tick_execution_site){NULL, NULL, NULL, -1});
}

mc_world_storage_status mc_world_runtime_particle_detailed_in_execution(
    mc_entity_tick_execution *execution, mc_world_runtime *world, const mc_registry_string *name,
    double x, double y, double z, double velocity_x, double velocity_y, double velocity_z,
    mc_entity_tick_execution_site receiver_site, mc_world_mutation_error *error,
    mc_world_runtime_execution_result *native) {
    if (execution == NULL || native == NULL) {
        mc_world_runtime_execution_result_note(native, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "World particle execution/native result required",
                                               receiver_site);
        if (error != NULL) {
            mc_world_mutation_error_init(error);
            error->status = error->cause.status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
            error->operation = error->cause.operation =
                "World particle execution/native result required";
        }
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    }
    return particle_owned_run(execution, world, name, x, y, z, velocity_x, velocity_y, velocity_z,
                              error, native, receiver_site);
}
