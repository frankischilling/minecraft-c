#include "mc/entity/entity.h"

#include "../../src/core/java/random_profile_internal.h"
#include "../../src/entity/entity_internal.h"
#include "../support/java_random.h"

#include <emmintrin.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Literal fields come from paired constructors in entity-fields-construction-spec.md,
 * SHA256 64da5adaf212eefaac5602346eeb902af6b59f13dc13b60dc7b1187251c81b7f.
 * Private production layouts are inspected only while quiescent. Budget/hold/ID
 * changes below are explicitly native invariant controls, never entropy hooks. */
static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

#define BASE_INTS(X)                                                                               \
    X(id);                                                                                         \
    X(private_b);                                                                                  \
    X(bt);                                                                                         \
    X(bu);                                                                                         \
    X(bv);                                                                                         \
    X(bw);                                                                                         \
    X(by);                                                                                         \
    X(bz);                                                                                         \
    X(bG);                                                                                         \
    X(bH);                                                                                         \
    X(bI);                                                                                         \
    X(client_only.bJ);                                                                             \
    X(client_only.bK);                                                                             \
    X(client_only.bL);
#define BASE_DOUBLES(X)                                                                            \
    X(aE);                                                                                         \
    X(aJ);                                                                                         \
    X(aK);                                                                                         \
    X(aL);                                                                                         \
    X(x);                                                                                          \
    X(y);                                                                                          \
    X(z);                                                                                          \
    X(velocity_x);                                                                                 \
    X(velocity_y);                                                                                 \
    X(velocity_z);                                                                                 \
    X(bl);                                                                                         \
    X(bm);                                                                                         \
    X(bn);                                                                                         \
    X(private_d);                                                                                  \
    X(private_e);
#define BASE_FLOATS(X)                                                                             \
    X(yaw);                                                                                        \
    X(aT);                                                                                         \
    X(aU);                                                                                         \
    X(aV);                                                                                         \
    X(offset);                                                                                     \
    X(width);                                                                                      \
    X(height);                                                                                     \
    X(bi);                                                                                         \
    X(bj);                                                                                         \
    X(bk);                                                                                         \
    X(y_size);                                                                                     \
    X(bp);                                                                                         \
    X(br);                                                                                         \
    X(bE);
#define BASE_BOOLS(X)                                                                              \
    X(aF);                                                                                         \
    X(aX);                                                                                         \
    X(aY);                                                                                         \
    X(aZ);                                                                                         \
    X(ba);                                                                                         \
    X(bb);                                                                                         \
    X(bc);                                                                                         \
    X(bd);                                                                                         \
    X(be);                                                                                         \
    X(bq);                                                                                         \
    X(bx);                                                                                         \
    X(private_c);                                                                                  \
    X(bC);                                                                                         \
    X(bF);                                                                                         \
    X(bM);
#define BASE_REFS(X)                                                                               \
    X(aG);                                                                                         \
    X(aH);                                                                                         \
    X(world);                                                                                      \
    X(client_only.bA);                                                                             \
    X(client_only.bB);

