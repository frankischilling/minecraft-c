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
the [development status](../status.md). Full startup still needs its real
progress owner and post-grid provider maintenance, followed by Player
publication, ticking, controls and gameplay. Lighting alone does not establish
a playable client or complete server behavior.
