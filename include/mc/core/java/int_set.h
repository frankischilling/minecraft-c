#ifndef MC_CORE_JAVA_INT_SET_H
#define MC_CORE_JAVA_INT_SET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_java_int_set mc_java_int_set;

typedef enum mc_java_int_set_status {
    MC_JAVA_INT_SET_OK = 0,
    MC_JAVA_INT_SET_EMPTY,
    MC_JAVA_INT_SET_INVALID_ARGUMENT,
    MC_JAVA_INT_SET_LIMIT,
    MC_JAVA_INT_SET_ALLOCATION_FAILED
} mc_java_int_set_status;

typedef struct mc_java_int_set_limits {
    size_t max_entries;
    size_t max_retained_bytes;
    size_t max_allocation_bytes;
} mc_java_int_set_limits;

typedef struct mc_java_int_set_allocator {
    void *context;
    void *(*allocate)(void *context, size_t bytes);
    void (*deallocate)(void *context, void *storage);
} mc_java_int_set_allocator;

/* Defaults: 65536 entries, 8 MiB logical retained storage and 16 MiB peak
 * reservation. Retained storage includes the owner, current table and linked
 * nodes; peak reservation also includes the new node and all replacement
 * tables reserved before growth. Allocator overhead and borrowed outputs are
 * excluded. A caller-retained allocator may keep removed nodes, replaced
 * tables and failed reservations; its callback must enforce any cumulative
 * arena budget separately. Zero budgets are valid. max_entries must not exceed INT32_MAX.
 * These are resource bounds, not a Java initial-capacity/load-factor option. */
mc_java_int_set_limits mc_java_int_set_default_limits(void);
const char *mc_java_int_set_status_string(mc_java_int_set_status status);

/* NULL limits selects defaults. out is required and unchanged on failure.
 * The fresh opaque owner models default Integer HashSet ordering on the pinned
 * JDK17/21 reference profiles: lazy table construction, ordered bucket chains,
 * collision trees and their growth/removal history. Other library profiles,
 * Java6, null/general Object keys, concurrent access and iterator fail-fast
 * behavior are outside this interface. No node/iterator addresses are exposed.
 *
 * Give one owner exclusive access, do not copy it, and keep arguments/results
 * disjoint from its storage and each other. The implementation performs no
 * clocks or I/O. Allocator callbacks run synchronously and must not reenter or
 * mutate the owner or its input/output storage. Release any previous owner
 * before reusing out. */
mc_java_int_set_status mc_java_int_set_create(const mc_java_int_set_limits *limits,
                                              mc_java_int_set **out);

/* The default constructor above uses malloc/free. This constructor requires
 * allocator and allocate; deallocate may be NULL for caller-retained storage.
 * The allocator is copied into the owner and services every owner/table/node
 * allocation and release. It may return uncleared suitably aligned storage.
 * NULL storage is never passed to deallocate. The context and any retained
 * arena must outlive all set operations. out remains unchanged on failure. */
mc_java_int_set_status
mc_java_int_set_create_with_allocator(const mc_java_int_set_limits *limits,
                                      const mc_java_int_set_allocator *allocator,
                                      mc_java_int_set **out);

/* NULL returns zero/false. Queries do not allocate or change traversal state. */
size_t mc_java_int_set_size(const mc_java_int_set *set);
bool mc_java_int_set_contains(const mc_java_int_set *set, int32_t key);

/* changed is required. Duplicate add and absent remove succeed with false.
 * Every failure leaves the set, logical capacity/history and *changed unchanged.
 * Add reserves all required storage before changing any links or capacity.
 * Remove does not allocate and uses the reference Set.remove tree policy. */
mc_java_int_set_status mc_java_int_set_add(mc_java_int_set *set, int32_t key, bool *changed);
mc_java_int_set_status mc_java_int_set_remove(mc_java_int_set *set, int32_t key, bool *changed);

/* Rescan the current buckets on every call. EMPTY preserves *key. A successful
 * read does not remove the value or create a persistent iterator. */
mc_java_int_set_status mc_java_int_set_first(const mc_java_int_set *set, int32_t *key);

/* Copy the complete current order; *count receives its size. count is required.
 * output may be NULL only when capacity is zero. Insufficient capacity returns
 * LIMIT without changing output or count. A suffix beyond the size survives. */
mc_java_int_set_status mc_java_int_set_copy_order(const mc_java_int_set *set, int32_t *output,
                                                  size_t capacity, size_t *count);

/* Clear releases the nodes while retaining the logical table capacity/threshold.
 * Remove and clear never shrink the table. NULL clear/free are accepted.
 * Discard the owner pointer after free. Neither operation performs external
 * lifetime tracking or changes any other set. A NULL deallocate callback
 * leaves released storage with the caller's arena. */
void mc_java_int_set_clear(mc_java_int_set *set);
void mc_java_int_set_free(mc_java_int_set *set);

#endif
