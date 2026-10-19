#include "mc/core/java/int_set.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define RECORDS 4096u
#define CAPACITY 256u
static unsigned int checks;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct block {
    void *pointer;
    size_t bytes;
    unsigned int releases;
    bool live;
} block;
typedef struct tracking {
    block blocks[RECORDS];
    size_t count, requests, fail_at, release_calls;
    bool refused, invalid, malloc_failed;
} tracking;

/* Actual dirty, suitably aligned malloc storage. There is no private set
 * layout, production fault hook, or alternate set algorithm in this unit. */
static void *allocate(void *context, size_t bytes)
{
    tracking *t = context;
    ++t->requests;
    if (t->fail_at != 0 && t->requests == t->fail_at) {
        t->refused = true;
        return NULL;
    }
    if (t->count == RECORDS) {
        t->invalid = true;
        return NULL;
    }
    void *p = malloc(bytes == 0 ? 1 : bytes);
    if (p == NULL) {
        t->malloc_failed = true;
        return NULL;
    }
    memset(p, 0xa5, bytes == 0 ? 1 : bytes);
    for (size_t i = 0; i < t->count; ++i)
        if (t->blocks[i].live && t->blocks[i].pointer == p) {
            free(p);
            t->invalid = true;
            return NULL;
        }
    t->blocks[t->count++] = (block){p, bytes, 0, true};
    return p;
}

static void deallocate(void *context, void *storage)
{
    tracking *t = context;
    ++t->release_calls;
    for (size_t i = t->count; i != 0; --i)
        if (t->blocks[i - 1].live && t->blocks[i - 1].pointer == storage) {
            t->blocks[i - 1].live = false;
            ++t->blocks[i - 1].releases;
            free(storage);
            return;
        }
    t->invalid = true; /* NULL, foreign or already released: never free it. */
}

static void caller_cleanup(tracking *t)
{
    for (size_t i = 0; i < t->count; ++i)
        if (t->blocks[i].live) {
            free(t->blocks[i].pointer);
            t->blocks[i].live = false;
        }
}

typedef enum kind { ADD, REMOVE, REMOVE_FIRST, CLEAR } kind;
typedef struct operation {
    kind action;
    int32_t key;
} operation;

static size_t script(operation *ops)
{
    size_t n = 0;
    for (int32_t i = 0; i < 80; ++i)
        ops[n++] = (operation){ADD, i * 1024};
    for (int32_t i = 0; i < 80; i += 8)
        ops[n++] = (operation){ADD, i * 1024};
    const int32_t edges[] = {INT32_MIN, INT32_MAX, -1, 1, -1024, 0};
    for (size_t i = 0; i < COUNT(edges); ++i)
        ops[n++] = (operation){ADD, edges[i]};
    for (int32_t i = 0; i < 80; i += 3)
        ops[n++] = (operation){REMOVE, i * 1024};
    ops[n++] = (operation){REMOVE, INT32_MIN + 1};
    ops[n++] = (operation){REMOVE, 123456789};
    for (int32_t i = 0; i < 80; i += 6)
        ops[n++] = (operation){ADD, i * 1024};
    for (unsigned int i = 0; i < 8; ++i)
        ops[n++] = (operation){REMOVE_FIRST, 0};
    ops[n++] = (operation){CLEAR, 0};
    for (int32_t i = 0; i < 48; ++i)
        ops[n++] = (operation){ADD, i * 1024};
    for (int32_t i = 1; i < 48; i += 2)
        ops[n++] = (operation){REMOVE, i * 1024};
    for (int32_t i = 15; i >= 0; --i)
        ops[n++] = (operation){ADD, i * 1024};
    ops[n++] = (operation){CLEAR, 0};
    for (size_t i = 0; i < COUNT(edges); ++i)
        ops[n++] = (operation){ADD, edges[i]};
    for (size_t i = 0; i < COUNT(edges); ++i)
        ops[n++] = (operation){ADD, edges[i]};
    return n;
}

