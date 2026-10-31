#include "../../../src/block/registry/registry_internal.h"

#include <stdio.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static bool position_is(const mc_registry_redstone_position *key, int32_t x, int32_t y, int32_t z) {
    return key->x == x && key->y == y && key->z == z && key->identity != 0;
}

static bool same_node(const mc_registry_redstone_node_view *a,
                      const mc_registry_redstone_node_view *b) {
    return a->identity == b->identity && a->next == b->next && a->previous == b->previous &&
           a->parent == b->parent && a->left == b->left && a->right == b->right &&
           a->key.identity == b->key.identity && a->key.x == b->key.x && a->key.y == b->key.y &&
           a->key.z == b->key.z && a->hash == b->hash && a->bucket == b->bucket &&
           a->tree == b->tree && a->red == b->red;
}

static bool same_map(const mc_registry_redstone_set_view *a,
                     const mc_registry_redstone_set_view *b) {
    return a->map == b->map && a->table == b->table && a->size == b->size &&
           a->capacity == b->capacity && a->threshold == b->threshold &&
           a->mod_count == b->mod_count && a->load_factor == b->load_factor;
}

static int snapshot_lifetime(mc_registry_classes *owner) {
    int result = 1;
    mc_registry_redstone_set *set = NULL;
    mc_registry_redstone_snapshot *outer = NULL, *inner = NULL;
    mc_registry_redstone_set_view view;
    mc_registry_redstone_node_view first, second;
    mc_registry_redstone_position old[2], current;
    bool changed = false;
    CHECK(mc_registry_redstone_set_create(owner, &set) == MC_REGISTRY_OK);
    CHECK(mc_registry_redstone_set_inspect(set, &view) == MC_REGISTRY_OK);
    CHECK(view.map != NULL && view.table == NULL && view.capacity == 0 && view.size == 0 &&
          view.threshold == 0 && view.mod_count == 0 && view.load_factor == 0.75f);
    CHECK(mc_registry_redstone_set_add_fresh(set, 7, 64, -3, &changed) == MC_REGISTRY_OK &&
          changed);
    CHECK(mc_registry_redstone_set_identity_profile(set, NULL, NULL) == MC_REGISTRY_BUSY);
    CHECK(mc_registry_redstone_set_node_at(set, 0, &first) == MC_REGISTRY_OK);
    CHECK(mc_registry_redstone_set_readd_existing(set, first.key.identity, &changed) ==
              MC_REGISTRY_OK &&
          !changed);
    CHECK(mc_registry_redstone_set_add_fresh(set, 7, 64, -3, &changed) == MC_REGISTRY_OK &&
          !changed);
    CHECK(mc_registry_redstone_set_node_at(set, 0, &second) == MC_REGISTRY_OK &&
          same_node(&first, &second));
    CHECK(mc_registry_redstone_set_inspect(set, &view) == MC_REGISTRY_OK &&
          view.constructed_keys == 2 && view.mod_count == 1 && view.size == 1);
    CHECK(mc_registry_redstone_set_add_fresh(set, 8, 64, -3, &changed) == MC_REGISTRY_OK &&
          changed);
    CHECK(mc_registry_redstone_snapshot_copy(set, &outer) == MC_REGISTRY_OK);
    CHECK(mc_registry_redstone_snapshot_size(outer) == 2 && owner->redstone_snapshot_count == 1);
    for (size_t i = 0; i < 2; ++i)
        CHECK(mc_registry_redstone_snapshot_at(outer, i, &old[i]) == MC_REGISTRY_OK);
    CHECK(mc_registry_redstone_set_clear(set) == MC_REGISTRY_OK);
    CHECK(mc_registry_redstone_set_inspect(set, &view) == MC_REGISTRY_OK && view.size == 0 &&
          view.capacity == 16 && view.mod_count == 3);
    const void *table = view.table;
    CHECK(mc_registry_redstone_set_add_fresh(set, 7, 64, -3, &changed) == MC_REGISTRY_OK &&
          changed);
    CHECK(mc_registry_redstone_set_node_at(set, 0, &second) == MC_REGISTRY_OK &&
          position_is(&second.key, 7, 64, -3) && second.key.identity != first.key.identity);
    CHECK(mc_registry_redstone_snapshot_copy(set, &inner) == MC_REGISTRY_OK);
    CHECK(mc_registry_redstone_snapshot_at(inner, 0, &current) == MC_REGISTRY_OK &&
          current.identity == second.key.identity && owner->redstone_snapshot_count == 2);
    mc_registry_error error;
    CHECK(mc_registry_classes_free(owner, &error) == MC_REGISTRY_BUSY);
    CHECK(mc_registry_redstone_set_inspect(set, &view) == MC_REGISTRY_OK && view.size == 1 &&
          view.table == table);
    CHECK(mc_registry_redstone_set_clear(set) == MC_REGISTRY_OK);
    mc_registry_redstone_snapshot_release(inner);
    inner = NULL;
    CHECK(owner->redstone_snapshot_count == 1);
    for (size_t i = 0; i < 2; ++i) {
        CHECK(mc_registry_redstone_snapshot_at(outer, i, &current) == MC_REGISTRY_OK);
        CHECK(current.identity == old[i].identity && current.x == old[i].x &&
              current.y == old[i].y && current.z == old[i].z);
    }
    mc_registry_redstone_snapshot_release(outer);
    outer = NULL;
    const size_t retained = owner->managed_current_bytes;
    const size_t permanent = owner->allocation_bytes;
    for (int32_t i = 0; i < 512; ++i) {
        CHECK(mc_registry_redstone_set_add_fresh(set, i, -i, 3, &changed) == MC_REGISTRY_OK &&
              changed);
        CHECK(mc_registry_redstone_snapshot_copy(set, &outer) == MC_REGISTRY_OK);
        CHECK(mc_registry_redstone_set_clear(set) == MC_REGISTRY_OK);
        CHECK(mc_registry_redstone_snapshot_at(outer, 0, &current) == MC_REGISTRY_OK &&
              position_is(&current, i, -i, 3));
        mc_registry_redstone_snapshot_release(outer);
        outer = NULL;
        CHECK(owner->allocation_bytes == permanent && owner->managed_current_bytes == retained &&
              owner->redstone_snapshot_count == 0);
    }
    const size_t peak = owner->total_peak_bytes;
    CHECK(peak > permanent + retained);
    CHECK(mc_registry_redstone_set_clear(set) == MC_REGISTRY_OK);
    CHECK(mc_registry_redstone_set_inspect(set, &view) == MC_REGISTRY_OK && view.table == table &&
          view.size == 0 && view.capacity == 16 && view.threshold == 12);
    CHECK(owner->total_peak_bytes == peak && owner->managed_current_bytes == retained);
    CHECK(mc_registry_redstone_snapshot_copy(set, &outer) == MC_REGISTRY_OK &&
          mc_registry_redstone_snapshot_size(outer) == 0);
    current = (mc_registry_redstone_position){.x = 123};
    CHECK(mc_registry_redstone_snapshot_at(outer, 0, &current) == MC_REGISTRY_INDEX_OUT_OF_RANGE &&
          current.x == 123);
    result = 0;
cleanup:
    mc_registry_redstone_snapshot_release(inner);
    mc_registry_redstone_snapshot_release(outer);
    return result;
}

