#include "mc/world/lighting/queue.h"
#include "reference_fixtures.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;

static bool same_bounds(const mc_light_bounds *a, const mc_light_bounds *b) {
    return a->min_x == b->min_x && a->min_y == b->min_y && a->min_z == b->min_z &&
           a->max_x == b->max_x && a->max_y == b->max_y && a->max_z == b->max_z;
}

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int test_original_merges(void) {
    for (size_t i = 0; i < sizeof(light_merge_fixtures) / sizeof(light_merge_fixtures[0]); ++i) {
        const light_merge_fixture *fixture = &light_merge_fixtures[i];
        mc_light_bounds stored = fixture->initial;
        CHECK(mc_light_bounds_merge(&stored, &fixture->requested) == fixture->accepted);
        CHECK(same_bounds(&stored, &fixture->expected));
    }
    return 0;
}

static int test_original_queue(const mc_light_region *baseline, size_t baseline_count,
                               const light_queue_fixture *steps, size_t step_count) {
    mc_light_region storage[256], expected[256];
    CHECK(baseline_count < 256u);
    mc_light_queue queue;
    CHECK(mc_light_queue_init(&queue, storage, 256));
    memcpy(storage, baseline, baseline_count * sizeof(*baseline));
    memcpy(expected, baseline, baseline_count * sizeof(*baseline));
    queue.count = baseline_count;
    for (size_t i = 0; i < step_count; ++i) {
        const light_queue_fixture *step = &steps[i];
        if (step->changed_index != SIZE_MAX) {
            CHECK(step->changed_index < step->count && step->count <= 256u);
            expected[step->changed_index] = step->changed;
        }
        if (step->admitted) {
            mc_light_queue_result result =
                mc_light_queue_enqueue(&queue, &step->request, step->coalesce);
            CHECK(result == MC_LIGHT_QUEUE_MERGED || result == MC_LIGHT_QUEUE_APPENDED);
        }
        CHECK(queue.count == step->count);
        for (size_t j = 0; j < queue.count; ++j) {
            CHECK(storage[j].kind == expected[j].kind &&
                  same_bounds(&storage[j].bounds, &expected[j].bounds));
        }
    }
    return 0;
}

static int test_storage_contract(void) {
    mc_light_region storage[2] = {{MC_LIGHT_SKY, {0, 0, 0, 1, 1, 1}},
                                  {MC_LIGHT_BLOCK, {-9, 63, -9, -9, 63, -9}}};
    mc_light_region before[2];
    memcpy(before, storage, sizeof(storage));
    mc_light_queue queue = {storage, 2, 2};
    CHECK(!mc_light_queue_init(NULL, storage, 2));
    CHECK(!mc_light_queue_init(&queue, NULL, 2));
    CHECK(!mc_light_queue_init(&queue, storage, (size_t)MC_LIGHT_QUEUE_LIMIT + 1u));
    CHECK(queue.entries == storage && queue.count == 2 && queue.capacity == 2);
    CHECK(mc_light_queue_init(&queue, storage, 2));
    CHECK(queue.entries == storage && queue.count == 0 && queue.capacity == 2);
    CHECK(memcmp(storage, before, sizeof(storage)) == 0);

    mc_light_region request = {MC_LIGHT_SKY, {0, 0, 0, 1, 1, 1}};
    CHECK(mc_light_queue_enqueue(&queue, NULL, true) == MC_LIGHT_QUEUE_INVALID);
    CHECK(mc_light_queue_enqueue(NULL, &request, true) == MC_LIGHT_QUEUE_INVALID);
    CHECK(mc_light_queue_enqueue(&queue, &request, false) == MC_LIGHT_QUEUE_APPENDED);
    CHECK(mc_light_queue_enqueue(&queue, &request, false) == MC_LIGHT_QUEUE_APPENDED);
    memcpy(before, storage, sizeof(storage));
    CHECK(mc_light_queue_enqueue(&queue, &request, false) == MC_LIGHT_QUEUE_FULL);
    CHECK(queue.count == 2 && memcmp(storage, before, sizeof(storage)) == 0);
    CHECK(mc_light_queue_enqueue(&queue, &storage[0], true) == MC_LIGHT_QUEUE_MERGED);
    CHECK(queue.count == 2 && memcmp(storage, before, sizeof(storage)) == 0);
    request.kind = (mc_light_kind)17;
    CHECK(mc_light_queue_enqueue(&queue, &request, true) == MC_LIGHT_QUEUE_INVALID);
    CHECK(queue.count == 2 && memcmp(storage, before, sizeof(storage)) == 0);
    queue.count = 3;
    request.kind = MC_LIGHT_SKY;
    CHECK(mc_light_queue_enqueue(&queue, &request, true) == MC_LIGHT_QUEUE_INVALID);
    CHECK(queue.count == 3 && memcmp(storage, before, sizeof(storage)) == 0);
    CHECK(mc_light_queue_init(&queue, NULL, 0));
    CHECK(mc_light_queue_enqueue(&queue, &request, true) == MC_LIGHT_QUEUE_FULL);
    CHECK(queue.entries == NULL && queue.count == 0 && queue.capacity == 0);
    CHECK(!mc_light_bounds_merge(NULL, &request.bounds));
    CHECK(!mc_light_bounds_merge(&request.bounds, NULL));
    mc_light_bounds bounds_before = request.bounds;
    CHECK(mc_light_bounds_merge(&request.bounds, &request.bounds));
    CHECK(memcmp(&bounds_before, &request.bounds, sizeof(bounds_before)) == 0);
    return 0;
}

