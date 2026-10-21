#include "mc/block/registry/registry.h"
#include "mc/resources/translation/translation.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/block/registry/runtime_internal.h"
#include "../../../src/core/java/random_profile_internal.h"

#include <stdio.h>
#include <string.h>

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool nullable_text_equal(const char *a, const char *b) {
    return a == NULL || b == NULL ? a == b : strcmp(a, b) == 0;
}

static bool same_error(mc_registry_error a, mc_registry_error b) {
    return a.status == b.status && nullable_text_equal(a.message, b.message) &&
           a.initializer_pc == b.initializer_pc && a.block_id == b.block_id &&
           nullable_text_equal(a.constructor_class, b.constructor_class) &&
           a.constructor_pc == b.constructor_pc && a.profile_status == b.profile_status &&
           nullable_text_equal(a.active_initializer_class, b.active_initializer_class) &&
           a.active_initializer_pc == b.active_initializer_pc &&
           a.item_argument == b.item_argument && a.format_error.status == b.format_error.status &&
           a.format_error.offset == b.format_error.offset &&
           a.format_error.length == b.format_error.length &&
           a.format_error.value == b.format_error.value &&
           a.format_error.flags == b.format_error.flags &&
           a.format_error.conversion == b.format_error.conversion &&
           a.format_error.date_time == b.format_error.date_time;
}

static mc_registry_error poison_error(void) {
    return (mc_registry_error){.status = MC_REGISTRY_DUPLICATE_STATISTIC,
                               .message = "poison message",
                               .initializer_pc = 71,
                               .block_id = 314,
                               .constructor_class = "poison constructor",
                               .constructor_pc = 42,
                               .profile_status = MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED,
                               .active_initializer_class = "poison initializer",
                               .active_initializer_pc = 68,
                               .item_argument = 999,
                               .format_error = {.status = MC_JAVA_FORMAT_UNSUPPORTED_LOCALE,
                                                .offset = 3,
                                                .length = 7,
                                                .value = -9,
                                                .flags = 5,
                                                .conversion = 'x',
                                                .date_time = true}};
}

static bool fresh_error(mc_registry_error error, mc_registry_status status) {
    return error.status == status && error.initializer_pc == -1 && error.block_id == -1 &&
           error.constructor_class == NULL && error.constructor_pc == -1 &&
           error.profile_status == MC_JAVA_RANDOM_PROFILE_OK &&
           error.active_initializer_class == NULL && error.active_initializer_pc == -1 &&
           error.item_argument == -1 && error.format_error.status == MC_JAVA_FORMAT_OK &&
           error.format_error.offset == 0 && error.format_error.length == 0 &&
           error.format_error.value == 0 && error.format_error.flags == 0 &&
           error.format_error.conversion == 0 && !error.format_error.date_time &&
           (status == MC_REGISTRY_OK ? error.message == NULL : error.message != NULL);
}

static bool text_is(const mc_registry_string *string, const char *text) {
    mc_registry_string_view view;
    if (!mc_registry_string_read(string, &view) || view.length != strlen(text) ||
        (view.length != 0 && view.units == NULL))
        return false;
    for (size_t i = 0; i < view.length; ++i)
        if (view.units[i] != (uint16_t)(unsigned char)text[i])
            return false;
    return true;
}

