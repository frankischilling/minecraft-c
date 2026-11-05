#ifndef MC_BLOCK_REGISTRY_REDSTONE_TORCH_LIST_INTERNAL_H
#define MC_BLOCK_REGISTRY_REDSTONE_TORCH_LIST_INTERNAL_H

#include "mc/block/registry/registry.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct mc_registry_redstone_torch_list mc_registry_redstone_torch_list;
typedef struct mc_registry_redstone_torch_record mc_registry_redstone_torch_record;
typedef struct mc_registry_redstone_torch_snapshot mc_registry_redstone_torch_snapshot;

/* An actual captured db.b reference. Capture neither dereferences the object
 * nor snapshots its size/backing. In particular NULL is loaded normally and
 * fails only at a reached List invocation. Live/exclusive classes required. */
typedef struct mc_registry_redstone_torch_list_ref {
    mc_registry_classes *classes;
    const void *object;
} mc_registry_redstone_torch_list_ref;

typedef enum mc_registry_redstone_torch_failure {
    MC_REGISTRY_REDSTONE_TORCH_NONE,
    MC_REGISTRY_REDSTONE_TORCH_LIST_NULL,
    MC_REGISTRY_REDSTONE_TORCH_LIST_INDEX,
    MC_REGISTRY_REDSTONE_TORCH_NATIVE
} mc_registry_redstone_torch_failure;

typedef struct mc_registry_redstone_torch_error {
    mc_registry_status status;
    mc_registry_redstone_torch_failure failure;
    /* JDK17 add increments modCount before reaching a fallible growth. */
    bool mod_count_stored;
} mc_registry_redstone_torch_error;

typedef enum mc_registry_redstone_torch_backing {
    MC_REGISTRY_REDSTONE_TORCH_DEFAULT_EMPTY = 1,
    MC_REGISTRY_REDSTONE_TORCH_OWNED_ARRAY
} mc_registry_redstone_torch_backing;

typedef struct mc_registry_redstone_torch_record_view {
    uint64_t identity;
    int32_t x, y, z;
    int64_t time;
    bool constructed, is_null;
} mc_registry_redstone_torch_record_view;

typedef struct mc_registry_redstone_torch_list_view {
    uint64_t identity, array_identity, allocated_records;
    mc_registry_redstone_torch_backing backing;
    int32_t size, capacity, mod_count;
    bool unused_slots_null;
} mc_registry_redstone_torch_list_view;

/* One permanent native List allocation, at the existing class new0. The
 * no-argument ArrayList constructor uses a tagged, nonnull default-empty
 * identity without another allocation. This tag is not a measured assertion
 * about bootstrap-array aliases across independent game class loaders.
 * Factory preserves permanent-arena failure diagnostics. out starts NULL. */
mc_registry_status mc_registry_redstone_torch_list_create(mc_registry_classes *,
                                                          mc_registry_redstone_torch_list **out);
mc_registry_status
mc_registry_redstone_torch_list_capture(mc_registry_classes *,
                                        mc_registry_redstone_torch_list_ref *out);

/* Actual split new/constructor seam. new zeroes fields and owns one external
 * occurrence, before the caller's World.t. construct performs only the real
 * XU integer/long stores, after that time read. No World/class/clock/Random or
 * callback is hidden here. Caller releases its occurrence on every path.
 * All other gameplay operations leave Registry.last_error unchanged. */
mc_registry_status mc_registry_redstone_torch_record_new(mc_registry_classes *,
                                                         mc_registry_redstone_torch_record **out,
                                                         mc_registry_redstone_torch_error *);
mc_registry_status mc_registry_redstone_torch_record_construct(mc_registry_redstone_torch_record *,
                                                               int32_t x, int32_t y, int32_t z,
                                                               int64_t time,
                                                               mc_registry_redstone_torch_error *);
