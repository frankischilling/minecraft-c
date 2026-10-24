#include "mc/world/runtime/movement.h"

#include "../../entity/entity_internal.h"
#include "collision/blocks_internal.h"
#include "construction_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/environment.h"
#include "mutation/mutation_internal.h"
#include "observers_internal.h"
#include "provider_internal.h"
#include "runtime_internal.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "World movement dependencies require x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "World movement dependencies require strict floating transformations"
#endif
#include <emmintrin.h>

static mc_registry_error empty_registry(void) {
    return (mc_registry_error){.initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

mc_world_movement_error mc_world_movement_empty_error(void) {
    mc_world_movement_error frame = {0};
    frame.presentation = mc_world_mutation_empty_error();
    frame.block = mc_world_collision_empty_error();
    frame.registry = frame.presentation.registry;
    frame.query.registry = frame.registry;
    frame.biome.sampling.registry = frame.registry;
    return frame;
}

static bool fail(mc_world_movement_error *frame, mc_world_storage_status status,
                 const char *operation) {
    frame->status = status;
    frame->operation = operation;
    frame->cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool storage_failure(mc_world_movement_error *frame, mc_world_storage_error cause) {
    frame->status = cause.status;
    frame->operation = cause.operation;
    frame->cause = cause;
    return false;
}

static bool registry_failure(mc_world_runtime *world, mc_world_movement_error *frame,
                             mc_registry_error cause, const char *operation) {
    frame->registry = cause;
    frame->status =
        mc_world_runtime_registry_result(world, cause.status, &cause, operation, &frame->cause);
    frame->operation = frame->cause.operation;
    return false;
}

static bool registry_null(mc_world_runtime *world, mc_world_movement_error *frame,
                          const char *operation) {
    mc_registry_error cause = empty_registry();
    cause.status = MC_REGISTRY_NULL_REFERENCE;
    cause.message = operation;
    return registry_failure(world, frame, cause, operation);
}

static mc_world_storage_status dimension_status(mc_world_dimension_status status) {
    return status == MC_WORLD_DIMENSION_LIMIT               ? MC_WORLD_STORAGE_LIMIT
           : status == MC_WORLD_DIMENSION_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                            : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static mc_world_storage_status entity_status(mc_entity_status status) {
    return status == MC_ENTITY_LIMIT               ? MC_WORLD_STORAGE_LIMIT
           : status == MC_ENTITY_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
           : status == MC_ENTITY_BUSY              ? MC_WORLD_STORAGE_BUSY
                                                   : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static bool enter(mc_world_runtime *world, bool has_output, mc_world_movement_error *frame) {
    if (world == NULL || !has_output)
        return fail(frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World movement arguments");
    if (world->construction.mode != MC_WORLD_RUNTIME_GAME ||
        world->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World movement phase");
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_call_enter(world, &cause) != MC_WORLD_STORAGE_OK)
        return storage_failure(frame, cause);
    return true;
}

static mc_world_storage_status result(mc_world_movement_error *error,
                                      const mc_world_movement_error *frame) {
    if (error != NULL)
        *error = *frame;
    return frame->status;
}

static mc_world_storage_status finish(mc_world_runtime *world, mc_world_movement_error *frame,
                                      mc_world_movement_error *error) {
    if (frame->status == MC_WORLD_STORAGE_OK)
        *frame = mc_world_movement_empty_error();
    else
        world->last_error = frame->cause;
    mc_world_runtime_call_leave(world);
    return result(error, frame);
}

static bool fp_ready(mc_world_movement_error *frame, const char *operation) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation);
}

/* Complete fd/dj.a(IIIIII)Z and private f/g(II)Z. The caller already holds
 * this World. No Y payload, acquiring getter or Registry use is performed. */
static bool area_loaded_held(mc_world_runtime *world, int32_t min_x, int32_t min_y, int32_t min_z,
                             int32_t max_x, int32_t max_y, int32_t max_z, bool *value,
                             mc_world_movement_error *frame) {
    if (max_y < 0 || min_y >= 128) {
        *value = false;
        return true;
    }
    min_x = mc_java_i32_shr(min_x, 4);
    min_y = mc_java_i32_shr(min_y, 4);
    min_z = mc_java_i32_shr(min_z, 4);
    max_x = mc_java_i32_shr(max_x, 4);
    max_y = mc_java_i32_shr(max_y, 4);
    max_z = mc_java_i32_shr(max_z, 4);
    (void)min_y;
    (void)max_y;
    for (int32_t x = min_x; x <= max_x; x = mc_java_i32_add(x, 1)) {
        for (int32_t z = min_z; z <= max_z; z = mc_java_i32_add(z, 1)) {
            frame->area_bytecode = 69;
            frame->presence_bytecode = 6;
            frame->provider_bytecode = 6;
            bool present;
            mc_world_storage_error cause = {0};
            if (mc_world_runtime_provider_contains(world, x, z, &present, &cause) !=
                MC_WORLD_STORAGE_OK)
                return storage_failure(frame, cause);
            frame->area_bytecode = frame->presence_bytecode = frame->provider_bytecode = 0;
            if (!present) {
                *value = false;
                return true;
            }
        }
    }
    *value = true;
    return true;
}

mc_world_storage_status mc_world_runtime_area_loaded(mc_world_runtime *world, int32_t min_x,
                                                     int32_t min_y, int32_t min_z, int32_t max_x,
                                                     int32_t max_y, int32_t max_z, bool *out,
                                                     mc_world_movement_error *error) {
    mc_world_movement_error frame = mc_world_movement_empty_error();
    if (!enter(world, out != NULL, &frame))
        return result(error, &frame);
    bool value;
    if (area_loaded_held(world, min_x, min_y, min_z, max_x, max_y, max_z, &value, &frame))
        *out = value;
    else
        frame.bytecode = frame.area_bytecode;
    return finish(world, &frame, error);
}

static bool query_failure(mc_world_movement_error *frame) {
    frame->registry = frame->query.registry;
    frame->status = frame->query.status;
    frame->operation = frame->query.operation;
    frame->cause = frame->query.cause;
    return false;
}

/* World.a() returns current Dimension.b. Its captured manager, including a
 * fixed nullable e, stays alive through the actual point and c/d calls. */
static bool rain_biome(mc_world_runtime *world, int32_t x, int32_t z, bool *value,
                       mc_world_movement_error *frame) {
    frame->bytecode = 34;
    frame->dimension_bytecode = 4;
    mc_world_dimension_fields dimension;
    frame->dimension = mc_world_dimension_fields_get(world->fields.t, &dimension);
    if (frame->dimension != MC_WORLD_DIMENSION_OK)
        return fail(frame, dimension_status(frame->dimension), "World rain Dimension receiver");
    mc_world_dimension_biome *const manager = dimension.b;
    if (manager != NULL) {
        frame->dimension = mc_world_dimension_biome_retain(manager);
        if (frame->dimension != MC_WORLD_DIMENSION_OK)
            return fail(frame, dimension_status(frame->dimension), "World rain manager hold");
    }
    frame->dimension_bytecode = 0;
    frame->bytecode = 39;
    bool completed = false;
    if (manager == NULL) {
        (void)fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World rain point receiver");
        goto leave;
    }
    const mc_biome *biome = NULL;
    frame->dimension = mc_world_dimension_biome_point(manager, x, z, &biome, &frame->biome);
    if (frame->dimension != MC_WORLD_DIMENSION_OK) {
        frame->biome_bytecode = frame->biome.bytecode;
        frame->registry = frame->biome.sampling.registry;
        if (frame->registry.status != MC_REGISTRY_OK)
            (void)registry_failure(world, frame, frame->registry, "World rain point Biome");
        else
            (void)fail(frame, dimension_status(frame->dimension), frame->biome.operation);
        goto leave;
    }
    frame->bytecode = 46;
    const mc_biome_fields *fields = mc_biome_fields_get(biome);
    if (fields == NULL) {
        (void)registry_null(world, frame, "World rain Biome.c receiver");
        goto leave;
    }
    /* Actual kd/gs.c() is field v1, return4. It has no override among the
     * registered concrete Biome kinds. Snow suppresses the second call. */
    if (fields->v) {
        *value = false;
        completed = true;
        goto leave;
    }
    frame->bytecode = 56;
    fields = mc_biome_fields_get(biome);
    /* Actual d() reloads v1, returning false when true, otherwise reads w10.
     * No source class lookup, projected biome ID or copied climate is used. */
    *value = !fields->v && fields->w;
    completed = true;
leave:
    mc_world_dimension_biome_release(manager);
    return completed;
}

static bool rain_at_held(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, bool *value,
                         mc_world_movement_error *frame) {
    frame->bytecode = 1;
    frame->raining_bytecode = 2;
    frame->rain_strength_bytecode = 12;
    float rain;
    mc_world_storage_error cause = {0};
    if (mc_world_runtime_rain_strength(world, 1.0f, &rain, &cause) != MC_WORLD_STORAGE_OK)
        return storage_failure(frame, cause);
    frame->raining_bytecode = frame->rain_strength_bytecode = 0;
    if (!((double)rain > 0.2)) {
        *value = false;
        return true;
    }
    frame->bytecode = 13;
    bool visible;
    if (mc_world_runtime_sky_visible(world, x, y, z, &visible, &frame->query) !=
        MC_WORLD_STORAGE_OK)
        return query_failure(frame);
    if (!visible) {
        *value = false;
        return true;
    }
    frame->bytecode = 24;
    int32_t top;
    if (mc_world_runtime_top_solid_liquid(world, x, z, &top, &frame->query) != MC_WORLD_STORAGE_OK)
        return query_failure(frame);
    if (top > y) {
        *value = false;
        return true;
    }
    return rain_biome(world, x, z, value, frame);
}

mc_world_storage_status mc_world_runtime_rain_at(mc_world_runtime *world, int32_t x, int32_t y,
                                                 int32_t z, bool *out,
                                                 mc_world_movement_error *error) {
    mc_world_movement_error frame = mc_world_movement_empty_error();
    if (!enter(world, out != NULL, &frame))
        return result(error, &frame);
    bool value;
    if (rain_at_held(world, x, y, z, &value, &frame))
        *out = value;
    return finish(world, &frame, error);
}

static bool burning_static_id(mc_world_runtime *world, const char *name, unsigned int static_pc,
                              unsigned int field_pc, int32_t *value,
                              mc_world_movement_error *frame) {
    frame->bytecode = static_pc;
    mc_registry_classes *classes = mc_world_runtime_registry(world);
    mc_registry_error cause = empty_registry();
    if (classes == NULL) {
        cause.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
        cause.message = "World burning Registry binding";
        return registry_failure(world, frame, cause, cause.message);
    }
    const mc_block *block = NULL;
    if (mc_registry_block_static_get(classes, name, &block, &cause) != MC_REGISTRY_OK)
        return registry_failure(world, frame, cause, "World burning Block static");
    frame->bytecode = field_pc;
    const mc_block_fields *fields = mc_block_fields_get(block);
    if (fields == NULL)
        return registry_null(world, frame, "World burning Block.bn receiver");
    *value = fields->bn;
    return true;
}

static bool burning_held(mc_world_runtime *world, const mc_entity_bounds *bounds, bool *value,
                         mc_world_movement_error *frame) {
    frame->bytecode = 1;
    if (bounds == NULL)
        return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World burning bounds receiver");
    const double min_x = bounds->min_x;
    frame->bytecode = 4;
    if (!fp_ready(frame, "World burning floor FP admission"))
        return false;
    const int32_t low_x = mc_math_floor_double(min_x);
    const int32_t high_x = mc_math_floor_double(bounds->max_x + 1.0);
    const int32_t low_y = mc_math_floor_double(bounds->min_y);
    const int32_t high_y = mc_math_floor_double(bounds->max_y + 1.0);
    const int32_t low_z = mc_math_floor_double(bounds->min_z);
    const int32_t high_z = mc_math_floor_double(bounds->max_z + 1.0);
    frame->bytecode = 69;
    bool loaded;
    if (!area_loaded_held(world, low_x, low_y, low_z, high_x, high_y, high_z, &loaded, frame))
        return false;
    if (!loaded) {
        *value = false;
        return true;
    }
    for (int32_t x = low_x; x < high_x; x = mc_java_i32_add(x, 1)) {
        for (int32_t y = low_y; y < high_y; y = mc_java_i32_add(y, 1)) {
            for (int32_t z = low_z; z < high_z; z = mc_java_i32_add(z, 1)) {
                frame->bytecode = 113;
                int32_t id;
                /* The shared getter resets its complete lower attempt frame
                 * before acquisition; only this call's full child survives. */
                if (mc_world_collision_block_value_held(world, x, y, z, false, &id,
                                                        &frame->block) != MC_WORLD_STORAGE_OK) {
                    frame->query = frame->block.nearby.query;
                    frame->provider_bytecode = frame->query.provider_bytecode;
                    return query_failure(frame);
                }
                int32_t expected;
                if (!burning_static_id(world, "as", 120, 123, &expected, frame))
                    return false;
                if (id == expected) {
                    *value = true;
                    return true;
                }
                if (!burning_static_id(world, "D", 131, 134, &expected, frame))
                    return false;
                if (id == expected) {
                    *value = true;
                    return true;
                }
                if (!burning_static_id(world, "E", 142, 145, &expected, frame))
                    return false;
                if (id == expected) {
                    *value = true;
                    return true;
                }
            }
        }
    }
    *value = false;
    return true;
}

mc_world_storage_status mc_world_runtime_burning(mc_world_runtime *world,
                                                 const mc_entity_bounds *bounds, bool *out,
                                                 mc_world_movement_error *error) {
    mc_world_movement_error frame = mc_world_movement_empty_error();
    if (!enter(world, out != NULL, &frame))
        return result(error, &frame);
    bool value;
    if (burning_held(world, bounds, &value, &frame))
        *out = value;
    return finish(world, &frame, error);
}

static bool observer_cause(mc_world_runtime *world, mc_world_movement_error *frame, unsigned int pc,
                           mc_world_storage_error cause) {
    frame->bytecode = frame->observer_bytecode = pc;
    frame->presentation.observer_bytecode = pc;
    frame->presentation.status = cause.status;
    frame->presentation.operation = cause.operation;
    frame->presentation.cause = cause;
    world->last_error = cause;
    return storage_failure(frame, cause);
}

static bool entity_sound_held(mc_world_runtime *world, mc_entity *entity,
                              const mc_registry_string *name, float volume, float pitch,
                              mc_world_movement_error *frame) {
    for (size_t index = 0;; ++index) {
        mc_world_observer *observer = NULL;
        bool present = false;
        mc_world_storage_error cause = {0};
        if (!mc_world_observer_next(world, index, 9, 23, 28, &observer, &present,
                                    &frame->observer_bytecode, &cause))
            return observer_cause(world, frame, frame->observer_bytecode, cause);
        if (!present)
            return true; /* return64, before any Entity/FP access */
        frame->bytecode = 33;
        if (entity == NULL) {
            mc_world_observer_release_held(observer);
            return fail(frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "World Entity sound Entity receiver");
        }
        const mc_entity_status held = mc_entity_retain(entity);
        if (held != MC_ENTITY_OK) {
            mc_world_observer_release_held(observer);
            frame->entity =
                (mc_entity_error){.status = held, .operation = "World Entity sound Entity hold"};
            return fail(frame, entity_status(held), frame->entity.operation);
        }
        const mc_entity_base_fields *fields = mc_entity_runtime_fields(entity);
        volatile double x = fields->x;
        frame->bytecode = 37;
        volatile double y = fields->y;
        frame->bytecode = 41;
        volatile float offset = fields->offset;
        frame->bytecode = 44;
        if (!fp_ready(frame, "World Entity sound FP admission")) {
            mc_entity_release(entity);
            mc_world_observer_release_held(observer);
            return false;
        }
        volatile double widened_offset = (double)offset;
        frame->bytecode = 45;
        volatile double sound_y = y - widened_offset;
        frame->bytecode = 47;
        volatile double z = fields->z;
        /* Even a NULL receiver or missing slot is tested only after the
         * Entity x/y/offset/z reads and actual y-offset argument arithmetic. */
        frame->bytecode = frame->observer_bytecode = 53;
        mc_world_storage_status status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        if (observer == NULL || observer->callbacks.sound == NULL)
            cause.operation = "World Entity sound observer invocation";
        else
            status = observer->callbacks.sound(observer->context, world, name, x, sound_y, z,
                                               volume, pitch, &cause);
        mc_entity_release(entity);
        mc_world_observer_release_held(observer);
        if (status != MC_WORLD_STORAGE_OK)
            return observer_cause(world, frame, 53,
                                  mc_world_observer_callback_error(status, cause));
        /* iinc58 follows the callback; the next receiver gets fresh live
         * Entity fields, including edits made by this callback. */
    }
}

mc_world_storage_status mc_world_runtime_entity_sound(mc_world_runtime *world, mc_entity *entity,
                                                      const mc_registry_string *name, float volume,
                                                      float pitch, mc_world_movement_error *error) {
    mc_world_movement_error frame = mc_world_movement_empty_error();
    if (!enter(world, true, &frame))
        return result(error, &frame);
    /* Already evaluated caller arguments are neither dereferenced nor changed.
     * The actual Entity-coordinate overload is not the coordinate-sound body. */
    (void)entity_sound_held(world, entity, name, volume, pitch, &frame);
    return finish(world, &frame, error);
}
