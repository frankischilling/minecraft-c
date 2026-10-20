# World and Chunk Entity publication

The base World insertion method and normal Chunk insertion method use the real
Entity, World list and eight Chunk slice lists. The public entry points are in
[`runtime/entities.h`](../../include/mc/world/runtime/entities.h). These methods
currently cover the runtime's constructed ItemEntities. Player-specific sleep,
tracking, networking, Entity removal and nonempty persistence remain unfinished.
Standalone [Item NBT and canonical Class methods](../entity/nbt.md) are available;
the ordinary GAME load/save loops still require their integration.

## Source order

World insertion reads the Entity's live X and Z, divides by sixteen and applies
the original floor conversion. An absent nonplayer chunk returns ordinary false
without acquiring a Chunk, appending to World.b or invoking an observer. A present
chunk uses the provider's `b` getter, then its actual virtual add method. A
cached hit preserves that Chunk's pending key; it does not enter the separate
cache `c` method that removes pending work.
Normal completion appends to the current World.b and invokes the observer hook
before returning true. The current empty observer list completes. Reached
nonempty untyped dispatch fails explicitly after both publications.

Normal Chunk insertion sets q before reading the Entity. It computes X and Z
again, checks the actual final Chunk coordinates, then computes and clamps Y to
one of the eight slices. The original membership stores precede the selected
list append. A NULL Entity therefore retains q before failing. The wrong-location
diagnostic is an explicit dependency after q; it does not fabricate a successful
continuation. The actual Blank override returns without normal Chunk work.

Neither method deduplicates entries, tests the earlier added flag, rebinds
Entity.world, consumes World.r or imposes an Entity READY requirement. Repeated
references remain separate ordered occurrences. A matching application Entity
can be inserted into another same-side World while retaining its construction
World. Native admission requires the exact bound EntityClasses owner; it does
not treat a foreign type as an ItemEntity.

## Whole-batch activation and deferral

Chunk activation uses the actual World batch method rather than repeated single
insertion. It appends the whole slice to World.b before reading each live input
occurrence and invoking its observer. Deferral appends the whole slice to the
constructor-created D list without observing it. Duplicate references own
separate holds; typed NULL values own none. The real empty observer body returns
normally, while reached nonempty interface dispatch remains unsupported.

Native allocation and occurrence holds are admitted before publishing the
complete append. A later observer failure preserves every appended occurrence.
This transactional native preparation does not reproduce JDK ArrayList/OOM
timing. Batch methods do not impose an Entity READY, construction-World,
application-class, coordinate or current-Chunk gate. The caller supplies genuine
typed live owners, not arbitrary untyped World list values. Source deferred
processing/removal remains unfinished.

Separate b/D ledgers retain actual occurrences and peak array/scratch bytes.
Activation, acquisition and lifecycle diagnostics preserve the complete batch
child, failing occurrence and independent Chunk/World/observer positions. Tile
and mutation acquisition callers retain that child too. Terminal disposal admits
both ledgers before releasing any owner, even when source projections have been
replaced; it does not free unknown replacement descriptors.

## Ownership and failure

Each slice occurrence and each World.b occurrence retains the same Entity once.
Slice identities and the World.b descriptor remain stable. World.b's typed
ownership ledger records every occurrence separately from the source list. List
growth charges retained and peak replacement storage to the existing World or
Chunk budget. A late World allocation failure preserves the earlier Chunk
publication and membership fields. Observer failure preserves both publications.
Outputs stay unchanged on failure, and diagnostics retain the reached World,
Chunk and observer operation plus the deepest storage or Entity cause.

The lower world/storage libraries invoke a native Entity release function only
after the simulation layer has established a real retained occurrence. This
keeps the existing library dependency order and does not introduce gameplay
callbacks or a reverse dependency from persistence into simulation.

Base and crop drop paths now invoke World insertion at their original calls 119
and 160. They discard its ordinary Boolean result and release only their local
Entity hold. A genuine insertion failure preserves the nested publication frame.
Horizontal fluid displacement can consequently publish its ItemEntity and
continue the original block replacement and notification sequence.

## Persistence and terminal disposal

The GAME save coordinator resets q and sets it for the first live occurrence
before its still-unfinished Entity serializer call. Generic Store writers keep
their separate nonempty guards. Neither path writes a live nonempty Entity list
to a region yet. Opaque retained NBT remains a separate contract;
it does not supply a live Entity serializer. The caller-owned numerical storage
interface cannot infer the provenance of arbitrarily copied state. Borrowed
concrete Chunk state must stay within its documented ownership contract.

`mc_world_runtime_retire_entity_containers` is terminal native disposal, separate
from source removal or unload. Its historical name now covers actual Entity and
[Tile ownership](tiles.md). Admission first validates the complete World b/c/G
ledgers, D, every bound Chunk slice/embedded Tile Map, failed Store stages, other
unsupported collections and all leases.
Any failed admission leaves all occurrences intact. Success marks the World and
Chunk containers terminal before releasing any hold, then releases each actual
occurrence and frees retained list capacity. It preserves q, active flags,
membership scalars and Entity.world. Source operations cannot resume, and Store
serialization refuses terminal containers even after their lists become empty.

The owner can then retire native cache/pending descriptors, World contexts and
Store Chunk owners. External Entities still retain their real construction
World; external Tiles do likewise. Final World destruction stays busy until
those bindings are released.
No save, callback or apparent unload is used to break the ownership cycle.

## Validation and limits

Whole-batch activation and deferral pass 353 native checks per host within all
149 configured MSVC cases and 151 cases on each sanitized Linux GCC/Clang build,
with zero skips and local JDK comparisons enabled. Actual late observer failure
retains the whole appended batch, including occurrences after the failing one.
Separate b/D holds, duplicate and NULL occurrences, foreign construction Worlds,
RAW Entities, real allocation ceilings and terminal retirement are covered.
The [Item NBT comparisons](../reference/entity-nbt.md) qualify a separate finite
read/write/factory corpus; they do not replay ordinary World publication.

At the earlier publication checkpoint:

All 66 selected cases pass on MSVC and Linux GCC/Clang, with Linux address,
undefined-behavior and leak checks. Each host retains the same 640 selected
inputs before/after execution and 57 actual products. Publication passes 730
checks per host; fluid checks number 529/528/528 in that order because genuine
World Random branches select additional assertions. Selected resolved tool
executables also match before and after; this does not capture every compiler
runtime or system library.

The native integration test uses eight genuine GAME Worlds across both sides,
real populated loaded records, shared typed Stacks, duplicate occurrences,
cross-World insertion, lower/upper Y clamps and failure prefixes. Immutable
allocation budgets produce actual late World and Chunk growth failures. A real
external lease blocks whole-container disposal. All eight regions are saved
before publication; every byte remains equal to its saved baseline after refused
writes and terminal teardown. Source force-save eligibility still requires dirty
state or a changed time for a clean q Chunk; the test advances actual WorldInfo
time before exercising that refusal.

The fluid test performs its genuine sixteen-record save/unload/disk reload before
publishing a displaced flower. It then checks the same actual Entity in World.b
and a Chunk slice, Stack 37/count 1/damage 0, pickup delay 10, World Random order,
continued water flow and refusal to overwrite the saved region.

Separate [original client and server observations](../reference/entity-publication.md)
record actual constructors, base World insertion and direct normal/Blank Chunk
calls. These captures and the native contracts are separate evidence. There is
no native replay or complete original/native Entity, fluid or population parity
claim. Batch activation has its separate native contract; arbitrary observers, dedicated indexing, player
descendants, item ticking and Entity save interoperability remain unfinished.