static uint64_t double_bits(double value) {
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary32 required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool base_equal(const mc_entity_base_fields *left, const mc_entity_base_fields *right) {
#define EQUAL_SCALAR(field)                                                                        \
    if (left->field != right->field)                                                               \
    return false
#define EQUAL_DOUBLE(field)                                                                        \
    if (double_bits(left->field) != double_bits(right->field))                                     \
    return false
#define EQUAL_FLOAT(field)                                                                         \
    if (float_bits(left->field) != float_bits(right->field))                                       \
    return false
    BASE_INTS(EQUAL_SCALAR)
    BASE_DOUBLES(EQUAL_DOUBLE)
    BASE_FLOATS(EQUAL_FLOAT)
    BASE_BOOLS(EQUAL_SCALAR)
    BASE_REFS(EQUAL_SCALAR)
#undef EQUAL_SCALAR
#undef EQUAL_DOUBLE
#undef EQUAL_FLOAT
    return true;
}

static bool bounds_equal(const mc_entity_bounds *left, const mc_entity_bounds *right) {
    return double_bits(left->min_x) == double_bits(right->min_x) &&
           double_bits(left->min_y) == double_bits(right->min_y) &&
           double_bits(left->min_z) == double_bits(right->min_z) &&
           double_bits(left->max_x) == double_bits(right->max_x) &&
           double_bits(left->max_y) == double_bits(right->max_y) &&
           double_bits(left->max_z) == double_bits(right->max_z);
}

static bool raw_is(const mc_entity_snapshot *value, mc_entity_source source) {
    const mc_entity_base_fields zero = {0};
    return value->source == source && value->phase == MC_ENTITY_RAW && value->base_bytecode == 0 &&
           value->item_bytecode == 0 && base_equal(&value->base, &zero) && value->item.b == 0 &&
           value->item.e == 0 && value->item.c == 0 && value->item.f == 0 &&
           float_bits(value->item.d) == 0 && value->bounds == NULL && value->random == NULL &&
           value->watcher == NULL && value->stack == NULL;
}

static mc_entity_base_fields base_literals(int32_t id) {
    /* Original complete base constructor scalar state; every omitted field is
     * an independently required raw zero/null, including all five client slots. */
    const mc_entity_base_fields value = {.id = id,
                                         .aE = 1.0,
                                         .bd = true,
                                         .width = 0.6f,
                                         .height = 1.8f,
                                         .private_b = 1,
                                         .bu = 1,
                                         .bw = 300,
                                         .bz = 300,
                                         .private_c = true};
    return value;
}

static bool fresh_random(const mc_java_random *value) {
    return value != NULL && value->state <= UINT64_C(0xffffffffffff) &&
           double_bits(value->next_next_gaussian) == 0 && !value->have_next_next_gaussian;
}

static size_t profile_budget(void) {
    return sizeof(mc_java_random_profile) + sizeof(mc_java_math_random_owner);
}

static uint64_t uniquifier(const mc_java_random_profile *profile) {
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static uint64_t advanced(uint64_t before, unsigned int count) {
    for (unsigned int i = 0; i < count; ++i)
        before *= UINT64_C(1181783497276652981);
    return before;
}

static bool holder_is(const mc_java_random_profile *profile, mc_java_random_holder_phase phase) {
    return atomic_load_explicit(&profile->holder_phase, memory_order_acquire) ==
           (unsigned int)phase;
}

static mc_java_random math_snapshot(const mc_java_random_profile *profile) {
    const mc_java_random value = {
        .state = atomic_load_explicit(&profile->math->state, memory_order_relaxed),
        .next_next_gaussian = profile->math->next_next_gaussian,
        .have_next_next_gaussian = profile->math->have_next_next_gaussian};
    return value;
}

static bool math_equal(const mc_java_random_profile *profile, const mc_java_random *expected) {
    const mc_java_random actual = math_snapshot(profile);
    return mc_test_java_random_equal(&actual, expected);
}

static bool watcher_is(const mc_entity_snapshot *value, bool installed) {
    mc_entity_watcher_snapshot watch = {0};
    if (value->watcher == NULL ||
        mc_entity_watcher_get_snapshot(value->watcher, &watch) != MC_ENTITY_OK)
        return false;
    if (watch.dirty || watch.size != (installed ? 1u : 0u))
        return false;
    return installed ? watch.record != NULL && watch.index == 0 && watch.type == 0 &&
                           watch.byte_value == 0 && watch.record_dirty
                     : watch.record == NULL;
}

static bool ready_is(const mc_entity_snapshot *value, mc_entity_source source, int32_t id,
                     bool full, const double *draws) {
    mc_entity_base_fields expected = base_literals(id);
    mc_entity_bounds box = {-(double)0.3f, 0.0,          -(double)0.3f,
                            (double)0.3f,  (double)1.8f, (double)0.3f};
    expected.width = 0.25f;
    expected.height = 0.25f;
    expected.offset = 0.125f;
    if (full) {
        expected.x = 8.0;
        expected.y = 16.0;
        expected.z = 32.0;
        expected.velocity_y = 0.20000000298023224;
        expected.yaw = draws == NULL ? value->base.yaw : mc_entity_item_yaw_from_draw(draws[1]);
        expected.velocity_x =
            draws == NULL ? value->base.velocity_x : mc_entity_item_velocity_from_draw(draws[2]);
        expected.velocity_z =
            draws == NULL ? value->base.velocity_z : mc_entity_item_velocity_from_draw(draws[3]);
        box = (mc_entity_bounds){7.875, 15.875, 31.875, 8.125, 16.125, 32.125};
        if (!(value->base.yaw >= 0.0f && value->base.yaw <= 360.0f &&
              value->base.velocity_x >= -0.10000000149011612 &&
              value->base.velocity_x <= 0.10000000149011612 &&
              value->base.velocity_z >= -0.10000000149011612 &&
              value->base.velocity_z <= 0.10000000149011612))
            return false;
    }
    const float phase = draws == NULL ? value->item.d : mc_entity_item_phase_from_draw(draws[0]);
    return value->source == source && value->phase == MC_ENTITY_READY &&
           value->base_bytecode == 231 && value->item_bytecode == (full ? 117 : 49) &&
           base_equal(&value->base, &expected) && value->item.b == 0 && value->item.e == 0 &&
           value->item.c == 0 && value->item.f == 5 &&
           float_bits(value->item.d) == float_bits(phase) && value->item.d >= 0.0f &&
           value->item.d <= 6.283186f && value->bounds != NULL &&
           bounds_equal(value->bounds, &box) && fresh_random(value->random) &&
           watcher_is(value, true);
}

static int retire_classes(mc_entity_classes **classes) {
    const mc_entity_status status = mc_entity_classes_free(*classes);
    if (status != MC_ENTITY_OK) {
        fprintf(stderr, "class cleanup: %s\n", mc_entity_status_string(status));
        return 1;
    }
    *classes = NULL;
    return 0;
}

static int test_raw(mc_entity_source source) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL, *candidate_classes = NULL, *limited = NULL;
    mc_entity *raw = NULL, *held = NULL, *candidate = NULL;
    mc_entity_snapshot view = {0}, sentinel = {.phase = MC_ENTITY_READY, .base.id = 77};
    mc_entity_class_snapshot state = {0};
    mc_entity_error error = {0};
    mc_entity_limits limits = {SIZE_MAX, 1};
    uint64_t before = 0;
    size_t saved_holds = 0;
    mc_entity_status status;

    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    before = uniquifier(profile);
    CHECK(mc_entity_classes_create(source, profile, &limits, &classes) == MC_ENTITY_OK);
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK);
    CHECK(state.entity_class == MC_ENTITY_CLASS_COLD &&
          state.bounds_class == MC_ENTITY_CLASS_COLD &&
          state.watcher_class == MC_ENTITY_CLASS_COLD && state.next_id == 0 &&
          state.live_entities == 0 && state.allocation_bytes == sizeof(*classes));
    candidate_classes = classes;
    limits.max_allocation_bytes = sizeof(*classes) - 1u;
    CHECK(mc_entity_classes_create(source, profile, &limits, &candidate_classes) ==
          MC_ENTITY_LIMIT);
    CHECK(candidate_classes == classes && uniquifier(profile) == before);
    CHECK(mc_item_entity_allocate(classes, &raw, &error) == MC_ENTITY_OK);
    CHECK(mc_entity_get_snapshot(raw, &view) == MC_ENTITY_OK && raw_is(&view, source));
    CHECK(mc_entity_native_bytes(raw) == sizeof(*raw));
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_BUSY);
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK);
    CHECK(state.entity_class == MC_ENTITY_CLASS_READY && state.next_id == 0 &&
          state.live_entities == 1 && state.bounds_pool_identity == NULL &&
          state.watcher_types_identity == NULL &&
          state.allocation_bytes == sizeof(*classes) + sizeof(*raw) &&
          uniquifier(profile) == before);
    candidate = raw;
    CHECK(mc_item_entity_allocate(classes, &candidate, &error) == MC_ENTITY_LIMIT);
    CHECK(candidate == raw && raw_is(&view, source));
    CHECK(mc_entity_set_velocity(raw, 0, 1.0) == MC_ENTITY_INVALID_PHASE);
    CHECK(mc_entity_set_size(raw, 1.0f, 2.0f) == MC_ENTITY_INVALID_PHASE);
    CHECK(mc_entity_set_position(raw, 1.0, 2.0, 3.0) == MC_ENTITY_INVALID_PHASE);
    CHECK(mc_entity_get_snapshot(NULL, &sentinel) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(sentinel.phase == MC_ENTITY_READY && sentinel.base.id == 77);
    CHECK(mc_entity_retain(raw) == MC_ENTITY_OK);
    held = raw;
    saved_holds = raw->holds;
    raw->holds = SIZE_MAX; /* Native hold-capacity control, restored before CHECK/cleanup. */
    status = mc_entity_retain(raw);
    const bool hold_unchanged = raw->holds == SIZE_MAX;
    raw->holds = saved_holds;
    CHECK(status == MC_ENTITY_LIMIT && hold_unchanged);
    candidate = NULL;
    mc_entity_release(raw);
    raw = NULL;
    CHECK(mc_entity_get_snapshot(held, &view) == MC_ENTITY_OK && raw_is(&view, source));
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_BUSY);
    mc_entity_release(held);
    held = NULL;
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK &&
          state.live_entities == 0 && state.allocation_bytes == sizeof(*classes) &&
          state.next_id == 0);
    CHECK(mc_item_entity_allocate(classes, &raw, &error) == MC_ENTITY_OK);
    CHECK(mc_entity_get_snapshot(raw, &view) == MC_ENTITY_OK && raw_is(&view, source) &&
          uniquifier(profile) == before);
    mc_entity_release(raw);
    raw = NULL;

    limits = (mc_entity_limits){SIZE_MAX, 0};
    CHECK(mc_entity_classes_create(source, profile, &limits, &limited) == MC_ENTITY_OK);
    CHECK(mc_item_entity_allocate(limited, &raw, &error) == MC_ENTITY_LIMIT && raw == NULL);
    CHECK(mc_entity_classes_get_snapshot(limited, &state) == MC_ENTITY_OK &&
          state.entity_class == MC_ENTITY_CLASS_READY && state.next_id == 0 &&
          state.live_entities == 0);
    CHECK(retire_classes(&limited) == 0);
    limits = (mc_entity_limits){sizeof(*classes) + sizeof(mc_entity) - 1u, 1};
    CHECK(mc_entity_classes_create(source, profile, &limits, &limited) == MC_ENTITY_OK);
    CHECK(mc_item_entity_allocate(limited, &raw, &error) == MC_ENTITY_LIMIT && raw == NULL);
    CHECK(mc_entity_classes_get_snapshot(limited, &state) == MC_ENTITY_OK &&
          state.entity_class == MC_ENTITY_CLASS_READY && state.live_entities == 0 &&
          state.allocation_bytes == sizeof(*classes) && uniquifier(profile) == before);
    result = 0;
