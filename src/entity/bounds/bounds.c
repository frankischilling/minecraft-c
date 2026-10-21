#include "bounds_internal.h"

#include "mc/core/java/java.h"

#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Bounds require strict floating transformations"
#endif

_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 &&
                   FLT_EVAL_METHOD == 0 && sizeof(double) == sizeof(uint64_t),
               "strict binary64 evaluation required");

static double add(double left, double right) {
    uint64_t left_bits, right_bits;
    memcpy(&left_bits, &left, sizeof(left_bits));
    memcpy(&right_bits, &right, sizeof(right_bits));
    /* The captured x86-64 interpreter's dadd gives the top/right NaN priority,
     * quieting it without discarding sign or payload. C compilers may select
     * the other operand for commutative addition even under strict FP flags.
     * Keep ordinary arithmetic untouched and fix only this two-NaN case. */
    if ((left_bits & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000) &&
        (right_bits & UINT64_C(0x7fffffffffffffff)) > UINT64_C(0x7ff0000000000000)) {
        right_bits |= UINT64_C(0x0008000000000000);
        double value;
        memcpy(&value, &right_bits, sizeof(value));
        return value;
    }
    return left + right;
}

static mc_entity_status child_result(mc_entity_error *error, mc_entity_status status,
                                     const char *operation, uint16_t bytecode) {
    *error = (mc_entity_error){.status = status, .operation = operation, .bytecode = bytecode};
    return status;
}

static void *allocate(mc_entity_classes *classes, size_t bytes, mc_entity_error *error,
                      const char *operation, uint16_t bytecode) {
    if (classes->allocated_bytes > classes->limits.max_allocation_bytes ||
        bytes > classes->limits.max_allocation_bytes - classes->allocated_bytes) {
        child_result(error, MC_ENTITY_LIMIT, operation, bytecode);
        return NULL;
    }
    void *storage = malloc(bytes);
    if (storage == NULL) {
        child_result(error, MC_ENTITY_ALLOCATION_FAILED, operation, bytecode);
        return NULL;
    }
    classes->allocated_bytes += bytes;
    return storage;
}

static void retire(mc_entity_classes *classes, void *storage, size_t bytes) {
    if (storage != NULL) {
        free(storage);
        classes->allocated_bytes -= bytes;
    }
}

mc_entity_status mc_entity_bounds_initialize(mc_entity_classes *classes, mc_entity_error *error) {
    if (classes->bounds_phase == MC_ENTITY_CLASS_READY)
        return MC_ENTITY_OK;
    if (classes->bounds_phase == MC_ENTITY_CLASS_FAILED)
        return child_result(error, MC_ENTITY_CLASS_FAILED_STATUS, "bounds class initialization", 0);
    if (classes->bounds_phase != MC_ENTITY_CLASS_COLD)
        return child_result(error, MC_ENTITY_STATE_ERROR, "bounds class initialization", 0);
    classes->bounds_phase = MC_ENTITY_CLASS_INITIALIZING;
    mc_entity_bounds_pool *pool =
        allocate(classes, sizeof(*pool), error, "bounds class initialization", 0);
    if (pool == NULL) {
        classes->bounds_phase = MC_ENTITY_CLASS_FAILED;
        return error->status;
    }
    *pool = (mc_entity_bounds_pool){0};
    classes->bounds_pool = pool; /* Original pool publication precedes cursor. */
    classes->bounds_cursor = 0;
    classes->bounds_phase = MC_ENTITY_CLASS_READY;
    return MC_ENTITY_OK;
}

static mc_entity_status result(mc_entity_bounds_error *error, mc_entity_status status,
                               const char *operation, uint16_t bytecode) {
    error->status = status;
    error->operation = operation;
    error->bytecode = bytecode;
    return status;
}

static mc_entity_bounds_object *find(mc_entity_classes *classes, const mc_entity_bounds *bounds) {
    for (mc_entity_bounds_object *object = classes->bounds_objects; object != NULL;
         object = object->next)
        if (&object->value == bounds)
            return object;
    return NULL;
}