static int equal_sets(const mc_java_int_set *actual, const mc_java_int_set *control)
{
    int result = 1;
    int32_t a[CAPACITY], b[CAPACITY];
    size_t an = SIZE_MAX, bn = SIZE_MAX;
    CHECK(mc_java_int_set_size(actual) == mc_java_int_set_size(control));
    CHECK(mc_java_int_set_copy_order(actual, a, COUNT(a), &an) == MC_JAVA_INT_SET_OK);
    CHECK(mc_java_int_set_copy_order(control, b, COUNT(b), &bn) == MC_JAVA_INT_SET_OK && an == bn);
    for (size_t i = 0; i < an; ++i) {
        CHECK(a[i] == b[i]);
        CHECK(mc_java_int_set_contains(actual, a[i]) && mc_java_int_set_contains(control, a[i]));
    }
    for (int32_t i = 0; i < 80; ++i)
        CHECK(mc_java_int_set_contains(actual, i * 1024) ==
              mc_java_int_set_contains(control, i * 1024));
    const int32_t edges[] = {INT32_MIN, INT32_MAX, -1, 1, -1024, 0, INT32_MIN + 1, 123456789};
    for (size_t i = 0; i < COUNT(edges); ++i)
        CHECK(mc_java_int_set_contains(actual, edges[i]) ==
              mc_java_int_set_contains(control, edges[i]));
    int32_t af = 123, bf = 123;
    mc_java_int_set_status as = mc_java_int_set_first(actual, &af),
                           bs = mc_java_int_set_first(control, &bf);
    CHECK(as == bs &&
          (as == MC_JAVA_INT_SET_OK ? af == bf
                                    : as == MC_JAVA_INT_SET_EMPTY && af == 123 && bf == 123));
    result = 0;
cleanup:
    return result;
}

static int released(const tracking *t, bool retained)
{
    int result = 1;
    CHECK(!t->invalid && !t->malloc_failed);
    CHECK(t->release_calls == (retained ? 0 : t->count));
    for (size_t i = 0; i < t->count; ++i)
        CHECK(retained ? t->blocks[i].live && t->blocks[i].releases == 0
                       : !t->blocks[i].live && t->blocks[i].releases == 1);
    result = 0;
cleanup:
    return result;
}

static int history(bool retained, size_t fail_at, size_t *request_count)
{
    int result = 1;
    tracking t = {0};
    t.fail_at = fail_at;
    mc_java_int_set *actual = NULL, *control = NULL;
    operation ops[320];
    size_t operations = script(ops), failures = 0;
    CHECK(operations <= COUNT(ops));
    CHECK(mc_java_int_set_create(NULL, &control) == MC_JAVA_INT_SET_OK);
    mc_java_int_set_allocator supplied = {&t, allocate, retained ? NULL : deallocate};
    mc_java_int_set_status status = mc_java_int_set_create_with_allocator(NULL, &supplied, &actual);
    if (status == MC_JAVA_INT_SET_ALLOCATION_FAILED) {
        CHECK(fail_at != 0 && t.refused && actual == NULL && mc_java_int_set_size(control) == 0);
        ++failures;
        t.fail_at = 0;
        CHECK(mc_java_int_set_create_with_allocator(NULL, &supplied, &actual) ==
              MC_JAVA_INT_SET_OK);
    } else
        CHECK(status == MC_JAVA_INT_SET_OK);
    /* The owner must use its actual copied allocator after this local object
     * changes, including its copied nonnull release function when supplied. */
    supplied = (mc_java_int_set_allocator){0};
    CHECK(equal_sets(actual, control) == 0);
    for (size_t step = 0; step < operations; ++step) {
        operation op = ops[step];
        if (op.action == CLEAR) {
            size_t requests = t.requests;
            mc_java_int_set_clear(actual);
            mc_java_int_set_clear(control);
            CHECK(t.requests == requests && equal_sets(actual, control) == 0);
            continue;
        }
        if (op.action == REMOVE_FIRST) {
            CHECK(mc_java_int_set_first(control, &op.key) == MC_JAVA_INT_SET_OK);
            op.action = REMOVE;
        }
        int32_t old_order[CAPACITY];
        size_t old_count = SIZE_MAX, old_size = mc_java_int_set_size(actual), requests = t.requests;
        CHECK(mc_java_int_set_copy_order(actual, old_order, COUNT(old_order), &old_count) ==
              MC_JAVA_INT_SET_OK);
        bool present = mc_java_int_set_contains(actual, op.key), sentinel = (step & 1u) != 0;
        bool changed = sentinel, control_changed = !sentinel;
        status = op.action == ADD ? mc_java_int_set_add(actual, op.key, &changed)
                                  : mc_java_int_set_remove(actual, op.key, &changed);
        if (op.action == REMOVE)
            CHECK(t.requests == requests);
        if (op.action == ADD && present)
            CHECK(t.requests == requests);
        if (status == MC_JAVA_INT_SET_ALLOCATION_FAILED) {
            CHECK(op.action == ADD && fail_at != 0 && t.refused && failures == 0 &&
                  changed == sentinel);
            CHECK(mc_java_int_set_size(actual) == old_size &&
                  mc_java_int_set_contains(actual, op.key) == present);
            int32_t order[CAPACITY];
            size_t count = SIZE_MAX;
            CHECK(mc_java_int_set_copy_order(actual, order, COUNT(order), &count) ==
                      MC_JAVA_INT_SET_OK &&
                  count == old_count);
            for (size_t i = 0; i < count; ++i)
                CHECK(order[i] == old_order[i]);
            CHECK(equal_sets(actual, control) == 0);
            ++failures;
            t.fail_at = 0;
            CHECK(mc_java_int_set_add(actual, op.key, &changed) == MC_JAVA_INT_SET_OK);
        } else
            CHECK(status == MC_JAVA_INT_SET_OK);
        CHECK((op.action == ADD ? mc_java_int_set_add(control, op.key, &control_changed)
                                : mc_java_int_set_remove(control, op.key, &control_changed)) ==
              MC_JAVA_INT_SET_OK);
        CHECK(changed == control_changed && equal_sets(actual, control) == 0);
        CHECK(!t.invalid && !t.malloc_failed);
    }
    CHECK(fail_at == 0 ? !t.refused && failures == 0 : t.refused && failures == 1);
    *request_count = t.requests;
    mc_java_int_set_free(actual);
    actual = NULL;
    CHECK(released(&t, retained) == 0);
    result = 0;
cleanup:
    mc_java_int_set_free(actual);
    mc_java_int_set_free(control);
    caller_cleanup(&t);
    return result;
}