/* Changing only the private byte bound gives deterministic native resource
 * failures without replacing malloc or adding a production fault hook. */
static int allocation_prefixes(mc_registry_classes *owner) {
    int result = 1;
    mc_registry_redstone_set *set = NULL;
    mc_registry_redstone_snapshot *snapshot = NULL;
    mc_registry_redstone_set_view before, after;
    mc_registry_redstone_node_view nodes[12], actual;
    const size_t maximum = owner->options.max_allocation_bytes;
    bool changed = false, committed = false;
    CHECK(mc_registry_redstone_set_create(owner, &set) == MC_REGISTRY_OK);
    for (int32_t i = 0; i < 12; ++i)
        CHECK(mc_registry_redstone_set_add_fresh(set, 0, 0, i, &changed) == MC_REGISTRY_OK &&
              changed);
    CHECK(mc_registry_redstone_set_inspect(set, &before) == MC_REGISTRY_OK &&
          before.capacity == 16);
    for (size_t i = 0; i < COUNT(nodes); ++i)
        CHECK(mc_registry_redstone_set_node_at(set, i, &nodes[i]) == MC_REGISTRY_OK);
    const size_t current = owner->allocation_bytes + owner->managed_current_bytes;
    mc_registry_error diagnostic;
    memcpy(&diagnostic, &owner->last_error, sizeof(diagnostic));
    for (size_t extra = 0; extra < 4096; ++extra) {
        owner->options.max_allocation_bytes = current + extra;
        changed = false;
        mc_registry_status status = mc_registry_redstone_set_add_fresh(set, 0, 0, 12, &changed);
        owner->options.max_allocation_bytes = maximum;
        if (status == MC_REGISTRY_OK) {
            CHECK(changed && extra != 0);
            committed = true;
            break;
        }
        CHECK(status == MC_REGISTRY_LIMIT && !changed);
        CHECK(owner->allocation_bytes + owner->managed_current_bytes == current);
        CHECK(mc_registry_redstone_set_inspect(set, &after) == MC_REGISTRY_OK &&
              same_map(&before, &after));
        for (size_t i = 0; i < COUNT(nodes); ++i)
            CHECK(mc_registry_redstone_set_node_at(set, i, &actual) == MC_REGISTRY_OK &&
                  same_node(&nodes[i], &actual));
        CHECK(memcmp(&owner->last_error, &diagnostic, sizeof(diagnostic)) == 0);
    }
    CHECK(committed);
    CHECK(mc_registry_redstone_set_inspect(set, &before) == MC_REGISTRY_OK &&
          before.capacity == 32 && before.size == 13 && before.threshold == 24);
    const size_t copy_current = owner->allocation_bytes + owner->managed_current_bytes;
    committed = false;
    for (size_t extra = 0; extra < 4096; ++extra) {
        owner->options.max_allocation_bytes = copy_current + extra;
        mc_registry_status status = mc_registry_redstone_snapshot_copy(set, &snapshot);
        owner->options.max_allocation_bytes = maximum;
        if (status == MC_REGISTRY_OK) {
            CHECK(snapshot != NULL && extra != 0);
            committed = true;
            break;
        }
        CHECK(status == MC_REGISTRY_LIMIT && snapshot == NULL &&
              owner->redstone_snapshot_count == 0);
        CHECK(owner->allocation_bytes + owner->managed_current_bytes == copy_current);
        CHECK(mc_registry_redstone_set_inspect(set, &after) == MC_REGISTRY_OK &&
              same_map(&before, &after));
    }
    CHECK(committed && mc_registry_redstone_snapshot_size(snapshot) == 13);
    mc_registry_redstone_snapshot_release(snapshot);
    snapshot = NULL;
    owner->options.max_allocation_bytes = copy_current;
    changed = true;
    const mc_registry_status duplicate = mc_registry_redstone_set_add_fresh(set, 0, 0, 0, &changed);
    owner->options.max_allocation_bytes = maximum;
    CHECK(duplicate == MC_REGISTRY_LIMIT && changed);
    CHECK(mc_registry_redstone_set_inspect(set, &after) == MC_REGISTRY_OK &&
          same_map(&before, &after));
    result = 0;
cleanup:
    owner->options.max_allocation_bytes = maximum;
    mc_registry_redstone_snapshot_release(snapshot);
    return result;
}

