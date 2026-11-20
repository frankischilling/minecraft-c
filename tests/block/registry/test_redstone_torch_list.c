#include "block/registry/managed_internal.h"
#include "block/registry/registry_internal.h"
#include "mc/core/java/java.h"

#include <limits.h>
#include <stdio.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef mc_registry_redstone_torch_list_ref list_ref;
typedef mc_registry_redstone_torch_record record;
typedef mc_registry_redstone_torch_record_view record_view;
typedef mc_registry_redstone_torch_list_view list_view;
typedef mc_registry_redstone_torch_snapshot snapshot;
typedef mc_registry_redstone_torch_error list_error;

static bool same_record(const record_view *a, const record_view *b) {
    return a->identity == b->identity && a->x == b->x && a->y == b->y && a->z == b->z &&
           a->time == b->time && a->constructed == b->constructed && a->is_null == b->is_null;
}

static bool same_list(const list_view *a, const list_view *b) {
    return a->identity == b->identity && a->array_identity == b->array_identity &&
           a->allocated_records == b->allocated_records && a->backing == b->backing &&
           a->size == b->size && a->capacity == b->capacity && a->mod_count == b->mod_count &&
           a->unused_slots_null == b->unused_slots_null;
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

static int factory_and_null_prefix(mc_registry_classes *owner) {
    int result = 1;
    const size_t maximum = owner->options.max_allocation_bytes;
    mc_registry_redstone_torch_list *first = NULL, *second = NULL;
    record *value = NULL, *removed = NULL;
    snapshot *empty = NULL;
    mc_registry_redstone_set *legacy = NULL;
    mc_registry_redstone_set_view legacy_before, legacy_after;
    mc_registry_redstone_node_view node_before, node_after;
    list_ref captured, current, nil;
    list_view a, b;
    record_view raw, constructed, inspected;
    list_error error;
    const size_t original = owner->allocation_bytes;
    owner->options.max_allocation_bytes = original;
    const mc_registry_status refused = mc_registry_redstone_torch_list_create(owner, &first);
    owner->options.max_allocation_bytes = maximum;
    CHECK(refused == MC_REGISTRY_LIMIT && first == NULL && owner->allocation_bytes == original &&
          owner->managed_current_bytes == 0 && owner->last_error.status == MC_REGISTRY_LIMIT);
    CHECK(mc_registry_redstone_torch_list_create(owner, &first) == MC_REGISTRY_OK && first != NULL);
    const size_t allocation = owner->allocation_bytes - original;
    CHECK(allocation > 0 && owner->managed_current_bytes == 0);
    owner->db_b = first;
    CHECK(mc_registry_redstone_torch_list_capture(owner, &captured) == MC_REGISTRY_OK &&
          captured.object == first &&
          mc_registry_redstone_torch_list_inspect(&captured, &a, &error) == MC_REGISTRY_OK &&
          a.identity != 0 && a.array_identity != 0 &&
          a.backing == MC_REGISTRY_REDSTONE_TORCH_DEFAULT_EMPTY && a.size == 0 && a.capacity == 0 &&
          a.mod_count == 0 && a.unused_slots_null);
    CHECK(mc_registry_redstone_torch_snapshot_copy(&captured, &empty, &error) == MC_REGISTRY_OK &&
          owner->redstone_torch_external_occurrences == 1 &&
          mc_registry_redstone_torch_snapshot_inspect(empty, &b) == MC_REGISTRY_OK &&
          same_list(&a, &b));
    CHECK(mc_registry_classes_free(owner, NULL) == MC_REGISTRY_BUSY);
    mc_registry_redstone_torch_snapshot_release(empty);
    empty = NULL;
    CHECK(owner->redstone_torch_external_occurrences == 0 && owner->managed_current_bytes == 0);
    CHECK(mc_registry_redstone_torch_list_create(owner, &second) == MC_REGISTRY_OK &&
          owner->allocation_bytes - original == allocation * 2u &&
          owner->managed_current_bytes == 0);
    bool changed = false;
    CHECK(mc_registry_redstone_set_create(owner, &legacy) == MC_REGISTRY_OK &&
          mc_registry_redstone_set_add_fresh(legacy, 1, 2, 3, &changed) == MC_REGISTRY_OK &&
          changed && mc_registry_redstone_set_inspect(legacy, &legacy_before) == MC_REGISTRY_OK &&
          mc_registry_redstone_set_node_at(legacy, 0, &node_before) == MC_REGISTRY_OK);
    owner->db_b = second;
    CHECK(mc_registry_redstone_torch_list_capture(owner, &current) == MC_REGISTRY_OK &&
          current.object == second);
    owner->db_b = NULL;
    CHECK(mc_registry_redstone_torch_list_capture(owner, &nil) == MC_REGISTRY_OK &&
          nil.object == NULL);
    record *unchanged = (record *)(uintptr_t)1;
    CHECK(mc_registry_redstone_torch_list_get(&nil, INT32_MIN, &unchanged, &error) ==
              MC_REGISTRY_NULL_REFERENCE &&
          error.failure == MC_REGISTRY_REDSTONE_TORCH_LIST_NULL && !error.mod_count_stored &&
          unchanged == (record *)(uintptr_t)1);
    const mc_registry_error saved = owner->last_error;
    CHECK(mc_registry_redstone_torch_record_new(owner, &value, &error) == MC_REGISTRY_OK &&
          error.failure == MC_REGISTRY_REDSTONE_TORCH_NONE && !error.mod_count_stored &&
          mc_registry_redstone_torch_record_inspect(value, &raw) == MC_REGISTRY_OK &&
          raw.identity != 0 && !raw.constructed && !raw.is_null && raw.x == 0 && raw.y == 0 &&
          raw.z == 0 && raw.time == 0 && owner->redstone_torch_external_occurrences == 1);
    CHECK(mc_registry_classes_free(owner, NULL) == MC_REGISTRY_BUSY);
    CHECK(mc_registry_redstone_set_inspect(legacy, &legacy_after) == MC_REGISTRY_OK &&
          mc_registry_redstone_set_node_at(legacy, 0, &node_after) == MC_REGISTRY_OK &&
          legacy_before.map == legacy_after.map && legacy_before.table == legacy_after.table &&
          legacy_before.size == legacy_after.size &&
          legacy_before.mod_count == legacy_after.mod_count &&
          node_before.identity == node_after.identity &&
          node_before.key.identity == node_after.key.identity);
    /* Constructor operands only: this module never fabricates a World/time call. */
    CHECK(mc_registry_redstone_torch_record_construct(value, INT32_MIN, INT32_MAX, -1, INT64_MIN,
                                                      &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_record_inspect(value, &constructed) == MC_REGISTRY_OK &&
          constructed.identity == raw.identity && constructed.constructed && !constructed.is_null &&
          constructed.x == INT32_MIN && constructed.y == INT32_MAX && constructed.z == -1 &&
          constructed.time == INT64_MIN);
    CHECK(mc_registry_redstone_torch_list_add(&nil, value, &error) == MC_REGISTRY_NULL_REFERENCE &&
          error.failure == MC_REGISTRY_REDSTONE_TORCH_LIST_NULL && !error.mod_count_stored &&
          mc_registry_redstone_torch_record_inspect(value, &inspected) == MC_REGISTRY_OK &&
          same_record(&constructed, &inspected));
    CHECK(mc_registry_redstone_torch_list_add(&captured, value, &error) == MC_REGISTRY_OK &&
          error.mod_count_stored &&
          mc_registry_redstone_torch_list_size(&captured, &a.size, &error) == MC_REGISTRY_OK &&
          a.size == 1 &&
          mc_registry_redstone_torch_list_size(&current, &b.size, &error) == MC_REGISTRY_OK &&
          b.size == 0 && owner->db_b == NULL);
    mc_registry_redstone_torch_record_release(value);
    value = NULL;
    CHECK(mc_registry_redstone_torch_list_remove_first(&captured, &removed, &error) ==
              MC_REGISTRY_OK &&
          owner->redstone_torch_external_occurrences == 1 &&
          mc_registry_redstone_torch_record_inspect(removed, &inspected) == MC_REGISTRY_OK &&
          same_record(&constructed, &inspected));
    mc_registry_redstone_torch_record_release(removed);
    removed = NULL;
    CHECK(owner->redstone_torch_external_occurrences == 0 &&
          same_error(&saved, &owner->last_error));
    result = 0;
cleanup:
    owner->options.max_allocation_bytes = maximum;
    mc_registry_redstone_torch_record_release(removed);
    mc_registry_redstone_torch_record_release(value);
    mc_registry_redstone_torch_snapshot_release(empty);
    return result;
}

static int growth_prune_snapshots(mc_registry_classes *owner) {
    int result = 1;
    mc_registry_redstone_torch_list *list = NULL;
    record *value = NULL, *removed = NULL, *held = NULL, *borrowed = NULL;
    snapshot *outer = NULL, *inner = NULL;
    list_ref ref;
    list_view view, old;
    record_view values[17], actual, replacement;
    list_error error;
    CHECK(mc_registry_redstone_torch_list_create(owner, &list) == MC_REGISTRY_OK);
    owner->db_b = list;
    CHECK(mc_registry_redstone_torch_list_capture(owner, &ref) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_list_inspect(&ref, &view, &error) == MC_REGISTRY_OK);
    uint64_t previous_array = view.array_identity;
    const mc_registry_error saved = owner->last_error;
    for (int32_t i = 0; i < 17; ++i) {
        CHECK(mc_registry_redstone_torch_record_new(owner, &value, &error) == MC_REGISTRY_OK &&
              mc_registry_redstone_torch_record_construct(value, i, 64, -i, INT64_MAX - i,
                                                          &error) == MC_REGISTRY_OK &&
              mc_registry_redstone_torch_record_inspect(value, &values[i]) == MC_REGISTRY_OK &&
              mc_registry_redstone_torch_list_add(&ref, value, &error) == MC_REGISTRY_OK);
        mc_registry_redstone_torch_record_release(value);
        value = NULL;
        CHECK(mc_registry_redstone_torch_list_inspect(&ref, &view, &error) == MC_REGISTRY_OK &&
              view.size == i + 1 && view.mod_count == i + 1 && view.unused_slots_null &&
              view.capacity == (i < 10   ? 10
                                : i < 15 ? 15
                                         : 22) &&
              view.backing == MC_REGISTRY_REDSTONE_TORCH_OWNED_ARRAY);
        if (i == 0 || i == 10 || i == 15)
            CHECK(view.array_identity != previous_array);
        else
            CHECK(view.array_identity == previous_array);
        previous_array = view.array_identity;
    }
    CHECK(mc_registry_redstone_torch_list_get(&ref, 0, &borrowed, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_record_retain(borrowed, &error) == MC_REGISTRY_OK);
    held = borrowed;
    borrowed = NULL;
    CHECK(mc_registry_redstone_torch_snapshot_copy(&ref, &outer, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_snapshot_inspect(outer, &old) == MC_REGISTRY_OK &&
          old.size == 17 && old.mod_count == 17 &&
          owner->redstone_torch_external_occurrences == 2 &&
          mc_registry_classes_free(owner, NULL) == MC_REGISTRY_BUSY);
    for (int32_t i = 0; i < 17; ++i) {
        CHECK(mc_registry_redstone_torch_list_remove_first(&ref, &removed, &error) ==
                  MC_REGISTRY_OK &&
              mc_registry_redstone_torch_record_inspect(removed, &actual) == MC_REGISTRY_OK &&
              same_record(&actual, &values[i]));
        mc_registry_redstone_torch_record_release(removed);
        removed = NULL;
        CHECK(mc_registry_redstone_torch_list_inspect(&ref, &view, &error) == MC_REGISTRY_OK &&
              view.size == 16 - i && view.capacity == 22 && view.mod_count == 18 + i &&
              view.array_identity == previous_array && view.unused_slots_null);
    }
    for (size_t i = 0; i < 17; ++i)
        CHECK(mc_registry_redstone_torch_snapshot_at(outer, i, &actual) == MC_REGISTRY_OK &&
              same_record(&actual, &values[i]));
    CHECK(mc_registry_redstone_torch_snapshot_inspect(outer, &view) == MC_REGISTRY_OK &&
          same_list(&old, &view));
    CHECK(mc_registry_redstone_torch_record_new(owner, &value, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_record_construct(value, 0, 64, 0, INT64_MAX, &error) ==
              MC_REGISTRY_OK &&
          mc_registry_redstone_torch_record_inspect(value, &replacement) == MC_REGISTRY_OK &&
          replacement.identity > values[16].identity &&
          replacement.identity != values[0].identity &&
          mc_registry_redstone_torch_list_add(&ref, value, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_snapshot_copy(&ref, &inner, &error) == MC_REGISTRY_OK);
    mc_registry_redstone_torch_record_release(value);
    value = NULL;
    mc_registry_redstone_torch_snapshot_release(outer);
    outer = NULL;
    CHECK(mc_registry_redstone_torch_record_inspect(held, &actual) == MC_REGISTRY_OK &&
          same_record(&actual, &values[0]));
    mc_registry_redstone_torch_record_release(held);
    held = NULL;
    CHECK(mc_registry_redstone_torch_list_remove_first(&ref, &removed, &error) == MC_REGISTRY_OK);
    mc_registry_redstone_torch_record_release(removed);
    removed = NULL;
    CHECK(mc_registry_classes_free(owner, NULL) == MC_REGISTRY_BUSY &&
          mc_registry_redstone_torch_snapshot_at(inner, 0, &actual) == MC_REGISTRY_OK &&
          same_record(&actual, &replacement));
    mc_registry_redstone_torch_snapshot_release(inner);
    inner = NULL;
    CHECK(owner->redstone_torch_external_occurrences == 0);
    const size_t retained_array = owner->managed_current_bytes;
    const size_t permanent = owner->allocation_bytes;
    uint64_t last_identity = replacement.identity;
    for (int32_t i = 0; i < 256; ++i) {
        CHECK(mc_registry_redstone_torch_record_new(owner, &value, &error) == MC_REGISTRY_OK &&
              mc_registry_redstone_torch_record_construct(value, -i, i, 1, i, &error) ==
                  MC_REGISTRY_OK &&
              mc_registry_redstone_torch_record_inspect(value, &actual) == MC_REGISTRY_OK &&
              actual.identity > last_identity &&
              mc_registry_redstone_torch_list_add(&ref, value, &error) == MC_REGISTRY_OK);
        last_identity = actual.identity;
        mc_registry_redstone_torch_record_release(value);
        value = NULL;
        CHECK(mc_registry_redstone_torch_list_remove_first(&ref, &removed, &error) ==
              MC_REGISTRY_OK);
        mc_registry_redstone_torch_record_release(removed);
        removed = NULL;
        CHECK(owner->managed_current_bytes == retained_array &&
              owner->allocation_bytes == permanent &&
              owner->redstone_torch_external_occurrences == 0);
    }
    CHECK(same_error(&saved, &owner->last_error));
    result = 0;
cleanup:
    mc_registry_redstone_torch_record_release(value);
    mc_registry_redstone_torch_record_release(removed);
    mc_registry_redstone_torch_record_release(held);
    mc_registry_redstone_torch_snapshot_release(inner);
    mc_registry_redstone_torch_snapshot_release(outer);
    return result;
}

/* Existing Registry byte admission, not a new budget or allocator hook. */
static int allocation_prefixes(mc_registry_classes *owner) {
    int result = 1;
    const size_t maximum = owner->options.max_allocation_bytes;
    mc_registry_redstone_torch_list *list = NULL;
    record *value = NULL, *removed = NULL;
    snapshot *copy = NULL;
    list_ref ref;
    list_view before, after;
    record_view raw, actual;
    list_error error;
    CHECK(mc_registry_redstone_torch_list_create(owner, &list) == MC_REGISTRY_OK);
    owner->db_b = list;
    CHECK(mc_registry_redstone_torch_list_capture(owner, &ref) == MC_REGISTRY_OK);
    for (int32_t i = 0; i < 10; ++i) {
        CHECK(mc_registry_redstone_torch_record_new(owner, &value, &error) == MC_REGISTRY_OK &&
              mc_registry_redstone_torch_record_construct(value, i, 1, 2, i, &error) ==
                  MC_REGISTRY_OK &&
              mc_registry_redstone_torch_list_add(&ref, value, &error) == MC_REGISTRY_OK);
        mc_registry_redstone_torch_record_release(value);
        value = NULL;
    }
    CHECK(mc_registry_redstone_torch_list_inspect(&ref, &before, &error) == MC_REGISTRY_OK &&
          before.size == 10 && before.capacity == 10);
    const mc_registry_error saved = owner->last_error;
    const size_t base = owner->allocation_bytes + owner->managed_current_bytes;
    const size_t old_peak = owner->total_peak_bytes;
    owner->options.max_allocation_bytes = base;
    const mc_registry_status new_failure =
        mc_registry_redstone_torch_record_new(owner, &value, &error);
    owner->options.max_allocation_bytes = maximum;
    CHECK(new_failure == MC_REGISTRY_LIMIT && value == NULL &&
          error.failure == MC_REGISTRY_REDSTONE_TORCH_NATIVE && !error.mod_count_stored &&
          owner->total_peak_bytes == old_peak &&
          mc_registry_redstone_torch_list_inspect(&ref, &after, &error) == MC_REGISTRY_OK &&
          same_list(&before, &after));
    CHECK(mc_registry_redstone_torch_record_new(owner, &value, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_record_inspect(value, &raw) == MC_REGISTRY_OK &&
          !raw.constructed);
    /* Abandon this actual raw prefix before any constructor. */
    const uint64_t abandoned = raw.identity;
    mc_registry_redstone_torch_record_release(value);
    value = NULL;
    CHECK(mc_registry_redstone_torch_record_new(owner, &value, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_record_construct(value, 10, 1, 2, 10, &error) ==
              MC_REGISTRY_OK &&
          mc_registry_redstone_torch_record_inspect(value, &raw) == MC_REGISTRY_OK &&
          raw.identity > abandoned);
    CHECK(mc_registry_redstone_torch_list_inspect(&ref, &before, &error) == MC_REGISTRY_OK);
    const size_t before_growth = owner->allocation_bytes + owner->managed_current_bytes;
    const size_t peak_before_growth = owner->total_peak_bytes;
    owner->options.max_allocation_bytes = before_growth;
    const mc_registry_status add_failure = mc_registry_redstone_torch_list_add(&ref, value, &error);
    owner->options.max_allocation_bytes = maximum;
    CHECK(add_failure == MC_REGISTRY_LIMIT && error.failure == MC_REGISTRY_REDSTONE_TORCH_NATIVE &&
          error.mod_count_stored &&
          mc_registry_redstone_torch_list_inspect(&ref, &after, &error) == MC_REGISTRY_OK &&
          after.size == 10 && after.capacity == 10 &&
          after.array_identity == before.array_identity &&
          after.mod_count == mc_java_i32_add(before.mod_count, 1) && after.unused_slots_null &&
          owner->allocation_bytes + owner->managed_current_bytes == before_growth &&
          owner->total_peak_bytes == peak_before_growth &&
          mc_registry_redstone_torch_record_inspect(value, &actual) == MC_REGISTRY_OK &&
          same_record(&raw, &actual));
    before = after;
    owner->options.max_allocation_bytes = before_growth;
    const mc_registry_status snapshot_failure =
        mc_registry_redstone_torch_snapshot_copy(&ref, &copy, &error);
    owner->options.max_allocation_bytes = maximum;
    CHECK(snapshot_failure == MC_REGISTRY_LIMIT && copy == NULL &&
          owner->redstone_torch_external_occurrences == 1 &&
          owner->allocation_bytes + owner->managed_current_bytes == before_growth &&
          mc_registry_redstone_torch_list_inspect(&ref, &after, &error) == MC_REGISTRY_OK &&
          same_list(&before, &after));
    CHECK(mc_registry_redstone_torch_list_add(&ref, value, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_list_inspect(&ref, &after, &error) == MC_REGISTRY_OK &&
          after.size == 11 && after.capacity == 15 &&
          after.array_identity != before.array_identity &&
          after.mod_count == mc_java_i32_add(before.mod_count, 1) &&
          owner->allocation_bytes + owner->managed_current_bytes ==
              before_growth + 5u * sizeof(record *) &&
          owner->total_peak_bytes >= before_growth + 15u * sizeof(record *));
    mc_registry_redstone_torch_record_release(value);
    value = NULL;
    CHECK(same_error(&saved, &owner->last_error));
    for (int32_t i = 0; i < 11; ++i) {
        CHECK(mc_registry_redstone_torch_list_remove_first(&ref, &removed, &error) ==
                  MC_REGISTRY_OK &&
              mc_registry_redstone_torch_record_inspect(removed, &actual) == MC_REGISTRY_OK &&
              actual.x == i);
        mc_registry_redstone_torch_record_release(removed);
        removed = NULL;
    }
    result = 0;
cleanup:
    owner->options.max_allocation_bytes = maximum;
    mc_registry_redstone_torch_record_release(value);
    mc_registry_redstone_torch_record_release(removed);
    mc_registry_redstone_torch_snapshot_release(copy);
    return result;
}

static int duplicates_nullable_terminal(mc_registry_classes *owner) {
    int result = 1;
    mc_registry_redstone_torch_list *first = NULL, *second = NULL;
    record *value = NULL, *removed = NULL, *borrowed = NULL;
    snapshot *copy = NULL;
    list_ref a, b;
    list_view before, after;
    record_view expected, actual;
    list_error error;
    CHECK(mc_registry_redstone_torch_list_create(owner, &first) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_list_create(owner, &second) == MC_REGISTRY_OK);
    owner->db_b = first;
    CHECK(mc_registry_redstone_torch_list_capture(owner, &a) == MC_REGISTRY_OK);
    owner->db_b = second;
    CHECK(mc_registry_redstone_torch_list_capture(owner, &b) == MC_REGISTRY_OK);
    CHECK(mc_registry_redstone_torch_record_new(owner, &value, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_record_construct(value, 3, 4, 5, 6, &error) ==
              MC_REGISTRY_OK &&
          mc_registry_redstone_torch_record_inspect(value, &expected) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_list_add(&a, value, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_list_add(&a, value, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_list_add(&b, value, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_snapshot_copy(&a, &copy, &error) == MC_REGISTRY_OK);
    mc_registry_redstone_torch_record_release(value);
    value = NULL;
    for (int i = 0; i < 2; ++i) {
        CHECK(mc_registry_redstone_torch_list_remove_first(&a, &removed, &error) ==
                  MC_REGISTRY_OK &&
              mc_registry_redstone_torch_record_inspect(removed, &actual) == MC_REGISTRY_OK &&
              same_record(&expected, &actual));
        mc_registry_redstone_torch_record_release(removed);
        removed = NULL;
    }
    CHECK(mc_registry_redstone_torch_list_remove_first(&b, &removed, &error) == MC_REGISTRY_OK);
    mc_registry_redstone_torch_record_release(removed);
    removed = NULL;
    for (size_t i = 0; i < 2; ++i)
        CHECK(mc_registry_redstone_torch_snapshot_at(copy, i, &actual) == MC_REGISTRY_OK &&
              same_record(&expected, &actual));
    mc_registry_redstone_torch_snapshot_release(copy);
    copy = NULL;
    CHECK(mc_registry_redstone_torch_list_add(&a, NULL, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_list_get(&a, 0, &borrowed, &error) == MC_REGISTRY_OK &&
          borrowed == NULL &&
          mc_registry_redstone_torch_record_inspect(borrowed, &actual) == MC_REGISTRY_OK &&
          actual.is_null && actual.identity == 0 &&
          mc_registry_redstone_torch_list_remove_first(&a, &removed, &error) == MC_REGISTRY_OK &&
          removed == NULL && owner->redstone_torch_external_occurrences == 0);
    CHECK(mc_registry_redstone_torch_list_inspect(&a, &before, &error) == MC_REGISTRY_OK &&
          before.unused_slots_null);
    borrowed = (record *)(uintptr_t)1;
    CHECK(mc_registry_redstone_torch_list_get(&a, -1, &borrowed, &error) ==
              MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          error.failure == MC_REGISTRY_REDSTONE_TORCH_LIST_INDEX &&
          borrowed == (record *)(uintptr_t)1 && !error.mod_count_stored);
    CHECK(mc_registry_redstone_torch_list_remove_first(&a, &removed, &error) ==
              MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          error.failure == MC_REGISTRY_REDSTONE_TORCH_LIST_INDEX && removed == NULL &&
          mc_registry_redstone_torch_list_inspect(&a, &after, &error) == MC_REGISTRY_OK &&
          same_list(&before, &after));
    borrowed = NULL;
    const list_ref unknown = {owner, (const void *)(uintptr_t)1};
    CHECK(mc_registry_redstone_torch_list_get(&unknown, 0, &borrowed, &error) ==
              MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          error.failure == MC_REGISTRY_REDSTONE_TORCH_NATIVE && borrowed == NULL);
    CHECK(mc_registry_redstone_torch_record_new(owner, &value, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_record_construct(value, 1, 2, 3, 4, &error) ==
              MC_REGISTRY_OK &&
          mc_registry_redstone_torch_list_add(&a, value, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_list_add(&b, value, &error) == MC_REGISTRY_OK);
    mc_registry_redstone_torch_record_release(value);
    value = NULL;
    owner->db_b = NULL; /* Neither owned List is reachable through the class field. */
    CHECK(owner->managed_current_bytes != 0 && owner->redstone_torch_external_occurrences == 0);
    const size_t permanent = owner->allocation_bytes, peak = owner->total_peak_bytes;
    mc_registry_redstone_torch_lists_retire(owner);
    CHECK(owner->redstone_torch_lists == NULL && owner->managed_current_bytes == 0 &&
          owner->allocation_bytes == permanent && owner->total_peak_bytes == peak);
    mc_registry_redstone_torch_lists_retire(owner);
    CHECK(owner->managed_current_bytes == 0);
    result = 0;
cleanup:
    mc_registry_redstone_torch_record_release(value);
    mc_registry_redstone_torch_record_release(removed);
    mc_registry_redstone_torch_snapshot_release(copy);
    return result;
}

static int foreign_owner_refusal(mc_registry_classes *owner) {
    int result = 1;
    mc_registry_classes *foreign = NULL;
    mc_registry_redstone_torch_list *local_list = NULL, *foreign_list = NULL;
    record *value = NULL, *borrowed = NULL;
    list_ref local, captured;
    list_view before, after;
    record_view a, b;
    list_error error;
    CHECK(mc_registry_classes_allocate(&owner->options, &foreign, NULL) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_list_create(owner, &local_list) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_list_create(foreign, &foreign_list) == MC_REGISTRY_OK);
    owner->db_b = local_list;
    CHECK(mc_registry_redstone_torch_list_capture(owner, &local) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_list_inspect(&local, &before, &error) == MC_REGISTRY_OK);
    owner->db_b = foreign_list;
    CHECK(mc_registry_redstone_torch_list_capture(owner, &captured) == MC_REGISTRY_OK &&
          captured.object == foreign_list &&
          mc_registry_redstone_torch_list_get(&captured, INT32_MIN, &borrowed, &error) ==
              MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          borrowed == NULL && error.failure == MC_REGISTRY_REDSTONE_TORCH_NATIVE &&
          !error.mod_count_stored);
    owner->db_b = local_list;
    CHECK(mc_registry_redstone_torch_record_new(foreign, &value, &error) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_record_construct(value, 1, 2, 3, 4, &error) ==
              MC_REGISTRY_OK &&
          mc_registry_redstone_torch_record_inspect(value, &a) == MC_REGISTRY_OK &&
          mc_registry_redstone_torch_list_add(&local, value, &error) ==
              MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          error.failure == MC_REGISTRY_REDSTONE_TORCH_NATIVE && !error.mod_count_stored &&
          mc_registry_redstone_torch_record_inspect(value, &b) == MC_REGISTRY_OK &&
          same_record(&a, &b) &&
          mc_registry_redstone_torch_list_inspect(&local, &after, &error) == MC_REGISTRY_OK &&
          same_list(&before, &after));
    CHECK(mc_registry_classes_free(foreign, NULL) == MC_REGISTRY_BUSY);
    mc_registry_redstone_torch_record_release(value);
    value = NULL;
    CHECK(mc_registry_classes_free(foreign, NULL) == MC_REGISTRY_OK);
    foreign = NULL;
    result = 0;
cleanup:
    owner->db_b = local_list;
    mc_registry_redstone_torch_record_release(value);
    if (foreign != NULL)
        (void)mc_registry_classes_free(foreign, NULL);
    return result;
}

int main(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    mc_registry_classes *owner = NULL;
    int (*const cases[])(mc_registry_classes *) = {
        factory_and_null_prefix, growth_prune_snapshots, allocation_prefixes,
        duplicates_nullable_terminal, foreign_owner_refusal};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&materials));
    for (unsigned int side = 0; side < 2; ++side)
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            const mc_registry_options options = {
                .side = (mc_registry_side)side, .random_profile = profile, .materials = materials};
            CHECK(mc_registry_classes_allocate(&options, &owner, NULL) == MC_REGISTRY_OK);
            CHECK(cases[i](owner) == 0 && owner->redstone_torch_external_occurrences == 0);
            CHECK(mc_registry_classes_free(owner, NULL) == MC_REGISTRY_OK);
            owner = NULL;
        }
    printf("PASS: %u Redstone Torch List ownership, ArrayList and native prefix controls\n",
           checks);
    result = 0;
cleanup:
    if (owner != NULL)
        (void)mc_registry_classes_free(owner, NULL);
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
