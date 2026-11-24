# World lighting propagation

`mc/world/runtime/propagation.h` operates on the actual GAME World, its current
ChunkProvider, store-owned Chunks and packed SkyLight/BlockLight arrays. It adds
the saved-light setter, point recheck, nonloading sky-exposure query and queued
region drain to the existing [height and enqueue operations](runtime-lighting.md).
The implementation is shared C17 without graphics or audio dependencies.

## Source operations

The supplied [binary fingerprints](../reference/binaries.md) identify these
paired bodies. Thirty selected method pairs agree instruction for instruction
after explicit class/method mapping and constant-pool index normalization.
Branches, constants, program counters and exception tables remain unchanged.

| Operation | Client | Dedicated server |
| --- | --- | --- |
| Queue drain | `fd.j()Z` | `dj.f()Z` |
| Region propagation | `st.a(Lfd;)V` | `lv.a(Ldj;)V` |
| Saved-light setter | `fd.b(Leb;IIII)V` | `dj.b(Lco;IIII)V` |
| Point recheck | `fd.a(Leb;IIII)V` | `dj.a(Lco;IIII)V` |
| Nonloading sky exposure | `fd.o(III)Z` | `dj.m(III)Z` |
| Point presence | `fd.i(III)Z` | `dj.g(III)Z` |
| Radius presence | `fd.b(IIII)Z` | `dj.a(IIII)Z` |
| Chunk getter | `fd.c(II)Llm;` | `dj.c(II)Lhi;` |
| Normal / blank Chunk | `lm` / `li` | `hi` / `hg` |
| Packed nibble access | `wi` | `ob` |

The audit also covers both enqueue overloads, region construction/merge, area
and provider presence, block/saved-light getters, normal/blank Chunk methods
and Light initialization. This is binary correspondence for the selected
closure. It does not establish executed server propagation or equivalence of
every downstream provider, Registry or WorldAccess implementation.

## Order and ownership

The drain increments the actual World `M` counter after its `M >= 50` gate.
It removes the newest region before invoking it and processes at most 499
regions per call. It returns true when that source budget expires with queued
work remaining. Every entered exit decrements the current `M`, including a
failure. A failed region has already left the queue; its completed writes and
callbacks remain visible.

The popped region stays locally owned through propagation. Nested submissions,
queue clearing and callback reentry cannot retire it. The drain frees it once
its invocation finishes. Other regions retain their existing queue ownership.
Operations hold the same World through synchronous reentry and reject retired
or COMPOSITION owners. Reached RAW, constructing and failed GAME prefixes keep
their actual field behavior.

Region extents and volume use Java int wrapping. The source volume diagnostic
skips regions above 32,768 before the native work budget is consumed. X/Z/Y
traversal preserves separate presence and getter calls, mutable Y bounds and
blank-Chunk gates. Samples run in order -X, +X, -Y, +Y, -Z, +Z. Changed values
reach the saved-light setter before ordered neighboring point rechecks.

A normal Chunk becomes dirty before Light active use and the masked nibble
write. The setter then visits the live WorldAccess list. That notification also
runs for a blank Chunk or an unmatched nullable Light. A missing callback fails
after the completed write prefix. The operation does not substitute Block
placement hooks or silently install an empty callback.

Point recheck preserves the no-sky gate, point presence, intrinsic Sky/Block
hint adjustment, saved-value comparison and one-cell coalescing submission.
The separate sky-exposure query checks border and Y before provider presence;
an absent Chunk returns false without generation. It is distinct from the
acquiring sky-visible query used elsewhere.

## Native bounds and errors

The drain takes an explicit `max_steps` work limit. It counts outer X iterations,
columns and cells, including skipped columns, so wrapped outer loops cannot
bypass it. `SIZE_MAX` selects the largest native bound. Exhaustion returns
`MC_WORLD_STORAGE_LIMIT`, retains prior effects and leaves the failed popped
region removed. It is not a completed drain. Ordinary output remains unchanged
on failure.

Errors retain the reached drain, region, setter, point, sky, presence, provider,
Chunk and WorldAccess sites, with nested query/enqueue/Registry/acquisition
causes. Diagnostic program counters name the client bodies. The selected paired
server bodies have the same counters; downstream server frames remain outside
that correspondence audit. Native owner, allocation and output failures keep
their explicit policy rather than pretending to reproduce general Java
exception or out-of-memory timing.

