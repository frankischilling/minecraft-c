/* Compile this unchanged source against either the old or extracted production
 * Integer set, with identical C17 -O2 -DNDEBUG flags. No private layout access.
 * The clock() interval includes the same checks and checksum work in both.
 * Interpret ticks in the actual C runtime context; MSVC clock() differs from
 * GCC's process-CPU interpretation. This is not a cross-platform timing claim.
 *
 * Usage: benchmark_int_set [LOOPS], canonical decimal 1..128; default 64.
 * Start a paired correctness smoke with 8, then use 64 for measurement. The
 * fixed workload sizes do not change with timing or observed output. The
 * 0.2..2-second/case sizing aim is not an observed duration or an FPS claim.
 */
#include "mc/core/java/int_set.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>

#define DEFAULT_LOOPS UINT32_C(64)
#define MAX_LOOPS UINT32_C(128)
#define DISPERSED_ENTRIES UINT32_C(4096)
#define COLLISION_ENTRIES UINT32_C(4096)
#define SPARSE_ENTRIES UINT32_C(16384)
#define SPARSE_SURVIVORS UINT32_C(8)
#define SPARSE_COPIES UINT32_C(256)

static int32_t dispersed_keys[DISPERSED_ENTRIES * 2u];
static int32_t collision_keys[COLLISION_ENTRIES];
static int32_t sparse_keys[SPARSE_ENTRIES];
/* Caller-owned output is disjoint from every production set and input array. */
static int32_t order[SPARSE_ENTRIES];

typedef struct sample {
    const char *name;
    uint32_t iteration;
    uint64_t operations, checksum;
} sample;

enum operation_tag { CREATE = 1, ADD, REMOVE, CONTAINS, SIZE, COPY_ORDER, FIRST, FREE };

static void fold(sample *s, uint64_t word) {
    s->checksum ^= word + UINT64_C(0x9e3779b97f4a7c15) + (s->checksum << 6u) + (s->checksum >> 2u);
    s->checksum = (s->checksum << 27u) | (s->checksum >> 37u);
}

static void entered(sample *s, enum operation_tag tag) {
    ++s->operations;
    fold(s, (uint64_t)tag);
}

static bool failure(const sample *s, const char *operation, const char *reason) {
    fprintf(stderr, "benchmark: %s iteration %" PRIu32 " %s: %s\n", s->name, s->iteration,
            operation, reason);
    return false;
}

/* Avoid implementation-defined conversion of an out-of-range unsigned word. */
static int32_t signed_bits(uint32_t word) {
    if (word <= (uint32_t)INT32_MAX)
        return (int32_t)word;
    return INT32_MIN + (int32_t)(word - UINT32_C(0x80000000));
}

/* Every step is bijective over 32-bit words, so the authored indices give
 * distinct keys. This is test-data arithmetic, not a replacement set/hash. */
static int32_t dispersed_key(uint32_t index) {
    uint32_t word = index * UINT32_C(0x9e3779b9) + UINT32_C(0x7f4a7c15);
    word ^= word >> 16u;
    word *= UINT32_C(0x85ebca6b);
    word ^= word >> 13u;
    word *= UINT32_C(0xc2b2ae35);
    word ^= word >> 16u;
    return signed_bits(word);
}

static void prepare_keys(void) {
    for (uint32_t i = 0; i < DISPERSED_ENTRIES * 2u; ++i)
        dispersed_keys[i] = dispersed_key(i);
    for (uint32_t i = 0; i < COLLISION_ENTRIES; ++i) {
        uint32_t spread = i * UINT32_C(65536);
        if ((i & 1u) != 0)
            spread ^= UINT32_C(0x80000000);
        collision_keys[i] = signed_bits(spread ^ (spread >> 16u));
    }
    for (uint32_t i = 0; i < SPARSE_ENTRIES; ++i)
        sparse_keys[i] = dispersed_key(i + UINT32_C(0x01000000));
}

static bool create(sample *s, mc_java_int_set **out) {
    entered(s, CREATE);
    mc_java_int_set_status status = mc_java_int_set_create(NULL, out);
    fold(s, (uint64_t)status);
    return (status == MC_JAVA_INT_SET_OK && *out != NULL) ||
           failure(s, "create", "public status/owner");
}

static bool mutate(sample *s, mc_java_int_set *set, int32_t key, bool add, bool expected) {
    bool changed = !expected;
    entered(s, add ? ADD : REMOVE);
    mc_java_int_set_status status =
        add ? mc_java_int_set_add(set, key, &changed) : mc_java_int_set_remove(set, key, &changed);
    fold(s, (uint64_t)status);
    fold(s, changed ? UINT64_C(1) : UINT64_C(0));
    return (status == MC_JAVA_INT_SET_OK && changed == expected) ||
           failure(s, add ? "add" : "remove", "public status/changed");
}

