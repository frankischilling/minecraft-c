#include "mc/block/registry/falling.h"
#include "mc/block/registry/runtime.h"
#include "mc/resources/translation/translation.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/core/java/random_profile_internal.h"

#include <stdio.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
static unsigned int checks;
static bool retirement_failed;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)
#define OBSERVE(f, condition)                                                                      \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: callback failed: %s\n", __FILE__, __LINE__, #condition);       \
            (f)->bad = true;                                                                       \
        }                                                                                          \
    } while (0)

/* Actual finite class-loader byte streams, shared with the established
 * asset-independent Registry fixtures. Empty stats/achievement streams are
 * actual EOF, not callback-success substitutes for parsing or class bodies. */
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct stream {
    const uint8_t *bytes;
    size_t length, offset;
    bool closed;
} stream;

typedef struct fixture {
    mc_registry_classes *owner;
    mc_registry_resources resources;
    stream streams[3];
    size_t opens, closes, busy_checks;
    bool missing, bad, retained;
} fixture;

static uint64_t uniquifier(const mc_java_random_profile *profile) {
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static uint64_t advanced(uint64_t value, size_t count) {
    while (count-- != 0)
        value *= UINT64_C(1181783497276652981);
    return value;
}

static mc_registry_error poison_error(void) {
    return (mc_registry_error){.status = MC_REGISTRY_DUPLICATE_STATISTIC,
                               .message = "old message",
                               .initializer_pc = 71,
                               .block_id = 314,
                               .constructor_class = "old constructor",
                               .constructor_pc = 42,
                               .profile_status = MC_JAVA_RANDOM_PROFILE_HOLDER_FAILED,
                               .active_initializer_class = "old initializer",
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

static bool fresh(mc_registry_error error, mc_registry_status status) {
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

static bool same_text(const char *a, const char *b) {
    return a == b || (a != NULL && b != NULL && strcmp(a, b) == 0);
}

static bool same_error(mc_registry_error a, mc_registry_error b) {
    return a.status == b.status && same_text(a.message, b.message) &&
           a.initializer_pc == b.initializer_pc && a.block_id == b.block_id &&
           same_text(a.constructor_class, b.constructor_class) &&
           a.constructor_pc == b.constructor_pc && a.profile_status == b.profile_status &&
           same_text(a.active_initializer_class, b.active_initializer_class) &&
           a.active_initializer_pc == b.active_initializer_pc &&
           a.item_argument == b.item_argument && a.format_error.status == b.format_error.status &&
           a.format_error.offset == b.format_error.offset &&
           a.format_error.length == b.format_error.length &&
           a.format_error.value == b.format_error.value &&
           a.format_error.flags == b.format_error.flags &&
           a.format_error.conversion == b.format_error.conversion &&
           a.format_error.date_time == b.format_error.date_time;
}

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count) {
    stream *input = context;
    if (input == NULL || out == NULL || count == NULL || capacity == 0 || input->closed ||
        input->offset > input->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = input->length - input->offset;
    if (take > 17u)
        take = 17u;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, input->bytes + input->offset, take);
    input->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static void entered_checks(fixture *f) {
    mc_registry_classes *owner = f->owner;
    OBSERVE(f, owner != NULL && owner->entered != 0 && owner->initializer != NULL);
    if (owner == NULL || owner->initializer == NULL)
        return;
    /* Entered is caused only by the genuine cold flag -> superclass ->
     * Translation/achievement path, never a manufactured test assignment. */
    OBSERVE(f, owner->blocks_phase == MC_REGISTRY_INITIALIZING &&
                   owner->block_states[MC_BLOCK_GK_EI].phase == MC_REGISTRY_INITIALIZING &&
                   owner->block_states[MC_BLOCK_GK_EI].pc == -1 && !owner->gk_a);
    unsigned char before[sizeof(*owner)], frame_before[sizeof(*owner->initializer)];
    memcpy(before, owner, sizeof(before));
    memcpy(frame_before, owner->initializer, sizeof(frame_before));
    const uint64_t random_before = uniquifier(owner->options.random_profile);
    const size_t profile_bytes =
        mc_java_random_profile_allocation_bytes(owner->options.random_profile);
    bool value = true;
    mc_registry_error error = poison_error();
    OBSERVE(f, mc_registry_falling_flag_get(owner, &value, &error) == MC_REGISTRY_BUSY);
    OBSERVE(f, value && fresh(error, MC_REGISTRY_BUSY));
    value = false;
    error = poison_error();
    OBSERVE(f, mc_registry_falling_flag_get(owner, &value, &error) == MC_REGISTRY_BUSY);
    OBSERVE(f, !value && fresh(error, MC_REGISTRY_BUSY));
    error = poison_error();
    OBSERVE(f, mc_registry_falling_flag_set(owner, true, &error) == MC_REGISTRY_BUSY);
    OBSERVE(f, fresh(error, MC_REGISTRY_BUSY));
    error = poison_error();
    OBSERVE(f, mc_registry_falling_flag_set(owner, false, &error) == MC_REGISTRY_BUSY);
    OBSERVE(f, fresh(error, MC_REGISTRY_BUSY));
    OBSERVE(f, mc_registry_falling_flag_get(owner, &value, NULL) == MC_REGISTRY_BUSY && !value);
    OBSERVE(f, mc_registry_falling_flag_set(owner, true, NULL) == MC_REGISTRY_BUSY);
    f->busy_checks += 6;
    OBSERVE(f, memcmp(before, owner, sizeof(before)) == 0 &&
                   memcmp(frame_before, owner->initializer, sizeof(frame_before)) == 0);
    OBSERVE(f, uniquifier(owner->options.random_profile) == random_before &&
                   mc_java_random_profile_allocation_bytes(owner->options.random_profile) ==
                       profile_bytes);
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out) {
    fixture *f = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    if (name == NULL || out == NULL || f->opens >= COUNT(names) ||
        strcmp(name, names[f->opens]) != 0) {
        f->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    entered_checks(f);
    const size_t index = f->opens++;
    if (f->missing)
        return MC_REGISTRY_NULL_REFERENCE; /* Actual absent stream reaches Properties.load. */
    *out = (mc_java_properties_input){&f->streams[index], read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    fixture *f = context;
    if (input == NULL || input->context != &f->streams[2] || input->read != read_bytes) {
        f->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    f->streams[2].closed = true;
    ++f->closes;
    return MC_REGISTRY_OK;
}

static bool ready_resource(void *context, const mc_java_properties_input *input) {
    fixture *f = context;
    if (input == NULL || input->read != read_bytes)
        f->bad = true;
    return false;
}

static mc_registry_status allocate_fixture(fixture *f, mc_registry_options options) {
    f->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
    f->streams[1] = f->streams[2] = (stream){.bytes = language};
    f->resources = (mc_registry_resources){f, open_resource, close_resource, ready_resource};
    options.resources = &f->resources;
    return mc_registry_classes_allocate(&options, &f->owner, NULL);
}

static bool retire(fixture *f) {
    if (f->retained) {
        if (mc_registry_classes_release(f->owner, NULL) != MC_REGISTRY_OK)
            goto refused;
        f->retained = false;
    }
    if (mc_registry_classes_free(f->owner, NULL) != MC_REGISTRY_OK)
        goto refused;
    f->owner = NULL;
    return true;
refused:
    retirement_failed = true;
    fputs("Falling flag fixture retirement refused; dependencies retained\n", stderr);
    return false;
}

static bool cold(const fixture *f) {
    return f->owner != NULL && f->owner->blocks_phase == MC_REGISTRY_COLD &&
           f->owner->block_states[MC_BLOCK_GK_EI].phase == MC_REGISTRY_COLD &&
           f->owner->block_states[MC_BLOCK_GK_EI].pc == -1 && !f->owner->gk_a &&
           f->owner->blocks.m == NULL && f->owner->entered == 0 && f->owner->initializer == NULL &&
           f->owner->runtime_borrows == 0 && f->opens == 0;
}

/* Two ordinary callers share only the real Registry owner, never a local flag. */
static mc_registry_status first_caller_write(mc_registry_classes *owner, bool value,
                                             mc_registry_error *error) {
    return mc_registry_falling_flag_set(owner, value, error);
}

static mc_registry_status second_caller_read(mc_registry_classes *owner, bool *out,
                                             mc_registry_error *error) {
    return mc_registry_falling_flag_get(owner, out, error);
}

static int arguments(mc_registry_options options) {
    int result = 1;
    fixture f = {0};
    mc_registry_error error = poison_error();
    bool value = true;
    CHECK(allocate_fixture(&f, options) == MC_REGISTRY_OK && cold(&f));
    unsigned char before[sizeof(*f.owner)];
    memcpy(before, f.owner, sizeof(before));
    const uint64_t random_before = uniquifier(options.random_profile);
    const size_t profile_bytes = mc_java_random_profile_allocation_bytes(options.random_profile);
    CHECK(mc_registry_falling_flag_get(NULL, &value, &error) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(value && fresh(error, MC_REGISTRY_INVALID_ARGUMENT));
    error = poison_error();
    CHECK(mc_registry_falling_flag_set(NULL, true, &error) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(fresh(error, MC_REGISTRY_INVALID_ARGUMENT));
    error = poison_error();
    CHECK(mc_registry_falling_flag_get(f.owner, NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(fresh(error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_falling_flag_get(NULL, &value, NULL) == MC_REGISTRY_INVALID_ARGUMENT &&
          value);
    CHECK(mc_registry_falling_flag_set(NULL, false, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(memcmp(before, f.owner, sizeof(before)) == 0 && cold(&f));
    CHECK(uniquifier(options.random_profile) == random_before &&
          mc_java_random_profile_allocation_bytes(options.random_profile) == profile_bytes);
    result = 0;
cleanup:
    if (!retire(&f))
        result = 1;
    return result;
}

static int cold_success(mc_registry_options options, bool first_store) {
    int result = 1;
    fixture f = {0};
    mc_registry_error error = poison_error();
    bool value = true;
    const uint64_t random_before = uniquifier(options.random_profile);
    CHECK(allocate_fixture(&f, options) == MC_REGISTRY_OK && cold(&f));
    CHECK((first_store ? mc_registry_falling_flag_set(f.owner, true, &error)
                       : mc_registry_falling_flag_get(f.owner, &value, &error)) == MC_REGISTRY_OK);
    CHECK(fresh(error, MC_REGISTRY_OK) && !f.bad && f.opens == 3 && f.closes == 1 &&
          f.busy_checks == 18 && (first_store ? value : !value));
    CHECK(f.owner->blocks_phase == MC_REGISTRY_READY &&
          f.owner->block_states[MC_BLOCK_GK_EI].phase == MC_REGISTRY_READY &&
          f.owner->block_states[MC_BLOCK_GK_EI].pc == 4 &&
          f.owner->block_states[MC_BLOCK_UU_NA].phase == MC_REGISTRY_READY &&
          f.owner->entered == 0 && f.owner->initializer == NULL && f.owner->runtime_borrows == 0 &&
          f.owner->gk_a == first_store);
    CHECK(uniquifier(options.random_profile) == advanced(random_before, 5));
    CHECK(mc_registry_classes_retain(f.owner, options.side, options.random_profile, &error) ==
          MC_REGISTRY_OK);
    f.retained = true;
    CHECK(f.owner->runtime_borrows == 1 &&
          mc_registry_classes_free(f.owner, &error) == MC_REGISTRY_BUSY);
    const size_t bytes = f.owner->allocation_bytes;
    mc_registry_allocation *const allocations = f.owner->allocations;
    const uint64_t ready_random = uniquifier(options.random_profile);
    const size_t profile_bytes = mc_java_random_profile_allocation_bytes(options.random_profile);
    const mc_registry_error ambient = f.owner->last_error;
    error = poison_error();
    CHECK(second_caller_read(f.owner, &value, &error) == MC_REGISTRY_OK && value == first_store &&
          fresh(error, MC_REGISTRY_OK));
    error = poison_error();
    CHECK(first_caller_write(f.owner, true, &error) == MC_REGISTRY_OK &&
          fresh(error, MC_REGISTRY_OK));
    value = false;
    CHECK(second_caller_read(f.owner, &value, NULL) == MC_REGISTRY_OK && value);
    CHECK(mc_registry_falling_flag_get(f.owner, &value, NULL) == MC_REGISTRY_OK && value);
    CHECK(first_caller_write(f.owner, false, NULL) == MC_REGISTRY_OK);
    value = true;
    error = poison_error();
    CHECK(second_caller_read(f.owner, &value, &error) == MC_REGISTRY_OK && !value &&
          fresh(error, MC_REGISTRY_OK));
    CHECK(mc_registry_falling_flag_set(f.owner, false, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_falling_flag_set(f.owner, true, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_falling_flag_get(f.owner, &value, NULL) == MC_REGISTRY_OK && value);
    CHECK(f.owner->gk_a && f.owner->runtime_borrows == 1 && f.owner->entered == 0 &&
          f.owner->initializer == NULL && f.owner->allocation_bytes == bytes &&
          f.owner->allocations == allocations && same_error(f.owner->last_error, ambient));
    CHECK(uniquifier(options.random_profile) == ready_random &&
          mc_java_random_profile_allocation_bytes(options.random_profile) == profile_bytes &&
          f.opens == 3 && f.closes == 1 && !f.bad);
    CHECK(mc_registry_classes_release(f.owner, &error) == MC_REGISTRY_OK);
    f.retained = false;
    CHECK(f.owner->runtime_borrows == 0);
    result = 0;
cleanup:
    if (!retire(&f))
        result = 1;
    return result;
}

static int cold_failure(mc_registry_options options, bool first_store) {
    int result = 1;
    fixture f = {.missing = true};
    mc_registry_error error = poison_error();
    bool value = true;
    CHECK(allocate_fixture(&f, options) == MC_REGISTRY_OK && cold(&f));
    CHECK((first_store ? mc_registry_falling_flag_set(f.owner, true, &error)
                       : mc_registry_falling_flag_get(f.owner, &value, &error)) ==
          MC_REGISTRY_NULL_REFERENCE);
    CHECK(value && !f.bad && f.opens == 1 && f.closes == 0 && f.busy_checks == 6);
    const char *declaring = options.side == MC_REGISTRY_CLIENT ? "nh" : "ii";
    const mc_registry_error first = {.status = MC_REGISTRY_NULL_REFERENCE,
                                     .message = "Translation Properties.load null InputStream",
                                     .initializer_pc = 3398,
                                     .block_id = -1,
                                     .constructor_class = declaring,
                                     .constructor_pc = 26,
                                     .profile_status = MC_JAVA_RANDOM_PROFILE_OK,
                                     .active_initializer_class = declaring,
                                     .active_initializer_pc = 4,
                                     .item_argument = -1};
    CHECK(same_error(error, first) && same_error(f.owner->last_error, first));
    CHECK(f.owner->blocks_phase == MC_REGISTRY_FAILED &&
          f.owner->block_states[MC_BLOCK_GK_EI].phase == MC_REGISTRY_FAILED &&
          f.owner->block_states[MC_BLOCK_GK_EI].pc == -1 && !f.owner->gk_a &&
          f.owner->entered == 0 && f.owner->initializer == NULL && f.owner->runtime_borrows == 0);
    CHECK(same_error(f.owner->block_states[MC_BLOCK_GK_EI].failure, first) &&
          same_error(f.owner->block_failure, first) &&
          same_error(f.owner->block_states[MC_BLOCK_UU_NA].failure, first) &&
          same_error(f.owner->translation_state.failure, first) &&
          same_error(f.owner->translation_wrapper_state.failure, first));
    const size_t bytes = f.owner->allocation_bytes;
    mc_registry_allocation *const allocations = f.owner->allocations;
    const uint64_t failed_random = uniquifier(options.random_profile);
    const size_t profile_bytes = mc_java_random_profile_allocation_bytes(options.random_profile);
    error = poison_error();
    CHECK(mc_registry_falling_flag_get(f.owner, NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          fresh(error, MC_REGISTRY_INVALID_ARGUMENT) && same_error(f.owner->last_error, first));
    /* A different actual active-use entry sets its own cached-return status.
     * The declaring-class retry below must recover its complete first cause,
     * rather than copy an ambient/native argument diagnostic as the cause. */
    const mc_block *const block_anchor = f.owner->blocks.m[1];
    const mc_block *block = block_anchor;
    mc_registry_error block_retry = first;
    block_retry.status = MC_REGISTRY_FAILED_INITIALIZATION;
    error = poison_error();
    CHECK(mc_registry_block_get(f.owner, -1, &block, &error) == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(block == block_anchor && block_anchor != NULL && same_error(error, block_retry) &&
          same_error(f.owner->last_error, block_retry));
    error = poison_error();
    CHECK(mc_registry_falling_flag_get(f.owner, &value, &error) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(value && same_error(error, first) && same_error(f.owner->last_error, first));
    error = poison_error();
    CHECK(mc_registry_falling_flag_set(f.owner, true, &error) == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(same_error(error, first) && !f.owner->gk_a);
    CHECK(mc_registry_falling_flag_get(f.owner, &value, NULL) ==
              MC_REGISTRY_FAILED_INITIALIZATION &&
          value);
    CHECK(mc_registry_falling_flag_set(f.owner, false, NULL) == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(same_error(f.owner->block_states[MC_BLOCK_GK_EI].failure, first) &&
          same_error(f.owner->block_failure, first) && f.owner->allocation_bytes == bytes &&
          f.owner->allocations == allocations && f.owner->runtime_borrows == 0 &&
          f.owner->entered == 0 && f.owner->initializer == NULL && !f.owner->gk_a && f.opens == 1 &&
          f.closes == 0 && !f.bad);
    CHECK(uniquifier(options.random_profile) == failed_random &&
          mc_java_random_profile_allocation_bytes(options.random_profile) == profile_bytes);
    result = 0;
cleanup:
    if (!retire(&f))
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
        CHECK(arguments(options) == 0);
        CHECK(cold_success(options, false) == 0);
        CHECK(cold_success(options, true) == 0);
        CHECK(cold_failure(options, false) == 0);
        CHECK(cold_failure(options, true) == 0);
    }
    CHECK(profile->math == NULL &&
          atomic_load_explicit(&profile->holder_phase, memory_order_relaxed) ==
              MC_JAVA_RANDOM_HOLDER_COLD);
    CHECK(mc_java_random_profile_allocation_bytes(profile) == profile_bytes);
    printf("PASS: %u Registry falling flag checks\n", checks);
    result = 0;
cleanup:
    if (!retirement_failed) {
        mc_materials_destroy(materials);
        mc_java_random_profile_free(profile);
    }
    return result;
}
