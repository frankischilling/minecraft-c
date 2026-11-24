# Normal chunk finishing

The [concrete generated Chunk interface](chunk-objects.md) provides the GAME
constructor and live finalizer. This page describes the separate caller-owned
numerical contract and its preflight guarantees.

`mc_world` provides normal chunk finishing through
`mc/world/chunk/finish.h`, cache storage through `mc/world/cache/cache.h`, and
world height and light admission through `mc/world/lighting/world.h`. These
interfaces connect [local voxel data](chunk.md) with the
[ordered light queue](lighting.md).

## Ownership and cache visibility

A caller-owned `mc_chunk_state` gives a chunk stable identity, signed chunk
coordinates, a borrowed `mc_chunk_data`, and explicit blank and dirty flags.
Keep its identity stable while cache entries or insertion records refer to it.
A blank object may have no data; height reads return zero for it. Full normal
finishing requires a nonblank object with valid mutable block storage.

`mc_chunk_cache_init` borrows a map-entry array and a separate ordered array of
chunk pointers. Empty `(NULL,0)` arrays are valid. Initialization changes only
the cache descriptor. `mc_chunk_cache_validate` checks capacities, nonnull valid
objects, and uniqueness of the live packed keys. Imported storage must pass
validation before use.

The original key packs each coordinate's sign and low 15 bits into one signed
32-bit integer. Distinct coordinates can therefore alias: `(0,0)` and
`(32768,0)` have the same key. Lookup uses that key without checking the mapped
object's coordinates. It reports absence separately and preserves the output
pointer on a miss.

`mc_chunk_cache_publish` implements the publication phase after generation:
replace or insert the packed-key mapping, then append the supplied identity to
the insertion list. Replacement retains the map entry's position. The old
object remains in the insertion list, which can also contain repeated pointers.
Invalid inputs or insufficient map/list storage leave all state unchanged.

Finish a newly generated chunk before publishing it. Its rebuilt local heights
are invisible to world queries until publication. A query can instead resolve
an older cached identity or a packed-key alias. If a previously published
identity is finished again, cache queries see that same object's newly rebuilt
heightmap. No automatic publication occurs inside finishing.

These operations implement neither loading nor generation. Present-null cache
entries are unsupported because the original cache can load or generate when
it resolves them. They are rejected rather than treated as missing or blank.
Pending unload sets, lifecycle hooks, population and Java map iteration order
are outside this storage interface.

## World heights and light admission

`mc_world_height` accepts signed block X/Z coordinates and reads the selected
cached chunk's unsigned height byte. It returns zero for missing or blank
chunks and for coordinates outside the original border. That border accepts
X/Z at least `-32000000`, X below `32000000`, and Z through `32000000` inclusive.
Negative chunk addressing uses Java's arithmetic right shift; local coordinates
use the low four bits. Invalid cache objects or a null output return false,
preserving the output.

A caller-owned `mc_world_lighting` borrows the cache, queue and shared enqueue
depth counter, plus a no-sky flag. Worlds belonging to one runtime share the
same depth owner. `mc_world_lighting_validate` checks the complete imported
cache and live queue kinds. All mutable objects and borrowed storage require
exclusive access and the documented disjoint lifetimes.

`mc_world_light_submit` skips sky requests when no-sky is set. Otherwise it
increments the shared Java integer depth and skips exactly when the incremented
value equals 50. It restores the prior depth on every result. Each horizontal
midpoint adds its two endpoints with Java integer wrapping, then divides by two
with truncation toward zero. The midpoint's packed cache key must be present
and resolve a nonblank object before the queue accepts the request.

Midpoint presence applies no world X/Z border check. It is a separate operation
from the bounded world-height read. Region bounds, including Y, are retained
without normalization. Admitted requests use the queue's coalescing and overflow
rules. `FULL` reports a storage failure; `CLEARED` requires the caller to report
the original queue-abort diagnostic.

## Finishing and tracing

`mc_chunk_finish_normal` first rebuilds all local heights, minimum height and
direct skylight using the supplied opacity table. It then visits columns with
X outermost and Z innermost, querying west, east, north and south in that order.
All 256 columns participate, giving exactly 1024 world-height reads.

If the returned world height differs from the local height, finishing submits
a sky interval between those heights, including both stored endpoints. A
rejected request does not suppress the chunk's dirty effect. Finishing always
sets dirty on success, including no-sky worlds and equal-height contexts.
Coordinate multiplication, addition and neighbor offsets retain Java wrapping.

An optional `mc_chunk_finish_trace` borrows space for at least 1024
`mc_chunk_height_query` records. Each stores the queried X/Z, local height and
actual world-height result. Success replaces the trace count with 1024 and
preserves its capacity suffix. Passing `NULL` disables tracing. An optional,
disjoint `queue_aborts` output receives the number of original diagnostics the
caller must report.

Preflight validates the normal chunk, first 256 nonnegative opacity entries,
world, trace storage and queue reserve before any mutation. A bounded queue
needs 1024 free entries. Full million-entry storage can instead use the original
clear-on-overflow policy. Failure preserves chunk data, dirty state, cache,
queue, depth, trace and abort output. Storage disjointness remains a caller
requirement.

Successful finishing preserves blocks and their capacity suffix, metadata,
block light, opacity, cache membership and insertion order, shared depth, and
unrelated cached objects. Skylight retains skipped positions according to the
local rebuild contract. These functions allocate no memory and perform no I/O,
random draws, publication, population, queue draining or light propagation.

The [reference procedure](../reference/finishing.md) distinguishes constructed
C contract tests, complete fixed observations and live original tracing.
