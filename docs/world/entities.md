# World and Chunk Entity publication

The base World insertion method and normal Chunk insertion method use the real
Entity, World list and eight Chunk slice lists. The public entry points are in
[`runtime/entities.h`](../../include/mc/world/runtime/entities.h). These methods
currently cover the runtime's constructed ItemEntities. Player-specific sleep,
tracking, networking, Entity removal and nonempty persistence remain unfinished.

## Source order

World insertion reads the Entity's live X and Z, divides by sixteen and applies
the original floor conversion. An absent nonplayer chunk returns ordinary false
without acquiring a Chunk, appending to World.b or invoking an observer. A present
chunk uses the acquiring provider route, then its actual virtual add method.
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

Runtime and generic Store writers reject actual nonempty Chunk lists before
resetting q or writing a region. Opaque retained NBT remains a separate contract;
it does not supply a live Entity serializer. The caller-owned numerical storage
interface cannot infer the provenance of arbitrarily copied state. Borrowed
concrete Chunk state must stay within its documented ownership contract.

`mc_world_runtime_retire_entity_containers` is terminal native disposal, separate
from source removal or unload. Admission first validates the complete World.b
ledger, every bound Chunk slice, other unsupported collections and all leases.
Any failed admission leaves all occurrences intact. Success marks the World and
Chunk containers terminal before releasing any hold, then releases each actual
occurrence and frees retained list capacity. It preserves q, active flags,
membership scalars and Entity.world. Source operations cannot resume, and Store
serialization refuses terminal containers even after their lists become empty.

The owner can then retire native cache/pending descriptors, World contexts and
Store Chunk owners. External Entities still retain their real construction
World; final World destruction stays busy until those bindings are released.
No save, callback or apparent unload is used to break the ownership cycle.

## Validation and limits

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
claim. Nonempty activation, arbitrary observers, dedicated indexing, player
descendants, item ticking and Entity save interoperability remain unfinished.
