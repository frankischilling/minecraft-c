#include "mc/entity/hit_result.h"
#include "mc/entity/minecart/minecart.h"
#include "mc/entity/particles/particle.h"
#include "mc/entity/player/player.h"

#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/entity/hit_result/internal.h"
#include "../../../src/entity/vector/vector_internal.h"

#include <emmintrin.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct work {
    mc_java_random_profile *profile;
    mc_entity_classes *classes;
    uint64_t uniquifier;
    mc_entity_hit_error hit_error;
    mc_entity_geometry_error geometry;
    mc_entity_error entity_error;
    mc_entity_hit_result_view hit_view;
    mc_entity_hit_kind_class_view kind_view;
    mc_entity_hit_kind_view kind;
    mc_entity_bounds_intercept_view call_view;
    mc_entity_vector_snapshot vector_view;
    mc_entity_hit_result *hits[3];
    mc_entity *entities[5];
    mc_entity_vector *vectors[10];
    mc_entity_bounds_intercept_call *call;
    char line[2048];
} work;

static bool open_work(work *test, mc_entity_source source) {
    if (mc_java_random_profile_create(sizeof(mc_java_random_profile), &test->profile) !=
        MC_JAVA_RANDOM_PROFILE_OK)
        return false;
    test->uniquifier = atomic_load(&test->profile->uniquifier);
    return mc_entity_classes_create(source, test->profile, NULL, &test->classes) == MC_ENTITY_OK;
}

static bool close_work(work *test) {
    bool okay = true;
    if (test->call != NULL)
        okay =
            mc_entity_bounds_intercept_release(&test->call, &test->hit_error) == MC_ENTITY_HIT_OK;
    for (size_t index = 0; index < 3; ++index)
        if (mc_entity_hit_result_release(&test->hits[index], &test->hit_error) != MC_ENTITY_HIT_OK)
            okay = false;
    for (size_t index = 0; index < 5; ++index)
        mc_entity_release(test->entities[index]);
    for (size_t index = 0; index < 10; ++index)
        if (test->vectors[index] != NULL &&
            mc_entity_vector_release(test->classes, test->vectors[index]) != MC_ENTITY_OK)
            okay = false;
    if (test->classes != NULL && mc_entity_classes_free(test->classes) != MC_ENTITY_OK)
        okay = false;
    if (test->profile != NULL) {
        if (atomic_load(&test->profile->uniquifier) != test->uniquifier ||
            atomic_load(&test->profile->holder_phase) != MC_JAVA_RANDOM_HOLDER_COLD ||
            test->profile->math != NULL)
            okay = false;
        mc_java_random_profile_free(test->profile);
    }
    if (!okay)
        fprintf(stderr, "Hit/geometry ownership or no-Random cleanup failed\n");
    return okay;
}