static mc_entity_status hold(mc_entity_classes *classes, mc_entity_bounds_object *object) {
    if (object->holds == SIZE_MAX || classes->bounds_holds == SIZE_MAX)
        return MC_ENTITY_LIMIT;
    ++object->holds;
    ++classes->bounds_holds;
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_bounds_retain(mc_entity_classes *classes, mc_entity_bounds *bounds) {
    if (classes == NULL || bounds == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_entity_bounds_object *object = find(classes, bounds);
    if (object == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    return hold(classes, object);
}

mc_entity_status mc_entity_bounds_release(mc_entity_classes *classes, mc_entity_bounds *bounds) {
    if (classes == NULL || bounds == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_entity_bounds_object **link = &classes->bounds_objects;
    while (*link != NULL && &(*link)->value != bounds)
        link = &(*link)->next;
    mc_entity_bounds_object *object = *link;
    if (object == NULL || object->holds == 0)
        return MC_ENTITY_INVALID_ARGUMENT;
    --object->holds;
    --classes->bounds_holds;
    if (object->holds == 0 && !object->pooled) {
        *link = object->next;
        retire(classes, object, sizeof(*object));
    }
    return MC_ENTITY_OK;
}

static mc_entity_status grow(mc_entity_classes *classes, mc_entity_error *error) {
    mc_entity_bounds_pool *pool = classes->bounds_pool;
    if (pool->length < pool->capacity)
        return MC_ENTITY_OK;
    if (pool->length >= (size_t)INT32_MAX)
        return child_result(error, MC_ENTITY_LIMIT, "bounds pool append", 0);
    /* Native capacity policy, not a claim about a particular JDK ArrayList.
     * The old and new arrays overlap until the complete copy is published. */
    size_t capacity = pool->capacity == 0 ? 10u : pool->capacity + pool->capacity / 2u;
    if (capacity > (size_t)INT32_MAX)
        capacity = (size_t)INT32_MAX;
    if (capacity <= pool->length)
        capacity = pool->length + 1u;
    if (capacity > SIZE_MAX / sizeof(*pool->elements))
        return child_result(error, MC_ENTITY_LIMIT, "bounds pool append", 0);
    mc_entity_bounds **elements =
        allocate(classes, capacity * sizeof(*elements), error, "bounds pool append", 0);
    if (elements == NULL)
        return error->status;
    if (pool->length != 0)
        memcpy(elements, pool->elements, pool->length * sizeof(*elements));
    mc_entity_bounds **previous = pool->elements;
    const size_t previous_capacity = pool->capacity;
    pool->elements = elements;
    pool->capacity = capacity;
    retire(classes, previous, previous_capacity * sizeof(*elements));
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_bounds_pool_get(mc_entity_classes *classes, double min_x, double min_y,
                                           double min_z, double max_x, double max_y, double max_z,
                                           mc_entity_bounds **out, mc_entity_bounds_error *error) {
    mc_entity_bounds_error local;
    if (error == NULL)
        error = &local;
    *error = (mc_entity_bounds_error){0};
    const char *operation = "bounds pooled factory";
    if (classes == NULL || out == NULL)
        return result(error, MC_ENTITY_INVALID_ARGUMENT, operation, 0);
    mc_entity_status status = mc_entity_bounds_initialize(classes, &error->pool);
    if (status != MC_ENTITY_OK)
        return result(error, status, operation, 0);
    mc_entity_bounds_pool *pool = classes->bounds_pool;
    if (pool->length > (size_t)INT32_MAX)
        return result(error, MC_ENTITY_STATE_ERROR, operation, 7);
    if (classes->bounds_cursor >= (int32_t)pool->length) {
        mc_entity_bounds_object *object =
            allocate(classes, sizeof(*object), &error->pool, "bounds factory", 0);
        if (object == NULL)
            return result(error, error->pool.status, operation, 23);
        *object = (mc_entity_bounds_object){0};
        status = grow(classes, &error->pool);
        if (status != MC_ENTITY_OK) {
            /* Unpublished native child, analogous to an unreachable new box.
             * No earlier source append/cursor/coordinate effect is undone. */
            retire(classes, object, sizeof(*object));
            return result(error, status, operation, 26);
        }
        object->pooled = true;
        object->next = classes->bounds_objects;
        classes->bounds_objects = object;
        pool->elements[pool->length++] = &object->value;
    }
    const int32_t index = classes->bounds_cursor;
    classes->bounds_cursor = mc_java_i32_add(index, 1); /* PC41 precedes get44. */
    if (index < 0 || (size_t)index >= pool->length)
        return result(error, MC_ENTITY_STATE_ERROR, operation, 44);
    mc_entity_bounds *bounds = pool->elements[(size_t)index];
    if (bounds == NULL)
        return result(error, MC_ENTITY_STATE_ERROR, operation, 62);
    bounds->min_x = min_x;
    bounds->min_y = min_y;
    bounds->min_z = min_z;
    bounds->max_x = max_x;
    bounds->max_y = max_y;
    bounds->max_z = max_z;
    /* This private pool slot refers to the initial member of its actual live
     * wrapper. C permits conversion back to that containing object. External
     * retain still validates by identity; source acquisition stays O(1). */
    status = hold(classes, (mc_entity_bounds_object *)bounds);
    if (status != MC_ENTITY_OK)
        return result(error, status, operation, 65);
    *out = bounds;
    return result(error, MC_ENTITY_OK, NULL, 0);
}

static mc_entity_status reset(mc_entity_classes *classes, bool clear,
                              mc_entity_bounds_error *error) {
    mc_entity_bounds_error local;
    if (error == NULL)
        error = &local;
    *error = (mc_entity_bounds_error){0};
    const char *operation = clear ? "bounds client clear" : "bounds pool reset";
    if (classes == NULL || (clear && classes->source != MC_ENTITY_CLIENT))
        return result(error, MC_ENTITY_INVALID_ARGUMENT, operation, 0);
    const mc_entity_status status = mc_entity_bounds_initialize(classes, &error->pool);
    if (status != MC_ENTITY_OK)
        return result(error, status, operation, 0);
    if (clear) {
        mc_entity_bounds_pool *pool = classes->bounds_pool;
        for (size_t index = 0; index < pool->length; ++index)
            pool->elements[index] = NULL;
        pool->length = 0;
        mc_entity_bounds_object **link = &classes->bounds_objects;
        while (*link != NULL) {
            mc_entity_bounds_object *object = *link;
            object->pooled = false;
            if (object->holds == 0) {
                *link = object->next;
                retire(classes, object, sizeof(*object));
            } else {
                link = &object->next;
            }
        }
    }
    classes->bounds_cursor = 0;
    return result(error, MC_ENTITY_OK, NULL, 0);
}

mc_entity_status mc_entity_bounds_pool_reset(mc_entity_classes *classes,
                                             mc_entity_bounds_error *error) {
    return reset(classes, false, error);
}

mc_entity_status mc_entity_bounds_pool_clear_client(mc_entity_classes *classes,
                                                    mc_entity_bounds_error *error) {
    return reset(classes, true, error);
}

void mc_entity_bounds_dispose(mc_entity_classes *classes) {
    mc_entity_bounds_object *object = classes->bounds_objects;
    while (object != NULL) {
        mc_entity_bounds_object *next = object->next;
        retire(classes, object, sizeof(*object));
        object = next;
    }
    if (classes->bounds_pool != NULL) {
        retire(classes, classes->bounds_pool->elements,
               classes->bounds_pool->capacity * sizeof(*classes->bounds_pool->elements));
        retire(classes, classes->bounds_pool, sizeof(*classes->bounds_pool));
    }
}

static mc_entity_status transformed(mc_entity_classes *classes, const mc_entity_bounds *value,
                                    mc_entity_bounds **out, mc_entity_bounds_error *error,
                                    const char *operation, uint16_t bytecode) {
    mc_entity_bounds_error local;
    if (error == NULL)
        error = &local;
    const mc_entity_status status =
        mc_entity_bounds_pool_get(classes, value->min_x, value->min_y, value->min_z, value->max_x,
                                  value->max_y, value->max_z, out, error);
    if (status != MC_ENTITY_OK) {
        /* Keep the reached pooled-factory frame as the child of this call. */
        error->factory = (mc_entity_error){
            .status = error->status, .operation = error->operation, .bytecode = error->bytecode};
        return result(error, status, operation, bytecode);
    }
    return status;
}

mc_entity_status mc_entity_bounds_sweep(mc_entity_classes *classes, const mc_entity_bounds *bounds,
                                        double x, double y, double z, mc_entity_bounds **out,
                                        mc_entity_bounds_error *error) {
    mc_entity_bounds value = *bounds;
    if (x < 0.0)
        value.min_x += x;
    if (x > 0.0)
        value.max_x += x;
    if (y < 0.0)
        value.min_y += y;
    if (y > 0.0)
        value.max_y += y;
    if (z < 0.0)
        value.min_z += z;
    if (z > 0.0)
        value.max_z += z;
    return transformed(classes, &value, out, error, "bounds sweep", 124);
}

mc_entity_status mc_entity_bounds_expand(mc_entity_classes *classes, const mc_entity_bounds *bounds,
                                         double x, double y, double z, mc_entity_bounds **out,
                                         mc_entity_bounds_error *error) {
    const mc_entity_bounds value = {bounds->min_x - x,     bounds->min_y - y,
                                    bounds->min_z - z,     add(bounds->max_x, x),
                                    add(bounds->max_y, y), add(bounds->max_z, z)};
    return transformed(classes, &value, out, error, "bounds expand", 62);
}

mc_entity_status mc_entity_bounds_shift(mc_entity_classes *classes, const mc_entity_bounds *bounds,
                                        double x, double y, double z, mc_entity_bounds **out,
                                        mc_entity_bounds_error *error) {
    const mc_entity_bounds value = {add(bounds->min_x, x), add(bounds->min_y, y),
                                    add(bounds->min_z, z), add(bounds->max_x, x),
                                    add(bounds->max_y, y), add(bounds->max_z, z)};
    return transformed(classes, &value, out, error, "bounds shift", 38);
}

mc_entity_status mc_entity_bounds_contract(mc_entity_classes *classes,
                                           const mc_entity_bounds *bounds, double x, double y,
                                           double z, mc_entity_bounds **out,
                                           mc_entity_bounds_error *error) {
    const mc_entity_bounds value = {add(bounds->min_x, x), add(bounds->min_y, y),
                                    add(bounds->min_z, z), bounds->max_x - x,
                                    bounds->max_y - y,     bounds->max_z - z};
    return transformed(classes, &value, out, error, "bounds contract", 62);
}

mc_entity_status mc_entity_bounds_copy(mc_entity_classes *classes, const mc_entity_bounds *bounds,
                                       mc_entity_bounds **out, mc_entity_bounds_error *error) {
    const mc_entity_bounds value = *bounds;
    return transformed(classes, &value, out, error, "bounds copy", 24);
}

double mc_entity_bounds_clip_x(const mc_entity_bounds *fixed, const mc_entity_bounds *moving,
                               double delta) {
    if (moving->max_y <= fixed->min_y || moving->min_y >= fixed->max_y)
        return delta;
    if (moving->max_z <= fixed->min_z || moving->min_z >= fixed->max_z)
        return delta;
    if (delta > 0.0 && moving->max_x <= fixed->min_x) {
        const double gap = fixed->min_x - moving->max_x;
        if (gap < delta)
            delta = gap;
    }
    if (delta < 0.0 && moving->min_x >= fixed->max_x) {
        const double gap = fixed->max_x - moving->min_x;
        if (gap > delta)
            delta = gap;
    }
    return delta;
}

double mc_entity_bounds_clip_y(const mc_entity_bounds *fixed, const mc_entity_bounds *moving,
                               double delta) {
    if (moving->max_x <= fixed->min_x || moving->min_x >= fixed->max_x)
        return delta;
    if (moving->max_z <= fixed->min_z || moving->min_z >= fixed->max_z)
        return delta;
    if (delta > 0.0 && moving->max_y <= fixed->min_y) {
        const double gap = fixed->min_y - moving->max_y;
        if (gap < delta)
            delta = gap;
    }
    if (delta < 0.0 && moving->min_y >= fixed->max_y) {
        const double gap = fixed->max_y - moving->min_y;
        if (gap > delta)
            delta = gap;
    }
    return delta;
}

double mc_entity_bounds_clip_z(const mc_entity_bounds *fixed, const mc_entity_bounds *moving,
                               double delta) {
    if (moving->max_x <= fixed->min_x || moving->min_x >= fixed->max_x)
        return delta;
    if (moving->max_y <= fixed->min_y || moving->min_y >= fixed->max_y)
        return delta;
    if (delta > 0.0 && moving->max_z <= fixed->min_z) {
        const double gap = fixed->min_z - moving->max_z;
        if (gap < delta)
            delta = gap;
    }
    if (delta < 0.0 && moving->min_z >= fixed->max_z) {
        const double gap = fixed->max_z - moving->min_z;
        if (gap > delta)
            delta = gap;
    }
    return delta;
}

bool mc_entity_bounds_intersects(const mc_entity_bounds *fixed, const mc_entity_bounds *moving) {
    if (moving->max_x <= fixed->min_x || moving->min_x >= fixed->max_x)
        return false;
    if (moving->max_y <= fixed->min_y || moving->min_y >= fixed->max_y)
        return false;
    if (moving->max_z <= fixed->min_z || moving->min_z >= fixed->max_z)
        return false;
    return true;
}

mc_entity_bounds *mc_entity_bounds_translate(mc_entity_bounds *target, double x, double y,
                                             double z) {
    target->min_x = add(target->min_x, x);
    target->min_y = add(target->min_y, y);
    target->min_z = add(target->min_z, z);
    target->max_x = add(target->max_x, x);
    target->max_y = add(target->max_y, y);
    target->max_z = add(target->max_z, z);
    return target;
}

mc_entity_bounds *mc_entity_bounds_assign(mc_entity_bounds *target,
                                          const mc_entity_bounds *source) {
    target->min_x = source->min_x;
    target->min_y = source->min_y;
    target->min_z = source->min_z;
    target->max_x = source->max_x;
    target->max_y = source->max_y;
    target->max_z = source->max_z;
    return target;
}