static uint64_t uniquifier(const mc_java_random_profile *profile) {
    /* Exclusive tests; no profile operation overlaps these private reads. */
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static bool cold_except_literals(const mc_registry_classes *owner) {
    return owner->blocks_phase == MC_REGISTRY_COLD && owner->block_initializer_pc == -1 &&
           owner->named_blocks == 0 && owner->blocks.m == NULL && owner->blocks.q == NULL &&
           owner->items.b == NULL && owner->translation_state.phase == MC_REGISTRY_COLD &&
           owner->translation_wrapper_state.phase == MC_REGISTRY_COLD &&
           owner->statistics_state.phase == MC_REGISTRY_COLD &&
           owner->statistic_base_state.phase == MC_REGISTRY_COLD &&
           owner->statistic_key_state.phase == MC_REGISTRY_COLD &&
           owner->achievements_state.phase == MC_REGISTRY_COLD &&
           owner->crafting_state.phase == MC_REGISTRY_COLD &&
           owner->smelting_state.phase == MC_REGISTRY_COLD && owner->entered == 0 &&
           owner->initializer == NULL;
}

static int test_cold(mc_registry_options options) {
    int result = 1;
    mc_registry_classes *owner = NULL;
    const mc_registry_string *first = NULL, *same = NULL, *different = NULL, *empty = NULL;
    mc_registry_error error = poison_error();
    const uint64_t before_random = uniquifier(options.random_profile);
    CHECK(mc_registry_classes_allocate(&options, &owner, &error) == MC_REGISTRY_OK);
    CHECK(cold_except_literals(owner));
    const size_t base = owner->allocation_bytes;
    const mc_registry_error before_error = owner->last_error;
    CHECK(mc_registry_runtime_literal_ascii(owner, "runtime.literal.alpha", &first, &error) ==
          MC_REGISTRY_OK);
    CHECK(fresh_error(error, MC_REGISTRY_OK) && text_is(first, "runtime.literal.alpha"));
    CHECK(owner->allocation_bytes > base && cold_except_literals(owner));
    const size_t first_bytes = owner->allocation_bytes;
    const char equal_text[] = "runtime.literal.alpha";
    CHECK(mc_registry_runtime_literal_ascii(owner, equal_text, &same, NULL) == MC_REGISTRY_OK);
    CHECK(same == first && owner->allocation_bytes == first_bytes);
    CHECK(mc_registry_runtime_literal_ascii(owner, "runtime.literal.bravo", &different, &error) ==
          MC_REGISTRY_OK);
    CHECK(different != first && text_is(different, "runtime.literal.bravo"));
    CHECK(text_is(first, "runtime.literal.alpha"));
    CHECK(mc_registry_runtime_literal_ascii(owner, "", &empty, &error) == MC_REGISTRY_OK);
    CHECK(empty != first && empty != different && text_is(empty, ""));
    const size_t all_bytes = owner->allocation_bytes;
    same = NULL;
    CHECK(mc_registry_runtime_literal_ascii(owner, "", &same, &error) == MC_REGISTRY_OK);
    CHECK(same == empty && owner->allocation_bytes == all_bytes);
    CHECK(cold_except_literals(owner) && same_error(owner->last_error, before_error));
    CHECK(uniquifier(options.random_profile) == before_random);

    same = first;
    error = poison_error();
    CHECK(mc_registry_runtime_literal_ascii(NULL, "guard", &same, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(same == first && fresh_error(error, MC_REGISTRY_INVALID_ARGUMENT));
    error = poison_error();
    CHECK(mc_registry_runtime_literal_ascii(owner, NULL, &same, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(same == first && fresh_error(error, MC_REGISTRY_INVALID_ARGUMENT));
    error = poison_error();
    CHECK(mc_registry_runtime_literal_ascii(owner, "guard", NULL, &error) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(fresh_error(error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_runtime_literal_ascii(owner, NULL, &same, NULL) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(same == first && owner->allocation_bytes == all_bytes);
    CHECK(same_error(owner->last_error, before_error) && cold_except_literals(owner));
    CHECK(text_is(first, "runtime.literal.alpha") && text_is(empty, ""));
    result = 0;
cleanup:
    if (owner != NULL && mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK)
        result = 1;
    return result;
}

static int test_literal_budgets(mc_registry_options options) {
    int result = 1;
    mc_registry_classes *measure = NULL, *owner = NULL;
    const mc_registry_string *keep = NULL, *target = NULL, *candidate = NULL;
    mc_registry_error error = {0};
    const uint64_t before_random = uniquifier(options.random_profile);
    CHECK(mc_registry_classes_allocate(&options, &measure, &error) == MC_REGISTRY_OK);
    CHECK(mc_registry_runtime_literal_ascii(measure, "keep", &keep, &error) == MC_REGISTRY_OK);
    const size_t prefix_bytes = measure->allocation_bytes;
    CHECK(mc_registry_runtime_literal_ascii(measure, "runtime.literal.budget", &target, &error) ==
          MC_REGISTRY_OK);
    const size_t cost = measure->allocation_bytes - prefix_bytes;
    CHECK(cost > 0 && cost <= 4096 && prefix_bytes <= SIZE_MAX - cost);
    CHECK(mc_registry_classes_free(measure, NULL) == MC_REGISTRY_OK);
    measure = NULL;
    keep = target = NULL;
    bool saw_partial = false;
    for (size_t extra = 0; extra <= cost; ++extra) {
        options.max_allocation_bytes = prefix_bytes + extra;
        CHECK(mc_registry_classes_allocate(&options, &owner, &error) == MC_REGISTRY_OK);
        CHECK(mc_registry_runtime_literal_ascii(owner, "keep", &keep, &error) == MC_REGISTRY_OK);
        CHECK(owner->allocation_bytes == prefix_bytes && text_is(keep, "keep"));
        candidate = keep;
        error = poison_error();
        const mc_registry_status status =
            mc_registry_runtime_literal_ascii(owner, "runtime.literal.budget", &candidate, &error);
        if (extra == cost) {
            CHECK(status == MC_REGISTRY_OK && fresh_error(error, MC_REGISTRY_OK));
            CHECK(candidate != keep && text_is(candidate, "runtime.literal.budget"));
            CHECK(owner->allocation_bytes == options.max_allocation_bytes);
            target = candidate;
            CHECK(mc_registry_runtime_literal_ascii(owner, "runtime.literal.budget", &candidate,
                                                    &error) == MC_REGISTRY_OK);
            CHECK(candidate == target && owner->allocation_bytes == options.max_allocation_bytes);
        } else {
            CHECK(status == MC_REGISTRY_LIMIT && candidate == keep);
            CHECK(fresh_error(error, MC_REGISTRY_LIMIT) && same_error(error, owner->last_error));
            CHECK(owner->allocation_bytes >= prefix_bytes &&
                  owner->allocation_bytes <= options.max_allocation_bytes);
            if (owner->allocation_bytes > prefix_bytes)
                saw_partial = true;
            const size_t first_failure_bytes = owner->allocation_bytes;
            CHECK(mc_registry_runtime_literal_ascii(owner, "runtime.literal.budget", &candidate,
                                                    NULL) == MC_REGISTRY_LIMIT);
            CHECK(candidate == keep && owner->allocation_bytes >= first_failure_bytes &&
                  owner->allocation_bytes <= options.max_allocation_bytes);
            CHECK(fresh_error(owner->last_error, MC_REGISTRY_LIMIT));
        }
        const size_t retained = owner->allocation_bytes;
        const mc_registry_error retained_error = owner->last_error;
        candidate = NULL;
        CHECK(mc_registry_runtime_literal_ascii(owner, "keep", &candidate, &error) ==
              MC_REGISTRY_OK);
        CHECK(candidate == keep && text_is(keep, "keep") && fresh_error(error, MC_REGISTRY_OK));
        CHECK(owner->allocation_bytes == retained && same_error(owner->last_error, retained_error));
        CHECK(cold_except_literals(owner) && uniquifier(options.random_profile) == before_random);
        CHECK(mc_registry_classes_free(owner, NULL) == MC_REGISTRY_OK);
        owner = NULL;
        keep = target = candidate = NULL;
    }
    CHECK(saw_partial);
    result = 0;
cleanup:
    if (owner != NULL && mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK)
        result = 1;
    if (measure != NULL && mc_registry_classes_free(measure, NULL) != MC_REGISTRY_OK)
        result = 1;
    return result;
}

typedef struct entered_fixture {
    mc_registry_classes *owner;
    const mc_registry_string *anchor, *created;
    unsigned int opens;
    bool valid, expect_limit;
    size_t before_create_bytes, created_cost;
} entered_fixture;

#define OBSERVE(fixture, condition)                                                                \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: callback failed: %s\n", __FILE__, __LINE__, #condition);       \
            (fixture)->valid = false;                                                              \
        }                                                                                          \
    } while (0)

static mc_registry_status missing_resource(void *context, const char *name,
                                           mc_java_properties_input *out) {
    entered_fixture *fixture = context;
    mc_registry_classes *owner = fixture->owner;
    ++fixture->opens;
    OBSERVE(fixture, name != NULL && strcmp(name, "/lang/en_US.lang") == 0 && out != NULL);
    const unsigned int entered = owner->entered;
    mc_registry_initializer *const initializer = owner->initializer;
    const int32_t source_pc =
        initializer != NULL && initializer->pc != NULL ? *initializer->pc : -1;
    const uint64_t before_random = uniquifier(owner->options.random_profile);
    OBSERVE(fixture, entered != 0 && initializer != NULL && source_pc == 4);
    OBSERVE(fixture, owner->translation_state.phase == MC_REGISTRY_INITIALIZING &&
                         owner->translation_wrapper_state.phase == MC_REGISTRY_INITIALIZING);
    const size_t before_bytes = owner->allocation_bytes;
    const mc_registry_string *literal = NULL;
    mc_registry_error error = poison_error();
    OBSERVE(fixture, mc_registry_runtime_literal_ascii(owner, "callback.anchor", &literal,
                                                       &error) == MC_REGISTRY_OK);
    OBSERVE(fixture, literal == fixture->anchor && owner->allocation_bytes == before_bytes);
    OBSERVE(fixture, fresh_error(error, MC_REGISTRY_OK));
    fixture->before_create_bytes = owner->allocation_bytes;
    literal = fixture->anchor;
    const mc_registry_status status =
        mc_registry_runtime_literal_ascii(owner, "entered.literal", &literal, &error);
    if (fixture->expect_limit) {
        OBSERVE(fixture, status == MC_REGISTRY_LIMIT && literal == fixture->anchor);
        OBSERVE(fixture,
                fresh_error(error, MC_REGISTRY_LIMIT) && same_error(error, owner->last_error));
        OBSERVE(fixture, owner->allocation_bytes > fixture->before_create_bytes &&
                             owner->allocation_bytes <= owner->options.max_allocation_bytes);
    } else {
        OBSERVE(fixture, status == MC_REGISTRY_OK && fresh_error(error, MC_REGISTRY_OK));
        fixture->created = literal;
        OBSERVE(fixture,
                text_is(literal, "entered.literal") && owner->allocation_bytes > before_bytes);
        fixture->created_cost = owner->allocation_bytes - fixture->before_create_bytes;
        const size_t created_bytes = owner->allocation_bytes;
        literal = NULL;
        OBSERVE(fixture, mc_registry_runtime_literal_ascii(owner, "entered.literal", &literal,
                                                           &error) == MC_REGISTRY_OK);
        OBSERVE(fixture, literal == fixture->created && owner->allocation_bytes == created_bytes);
    }
    OBSERVE(fixture, owner->entered == entered && owner->initializer == initializer &&
                         initializer != NULL && initializer->pc != NULL &&
                         *initializer->pc == source_pc);
    OBSERVE(fixture, owner->blocks_phase == MC_REGISTRY_COLD && owner->blocks.m == NULL &&
                         owner->named_blocks == 0);
    OBSERVE(fixture, uniquifier(owner->options.random_profile) == before_random);
    OBSERVE(fixture, mc_registry_classes_initialize_translation(owner, &error) == MC_REGISTRY_BUSY);
    OBSERVE(fixture, owner->entered == entered && owner->initializer == initializer);
    /* A real absent stream, not a parser or resource replacement success. */
    return MC_REGISTRY_NULL_REFERENCE;
}

static int test_entered(mc_registry_options options) {
    int result = 1;
    mc_registry_classes *owner = NULL;
    entered_fixture fixture = {.valid = true};
    const mc_registry_resources resources = {.context = &fixture, .open = missing_resource};
    options.resources = &resources;
    mc_registry_error error = {0};
    const uint64_t before_random = uniquifier(options.random_profile);
    CHECK(mc_registry_classes_allocate(&options, &owner, &error) == MC_REGISTRY_OK);
    fixture.owner = owner;
    CHECK(mc_registry_runtime_literal_ascii(owner, "callback.anchor", &fixture.anchor, &error) ==
          MC_REGISTRY_OK);
    CHECK(fixture.opens == 0 && cold_except_literals(owner));
    CHECK(mc_registry_classes_initialize_translation(owner, &error) == MC_REGISTRY_NULL_REFERENCE);
    CHECK(fixture.valid && fixture.opens == 1 && fixture.created != NULL);
    const char *declaring = options.side == MC_REGISTRY_CLIENT ? "nh" : "ii";
    CHECK(nullable_text_equal(error.constructor_class, declaring) && error.constructor_pc == 26);
    CHECK(nullable_text_equal(error.active_initializer_class, declaring) &&
          error.active_initializer_pc == 4 && error.initializer_pc == -1);
    CHECK(owner->translation_state.phase == MC_REGISTRY_FAILED &&
          owner->translation_wrapper_state.phase == MC_REGISTRY_FAILED && owner->entered == 0 &&
          owner->initializer == NULL && owner->blocks_phase == MC_REGISTRY_COLD);
    CHECK(text_is(fixture.anchor, "callback.anchor") &&
          text_is(fixture.created, "entered.literal"));
    const mc_registry_error first_failure = error;
    const size_t before_literal = owner->allocation_bytes;
    const mc_registry_string *same = NULL;
    CHECK(mc_registry_runtime_literal_ascii(owner, "entered.literal", &same, &error) ==
          MC_REGISTRY_OK);
    CHECK(same == fixture.created && owner->allocation_bytes == before_literal &&
          fixture.opens == 1);
    CHECK(same_error(owner->last_error, first_failure) && fresh_error(error, MC_REGISTRY_OK));
    CHECK(uniquifier(options.random_profile) == before_random);

    const size_t entry_prefix = fixture.before_create_bytes;
    const size_t cost = fixture.created_cost;
    CHECK(cost > 1 && cost <= 4096 && entry_prefix <= SIZE_MAX - cost);
    CHECK(mc_registry_classes_free(owner, NULL) == MC_REGISTRY_OK);
    owner = NULL;
    fixture = (entered_fixture){.valid = true, .expect_limit = true};
    options.max_allocation_bytes = entry_prefix + cost - 1;
    CHECK(mc_registry_classes_allocate(&options, &owner, &error) == MC_REGISTRY_OK);
    fixture.owner = owner;
    CHECK(mc_registry_runtime_literal_ascii(owner, "callback.anchor", &fixture.anchor, &error) ==
          MC_REGISTRY_OK);
    CHECK(fixture.opens == 0);
    CHECK(mc_registry_classes_initialize_translation(owner, &error) == MC_REGISTRY_NULL_REFERENCE);
    CHECK(fixture.valid && fixture.opens == 1 && fixture.created == NULL);
    CHECK(fixture.before_create_bytes == entry_prefix);
    CHECK(nullable_text_equal(error.constructor_class, declaring) && error.constructor_pc == 26 &&
          nullable_text_equal(error.active_initializer_class, declaring) &&
          error.active_initializer_pc == 4);
    CHECK(owner->translation_state.phase == MC_REGISTRY_FAILED &&
          owner->translation_wrapper_state.phase == MC_REGISTRY_FAILED && owner->entered == 0 &&
          owner->initializer == NULL && owner->blocks_phase == MC_REGISTRY_COLD);
    CHECK(text_is(fixture.anchor, "callback.anchor") &&
          uniquifier(options.random_profile) == before_random);
    result = 0;
cleanup:
    if (owner != NULL && mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK)
        result = 1;
    return result;
}

static int test_fresh_failure(mc_registry_options options) {
    int result = 1;
    mc_registry_classes *measure = NULL, *owner = NULL;
    const mc_registry_string *anchor = NULL, *literal = NULL, *candidate = NULL;
    mc_registry_error error = {0};
    CHECK(mc_registry_classes_allocate(&options, &measure, &error) == MC_REGISTRY_OK);
    CHECK(mc_registry_runtime_literal_ascii(measure, "failed.anchor", &anchor, &error) ==
          MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_blocks(measure, &error) ==
          MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    const size_t failure_bytes = measure->allocation_bytes;
    const uint64_t measure_random = uniquifier(options.random_profile);
    CHECK(mc_registry_runtime_literal_ascii(measure, "runtime.literal.after.failure", &literal,
                                            &error) == MC_REGISTRY_OK);
    CHECK(uniquifier(options.random_profile) == measure_random);
    const size_t literal_cost = measure->allocation_bytes - failure_bytes;
    CHECK(literal_cost > 1 && literal_cost <= 4096 && failure_bytes <= SIZE_MAX - literal_cost);
    CHECK(mc_registry_classes_free(measure, NULL) == MC_REGISTRY_OK);
    measure = NULL;
    anchor = literal = NULL;

    /* Real public immutable creation limit; no phase/counter/budget writes. */
    options.max_allocation_bytes = failure_bytes + literal_cost - 1;
    CHECK(mc_registry_classes_allocate(&options, &owner, &error) == MC_REGISTRY_OK);
    CHECK(mc_registry_runtime_literal_ascii(owner, "failed.anchor", &anchor, &error) ==
          MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_blocks(owner, &error) ==
          MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY);
    CHECK(owner->allocation_bytes == failure_bytes);
    const uint64_t before_runtime_literal = uniquifier(options.random_profile);
    const char *declaring = options.side == MC_REGISTRY_CLIENT ? "nh" : "ii";
    CHECK(nullable_text_equal(error.constructor_class, declaring) && error.constructor_pc == 23);
    CHECK(nullable_text_equal(error.active_initializer_class, declaring) &&
          error.active_initializer_pc == 4 && error.initializer_pc == 3398);
    const mc_registry_error first_failure = error;
    CHECK(same_error(owner->block_failure, first_failure) &&
          same_error(owner->translation_state.failure, first_failure) &&
          same_error(owner->translation_wrapper_state.failure, first_failure));
    candidate = anchor;
    error = poison_error();
    CHECK(mc_registry_runtime_literal_ascii(owner, "runtime.literal.after.failure", &candidate,
                                            &error) == MC_REGISTRY_LIMIT);
    CHECK(candidate == anchor && fresh_error(error, MC_REGISTRY_LIMIT));
    CHECK(same_error(owner->last_error, error) &&
          strcmp(error.message, "registry allocation bound") == 0);
    CHECK(owner->allocation_bytes > failure_bytes &&
          owner->allocation_bytes <= options.max_allocation_bytes);
    CHECK(same_error(owner->block_failure, first_failure) &&
          same_error(owner->translation_state.failure, first_failure) &&
          same_error(owner->translation_wrapper_state.failure, first_failure));
    CHECK(owner->translation_state.phase == MC_REGISTRY_FAILED &&
          owner->translation_wrapper_state.phase == MC_REGISTRY_FAILED &&
          owner->blocks_phase == MC_REGISTRY_FAILED && owner->entered == 0 &&
          owner->initializer == NULL);
    CHECK(text_is(anchor, "failed.anchor"));
    const size_t retained_bytes = owner->allocation_bytes;
    const mc_registry_error runtime_failure = owner->last_error;
    candidate = NULL;
    CHECK(mc_registry_runtime_literal_ascii(owner, "failed.anchor", &candidate, &error) ==
          MC_REGISTRY_OK);
    CHECK(candidate == anchor && fresh_error(error, MC_REGISTRY_OK));
    CHECK(owner->allocation_bytes == retained_bytes &&
          same_error(owner->last_error, runtime_failure));
    mc_registry_error expected_retry = first_failure;
    expected_retry.status = MC_REGISTRY_FAILED_INITIALIZATION;
    expected_retry.message = "Block initialization previously failed";
    CHECK(mc_registry_classes_initialize_blocks(owner, &error) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(same_error(error, expected_retry) && owner->allocation_bytes == retained_bytes);
    CHECK(same_error(owner->last_error, runtime_failure) &&
          same_error(owner->block_failure, first_failure) &&
          same_error(owner->translation_state.failure, first_failure) &&
          same_error(owner->translation_wrapper_state.failure, first_failure));
    CHECK(uniquifier(options.random_profile) == before_runtime_literal &&
          text_is(anchor, "failed.anchor"));
    result = 0;
cleanup:
    if (owner != NULL && mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK)
        result = 1;
    if (measure != NULL && mc_registry_classes_free(measure, NULL) != MC_REGISTRY_OK)
        result = 1;
    return result;
}

typedef struct p_entered_fixture {
    mc_registry_classes *owner;
    mc_registry_block_p_view captured;
    const mc_java_random *randoms[4];
    unsigned char random_bytes[4][sizeof(mc_java_random)];
    size_t captured_bytes;
    uint64_t captured_uniquifier;
    unsigned int opens;
    bool valid;
} p_entered_fixture;

static mc_registry_status p_missing_resource(void *context, const char *name,
                                             mc_java_properties_input *out) {
    p_entered_fixture *fixture = context;
    mc_registry_classes *owner = fixture->owner;
    ++fixture->opens;
    OBSERVE(fixture, name != NULL && strcmp(name, "/lang/en_US.lang") == 0 && out != NULL);
    OBSERVE(fixture, owner->blocks_phase == MC_REGISTRY_INITIALIZING &&
                         owner->block_initializer_pc == 3398 && owner->named_blocks == 96);
    OBSERVE(fixture, owner->translation_state.phase == MC_REGISTRY_INITIALIZING &&
                         owner->translation_wrapper_state.phase == MC_REGISTRY_INITIALIZING);
    OBSERVE(fixture, owner->entered != 0 && owner->initializer != NULL &&
                         owner->initializer->pc != NULL && *owner->initializer->pc == 4);
    OBSERVE(fixture, owner->blocks.p != NULL && owner->blocks.m != NULL);
    if (owner->initializer == NULL || owner->initializer->pc == NULL || owner->blocks.p == NULL ||
        owner->blocks.m == NULL)
        return MC_REGISTRY_NULL_REFERENCE;

    /* Explicit native callback input from this live owner's actual array.
     * This is not a successful p_get while Block initialization is entered. */
    fixture->captured = (mc_registry_block_p_view){owner, owner->blocks.p, MC_REGISTRY_BLOCK_SLOTS};
    fixture->captured_bytes = owner->allocation_bytes;
    fixture->captured_uniquifier = uniquifier(owner->options.random_profile);
    static const size_t random_ids[] = {23, 54, 61, 62};
    for (size_t i = 0; i < 4; ++i) {
        const mc_block *block = owner->blocks.m[random_ids[i]];
        OBSERVE(fixture, block != NULL);
        if (block == NULL)
            return MC_REGISTRY_NULL_REFERENCE;
        const mc_block_kind kind = i == 0 ? MC_BLOCK_XQ_PA : i == 1 ? MC_BLOCK_E_E : MC_BLOCK_TC_LZ;
        OBSERVE(fixture, block->kind == kind);
        if (block->kind != kind)
            return MC_REGISTRY_NULL_REFERENCE;
        fixture->randoms[i] = i == 0   ? block->subtype.xq.a
                              : i == 1 ? block->subtype.e.a
                                       : block->subtype.tc.a;
        OBSERVE(fixture, fixture->randoms[i] != NULL);
        if (fixture->randoms[i] == NULL)
            return MC_REGISTRY_NULL_REFERENCE;
        memcpy(fixture->random_bytes[i], fixture->randoms[i], sizeof(mc_java_random));
    }

    mc_registry_initializer *const initializer = owner->initializer;
    unsigned char owner_before[sizeof(*owner)], frame_before[sizeof(*initializer)];
    bool p_before[MC_REGISTRY_BLOCK_SLOTS];
    memcpy(owner_before, owner, sizeof(owner_before));
    memcpy(frame_before, initializer, sizeof(frame_before));
    memcpy(p_before, fixture->captured.identity, sizeof(p_before));
    const size_t profile_bytes =
        mc_java_random_profile_allocation_bytes(owner->options.random_profile);
    const unsigned int holder_phase =
        atomic_load_explicit(&owner->options.random_profile->holder_phase, memory_order_relaxed);
    mc_java_math_random_owner *const math = owner->options.random_profile->math;
    mc_registry_block_p_view nested = {.classes = NULL, .identity = NULL, .length = SIZE_MAX};
    mc_registry_error error = poison_error();
    OBSERVE(fixture, mc_registry_block_p_get(owner, &nested, &error) == MC_REGISTRY_BUSY);
    OBSERVE(fixture,
            nested.classes == NULL && nested.identity == NULL && nested.length == SIZE_MAX);
    OBSERVE(fixture, fresh_error(error, MC_REGISTRY_BUSY));
    OBSERVE(fixture, memcmp(owner_before, owner, sizeof(owner_before)) == 0 &&
                         memcmp(frame_before, initializer, sizeof(frame_before)) == 0);

    bool value = true;
    error = poison_error();
    OBSERVE(fixture,
            mc_registry_block_p_at(&fixture->captured, 1, &value, &error) == MC_REGISTRY_OK);
    OBSERVE(fixture, !value && fresh_error(error, MC_REGISTRY_OK));
    value = false;
    error = poison_error();
    OBSERVE(fixture,
            mc_registry_block_p_at(&fixture->captured, 54, &value, &error) == MC_REGISTRY_OK);
    OBSERVE(fixture, value && fresh_error(error, MC_REGISTRY_OK));
    OBSERVE(fixture, memcmp(owner_before, owner, sizeof(owner_before)) == 0 &&
                         memcmp(frame_before, initializer, sizeof(frame_before)) == 0 &&
                         memcmp(p_before, fixture->captured.identity, sizeof(p_before)) == 0);
    for (size_t i = 0; i < 4; ++i)
        OBSERVE(fixture,
                memcmp(fixture->random_bytes[i], fixture->randoms[i], sizeof(mc_java_random)) == 0);
    OBSERVE(fixture, uniquifier(owner->options.random_profile) == fixture->captured_uniquifier &&
                         mc_java_random_profile_allocation_bytes(owner->options.random_profile) ==
                             profile_bytes &&
                         owner->options.random_profile->math == math &&
                         atomic_load_explicit(&owner->options.random_profile->holder_phase,
                                              memory_order_relaxed) == holder_phase);
    /* The actual Translation constructor receives a missing stream. */
    return MC_REGISTRY_NULL_REFERENCE;
}

static int test_p_entered_failure(mc_registry_options options) {
    int result = 1;
    mc_registry_classes *owner = NULL;
    p_entered_fixture fixture = {.valid = true};
    const mc_registry_resources resources = {.context = &fixture, .open = p_missing_resource};
    options.resources = &resources;
    mc_registry_error error = poison_error();
    bool output_anchor = true;
    mc_registry_block_p_view view = {
        .classes = NULL, .identity = &output_anchor, .length = SIZE_MAX};
    CHECK(mc_registry_classes_allocate(&options, &owner, &error) == MC_REGISTRY_OK);
    fixture.owner = owner;
    CHECK(cold_except_literals(owner) && owner->blocks.p == NULL && fixture.opens == 0);
    error = poison_error();
    CHECK(mc_registry_block_p_get(owner, &view, &error) == MC_REGISTRY_NULL_REFERENCE);
    CHECK(fixture.valid && fixture.opens == 1);
    CHECK(view.classes == NULL && view.identity == &output_anchor && view.length == SIZE_MAX &&
          output_anchor);
    const char *declaring = options.side == MC_REGISTRY_CLIENT ? "nh" : "ii";
    const mc_registry_error first_failure = {.status = MC_REGISTRY_NULL_REFERENCE,
                                             .message =
                                                 "Translation Properties.load null InputStream",
                                             .initializer_pc = 3398,
                                             .block_id = -1,
                                             .constructor_class = declaring,
                                             .constructor_pc = 26,
                                             .profile_status = MC_JAVA_RANDOM_PROFILE_OK,
                                             .active_initializer_class = declaring,
                                             .active_initializer_pc = 4,
                                             .item_argument = -1};
    CHECK(same_error(error, first_failure) && same_error(owner->last_error, first_failure));
    CHECK(owner->blocks_phase == MC_REGISTRY_FAILED && owner->entered == 0 &&
          owner->initializer == NULL && owner->translation_state.phase == MC_REGISTRY_FAILED &&
          owner->translation_wrapper_state.phase == MC_REGISTRY_FAILED);
    CHECK(same_error(owner->block_failure, first_failure) &&
          same_error(owner->block_states[MC_BLOCK_UU_NA].failure, first_failure) &&
          same_error(owner->translation_state.failure, first_failure) &&
          same_error(owner->translation_wrapper_state.failure, first_failure));
    CHECK(fixture.captured.classes == owner && fixture.captured.identity == owner->blocks.p &&
          fixture.captured.length == MC_REGISTRY_BLOCK_SLOTS);
    CHECK(owner->allocation_bytes == fixture.captured_bytes);
    mc_registry_allocation *const allocations = owner->allocations;
    mc_registry_error retry_failure = first_failure;
    retry_failure.status = MC_REGISTRY_FAILED_INITIALIZATION;
    error = poison_error();
    CHECK(mc_registry_block_p_get(owner, &view, &error) == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(same_error(error, retry_failure) && same_error(owner->last_error, retry_failure));
    CHECK(view.classes == NULL && view.identity == &output_anchor && view.length == SIZE_MAX &&
          output_anchor && fixture.opens == 1);

    /* The retained array remains usable even though fresh Block active use fails. */
    bool value = true;
    error = poison_error();
    CHECK(mc_registry_block_p_at(&fixture.captured, 1, &value, &error) == MC_REGISTRY_OK);
    CHECK(!value && fresh_error(error, MC_REGISTRY_OK));
    value = false;
    error = poison_error();
    CHECK(mc_registry_block_p_at(&fixture.captured, 54, &value, &error) == MC_REGISTRY_OK);
    CHECK(value && fresh_error(error, MC_REGISTRY_OK) &&
          same_error(owner->last_error, retry_failure));
    static const int32_t bad_indices[] = {-1, (int32_t)MC_REGISTRY_BLOCK_SLOTS};
    for (size_t i = 0; i < 2; ++i) {
        value = true;
        error = poison_error();
        CHECK(mc_registry_block_p_at(&fixture.captured, bad_indices[i], &value, &error) ==
              MC_REGISTRY_INDEX_OUT_OF_RANGE);
        CHECK(value && fresh_error(error, MC_REGISTRY_INDEX_OUT_OF_RANGE) &&
              same_error(owner->last_error, error));
        CHECK(same_error(owner->block_failure, first_failure) &&
              same_error(owner->block_states[MC_BLOCK_UU_NA].failure, first_failure) &&
              same_error(owner->translation_state.failure, first_failure) &&
              same_error(owner->translation_wrapper_state.failure, first_failure));
        error = poison_error();
        CHECK(mc_registry_block_p_get(owner, &view, &error) == MC_REGISTRY_FAILED_INITIALIZATION);
        CHECK(same_error(error, retry_failure) && same_error(owner->last_error, retry_failure));
        CHECK(view.classes == NULL && view.identity == &output_anchor && view.length == SIZE_MAX &&
              output_anchor && fixture.opens == 1);
    }
    CHECK(owner->blocks_phase == MC_REGISTRY_FAILED && owner->entered == 0 &&
          owner->initializer == NULL && owner->allocation_bytes == fixture.captured_bytes &&
          owner->allocations == allocations && owner->blocks.p == fixture.captured.identity);
    CHECK(uniquifier(options.random_profile) == fixture.captured_uniquifier);
    for (size_t i = 0; i < 4; ++i)
        CHECK(memcmp(fixture.random_bytes[i], fixture.randoms[i], sizeof(mc_java_random)) == 0);
    result = 0;
cleanup:
    if (owner != NULL && mc_registry_classes_free(owner, NULL) != MC_REGISTRY_OK)
        result = 1;
    return result;
}

int main(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&materials));
    const size_t profile_bytes = mc_java_random_profile_allocation_bytes(profile);
    for (int side = (int)MC_REGISTRY_CLIENT; side <= (int)MC_REGISTRY_SERVER; ++side) {
        const mc_registry_options options = {.side = (mc_registry_side)side,
                                             .random_profile = profile,
                                             .materials = materials,
                                             .max_allocation_bytes = SIZE_MAX};
        CHECK(test_cold(options) == 0);
        CHECK(test_literal_budgets(options) == 0);
        CHECK(test_entered(options) == 0);
        CHECK(test_fresh_failure(options) == 0);
        CHECK(test_p_entered_failure(options) == 0);
    }
    CHECK(profile->math == NULL);
    CHECK(atomic_load_explicit(&profile->holder_phase, memory_order_relaxed) ==
          MC_JAVA_RANDOM_HOLDER_COLD);
    CHECK(mc_java_random_profile_allocation_bytes(profile) == profile_bytes);
    printf("PASS: %u Registry runtime literal checks\n", checks);
    result = 0;
cleanup:
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
