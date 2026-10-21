#include "mc/entity/bounds/bounds.h"

#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/entity/bounds/bounds_internal.h"

#include <emmintrin.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Geometry expectations are native contract tests, not an original-runtime
 * replay. Private layout changes below are quiescent native invariant controls;
 * they do not supply source state, entropy, or a gameplay execution route. */
static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_java_random_profile *profile;
    mc_entity_classes *classes;
} fixture;

static int fixture_open(fixture *test, mc_entity_source source) {
    if (mc_java_random_profile_create(sizeof(mc_java_random_profile), &test->profile) !=
        MC_JAVA_RANDOM_PROFILE_OK)
        return 1;
    if (mc_entity_classes_create(source, test->profile, NULL, &test->classes) != MC_ENTITY_OK) {
        mc_java_random_profile_free(test->profile);
        test->profile = NULL;
        return 1;
    }
    return 0;
}

static int fixture_close(fixture *test, int result) {
    if (test->classes != NULL) {
        /* Failed assertions still retire every real native hold they reached. */
        if (result != 0) {
            mc_entity_bounds_object *object = test->classes->bounds_objects;
            while (object != NULL) {
                mc_entity_bounds_object *next = object->next;
                const size_t holds = object->holds;
                for (size_t index = 0; index < holds; ++index)
                    if (mc_entity_bounds_release(test->classes, &object->value) != MC_ENTITY_OK)
                        return 1;
                object = next;
            }
        }
        if (mc_entity_classes_free(test->classes) != MC_ENTITY_OK) {
            fprintf(stderr, "bounds owner remained busy during cleanup\n");
            return 1;
        }
        test->classes = NULL;
    }
    mc_java_random_profile_free(test->profile);
    test->profile = NULL;
    return result;
}