static bool contains(sample *s, const mc_java_int_set *set, int32_t key, bool expected) {
    entered(s, CONTAINS);
    bool result = mc_java_int_set_contains(set, key);
    fold(s, result ? UINT64_C(1) : UINT64_C(0));
    return result == expected || failure(s, "contains", "public membership result");
}

static bool observe_order(sample *s, const mc_java_int_set *set, size_t expected) {
    entered(s, SIZE);
    size_t size = mc_java_int_set_size(set);
    fold(s, (uint64_t)size);
    if (size != expected)
        return failure(s, "size", "public size differs");

    size_t count = SIZE_MAX;
    entered(s, COPY_ORDER);
    mc_java_int_set_status status = mc_java_int_set_copy_order(set, order, SPARSE_ENTRIES, &count);
    fold(s, (uint64_t)status);
    if (status != MC_JAVA_INT_SET_OK || count != expected || count > SPARSE_ENTRIES)
        return failure(s, "copy_order", "public status/count");
    fold(s, (uint64_t)count);
    for (size_t i = 0; i < count; ++i)
        fold(s, (uint64_t)(uint32_t)order[i]);

    int32_t first = INT32_MIN;
    entered(s, FIRST);
    status = mc_java_int_set_first(set, &first);
    fold(s, (uint64_t)status);
    fold(s, (uint64_t)(uint32_t)first);
    if (count == 0) {
        if (status != MC_JAVA_INT_SET_EMPTY || first != INT32_MIN)
            return failure(s, "first", "empty status/output preservation");
    } else if (status != MC_JAVA_INT_SET_OK || first != order[0]) {
        return failure(s, "first", "public status/order prefix");
    }
    return true;
}

static void destroy(sample *s, mc_java_int_set *set) {
    entered(s, FREE);
    mc_java_int_set_free(set);
}

static bool dispersed_round(sample *s) {
    mc_java_int_set *set = NULL;
    if (!create(s, &set))
        goto failed;
    for (uint32_t i = 0; i < DISPERSED_ENTRIES; ++i)
        if (!mutate(s, set, dispersed_keys[i], true, true))
            goto failed;
    for (uint32_t i = 0; i < DISPERSED_ENTRIES; ++i)
        if (!mutate(s, set, dispersed_keys[i], true, false))
            goto failed;
    for (uint32_t i = 0; i < DISPERSED_ENTRIES; ++i) {
        if (!contains(s, set, dispersed_keys[i], true) ||
            !contains(s, set, dispersed_keys[DISPERSED_ENTRIES + i], false))
            goto failed;
    }
    if (!observe_order(s, set, DISPERSED_ENTRIES))
        goto failed;
    for (uint32_t i = 0; i < DISPERSED_ENTRIES; i += 2u)
        if (!mutate(s, set, dispersed_keys[i], false, true))
            goto failed;
    for (uint32_t i = 0; i < DISPERSED_ENTRIES; ++i)
        if (!contains(s, set, dispersed_keys[i], (i & 1u) != 0))
            goto failed;
    for (uint32_t i = 0; i < DISPERSED_ENTRIES; i += 2u)
        if (!mutate(s, set, dispersed_keys[i], false, false))
            goto failed;
    for (uint32_t i = 0; i < DISPERSED_ENTRIES; i += 2u)
        if (!mutate(s, set, dispersed_keys[i], true, true))
            goto failed;
    if (!observe_order(s, set, DISPERSED_ENTRIES))
        goto failed;
    for (uint32_t i = DISPERSED_ENTRIES; i != 0; --i)
        if (!mutate(s, set, dispersed_keys[i - 1u], false, true))
            goto failed;
    if (!observe_order(s, set, 0))
        goto failed;
    destroy(s, set);
    return true;
failed:
    destroy(s, set);
    return false;
}

