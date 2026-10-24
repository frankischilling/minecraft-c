#include "mc/block/registry/runtime.h"
#include "mc/item/registry/runtime.h"
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

/* Real byte streams, as in existing Registry/World fixtures. No successful
 * gameplay backend or READY/entered/profile/owner-count assignments exist. */
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
    const mc_registry_item_array *callback_capture;
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
    /* A labeled native borrow of the SAME real bootstrap array, not a public
     * successful c_get during entered initialization and not a detached table. */
    f->callback_capture = owner->items.c;
    OBSERVE(f, f->callback_capture != NULL && f->callback_capture->length == 32000);
    unsigned char before[sizeof(*owner)], frame_before[sizeof(*owner->initializer)];
    memcpy(before, owner, sizeof(before));
    memcpy(frame_before, owner->initializer, sizeof(frame_before));
    const uint64_t random_before = uniquifier(owner->options.random_profile);
    const size_t profile_bytes =
        mc_java_random_profile_allocation_bytes(owner->options.random_profile);
    mc_item output_anchor = {0};
    mc_registry_item_array array_anchor = {0};
    const mc_item *item = &output_anchor;
    const mc_registry_item_array *array = &array_anchor;
    mc_registry_error error = poison_error();
    OBSERVE(f, mc_registry_item_static_get(owner, "d", &item, &error) == MC_REGISTRY_BUSY);
    OBSERVE(f, item == &output_anchor && fresh(error, MC_REGISTRY_BUSY));
    error = poison_error();
    OBSERVE(f, mc_registry_item_c_get(owner, &array, &error) == MC_REGISTRY_BUSY);
    OBSERVE(f, array == &array_anchor && fresh(error, MC_REGISTRY_BUSY));
    error = poison_error();
    OBSERVE(f, mc_registry_item_c_at(owner, f->callback_capture, 0, &item, &error) ==
                   MC_REGISTRY_BUSY);
    OBSERVE(f, item == &output_anchor && fresh(error, MC_REGISTRY_BUSY));
    f->busy_checks += 3;
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
        return MC_REGISTRY_NULL_REFERENCE; /* Actual absent stream, no fake parse success. */
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
    fputs("Item runtime fixture retirement refused; dependencies retained\n", stderr);
    return false;
}