typedef struct identity_input {
    unsigned int calls[64], total;
} identity_input;

static mc_registry_status identity_hash(void *context, const mc_registry_redstone_position *key,
                                        int32_t *out) {
    identity_input *input = context;
    if (key->identity == 0 || key->identity > COUNT(input->calls))
        return MC_REGISTRY_PROFILE_FAILED;
    ++input->calls[key->identity - 1];
    ++input->total;
    /* Explicit conditional input. Both signed ordering and equal identities
     * are exercised; these values are not claimed as default JVM entropy. */
    *out = key->identity % 2 == 0 ? INT32_MIN : INT32_MAX;
    return MC_REGISTRY_OK;
}

static int collision_policies(mc_registry_classes *owner) {
    int result = 1;
    mc_registry_redstone_set *unbound = NULL, *bound = NULL, *distinct = NULL;
    mc_registry_redstone_set_view before, after;
    mc_registry_redstone_node_view nodes[10], actual;
    identity_input input = {0};
    bool changed = false;
    CHECK(mc_registry_redstone_set_create(owner, &unbound) == MC_REGISTRY_OK);
    for (int32_t i = 0; i < 10; ++i) {
        CHECK(mc_registry_redstone_set_add_fresh(unbound, i, 0, -(i * 8976890), &changed) ==
                  MC_REGISTRY_OK &&
              changed);
        CHECK(mc_registry_redstone_set_inspect(unbound, &after) == MC_REGISTRY_OK);
        CHECK(after.capacity == (i < 8 ? 16u : i == 8 ? 32u : 64u));
    }
    CHECK(mc_registry_redstone_set_inspect(unbound, &before) == MC_REGISTRY_OK &&
          before.size == 10);
    for (size_t i = 0; i < COUNT(nodes); ++i)
        CHECK(mc_registry_redstone_set_node_at(unbound, i, &nodes[i]) == MC_REGISTRY_OK);
    changed = false;
    CHECK(mc_registry_redstone_set_add_fresh(unbound, 10, 0, -89768900, &changed) ==
              MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
          !changed);
    CHECK(mc_registry_redstone_set_inspect(unbound, &after) == MC_REGISTRY_OK &&
          same_map(&before, &after));
    for (size_t i = 0; i < COUNT(nodes); ++i)
        CHECK(mc_registry_redstone_set_node_at(unbound, i, &actual) == MC_REGISTRY_OK &&
              same_node(&nodes[i], &actual));
    CHECK(mc_registry_redstone_set_create(owner, &bound) == MC_REGISTRY_OK);
    CHECK(mc_registry_redstone_set_identity_profile(bound, identity_hash, &input) ==
          MC_REGISTRY_OK);
    for (int32_t i = 0; i < 16; ++i)
        CHECK(mc_registry_redstone_set_add_fresh(bound, i, 0, -(i * 8976890), &changed) ==
                  MC_REGISTRY_OK &&
              changed);
    CHECK(input.total > 0 && input.total <= 16);
    const unsigned int calls = input.total;
    CHECK(mc_registry_redstone_set_add_fresh(bound, 5, 0, -44884450, &changed) == MC_REGISTRY_OK &&
          !changed && input.total == calls);
    for (size_t i = 0; i < COUNT(input.calls); ++i)
        CHECK(input.calls[i] <= 1);
    for (int32_t i = 0; i < 16; ++i)
        CHECK(mc_registry_redstone_set_remove_fresh(bound, i, 0, -(i * 8976890), &changed) ==
                  MC_REGISTRY_OK &&
              changed);
    CHECK(mc_registry_redstone_set_inspect(bound, &after) == MC_REGISTRY_OK && after.size == 0 &&
          input.total == calls);
    /* Distinct full hashes sharing a bucket must treeify without a profile. */
    CHECK(mc_registry_redstone_set_create(owner, &distinct) == MC_REGISTRY_OK);
    for (int32_t i = 0; i < 16; ++i)
        CHECK(mc_registry_redstone_set_add_fresh(distinct, 0, 0, i * 64, &changed) ==
                  MC_REGISTRY_OK &&
              changed);
    CHECK(mc_registry_redstone_set_node_at(distinct, 0, &actual) == MC_REGISTRY_OK && actual.tree);
    for (int32_t i = 1; i <= 48; ++i)
        CHECK(mc_registry_redstone_set_add_fresh(distinct, 0, 0, i, &changed) == MC_REGISTRY_OK &&
              changed);
    CHECK(mc_registry_redstone_set_inspect(distinct, &after) == MC_REGISTRY_OK &&
          after.capacity == 128 && after.size == 64);
    for (int32_t i = 0; i < 16; ++i)
        CHECK(mc_registry_redstone_set_remove_fresh(distinct, 0, 0, i * 64, &changed) ==
                  MC_REGISTRY_OK &&
              changed);
    result = 0;
cleanup:
    return result;
}

int main(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    mc_registry_classes *owner = NULL;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&materials));
    for (unsigned int side = 0; side < 2; ++side) {
        const mc_registry_options options = {
            .side = (mc_registry_side)side, .random_profile = profile, .materials = materials};
        CHECK(mc_registry_classes_allocate(&options, &owner, NULL) == MC_REGISTRY_OK);
        CHECK(snapshot_lifetime(owner) == 0);
        CHECK(allocation_prefixes(owner) == 0);
        CHECK(collision_policies(owner) == 0);
        CHECK(mc_registry_classes_free(owner, NULL) == MC_REGISTRY_OK);
        owner = NULL;
    }
    printf("PASS: %u redstone Set lifetime, ordering-policy and resource-prefix checks\n", checks);
    result = 0;
cleanup:
    if (owner != NULL)
        (void)mc_registry_classes_free(owner, NULL);
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