cleanup:
    if (candidate != raw && candidate != held)
        mc_entity_release(candidate);
    mc_entity_release(raw);
    mc_entity_release(held);
    if (candidate_classes == classes)
        candidate_classes = NULL; /* Borrowed sentinel, not an extra hold. */
    if (retire_classes(&candidate_classes) != 0)
        result = 1;
    if (retire_classes(&limited) != 0)
        result = 1;
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL && limited == NULL && candidate_classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int test_constructors(mc_entity_source source, bool cold_full) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entities[3] = {NULL, NULL, NULL};
    mc_entity_snapshot views[3] = {{0}, {0}, {0}};
    mc_entity_class_snapshot state = {0};
    mc_entity_error error = {0};
    mc_java_random expected = {0};
    mc_java_math_random_owner *math = NULL;
    const mc_entity_bounds *first_box = NULL;
    const mc_entity_watcher *first_watcher = NULL;
    const mc_entity_watch_record *first_record = NULL;
    mc_entity_watcher_snapshot watch = {0};
    double draws[4] = {0};
    uint64_t before = 0;
    size_t after_first_bytes = 0;

    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK);
    before = uniquifier(profile);
    for (size_t i = 0; i < 3; ++i) {
        const bool full = i == 1 ? !cold_full : cold_full;
        if (i != 0) {
            expected = math_snapshot(profile);
            for (size_t d = 0; d < (full ? 4u : 1u); ++d)
                draws[d] = mc_java_random_next_double(&expected);
        }
        CHECK(mc_item_entity_allocate(classes, &entities[i], &error) == MC_ENTITY_OK);
        CHECK((full ? mc_item_entity_construct(entities[i], NULL, 8.0, 16.0, 32.0, NULL, &error)
                    : mc_item_entity_construct_world(entities[i], NULL, &error)) == MC_ENTITY_OK);
        CHECK(mc_entity_get_snapshot(entities[i], &views[i]) == MC_ENTITY_OK);
        CHECK(ready_is(&views[i], source, (int32_t)i, full, i == 0 ? NULL : draws));
        CHECK(views[i].stack == NULL &&
              (const void *)views[i].random != (const void *)profile->math);
        CHECK(uniquifier(profile) == advanced(before, (unsigned int)i + 2u));
        CHECK(holder_is(profile, MC_JAVA_RANDOM_HOLDER_READY) && profile->math != NULL);
        if (i == 0) {
            math = profile->math;
            expected = math_snapshot(profile);
            CHECK(fresh_random(&expected));
            first_box = views[i].bounds;
            first_watcher = views[i].watcher;
            CHECK(mc_entity_watcher_get_snapshot(first_watcher, &watch) == MC_ENTITY_OK);
            first_record = watch.record;
            CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK);
            after_first_bytes = state.allocation_bytes;
        } else {
            CHECK(profile->math == math && math_equal(profile, &expected));
            CHECK(views[i].random != views[0].random && views[i].bounds != first_box &&
                  views[i].watcher != first_watcher);
            CHECK(mc_entity_watcher_get_snapshot(first_watcher, &watch) == MC_ENTITY_OK &&
                  watch.record == first_record);
            CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK &&
                  state.allocation_bytes ==
                      after_first_bytes + i * mc_entity_native_bytes(entities[i]));
        }
    }
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK && state.next_id == 3 &&
          state.live_entities == 3 && state.bounds_pool_cursor == 0 &&
          state.entity_class == MC_ENTITY_CLASS_READY &&
          state.bounds_class == MC_ENTITY_CLASS_READY &&
          state.watcher_class == MC_ENTITY_CLASS_READY && state.watcher_type_count == 7);
    CHECK(classes->profile == profile && classes->types->size == 7);
    const mc_entity_type_entry *entry = classes->types->head;
    for (int32_t i = 0; i < 7; ++i) {
        CHECK(entry != NULL && entry->kind == i && entry->value == i);
        entry = entry->next;
    }
    CHECK(entry == NULL && classes->bounds_pool->length == 0 &&
          classes->bounds_pool->capacity == 0 && classes->bounds_pool->elements == NULL);
    /* Exercise the actual retained private owner; a local genuine-state replay
     * checks stream ownership, not an independent primitive algorithm oracle. */
    mc_java_random private_expected = *entities[0]->random;
    const mc_java_random other_before = *entities[1]->random;
    const mc_java_random math_before = math_snapshot(profile);
    const int32_t wanted = mc_java_random_next_i32(&private_expected);
    const int32_t actual = mc_java_random_next_i32(entities[0]->random);
    CHECK(actual == wanted && mc_test_java_random_equal(entities[0]->random, &private_expected));
    CHECK(mc_test_java_random_equal(entities[1]->random, &other_before) &&
          math_equal(profile, &math_before));
    CHECK(uniquifier(profile) == advanced(before, 4));
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_BUSY);
    result = 0;
