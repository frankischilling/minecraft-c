#include "mc/block/registry/registry.h"
#include "mc/block/registry/runtime.h"

#include "../../../src/block/registry/horizontal_directions_internal.h"
#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/core/java/random_profile_internal.h"

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

/* Literal expectations from complete paired jj/ge bodies. Budget, projection,
 * phase and frame controls test native invariants, not original execution or
 * JVM allocation-failure parity. No instance constructor is exercised here. */
static const int32_t expected_a[4] = {3, 4, 2, 5};
static const int32_t expected_b[4] = {2, 3, 0, 1};
static const int32_t expected_c[4][6] = {
    {1, 0, 3, 2, 5, 4}, {1, 0, 5, 4, 2, 3}, {1, 0, 2, 3, 4, 5}, {1, 0, 4, 5, 3, 2}};

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

static bool independent_cold(const mc_registry_classes *owner) {
    if (owner->blocks_phase != MC_REGISTRY_COLD || owner->block_initializer_pc != -1 ||
        owner->named_blocks != 0 || owner->blocks.m != NULL || owner->ve_a != NULL ||
        owner->direction_state.phase != MC_REGISTRY_COLD || owner->direction_a != NULL ||
        owner->direction_b != NULL || owner->direction_c != NULL || owner->direction_d != NULL ||
        owner->items.c != NULL || owner->entities != NULL ||
        owner->translation_state.phase != MC_REGISTRY_COLD ||
        owner->crafting_state.phase != MC_REGISTRY_COLD)
        return false;
    for (size_t i = 0; i < MC_BLOCK_KIND_COUNT; ++i)
        if (owner->block_states[i].phase != MC_REGISTRY_COLD)
            return false;
    return true;
}

static bool ready_tables(const mc_registry_classes *owner) {
    if (owner->horizontal_state.phase != MC_REGISTRY_READY || owner->horizontal_state.pc != 175 ||
        owner->horizontal_a == NULL || owner->horizontal_b == NULL || owner->horizontal_c == NULL ||
        owner->horizontal_a != owner->horizontal_storage[0] ||
        owner->horizontal_b != owner->horizontal_storage[1] ||
        owner->horizontal_c != owner->horizontal_c_storage ||
        memcmp(owner->horizontal_a, expected_a, sizeof(expected_a)) != 0 ||
        memcmp(owner->horizontal_b, expected_b, sizeof(expected_b)) != 0)
        return false;
    const void *objects[7] = {owner->horizontal_a,       owner->horizontal_b,
                              owner->horizontal_c,       owner->horizontal_rows[0],
                              owner->horizontal_rows[1], owner->horizontal_rows[2],
                              owner->horizontal_rows[3]};
    for (size_t i = 0; i < COUNT(objects); ++i) {
        if (objects[i] == NULL)
            return false;
        for (size_t j = 0; j < i; ++j)
            if (objects[i] == objects[j])
                return false;
    }
    for (size_t i = 0; i < 4; ++i)
        if (owner->horizontal_c[i] != owner->horizontal_rows[i] ||
            memcmp(owner->horizontal_c[i], expected_c[i], sizeof(expected_c[i])) != 0)
            return false;
    return true;
}

