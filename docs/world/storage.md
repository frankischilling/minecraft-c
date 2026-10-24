# World storage ownership

`mc_persistence` composes the shared C world and storage libraries. It has no
Java, graphics, audio or socket dependency. The world-store implementation is
connected to the acquisition coordinator. Native MSVC/GCC/Clang copied-world
and exact-source acceptance passes for the finite foundation scenarios at
`9ad690d83592f127b9bb39d7233e8eef06fd0029`. See the
[checkpoint evidence](../reference/storage.md#exact-source-storage-checkpoint).

The separate [SavedData manager](saved-data.md) owns ID counters, named MapData
values and ordered save occurrences. It copies the store's `data/idcounts.dat`
path and uses the same data directory for named files. ID writes directly
overwrite raw NBT; dirty named values directly overwrite gzip under their own
name. Both routes have separate caught-error and partial-file contracts,
without a session check or metadata rotation. The
[named comparison](../reference/population-named-data.md) retains the finite
default-map scope; original filename and stream-failure behavior, trackers and
gameplay restart remain unqualified or unfinished.

[Minecart inventory and storage](../entity/minecart-inventory.md) now has a
separate native contract for semantic NBT, per-occurrence provenance and genuine
GAME load/save/reload. Its [native validation](../reference/minecart-inventory-nbt.md#native-validation)
keeps the staged native failure control separate from the full loader route.
Existing Item/Falling storage evidence retains its recorded scope and source
binding. Original/native Minecart inventory, NBT and storage equality remains
pending; optional unknown-field preservation is native policy.

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

Passive stored coordinates come from Level even when they disagree with the requested slot.
LastUpdate is ignored. Minimum, active, dirty, never-save and last-save time
begin at zero; a later successful acquisition stamps live world time. Frozen
registry normalization is a separate block-only operation, with no height,
light or dirty mutation.

In the passive storage path, Entity/TileEntity compounds and unknown root/Level children remain opaque. Building
an independent result writes explicit LastUpdate and known current state.
Nonempty opaque lists permit only unchanged arrays,
coordinates and population state. Other edits fail. Normalization of unknown
block IDs can therefore make an opaque record unwritable; the source remains
available instead of being silently replaced. Opaque retention does not run
factories, register world objects or establish subtype persistence.

Actual [live Entity publication](entities.md) is separate from opaque NBT.
Writers refuse nonempty concrete Chunk lists before resetting q or writing a
region. Terminal native disposal does not make those Chunks serializable after
their Entity lists are cleared. No Entity save format or copied-world Entity
interoperability follows from the current insertion methods.

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

Legacy `save_info` forces the McRegion version 19132 in an independent result and writes
the supplied clock as LastPlayed. The retained info's loaded LastPlayed field
does not change. The complete gzip `_new` file precedes checked current/old
rotation. A failed step stops with recovery files retained; it is not a crash
transaction. Opaque Player and unknown fields survive. Player replacement,
removal or introduction is refused at this seam. This intermediate preservation
is stronger than the original fresh serializer and empty-player-list behavior.

The separate typed `save_world_info` path retains the Info's version and uses
its actual no-list serializer and internal wall clock. It accepts that owner's
Player without the legacy pointer guard or a new Dimension read. Session
admission precedes serialization as extra native store policy; the original
one-argument handler has no internal check. Shared gzip/rotation code replaces
only the store persistence view after success. Live Info fields/references stay
independent; reached Player rename survives later codec or file failure.
The [binding record](../reference/world-info-runtime.md) explains the distinct
World/list-save gap.

Dimension 0 uses the world root; -1 uses DIM-1. Storage loads apply a copied
registry map. The generic acquisition adapter permits generation only for missing
records, refuses unreadable data and refuses opaque activation. GAME uses a
separate staged protocol described below. The coordinator owns cache publication,
activation and loaded time stamping; unknown-NBT serialization remains separate
from [live TileEntity ownership](tile-entities.md).
Explicit chunk writes update live SizeOnDisk after commit; save eligibility,
dirty reset, last-save stamps and save/unload scheduling remain caller-owned.
The typed runtime variant instead adds growth only to its supplied READY Info,
without requiring store metadata or changing the legacy store counter. Shared
storage does not broadcast growth to another World's Info.

Each load retains a new stable chunk identity, even for a previously loaded
slot. The default owner limit is 512 chunks, including retained failed stages.
Individual retirement checks exact identity leases and registered references.
Freeing a store requires retiring every cache/list reference first; it does not
save chunks, remove the lock file or clear the borrowed region cache. The
copied registry map must contain only zero or each entry's original index.

Metadata gzip, chunk NBT and the shared region transport have separate limits.
The store's compression limits apply to metadata, while chunk compression uses
the borrowed region owner's limits. These bounded operations do not form one
process-wide allocation budget.

## Staged GAME loading

The internal GAME decoder preserves fixed-shape transport admission while
deferring Entity/TileEntity list semantics. Its actual Chunk starts with q=false.
The Store attaches the same decoded owner and concrete header to its canonical
ownership chain before exposing a stage and one coordinator lease. Native
state/data/array identities remain stable through imports and normalization.

Simulation traverses Entities first through the actual bound Entity namespace
and Item constructor/read. Every normal factory return sets q=true277, including
an unknown-name NULL. Nonnull results enter the same held Chunk directly at289,
then receive independent Entity/Stack provenance. A native ordered journal
records only those completed results. Unsupported constructors and later read,
insertion or provenance failures preserve the reached prefix. Only normal
completion of every Entity allows Chest/Spawner TileEntity factories and local import. Only after
both loops return does the Store normalize those same block bytes and publish
ordinary load success. A wrong-location result stops at the unfinished Region
diagnostic after first imports, preserving them before normalization or a second
builder. Generic decode/load and the old known-empty native route retain their
separate contracts.

The journal is tied to the exact decoded root/List owner and source count.
Known records are consumed; unknown normal-NULL records remain opaque in that
Store only. During GAME save, current semantic records fill consumed baseline
positions in stream order, surplus consumed positions disappear, extra current
records append and unknown records retain relative order. No retained known
payload replaces live state. This is an intermediate native preservation policy;
the original fresh writer discards unknown fields. The journal's own bounded
allocation and the retained document keep separate native budgets.

Every attached failed stage remains owned, capacity-counted and enumerable.
Failure balances only the coordinator lease and disables source continuation.
It preserves source fields, earlier Map/Stack/class effects and independent
external leases. Incomplete stages refuse saving before session or serialization
effects. Pending or failed imports require joint terminal container retirement
before individual owner retirement, even when source active is false. Completed
chunks retain ordinary retirement after unload, subject to their actual leases,
registered references and borrowed-view obligations; the continuing World need
not be terminally retired.

`mc_world_store_free_checked` validates the complete known ownership graph before
releasing anything. Registrations, reservations, leases, invalid owners or live
runtime containers preserve the caller's owner pointer and all resources.
Successful close releases quiescent owners and clears that pointer. The Store
must still outlive borrowing Worlds and unregistered views. The legacy void free
retains its quiescent caller precondition. Neither close path saves or removes
the session file.

Decoder/document and header limits remain separate native budgets. A staged
load is not a whole-process heap limit, original malformed-array repair profile
or complete live Entity/TileEntity save implementation. The separate
[live GAME coordinator](saving.md) now composes supported Chest/Spawner records
and explicit empty live Entities over this same stored owner. Generic opaque
editing APIs retain their separate restrictions.

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