/* Pure value inspection also represents NULL explicitly. It does not execute
 * a Source getfield: the caller maps a null record at its actual field site. */
mc_registry_status
mc_registry_redstone_torch_record_inspect(const mc_registry_redstone_torch_record *,
                                          mc_registry_redstone_torch_record_view *out);
mc_registry_status mc_registry_redstone_torch_record_retain(mc_registry_redstone_torch_record *,
                                                            mc_registry_redstone_torch_error *);
void mc_registry_redstone_torch_record_release(mc_registry_redstone_torch_record *);

/* Actual owned ArrayList/XU scope, not arbitrary List implementations. Known
 * List objects remain live through field rebinding until Registry retirement.
 * Unknown/foreign receiver storage is refused without dereferencing it.
 * add accepts a same-owner constructed XU or NULL. A normal return is true;
 * native growth refusal retains the already stored modCount, old contents and
 * caller's XU occurrence. Native resource limits are not Java OOME claims.
 * List-index errors are Source IndexOutOfBoundsException, not array AIOOBE.
 * Ordinary outputs remain unchanged on failure. Native arguments/results and
 * error storage must be live, exclusive and mutually disjoint from owners. */
mc_registry_status mc_registry_redstone_torch_list_add(const mc_registry_redstone_torch_list_ref *,
                                                       mc_registry_redstone_torch_record *,
                                                       mc_registry_redstone_torch_error *);
mc_registry_status mc_registry_redstone_torch_list_size(const mc_registry_redstone_torch_list_ref *,
                                                        int32_t *out,
                                                        mc_registry_redstone_torch_error *);
/* Borrow only until the next mutation of this List. Retain explicitly before
 * keeping an object across that mutation. A stored NULL returns normally. */
mc_registry_status mc_registry_redstone_torch_list_get(const mc_registry_redstone_torch_list_ref *,
                                                       int32_t index,
                                                       mc_registry_redstone_torch_record **out,
                                                       mc_registry_redstone_torch_error *);
/* Actual remove(0), no shrink/allocation. The removed membership transfers to
 * one external return occurrence; release at the ignored Source-result pop.
 * out starts NULL; a removed NULL needs no release. */
mc_registry_status
mc_registry_redstone_torch_list_remove_first(const mc_registry_redstone_torch_list_ref *,
                                             mc_registry_redstone_torch_record **out,
                                             mc_registry_redstone_torch_error *);
mc_registry_status
mc_registry_redstone_torch_list_inspect(const mc_registry_redstone_torch_list_ref *,
                                        mc_registry_redstone_torch_list_view *out,
                                        mc_registry_redstone_torch_error *);

/* Native-only immutable shallow snapshots. Each slot retains its actual XU,
 * including duplicate references, while metadata keeps logical List/array IDs.
 * No Source List copy, clock call, class use or hash/entropy input occurs.
 * Empty snapshots still own a Registry occurrence. out starts NULL. */
mc_registry_status
mc_registry_redstone_torch_snapshot_copy(const mc_registry_redstone_torch_list_ref *,
                                         mc_registry_redstone_torch_snapshot **out,
                                         mc_registry_redstone_torch_error *);
mc_registry_status
mc_registry_redstone_torch_snapshot_inspect(const mc_registry_redstone_torch_snapshot *,
                                            mc_registry_redstone_torch_list_view *out);
mc_registry_status
mc_registry_redstone_torch_snapshot_at(const mc_registry_redstone_torch_snapshot *, size_t index,
                                       mc_registry_redstone_torch_record_view *out);
void mc_registry_redstone_torch_snapshot_release(mc_registry_redstone_torch_snapshot *);

/* Quiescent final Registry cleanup, before managed/permanent arenas. External
 * record/snapshot occurrences make classes_free BUSY before any retirement.
 * No Source reset, World key, callback, constructor or failure replay occurs. */
void mc_registry_redstone_torch_lists_retire(mc_registry_classes *);

#endif