static int test_original_limit(void) {
    mc_light_region *storage = malloc((size_t)MC_LIGHT_QUEUE_LIMIT * sizeof(*storage));
    CHECK(storage != NULL);
    mc_light_queue queue;
    CHECK(mc_light_queue_init(&queue, storage, MC_LIGHT_QUEUE_LIMIT));
    mc_light_region request = {MC_LIGHT_SKY, {0, 64, 0, 0, 64, 0}};
    /* No merge makes every request observable. The exact original limit is
     * retained; a smaller resource budget must instead report FULL. */
    for (size_t i = 0; i < MC_LIGHT_QUEUE_LIMIT; ++i) {
        CHECK(mc_light_queue_enqueue(&queue, &request, false) == MC_LIGHT_QUEUE_APPENDED);
        CHECK(queue.count == i + 1u);
    }
    CHECK(mc_light_queue_enqueue(&queue, &request, true) == MC_LIGHT_QUEUE_MERGED);
    CHECK(queue.count == MC_LIGHT_QUEUE_LIMIT);
    CHECK(mc_light_queue_enqueue(&queue, &request, false) == MC_LIGHT_QUEUE_CLEARED);
    CHECK(queue.count == 0 && queue.entries == storage && queue.capacity == MC_LIGHT_QUEUE_LIMIT);
    request.kind = MC_LIGHT_BLOCK;
    CHECK(mc_light_queue_enqueue(&queue, &request, false) == MC_LIGHT_QUEUE_APPENDED);
    CHECK(queue.count == 1 && storage[0].kind == MC_LIGHT_BLOCK);
    free(storage);
    return 0;
}

int main(void) {
    if (test_original_merges() ||
        test_original_queue(
            light_client_baseline, sizeof(light_client_baseline) / sizeof(light_client_baseline[0]),
            light_client_steps, sizeof(light_client_steps) / sizeof(light_client_steps[0])) ||
        test_original_queue(
            light_server_baseline, sizeof(light_server_baseline) / sizeof(light_server_baseline[0]),
            light_server_steps, sizeof(light_server_steps) / sizeof(light_server_steps[0])) ||
        test_storage_contract() || test_original_limit())
        return 1;
    printf("light queue storage checks: %u\n", checks);
    return 0;
}
