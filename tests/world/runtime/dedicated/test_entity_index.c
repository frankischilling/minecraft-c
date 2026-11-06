#include "../../../../src/world/runtime/dedicated/entity_index.h"

#include "../../../../src/entity/entity_internal.h"

#include <limits.h>
#include <stdio.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Constructed native contracts over genuine RAW Entity owners. go stores
 * references without a class/READY/ID gate; no constructor entropy is needed.
 * Only the isolated hold-overflow controls alter private native hold counters,
 * and each restores the naturally owned count before assertions/cleanup. */
typedef struct fixture {
    mc_java_random_profile *profile;
    mc_entity_classes *classes[2];
    mc_entity *values[4];
} fixture;

static unsigned int checks;

static bool prepare(fixture *f) {
    if (mc_java_random_profile_create(1024u * 1024u, &f->profile) != MC_JAVA_RANDOM_PROFILE_OK)
        return false;
    for (unsigned int side = 0; side < 2u; ++side) {
        if (mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                     NULL, &f->classes[side]) != MC_ENTITY_OK)
            return false;
        for (unsigned int slot = 0; slot < 2u; ++slot)
            if (mc_falling_entity_allocate(f->classes[side], &f->values[2u * side + slot], NULL) !=
                MC_ENTITY_OK)
                return false;
    }
    return true;
}

static bool finish(fixture *f) {
    bool complete = true;
    for (size_t slot = 0; slot < COUNT(f->values); ++slot)
        mc_entity_release(f->values[slot]);
    for (size_t side = 0; side < COUNT(f->classes); ++side) {
        if (mc_entity_classes_free(f->classes[side]) != MC_ENTITY_OK)
            complete = false;
    }
    if (complete)
        mc_java_random_profile_free(f->profile);
    return complete;
}

static size_t index_holds(const mc_world_entity_index *index, const mc_entity *value) {
    mc_world_entity_index_view view;
    size_t count = 0;
    if (mc_world_entity_index_inspect(index, &view, NULL) != MC_WORLD_STORAGE_OK)
        return 0;
    for (size_t slot = 0; slot < view.a_length; ++slot)
        for (const mc_world_entity_index_node *node = view.a[slot]; node != NULL; node = node->c)
            if (node->b == value)
                ++count;
    return count;
}