cleanup:
    for (size_t i = 0; i < 3; ++i)
        mc_entity_release(entities[i]);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int test_stack_mutators(mc_entity_source source) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entity = NULL, *held = NULL;
    mc_item_stack *stack = NULL, *stack_alias = NULL;
    mc_entity_snapshot view = {0}, previous = {0};
    mc_entity_watcher_snapshot watch = {0};
    mc_item_stack_fields item = {0};
    mc_item_stack_phase item_phase = MC_ITEM_STACK_FAILED_CONSTRUCTION;
    mc_entity_error error = {0};
    mc_java_random math = {0};
    const mc_entity_bounds *box = NULL;
    const mc_entity_watcher *watcher = NULL;
    const mc_entity_watch_record *record = NULL;
    uint64_t before = 0;

    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK);
    CHECK(mc_item_entity_allocate(classes, &entity, &error) == MC_ENTITY_OK);
    CHECK(mc_item_entity_construct(entity, NULL, 8.0, 16.0, 32.0, stack, &error) == MC_ENTITY_OK);
    CHECK(mc_entity_get_snapshot(entity, &view) == MC_ENTITY_OK && view.stack == stack);
    CHECK(mc_item_stack_phase_get(stack, &item_phase) == MC_ITEM_STACK_OK &&
          item_phase == MC_ITEM_STACK_RAW);
    CHECK(mc_item_entity_stack_get(entity, &stack_alias) == MC_ENTITY_OK && stack_alias == stack);
    mc_item_stack_release(stack);
    stack = NULL;
    CHECK(mc_item_stack_construct(stack_alias, 17, 4, 5) == MC_ITEM_STACK_OK);
    CHECK(mc_item_stack_fields_get(view.stack, &item) == MC_ITEM_STACK_OK && item.id == 17 &&
          item.count == 4 && item.damage == 5 && item.animation == 0);
    box = view.bounds;
    watcher = view.watcher;
    CHECK(mc_entity_watcher_get_snapshot(watcher, &watch) == MC_ENTITY_OK);
    record = watch.record;
    before = uniquifier(profile);
    math = math_snapshot(profile);
    CHECK(mc_entity_retain(entity) == MC_ENTITY_OK);
    held = entity;
    mc_entity_release(entity);
    entity = NULL;
    CHECK(mc_entity_set_velocity(held, 0, -0.0) == MC_ENTITY_OK);
    CHECK(mc_entity_get_snapshot(held, &previous) == MC_ENTITY_OK &&
          double_bits(previous.base.velocity_x) == UINT64_C(0x8000000000000000) &&
          double_bits(previous.base.velocity_y) == double_bits(view.base.velocity_y) &&
          double_bits(previous.base.velocity_z) == double_bits(view.base.velocity_z));
    CHECK(mc_entity_set_velocity(held, 1, 2.5) == MC_ENTITY_OK);
    CHECK(mc_entity_set_velocity(held, 2, -7.5) == MC_ENTITY_OK);
    CHECK(mc_entity_get_snapshot(held, &previous) == MC_ENTITY_OK);
    CHECK(mc_entity_set_velocity(held, 3, 99.0) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(mc_entity_get_snapshot(held, &view) == MC_ENTITY_OK &&
          base_equal(&view.base, &previous.base));
    CHECK(double_bits(view.base.velocity_x) == UINT64_C(0x8000000000000000) &&
          view.base.velocity_y == 2.5 && view.base.velocity_z == -7.5);

    /* Cancellation witness: (Y-.125)+.25 differs from Y+.125 at this Y. */
    CHECK(mc_entity_set_position(held, 8.0, 0x1.0000000000001p50, 32.0) == MC_ENTITY_OK);
    CHECK(mc_entity_get_snapshot(held, &view) == MC_ENTITY_OK && view.bounds == box &&
          view.bounds->min_y == 0x1p50 && view.bounds->max_y == 0x1.0000000000001p50);
    CHECK(view.base.aJ == 0.0 && view.base.aK == 0.0 && view.base.aL == 0.0 &&
          float_bits(view.base.aT) == 0 && float_bits(view.base.aU) == 0 &&
          float_bits(view.base.aV) == 0);
    const mc_entity_bounds old_box = *box;
    CHECK(mc_entity_set_size(held, -0.5f, 2.0f) == MC_ENTITY_OK);
    CHECK(bounds_equal(box, &old_box));
    CHECK(mc_entity_set_position(held, 8.0, 16.0, 32.0) == MC_ENTITY_OK);
    const mc_entity_bounds inverted = {8.25, 15.875, 32.25, 7.75, 17.875, 31.75};
    CHECK(bounds_equal(box, &inverted));
    /* Smallest float divided by2 rounds to +0 before widening to double. */
    CHECK(mc_entity_set_size(held, 0x1p-149f, 0.25f) == MC_ENTITY_OK);
    CHECK(mc_entity_set_position(held, 0.0, 0.0, 0.0) == MC_ENTITY_OK);
    const mc_entity_bounds tiny = {0.0, -0.125, 0.0, 0.0, 0.125, 0.0};
    CHECK(bounds_equal(box, &tiny));
    CHECK(mc_entity_get_snapshot(held, &view) == MC_ENTITY_OK && view.bounds == box &&
          view.watcher == watcher && view.stack == stack_alias &&
          float_bits(view.base.width) == UINT32_C(1));
    CHECK(mc_entity_watcher_get_snapshot(watcher, &watch) == MC_ENTITY_OK &&
          watch.record == record && !watch.dirty);
    CHECK(uniquifier(profile) == before && math_equal(profile, &math));
    CHECK(mc_item_entity_construct_world(held, NULL, &error) == MC_ENTITY_INVALID_PHASE);
    CHECK(uniquifier(profile) == before && math_equal(profile, &math));
    mc_entity_release(held);
    held = NULL;
    CHECK(mc_item_stack_fields_get(stack_alias, &item) == MC_ITEM_STACK_OK && item.count == 4 &&
          item.id == 17);
    result = 0;
cleanup:
    mc_entity_release(entity);
    mc_entity_release(held);
    mc_item_stack_release(stack);
    mc_item_stack_release(stack_alias);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int test_ids(mc_entity_source source) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL, *same_source_class = NULL, *other_source = NULL;
    mc_entity *entities[5] = {NULL, NULL, NULL, NULL, NULL};
    mc_entity_snapshot view = {0};
    mc_entity_class_snapshot class_view = {0};
    mc_entity_error error = {0};
    double discarded = 0.0;
    uint64_t before = 0;

    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_java_math_random(profile, &discarded) == MC_JAVA_RANDOM_PROFILE_OK);
    before = uniquifier(profile);
    CHECK(mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_entity_classes_create(source, profile, NULL, &same_source_class) == MC_ENTITY_OK);
    CHECK(mc_entity_classes_create(source == MC_ENTITY_CLIENT ? MC_ENTITY_SERVER : MC_ENTITY_CLIENT,
                                   profile, NULL, &other_source) == MC_ENTITY_OK);
    for (size_t i = 0; i < 5; ++i) {
        mc_entity_classes *owner = i < 3 ? classes : i == 3 ? same_source_class : other_source;
        CHECK(mc_item_entity_allocate(owner, &entities[i], &error) == MC_ENTITY_OK);
        if (i != 1)
            owner->next_id =
                INT32_MAX; /* Actual private class counter, native wrap/collision control. */
        CHECK(mc_item_entity_construct_world(entities[i], NULL, &error) == MC_ENTITY_OK);
        CHECK(mc_entity_get_snapshot(entities[i], &view) == MC_ENTITY_OK &&
              view.base.id == (i == 1 ? INT32_MIN : INT32_MAX));
        CHECK(mc_entity_hash_code(entities[i]) == view.base.id);
        CHECK(mc_entity_classes_get_snapshot(owner, &class_view) == MC_ENTITY_OK &&
              class_view.next_id == (i == 1 ? INT32_MIN + 1 : INT32_MIN));
    }
    CHECK(entities[0] != entities[2] && mc_entity_equals(entities[0], entities[2]));
    CHECK(mc_entity_equals(entities[0], entities[0]) &&
          !mc_entity_equals(entities[0], entities[1]));
    CHECK(!mc_entity_equals(entities[0], entities[3]) &&
          !mc_entity_equals(entities[0], entities[4]));
    CHECK(!mc_entity_equals(entities[0], NULL));
    CHECK(uniquifier(profile) == advanced(before, 5));
    result = 0;