static uint64_t bits(double value) {
    uint64_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool owners(mc_entity_source source) {
    bool result = false;
    work *test = calloc(1, sizeof(*test));
    if (test == NULL)
        return false;
    CHECK(open_work(test, source));
    CHECK(mc_entity_hit_result_allocate(test->classes, &test->hits[0], &test->hit_error) ==
          MC_ENTITY_HIT_OK);
    CHECK(test->classes->hit_kind_phase == MC_ENTITY_CLASS_COLD &&
          test->classes->entity_phase == MC_ENTITY_CLASS_COLD &&
          test->classes->vector_phase == MC_ENTITY_CLASS_COLD);
    CHECK(mc_entity_classes_free(test->classes) == MC_ENTITY_BUSY);
    CHECK(mc_entity_hit_result_construct_block(test->hits[0], -7, 9, INT32_MIN, 77, NULL,
                                               &test->hit_error) ==
          MC_ENTITY_HIT_SOURCE_NULL_REFERENCE);
    CHECK(!test->hit_error.native_policy && test->hit_error.site.entered &&
          test->hit_error.site.bytecode == 35);
    CHECK(mc_entity_hit_result_inspect(test->hits[0], &test->hit_view) == MC_ENTITY_HIT_OK);
    CHECK(test->hit_view.phase == MC_ENTITY_HIT_FAILED_CONSTRUCTION &&
          test->hit_view.fields.a != NULL && test->hit_view.fields.b == -7 &&
          test->hit_view.fields.c == 9 && test->hit_view.fields.d == INT32_MIN &&
          test->hit_view.fields.e == 77 && test->hit_view.fields.f == NULL &&
          test->hit_view.fields.g == NULL);
    CHECK(mc_entity_hit_kind_class_inspect(test->classes, &test->kind_view) == MC_ENTITY_HIT_OK);
    CHECK(test->kind_view.phase == MC_ENTITY_CLASS_READY &&
          test->kind_view.a == test->hit_view.fields.a && test->kind_view.a != test->kind_view.b &&
          test->kind_view.values_length == 2 && test->kind_view.values[0] == test->kind_view.a &&
          test->kind_view.values[1] == test->kind_view.b);
    CHECK(mc_entity_hit_kind_inspect(test->kind_view.a, &test->kind) == MC_ENTITY_HIT_OK);
    CHECK(test->kind.ordinal == 0 && test->kind.name != NULL && test->kind.name_length == 4 &&
          test->kind.name_units[0] == 'T' && test->kind.name_units[3] == 'E');
    CHECK(mc_entity_hit_kind_inspect(test->kind_view.b, &test->kind) == MC_ENTITY_HIT_OK);
    CHECK(test->kind.ordinal == 1 && test->kind.name_length == 6 &&
          test->kind.name_units[0] == 'E');
    CHECK(mc_entity_hit_result_construct_block(test->hits[0], 0, 0, 0, 0, NULL, &test->hit_error) ==
          MC_ENTITY_HIT_INVALID_PHASE);
    CHECK(mc_entity_hit_result_release(&test->hits[0], &test->hit_error) == MC_ENTITY_HIT_OK);
    CHECK(mc_entity_hit_result_allocate(test->classes, &test->hits[0], NULL) == MC_ENTITY_HIT_OK);
    CHECK(mc_entity_hit_result_construct_entity(test->hits[0], NULL, &test->hit_error) ==
          MC_ENTITY_HIT_SOURCE_NULL_REFERENCE);
    CHECK(test->hit_error.site.bytecode == 18 && !test->hit_error.native_policy);
    CHECK(mc_entity_hit_result_inspect(test->hits[0], &test->hit_view) == MC_ENTITY_HIT_OK &&
          test->hit_view.fields.a == test->kind_view.b && test->hit_view.fields.g == NULL &&
          test->hit_view.fields.b == 0 && test->hit_view.fields.f == NULL);
    CHECK(mc_entity_hit_result_release(&test->hits[0], NULL) == MC_ENTITY_HIT_OK);
    CHECK(mc_entity_hit_result_allocate(test->classes, &test->hits[0], NULL) == MC_ENTITY_HIT_OK);
    const mc_entity_vector point = {-0.0, 2.0, 3.0};
    CHECK(mc_entity_hit_result_construct_block(test->hits[0], 4, 5, 6, 2, &point,
                                               &test->hit_error) == MC_ENTITY_HIT_OK);
    CHECK(mc_entity_hit_result_inspect(test->hits[0], &test->hit_view) == MC_ENTITY_HIT_OK &&
          bits(test->hit_view.fields.f->x) == bits(-0.0));
    CHECK(mc_entity_hit_result_retain(test->hits[0], &test->hit_error) == MC_ENTITY_HIT_OK);
    test->hits[1] = test->hits[0];
    CHECK(mc_entity_hit_result_release(&test->hits[0], NULL) == MC_ENTITY_HIT_OK);
    CHECK(mc_entity_classes_free(test->classes) == MC_ENTITY_BUSY);
    CHECK(mc_entity_vector_pool_reset(test->classes, &test->geometry.vector) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_pool_get(test->classes, 11, 12, 13, &test->vectors[0],
                                    &test->geometry.vector) == MC_ENTITY_OK);
    CHECK(test->vectors[0] == test->hit_view.fields.f && test->hit_view.fields.f->x == 11);
    if (source == MC_ENTITY_CLIENT) {
        CHECK(mc_entity_vector_pool_clear_client(test->classes, &test->geometry.vector) ==
              MC_ENTITY_OK);
        CHECK(test->hit_view.fields.f->z == 13);
    }
    CHECK(mc_entity_hit_result_release(&test->hits[1], NULL) == MC_ENTITY_HIT_OK);
    CHECK(mc_item_entity_allocate(test->classes, &test->entities[0], &test->entity_error) ==
          MC_ENTITY_OK);
    test->entities[0]->base.x = 17;
    test->entities[0]->base.y = 18;
    test->entities[0]->base.z = 19;
    CHECK(mc_entity_hit_result_allocate(test->classes, &test->hits[0], NULL) == MC_ENTITY_HIT_OK);
    CHECK(mc_entity_hit_result_construct_entity(test->hits[0], test->entities[0],
                                                &test->hit_error) == MC_ENTITY_HIT_OK);
    CHECK(test->entities[0]->holds == 2 && test->entities[0]->phase == MC_ENTITY_RAW);
    CHECK(mc_entity_hit_result_inspect(test->hits[0], &test->hit_view) == MC_ENTITY_HIT_OK &&
          test->hit_view.fields.g == test->entities[0] && test->hit_view.fields.f->y == 18);
    mc_entity_release(test->entities[0]);
    test->entities[0] = NULL;
    CHECK(test->hit_view.fields.g->holds == 1 && test->hit_view.fields.g->base.z == 19);
    result = true;
cleanup:
    if (!close_work(test))
        result = false;
    free(test);
    return result;
}

static bool quotas(unsigned int stage) {
    bool result = false;
    work *test = calloc(1, sizeof(*test));
    if (test == NULL)
        return false;
    CHECK(open_work(test, MC_ENTITY_CLIENT));
    CHECK(mc_entity_hit_result_allocate(test->classes, &test->hits[0], NULL) == MC_ENTITY_HIT_OK);
    size_t budget = test->classes->allocated_bytes;
    if (stage > 0)
        budget += sizeof(mc_entity_hit_kind_state);
    if (stage > 1)
        budget += sizeof(mc_entity_hit_kind);
    if (stage > 2)
        budget += sizeof(mc_entity_hit_kind);
    test->classes->limits.max_allocation_bytes = budget;
    CHECK(mc_entity_hit_result_construct_block(test->hits[0], 1, 2, 3, 4, NULL, &test->hit_error) ==
          MC_ENTITY_HIT_NATIVE_FAILED);
    CHECK(test->hit_error.site.bytecode == 5 && test->hit_error.native_policy &&
          test->hit_error.kind.native.status == MC_ENTITY_LIMIT &&
          test->hit_error.kind.site.bytecode == (stage < 2    ? 0
                                                 : stage == 2 ? 13
                                                              : 27));
    CHECK(mc_entity_hit_kind_class_inspect(test->classes, &test->kind_view) == MC_ENTITY_HIT_OK);
    CHECK(test->kind_view.phase == MC_ENTITY_CLASS_FAILED &&
          test->kind_view.values_identity == NULL && (test->kind_view.a != NULL) == (stage >= 2) &&
          (test->kind_view.b != NULL) == (stage >= 3));
    CHECK(mc_entity_hit_result_inspect(test->hits[0], &test->hit_view) == MC_ENTITY_HIT_OK &&
          test->hit_view.fields.a == NULL && test->hit_view.fields.b == 0);
    test->classes->limits.max_allocation_bytes = MC_ENTITY_DEFAULT_ALLOCATION_BYTES;
    CHECK(mc_entity_hit_result_allocate(test->classes, &test->hits[1], NULL) == MC_ENTITY_HIT_OK);
    CHECK(mc_entity_hit_result_construct_entity(test->hits[1], NULL, &test->hit_error) ==
          MC_ENTITY_HIT_CLASS_FAILED);
    CHECK(test->hit_error.kind.native.status == MC_ENTITY_LIMIT && test->hit_error.native_policy);
    result = true;
cleanup:
    if (!close_work(test))
        result = false;
    free(test);
    return result;
}

static bool inputs(work *test, unsigned int mode, const double *start, const double *end) {
    if (mc_entity_vector_pool_reset(test->classes, &test->geometry.vector) != MC_ENTITY_OK)
        return false;
    mc_entity_status (*factory)(mc_entity_classes *, double, double, double, mc_entity_vector **,
                                mc_entity_vector_error *) =
        mode == 1 ? mc_entity_vector_pool_get : mc_entity_vector_create;
    if (factory(test->classes, 0, 0, 0, &test->vectors[0], &test->geometry.vector) != MC_ENTITY_OK)
        return false;
    if (mode == 2) {
        if (mc_entity_vector_retain(test->classes, test->vectors[0]) != MC_ENTITY_OK)
            return false;
        test->vectors[1] = test->vectors[0];
    } else if (factory(test->classes, 0, 0, 0, &test->vectors[1], &test->geometry.vector) !=
               MC_ENTITY_OK)
        return false;
    *test->vectors[0] = (mc_entity_vector){start[0], start[1], start[2]};
    if (mode != 2)
        *test->vectors[1] = (mc_entity_vector){end[0], end[1], end[2]};
    return mode != 1 ||
           mc_entity_vector_pool_reset(test->classes, &test->geometry.vector) == MC_ENTITY_OK;
}

static bool release_inputs(work *test) {
    for (size_t index = 0; index < 3; ++index) {
        if (test->vectors[index] != NULL &&
            mc_entity_vector_release(test->classes, test->vectors[index]) != MC_ENTITY_OK)
            return false;
        test->vectors[index] = NULL;
    }
    return true;
}

static bool intercept_controls(unsigned int budget_case) {
    bool result = false;
    work *test = calloc(1, sizeof(*test));
    if (test == NULL)
        return false;
    CHECK(open_work(test, MC_ENTITY_CLIENT));
    const double zero[3] = {0, 0, 0}, start[3] = {-2, -2, -2}, end[3] = {2, 2, 2};
    const mc_entity_bounds box = {-1, -1, -1, 1, 1, 1};
    CHECK(inputs(test, 0, zero, zero));
    const size_t before = test->classes->allocated_bytes;
    CHECK(mc_entity_bounds_intercept_begin(test->classes, &box, test->vectors[0], test->vectors[1],
                                           &test->call, &test->entity_error) == MC_ENTITY_HIT_OK);
    const size_t frame_bytes = test->classes->allocated_bytes - before;
    CHECK(mc_entity_bounds_intercept_release(&test->call, &test->hit_error) == MC_ENTITY_HIT_OK);
    *test->vectors[0] = (mc_entity_vector){start[0], start[1], start[2]};
    *test->vectors[1] = (mc_entity_vector){end[0], end[1], end[2]};
    if (budget_case != 0) {
        size_t extra = budget_case == 1 ? 0 : frame_bytes + 10 * sizeof(mc_entity_vector *);
        if (budget_case == 2)
            extra += 2 * sizeof(mc_entity_vector_object);
        if (budget_case >= 3)
            extra += 6 * sizeof(mc_entity_vector_object);
        if (budget_case == 4)
            extra += sizeof(mc_entity_hit_result);
        test->classes->limits.max_allocation_bytes = test->classes->allocated_bytes + extra;
    }
    const mc_entity_hit_status status = mc_entity_bounds_intercept_begin(
        test->classes, &box, test->vectors[0], test->vectors[1], &test->call, &test->entity_error);
    if (budget_case == 1) {
        CHECK(status == MC_ENTITY_HIT_NATIVE_FAILED && test->call == NULL &&
              test->entity_error.status == MC_ENTITY_LIMIT);
    } else {
        CHECK(test->call != NULL &&
              mc_entity_bounds_intercept_inspect(test->call, &test->call_view) == MC_ENTITY_HIT_OK);
        CHECK(mc_entity_classes_free(test->classes) == MC_ENTITY_BUSY);
        if (budget_case == 0) {
            CHECK(status == MC_ENTITY_HIT_OK && test->call_view.source_returned &&
                  test->call_view.face == 4 && test->call_view.result != NULL &&
                  test->call_view.result == test->call_view.candidate);
            for (size_t index = 0; index < 6; ++index)
                CHECK(test->call_view.generated[index] != NULL &&
                      test->call_view.locals[index] != NULL);
            CHECK(mc_entity_vector_get_snapshot(test->classes, &test->vector_view) ==
                      MC_ENTITY_OK &&
                  test->vector_view.cursor == 7);
            test->hits[0] = test->call_view.result;
            CHECK(mc_entity_hit_result_retain(test->hits[0], NULL) == MC_ENTITY_HIT_OK);
            CHECK(mc_entity_bounds_intercept_release(&test->call, NULL) == MC_ENTITY_HIT_OK);
            CHECK(mc_entity_hit_result_inspect(test->hits[0], &test->hit_view) ==
                      MC_ENTITY_HIT_OK &&
                  test->hit_view.fields.f->x == -1 && test->hit_view.fields.b == 0 &&
                  test->hit_view.native_holds == 1);
        } else if (budget_case == 2) {
            CHECK(status == MC_ENTITY_HIT_VECTOR_FAILED &&
                  test->call_view.error->site.bytecode == 27 &&
                  test->call_view.generated[0] != NULL && test->call_view.generated[1] != NULL &&
                  test->call_view.generated[2] == NULL && test->call_view.candidate == NULL &&
                  test->call_view.error->geometry.vector.pool.status == MC_ENTITY_LIMIT);
        } else if (budget_case == 3) {
            CHECK(status == MC_ENTITY_HIT_NATIVE_FAILED &&
                  test->call_view.error->site.bytecode == 384 &&
                  test->call_view.generated[5] != NULL && test->call_view.candidate == NULL);
        } else {
            CHECK(status == MC_ENTITY_HIT_NATIVE_FAILED &&
                  test->call_view.error->site.bytecode == 395 &&
                  test->call_view.candidate != NULL && test->call_view.result == NULL);
            CHECK(mc_entity_hit_result_inspect(test->call_view.candidate, &test->hit_view) ==
                      MC_ENTITY_HIT_OK &&
                  test->hit_view.phase == MC_ENTITY_HIT_FAILED_CONSTRUCTION &&
                  test->hit_view.fields.a == NULL &&
                  test->call_view.error->hit.kind.native.status == MC_ENTITY_LIMIT);
        }
    }
    result = true;
cleanup:
    if (!close_work(test))
        result = false;
    free(test);
    return result;
}

static bool classifications(void) {
    bool result = false;
    work *test = calloc(1, sizeof(*test)), *foreign = calloc(1, sizeof(*foreign));
    if (test == NULL || foreign == NULL) {
        free(test);
        free(foreign);
        return false;
    }
    CHECK(open_work(test, MC_ENTITY_CLIENT) && open_work(foreign, MC_ENTITY_CLIENT));
    CHECK(mc_item_entity_allocate(test->classes, &test->entities[0], NULL) == MC_ENTITY_OK);
    CHECK(mc_falling_entity_allocate(test->classes, &test->entities[1], NULL) == MC_ENTITY_OK);
    CHECK(mc_minecart_entity_allocate(test->classes, &test->entities[2], NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_allocate(test->classes, &test->entities[3], NULL) == MC_ENTITY_OK);
    CHECK(mc_pickup_particle_allocate(test->classes, &test->entities[4], NULL) == MC_ENTITY_OK);
    for (size_t index = 0; index < 5; ++index) {
        for (unsigned int dead = 0; dead < 2; ++dead) {
            test->entities[index]->base.be = dead != 0;
            bool pickable = true;
            float border = 0;
            CHECK(mc_entity_pickable(test->entities[index], &pickable, &test->hit_error) ==
                  MC_ENTITY_HIT_OK);
            CHECK(pickable == (index >= 1 && index <= 3 && dead == 0));
            CHECK(mc_entity_pick_border_size(test->entities[index], &border, &test->hit_error) ==
                      MC_ENTITY_HIT_OK &&
                  border == .1f);
            CHECK(test->entities[index]->holds == 1 &&
                  test->entities[index]->phase == MC_ENTITY_RAW);
        }
    }
    CHECK(mc_item_entity_allocate(foreign->classes, &foreign->entities[0], NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_hit_result_allocate(test->classes, &test->hits[0], NULL) == MC_ENTITY_HIT_OK);
    CHECK(mc_entity_hit_result_construct_entity(test->hits[0], foreign->entities[0],
                                                &test->hit_error) ==
          MC_ENTITY_HIT_INVALID_ARGUMENT);
    CHECK(mc_entity_hit_result_inspect(test->hits[0], &test->hit_view) == MC_ENTITY_HIT_OK &&
          test->hit_view.phase == MC_ENTITY_HIT_RAW);
    CHECK(mc_entity_vector_create(foreign->classes, 1, 2, 3, &foreign->vectors[0],
                                  &foreign->geometry.vector) == MC_ENTITY_OK);
    const mc_entity_bounds box = {-1, -1, -1, 1, 1, 1};
    CHECK(mc_entity_bounds_intercept_begin(test->classes, &box, foreign->vectors[0], NULL,
                                           &test->call,
                                           &test->entity_error) == MC_ENTITY_HIT_NATIVE_FAILED);
    CHECK(mc_entity_bounds_intercept_inspect(test->call, &test->call_view) == MC_ENTITY_HIT_OK &&
          test->call_view.error->native_policy && !test->call_view.error->site.entered);
    result = true;
cleanup:
    if (!close_work(test))
        result = false;
    if (!close_work(foreign))
        result = false;
    free(test);
    free(foreign);
    return result;
}

static bool edge_controls(void) {
    bool result = false;
    work *test = calloc(1, sizeof(*test));
    const unsigned int saved_csr = _mm_getcsr();
    if (test == NULL)
        return false;
    CHECK(open_work(test, MC_ENTITY_CLIENT));
    const double start[3] = {-2, 10, -2}, end[3] = {2, 10, 2};
    const mc_entity_bounds box = {-1, -1, -1, 1, 1, 1};
    double scalar = 999;
    bool inside = false;
    CHECK(mc_entity_vector_distance(test->classes, NULL, NULL, &scalar, &test->geometry) ==
          MC_ENTITY_GEOMETRY_SOURCE_NULL_REFERENCE);
    CHECK(scalar == 999 && !test->geometry.site.entered && !test->geometry.native_policy);
    CHECK(inputs(test, 0, start, end));
    CHECK(mc_entity_vector_distance(test->classes, test->vectors[0], NULL, &scalar,
                                    &test->geometry) == MC_ENTITY_GEOMETRY_SOURCE_NULL_REFERENCE);
    CHECK(scalar == 999 && test->geometry.site.entered && test->geometry.site.bytecode == 1);
    CHECK(mc_entity_bounds_contains_point(test->classes, &box, NULL, &inside, &test->geometry) ==
          MC_ENTITY_GEOMETRY_SOURCE_NULL_REFERENCE);
    CHECK(test->geometry.site.entered && test->geometry.site.bytecode == 1);
    CHECK(mc_entity_vector_distance(test->classes, test->vectors[0], test->vectors[1], &scalar,
                                    NULL) == MC_ENTITY_GEOMETRY_INVALID_ARGUMENT);
    CHECK(mc_entity_bounds_intercept_begin(test->classes, &box, NULL, test->vectors[1], &test->call,
                                           &test->entity_error) ==
          MC_ENTITY_HIT_SOURCE_NULL_REFERENCE);
    CHECK(mc_entity_bounds_intercept_inspect(test->call, &test->call_view) == MC_ENTITY_HIT_OK &&
          test->call_view.error->site.bytecode == 6 &&
          !test->call_view.error->geometry.site.entered && test->call_view.result == NULL);
    CHECK(mc_entity_bounds_intercept_release(&test->call, NULL) == MC_ENTITY_HIT_OK);
    CHECK(mc_entity_bounds_intercept_begin(test->classes, &box, test->vectors[0], NULL, &test->call,
                                           &test->entity_error) ==
          MC_ENTITY_HIT_SOURCE_NULL_REFERENCE);
    CHECK(mc_entity_bounds_intercept_inspect(test->call, &test->call_view) == MC_ENTITY_HIT_OK &&
          test->call_view.error->geometry.site.entered &&
          test->call_view.error->geometry.site.bytecode == 1);
    CHECK(mc_entity_bounds_intercept_release(&test->call, NULL) == MC_ENTITY_HIT_OK);
    CHECK(mc_entity_bounds_intercept_begin(test->classes, &box, test->vectors[0], test->vectors[1],
                                           &test->call, &test->entity_error) == MC_ENTITY_HIT_OK);
    CHECK(mc_entity_bounds_intercept_inspect(test->call, &test->call_view) == MC_ENTITY_HIT_OK &&
          test->call_view.result == NULL && test->call_view.candidate == NULL &&
          test->call_view.source_returned);
    for (size_t index = 0; index < 6; ++index)
        CHECK(test->call_view.locals[index] == NULL);
    CHECK(test->call_view.generated[0] != NULL && test->call_view.generated[1] != NULL &&
          test->call_view.generated[2] == NULL && test->call_view.generated[3] == NULL &&
          test->call_view.generated[4] != NULL && test->call_view.generated[5] != NULL);
    CHECK(mc_entity_vector_get_snapshot(test->classes, &test->vector_view) == MC_ENTITY_OK &&
          test->vector_view.cursor == 4);
    CHECK(test->classes->hit_kind_phase == MC_ENTITY_CLASS_COLD);
    CHECK(mc_entity_bounds_intercept_release(&test->call, NULL) == MC_ENTITY_HIT_OK);
    *test->vectors[0] = (mc_entity_vector){NAN, 0, 0};
    CHECK(mc_entity_bounds_contains_point(test->classes, &box, test->vectors[0], &inside,
                                          &test->geometry) == MC_ENTITY_GEOMETRY_OK &&
          inside);
    *test->vectors[0] = (mc_entity_vector){-1, 0, 0};
    CHECK(mc_entity_bounds_contains_point(test->classes, &box, test->vectors[0], &inside,
                                          &test->geometry) == MC_ENTITY_GEOMETRY_OK &&
          !inside);
    *test->vectors[0] = (mc_entity_vector){0, 0, 0};
    *test->vectors[1] = (mc_entity_vector){NAN, 1, 1};
    CHECK(mc_entity_vector_intersect_x(test->classes, test->vectors[0], test->vectors[1], 0,
                                       &test->vectors[2],
                                       &test->geometry) == MC_ENTITY_GEOMETRY_OK);
    CHECK(test->vectors[2] != NULL && isnan(test->vectors[2]->x) && isnan(test->vectors[2]->y) &&
          isnan(test->vectors[2]->z));
    _mm_setcsr(saved_csr | 0x40u);
    CHECK(mc_entity_vector_distance(test->classes, test->vectors[0], test->vectors[1], &scalar,
                                    &test->geometry) == MC_ENTITY_GEOMETRY_UNSUPPORTED_FP);
    CHECK(test->geometry.native_policy && scalar == 999);
    _mm_setcsr(saved_csr);
    bool pickable = true;
    CHECK(mc_entity_pickable(NULL, &pickable, &test->hit_error) ==
              MC_ENTITY_HIT_SOURCE_NULL_REFERENCE &&
          pickable);
    CHECK(!test->hit_error.site.entered && strcmp(test->hit_error.site.declaring_class, "sn") == 0);
    result = true;
cleanup:
    _mm_setcsr(saved_csr);
    if (!close_work(test))
        result = false;
    free(test);
    return result;
}

static bool vector_quota(void) {
    bool result = false;
    work *test = calloc(1, sizeof(*test));
    if (test == NULL)
        return false;
    CHECK(open_work(test, MC_ENTITY_CLIENT));
    CHECK(mc_entity_hit_result_allocate(test->classes, &test->hits[0], NULL) == MC_ENTITY_HIT_OK);
    CHECK(mc_entity_hit_result_construct_block(test->hits[0], 1, 2, 3, 4, NULL, &test->hit_error) ==
          MC_ENTITY_HIT_SOURCE_NULL_REFERENCE);
    CHECK(mc_entity_hit_result_release(&test->hits[0], NULL) == MC_ENTITY_HIT_OK);
    test->classes->limits.max_allocation_bytes =
        test->classes->allocated_bytes + sizeof(mc_entity_hit_result);
    CHECK(mc_entity_hit_result_allocate(test->classes, &test->hits[0], NULL) == MC_ENTITY_HIT_OK);
    const mc_entity_vector point = {7, 8, 9};
    CHECK(mc_entity_hit_result_construct_block(test->hits[0], 11, 12, 13, 14, &point,
                                               &test->hit_error) == MC_ENTITY_HIT_VECTOR_FAILED);
    CHECK(test->hit_error.site.bytecode == 48 &&
          test->hit_error.vector.pool.status == MC_ENTITY_LIMIT &&
          test->classes->hit_kind_phase == MC_ENTITY_CLASS_READY &&
          test->classes->vector_phase == MC_ENTITY_CLASS_FAILED);
    CHECK(mc_entity_hit_result_inspect(test->hits[0], &test->hit_view) == MC_ENTITY_HIT_OK &&
          test->hit_view.fields.a != NULL && test->hit_view.fields.b == 11 &&
          test->hit_view.fields.e == 14 && test->hit_view.fields.f == NULL);
    result = true;
cleanup:
    if (!close_work(test))
        result = false;
    free(test);
    return result;
}

static bool parse_values(const char *text, double *values, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        char *end = NULL;
        const uint64_t raw = strtoull(text, &end, 16);
        if (end == text || (index + 1 < count ? *end != ',' : *end != '\0'))
            return false;
        memcpy(&values[index], &raw, sizeof(raw));
        text = end + (index + 1 < count ? 1 : 0);
    }
    return true;
}

static bool matches(const char *text, const mc_entity_vector *vector) {
    if (strcmp(text, "-") == 0)
        return vector == NULL;
    double expected[3];
    return vector != NULL && parse_values(text, expected, 3) &&
           bits(vector->x) == bits(expected[0]) && bits(vector->y) == bits(expected[1]) &&
           bits(vector->z) == bits(expected[2]);
}

static bool tokenize(char *line, char **tokens, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        while (*line == ' ' || *line == '\r' || *line == '\n')
            ++line;
        if (*line == '\0')
            return false;
        tokens[index] = line;
        while (*line != '\0' && *line != ' ' && *line != '\r' && *line != '\n')
            ++line;
        if (*line != '\0')
            *line++ = '\0';
    }
    while (*line == ' ' || *line == '\r' || *line == '\n')
        ++line;
    return *line == '\0';
}

static bool parse_cursors(const char *text, int *values) {
    for (size_t index = 0; index < 4; ++index) {
        char *end = NULL;
        const long value = strtol(text, &end, 10);
        if (end == text || value < 0 || value > INT32_MAX ||
            (index < 3 ? *end != ',' : *end != '\0'))
            return false;
        values[index] = (int)value;
        text = end + (index < 3 ? 1 : 0);
    }
    return true;
}

static bool observations(const char *path, mc_entity_source source) {
    bool result = false;
    size_t row = 0;
    work *test = calloc(1, sizeof(*test));
    FILE *file = NULL;
    if (test == NULL)
        return false;
    CHECK(open_work(test, source));
#ifdef _WIN32
    if (fopen_s(&file, path, "rb") != 0)
        file = NULL;
#else
    file = fopen(path, "rb");
#endif
    CHECK(file != NULL);
    while (fgets(test->line, sizeof(test->line), file) != NULL) {
        ++row;
        char *tokens[14];
        CHECK(tokenize(test->line, tokens, 14));
        const unsigned int mode = (unsigned int)strtoul(tokens[0], NULL, 10);
        double start[3], end[3], bounds[6], planes[3], expected[3];
        int cursors[4];
        CHECK(mode < 3 && parse_values(tokens[1], start, 3) && parse_values(tokens[2], end, 3) &&
              parse_values(tokens[3], bounds, 6) && parse_values(tokens[4], planes, 3));
        CHECK(parse_cursors(tokens[13], cursors));
        const mc_entity_bounds box = {bounds[0], bounds[1], bounds[2],
                                      bounds[3], bounds[4], bounds[5]};
        CHECK(inputs(test, mode, start, end));
        for (size_t index = 0; index < 3; ++index)
            CHECK(parse_values(tokens[5 + index], &expected[index], 1));
        double actual = 0;
        CHECK(mc_entity_vector_distance(test->classes, test->vectors[0], test->vectors[1], &actual,
                                        &test->geometry) == MC_ENTITY_GEOMETRY_OK &&
              bits(actual) == bits(expected[0]));
        CHECK(mc_entity_vector_distance_squared(test->classes, test->vectors[0], test->vectors[1],
                                                &actual,
                                                &test->geometry) == MC_ENTITY_GEOMETRY_OK &&
              bits(actual) == bits(expected[1]));
        CHECK(mc_entity_vector_distance_squared_to(
                  test->classes, test->vectors[0], test->vectors[1]->x, test->vectors[1]->y,
                  test->vectors[1]->z, &actual, &test->geometry) == MC_ENTITY_GEOMETRY_OK &&
              bits(actual) == bits(expected[2]));
        bool inside = false;
        CHECK(mc_entity_bounds_contains_point(test->classes, &box, test->vectors[0], &inside,
                                              &test->geometry) == MC_ENTITY_GEOMETRY_OK &&
              inside == (strcmp(tokens[8], "1") == 0));
        CHECK(release_inputs(test));
        for (size_t axis = 0; axis < 3; ++axis) {
            CHECK(inputs(test, mode, start, end));
            mc_entity_geometry_status status;
            if (axis == 0)
                status =
                    mc_entity_vector_intersect_x(test->classes, test->vectors[0], test->vectors[1],
                                                 planes[axis], &test->vectors[2], &test->geometry);
            else if (axis == 1)
                status =
                    mc_entity_vector_intersect_y(test->classes, test->vectors[0], test->vectors[1],
                                                 planes[axis], &test->vectors[2], &test->geometry);
            else
                status =
                    mc_entity_vector_intersect_z(test->classes, test->vectors[0], test->vectors[1],
                                                 planes[axis], &test->vectors[2], &test->geometry);
            CHECK(status == MC_ENTITY_GEOMETRY_OK && matches(tokens[9 + axis], test->vectors[2]));
            CHECK(mc_entity_vector_get_snapshot(test->classes, &test->vector_view) ==
                      MC_ENTITY_OK &&
                  test->vector_view.cursor == cursors[axis]);
            CHECK(release_inputs(test));
        }
        CHECK(inputs(test, mode, start, end));
        CHECK(mc_entity_bounds_intercept_begin(test->classes, &box, test->vectors[0],
                                               test->vectors[1], &test->call,
                                               &test->entity_error) == MC_ENTITY_HIT_OK);
        CHECK(mc_entity_bounds_intercept_inspect(test->call, &test->call_view) ==
                  MC_ENTITY_HIT_OK &&
              test->call_view.source_returned);
        if (strcmp(tokens[12], "-") == 0)
            CHECK(test->call_view.result == NULL);
        else {
            char *point = strchr(tokens[12], ',');
            CHECK(point != NULL &&
                  mc_entity_hit_result_inspect(test->call_view.result, &test->hit_view) ==
                      MC_ENTITY_HIT_OK);
            CHECK(test->hit_view.fields.e == (int32_t)strtol(tokens[12], NULL, 10) &&
                  matches(point + 1, test->hit_view.fields.f));
        }
        CHECK(mc_entity_vector_get_snapshot(test->classes, &test->vector_view) == MC_ENTITY_OK &&
              test->vector_view.cursor == cursors[3]);
        CHECK(mc_entity_bounds_intercept_release(&test->call, &test->hit_error) ==
              MC_ENTITY_HIT_OK);
        CHECK(release_inputs(test));
    }
    CHECK(!ferror(file) && row == 942);
    result = true;
cleanup:
    if (!result)
        fprintf(stderr, "Original geometry row %zu side %u\n", row, (unsigned int)source);
    if (file != NULL)
        fclose(file);
    if (!close_work(test))
        result = false;
    free(test);
    return result;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Expected authored original fixture path\n");
        return 2;
    }
    if (!owners(MC_ENTITY_CLIENT) || !owners(MC_ENTITY_SERVER) || !classifications() ||
        !edge_controls() || !vector_quota())
        return 1;
    for (unsigned int stage = 0; stage < 4; ++stage)
        if (!quotas(stage))
            return 1;
    for (unsigned int stage = 0; stage < 5; ++stage)
        if (!intercept_controls(stage))
            return 1;
    if (!observations(argv[1], MC_ENTITY_CLIENT) || !observations(argv[1], MC_ENTITY_SERVER))
        return 1;
    printf("Hit/geometry: %zu checks, 942 original rows per side\n", checks);
    return 0;
}
