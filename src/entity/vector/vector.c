#include "vector_internal.h"

#include "mc/core/java/java.h"

#include <emmintrin.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Vectors require x86-64/SSE2"
#endif

#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Vectors require strict floating transformations"
#endif

_Static_assert(FLT_RADIX == 2 && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 &&
                   FLT_EVAL_METHOD == 0 && sizeof(double) == sizeof(uint64_t),
               "strict binary64 evaluation required");

static double java_add(double left, double right) {
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

mc_entity_status mc_entity_vector_initialize(mc_entity_classes *classes, mc_entity_error *error) {
    if (classes->vector_phase == MC_ENTITY_CLASS_READY)
        return MC_ENTITY_OK;
    if (classes->vector_phase == MC_ENTITY_CLASS_FAILED)
        return child_result(error, MC_ENTITY_CLASS_FAILED_STATUS, "vector class initialization", 0);
    if (classes->vector_phase != MC_ENTITY_CLASS_COLD)
        return child_result(error, MC_ENTITY_STATE_ERROR, "vector class initialization", 0);
    classes->vector_phase = MC_ENTITY_CLASS_INITIALIZING;
    mc_entity_vector_pool *pool =
        allocate(classes, sizeof(*pool), error, "vector class initialization", 0);
    if (pool == NULL) {
        classes->vector_phase = MC_ENTITY_CLASS_FAILED;
        return error->status;
    }
    *pool = (mc_entity_vector_pool){0};
    classes->vector_pool = pool; /* Original pool publication precedes cursor. */
    classes->vector_cursor = 0;
    classes->vector_phase = MC_ENTITY_CLASS_READY;
    return MC_ENTITY_OK;
}

static mc_entity_status result(mc_entity_vector_error *error, mc_entity_status status,
                               const char *operation, uint16_t bytecode) {
    error->status = status;
    error->operation = operation;
    error->bytecode = bytecode;
    return status;
}

static mc_entity_vector_object *find(mc_entity_classes *classes, const mc_entity_vector *vector) {
    for (mc_entity_vector_object *object = classes->vector_objects; object != NULL;
         object = object->next)
        if (&object->value == vector)
            return object;
    return NULL;
}

static mc_entity_status hold(mc_entity_classes *classes, mc_entity_vector_object *object) {
    if (object->holds == SIZE_MAX || classes->vector_holds == SIZE_MAX)
        return MC_ENTITY_LIMIT;
    ++object->holds;
    ++classes->vector_holds;
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_vector_create(mc_entity_classes *classes, double x, double y, double z,
                                         mc_entity_vector **out, mc_entity_vector_error *error) {
    mc_entity_vector_error local;
    if (error == NULL)
        error = &local;
    *error = (mc_entity_vector_error){0};
    const char *operation = "vector factory";
    if (classes == NULL || out == NULL)
        return result(error, MC_ENTITY_INVALID_ARGUMENT, operation, 0);
    mc_entity_status status = mc_entity_vector_initialize(classes, &error->pool);
    if (status != MC_ENTITY_OK)
        return result(error, status, operation, 0);
    mc_entity_vector_object *object =
        allocate(classes, sizeof(*object), &error->pool, operation, 0);
    if (object == NULL)
        return result(error, error->pool.status, operation, 0);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        retire(classes, object, sizeof(*object));
        return result(error, MC_ENTITY_UNSUPPORTED_FP, "vector constructor FP admission", 8);
    }
    *object = (mc_entity_vector_object){0};
    object->value.x = x == 0.0 ? 0.0 : x;
    object->value.y = y == 0.0 ? 0.0 : y;
    object->value.z = z == 0.0 ? 0.0 : z;
    status = hold(classes, object);
    if (status != MC_ENTITY_OK) {
        /* Native return ownership, not a Java failure at areturn11. Retire
         * only this unpublished box; prior class initialization remains. */
        retire(classes, object, sizeof(*object));
        return result(error, status, "vector factory return hold", 11);
    }
    object->next = classes->vector_objects;
    classes->vector_objects = object;
    *out = &object->value;
    return result(error, MC_ENTITY_OK, NULL, 0);
}

size_t mc_entity_vector_native_holds(const mc_entity_classes *classes,
                                     const mc_entity_vector *vector) {
    if (classes == NULL || vector == NULL)
        return 0;
    for (const mc_entity_vector_object *object = classes->vector_objects; object != NULL;
         object = object->next)
        if (&object->value == vector)
            return object->holds;
    return 0;
}

mc_entity_status mc_entity_vector_retain(mc_entity_classes *classes, mc_entity_vector *vector) {
    if (classes == NULL || vector == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_entity_vector_object *object = find(classes, vector);
    if (object == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    return hold(classes, object);
}

mc_entity_status mc_entity_vector_release(mc_entity_classes *classes, mc_entity_vector *vector) {
    if (classes == NULL || vector == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_entity_vector_object **link = &classes->vector_objects;
    while (*link != NULL && &(*link)->value != vector)
        link = &(*link)->next;
    mc_entity_vector_object *object = *link;
    if (object == NULL || object->holds == 0)
        return MC_ENTITY_INVALID_ARGUMENT;
    --object->holds;
    --classes->vector_holds;
    if (object->holds == 0 && !object->pooled) {
        *link = object->next;
        retire(classes, object, sizeof(*object));
    }
    return MC_ENTITY_OK;
}

static mc_entity_status grow(mc_entity_classes *classes, mc_entity_error *error) {
    mc_entity_vector_pool *pool = classes->vector_pool;
    if (pool->length < pool->capacity)
        return MC_ENTITY_OK;
    if (pool->length >= (size_t)INT32_MAX)
        return child_result(error, MC_ENTITY_LIMIT, "vector pool append", 0);
    /* Native capacity policy, not a claim about a particular JDK ArrayList.
     * The old and new arrays overlap until the complete copy is published. */
    size_t capacity = pool->capacity == 0 ? 10u : pool->capacity + pool->capacity / 2u;
    if (capacity > (size_t)INT32_MAX)
        capacity = (size_t)INT32_MAX;
    if (capacity <= pool->length)
        capacity = pool->length + 1u;
    if (capacity > SIZE_MAX / sizeof(*pool->elements))
        return child_result(error, MC_ENTITY_LIMIT, "vector pool append", 0);
    mc_entity_vector **elements =
        allocate(classes, capacity * sizeof(*elements), error, "vector pool append", 0);
    if (elements == NULL)
        return error->status;
    if (pool->length != 0)
        memcpy(elements, pool->elements, pool->length * sizeof(*elements));
    mc_entity_vector **previous = pool->elements;
    const size_t previous_capacity = pool->capacity;
    pool->elements = elements;
    pool->capacity = capacity;
    retire(classes, previous, previous_capacity * sizeof(*elements));
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_vector_pool_get(mc_entity_classes *classes, double x, double y, double z,
                                           mc_entity_vector **out, mc_entity_vector_error *error) {
    mc_entity_vector_error local;
    if (error == NULL)
        error = &local;
    *error = (mc_entity_vector_error){0};
    const char *operation = "vector pooled factory";
    if (classes == NULL || out == NULL)
        return result(error, MC_ENTITY_INVALID_ARGUMENT, operation, 0);
    mc_entity_status status = mc_entity_vector_initialize(classes, &error->pool);
    if (status != MC_ENTITY_OK)
        return result(error, status, operation, 0);
    mc_entity_vector_pool *pool = classes->vector_pool;
    if (pool->length > (size_t)INT32_MAX)
        return result(error, MC_ENTITY_STATE_ERROR, operation, 7);
    if (classes->vector_cursor >= (int32_t)pool->length) {
        mc_entity_vector_object *object =
            allocate(classes, sizeof(*object), &error->pool, "vector factory", 0);
        if (object == NULL)
            return result(error, error->pool.status, operation, 20);
        *object = (mc_entity_vector_object){0};
        status = grow(classes, &error->pool);
        if (status != MC_ENTITY_OK) {
            /* Unpublished native child, analogous to an unreachable new box.
             * No earlier source append/cursor/coordinate effect is undone. */
            retire(classes, object, sizeof(*object));
            return result(error, status, operation, 23);
        }
        object->pooled = true;
        object->next = classes->vector_objects;
        classes->vector_objects = object;
        pool->elements[pool->length++] = &object->value;
    }
    const int32_t index = classes->vector_cursor;
    classes->vector_cursor = mc_java_i32_add(index, 1); /* PC38 precedes get41. */
    if (index < 0 || (size_t)index >= pool->length)
        return result(error, MC_ENTITY_STATE_ERROR, operation, 41);
    mc_entity_vector *vector = pool->elements[(size_t)index];
    if (vector == NULL)
        return result(error, MC_ENTITY_STATE_ERROR, operation, 53);
    vector->x = x;
    vector->y = y;
    vector->z = z;
    /* This private pool slot refers to the initial member of its actual live
     * wrapper. C permits conversion back to that containing object. External
     * retain still validates by identity; source acquisition stays O(1). */
    status = hold(classes, (mc_entity_vector_object *)vector);
    if (status != MC_ENTITY_OK)
        return result(error, status, operation, 56);
    *out = vector;
    return result(error, MC_ENTITY_OK, NULL, 0);
}

static mc_entity_status reset(mc_entity_classes *classes, bool clear,
                              mc_entity_vector_error *error) {
    mc_entity_vector_error local;
    if (error == NULL)
        error = &local;
    *error = (mc_entity_vector_error){0};
    const char *operation = clear ? "vector client clear" : "vector pool reset";
    if (classes == NULL || (clear && classes->source != MC_ENTITY_CLIENT))
        return result(error, MC_ENTITY_INVALID_ARGUMENT, operation, 0);
    const mc_entity_status status = mc_entity_vector_initialize(classes, &error->pool);
    if (status != MC_ENTITY_OK)
        return result(error, status, operation, 0);
    if (clear) {
        mc_entity_vector_pool *pool = classes->vector_pool;
        for (size_t index = 0; index < pool->length; ++index)
            pool->elements[index] = NULL;
        pool->length = 0;
        mc_entity_vector_object **link = &classes->vector_objects;
        while (*link != NULL) {
            mc_entity_vector_object *object = *link;
            object->pooled = false;
            if (object->holds == 0) {
                *link = object->next;
                retire(classes, object, sizeof(*object));
            } else {
                link = &object->next;
            }
        }
    }
    classes->vector_cursor = 0;
    return result(error, MC_ENTITY_OK, NULL, 0);
}

mc_entity_status mc_entity_vector_pool_reset(mc_entity_classes *classes,
                                             mc_entity_vector_error *error) {
    return reset(classes, false, error);
}

mc_entity_status mc_entity_vector_pool_clear_client(mc_entity_classes *classes,
                                                    mc_entity_vector_error *error) {
    return reset(classes, true, error);
}

void mc_entity_vector_dispose(mc_entity_classes *classes) {
    mc_entity_vector_object *object = classes->vector_objects;
    while (object != NULL) {
        mc_entity_vector_object *next = object->next;
        retire(classes, object, sizeof(*object));
        object = next;
    }
    if (classes->vector_pool != NULL) {
        retire(classes, classes->vector_pool->elements,
               classes->vector_pool->capacity * sizeof(*classes->vector_pool->elements));
        retire(classes, classes->vector_pool, sizeof(*classes->vector_pool));
    }
}

mc_entity_status mc_entity_vector_get_snapshot(const mc_entity_classes *classes,
                                               mc_entity_vector_snapshot *out) {
    if (classes == NULL || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    *out = (mc_entity_vector_snapshot){
        classes->vector_phase,
        classes->vector_cursor,
        classes->vector_pool,
        classes->vector_pool == NULL ? 0 : classes->vector_pool->length,
        classes->vector_pool == NULL ? 0 : classes->vector_pool->capacity,
        classes->vector_holds};
    return MC_ENTITY_OK;
}

static mc_entity_status math_admit(const mc_entity_vector *value, bool output,
                                   mc_entity_vector_error *error, const char *operation) {
    *error = (mc_entity_vector_error){0};
    if (value == NULL || !output)
        return result(error, MC_ENTITY_INVALID_ARGUMENT, operation, 0);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return result(error, MC_ENTITY_UNSUPPORTED_FP, operation, 0);
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_vector_length(const mc_entity_vector *value, double *out,
                                         mc_entity_vector_error *error) {
    mc_entity_vector_error local;
    if (error == NULL)
        error = &local;
    if (math_admit(value, out != NULL, error, "vector length") != MC_ENTITY_OK)
        return error->status;
    volatile double xx = value->x * value->x;
    volatile double yy = value->y * value->y;
    volatile double xy = java_add(xx, yy);
    volatile double zz = value->z * value->z;
    volatile double square = java_add(xy, zz);
    /* in/fq.a(D)F narrows Math.sqrt to binary32 BEFORE widening to double. */
    const __m128d root64 = _mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(square));
    volatile float root = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), root64));
    *out = (double)root;
    return MC_ENTITY_OK;
}

mc_entity_status mc_entity_vector_add(mc_entity_classes *classes, const mc_entity_vector *value,
                                      double x, double y, double z, mc_entity_vector **out,
                                      mc_entity_vector_error *error) {
    mc_entity_vector_error local;
    if (error == NULL)
        error = &local;
    if (math_admit(value, classes != NULL && out != NULL, error, "vector add") != MC_ENTITY_OK)
        return error->status;
    const double vx = java_add(value->x, x), vy = java_add(value->y, y), vz = java_add(value->z, z);
    const mc_entity_status status = mc_entity_vector_pool_get(classes, vx, vy, vz, out, error);
    if (status != MC_ENTITY_OK) {
        error->factory = (mc_entity_error){
            .status = error->status, .operation = error->operation, .bytecode = error->bytecode};
        result(error, status, "vector add", 27);
    }
    return status;
}

mc_entity_status mc_entity_vector_normalize(mc_entity_classes *classes,
                                            const mc_entity_vector *value, mc_entity_vector **out,
                                            mc_entity_vector_error *error) {
    mc_entity_vector_error local;
    if (error == NULL)
        error = &local;
    if (math_admit(value, classes != NULL && out != NULL, error, "vector normalize") !=
        MC_ENTITY_OK)
        return error->status;
    double length;
    mc_entity_status status = mc_entity_vector_length(value, &length, error);
    if (status != MC_ENTITY_OK)
        return status;
    const bool zero = length < 1.0e-4; /* dcmpg: NaN takes the division branch. */
    const double x = zero ? 0.0 : value->x / length;
    const double y = zero ? 0.0 : value->y / length;
    const double z = zero ? 0.0 : value->z / length;
    status = mc_entity_vector_pool_get(classes, x, y, z, out, error);
    if (status != MC_ENTITY_OK) {
        error->factory = (mc_entity_error){
            .status = error->status, .operation = error->operation, .bytecode = error->bytecode};
        result(error, status, "vector normalize", zero ? 45 : 67);
    }
    return status;
}