cleanup:
    for (size_t i = 0; i < 5; ++i)
        mc_entity_release(entities[i]);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (retire_classes(&same_source_class) != 0)
        result = 1;
    if (retire_classes(&other_source) != 0)
        result = 1;
    if (classes == NULL && same_source_class == NULL && other_source == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

typedef struct budget_case {
    size_t allowed;
    const char *operation;
    uint16_t bytecode, base_pc;
    int type_count; /* -1: no type-map allocation; otherwise retained entry prefix. */
    bool random, watcher, bounds_latched, watcher_latched;
} budget_case;

static int test_budget_case(mc_entity_source source, const budget_case *test) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entity = NULL, *later = NULL;
    mc_entity_snapshot view = {0};
    mc_entity_class_snapshot state = {0};
    mc_entity_error error = {0};
    mc_entity_base_fields expected = {0};
    const mc_entity_bounds zero_box = {0};
    const mc_entity_bounds base_box = {-(double)0.3f, 0.0,          -(double)0.3f,
                                       (double)0.3f,  (double)1.8f, (double)0.3f};
    uint64_t before = 0;
    size_t static_bytes = 0;
    const void *pool = NULL, *types = NULL;

    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_item_entity_allocate(classes, &entity, &error) == MC_ENTITY_OK);
    before = uniquifier(profile);
    /* Native allocation admission control; never below the actual live ledger. */
    classes->limits.max_allocation_bytes = classes->allocated_bytes + test->allowed;
    CHECK(mc_item_entity_construct(entity, NULL, 8.0, 16.0, 32.0, NULL, &error) == MC_ENTITY_LIMIT);
    CHECK(error.status == MC_ENTITY_LIMIT && error.bytecode == test->bytecode &&
          error.operation != NULL && strcmp(error.operation, test->operation) == 0);
    CHECK(mc_entity_get_snapshot(entity, &view) == MC_ENTITY_OK &&
          view.phase == MC_ENTITY_FAILED_CONSTRUCTION && view.base_bytecode == test->base_pc &&
          view.item_bytecode == 0 && view.stack == NULL);
    if (test->base_pc == 24)
        expected = (mc_entity_base_fields){.id = 0, .aE = 1.0};
    else if (test->base_pc == 119)
        expected = (mc_entity_base_fields){
            .id = 0, .aE = 1.0, .bd = true, .width = 0.6f, .height = 1.8f, .private_b = 1};
    else
        expected = base_literals(0);
    CHECK(base_equal(&view.base, &expected));
    CHECK((view.random != NULL) == test->random && (view.watcher != NULL) == test->watcher);
    CHECK(view.random == NULL || fresh_random(view.random));
    CHECK(uniquifier(profile) == advanced(before, test->random ? 1u : 0u) &&
          holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD));
    if (test->base_pc == 24)
        CHECK(view.bounds == NULL);
    else
        CHECK(view.bounds != NULL &&
              bounds_equal(view.bounds, test->watcher ? &base_box : &zero_box));
    if (test->watcher)
        CHECK(watcher_is(&view, false));
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK && state.next_id == 1 &&
          state.live_entities == 1);
    CHECK(state.entity_class == MC_ENTITY_CLASS_READY && state.bounds_pool_cursor == 0);
    CHECK(state.bounds_class ==
          (test->bounds_latched ? MC_ENTITY_CLASS_FAILED : MC_ENTITY_CLASS_READY));
    CHECK(state.watcher_class == (test->watcher_latched   ? MC_ENTITY_CLASS_FAILED
                                  : test->type_count >= 0 ? MC_ENTITY_CLASS_READY
                                                          : MC_ENTITY_CLASS_COLD));
    CHECK(state.watcher_type_count == (test->type_count < 0 ? 0u : (size_t)test->type_count));
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_BUSY);
    CHECK(mc_item_entity_construct_world(entity, NULL, &error) == MC_ENTITY_INVALID_PHASE);
    CHECK(uniquifier(profile) == advanced(before, test->random ? 1u : 0u));
    pool = state.bounds_pool_identity;
    types = state.watcher_types_identity;
    static_bytes = sizeof(*classes) + (pool == NULL ? 0u : sizeof(mc_entity_bounds_pool)) +
                   (types == NULL ? 0u
                                  : sizeof(mc_entity_type_map) +
                                        state.watcher_type_count * sizeof(mc_entity_type_entry));
    const size_t direct_bytes =
        sizeof(*entity) + (view.bounds == NULL ? 0u : sizeof(mc_entity_bounds)) +
        (view.random == NULL ? 0u : sizeof(mc_java_random)) +
        (view.watcher == NULL ? 0u : sizeof(mc_entity_watcher) + sizeof(mc_entity_watch_map));
    CHECK(mc_entity_native_bytes(entity) == direct_bytes &&
          state.allocation_bytes == static_bytes + direct_bytes);
    mc_entity_release(entity);
    entity = NULL;
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK &&
          state.live_entities == 0 && state.next_id == 1 &&
          state.allocation_bytes == static_bytes && state.bounds_pool_identity == pool &&
          state.watcher_types_identity == types);
    classes->limits.max_allocation_bytes = SIZE_MAX;
    CHECK(mc_item_entity_allocate(classes, &later, &error) == MC_ENTITY_OK);
    if (test->bounds_latched || test->watcher_latched) {
        CHECK(mc_item_entity_construct_world(later, NULL, &error) == MC_ENTITY_CLASS_FAILED_STATUS);
        CHECK(error.bytecode == 0 &&
              strcmp(error.operation, test->bounds_latched ? "bounds class initialization"
                                                           : "watcher class initialization") == 0);
        CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK &&
              state.next_id == 2 &&
              state.watcher_type_count == (test->type_count < 0 ? 0u : (size_t)test->type_count) &&
              state.bounds_pool_identity == pool && state.watcher_types_identity == types);
        CHECK(uniquifier(profile) == advanced(before, test->watcher_latched ? 2u : 0u));
        CHECK(holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD));
    } else {
        CHECK(mc_item_entity_construct_world(later, NULL, &error) == MC_ENTITY_OK);
        CHECK(mc_entity_get_snapshot(later, &view) == MC_ENTITY_OK &&
              ready_is(&view, source, 1, false, NULL));
        CHECK(uniquifier(profile) == advanced(before, (test->random ? 1u : 0u) + 2u));
    }
    mc_entity_release(later);
    later = NULL;
    const size_t retained = test->bounds_latched || test->watcher_latched
                                ? static_bytes
                                : sizeof(*classes) + sizeof(mc_entity_bounds_pool) +
                                      sizeof(mc_entity_type_map) +
                                      7u * sizeof(mc_entity_type_entry);
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK &&
          state.live_entities == 0 && state.next_id == 2 && state.allocation_bytes == retained);
    result = 0;