static bool success_and_captures(mc_registry_options options, size_t costs[7]) {
    bool ok = false, retained = false;
    mc_registry_classes *owner = NULL, *other = NULL;
    mc_registry_error error = poison();
    mc_registry_horizontal_view first = {0}, current = {0}, foreign = {0};
    CHECK(mc_registry_classes_allocate(&options, &owner, NULL) == MC_REGISTRY_OK &&
          mc_registry_classes_allocate(&options, &other, NULL) == MC_REGISTRY_OK);
    const size_t before = owner->allocation_bytes;
    CHECK(independent_cold(owner) && owner->horizontal_state.phase == MC_REGISTRY_COLD &&
          owner->horizontal_a == NULL && owner->horizontal_b == NULL &&
          owner->horizontal_c == NULL && owner->horizontal_c_storage == NULL);
    CHECK(mc_registry_horizontal_b_get(owner, &first, &error) == MC_REGISTRY_OK &&
          fresh_error(&error, MC_REGISTRY_OK) && ready_tables(owner) && independent_cold(owner) &&
          first.classes == owner && first.identity == owner->horizontal_b && first.length == 4 &&
          owner->entered == 0 && owner->initializer == NULL);
    const size_t used = owner->allocation_bytes;
    const size_t payload = 32u * sizeof(int32_t) + 4u * sizeof(int32_t *);
    CHECK(used - before >= payload && (used - before - payload) % 7u == 0);
    const size_t record = (used - before - payload) / 7u;
    CHECK(record > 0);
    costs[0] = costs[1] = record + 4u * sizeof(int32_t);
    costs[2] = record + 4u * sizeof(int32_t *);
    for (size_t i = 3; i < 7; ++i)
        costs[i] = record + 6u * sizeof(int32_t);
    for (int32_t i = 0; i < 4; ++i) {
        int32_t value = 91;
        error = poison();
        CHECK(mc_registry_horizontal_at(&first, i, &value, &error) == MC_REGISTRY_OK &&
              value == expected_b[i] && fresh_error(&error, MC_REGISTRY_OK));
    }
    CHECK(mc_registry_horizontal_b_get(owner, &current, NULL) == MC_REGISTRY_OK &&
          current.identity == first.identity && owner->allocation_bytes == used);
    CHECK(mc_registry_horizontal_b_get(other, &foreign, NULL) == MC_REGISTRY_OK &&
          ready_tables(other) && independent_cold(other) && foreign.identity != first.identity);

    /* Restore each mutable native projection/cell/phase before CHECK branches. */
    int32_t value = 91;
    const int32_t saved_cell = owner->horizontal_b[0];
    owner->horizontal_b[0] = 23;
    const mc_registry_status changed = mc_registry_horizontal_at(&first, 0, &value, NULL);
    owner->horizontal_b[0] = saved_cell;
    CHECK(changed == MC_REGISTRY_OK && value == 23);
    int32_t *const saved_b = owner->horizontal_b;
    owner->horizontal_b = owner->horizontal_a;
    const mc_registry_status replaced = mc_registry_horizontal_b_get(owner, &current, NULL);
    owner->horizontal_b = saved_b;
    CHECK(replaced == MC_REGISTRY_OK && current.identity == owner->horizontal_a &&
          current.length == 4 && first.identity == saved_b);
    CHECK(mc_registry_horizontal_at(&first, 0, &value, NULL) == MC_REGISTRY_OK && value == 2);
    CHECK(mc_registry_horizontal_at(&current, 0, &value, NULL) == MC_REGISTRY_OK && value == 3);
    owner->horizontal_b = NULL;
    const mc_registry_status captured_null = mc_registry_horizontal_b_get(owner, &current, NULL);
    const mc_registry_status old_capture = mc_registry_horizontal_at(&first, 0, &value, NULL);
    owner->horizontal_b = saved_b;
    CHECK(captured_null == MC_REGISTRY_OK && current.classes == owner && current.identity == NULL &&
          current.length == 0 && old_capture == MC_REGISTRY_OK && value == 2);
    static const int32_t bad_indices[] = {INT32_MIN, -1, 4, INT32_MAX};
    for (size_t i = 0; i < COUNT(bad_indices); ++i) {
        value = 91;
        error = poison();
        CHECK(mc_registry_horizontal_at(&current, bad_indices[i], &value, &error) ==
                  MC_REGISTRY_NULL_REFERENCE &&
              value == 91 && fresh_error(&error, MC_REGISTRY_NULL_REFERENCE));
        error = poison();
        CHECK(mc_registry_horizontal_at(&first, bad_indices[i], &value, &error) ==
                  MC_REGISTRY_INDEX_OUT_OF_RANGE &&
              value == 91 && fresh_error(&error, MC_REGISTRY_INDEX_OUT_OF_RANGE));
    }
    owner->horizontal_b = other->horizontal_b;
    const mc_registry_status captured_foreign = mc_registry_horizontal_b_get(owner, &current, NULL);
    owner->horizontal_b = saved_b;
    value = 91;
    error = poison();
    CHECK(captured_foreign == MC_REGISTRY_OK && current.identity == foreign.identity &&
          mc_registry_horizontal_at(&current, 0, &value, &error) ==
              MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          value == 91 && fresh_error(&error, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY));
    current = first;
    current.classes = other;
    CHECK(mc_registry_horizontal_at(&current, 0, &value, NULL) ==
              MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          value == 91);
    owner->horizontal_b = owner->horizontal_rows[0];
    const mc_registry_status captured_row = mc_registry_horizontal_b_get(owner, &current, NULL);
    owner->horizontal_b = saved_b;
    CHECK(captured_row == MC_REGISTRY_OK && current.identity == owner->horizontal_rows[0] &&
          mc_registry_horizontal_at(&current, 0, &value, NULL) ==
              MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          value == 91);
    const mc_registry_class_state saved_state = owner->horizontal_state;
    owner->horizontal_state.phase = MC_REGISTRY_FAILED;
    const mc_registry_status retained_phase = mc_registry_horizontal_at(&first, 1, &value, NULL);
    owner->horizontal_state = saved_state;
    CHECK(retained_phase == MC_REGISTRY_OK && value == 3 && ready_tables(owner) &&
          owner->allocation_bytes == used && independent_cold(owner));

    CHECK(mc_registry_classes_retain(owner, options.side, options.random_profile, NULL) ==
          MC_REGISTRY_OK);
    retained = true;
    CHECK(mc_registry_classes_free(owner, &error) == MC_REGISTRY_BUSY && ready_tables(owner));
    CHECK(mc_registry_classes_release(owner, NULL) == MC_REGISTRY_OK);
    retained = false;
    /* Arena teardown follows retained allocation handles, not these mutable
     * source fields. A foreign projection must not be adopted or freed. */
    owner->horizontal_b = other->horizontal_b;
    owner->horizontal_c = other->horizontal_c;
    CHECK(mc_registry_classes_free(owner, NULL) == MC_REGISTRY_OK);
    owner = NULL;
    CHECK(mc_registry_horizontal_at(&foreign, 2, &value, NULL) == MC_REGISTRY_OK && value == 0 &&
          ready_tables(other));
    ok = true;
cleanup:
    if (retained && mc_registry_classes_release(owner, NULL) != MC_REGISTRY_OK)
        ok = false;
    if (mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK ||
        mc_registry_classes_free(other, NULL) != MC_REGISTRY_OK)
        ok = false;
    return ok;
}

