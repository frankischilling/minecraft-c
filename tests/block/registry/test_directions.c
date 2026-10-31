#include "mc/block/registry/registry.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/block/registry/runtime_internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "mc/block/registry/runtime.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define COUNT(array) (sizeof(array) / sizeof((array)[0]))
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static unsigned int checks;

/* Independent literals from complete paired wj/od initializer bodies. Native
 * projection/budget/frame controls below are explicit invariant tests, not
 * original execution, Java allocation-failure parity or gameplay routes. */
static const int32_t expected[4][6] = {
    {1, 0, 3, 2, 5, 4}, {0, 0, 0, 0, -1, 1}, {-1, 1, 0, 0, 0, 0}, {0, 0, -1, 1, 0, 0}};

static int32_t *source_array(mc_registry_classes *owner, size_t field) {
    switch (field) {
        case 0:
            return owner->direction_a;
        case 1:
            return owner->direction_b;
        case 2:
            return owner->direction_c;
        case 3:
            return owner->direction_d;
        default:
            return NULL;
    }
}

static bool unrelated_cold(const mc_registry_classes *owner) {
    if (owner->blocks_phase != MC_REGISTRY_COLD || owner->block_initializer_pc != -1 ||
        owner->named_blocks != 0 || owner->blocks.m != NULL || owner->ve_a != NULL ||
        owner->items.c != NULL || owner->translation_state.phase != MC_REGISTRY_COLD ||
        owner->crafting_state.phase != MC_REGISTRY_COLD || owner->entities != NULL)
        return false;
    for (size_t i = 0; i < MC_BLOCK_KIND_COUNT; ++i)
        if (owner->block_states[i].phase != MC_REGISTRY_COLD)
            return false;
    return true;
}

static bool fresh_error(const mc_registry_error *error, mc_registry_status status) {
    return error->status == status && error->initializer_pc == -1 && error->block_id == -1 &&
           error->constructor_class == NULL && error->constructor_pc == -1 &&
           error->profile_status == MC_JAVA_RANDOM_PROFILE_OK &&
           error->active_initializer_class == NULL && error->active_initializer_pc == -1 &&
           error->item_argument == -1 && error->format_error.status == MC_JAVA_FORMAT_OK &&
           error->format_error.offset == 0 && error->format_error.length == 0 &&
           error->format_error.value == 0 && error->format_error.flags == 0 &&
           error->format_error.conversion == 0 && !error->format_error.date_time;
}

static mc_registry_error poison(void) {
    return (mc_registry_error){.status = MC_REGISTRY_DUPLICATE_STATISTIC,
                               .message = "old unrelated error",
                               .initializer_pc = 79,
                               .block_id = 61,
                               .constructor_class = "old constructor",
                               .constructor_pc = 42,
                               .profile_status = MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED,
                               .active_initializer_class = "old initializer",
                               .active_initializer_pc = 35,
                               .item_argument = 301,
                               .format_error = {.status = MC_JAVA_FORMAT_UNSUPPORTED_LOCALE,
                                                .offset = 8,
                                                .length = 9,
                                                .value = -3,
                                                .flags = 7,
                                                .conversion = 'x',
                                                .date_time = true}};
}