cleanup:
    mc_entity_release(entity);
    mc_entity_release(later);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int test_budgets(mc_entity_source source) {
    const size_t pool = sizeof(mc_entity_bounds_pool), box = sizeof(mc_entity_bounds);
    const size_t random = sizeof(mc_java_random), types = sizeof(mc_entity_type_map);
    const size_t entry = sizeof(mc_entity_type_entry), watcher = sizeof(mc_entity_watcher);
    const size_t map = sizeof(mc_entity_watch_map), record = sizeof(mc_entity_watch_record);
    const size_t prefix = pool + box + random;
    const size_t typed = prefix + types + 7u * entry;
    const uint16_t puts[7] = {19, 32, 45, 58, 71, 84, 98};
    budget_case cases[15] = {
        {0, "bounds class initialization", 0, 24, -1, false, false, true, false},
        {pool, "bounds factory", 0, 24, -1, false, false, false, false},
        {pool + box, "entity constructor", 123, 119, -1, false, false, false, false},
        {prefix, "watcher class initialization", 0, 179, -1, true, false, false, true}};
    for (size_t i = 0; i < 7; ++i)
        cases[4 + i] = (budget_case){prefix + types + i * entry,
                                     "watcher class initialization",
                                     puts[i],
                                     179,
                                     (int)i,
                                     true,
                                     false,
                                     false,
                                     true};
    cases[11] = (budget_case){typed, "entity constructor", 183, 179, 7, true, false, false, false};
    cases[12] =
        (budget_case){typed + watcher, "watcher constructor", 5, 179, 7, true, false, false, false};
    cases[13] = (budget_case){
        typed + watcher + map, "watcher registration", 144, 221, 7, true, true, false, false};
    cases[14] = (budget_case){typed + watcher + map + record,
                              "watcher registration",
                              169,
                              221,
                              7,
                              true,
                              true,
                              false,
                              false};
    for (size_t i = 0; i < 15; ++i)
        if (test_budget_case(source, &cases[i]) != 0)
            return 1;
    return 0;
}