static int queries_and_chains(void) {
    int result = 1;
    fixture f = {0};
    mc_world_entity_index *index = NULL;
    mc_entity *transferred = NULL;
    mc_world_entity_index_view view = {0};
    mc_world_entity_index_error error = {0};
    bool present = true;
    mc_entity *value = NULL;

    static const struct {
        int32_t key;
        uint32_t hash;
    } vectors[] = {{0, 0},
                   {1, 1},
                   {16, 17},
                   {17, 16},
                   {INT32_MIN, UINT32_C(0x89089890)},
                   {INT32_MAX, UINT32_C(0x78f8778f)},
                   {-1, UINT32_C(0xf1f0ef1f)}};

    CHECK(prepare(&f));
    CHECK(mc_world_entity_index_create(NULL, &index, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_entity_index_inspect(index, &view, &error) == MC_WORLD_STORAGE_OK);
    CHECK(view.owner == index && view.a != NULL && view.a_length == 16u && view.b == 0 &&
          view.c == 12 && view.d == 0.75f && view.e == 0);
    for (size_t slot = 0; slot < view.a_length; ++slot)
        CHECK(view.a[slot] == NULL);
    for (size_t slot = 0; slot < COUNT(vectors); ++slot) {
        mc_entity *input = f.values[slot % COUNT(f.values)];
        CHECK(mc_world_entity_index_put(index, vectors[slot].key, input, &error) ==
              MC_WORLD_STORAGE_OK);
        CHECK(mc_world_entity_index_get(index, vectors[slot].key, &present, &value, &error) ==
                  MC_WORLD_STORAGE_OK &&
              present && value == input);
        CHECK(mc_world_entity_index_inspect(index, &view, &error) == MC_WORLD_STORAGE_OK);
        const mc_world_entity_index_node *node = view.a[vectors[slot].hash & UINT32_C(15)];
        CHECK(node != NULL && node->a == vectors[slot].key && node->b == input &&
              (uint32_t)node->d == vectors[slot].hash);
    }
    CHECK(view.b == 7 && view.e == 7);
    value = f.values[0];
    CHECK(mc_world_entity_index_get(index, 99, &present, &value, &error) == MC_WORLD_STORAGE_OK &&
          !present && value == NULL);
    CHECK(mc_world_entity_index_contains(index, 99, &present, &error) == MC_WORLD_STORAGE_OK &&
          !present);
    CHECK(mc_world_entity_index_remove(index, 99, &present, &transferred, &error) ==
              MC_WORLD_STORAGE_OK &&
          !present && transferred == NULL);
    CHECK(mc_world_entity_index_inspect(index, &view, &error) == MC_WORLD_STORAGE_OK &&
          view.e == 7);

    /* Present-null is distinct from absence for get, contains and removal. */
    CHECK(mc_world_entity_index_put(index, 99, NULL, &error) == MC_WORLD_STORAGE_OK);
    value = f.values[0];
    CHECK(mc_world_entity_index_get(index, 99, &present, &value, &error) == MC_WORLD_STORAGE_OK &&
          present && value == NULL);
    CHECK(mc_world_entity_index_contains(index, 99, &present, &error) == MC_WORLD_STORAGE_OK &&
          present);
    CHECK(mc_world_entity_index_remove(index, 99, &present, &transferred, &error) ==
              MC_WORLD_STORAGE_OK &&
          present && transferred == NULL);

    /* The two old key17/key0 entries share bucket0 with INT32_MIN. Remove the
     * interior key17, preserving both neighboring actual node identities. */
    CHECK(mc_world_entity_index_inspect(index, &view, &error) == MC_WORLD_STORAGE_OK);
    const mc_world_entity_index_node *head = view.a[0];
    CHECK(head != NULL && head->c != NULL && head->c->c != NULL);
    const mc_world_entity_index_node *tail = head->c->c;
    CHECK(head->a == INT32_MIN && head->c->a == 17 && tail->a == 0 && tail->c == NULL);
    const size_t held = f.values[3]->holds;
    CHECK(mc_world_entity_index_remove(index, 17, &present, &transferred, &error) ==
              MC_WORLD_STORAGE_OK &&
          present && transferred == f.values[3]);
    CHECK(head->c == tail && transferred->holds == held);
    mc_entity_release(transferred);
    transferred = NULL;
    CHECK(f.values[3]->holds == held - 1u);
    CHECK(mc_world_entity_index_get(index, 0, &present, &value, &error) == MC_WORLD_STORAGE_OK &&
          present && value == f.values[0]);
    CHECK(mc_world_entity_index_clear(index, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_entity_index_inspect(index, &view, &error) == MC_WORLD_STORAGE_OK &&
          view.b == 0 && view.e == 11);
    const void *table = view.a;
    const size_t bytes = view.allocation_bytes;
    CHECK(mc_world_entity_index_clear(index, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_entity_index_inspect(index, &view, &error) == MC_WORLD_STORAGE_OK &&
          view.a == table && view.allocation_bytes == bytes && view.e == 12 && view.c == 12);
    for (size_t slot = 0; slot < COUNT(f.values); ++slot)
        CHECK(f.values[slot]->holds == 1u);
    result = 0;
cleanup:
    mc_entity_release(transferred);
    mc_world_entity_index_free(index);
    if (!finish(&f))
        result = 1;
    return result;
}

static int duplicates_and_resize(void) {
    int result = 1;
    fixture f = {0};
    mc_world_entity_index *index = NULL;
    mc_world_entity_index_view view = {0};
    mc_world_entity_index_node *nodes[13] = {0};
    mc_entity *transferred = NULL;
    bool present = false;
    CHECK(prepare(&f));
    CHECK(mc_world_entity_index_create(NULL, &index, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_entity_index_inspect(index, &view, NULL) == MC_WORLD_STORAGE_OK);
    const uintptr_t original_table = (uintptr_t)view.a;
    for (size_t slot = 0; slot < COUNT(nodes); ++slot) {
        mc_entity *input = slot < 12u ? f.values[0] : f.values[2];
        CHECK(mc_world_entity_index_put(index, 0, input, NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_entity_index_inspect(index, &view, NULL) == MC_WORLD_STORAGE_OK);
        if (slot < 12u) {
            CHECK(view.a_length == 16u && view.c == 12 && (uintptr_t)view.a == original_table);
            nodes[slot] = view.a[0];
        } else {
            CHECK(view.a_length == 32u && view.c == 24 && (uintptr_t)view.a != original_table);
            mc_world_entity_index_node *node = view.a[0];
            for (size_t earlier = 0; earlier < 12u; ++earlier) {
                CHECK(node == nodes[earlier] && node->a == 0 && node->b == f.values[2]);
                node = node->c;
            }
            nodes[slot] = node;
            CHECK(node != NULL && node->a == 0 && node->b == f.values[2] && node->c == NULL);
        }
        CHECK(view.b == (int32_t)(slot + 1u) && view.e == (int32_t)(slot + 1u));
    }
    CHECK(f.values[0]->holds == 1u && f.values[2]->holds == 14u);
    CHECK(mc_world_entity_index_remove(index, 0, &present, &transferred, NULL) ==
              MC_WORLD_STORAGE_OK &&
          present && transferred == f.values[2]);
    CHECK(mc_world_entity_index_inspect(index, &view, NULL) == MC_WORLD_STORAGE_OK &&
          view.a[0] == nodes[1] && view.b == 12 && view.e == 14 && transferred->holds == 14u);
    mc_entity_release(transferred);
    transferred = NULL;
    CHECK(f.values[2]->holds == 13u);
    CHECK(mc_world_entity_index_put(index, 0, NULL, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(f.values[2]->holds == 1u);
    CHECK(mc_world_entity_index_inspect(index, &view, NULL) == MC_WORLD_STORAGE_OK &&
          view.b == 13 && view.e == 15 && view.a_length == 32u);
    const void *table = view.a;
    CHECK(mc_world_entity_index_clear(index, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_entity_index_inspect(index, &view, NULL) == MC_WORLD_STORAGE_OK &&
          view.a == table && view.a_length == 32u && view.c == 24 && view.b == 0 && view.e == 16);
    result = 0;
cleanup:
    mc_entity_release(transferred);
    mc_world_entity_index_free(index);
    if (!finish(&f))
        result = 1;
    return result;
}

static int allocation_prefixes(void) {
    int result = 1;
    fixture f = {0};
    mc_world_entity_index *index = NULL;
    mc_world_entity_index_view view = {0};
    mc_world_entity_index_error error = {0};
    CHECK(prepare(&f));
    CHECK(mc_world_entity_index_create(NULL, &index, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_entity_index_inspect(index, &view, NULL) == MC_WORLD_STORAGE_OK);
    const size_t base = view.allocation_bytes;
    mc_world_entity_index *preserved = index;
    mc_world_entity_index_limits limits = mc_world_entity_index_default_limits();
    limits.max_retained_bytes = base - 1u;
    CHECK(mc_world_entity_index_create(&limits, &preserved, &error) == MC_WORLD_STORAGE_LIMIT &&
          preserved == index && error.method == MC_WORLD_ENTITY_INDEX_CONSTRUCTOR &&
          error.bytecode == 19);
    limits = mc_world_entity_index_default_limits();
    limits.max_peak_bytes = base - 1u;
    CHECK(mc_world_entity_index_create(&limits, &preserved, &error) == MC_WORLD_STORAGE_LIMIT &&
          preserved == index);
    limits = mc_world_entity_index_default_limits();
    limits.max_nodes = (size_t)INT32_MAX + 1u;
    CHECK(mc_world_entity_index_create(&limits, &preserved, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          preserved == index);
    mc_world_entity_index_free(index);
    index = NULL;

    for (unsigned int bound = 0; bound < 3u; ++bound) {
        limits = mc_world_entity_index_default_limits();
        if (bound == 0u)
            limits.max_nodes = 2u;
        else if (bound == 1u)
            limits.max_retained_bytes = base + 2u * sizeof(mc_world_entity_index_node);
        else
            limits.max_peak_bytes = base + 2u * sizeof(mc_world_entity_index_node);
        CHECK(mc_world_entity_index_create(&limits, &index, &error) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_entity_index_put(index, -7, f.values[0], &error) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_entity_index_put(index, -7, f.values[0], &error) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_entity_index_put(index, -7, f.values[2], &error) == MC_WORLD_STORAGE_LIMIT &&
              error.method == MC_WORLD_ENTITY_INDEX_NODE && error.bytecode == 15);
        CHECK(mc_world_entity_index_inspect(index, &view, NULL) == MC_WORLD_STORAGE_OK &&
              view.b == 2 && view.e == 3 &&
              view.allocation_bytes == base + 2u * sizeof(mc_world_entity_index_node));
        CHECK(f.values[0]->holds == 1u && f.values[2]->holds == 3u);
        const size_t slot = UINT32_C(0xf1f0ef19) & UINT32_C(15);
        CHECK(view.a[slot] != NULL && view.a[slot]->a == -7 && view.a[slot]->b == f.values[2] &&
              view.a[slot]->c != NULL && view.a[slot]->c->b == f.values[2] &&
              view.a[slot]->c->c == NULL);
        mc_world_entity_index_free(index);
        index = NULL;
        CHECK(f.values[2]->holds == 1u);
    }

    for (unsigned int bound = 0; bound < 2u; ++bound) {
        limits = mc_world_entity_index_default_limits();
        const size_t nodes = 13u * sizeof(mc_world_entity_index_node);
        if (bound == 0u)
            limits.max_retained_bytes = base + nodes;
        else {
            limits.max_retained_bytes = base + nodes + 16u * sizeof(mc_world_entity_index_node *);
            limits.max_peak_bytes = base + nodes + 32u * sizeof(mc_world_entity_index_node *) - 1u;
        }
        CHECK(mc_world_entity_index_create(&limits, &index, &error) == MC_WORLD_STORAGE_OK);
        for (int32_t key = 0; key < 12; ++key)
            CHECK(mc_world_entity_index_put(index, key, f.values[1], &error) ==
                  MC_WORLD_STORAGE_OK);
        CHECK(mc_world_entity_index_put(index, 12, f.values[1], &error) == MC_WORLD_STORAGE_LIMIT &&
              error.method == MC_WORLD_ENTITY_INDEX_RESIZE && error.bytecode == 22);
        CHECK(mc_world_entity_index_inspect(index, &view, NULL) == MC_WORLD_STORAGE_OK &&
              view.b == 13 && view.e == 13 && view.a_length == 16u && view.c == 12 &&
              view.allocation_bytes == base + nodes && f.values[1]->holds == 14u);
        for (int32_t key = 0; key <= 12; ++key) {
            bool present;
            mc_entity *value;
            CHECK(mc_world_entity_index_get(index, key, &present, &value, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  present && value == f.values[1]);
        }
        CHECK(mc_world_entity_index_clear(index, NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_entity_index_inspect(index, &view, NULL) == MC_WORLD_STORAGE_OK &&
              view.b == 0 && view.e == 14 && view.a_length == 16u && view.c == 12 &&
              view.allocation_bytes == base);
        CHECK(f.values[1]->holds == 1u);
        mc_world_entity_index_free(index);
        index = NULL;
    }
    result = 0;
cleanup:
    mc_world_entity_index_free(index);
    if (!finish(&f))
        result = 1;
    return result;
}

static int collision_split_resize(void) {
    int result = 1;
    fixture f = {0};
    mc_world_entity_index *index = NULL;
    mc_world_entity_index_view view = {0};
    const mc_world_entity_index_node *nodes[8] = {0};
    const int32_t keys[] = {0, 17, 34, 51, 68, 85, 102, 119};
    CHECK(prepare(&f));
    CHECK(mc_world_entity_index_create(NULL, &index, NULL) == MC_WORLD_STORAGE_OK);
    /* These source hash vectors all start in bucket0, then split into buckets
     * 0/16 during growth. Expected chains retain the original node owners. */
    for (size_t slot = 0; slot < COUNT(keys); ++slot) {
        CHECK(mc_world_entity_index_put(index, keys[slot], f.values[slot % 4u], NULL) ==
              MC_WORLD_STORAGE_OK);
        CHECK(mc_world_entity_index_inspect(index, &view, NULL) == MC_WORLD_STORAGE_OK);
        nodes[slot] = view.a[0];
        CHECK(nodes[slot] != NULL && nodes[slot]->a == keys[slot] &&
              (uint32_t)nodes[slot]->d == (uint32_t)(16u * slot));
    }
    for (int32_t key = 1; key <= 5; ++key)
        CHECK(mc_world_entity_index_put(index, key, NULL, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_entity_index_inspect(index, &view, NULL) == MC_WORLD_STORAGE_OK &&
          view.b == 13 && view.a_length == 32u && view.c == 24 && view.e == 13);
    for (size_t parity = 0; parity < 2u; ++parity) {
        const mc_world_entity_index_node *node = view.a[16u * parity];
        for (size_t slot = parity; slot < COUNT(keys); slot += 2u) {
            CHECK(node == nodes[slot] && node != NULL && node->a == keys[slot] &&
                  node->b == f.values[slot % 4u]);
            node = node->c;
        }
        CHECK(node == NULL);
    }
    result = 0;
cleanup:
    mc_world_entity_index_free(index);
    if (!finish(&f))
        result = 1;
    return result;
}

static int hold_failure_prefixes(void) {
    int result = 1;
    fixture f = {0};
    mc_world_entity_index *index = NULL;
    mc_world_entity_index_view view = {0};
    mc_world_entity_index_error error = {0};
    CHECK(prepare(&f));
    CHECK(mc_world_entity_index_create(NULL, &index, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_entity_index_put(index, 0, f.values[0], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_entity_index_put(index, 0, f.values[0], NULL) == MC_WORLD_STORAGE_OK);

    /* First duplicate replacement obtains a real hold; the second overflows.
     * Restore the counter to caller+that completed node before any CHECK. */
    const size_t normal = f.values[2]->holds;
    f.values[2]->holds = SIZE_MAX - 1u;
    const mc_world_storage_status partial =
        mc_world_entity_index_put(index, 0, f.values[2], &error);
    const size_t reached = f.values[2]->holds;
    f.values[2]->holds = normal + index_holds(index, f.values[2]);
    CHECK(partial == MC_WORLD_STORAGE_LIMIT && reached == SIZE_MAX &&
          error.entity_status == MC_ENTITY_LIMIT && error.method == MC_WORLD_ENTITY_INDEX_PUT &&
          error.bytecode == 42);
    CHECK(mc_world_entity_index_inspect(index, &view, NULL) == MC_WORLD_STORAGE_OK && view.b == 2 &&
          view.e == 2 && view.a[0]->b == f.values[2] && view.a[0]->c->b == f.values[0]);
    CHECK(f.values[0]->holds == 2u && f.values[2]->holds == 2u);
    CHECK(mc_world_entity_index_clear(index, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(f.values[0]->holds == 1u && f.values[2]->holds == 1u);
    CHECK(mc_world_entity_index_put(index, 0, f.values[2], NULL) == MC_WORLD_STORAGE_OK);

    /* Same-pointer replacement needs no additional hold. The failure is later,
     * after mod++, at the new node's retain bridge, with no published node. */
    f.values[2]->holds = SIZE_MAX;
    const mc_world_storage_status late = mc_world_entity_index_put(index, 0, f.values[2], &error);
    const size_t late_holds = f.values[2]->holds;
    f.values[2]->holds = 1u + index_holds(index, f.values[2]);
    CHECK(late == MC_WORLD_STORAGE_LIMIT && late_holds == SIZE_MAX &&
          error.entity_status == MC_ENTITY_LIMIT && error.method == MC_WORLD_ENTITY_INDEX_NODE &&
          error.bytecode == 24);
    CHECK(mc_world_entity_index_inspect(index, &view, NULL) == MC_WORLD_STORAGE_OK && view.b == 1 &&
          view.e == 5 && view.a[0]->b == f.values[2] && view.a[0]->c == NULL);
    CHECK(f.values[2]->holds == 2u);
    result = 0;
cleanup:
    mc_world_entity_index_free(index);
    if (!finish(&f))
        result = 1;
    return result;
}

static int output_and_lifetime_contracts(void) {
    int result = 1;
    fixture f = {0};
    mc_world_entity_index *index = NULL;
    mc_world_entity_index_error error = {0};
    mc_entity *transferred = NULL;
    mc_entity_snapshot snapshot;
    mc_entity_class_snapshot classes;
    CHECK(prepare(&f));
    CHECK(mc_world_entity_index_create(NULL, &index, NULL) == MC_WORLD_STORAGE_OK);
    bool present = true;
    mc_entity *value = f.values[0];
    CHECK(mc_world_entity_index_get(NULL, 0, &present, &value, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          present && value == f.values[0]);
    CHECK(mc_world_entity_index_get(index, 0, NULL, &value, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value == f.values[0]);
    CHECK(mc_world_entity_index_get(index, 0, &present, NULL, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          present);
    CHECK(mc_world_entity_index_contains(NULL, 0, &present, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          present);
    CHECK(mc_world_entity_index_remove(index, 0, NULL, &value, &error) ==
              MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          value == f.values[0]);
    CHECK(mc_world_entity_index_put(index, 1, f.values[0], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_entity_index_put(index, 2, f.values[0], NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_entity_index_put(index, 3, f.values[2], NULL) == MC_WORLD_STORAGE_OK);
    for (size_t slot = 0; slot < COUNT(f.values); ++slot) {
        mc_entity_release(f.values[slot]);
        f.values[slot] = NULL;
    }
    CHECK(mc_entity_classes_free(f.classes[0]) == MC_ENTITY_BUSY &&
          mc_entity_classes_free(f.classes[1]) == MC_ENTITY_BUSY);
    CHECK(mc_world_entity_index_get(index, 1, &present, &value, NULL) == MC_WORLD_STORAGE_OK &&
          present && value != NULL && mc_entity_get_snapshot(value, &snapshot) == MC_ENTITY_OK &&
          snapshot.phase == MC_ENTITY_RAW && value->holds == 2u);
    CHECK(mc_world_entity_index_remove(index, 1, &present, &transferred, NULL) ==
              MC_WORLD_STORAGE_OK &&
          present && transferred == value && transferred->holds == 2u);
    CHECK(mc_world_entity_index_remove(index, 2, &present, NULL, NULL) == MC_WORLD_STORAGE_OK &&
          present && transferred->holds == 1u);
    mc_world_entity_index_free(index);
    index = NULL;
    CHECK(mc_entity_classes_get_snapshot(f.classes[1], &classes) == MC_ENTITY_OK &&
          classes.live_entities == 0u);
    CHECK(mc_entity_classes_free(f.classes[0]) == MC_ENTITY_BUSY &&
          mc_entity_get_snapshot(transferred, &snapshot) == MC_ENTITY_OK);
    mc_entity_release(transferred);
    transferred = NULL;
    CHECK(mc_entity_classes_get_snapshot(f.classes[0], &classes) == MC_ENTITY_OK &&
          classes.live_entities == 0u);
    result = 0;
cleanup:
    mc_entity_release(transferred);
    mc_world_entity_index_free(index);
    if (!finish(&f))
        result = 1;
    return result;
}

int main(void) {
    if (queries_and_chains() || duplicates_and_resize() || allocation_prefixes() ||
        collision_split_resize() || hold_failure_prefixes() || output_and_lifetime_contracts())
        return 1;
    printf("Dedicated Entity index: %u checks passed\n", checks);
    return 0;
}