static int cold_failure(mc_registry_options options, bool static_route) {
    int result = 1;
    fixture f = {.missing = true};
    mc_item item_anchor = {0};
    mc_registry_item_array array_anchor = {0};
    const mc_item *item = &item_anchor;
    const mc_registry_item_array *array = &array_anchor;
    mc_registry_error error = poison_error();
    const uint64_t before_random = uniquifier(options.random_profile);
    CHECK(allocate_fixture(&f, options) == MC_REGISTRY_OK);
    CHECK(f.owner->options.side == options.side &&
          f.owner->options.random_profile == options.random_profile &&
          f.owner->item_states[MC_ITEM_BASE].phase == MC_REGISTRY_COLD && f.owner->items.c == NULL);
    /* A missing selector still reaches actual Item initialization first. */
    CHECK((static_route
               ? mc_registry_item_static_get(f.owner, "missing", &item, &error)
               : mc_registry_item_c_get(f.owner, &array, &error)) == MC_REGISTRY_NULL_REFERENCE);
    CHECK(item == &item_anchor && array == &array_anchor && !f.bad && f.opens == 1 &&
          f.busy_checks == 3);
    CHECK(error.status == MC_REGISTRY_NULL_REFERENCE && error.initializer_pc >= 0 &&
          error.constructor_class != NULL && error.constructor_pc >= 0 &&
          error.active_initializer_class != NULL && error.active_initializer_pc >= 0);
    CHECK(f.owner->item_states[MC_ITEM_BASE].phase == MC_REGISTRY_FAILED &&
          f.owner->blocks_phase == MC_REGISTRY_FAILED && f.owner->entered == 0 &&
          f.owner->initializer == NULL);
    const mc_registry_error first = error;
    CHECK(same_error(first, f.owner->item_states[MC_ITEM_BASE].failure) &&
          same_error(first, f.owner->last_error));
    CHECK(uniquifier(options.random_profile) == advanced(before_random, 5) &&
          options.random_profile->math == NULL);
    CHECK(f.callback_capture != NULL && f.callback_capture == f.owner->items.c);
    const size_t bytes = f.owner->allocation_bytes;
    mc_registry_allocation *const allocations = f.owner->allocations;
    const uint64_t after_random = uniquifier(options.random_profile);
    /* This actual captured prefix remains readable without another active use,
     * even though a public getstatic must refuse the now erroneous class. */
    error = poison_error();
    CHECK(mc_registry_item_c_at(f.owner, f.callback_capture, 0, &item, &error) == MC_REGISTRY_OK);
    CHECK(item == NULL && fresh(error, MC_REGISTRY_OK));
    item = &item_anchor;
    error = poison_error();
    CHECK(mc_registry_item_c_at(f.owner, f.callback_capture, -1, &item, &error) ==
          MC_REGISTRY_INDEX_OUT_OF_RANGE);
    CHECK(item == &item_anchor && fresh(error, MC_REGISTRY_INDEX_OUT_OF_RANGE) &&
          same_error(error, f.owner->last_error));
    error = poison_error();
    CHECK(mc_registry_item_c_at(f.owner, NULL, INT32_MIN, &item, &error) ==
          MC_REGISTRY_NULL_REFERENCE);
    CHECK(item == &item_anchor && fresh(error, MC_REGISTRY_NULL_REFERENCE));
    mc_registry_error retry = first;
    retry.status = MC_REGISTRY_FAILED_INITIALIZATION;
    error = poison_error();
    CHECK(mc_registry_item_c_get(f.owner, &array, &error) == MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(array == &array_anchor && same_error(error, retry) &&
          same_error(f.owner->last_error, retry));
    error = poison_error();
    CHECK(mc_registry_item_static_get(f.owner, "d", &item, &error) ==
          MC_REGISTRY_FAILED_INITIALIZATION);
    CHECK(item == &item_anchor && same_error(error, retry));
    CHECK(same_error(f.owner->item_states[MC_ITEM_BASE].failure, first) && f.opens == 1 &&
          f.owner->allocation_bytes == bytes && f.owner->allocations == allocations &&
          uniquifier(options.random_profile) == after_random);
    result = 0;
cleanup:
    if (!retire(&f))
        result = 1;
    return result;
}

static int metadata_budget_and_arguments(mc_registry_options options) {
    int result = 1;
    fixture f = {0};
    mc_item item_anchor = {0};
    mc_registry_item_array array_anchor = {0};
    const mc_item *item = &item_anchor;
    const mc_registry_item_array *array = &array_anchor;
    mc_registry_error error = poison_error();
    const uint64_t before_random = uniquifier(options.random_profile);
    options.max_allocation_bytes =
        sizeof(mc_registry_classes); /* Real creation ceiling, not a changed ledger. */
    CHECK(allocate_fixture(&f, options) == MC_REGISTRY_OK);
    CHECK(mc_registry_item_static_get(NULL, "d", &item, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          item == &item_anchor && fresh(error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_item_c_get(NULL, &array, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          array == &array_anchor && fresh(error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_item_c_at(NULL, NULL, -1, &item, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          item == &item_anchor && fresh(error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_item_static_get(f.owner, NULL, &item, &error) ==
              MC_REGISTRY_INVALID_ARGUMENT &&
          item == &item_anchor && fresh(error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_item_static_get(f.owner, "d", NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          fresh(error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_item_c_get(f.owner, NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          fresh(error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(mc_registry_item_c_at(f.owner, NULL, 0, NULL, &error) == MC_REGISTRY_INVALID_ARGUMENT &&
          fresh(error, MC_REGISTRY_INVALID_ARGUMENT));
    CHECK(f.owner->item_states[MC_ITEM_BASE].phase == MC_REGISTRY_COLD &&
          f.owner->blocks_phase == MC_REGISTRY_COLD);
    CHECK(mc_registry_item_c_at(f.owner, NULL, -1, &item, &error) == MC_REGISTRY_NULL_REFERENCE &&
          item == &item_anchor && fresh(error, MC_REGISTRY_NULL_REFERENCE));
    CHECK(f.owner->item_states[MC_ITEM_BASE].phase == MC_REGISTRY_COLD &&
          uniquifier(options.random_profile) == before_random);
    error = poison_error();
    CHECK(mc_registry_item_c_get(f.owner, &array, &error) == MC_REGISTRY_LIMIT);
    CHECK(array == &array_anchor && f.owner->items.b == NULL && f.owner->items.c == NULL &&
          f.opens == 0 && f.owner->allocation_bytes == sizeof(mc_registry_classes));
    CHECK(error.status == MC_REGISTRY_LIMIT && error.initializer_pc == -1 &&
          error.active_initializer_pc == 0 &&
          same_text(error.active_initializer_class,
                    options.side == MC_REGISTRY_CLIENT ? "gm" : "ej"));
    CHECK(f.owner->item_states[MC_ITEM_BASE].phase == MC_REGISTRY_FAILED &&
          f.owner->item_states[MC_ITEM_BASE].pc == 0 && f.owner->blocks_phase == MC_REGISTRY_COLD);
    const mc_registry_error first = error;
    CHECK(same_error(first, f.owner->item_states[MC_ITEM_BASE].failure));
    CHECK(mc_registry_item_c_at(f.owner, NULL, INT32_MAX, &item, &error) ==
              MC_REGISTRY_NULL_REFERENCE &&
          fresh(error, MC_REGISTRY_NULL_REFERENCE));
    mc_registry_error retry = first;
    retry.status = MC_REGISTRY_FAILED_INITIALIZATION;
    CHECK(mc_registry_item_static_get(f.owner, "d", &item, &error) ==
              MC_REGISTRY_FAILED_INITIALIZATION &&
          item == &item_anchor && same_error(error, retry));
    CHECK(uniquifier(options.random_profile) == before_random && f.owner->entered == 0 &&
          f.owner->initializer == NULL);
    result = 0;
cleanup:
    if (!retire(&f))
        result = 1;
    return result;
}

static int ready_capture(mc_registry_options options) {
    int result = 1;
    fixture f = {0};
    const mc_registry_item_array *captured = NULL;
    const mc_item *iron = NULL, *first = NULL, *last = NULL;
    mc_registry_error error = poison_error();
    const uint64_t before_random = uniquifier(options.random_profile);
    CHECK(allocate_fixture(&f, options) == MC_REGISTRY_OK);
    /* Genuine supported Block-first order supplies the initialized Item class.
     * No claim that every public bootstrap entry/order succeeds is made. */
    CHECK(mc_registry_classes_initialize_blocks(f.owner, &error) == MC_REGISTRY_OK);
    CHECK(!f.bad && f.opens == 3 && f.closes == 1 && f.busy_checks == 9 &&
          f.owner->blocks_phase == MC_REGISTRY_READY &&
          f.owner->item_states[MC_ITEM_BASE].phase == MC_REGISTRY_READY);
    CHECK(uniquifier(options.random_profile) == advanced(before_random, 5));
    CHECK(mc_registry_classes_retain(f.owner, options.side, options.random_profile, &error) ==
          MC_REGISTRY_OK);
    f.retained = true;
    CHECK(mc_registry_classes_free(f.owner, &error) == MC_REGISTRY_BUSY);
    CHECK(mc_registry_item_c_get(f.owner, &captured, &error) == MC_REGISTRY_OK &&
          captured == f.owner->items.c && captured != NULL && captured->length == 32000 &&
          fresh(error, MC_REGISTRY_OK));
    CHECK(mc_registry_item_static_get(f.owner, "d", &first, &error) == MC_REGISTRY_OK &&
          first != NULL && first->fields.bf == 256 && fresh(error, MC_REGISTRY_OK));
    CHECK(mc_registry_item_static_get(f.owner, "m", &iron, &error) == MC_REGISTRY_OK &&
          iron != NULL && iron->fields.bf == 265);
    CHECK(mc_registry_item_static_get(f.owner, "be", &last, &error) == MC_REGISTRY_OK &&
          last != NULL && last->fields.bf == 2257);
    CHECK(first->classes == f.owner && iron->classes == f.owner && last->classes == f.owner &&
          first != iron && last != first);
    const size_t bytes = f.owner->allocation_bytes;
    const uint64_t ready_random = uniquifier(options.random_profile);
    const mc_item *item = last;
    CHECK(mc_registry_item_c_at(f.owner, captured, 256, &item, &error) == MC_REGISTRY_OK &&
          item == first && fresh(error, MC_REGISTRY_OK));
    CHECK(mc_registry_item_c_at(f.owner, captured, 0, &item, &error) == MC_REGISTRY_OK &&
          item == NULL && fresh(error, MC_REGISTRY_OK));
    CHECK(mc_registry_item_c_at(f.owner, captured, 31999, &item, &error) == MC_REGISTRY_OK &&
          item == NULL);
    /* Labeled source-cell inputs: restore each actual cell BEFORE assertions.
     * The held captured allocation remains real and is never cloned/adopted. */
    mc_registry_item_array *const source_c = f.owner->items.c;
    const mc_registry_item_array *null_capture = captured;
    f.owner->items.c = NULL;
    const mc_registry_status null_get = mc_registry_item_c_get(f.owner, &null_capture, &error);
    const mc_registry_error null_get_error = error;
    item = last;
    const mc_registry_status saved_read =
        mc_registry_item_c_at(f.owner, captured, 256, &item, &error);
    f.owner->items.c = source_c;
    CHECK(null_get == MC_REGISTRY_OK && null_capture == NULL &&
          fresh(null_get_error, MC_REGISTRY_OK));
    CHECK(saved_read == MC_REGISTRY_OK && item == first && fresh(error, MC_REGISTRY_OK));
    item = last;
    error = poison_error();
    CHECK(mc_registry_item_c_at(f.owner, null_capture, INT32_MIN, &item, &error) ==
              MC_REGISTRY_NULL_REFERENCE &&
          item == last && fresh(error, MC_REGISTRY_NULL_REFERENCE) &&
          same_error(error, f.owner->last_error));
    mc_item *const source_first = f.owner->items.named[0];
    f.owner->items.named[0] = NULL;
    const mc_registry_status null_static = mc_registry_item_static_get(f.owner, "d", &item, &error);
    f.owner->items.named[0] = source_first;
    CHECK(null_static == MC_REGISTRY_OK && item == NULL && fresh(error, MC_REGISTRY_OK));
    CHECK(mc_registry_item_c_at(f.owner, captured, 256, &item, NULL) == MC_REGISTRY_OK &&
          item == first);
    static const int32_t invalid[] = {-1, INT32_MIN, 32000, INT32_MAX};
    for (size_t index = 0; index < COUNT(invalid); ++index) {
        item = last;
        error = poison_error();
        CHECK(mc_registry_item_c_at(f.owner, captured, invalid[index], &item, &error) ==
              MC_REGISTRY_INDEX_OUT_OF_RANGE);
        CHECK(item == last && fresh(error, MC_REGISTRY_INDEX_OUT_OF_RANGE) &&
              same_error(error, f.owner->last_error));
    }
    item = last;
    error = poison_error();
    CHECK(mc_registry_item_static_get(f.owner, "Be", &item, &error) ==
              MC_REGISTRY_INVALID_ARGUMENT &&
          item == last && fresh(error, MC_REGISTRY_INVALID_ARGUMENT) &&
          same_error(error, f.owner->last_error));
    CHECK(mc_registry_item_static_get(f.owner, "d", &item, &error) == MC_REGISTRY_OK &&
          item == first && fresh(error, MC_REGISTRY_OK));
    CHECK(mc_registry_item_c_get(f.owner, &captured, NULL) == MC_REGISTRY_OK &&
          captured == source_c);
    CHECK(f.owner->items.c == source_c && f.owner->items.named[0] == source_first &&
          f.owner->allocation_bytes == bytes &&
          uniquifier(options.random_profile) == ready_random && f.owner->entered == 0 &&
          f.owner->initializer == NULL && f.opens == 3 && !f.bad);
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
        CHECK(metadata_budget_and_arguments(options) == 0);
        CHECK(cold_failure(options, false) == 0);
        CHECK(cold_failure(options, true) == 0);
        CHECK(ready_capture(options) == 0);
    }
    CHECK(profile->math == NULL &&
          atomic_load_explicit(&profile->holder_phase, memory_order_relaxed) ==
              MC_JAVA_RANDOM_HOLDER_COLD);
    CHECK(mc_java_random_profile_allocation_bytes(profile) == profile_bytes);
    printf("PASS: %u Item Registry runtime checks\n", checks);
    result = 0;
cleanup:
    if (!retirement_failed) {
        mc_materials_destroy(materials);
        mc_java_random_profile_free(profile);
    }
    return result;
}