static mc_entity_status construct_with_control(mc_entity *entity, unsigned int control,
                                               mc_entity_error *error) {
    const unsigned int saved = _mm_getcsr();
    _mm_setcsr(control);
    const mc_entity_status status = mc_item_entity_construct_world(entity, NULL, error);
    _mm_setcsr(saved); /* Restore before any assertion, formatting or destruction. */
    return status;
}

static int test_fp_prefix(mc_entity_source source) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entity = NULL;
    mc_entity_snapshot view = {0};
    mc_entity_class_snapshot state = {0};
    mc_entity_error error = {0};
    const mc_entity_bounds box = {0};
    const mc_entity_base_fields expected = {
        .id = 0, .aE = 1.0, .bd = true, .width = 0.6f, .height = 1.8f, .private_b = 1};
    uint64_t before = 0;

    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_item_entity_allocate(classes, &entity, &error) == MC_ENTITY_OK);
    before = uniquifier(profile);
    CHECK(construct_with_control(entity, _mm_getcsr() | 0x8000u, &error) ==
          MC_ENTITY_PROFILE_FAILED);
    CHECK(error.bytecode == 127 && error.profile == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP);
    CHECK(mc_entity_get_snapshot(entity, &view) == MC_ENTITY_OK &&
          view.phase == MC_ENTITY_FAILED_CONSTRUCTION && view.base_bytecode == 119 &&
          view.item_bytecode == 0 && base_equal(&view.base, &expected));
    CHECK(view.bounds != NULL && bounds_equal(view.bounds, &box) && view.random == NULL &&
          view.watcher == NULL && view.stack == NULL);
    CHECK(uniquifier(profile) == before && holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) &&
          profile->math == NULL);
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK && state.next_id == 1 &&
          state.bounds_class == MC_ENTITY_CLASS_READY &&
          state.watcher_class == MC_ENTITY_CLASS_COLD);
    CHECK(state.allocation_bytes == sizeof(*classes) + sizeof(*entity) +
                                        sizeof(mc_entity_bounds_pool) + sizeof(mc_entity_bounds));
    CHECK(mc_item_entity_construct_world(entity, NULL, &error) == MC_ENTITY_INVALID_PHASE &&
          uniquifier(profile) == before);
    result = 0;
