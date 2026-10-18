# World storage ownership

`mc_persistence` composes the shared C world and storage libraries. It has no
Java, graphics, audio or socket dependency. The world-store implementation is
connected to the acquisition coordinator. Native MSVC/GCC/Clang copied-world
and exact-source acceptance passes for the finite foundation scenarios at
`9ad690d83592f127b9bb39d7233e8eef06fd0029`. See the
[checkpoint evidence](../reference/storage.md#exact-source-storage-checkpoint).

## Stable chunks

[`storage/chunk.h`](../../include/mc/world/storage/chunk.h) owns the runtime
state, five fixed arrays and original NBT document at stable addresses. Cache
maps and insertion lists borrow that state. Retire every such reference before
freeing its owner; replacement does not make an older list entry safe to free.

Decode requires `Level` and `Blocks`, exact known types, 32,768 block bytes,
three packed arrays of 16,384 bytes each and 256 height bytes. Rejecting missing/short/oversized
arrays is an explicit C malformed-input restriction. Original getters return
empty arrays and do not trigger the apparent null-repair branches. C never
invents finishing or rebuilt lighting to conceal that difference.

Coordinates come from Level even when they disagree with the requested slot.
LastUpdate is ignored. Minimum, active, dirty, never-save and last-save time
begin at zero; a later successful acquisition stamps live world time. Frozen
registry normalization is a separate block-only operation, with no height,
light or dirty mutation.

Entity/tile compounds and unknown root/Level children remain opaque. Building
an independent result writes explicit LastUpdate and known current state.
Until live objects exist, nonempty opaque lists permit only unchanged arrays,
coordinates and population state. Other edits fail. Normalization of unknown
block IDs can therefore make an opaque record unwritable; the source remains
available instead of being silently replaced. Opaque retention does not run
factories, register world objects or establish subtype persistence.

## Native region transport

[`regions.h`](../../include/mc/world/storage/regions.h) maps full signed chunk
coordinates to `region/r.<x>>5>.<z>>5>.mcr`, using each coordinate's low five bits
for the local slot. One explicit owner can serve several dimension roots.
Cache keys retain stable lexical paths, without canonicalization. Callers must
avoid aliases and concurrent external writers.

A live hit retains its image. A miss at 256 entries clears the whole cache;
C strong ownership replaces the original GC-dependent SoftReferences. Reads
can create a missing region directory and fresh header, as the original
read/write route does. Existing malformed files are never repaired by reading.
The diagnostic reader may still select a valid record from a partly malformed
file. Unsafe whole images cannot be rewritten.

Read returns an independently owned decompressed buffer. Write compresses raw
bytes as zlib, applies the image allocator and stages a sibling file before
checked replacement. Existing temporary-file collisions are never overwritten.
Failure does not publish the changed cached image. Cleanup touches only a file
created by that operation. Growth includes unconsumed new-header bytes and is
returned/reset after a successful write.

Native helpers validate bounded UTF-8 paths, use wide Windows APIs, check file
types, lengths and close results, and preserve outputs on failure. They provide
no containment, symlink exclusion or concurrent-writer protection. Network UNC
behavior has not been validated. No fsync, crash-safe recovery or power-loss
durability is claimed.

## World-store ownership

[`store.h`](../../include/mc/world/storage/store.h) owns metadata and retained
chunks while borrowing the shared region owner. It creates data/optional-player
directories and the eight-byte session token. The constructor boolean controls
players-directory creation, not read-only access. Opening always replaces the
session token, including when that boolean is false. Session checks compare
only the first eight bytes and allow trailing data; they are not OS locks.
Metadata and chunk writes check the token before changing their target files.

Metadata loading tries `level.dat` then `level.dat_old`; `_new` is ignored.
Missing Data defaults successfully. Resource failures stop rather than hiding
behind a backup. Both missing files return `MISSING`; invalid files return an
error instead of implicitly starting a new world. Failed loads retain the
previous metadata owner and borrowed views.

Saving forces the McRegion version 19132 in an independent result and writes
the supplied clock as LastPlayed. The retained info's loaded LastPlayed field
does not change. The complete gzip `_new` file precedes checked current/old
rotation. A failed step stops with recovery files retained; it is not a crash
transaction. Opaque Player and unknown fields survive. Player replacement,
removal or introduction is refused at this seam. This intermediate preservation
is stronger than the original fresh serializer and empty-player-list behavior.

Dimension 0 uses the world root; -1 uses DIM-1. Storage loads apply a copied
registry map. The acquisition adapter permits generation only for missing
records, refuses unreadable data and refuses activation of opaque entities or
tiles. The coordinator owns publication, activation and loaded time stamping.
Explicit chunk writes update live SizeOnDisk after commit; save eligibility,
dirty reset, last-save stamps and save/unload scheduling remain caller-owned.

Each load retains a new stable chunk identity, even for a previously loaded
slot. The default owner limit is 512 chunks, with no individual retirement.
Freeing a store requires retiring every cache/list reference first; it does not
save chunks, remove the lock file or clear the borrowed region cache. The
copied registry map must contain only zero or each entry's original index.

Metadata gzip, chunk NBT and the shared region transport have separate limits.
The store's compression limits apply to metadata, while chunk compression uses
the borrowed region owner's limits. These bounded operations do not form one
process-wide allocation budget.

## Verification limits

`world.stored_chunk` tests ownership, shape/type restrictions, transient
defaults, explicit clocks, opaque edit guards and failure preservation.
`tools.chunk_storage_reference` compares complete original-derived local state
and supported fresh schemas, with separate C preservation/refusal checks.
`reference.chunk_storage` runs fresh genuine worlds and original loaders.

`tools.regions_*` checks owned filesystem scenarios, including nine global
mappings retained from original observations. Remaining transport scenarios
are constructed C contracts. They cover Unicode paths, cache clearing,
gzip/zlib interoperability, complete unrelated/dead-byte preservation, limits,
replacement failures and temporary-file collisions. They do not establish
original GC timing, concurrent mutation or failed-I/O durability.

`tools.world_store_interop` exercises native ownership, fallback, sessions,
rotation, dimensions, accounting and the actual load callback using paired
original-derived inputs and separately constructed C policies. At the accepted source checkpoint, all 71 selected tests pass without skips
on each of MSVC/GCC/Clang, with Linux ASan/UBSan. The separate
`reference.world_storage` profile opens original-created copies in C, opens
C-created containers in both originals and reloads both routes in fresh C
owners. It covers explicit dimensions 0/-1, signed storage boundaries and
opaque Boat/Sign transport. Independent source/provenance, typed-state,
clock/control and byte-preservation audits pass on all three hosts; all 74
Java-free world-harness guards pass as well.

These are finite seed23/API-context results. Entity/player simulation,
every subtype serializer, gameplay save/unload scheduling, dimension-transition
sharing, running-server persistence and crash recovery remain separate required
behavior. See [storage evidence](../reference/storage.md) for exact manifests,
controls, deliberate departures and excluded attempts.
