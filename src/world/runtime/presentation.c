#include "mc/world/runtime/presentation.h"

#include "construction_internal.h"
#include "mutation/mutation_internal.h"

#include <stddef.h>
#include <stdint.h>

static mc_world_storage_status presentation_result(mc_world_mutation_error *out,
                                                   const mc_world_mutation_error *frame)
{
    if (out != NULL)
        *out = *frame;
    return frame->status;
}

static bool presentation_admit(mc_world_runtime *world, mc_world_mutation_error *frame)
{
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

static bool presentation_list_metadata(const mc_world_reference_list *list)
{
    return list != NULL && list->count <= list->capacity &&
           list->capacity <= SIZE_MAX / sizeof(*list->items) &&
           (list->capacity == 0 || list->items != NULL);
}

/* fd/dj.a(DDD,String,FF)V and a(String,DDDDDD)V have the same u loop until
 * their interface invocation. The enclosing caller holds this exact World.
 * The current native observer domain cannot complete any nonempty iteration,
 * so only source index0 is reachable. Successful future concrete observers
 * would require the real iinc46/48 and size9 reload on every next iteration. */
static bool presentation_observe_held(mc_world_runtime *world, unsigned int invocation_pc,
                                      const char *operation, mc_world_mutation_error *frame)
{
    frame->observer_bytecode = 9;
    const mc_world_reference_list *list = world->fields.u; /* getfield6 */
    if (!presentation_list_metadata(list))
        return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation);
    if (list->count == 0) {
        /* Actual source return52(sound)/54(particle). No argument or Registry
         * reference has been dereferenced, and no callback or RNG call ran. */
        frame->observer_bytecode = 0;
        return true;
    }

    frame->observer_bytecode = 23;
    list = world->fields.u; /* Separate source getfield18 before List.get23. */
    if (!presentation_list_metadata(list) || list->count == 0)
        return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation);
    const void *observer = list->items[0];
    if (observer != NULL) {
        frame->observer_bytecode = 28;
        /* The native list has untyped elements. Do not assert a pm/jp cast
         * succeeded or replace the missing concrete observer with success. */
        return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation);
    }

    /* checkcast28 accepts NULL. The original argument loads31..39/41 do not
     * dereference String; only invokeinterface41/43 then rejects the receiver. */
    frame->observer_bytecode = invocation_pc;
    return mc_world_mutation_fail(world, frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation);
}

static bool presentation_enter(mc_world_runtime *world, mc_world_mutation_error *frame)
{
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
                                                   mc_world_mutation_error *error)
{
    if (completed)
        *frame = mc_world_mutation_empty_error();
    mc_world_runtime_call_leave(world);
    return presentation_result(error, frame);
}

bool mc_world_mutation_sound(mc_world_runtime *world, double x, double y, double z,
                             const mc_registry_string *name, float volume, float pitch,
                             mc_world_mutation_error *error)
{
    /* Preserve source values, including NULL String and all floating bits.
     * No observer can currently return normally from a nonempty iteration.
     * Thus the arguments require no conversion, read, clipping or validation.
     * In particular, caller-side fizz draws remain before this actual call. */
    (void)x;
    (void)y;
    (void)z;
    (void)name;
    (void)volume;
    (void)pitch;
    return presentation_observe_held(world, 41, "World sound observer", error);
}

bool mc_world_mutation_particle(mc_world_runtime *world, const mc_registry_string *name, double x,
                                double y, double z, double velocity_x, double velocity_y,
                                double velocity_z, mc_world_mutation_error *error)
{
    /* No Math.random, default-Random construction or velocity adjustment is
     * performed here. Every caller draw retains its original interleaving. */
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)velocity_x;
    (void)velocity_y;
    (void)velocity_z;
    return presentation_observe_held(world, 43, "World particle observer", error);
}

mc_world_storage_status mc_world_runtime_sound(mc_world_runtime *world, double x, double y,
                                               double z, const mc_registry_string *name,
                                               float volume, float pitch,
                                               mc_world_mutation_error *error)
{
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
                                                  mc_world_mutation_error *error)
{
    mc_world_mutation_error frame = mc_world_mutation_empty_error();
    if (!presentation_enter(world, &frame))
        return presentation_result(error, &frame);
    const bool completed = mc_world_mutation_particle(world, name, x, y, z, velocity_x, velocity_y,
                                                      velocity_z, &frame);
    return presentation_finish(world, completed, &frame, error);
}