cleanup:
    mc_entity_release(entity);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int test_math_limit(mc_entity_source source) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entities[2] = {NULL, NULL};
    mc_entity_snapshot view = {0};
    mc_entity_class_snapshot state = {0};
    mc_entity_error error = {0};
    const mc_entity_bounds box = {-(double)0.3f, 0.0,          -(double)0.3f,
                                  (double)0.3f,  (double)1.8f, (double)0.3f};
    uint64_t before = 0;

    CHECK(mc_java_random_profile_create(sizeof(*profile), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK);
    before = uniquifier(profile);
    for (size_t i = 0; i < 2; ++i) {
        CHECK(mc_item_entity_allocate(classes, &entities[i], &error) == MC_ENTITY_OK);
        CHECK(mc_item_entity_construct(entities[i], NULL, 8.0, 16.0, 32.0, NULL, &error) ==
              MC_ENTITY_PROFILE_FAILED);
        CHECK(error.bytecode == 16 &&
              error.profile ==
                  (i == 0 ? MC_JAVA_RANDOM_PROFILE_LIMIT : MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED));
        CHECK(mc_entity_get_snapshot(entities[i], &view) == MC_ENTITY_OK &&
              view.phase == MC_ENTITY_FAILED_CONSTRUCTION && view.base_bytecode == 231 &&
              view.item_bytecode == 12 && view.item.b == 0 && view.item.f == 5 &&
              view.item.e == 0 && view.item.c == 0 && float_bits(view.item.d) == 0);
        const mc_entity_base_fields expected = base_literals((int32_t)i);
        CHECK(base_equal(&view.base, &expected) && view.bounds != NULL &&
              bounds_equal(view.bounds, &box) && fresh_random(view.random) &&
              watcher_is(&view, true));
        CHECK(view.stack == NULL && uniquifier(profile) == advanced(before, (unsigned int)i + 1u));
        CHECK(holder_is(profile, MC_JAVA_RANDOM_HOLDER_FAILED) && profile->math == NULL &&
              mc_java_random_profile_allocation_bytes(profile) == sizeof(*profile));
    }
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK && state.next_id == 2 &&
          state.entity_class == MC_ENTITY_CLASS_READY &&
          state.bounds_class == MC_ENTITY_CLASS_READY &&
          state.watcher_class == MC_ENTITY_CLASS_READY && state.watcher_type_count == 7);
    result = 0;
cleanup:
    mc_entity_release(entities[0]);
    mc_entity_release(entities[1]);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fprintf(stderr, "entity tests require the qualified strict SSE environment\n");
        return 1;
    }
    for (unsigned int i = 0; i < 2; ++i) {
        const mc_entity_source source = i == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER;
        if (test_raw(source) || test_constructors(source, false) ||
            test_constructors(source, true) || test_stack_mutators(source) || test_ids(source) ||
            test_budgets(source) || test_fp_prefix(source) || test_math_limit(source))
            return 1;
    }
    printf("Entity construction: %u checks passed\n", checks);
    return 0;
}