static bool collision_round(sample *s) {
    mc_java_int_set *set = NULL;
    if (!create(s, &set))
        goto failed;
    for (uint32_t i = 0; i < COLLISION_ENTRIES; ++i)
        if (!mutate(s, set, collision_keys[i], true, true))
            goto failed;
    if (!observe_order(s, set, COLLISION_ENTRIES))
        goto failed;
    for (uint32_t i = 0; i < COLLISION_ENTRIES; ++i)
        if (!mutate(s, set, collision_keys[i], true, false))
            goto failed;
    for (uint32_t i = 0; i < COLLISION_ENTRIES; i += 2u)
        if (!mutate(s, set, collision_keys[i], false, true))
            goto failed;
    if (!observe_order(s, set, COLLISION_ENTRIES / 2u))
        goto failed;
    for (uint32_t i = 0; i < COLLISION_ENTRIES; i += 2u)
        if (!mutate(s, set, collision_keys[i], true, true))
            goto failed;
    for (uint32_t i = 0; i < COLLISION_ENTRIES; ++i)
        if (!contains(s, set, collision_keys[i], true))
            goto failed;
    if (!observe_order(s, set, COLLISION_ENTRIES))
        goto failed;
    for (uint32_t i = COLLISION_ENTRIES; i != 0; --i)
        if (!mutate(s, set, collision_keys[i - 1u], false, true))
            goto failed;
    if (!observe_order(s, set, 0))
        goto failed;
    destroy(s, set);
    return true;
failed:
    destroy(s, set);
    return false;
}

static bool sparse_round(sample *s) {
    const uint32_t stride = SPARSE_ENTRIES / SPARSE_SURVIVORS;
    mc_java_int_set *set = NULL;
    if (!create(s, &set))
        goto failed;
    for (uint32_t i = 0; i < SPARSE_ENTRIES; ++i)
        if (!mutate(s, set, sparse_keys[i], true, true))
            goto failed;
    if (!observe_order(s, set, SPARSE_ENTRIES))
        goto failed;
    for (uint32_t i = 0; i < SPARSE_ENTRIES; ++i)
        if (i % stride != 0 && !mutate(s, set, sparse_keys[i], false, true))
            goto failed;
    for (uint32_t i = 0; i < SPARSE_SURVIVORS; ++i)
        if (!contains(s, set, sparse_keys[i * stride], true))
            goto failed;
    if (!contains(s, set, sparse_keys[1], false))
        goto failed;
    for (uint32_t copy = 0; copy < SPARSE_COPIES; ++copy)
        if (!observe_order(s, set, SPARSE_SURVIVORS))
            goto failed;
    for (uint32_t i = 0; i < SPARSE_SURVIVORS; ++i)
        if (!mutate(s, set, sparse_keys[i * stride], false, true))
            goto failed;
    if (!observe_order(s, set, 0))
        goto failed;
    destroy(s, set);
    return true;
failed:
    destroy(s, set);
    return false;
}

static bool run_case(const char *name, uint32_t loops, uint64_t seed, bool (*round)(sample *)) {
    sample s = {.name = name, .checksum = seed};
    clock_t start = clock();
    if (start == (clock_t)-1)
        return failure(&s, "clock", "clock() unavailable");
    for (uint32_t i = 0; i < loops; ++i) {
        s.iteration = i;
        fold(&s, (uint64_t)i);
        if (!round(&s))
            return false;
    }
    clock_t finish = clock();
    if (finish == (clock_t)-1 || finish < start)
        return failure(&s, "clock", "clock() unavailable or wrapped");
    if (printf("{\"case\":\"%s\",\"iterations\":%" PRIu32 ",\"operations\":%" PRIu64
               ",\"checksum\":\"%016" PRIx64 "\",\"clock_ticks\":%" PRIuMAX
               ",\"CLOCKS_PER_SEC\":%" PRIuMAX "}\n",
               name, loops, s.operations, s.checksum, (uintmax_t)(finish - start),
               (uintmax_t)CLOCKS_PER_SEC) < 0 ||
        fflush(stdout) != 0)
        return failure(&s, "stdout", "write/flush failed");
    return true;
}

static bool parse_loops(const char *text, uint32_t *out) {
    if (text[0] < '1' || text[0] > '9')
        return false;
    uint32_t value = 0;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        if (text[i] < '0' || text[i] > '9')
            return false;
        uint32_t digit = (uint32_t)(text[i] - '0');
        if (value > (MAX_LOOPS - digit) / 10u)
            return false;
        value = value * 10u + digit;
    }
    if (value == 0 || value > MAX_LOOPS)
        return false;
    *out = value;
    return true;
}

int main(int argc, char **argv) {
    uint32_t loops = DEFAULT_LOOPS;
    if (argc > 2 || (argc == 2 && !parse_loops(argv[1], &loops))) {
        fputs("usage: benchmark_int_set [LOOPS]; canonical decimal 1..128\n", stderr);
        return 2;
    }
    prepare_keys();
    if (!run_case("dispersed", loops, UINT64_C(0x243f6a8885a308d3), dispersed_round) ||
        !run_case("collision_tree", loops, UINT64_C(0x13198a2e03707344), collision_round) ||
        !run_case("sparse_copy_order", loops, UINT64_C(0xa4093822299f31d0), sparse_round))
        return 1;
    return 0;
}