static bool failure_prefix(mc_registry_options options, const size_t costs[7], size_t prefix) {
    bool ok = false;
    mc_registry_classes *owner = NULL;
    size_t allowed = sizeof(*owner);
    for (size_t i = 0; i < prefix; ++i)
        allowed += costs[i];
    options.max_allocation_bytes = allowed + costs[prefix] - 1u;
    CHECK(mc_registry_classes_allocate(&options, &owner, NULL) == MC_REGISTRY_OK);
    int32_t anchor = 53;
    mc_registry_horizontal_view out = {NULL, &anchor, SIZE_MAX};
    mc_registry_error error = poison();
    static const int32_t pcs[7] = {1, 23, 45, 52, 83, 114, 145};
    CHECK(mc_registry_horizontal_b_get(owner, &out, &error) == MC_REGISTRY_LIMIT);
    CHECK(out.classes == NULL && out.identity == &anchor && out.length == SIZE_MAX && anchor == 53);
    CHECK(owner->horizontal_state.phase == MC_REGISTRY_FAILED &&
          owner->horizontal_state.pc == pcs[prefix] && owner->allocation_bytes == allowed &&
          owner->entered == 0 && owner->initializer == NULL && independent_cold(owner));
    CHECK(error.status == MC_REGISTRY_LIMIT && error.initializer_pc == -1 && error.block_id == -1 &&
          error.constructor_class == NULL && error.constructor_pc == -1 &&
          error.item_argument == -1 && error.active_initializer_class != NULL &&
          strcmp(error.active_initializer_class,
                 options.side == MC_REGISTRY_CLIENT ? "jj" : "ge") == 0 &&
          error.active_initializer_pc == pcs[prefix] && same_error(&error, &owner->last_error) &&
          same_error(&error, &owner->horizontal_state.failure));
    if (prefix >= 1)
        CHECK(owner->horizontal_a == owner->horizontal_storage[0] && owner->horizontal_a != NULL &&
              memcmp(owner->horizontal_a, expected_a, sizeof(expected_a)) == 0);
    else
        CHECK(owner->horizontal_a == NULL && owner->horizontal_storage[0] == NULL);
    if (prefix >= 2)
        CHECK(owner->horizontal_b == owner->horizontal_storage[1] && owner->horizontal_b != NULL &&
              memcmp(owner->horizontal_b, expected_b, sizeof(expected_b)) == 0);
    else
        CHECK(owner->horizontal_b == NULL && owner->horizontal_storage[1] == NULL);
    CHECK(owner->horizontal_c == NULL && (owner->horizontal_c_storage != NULL) == (prefix >= 3));
    for (size_t i = 0; i < 4; ++i) {
        if (prefix >= i + 4)
            CHECK(owner->horizontal_rows[i] != NULL &&
                  owner->horizontal_c_storage[i] == owner->horizontal_rows[i] &&
                  memcmp(owner->horizontal_rows[i], expected_c[i], sizeof(expected_c[i])) == 0);
        else
            CHECK(owner->horizontal_rows[i] == NULL &&
                  (owner->horizontal_c_storage == NULL || owner->horizontal_c_storage[i] == NULL));
    }
    const mc_registry_error first = owner->horizontal_state.failure;
    mc_registry_allocation *const allocations = owner->allocations;
    const mc_registry_horizontal_view null_view = {owner, NULL, 0};
    int32_t value = 91;
    CHECK(mc_registry_horizontal_at(&null_view, -1, &value, &error) == MC_REGISTRY_NULL_REFERENCE &&
          value == 91 && same_error(&first, &owner->horizontal_state.failure));
    owner->options.max_allocation_bytes = SIZE_MAX; /* Native control cannot retry Java init. */
    mc_registry_error cached = first;
    cached.status = MC_REGISTRY_FAILED_INITIALIZATION;
    for (size_t attempt = 0; attempt < 2; ++attempt) {
        error = poison();
        CHECK(mc_registry_horizontal_b_get(owner, &out, &error) ==
                  MC_REGISTRY_FAILED_INITIALIZATION &&
              same_error(&error, &cached) && same_error(&owner->last_error, &cached) &&
              same_error(&owner->horizontal_state.failure, &first) &&
              owner->allocation_bytes == allowed && owner->allocations == allocations &&
              out.classes == NULL && out.identity == &anchor && out.length == SIZE_MAX);
    }
    ok = true;
cleanup:
    if (mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK)
        ok = false;
    return ok;
}

