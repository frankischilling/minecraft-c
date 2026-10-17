#ifndef MC_WORLD_LIGHTING_QUEUE_H
#define MC_WORLD_LIGHTING_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MC_LIGHT_QUEUE_LIMIT 1000000u

typedef enum mc_light_kind { MC_LIGHT_SKY = 0, MC_LIGHT_BLOCK = 1 } mc_light_kind;

typedef struct mc_light_bounds {
    int32_t min_x, min_y, min_z;
    int32_t max_x, max_y, max_z;
} mc_light_bounds;

typedef struct mc_light_region {
    mc_light_kind kind;
    mc_light_bounds bounds;
} mc_light_region;

/* Original region containment/expansion, including Java int wrapping and
 * extents without an inclusive +1. Bounds are not normalized or clipped.
 * Failure leaves stored unchanged. Null arguments return false. Aliasing
 * stored and requested is allowed. No allocation or world access occurs.
 */
bool mc_light_bounds_merge(mc_light_bounds *stored, const mc_light_bounds *requested);

/* Caller-owned ordered storage. Entries [0,count) are live, oldest first.
 * Keep the queue and its storage disjoint and exclusively owned during calls.
 * Callers may restore an observed baseline by filling entries and setting
 * count <= capacity <= MC_LIGHT_QUEUE_LIMIT; every live kind must be valid.
 * Unused storage has no defined contents. No hidden allocation or globals.
 */
typedef struct mc_light_queue {
    mc_light_region *entries;
    size_t capacity;
    size_t count;
} mc_light_queue;

typedef enum mc_light_queue_result {
    MC_LIGHT_QUEUE_INVALID = 0,
    MC_LIGHT_QUEUE_MERGED,
    MC_LIGHT_QUEUE_APPENDED,
    MC_LIGHT_QUEUE_CLEARED,
    MC_LIGHT_QUEUE_FULL
} mc_light_queue_result;

/* Empty storage (NULL,0) is valid. Invalid arguments leave queue unchanged.
 * Success borrows storage, sets count=0 and preserves all storage bytes.
 */
bool mc_light_queue_init(mc_light_queue *queue, mc_light_region *storage, size_t capacity);

/* Call only after the world's no-sky, recursion, midpoint-presence and blank
 * chunk checks admit a request. With coalesce, inspect at most the last five
 * entries, newest first, counting both kinds; merge the first compatible
 * same-kind region in place. Otherwise append. The original million-entry
 * overflow clears the live count and returns CLEARED, signaling the caller
 * to report the original diagnostic. No propagation or queue drain occurs.
 *
 * A smaller full buffer returns FULL without mutation after attempting merges;
 * callers must provide more storage or report failure, never drop the request.
 * Invalid queue metadata/request kind returns INVALID without mutation. Live
 * entry kinds and borrowed storage validity are caller invariants. requested
 * may point to a live entry; its value is copied before mutation.
 */
mc_light_queue_result mc_light_queue_enqueue(mc_light_queue *queue,
                                             const mc_light_region *requested, bool coalesce);

#endif