The default scheduled-entry limit is now 262,144. A measured 17-by-17 startup
grid retained about 114,000 real scheduled entries. The previous 65,536 limit
failed at membership insertion before allocation-byte or provider limits.
The 16 MiB World direct limit, 8 MiB provider direct limit and 512 Chunk slots
are unchanged. Entries are allocated as needed; raising the count bound does
not reserve all records or discard pending work.

## Frontend use and validation

The fixed [World inspection view](../client/world-renderer.md) acquires its full
padded ChunkCache footprint before meshing. It drains lighting to completion,
then builds and uploads all sections. Later population therefore cannot leave
earlier uploaded sections with stale light or block data. This finite view is
separate from ordinary client startup, the Player and dynamic render scheduling.

`world.light_propagation` uses authored resource files and a fresh actual World.
Its cases exercise normal and blank gates, four-bit writes, nullable Light,
live self-removing WorldAccess membership, callback failure after a write,
499-record draining, explicit work exhaustion, oversized diagnostics, nested
queue mutation and 50 nested drains reaching the source recursion guard.
`tools.light_propagation_driver` requires successful process exit, empty stderr
and a positive final check count, with bounded output capture. These tests need
neither original assets nor a JDK.

Finite controlled original comparisons and exact tested build results belong in
the [development status](../status.md). Startup progress, provider maintenance,
Player publication and gameplay have separate coverage there. Lighting alone
does not establish unrestricted play or complete server behavior.

## Light state during the retained single-player replay

Two DISPLAY2 runs from the same saved World used one Windows Release
executable. A Chunk height update wrote skylight 6 at `(-55, 73, -344)` in both
runs. At pre-tick World time 13451, Tall Grass at that position read raw
skylight and Block light from the same Chunk that received the write. Block
light was zero in both runs.

The difference was when the frame loop processed the queued Sky regions. In
one run, a region raised skylight to 13 before the Tall Grass tick. In the
other, the first such change came two World ticks after the plant read
skylight 6. Both updates used a neighbor with skylight 14 and subtracted one
for the cell opacity. At 6, the plant's sky-visibility check failed and its
removal consumed Random draws. At 13, it survived. The changed draw order
shifted later snow attempts from 20 to 21, producing one extra World LCG
advance while the World Random state reconverged by the end of the tick.

The ordinary frame loop drains lighting after its batch of World ticks.
The [paused-frame control](../client/menu-runtime.md#frames-ticks-and-input)
exercises draining across frames with zero World ticks. A Windows run on
DISPLAY2 processed 499 regions in the first paused frame and the remaining
region in the second. The same queue stayed live, World time and Random stayed
unchanged, and saved Block light went from 15 to zero. Replays that compare
later simulation state need to account for when queued lighting reaches a cell.

## Point recheck diagnostics

Propagation asks the ordinary light submission for a diagnostic only on
failure. A failed submit reports its nested cause and the reached point and
queue positions. Completed operations initialize the public result directly.
The native test reaches a missing queue during a recheck, restores the same
queue and verifies the next successful call's canonical diagnostic.

Eight alternating Windows Release pairs measured the drain after the fixed
spring workload. Median time fell from 12.93 to 12.57 ms. Every run matched
the ordered digest of the remaining 393 regions and both light planes across
the four checked Chunks.

## Saved-light query diagnostics

Propagation reuses its own canonical successful query receipt for saved-light
reads. A private wrapper shares the same source body and admission with the
public query. Each successful read restores its changed source PCs; the first
failure retains the complete reached receipt and immediately exits the outer
operation. Reentrant operations own separate frames. Public callers retain
the complete successful diagnostic reset.

This avoids repeated diagnostic initialization and publication without caching
saved values, retaining a Chunk across reads, skipping a GET or changing callback
order. The original region budget, queue behavior, leases and active class uses
remain unchanged.

The [optional CPU benchmark](../../tests/benchmarks/light_drain.md) submits 500
distinct eight-cell SKY regions in nine real saved Chunks. One timed public
drain processes the original 499-region budget, including 27,944 saved-light
queries and 37,924 successful GETs. A controlled Windows comparison changes only
the query/propagation objects over the frozen earlier library closure, using one
shared benchmark object. Across 42 retained samples, median time falls from
3.2821 to 3.0601 ms, a 6.76% reduction. Baseline outliers remain in the record;
the larger mean reduction is not a general speedup estimate.

All declared queue, value, listener, Random, hold and allocation checks pass,
including a real callback failure after successful reads, raw export and later
native work exhaustion. The original propagation controls pass 1,479 checks per
variant with identical Chunk snapshots; updated query controls pass 6,301 checks.
These finite CPU results do not establish a gameplay FPS effect. The later
render-stage partition lost native focus and contributes no timing attribution.