static int arguments(bool retained)
{
    int result = 1;
    tracking t = {0};
    mc_java_int_set *existing = NULL, *out = NULL;
    CHECK(mc_java_int_set_create(NULL, &existing) == MC_JAVA_INT_SET_OK);
    bool changed;
    CHECK(mc_java_int_set_add(existing, INT32_MIN, &changed) == MC_JAVA_INT_SET_OK && changed);
    CHECK(mc_java_int_set_add(existing, 1024, &changed) == MC_JAVA_INT_SET_OK && changed);
    out = existing;
    mc_java_int_set_allocator supplied = {&t, allocate, retained ? NULL : deallocate};
    CHECK(mc_java_int_set_create_with_allocator(NULL, NULL, &out) ==
              MC_JAVA_INT_SET_INVALID_ARGUMENT &&
          out == existing);
    mc_java_int_set_allocator missing = {&t, NULL, NULL};
    CHECK(mc_java_int_set_create_with_allocator(NULL, &missing, &out) ==
              MC_JAVA_INT_SET_INVALID_ARGUMENT &&
          out == existing);
    CHECK(mc_java_int_set_create_with_allocator(NULL, &supplied, NULL) ==
              MC_JAVA_INT_SET_INVALID_ARGUMENT &&
          t.requests == 0);
    t.fail_at = 1;
    mc_java_int_set_status status = mc_java_int_set_create_with_allocator(NULL, &supplied, &out);
    bool preserved = out == existing;
    if (!preserved && status == MC_JAVA_INT_SET_OK)
        mc_java_int_set_free(out);
    CHECK(status == MC_JAVA_INT_SET_ALLOCATION_FAILED && preserved && t.refused);
    CHECK(mc_java_int_set_size(existing) == 2 && mc_java_int_set_contains(existing, INT32_MIN) &&
          mc_java_int_set_contains(existing, 1024));
    CHECK(t.count == 0 && t.release_calls == 0 && !t.invalid && !t.malloc_failed);
    result = 0;
cleanup:
    mc_java_int_set_free(existing);
    caller_cleanup(&t);
    return result;
}

int main(void)
{
    for (unsigned int mode = 0; mode < 2; ++mode) {
        size_t requests = 0, ignored = 0;
        if (arguments(mode != 0) != 0 || history(mode != 0, 0, &requests) != 0)
            return 1;
        if (requests == 0 || requests > 1024) {
            fputs("Unexpected finite allocator request count\n", stderr);
            return 1;
        }
        for (size_t failure = 1; failure <= requests; ++failure)
            if (history(mode != 0, failure, &ignored) != 0)
                return 1;
    }
    return printf("Integer-set allocator contracts: %u checks passed\n", checks) < 0 ? 1 : 0;
}