static bool nested_and_reentry(mc_registry_options options, size_t first_cost, bool fail_body) {
    bool ok = false, entered = false;
    mc_registry_classes *owner = NULL;
    mc_registry_initializer outer;
    if (fail_body)
        options.max_allocation_bytes = sizeof(*owner) + first_cost - 1u;
    CHECK(mc_registry_classes_allocate(&options, &owner, NULL) == MC_REGISTRY_OK);
    /* Synthetic admitted outer Block frame; no outer source body is run. */
    owner->blocks_phase = MC_REGISTRY_INITIALIZING;
    owner->block_initializer_pc = 3398;
    owner->entered = 1;
    mc_registry_initializer_enter(owner, &outer, "uu", "na", &owner->block_initializer_pc);
    entered = true;
    mc_block caller_block = {.requested_id = 61, .constructor_pc = 42};
    mc_item caller_item = {.requested_argument = 301, .constructor_pc = 35};
    owner->current_block = &caller_block;
    owner->current_item = &caller_item;
    mc_registry_horizontal_view out = {0};
    mc_registry_error error = poison();
    const mc_registry_status status = mc_registry_horizontal_b_get(owner, &out, &error);
    CHECK(owner->entered == 1 && owner->initializer == &outer && outer.parent == NULL &&
          owner->current_block == &caller_block && owner->current_item == &caller_item &&
          owner->blocks_phase == MC_REGISTRY_INITIALIZING && owner->block_initializer_pc == 3398 &&
          owner->direction_state.phase == MC_REGISTRY_COLD && owner->ve_a == NULL);
    if (fail_body)
        CHECK(status == MC_REGISTRY_LIMIT && error.initializer_pc == 3398 && error.block_id == -1 &&
              error.constructor_class == NULL && error.constructor_pc == -1 &&
              error.item_argument == -1 && error.active_initializer_pc == 1 &&
              strcmp(error.active_initializer_class,
                     options.side == MC_REGISTRY_CLIENT ? "jj" : "ge") == 0 &&
              owner->horizontal_state.phase == MC_REGISTRY_FAILED);
    else
        CHECK(status == MC_REGISTRY_OK && ready_tables(owner));
    CHECK(mc_registry_classes_free(owner, NULL) == MC_REGISTRY_BUSY);
    mc_registry_initializer_leave(owner, &outer);
    owner->entered = 0;
    owner->blocks_phase = MC_REGISTRY_COLD;
    owner->block_initializer_pc = -1;
    entered = false;
    CHECK(owner->initializer == NULL && owner->current_block == NULL &&
          owner->current_item == NULL);
    if (!fail_body) {
        const size_t bytes = owner->allocation_bytes;
        const mc_registry_class_state saved = owner->horizontal_state;
        int32_t *const saved_b = owner->horizontal_b;
        owner->horizontal_state.phase = MC_REGISTRY_INITIALIZING;
        owner->horizontal_state.pc = 23;
        owner->horizontal_b = NULL;
        const mc_registry_status reentry = mc_registry_horizontal_b_get(owner, &out, &error);
        const bool preserved = owner->horizontal_state.phase == MC_REGISTRY_INITIALIZING &&
                               owner->horizontal_state.pc == 23 && owner->entered == 0;
        owner->horizontal_state = saved;
        owner->horizontal_b = saved_b;
        CHECK(reentry == MC_REGISTRY_OK && preserved && out.identity == NULL && out.length == 0 &&
              out.classes == owner && fresh_error(&error, MC_REGISTRY_OK) &&
              owner->allocation_bytes == bytes && ready_tables(owner));
    }
    const unsigned int saved_entered = owner->entered;
    owner->entered = UINT_MAX;
    const mc_registry_status count_limit = mc_registry_horizontal_b_get(owner, &out, &error);
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

static bool stale_block_and_arguments(mc_registry_options options, size_t first_cost,
                                      bool fail_body) {
    bool ok = false;
    mc_registry_classes *owner = NULL;
    if (fail_body)
        options.max_allocation_bytes = sizeof(*owner) + first_cost - 1u;
    CHECK(mc_registry_classes_allocate(&options, &owner, NULL) == MC_REGISTRY_OK);
    /* A native completed/failed driver position is not a live outer frame. */
    owner->blocks_phase = MC_REGISTRY_FAILED;
    owner->block_initializer_pc = 3398;
    owner->block_failure = poison();
    const mc_registry_error block_failure = owner->block_failure;
    int32_t anchor = 53;
    mc_registry_horizontal_view out = {NULL, &anchor, SIZE_MAX};
    mc_registry_error error = poison();
    const mc_registry_status status = mc_registry_horizontal_b_get(owner, &out, &error);
    CHECK(owner->blocks_phase == MC_REGISTRY_FAILED && owner->block_initializer_pc == 3398 &&
          same_error(&owner->block_failure, &block_failure));
    if (fail_body)
        CHECK(status == MC_REGISTRY_LIMIT && error.initializer_pc == -1 &&
              error.active_initializer_pc == 1 && out.classes == NULL && out.identity == &anchor &&
              out.length == SIZE_MAX);
    else
        CHECK(status == MC_REGISTRY_OK && ready_tables(owner));
    owner->last_error = poison();
    const mc_registry_error last = owner->last_error;
    out = (mc_registry_horizontal_view){NULL, &anchor, SIZE_MAX};
    CHECK(mc_registry_horizontal_b_get(NULL, &out, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          out.classes == NULL && out.identity == &anchor && out.length == SIZE_MAX &&
          fresh_error(&error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_horizontal_b_get(owner, NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          fresh_error(&error, MC_REGISTRY_INVALID_ARGUMENT) &&
          same_error(&owner->last_error, &last));
    int32_t value = 91;
    CHECK(mc_registry_horizontal_at(NULL, 0, &value, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          value == 91 && fresh_error(&error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_horizontal_at(&out, 0, &value, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          value == 91 && fresh_error(&error, MC_REGISTRY_INVALID_ARGUMENT));
    if (!fail_body) {
        out = (mc_registry_horizontal_view){owner, owner->horizontal_b, 6};
        CHECK(mc_registry_horizontal_at(&out, 0, &value, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
              value == 91 && fresh_error(&error, MC_REGISTRY_INVALID_ARGUMENT) &&
              same_error(&owner->last_error, &last));
        out.length = 4;
        CHECK(mc_registry_horizontal_at(&out, 0, NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
              fresh_error(&error, MC_REGISTRY_INVALID_ARGUMENT) &&
              same_error(&owner->last_error, &last));
    }
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
        size_t costs[7] = {0};
        CHECK(success_and_captures(options, costs));
        for (size_t prefix = 0; prefix < 7; ++prefix)
            CHECK(failure_prefix(options, costs, prefix));
        CHECK(nested_and_reentry(options, costs[0], false) &&
              nested_and_reentry(options, costs[0], true));
        CHECK(stale_block_and_arguments(options, costs[0], false) &&
              stale_block_and_arguments(options, costs[0], true));
        CHECK(mc_java_random_profile_allocation_bytes(profile) == profile_bytes &&
              atomic_load_explicit(&profile->uniquifier, memory_order_relaxed) == uniquifier &&
              atomic_load_explicit(&profile->holder_phase, memory_order_relaxed) == holder_phase &&
              profile->math == math);
    }
    CHECK(printf("PASS: %u horizontal Direction class checks\n", checks) >= 0);
    result = 0;
cleanup:
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