static bool same_error(const mc_registry_error *a, const mc_registry_error *b) {
    return a->status == b->status && a->message == b->message &&
           a->initializer_pc == b->initializer_pc && a->block_id == b->block_id &&
           a->constructor_class == b->constructor_class && a->constructor_pc == b->constructor_pc &&
           a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

static bool ready_arrays(mc_registry_classes *owner) {
    if (owner->direction_state.phase != MC_REGISTRY_READY || owner->direction_state.pc != 124)
        return false;
    for (size_t field = 0; field < 4; ++field) {
        if (source_array(owner, field) == NULL ||
            source_array(owner, field) != owner->direction_storage[field] ||
            memcmp(source_array(owner, field), expected[field], sizeof(expected[field])) != 0)
            return false;
        for (size_t previous = 0; previous < field; ++previous)
            if (source_array(owner, field) == source_array(owner, previous))
                return false;
    }
    return true;
}

static bool success_and_capture(mc_registry_options options, size_t *array_bytes) {
    bool ok = false, retained = false;
    mc_registry_classes *owner = NULL, *other = NULL;
    mc_registry_error error = poison();
    mc_registry_direction_view views[4] = {0}, repeat = {0}, other_view = {0};
    CHECK(mc_registry_classes_allocate(&options, &owner, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_allocate(&options, &other, NULL) == MC_REGISTRY_OK);
    const size_t before = owner->allocation_bytes;
    CHECK(unrelated_cold(owner) && unrelated_cold(other) &&
          owner->direction_state.phase == MC_REGISTRY_COLD && owner->direction_a == NULL &&
          owner->direction_b == NULL && owner->direction_c == NULL && owner->direction_d == NULL);
    CHECK(mc_registry_direction_get(owner, MC_REGISTRY_DIRECTION_D, &views[3], &error) ==
              MC_REGISTRY_OK &&
          fresh_error(&error, MC_REGISTRY_OK) && ready_arrays(owner) && unrelated_cold(owner) &&
          owner->entered == 0 && owner->initializer == NULL);
    const size_t used = owner->allocation_bytes;
    CHECK(used > before && (used - before) % 4u == 0);
    *array_bytes = (used - before) / 4u;
    CHECK(*array_bytes >= sizeof(expected[0]));
    for (size_t field = 0; field < 4; ++field) {
        error = poison();
        CHECK(mc_registry_direction_get(owner, (mc_registry_direction_field)field, &views[field],
                                        &error) == MC_REGISTRY_OK &&
              fresh_error(&error, MC_REGISTRY_OK) && views[field].classes == owner &&
              views[field].identity == source_array(owner, field) && views[field].length == 6);
        for (int32_t index = 0; index < 6; ++index) {
            int32_t value = 91;
            error = poison();
            CHECK(mc_registry_direction_at(&views[field], index, &value, &error) ==
                      MC_REGISTRY_OK &&
                  value == expected[field][index] && fresh_error(&error, MC_REGISTRY_OK));
        }
    }
    CHECK(mc_registry_direction_get(owner, MC_REGISTRY_DIRECTION_B, &repeat, NULL) ==
              MC_REGISTRY_OK &&
          repeat.identity == views[1].identity && owner->allocation_bytes == used);
    CHECK(mc_registry_direction_get(other, MC_REGISTRY_DIRECTION_A, &other_view, NULL) ==
              MC_REGISTRY_OK &&
          ready_arrays(other) && unrelated_cold(other));
    for (size_t field = 0; field < 4; ++field)
        for (size_t other_field = 0; other_field < 4; ++other_field)
            CHECK(source_array(owner, field) != source_array(other, other_field));

    /* Explicit native controls: actual cell mutation, source-field replacement
     * and nullable captures are restored before any assertion can branch. */
    int32_t value = 91;
    const int32_t saved_cell = owner->direction_b[4];
    owner->direction_b[4] = 23;
    const mc_registry_status changed = mc_registry_direction_at(&views[1], 4, &value, NULL);
    owner->direction_b[4] = saved_cell;
    CHECK(changed == MC_REGISTRY_OK && value == 23);
    int32_t *const saved_b = owner->direction_b;
    owner->direction_b = owner->direction_d;
    const mc_registry_status replaced =
        mc_registry_direction_get(owner, MC_REGISTRY_DIRECTION_B, &repeat, NULL);
    owner->direction_b = saved_b;
    CHECK(replaced == MC_REGISTRY_OK && repeat.identity == views[3].identity &&
          views[1].identity == saved_b);
    CHECK(mc_registry_direction_at(&views[1], 4, &value, NULL) == MC_REGISTRY_OK && value == -1);
    CHECK(mc_registry_direction_at(&repeat, 4, &value, NULL) == MC_REGISTRY_OK && value == 0);
    owner->direction_b = NULL;
    const mc_registry_status captured_null =
        mc_registry_direction_get(owner, MC_REGISTRY_DIRECTION_B, &repeat, NULL);
    value = 91;
    const mc_registry_status old_capture = mc_registry_direction_at(&views[1], 4, &value, NULL);
    owner->direction_b = saved_b;
    CHECK(captured_null == MC_REGISTRY_OK && repeat.classes == owner && repeat.identity == NULL &&
          repeat.length == 0 && old_capture == MC_REGISTRY_OK && value == -1);
    static const int32_t invalid[] = {INT32_MIN, -1, 6, INT32_MAX};
    for (size_t i = 0; i < COUNT(invalid); ++i) {
        value = 91;
        error = poison();
        CHECK(mc_registry_direction_at(&repeat, invalid[i], &value, &error) ==
                  MC_REGISTRY_NULL_REFERENCE &&
              value == 91 && fresh_error(&error, MC_REGISTRY_NULL_REFERENCE));
        error = poison();
        CHECK(mc_registry_direction_at(&views[1], invalid[i], &value, &error) ==
                  MC_REGISTRY_INDEX_OUT_OF_RANGE &&
              value == 91 && fresh_error(&error, MC_REGISTRY_INDEX_OUT_OF_RANGE));
    }
    owner->direction_b = other->direction_b;
    const mc_registry_status captured_foreign =
        mc_registry_direction_get(owner, MC_REGISTRY_DIRECTION_B, &repeat, NULL);
    owner->direction_b = saved_b;
    CHECK(captured_foreign == MC_REGISTRY_OK && repeat.identity == other->direction_b);
    value = 91;
    error = poison();
    CHECK(mc_registry_direction_at(&repeat, 0, &value, &error) ==
              MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          value == 91 && fresh_error(&error, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY));
    CHECK(ready_arrays(owner) && unrelated_cold(owner) && owner->allocation_bytes == used);

    /* Borrowed captures require a supplying owner hold, rather than acquiring
     * an implicit hold themselves. Ordinary native retirement checks it. */
    CHECK(mc_registry_classes_retain(owner, options.side, options.random_profile, NULL) ==
          MC_REGISTRY_OK);
    retained = true;
    CHECK(mc_registry_classes_free(owner, &error) == MC_REGISTRY_BUSY && ready_arrays(owner));
    CHECK(mc_registry_classes_release(owner, NULL) == MC_REGISTRY_OK);
    retained = false;
    /* Final free uses arena provenance even when a source projection names a
     * different live owner's array. It must not free/adopt that foreign array. */
    owner->direction_b = other->direction_b;
    CHECK(mc_registry_classes_free(owner, NULL) == MC_REGISTRY_OK);
    owner = NULL;
    CHECK(mc_registry_direction_at(&other_view, 2, &value, NULL) == MC_REGISTRY_OK && value == 3 &&
          ready_arrays(other));
    ok = true;
cleanup:
    if (retained && mc_registry_classes_release(owner, NULL) != MC_REGISTRY_OK)
        ok = false;
    if (mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_free(other, NULL) != MC_REGISTRY_OK)
        ok = false;
    return ok;
}

static bool failure_prefix(mc_registry_options options, size_t array_bytes, size_t prefix) {
    bool ok = false;
    mc_registry_classes *owner = NULL;
    options.max_allocation_bytes = sizeof(*owner) + (prefix + 1u) * array_bytes - 1u;
    CHECK(mc_registry_classes_allocate(&options, &owner, NULL) == MC_REGISTRY_OK);
    const size_t before = owner->allocation_bytes;
    int32_t anchor = 53;
    mc_registry_direction_view out = {NULL, &anchor, SIZE_MAX};
    mc_registry_error error = poison();
    static const int32_t allocation_pcs[] = {2, 33, 64, 95};
    CHECK(mc_registry_direction_get(owner, MC_REGISTRY_DIRECTION_D, &out, &error) ==
          MC_REGISTRY_LIMIT);
    CHECK(out.classes == NULL && out.identity == &anchor && out.length == SIZE_MAX && anchor == 53);
    CHECK(owner->direction_state.phase == MC_REGISTRY_FAILED &&
          owner->direction_state.pc == allocation_pcs[prefix] && owner->entered == 0 &&
          owner->initializer == NULL && unrelated_cold(owner) &&
          owner->allocation_bytes == before + prefix * array_bytes);
    CHECK(error.status == MC_REGISTRY_LIMIT && error.initializer_pc == -1 && error.block_id == -1 &&
          error.constructor_class == NULL && error.constructor_pc == -1 &&
          error.item_argument == -1 && error.active_initializer_class != NULL &&
          strcmp(error.active_initializer_class,
                 options.side == MC_REGISTRY_CLIENT ? "wj" : "od") == 0 &&
          error.active_initializer_pc == allocation_pcs[prefix] &&
          same_error(&error, &owner->last_error) &&
          same_error(&error, &owner->direction_state.failure));
    for (size_t field = 0; field < 4; ++field) {
        if (field < prefix)
            CHECK(source_array(owner, field) != NULL &&
                  source_array(owner, field) == owner->direction_storage[field] &&
                  memcmp(source_array(owner, field), expected[field], sizeof(expected[field])) ==
                      0);
        else
            CHECK(source_array(owner, field) == NULL && owner->direction_storage[field] == NULL);
    }
    const mc_registry_error first = owner->direction_state.failure;
    mc_registry_allocation *const allocations = owner->allocations;
    const size_t bytes = owner->allocation_bytes;
    /* A separate reached element error cannot replace the first initializer
     * failure. This view is an explicit native failed-prefix invariant control. */
    const mc_registry_direction_view null_view = {owner, NULL, 0};
    int32_t value = 91;
    CHECK(mc_registry_direction_at(&null_view, -1, &value, &error) == MC_REGISTRY_NULL_REFERENCE &&
          value == 91 && fresh_error(&error, MC_REGISTRY_NULL_REFERENCE) &&
          same_error(&first, &owner->direction_state.failure));
    owner->options.max_allocation_bytes = SIZE_MAX; /* Native budget control, no source retry. */
    mc_registry_error cached = first;
    cached.status = MC_REGISTRY_FAILED_INITIALIZATION;
    for (size_t attempt = 0; attempt < 2; ++attempt) {
        error = poison();
        CHECK(mc_registry_direction_get(owner, MC_REGISTRY_DIRECTION_B, &out, &error) ==
                  MC_REGISTRY_FAILED_INITIALIZATION &&
              same_error(&error, &cached) && same_error(&owner->last_error, &cached) &&
              same_error(&owner->direction_state.failure, &first) &&
              owner->allocation_bytes == bytes && owner->allocations == allocations &&
              out.classes == NULL && out.identity == &anchor && out.length == SIZE_MAX);
    }
    ok = true;
cleanup:
    if (mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK)
        ok = false;
    return ok;
}

static bool entered_flow(mc_registry_options options, size_t array_bytes, bool fail_body) {
    bool ok = false, entered = false;
    mc_registry_classes *owner = NULL;
    mc_registry_initializer outer;
    if (fail_body)
        options.max_allocation_bytes = sizeof(*owner) + array_bytes - 1u;
    CHECK(mc_registry_classes_allocate(&options, &owner, NULL) == MC_REGISTRY_OK);
    /* Explicit native frame/count control verifies that nested class work
     * restores a caller. It does not execute or qualify an outer Block body. */
    owner->block_initializer_pc = 3398;
    owner->entered = 1;
    mc_registry_initializer_enter(owner, &outer, "uu", "na", &owner->block_initializer_pc);
    entered = true;
    mc_registry_direction_view out = {0};
    mc_registry_error error = poison();
    const mc_registry_status status =
        mc_registry_direction_get(owner, MC_REGISTRY_DIRECTION_B, &out, &error);
    CHECK(owner->entered == 1 && owner->initializer == &outer && outer.parent == NULL &&
          owner->block_initializer_pc == 3398 && owner->current_block == NULL &&
          owner->current_item == NULL);
    if (fail_body)
        CHECK(status == MC_REGISTRY_LIMIT && error.initializer_pc == 3398 &&
              error.active_initializer_pc == 2 &&
              owner->direction_state.phase == MC_REGISTRY_FAILED);
    else
        CHECK(status == MC_REGISTRY_OK && ready_arrays(owner));
    CHECK(mc_registry_classes_free(owner, NULL) == MC_REGISTRY_BUSY);
    mc_registry_initializer_leave(owner, &outer);
    owner->entered = 0;
    owner->block_initializer_pc = -1;
    entered = false;
    CHECK(owner->initializer == NULL && owner->current_block == NULL &&
          owner->current_item == NULL);
    const unsigned int saved_entered = owner->entered;
    owner->entered = UINT_MAX;
    const mc_registry_status count_limit =
        mc_registry_direction_get(owner, MC_REGISTRY_DIRECTION_A, &out, &error);
    owner->entered = saved_entered;
    CHECK(count_limit == MC_REGISTRY_LIMIT && fresh_error(&error, MC_REGISTRY_LIMIT));
    ok = true;
cleanup:
    if (entered) {
        mc_registry_initializer_leave(owner, &outer);
        owner->entered = 0;
    }
    if (mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK)
        ok = false;
    return ok;
}

static bool independent_failed_block(mc_registry_options options) {
    bool ok = false;
    mc_registry_classes *owner = NULL;
    CHECK(options.resources == NULL &&
          mc_registry_classes_allocate(&options, &owner, NULL) == MC_REGISTRY_OK);
    mc_registry_error block_error = poison();
    CHECK(mc_registry_classes_initialize_blocks(owner, &block_error) ==
              MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          owner->blocks_phase == MC_REGISTRY_FAILED && owner->block_initializer_pc == 3398 &&
          owner->direction_state.phase == MC_REGISTRY_COLD && owner->direction_a == NULL &&
          owner->ve_a != NULL);
    const mc_registry_error failure = owner->block_failure;
    const size_t profile_bytes = mc_java_random_profile_allocation_bytes(options.random_profile);
    const uint64_t uniquifier =
        atomic_load_explicit(&options.random_profile->uniquifier, memory_order_relaxed);
    mc_registry_direction_view out = {0};
    mc_registry_error error = poison();
    CHECK(mc_registry_direction_get(owner, MC_REGISTRY_DIRECTION_C, &out, &error) ==
              MC_REGISTRY_OK &&
          fresh_error(&error, MC_REGISTRY_OK) && ready_arrays(owner) &&
          owner->blocks_phase == MC_REGISTRY_FAILED && owner->block_initializer_pc == 3398 &&
          same_error(&owner->block_failure, &failure) && owner->ve_a != NULL &&
          owner->allocation_bytes <= owner->options.max_allocation_bytes);
    CHECK(mc_java_random_profile_allocation_bytes(options.random_profile) == profile_bytes &&
          atomic_load_explicit(&options.random_profile->uniquifier, memory_order_relaxed) ==
              uniquifier);
    int32_t value = 91;
    CHECK(mc_registry_direction_at(&out, 1, &value, NULL) == MC_REGISTRY_OK && value == 1);
    CHECK(mc_registry_classes_initialize_blocks(owner, &block_error) ==
              MC_REGISTRY_FAILED_INITIALIZATION &&
          same_error(&owner->block_failure, &failure) && ready_arrays(owner));
    ok = true;
cleanup:
    if (mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK)
        ok = false;
    return ok;
}

int main(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&materials));
    for (int side = (int)MC_REGISTRY_CLIENT; side <= (int)MC_REGISTRY_SERVER; ++side) {
        const mc_registry_options options = {.side = (mc_registry_side)side,
                                             .random_profile = profile,
                                             .materials = materials,
                                             .max_allocation_bytes = SIZE_MAX};
        const size_t profile_bytes = mc_java_random_profile_allocation_bytes(profile);
        const uint64_t uniquifier =
            atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
        const unsigned int holder_phase =
            atomic_load_explicit(&profile->holder_phase, memory_order_relaxed);
        mc_java_math_random_owner *const math = profile->math;
        size_t array_bytes = 0;
        CHECK(success_and_capture(options, &array_bytes));
        for (size_t prefix = 0; prefix < 4; ++prefix)
            CHECK(failure_prefix(options, array_bytes, prefix));
        CHECK(entered_flow(options, array_bytes, false) &&
              entered_flow(options, array_bytes, true));
        CHECK(mc_java_random_profile_allocation_bytes(profile) == profile_bytes &&
              atomic_load_explicit(&profile->uniquifier, memory_order_relaxed) == uniquifier &&
              atomic_load_explicit(&profile->holder_phase, memory_order_relaxed) == holder_phase &&
              profile->math == math);
        CHECK(independent_failed_block(options));
    }
    CHECK(printf("PASS: %u shared Direction class checks\n", checks) >= 0);
    result = 0;
cleanup:
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