static uint64_t bits(double value) {
    uint64_t result;
    _Static_assert(sizeof(result) == sizeof(value), "binary64 required");
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool same(double actual, double expected) {
    return isnan(expected) ? isnan(actual) : bits(actual) == bits(expected);
}

static bool bounds_same(const mc_entity_bounds *actual, const mc_entity_bounds *expected) {
    return same(actual->min_x, expected->min_x) && same(actual->min_y, expected->min_y) &&
           same(actual->min_z, expected->min_z) && same(actual->max_x, expected->max_x) &&
           same(actual->max_y, expected->max_y) && same(actual->max_z, expected->max_z);
}

static bool bounds_have_bits(const mc_entity_bounds *value, uint64_t minimum, uint64_t maximum) {
    return bits(value->min_x) == minimum && bits(value->min_y) == minimum &&
           bits(value->min_z) == minimum && bits(value->max_x) == maximum &&
           bits(value->max_y) == maximum && bits(value->max_z) == maximum;
}

static bool profile_cold(const fixture *test, uint64_t uniquifier) {
    return atomic_load_explicit(&test->profile->uniquifier, memory_order_relaxed) == uniquifier &&
           atomic_load_explicit(&test->profile->holder_phase, memory_order_acquire) ==
               MC_JAVA_RANDOM_HOLDER_COLD &&
           test->profile->math == NULL && test->profile->allocated_bytes == sizeof(*test->profile);
}

static int test_lifecycle(mc_entity_source source) {
    int result = 1;
    fixture test = {0};
    mc_entity_classes *other = NULL;
    mc_entity_bounds *first = NULL, *second = NULL, *reused = NULL, *fresh = NULL;
    mc_entity_bounds foreign = {0};
    mc_entity_bounds_error error;
    mc_entity_class_snapshot snapshot;
    const mc_entity_bounds original = {1, 2, 3, 4, 5, 6};
    const mc_entity_bounds replacement = {-8, -7, -6, -5, -4, -3};
    CHECK(fixture_open(&test, source) == 0);
    const uint64_t uniquifier =
        atomic_load_explicit(&test.profile->uniquifier, memory_order_relaxed);
    CHECK(mc_entity_classes_get_snapshot(test.classes, &snapshot) == MC_ENTITY_OK);
    CHECK(snapshot.entity_class == MC_ENTITY_CLASS_COLD &&
          snapshot.bounds_class == MC_ENTITY_CLASS_COLD &&
          snapshot.watcher_class == MC_ENTITY_CLASS_COLD && snapshot.next_id == 0 &&
          snapshot.live_entities == 0 && snapshot.bounds_pool_identity == NULL &&
          snapshot.allocation_bytes == sizeof(*test.classes));
    if (source == MC_ENTITY_SERVER) {
        CHECK(mc_entity_bounds_pool_clear_client(test.classes, &error) ==
              MC_ENTITY_INVALID_ARGUMENT);
        CHECK(test.classes->bounds_phase == MC_ENTITY_CLASS_COLD &&
              test.classes->bounds_pool == NULL &&
              test.classes->allocated_bytes == sizeof(*test.classes));
    }
    CHECK(mc_entity_bounds_pool_reset(test.classes, &error) == MC_ENTITY_OK);
    mc_entity_bounds_pool *pool = test.classes->bounds_pool;
    CHECK(pool != NULL && pool->elements == NULL && pool->length == 0 && pool->capacity == 0);
    CHECK(test.classes->bounds_phase == MC_ENTITY_CLASS_READY && test.classes->bounds_cursor == 0 &&
          test.classes->allocated_bytes == sizeof(*test.classes) + sizeof(*pool));
    CHECK(mc_entity_bounds_pool_get(test.classes, 1, 2, 3, 4, 5, 6, &first, &error) ==
          MC_ENTITY_OK);
    CHECK(bounds_same(first, &original));
    CHECK(pool->length == 1 && pool->capacity >= 1 && pool->elements[0] == first &&
          test.classes->bounds_cursor == 1 && test.classes->bounds_holds == 1);
    CHECK(test.classes->allocated_bytes == sizeof(*test.classes) + sizeof(*pool) +
                                               sizeof(mc_entity_bounds_object) +
                                               pool->capacity * sizeof(*pool->elements));
    CHECK(mc_entity_bounds_pool_get(test.classes, 10, 20, 30, 40, 50, 60, &second, NULL) ==
          MC_ENTITY_OK);
    CHECK(first != second && bounds_same(first, &original) && pool->length == 2);
    CHECK(mc_entity_classes_free(test.classes) == MC_ENTITY_BUSY);
    CHECK(mc_entity_classes_create(source, test.profile, NULL, &other) == MC_ENTITY_OK);
    CHECK(mc_entity_bounds_retain(other, first) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(mc_entity_bounds_release(other, first) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(other->bounds_phase == MC_ENTITY_CLASS_COLD);
    CHECK(mc_entity_bounds_retain(test.classes, &foreign) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(mc_entity_bounds_release(test.classes, &foreign) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(mc_entity_bounds_retain(NULL, first) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(mc_entity_bounds_release(test.classes, NULL) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(mc_entity_bounds_retain(test.classes, first) == MC_ENTITY_OK);
    CHECK(test.classes->bounds_holds == 3);
    CHECK(mc_entity_bounds_pool_reset(test.classes, NULL) == MC_ENTITY_OK);
    CHECK(pool == test.classes->bounds_pool && pool->length == 2 &&
          test.classes->bounds_cursor == 0 && bounds_same(first, &original));
    CHECK(mc_entity_bounds_pool_get(test.classes, -8, -7, -6, -5, -4, -3, &reused, &error) ==
          MC_ENTITY_OK);
    CHECK(reused == first && bounds_same(first, &replacement) && test.classes->bounds_holds == 4 &&
          pool->length == 2);
    CHECK(mc_entity_bounds_release(test.classes, reused) == MC_ENTITY_OK);
    CHECK(mc_entity_bounds_release(test.classes, first) == MC_ENTITY_OK);
    CHECK(mc_entity_bounds_release(test.classes, second) == MC_ENTITY_OK);
    CHECK(test.classes->bounds_holds == 1);
    CHECK(mc_entity_bounds_release(test.classes, second) == MC_ENTITY_INVALID_ARGUMENT);
    const size_t capacity = pool->capacity;
    mc_entity_bounds **elements = pool->elements;
    if (source == MC_ENTITY_CLIENT) {
        CHECK(mc_entity_bounds_pool_clear_client(test.classes, &error) == MC_ENTITY_OK);
        CHECK(pool->length == 0 && pool->capacity == capacity && pool->elements == elements &&
              test.classes->bounds_cursor == 0 && bounds_same(first, &replacement));
        CHECK(test.classes->allocated_bytes == sizeof(*test.classes) + sizeof(*pool) +
                                                   capacity * sizeof(*elements) +
                                                   sizeof(mc_entity_bounds_object));
        CHECK(mc_entity_classes_free(test.classes) == MC_ENTITY_BUSY);
        CHECK(mc_entity_bounds_pool_get(test.classes, 9, 8, 7, 6, 5, 4, &fresh, &error) ==
              MC_ENTITY_OK);
        CHECK(fresh != first && pool->length == 1 && pool->elements[0] == fresh &&
              bounds_same(first, &replacement));
        CHECK(mc_entity_bounds_retain(test.classes, first) == MC_ENTITY_OK);
        CHECK(mc_entity_bounds_release(test.classes, first) == MC_ENTITY_OK);
        CHECK(mc_entity_bounds_release(test.classes, first) == MC_ENTITY_OK);
        first = NULL;
        CHECK(test.classes->allocated_bytes == sizeof(*test.classes) + sizeof(*pool) +
                                                   capacity * sizeof(*elements) +
                                                   sizeof(mc_entity_bounds_object));
        CHECK(mc_entity_bounds_release(test.classes, fresh) == MC_ENTITY_OK);
        CHECK(mc_entity_bounds_pool_clear_client(test.classes, NULL) == MC_ENTITY_OK);
        CHECK(test.classes->bounds_objects == NULL && pool->length == 0 &&
              pool->capacity == capacity && pool->elements == elements &&
              test.classes->allocated_bytes ==
                  sizeof(*test.classes) + sizeof(*pool) + capacity * sizeof(*elements));
    } else {
        const size_t allocated = test.classes->allocated_bytes;
        CHECK(mc_entity_bounds_pool_clear_client(test.classes, &error) ==
              MC_ENTITY_INVALID_ARGUMENT);
        CHECK(pool->length == 2 && pool->capacity == capacity && pool->elements == elements &&
              test.classes->bounds_cursor == 1 && test.classes->allocated_bytes == allocated &&
              bounds_same(first, &replacement));
        CHECK(mc_entity_bounds_release(test.classes, first) == MC_ENTITY_OK);
        CHECK(pool->length == 2 && test.classes->allocated_bytes == allocated);
    }
    CHECK(test.classes->bounds_holds == 0);
    CHECK(mc_entity_classes_get_snapshot(test.classes, &snapshot) == MC_ENTITY_OK);
    CHECK(snapshot.bounds_pool_identity == pool && snapshot.entity_class == MC_ENTITY_CLASS_COLD &&
          snapshot.watcher_class == MC_ENTITY_CLASS_COLD && snapshot.next_id == 0 &&
          snapshot.live_entities == 0 && snapshot.watcher_types_identity == NULL);
    CHECK(profile_cold(&test, uniquifier));
    result = 0;
cleanup:
    if (mc_entity_classes_free(other) != MC_ENTITY_OK)
        result = 1;
    return fixture_close(&test, result);
}

static int test_budgets(mc_entity_source source) {
    int result = 1;
    fixture test = {0};
    mc_entity_bounds sentinel = {99, 98, 97, 96, 95, 94};
    mc_entity_bounds *out = &sentinel;
    mc_entity_bounds_error error;
    CHECK(fixture_open(&test, source) == 0);
    CHECK(mc_entity_bounds_pool_reset(test.classes, NULL) == MC_ENTITY_OK);
    mc_entity_bounds_pool *pool = test.classes->bounds_pool;
    const size_t empty_bytes = test.classes->allocated_bytes;
    test.classes->limits.max_allocation_bytes = empty_bytes;
    CHECK(mc_entity_bounds_pool_get(test.classes, 0, 0, 0, 1, 1, 1, &out, &error) ==
          MC_ENTITY_LIMIT);
    CHECK(out == &sentinel && error.bytecode == 23 && error.pool.status == MC_ENTITY_LIMIT &&
          error.pool.bytecode == 0 && pool->length == 0 && test.classes->bounds_cursor == 0 &&
          test.classes->allocated_bytes == empty_bytes &&
          test.classes->bounds_phase == MC_ENTITY_CLASS_READY);
    test.classes->limits.max_allocation_bytes = empty_bytes + sizeof(mc_entity_bounds_object);
    CHECK(mc_entity_bounds_pool_get(test.classes, 0, 0, 0, 1, 1, 1, &out, &error) ==
          MC_ENTITY_LIMIT);
    CHECK(out == &sentinel && error.bytecode == 26 && error.pool.status == MC_ENTITY_LIMIT &&
          pool->length == 0 && pool->elements == NULL && test.classes->bounds_cursor == 0 &&
          test.classes->bounds_objects == NULL && test.classes->allocated_bytes == empty_bytes &&
          test.classes->bounds_phase == MC_ENTITY_CLASS_READY);
    test.classes->limits.max_allocation_bytes = SIZE_MAX;
    CHECK(mc_entity_bounds_pool_get(test.classes, 0, 0, 0, 1, 1, 1, &out, &error) == MC_ENTITY_OK);
    CHECK(mc_entity_bounds_release(test.classes, out) == MC_ENTITY_OK);
    const size_t original_capacity = pool->capacity;
    CHECK(original_capacity > 0 && original_capacity < 1024);
    while (pool->length < original_capacity) {
        CHECK(mc_entity_bounds_pool_get(test.classes, 2, 3, 4, 5, 6, 7, &out, &error) ==
              MC_ENTITY_OK);
        CHECK(mc_entity_bounds_release(test.classes, out) == MC_ENTITY_OK);
    }
    const size_t full_bytes = test.classes->allocated_bytes;
    mc_entity_bounds **old_elements = pool->elements;
    const uintptr_t old_elements_address = (uintptr_t)old_elements;
    /* Native capacity policy is inspected, not equated to a JDK ArrayList.
     * This limit fits the final enlarged ledger but not old/new coexistence. */
    const size_t next_capacity = original_capacity + original_capacity / 2u;
    const size_t final_bytes = full_bytes + sizeof(mc_entity_bounds_object) +
                               (next_capacity - original_capacity) * sizeof(*pool->elements);
    test.classes->limits.max_allocation_bytes = final_bytes;
    out = &sentinel;
    CHECK(mc_entity_bounds_pool_get(test.classes, 8, 9, 10, 11, 12, 13, &out, &error) ==
          MC_ENTITY_LIMIT);
    CHECK(out == &sentinel && error.bytecode == 26 && pool->length == original_capacity &&
          pool->capacity == original_capacity && pool->elements == old_elements &&
          test.classes->bounds_cursor == (int32_t)original_capacity &&
          test.classes->allocated_bytes == full_bytes &&
          test.classes->bounds_phase == MC_ENTITY_CLASS_READY);
    test.classes->limits.max_allocation_bytes =
        full_bytes + sizeof(mc_entity_bounds_object) + next_capacity * sizeof(*pool->elements);
    CHECK(mc_entity_bounds_pool_get(test.classes, 8, 9, 10, 11, 12, 13, &out, &error) ==
          MC_ENTITY_OK);
    CHECK(pool->length == original_capacity + 1u && pool->capacity == next_capacity &&
          (uintptr_t)pool->elements != old_elements_address &&
          test.classes->allocated_bytes == final_bytes &&
          test.classes->bounds_cursor == (int32_t)original_capacity + 1 &&
          test.classes->bounds_holds == 1);
    CHECK(mc_entity_bounds_release(test.classes, out) == MC_ENTITY_OK);
    CHECK(test.classes->bounds_holds == 0);
    result = 0;
cleanup:
    return fixture_close(&test, result);
}

static int test_class_failure(mc_entity_source source) {
    int result = 1;
    fixture test = {0};
    mc_entity_bounds sentinel = {0}, *out = &sentinel;
    mc_entity_bounds_error error;
    CHECK(fixture_open(&test, source) == 0);
    const uint64_t uniquifier =
        atomic_load_explicit(&test.profile->uniquifier, memory_order_relaxed);
    test.classes->limits.max_allocation_bytes =
        sizeof(*test.classes) + sizeof(mc_entity_bounds_pool) - 1u;
    CHECK(mc_entity_bounds_pool_get(test.classes, 0, 0, 0, 1, 1, 1, &out, &error) ==
          MC_ENTITY_LIMIT);
    CHECK(out == &sentinel && error.bytecode == 0 && error.pool.status == MC_ENTITY_LIMIT &&
          test.classes->bounds_phase == MC_ENTITY_CLASS_FAILED &&
          test.classes->bounds_pool == NULL && test.classes->bounds_cursor == 0 &&
          test.classes->allocated_bytes == sizeof(*test.classes));
    test.classes->limits.max_allocation_bytes = SIZE_MAX;
    CHECK(mc_entity_bounds_pool_reset(test.classes, &error) == MC_ENTITY_CLASS_FAILED_STATUS);
    CHECK(error.pool.status == MC_ENTITY_CLASS_FAILED_STATUS && error.pool.bytecode == 0);
    CHECK(mc_entity_bounds_pool_get(test.classes, 0, 0, 0, 1, 1, 1, &out, &error) ==
          MC_ENTITY_CLASS_FAILED_STATUS);
    CHECK(out == &sentinel && test.classes->bounds_phase == MC_ENTITY_CLASS_FAILED &&
          test.classes->bounds_pool == NULL && test.classes->entity_phase == MC_ENTITY_CLASS_COLD &&
          test.classes->watcher_phase == MC_ENTITY_CLASS_COLD && profile_cold(&test, uniquifier));
    result = 0;
cleanup:
    return fixture_close(&test, result);
}

static int test_invariant_prefixes(mc_entity_source source) {
    int result = 1;
    fixture test = {0};
    mc_entity_bounds sentinel = {0}, *box = NULL, *out = &sentinel;
    mc_entity_bounds_error error;
    CHECK(fixture_open(&test, source) == 0);
    CHECK(mc_entity_bounds_pool_get(test.classes, 1, 2, 3, 4, 5, 6, &box, &error) == MC_ENTITY_OK);
    const mc_entity_bounds original = *box;
    mc_entity_bounds_object *object = test.classes->bounds_objects;
    /* A negative source cursor must be incremented before List.get rejects it. */
    test.classes->bounds_cursor = -1;
    const mc_entity_status negative =
        mc_entity_bounds_pool_get(test.classes, 9, 9, 9, 9, 9, 9, &out, &error);
    const bool negative_prefix = test.classes->bounds_cursor == 0;
    test.classes->bounds_cursor = 1;
    CHECK(negative == MC_ENTITY_STATE_ERROR && error.bytecode == 44 && negative_prefix &&
          out == &sentinel && bounds_same(box, &original) && test.classes->bounds_holds == 1);
    /* A null current List element fails only after the same cursor store. */
    test.classes->bounds_pool->elements[0] = NULL;
    test.classes->bounds_cursor = 0;
    const mc_entity_status null_slot =
        mc_entity_bounds_pool_get(test.classes, 9, 9, 9, 9, 9, 9, &out, &error);
    const bool null_prefix = test.classes->bounds_cursor == 1;
    test.classes->bounds_pool->elements[0] = box;
    CHECK(null_slot == MC_ENTITY_STATE_ERROR && error.bytecode == 62 && null_prefix &&
          out == &sentinel && bounds_same(box, &original));
    const size_t holds = object->holds;
    object->holds = SIZE_MAX;
    const mc_entity_status retained = mc_entity_bounds_retain(test.classes, box);
    const bool retained_unchanged = object->holds == SIZE_MAX && test.classes->bounds_holds == 1;
    object->holds = holds;
    CHECK(retained == MC_ENTITY_LIMIT && retained_unchanged);
    const size_t total_holds = test.classes->bounds_holds;
    test.classes->bounds_holds = SIZE_MAX;
    const mc_entity_status total = mc_entity_bounds_retain(test.classes, box);
    const bool total_unchanged = test.classes->bounds_holds == SIZE_MAX && object->holds == holds;
    test.classes->bounds_holds = total_holds;
    CHECK(total == MC_ENTITY_LIMIT && total_unchanged);
    /* Coordinate stores and cursor advancement precede native return-hold
     * admission. A hold failure does not roll those reached effects back. */
    CHECK(mc_entity_bounds_pool_reset(test.classes, NULL) == MC_ENTITY_OK);
    test.classes->bounds_holds = SIZE_MAX;
    const mc_entity_status acquisition =
        mc_entity_bounds_pool_get(test.classes, 7, 8, 9, 10, 11, 12, &out, &error);
    const bool acquisition_prefix =
        test.classes->bounds_holds == SIZE_MAX && object->holds == holds;
    test.classes->bounds_holds = total_holds;
    const mc_entity_bounds changed = {7, 8, 9, 10, 11, 12};
    CHECK(acquisition == MC_ENTITY_LIMIT && error.bytecode == 65 && acquisition_prefix &&
          out == &sentinel && test.classes->bounds_cursor == 1 && bounds_same(box, &changed));
    CHECK(mc_entity_bounds_release(test.classes, box) == MC_ENTITY_OK);
    CHECK(test.classes->bounds_holds == 0);
    result = 0;
cleanup:
    return fixture_close(&test, result);
}

typedef mc_entity_status (*transform)(mc_entity_classes *, const mc_entity_bounds *, double, double,
                                      double, mc_entity_bounds **, mc_entity_bounds_error *);

static int test_transforms(mc_entity_source source) {
    int result = 1;
    fixture test = {0};
    mc_entity_bounds *box = NULL, *out = NULL;
    mc_entity_bounds_error error;
    const transform operations[] = {mc_entity_bounds_sweep, mc_entity_bounds_expand,
                                    mc_entity_bounds_shift, mc_entity_bounds_contract};
    const mc_entity_bounds input = {1, 2, 3, 5, 7, 11};
    const mc_entity_bounds expected[] = {
        {-1, 2, -1, 5, 10, 11}, {3, -1, 7, 3, 10, 7}, {-1, 5, -1, 3, 10, 7}, {-1, 5, -1, 7, 4, 15}};
    const mc_entity_bounds nonfinite[] = {{1, -INFINITY, 3, INFINITY, 7, 11},
                                          {-INFINITY, INFINITY, NAN, INFINITY, -INFINITY, NAN},
                                          {INFINITY, -INFINITY, NAN, INFINITY, -INFINITY, NAN},
                                          {INFINITY, -INFINITY, NAN, -INFINITY, INFINITY, NAN}};
    const mc_entity_bounds zeros = {-0.0, -0.0, 0.0, 0.0, -0.0, -0.0};
    const mc_entity_bounds zero_results[] = {{-0.0, -0.0, 0.0, 0.0, -0.0, -0.0},
                                             {0.0, -0.0, 0.0, 0.0, 0.0, -0.0},
                                             {-0.0, 0.0, 0.0, 0.0, 0.0, -0.0},
                                             {-0.0, 0.0, 0.0, 0.0, -0.0, 0.0}};
    CHECK(fixture_open(&test, source) == 0);
    CHECK(mc_entity_bounds_pool_get(test.classes, 1, 2, 3, 5, 7, 11, &box, NULL) == MC_ENTITY_OK);
    for (size_t index = 0; index < sizeof(operations) / sizeof(*operations); ++index) {
        CHECK(mc_entity_bounds_assign(box, &input) == box);
        CHECK(mc_entity_bounds_pool_reset(test.classes, NULL) == MC_ENTITY_OK);
        CHECK(operations[index](test.classes, box, -2, 3, -4, &out, &error) == MC_ENTITY_OK);
        CHECK(out == box && bounds_same(out, &expected[index]) &&
              test.classes->bounds_pool->length == 1 && test.classes->bounds_cursor == 1 &&
              test.classes->bounds_holds == 2);
        CHECK(mc_entity_bounds_release(test.classes, out) == MC_ENTITY_OK);
        CHECK(mc_entity_bounds_pool_reset(test.classes, NULL) == MC_ENTITY_OK);
        CHECK(operations[index](test.classes, &input, INFINITY, -INFINITY, NAN, &out, &error) ==
              MC_ENTITY_OK);
        CHECK(out == box && bounds_same(out, &nonfinite[index]));
        CHECK(mc_entity_bounds_release(test.classes, out) == MC_ENTITY_OK);
        CHECK(mc_entity_bounds_assign(box, &zeros) == box);
        CHECK(mc_entity_bounds_pool_reset(test.classes, NULL) == MC_ENTITY_OK);
        CHECK(operations[index](test.classes, box, -0.0, 0.0, -0.0, &out, NULL) == MC_ENTITY_OK);
        CHECK(out == box && bounds_same(out, &zero_results[index]));
        CHECK(mc_entity_bounds_release(test.classes, out) == MC_ENTITY_OK);
    }
    CHECK(mc_entity_bounds_assign(box, &zeros) == box);
    CHECK(mc_entity_bounds_pool_reset(test.classes, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_bounds_copy(test.classes, box, &out, &error) == MC_ENTITY_OK);
    CHECK(out == box && bounds_same(out, &zeros));
    CHECK(mc_entity_bounds_release(test.classes, out) == MC_ENTITY_OK);
    const uint64_t nan_payload = UINT64_C(0x7ff8000000000137);
    double preserved_nan;
    memcpy(&preserved_nan, &nan_payload, sizeof(preserved_nan));
    box->min_x = preserved_nan;
    box->max_z = preserved_nan;
    CHECK(mc_entity_bounds_pool_reset(test.classes, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_bounds_copy(test.classes, box, &out, &error) == MC_ENTITY_OK);
    CHECK(out == box && bits(out->min_x) == nan_payload && bits(out->max_z) == nan_payload);
    CHECK(mc_entity_bounds_release(test.classes, out) == MC_ENTITY_OK);
    CHECK(mc_entity_bounds_assign(box, &zeros) == box);
    /* A full pool has no capacity for another child. Each transform must keep
     * its own call frame plus the actual failing factory and allocation child. */
    test.classes->limits.max_allocation_bytes = test.classes->allocated_bytes;
    const uint16_t bytecodes[] = {124, 62, 38, 62};
    for (size_t index = 0; index < sizeof(operations) / sizeof(*operations); ++index) {
        out = box;
        CHECK(operations[index](test.classes, &input, -2, 3, -4, &out, &error) == MC_ENTITY_LIMIT);
        CHECK(out == box && error.bytecode == bytecodes[index] &&
              error.factory.status == MC_ENTITY_LIMIT && error.factory.bytecode == 23 &&
              error.pool.status == MC_ENTITY_LIMIT && error.pool.bytecode == 0 &&
              test.classes->bounds_cursor == 1 && test.classes->bounds_pool->length == 1 &&
              bounds_same(box, &zeros));
    }
    CHECK(mc_entity_bounds_copy(test.classes, &input, &out, &error) == MC_ENTITY_LIMIT);
    CHECK(out == box && error.bytecode == 24 && error.factory.bytecode == 23 &&
          error.pool.status == MC_ENTITY_LIMIT);
    CHECK(mc_entity_bounds_release(test.classes, box) == MC_ENTITY_OK);
    CHECK(test.classes->bounds_holds == 0);
    result = 0;
cleanup:
    return fixture_close(&test, result);
}

static int test_nan_payloads(mc_entity_source source) {
    int result = 1;
    fixture test = {0};
    mc_entity_bounds *box = NULL, *out = NULL;

    /* Literal raw/quiet pairs cover both signs, two payloads per sign, and
     * quiet/signaling forms. The original interpreter's observed addition
     * selects the quieted right operand when both operands are NaNs;
     * subtraction retains the quieted left operand. Copies and an untaken
     * sweep branch perform no arithmetic and preserve signaling bits too. */
    const struct {
        uint64_t raw, quiet;
    } values[] = {{UINT64_C(0x7ff8000000000011), UINT64_C(0x7ff8000000000011)},
                  {UINT64_C(0x7ff80000000002a5), UINT64_C(0x7ff80000000002a5)},
                  {UINT64_C(0xfff8000000000137), UINT64_C(0xfff8000000000137)},
                  {UINT64_C(0xfff8000000005c43), UINT64_C(0xfff8000000005c43)},
                  {UINT64_C(0x7ff0000000000011), UINT64_C(0x7ff8000000000011)},
                  {UINT64_C(0x7ff00000000002a5), UINT64_C(0x7ff80000000002a5)},
                  {UINT64_C(0xfff0000000000137), UINT64_C(0xfff8000000000137)},
                  {UINT64_C(0xfff0000000005c43), UINT64_C(0xfff8000000005c43)}};

    CHECK(fixture_open(&test, source) == 0);
    CHECK(mc_entity_bounds_pool_get(test.classes, 0, 0, 0, 0, 0, 0, &box, NULL) == MC_ENTITY_OK);
    for (size_t left_index = 0; left_index < sizeof(values) / sizeof(*values); ++left_index) {
        double left;
        memcpy(&left, &values[left_index].raw, sizeof(left));
        const mc_entity_bounds input = {left, left, left, left, left, left};
        for (size_t right_index = 0; right_index < sizeof(values) / sizeof(*values);
             ++right_index) {
            double right;
            memcpy(&right, &values[right_index].raw, sizeof(right));
            const uint64_t raw_left = values[left_index].raw;
            const uint64_t quiet_left = values[left_index].quiet;
            const uint64_t quiet_right = values[right_index].quiet;
            CHECK(mc_entity_bounds_assign(box, &input) == box);
            CHECK(bounds_have_bits(box, raw_left, raw_left));
            CHECK(mc_entity_bounds_pool_reset(test.classes, NULL) == MC_ENTITY_OK);
            CHECK(mc_entity_bounds_pool_get(test.classes, left, left, left, left, left, left, &out,
                                            NULL) == MC_ENTITY_OK);
            CHECK(out == box && bounds_have_bits(out, raw_left, raw_left));
            CHECK(mc_entity_bounds_release(test.classes, out) == MC_ENTITY_OK);
            CHECK(mc_entity_bounds_pool_reset(test.classes, NULL) == MC_ENTITY_OK);
            CHECK(mc_entity_bounds_copy(test.classes, box, &out, NULL) == MC_ENTITY_OK);
            CHECK(out == box && bounds_have_bits(out, raw_left, raw_left));
            CHECK(mc_entity_bounds_release(test.classes, out) == MC_ENTITY_OK);
            CHECK(mc_entity_bounds_pool_reset(test.classes, NULL) == MC_ENTITY_OK);
            CHECK(mc_entity_bounds_sweep(test.classes, box, right, right, right, &out, NULL) ==
                  MC_ENTITY_OK);
            CHECK(out == box && bounds_have_bits(out, raw_left, raw_left));
            CHECK(mc_entity_bounds_release(test.classes, out) == MC_ENTITY_OK);
            CHECK(mc_entity_bounds_pool_reset(test.classes, NULL) == MC_ENTITY_OK);
            CHECK(mc_entity_bounds_expand(test.classes, box, right, right, right, &out, NULL) ==
                  MC_ENTITY_OK);
            CHECK(out == box && bounds_have_bits(out, quiet_left, quiet_right));
            CHECK(mc_entity_bounds_release(test.classes, out) == MC_ENTITY_OK);
            CHECK(mc_entity_bounds_assign(box, &input) == box);
            CHECK(mc_entity_bounds_pool_reset(test.classes, NULL) == MC_ENTITY_OK);
            CHECK(mc_entity_bounds_shift(test.classes, box, right, right, right, &out, NULL) ==
                  MC_ENTITY_OK);
            CHECK(out == box && bounds_have_bits(out, quiet_right, quiet_right));
            CHECK(mc_entity_bounds_release(test.classes, out) == MC_ENTITY_OK);
            CHECK(mc_entity_bounds_assign(box, &input) == box);
            CHECK(mc_entity_bounds_pool_reset(test.classes, NULL) == MC_ENTITY_OK);
            CHECK(mc_entity_bounds_contract(test.classes, box, right, right, right, &out, NULL) ==
                  MC_ENTITY_OK);
            CHECK(out == box && bounds_have_bits(out, quiet_right, quiet_left));
            CHECK(mc_entity_bounds_release(test.classes, out) == MC_ENTITY_OK);
            CHECK(mc_entity_bounds_assign(box, &input) == box);
            CHECK(mc_entity_bounds_translate(box, right, right, right) == box);
            CHECK(bounds_have_bits(box, quiet_right, quiet_right));
            CHECK(test.classes->bounds_pool->length == 1 && test.classes->bounds_cursor == 1 &&
                  test.classes->bounds_holds == 1);
        }
    }
    CHECK(mc_entity_bounds_release(test.classes, box) == MC_ENTITY_OK);
    CHECK(test.classes->bounds_holds == 0);
    result = 0;
cleanup:
    return fixture_close(&test, result);
}

/* Rotate the movement axis into x, y or z while keeping cross-axis geometry
 * independent. No arithmetic or collision algorithm is shared with production. */
static mc_entity_bounds axis_box(unsigned int axis, double low, double high) {
    mc_entity_bounds value = {0.5, 0.5, 0.5, 1.5, 1.5, 1.5};
    if (axis == 0) {
        value.min_x = low;
        value.max_x = high;
    } else if (axis == 1) {
        value.min_y = low;
        value.max_y = high;
    } else {
        value.min_z = low;
        value.max_z = high;
    }
    return value;
}

static void set_axis(mc_entity_bounds *value, unsigned int axis, double low, double high) {
    if (axis == 0) {
        value->min_x = low;
        value->max_x = high;
    } else if (axis == 1) {
        value->min_y = low;
        value->max_y = high;
    } else {
        value->min_z = low;
        value->max_z = high;
    }
}

static int test_geometry(void) {
    int result = 1;
    typedef double (*clip)(const mc_entity_bounds *, const mc_entity_bounds *, double);
    const clip clips[] = {mc_entity_bounds_clip_x, mc_entity_bounds_clip_y,
                          mc_entity_bounds_clip_z};

    const struct {
        double low, high, requested, expected;
    } cases[] = {
        {0, 1, 5, 1},        {0, 1, 0.5, 0.5},      {0, 1, -5, -5},
        {5, 6, -5, -1},      {5, 6, -0.5, -0.5},    {5, 6, 5, 5},
        {1, 2, 5, 0.0},      {4, 5, -5, 0.0},       {3, 3.5, 5, 5},
        {3, 3.5, -5, -5},    {0, 1, 0.0, 0.0},      {0, 1, -0.0, -0.0},
        {0, 1, INFINITY, 1}, {5, 6, -INFINITY, -1}, {0, 1, NAN, NAN},
        {0, NAN, 5, 5},      {NAN, 6, -5, -5},      {-INFINITY, -INFINITY, INFINITY, INFINITY},
        {1, 1, 5, 1},        {1, 0, 5, 2}};

    for (unsigned int axis = 0; axis < 3; ++axis) {
        const mc_entity_bounds fixed = axis_box(axis, 2, 4);
        for (size_t index = 0; index < sizeof(cases) / sizeof(*cases); ++index) {
            const mc_entity_bounds moving = axis_box(axis, cases[index].low, cases[index].high);
            CHECK(
                same(clips[axis](&fixed, &moving, cases[index].requested), cases[index].expected));
        }
        const mc_entity_bounds separated = axis_box(axis, 0, 1);
        CHECK(!mc_entity_bounds_intersects(&fixed, &separated));
        const mc_entity_bounds touching = axis_box(axis, 1, 2);
        CHECK(!mc_entity_bounds_intersects(&fixed, &touching));
        const mc_entity_bounds overlapping = axis_box(axis, 3, 5);
        CHECK(mc_entity_bounds_intersects(&fixed, &overlapping));
        CHECK(mc_entity_bounds_intersects(&fixed, &fixed));
        for (unsigned int cross = 0; cross < 3; ++cross) {
            if (cross == axis)
                continue;
            mc_entity_bounds moving = separated;
            set_axis(&moving, cross, -0.5, 0.5);
            CHECK(same(clips[axis](&fixed, &moving, 5), 5));
            CHECK(!mc_entity_bounds_intersects(&fixed, &moving));
            set_axis(&moving, cross, 1.5, 2.5);
            CHECK(same(clips[axis](&fixed, &moving, 5), 5));
            CHECK(!mc_entity_bounds_intersects(&fixed, &moving));
            /* Unordered comparisons do not reject cross-axis NaNs. */
            set_axis(&moving, cross, NAN, NAN);
            CHECK(same(clips[axis](&fixed, &moving, 5), 1));
            moving = overlapping;
            set_axis(&moving, cross, NAN, NAN);
            CHECK(mc_entity_bounds_intersects(&fixed, &moving));
            moving = separated;
            set_axis(&moving, cross, 1, 1);
            CHECK(same(clips[axis](&fixed, &moving, 5), 1));
            moving = overlapping;
            set_axis(&moving, cross, 1, 1);
            CHECK(mc_entity_bounds_intersects(&fixed, &moving));
        }
        mc_entity_bounds inverted = fixed;
        set_axis(&inverted, axis, 4, 2);
        CHECK(same(clips[axis](&inverted, &separated, 8), 3));
        mc_entity_bounds enclosing = axis_box(axis, 1, 5);
        CHECK(mc_entity_bounds_intersects(&inverted, &enclosing));
        set_axis(&enclosing, axis, 2.5, 3.5);
        CHECK(!mc_entity_bounds_intersects(&inverted, &enclosing));
        mc_entity_bounds unordered = fixed;
        set_axis(&unordered, axis, NAN, NAN);
        CHECK(mc_entity_bounds_intersects(&unordered, &separated));
        CHECK(same(clips[axis](&unordered, &separated, 5), 5));
    }
    mc_entity_bounds original = {1, -2, 3, 4, -5, 6}, target = {0};
    CHECK(mc_entity_bounds_assign(&target, &original) == &target);
    CHECK(bounds_same(&target, &original));
    CHECK(mc_entity_bounds_assign(&target, &target) == &target && bounds_same(&target, &original));
    CHECK(mc_entity_bounds_translate(&target, -3, 4, -5) == &target);
    const mc_entity_bounds shifted = {-2, 2, -2, 1, -1, 1};
    CHECK(bounds_same(&target, &shifted) &&
          bounds_same(&original, &(mc_entity_bounds){1, -2, 3, 4, -5, 6}));
    target = (mc_entity_bounds){-0.0, -0.0, 0.0, 0.0, -0.0, -0.0};
    CHECK(mc_entity_bounds_translate(&target, -0.0, 0.0, -0.0) == &target);
    CHECK(bounds_same(&target, &(mc_entity_bounds){-0.0, 0.0, 0.0, 0.0, 0.0, -0.0}));
    original = (mc_entity_bounds){INFINITY, -INFINITY, NAN, -INFINITY, INFINITY, NAN};
    CHECK(mc_entity_bounds_assign(&target, &original) == &target &&
          bounds_same(&target, &original));
    const uint64_t nan_payload = UINT64_C(0x7ff8000000000289);
    memcpy(&original.min_z, &nan_payload, sizeof(original.min_z));
    CHECK(mc_entity_bounds_assign(&target, &original) == &target &&
          bits(target.min_z) == nan_payload);
    CHECK(mc_entity_bounds_translate(&target, -INFINITY, INFINITY, 1) == &target);
    CHECK(bounds_same(&target, &(mc_entity_bounds){NAN, NAN, NAN, -INFINITY, INFINITY, NAN}));
    result = 0;
cleanup:
    return result;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fprintf(stderr, "bounds tests require the qualified strict SSE environment\n");
        return 1;
    }
    for (unsigned int index = 0; index < 2; ++index) {
        const mc_entity_source source = index == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER;
        if (test_lifecycle(source) || test_budgets(source) || test_class_failure(source) ||
            test_invariant_prefixes(source) || test_transforms(source) || test_nan_payloads(source))
            return 1;
    }
    if (test_geometry())
        return 1;
    printf("Entity bounds: %u checks passed (native contracts and invariant controls)\n", checks);
    return 0;
}
