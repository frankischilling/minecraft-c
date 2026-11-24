# World and Chunk Entity publication

The base World insertion method and normal Chunk insertion method use the real
Entity, World list and eight Chunk slice lists. The public entry points are in
[`runtime/entities.h`](../../include/mc/world/runtime/entities.h). These methods
cover the runtime's constructed Item and [FallingSand Entities](falling.md).
Player-specific sleep, tracking, networking and Entity removal remain unfinished.
[Subtype NBT and canonical Class methods](../entity/nbt.md) join the GAME load/save
loops; complete original interoperability remains unqualified.

## Source order

World insertion reads the Entity's live X and Z, divides by sixteen and applies
the original floor conversion. An absent nonplayer chunk returns ordinary false
without acquiring a Chunk, appending to World.b or invoking a WorldAccess receiver. A present
chunk uses the provider's `b` getter, then its actual virtual add method. A
cached hit preserves that Chunk's pending key; it does not enter the separate
cache `c` method that removes pending work.
Normal completion appends to the current World.b and invokes the WorldAccess hook
before returning true. The live [typed WorldAccess loop](world-access.md) dispatches
`entity_added` after both publications and before dedicated Entity-index
insertion. Untyped or missing reached callbacks fail after both publications.

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
occurrence and invoking its WorldAccess hook. Deferral appends the whole slice to the
constructor-created D list without observing it. Duplicate references own
separate holds; typed NULL values own none. The real empty WorldAccess loop returns
normally. Nonempty typed dispatch visits the current WorldAccess list by index;
callback changes affect the next size/get operation.

Native allocation and occurrence holds are admitted before publishing the
complete append. A later WorldAccess failure preserves every appended occurrence.
This transactional native preparation does not reproduce JDK ArrayList/OOM
timing. Batch methods do not impose an Entity READY, construction-World,
application-class, coordinate or current-Chunk gate. The caller supplies genuine
typed live owners, not arbitrary untyped World list values. Source deferred
processing/removal remains unfinished.

Separate b/D ledgers retain actual occurrences and peak array/scratch bytes.
Activation, acquisition and lifecycle diagnostics preserve the complete batch
child, failing occurrence and independent Chunk/World/WorldAccess positions. TileEntity
and mutation acquisition callers retain that child too. Terminal disposal admits
both ledgers before releasing any owner, even when source projections have been
replaced; it does not free unknown replacement descriptors.

## Ownership and failure

Each slice occurrence and each World.b occurrence retains the same Entity once.
Slice identities and the World.b descriptor remain stable. World.b's typed
ownership ledger records every occurrence separately from the source list. List
growth charges retained and peak replacement storage to the existing World or
Chunk budget. A late World allocation failure preserves the earlier Chunk
publication and membership fields. WorldAccess failure preserves both publications.
Outputs stay unchanged on failure, and diagnostics retain the reached World,
Chunk and WorldAccess operation plus the deepest storage or Entity cause.

The lower world/storage libraries invoke a native Entity release function only
after the simulation layer has established a real retained occurrence. This
keeps the existing library dependency order and does not introduce gameplay
callbacks or a reverse dependency from persistence into simulation.

Base and crop drop paths now invoke World insertion at their original calls 119
and 160. They discard its ordinary Boolean result and release only their local
Entity hold. A genuine insertion failure preserves the nested publication frame.
Horizontal fluid displacement can consequently publish its ItemEntity and
continue the original block replacement and notification sequence.

## World Entity tick diagnostics

The World already clears its private scope before the first dispatch. It uses
that cleared space for the first Mob's diagnostics and reuses the zero action
tail after a completed Mob tick. The next Mob still clears the prefix and
every smaller child field. A non-Mob dispatch or Chunk acquisition invalidates
the reuse. A failed action writes its complete child record before publishing
the failure. Public Mob calls with independent outputs clear their full record.

On Windows x64, the reused tail covers 200,760 of the 363,440 bytes in a Mob
diagnostic. A separate change removes the second clear of the 364,320-byte
World diagnostic during scope creation. These sizes describe the records.

Four alternating Windows MSVC Release pairs measured the native World Entity
loop on the authored 25-Chunk fixture. Each timed interval returned 16 World
ticks. Each build contributed 84 timed intervals per workload. The pairs
matched actor ticks, positions, Random, Chunk data and ownership checks. The
fixture retains the three independent constructor-entropy fields separately.

| Actors | Median before / after (ms/tick) | p95 before / after (ms/tick) |
| --- | ---: | ---: |
| None | 0.0305 / 0.0228 | 0.0438 / 0.0292 |
| 1 Pig | 0.1168 / 0.1087 | 0.2150 / 0.2006 |
| 8 Pigs | 0.7760 / 0.7320 | 1.0388 / 0.9674 |
| 64 Pigs | 6.1740 / 5.9491 | 6.6345 / 6.4483 |
| 1 Zombie | 0.1202 / 0.1099 | 0.2340 / 0.2152 |
| 8 Zombies | 0.8847 / 0.8631 | 1.1939 / 1.1376 |
| 64 Zombies | 6.9863 / 6.8598 | 7.7596 / 7.5863 |

Median time fell in every pair for the empty and Pig workloads. The 8- and
64-Zombie workloads improved in three pairs each. The measurements cover
World Entity ticks. Failed-action, reused-output, reentry and independent
fallback controls passed on Windows MSVC Release and on Linux Clang Debug
with ASan and UBSan.

## Persistence and terminal disposal

The GAME save coordinator resets q and sets it before each actual Boolean Item
serializer call. False/dead results continue and duplicates remain occurrences.
The [save path](saving.md) supplies explicit current Entity and TileEntity record lists
to persistence. Generic Store writers keep their separate nonempty guards.
Opaque retained NBT remains a separate contract and never supplies live state.
The caller-owned numerical storage
interface cannot infer the provenance of arbitrarily copied state. Borrowed
concrete Chunk state must stay within its documented ownership contract.

The passive [terminal admission](terminal-retirement.md) can first check exact
native Entity, TileEntity, provider and static Piston roots in its stated client
GAME scope. It leaves the complete live graph unchanged. Its private client
adapter covers an intact successful selected load; partial loads and the
complete outer destruction coordinator remain unfinished.

`mc_world_runtime_retire_entity_containers` is terminal native disposal, separate
from source removal or unload. Its historical name now covers actual Entity and
[TileEntity ownership](tile-entities.md). Admission first validates the complete World b/c/G
ledgers, D, every bound Chunk slice/embedded TileEntity Map, failed Store stages, other
unsupported collections and all leases.
Any failed admission leaves all occurrences intact. Success marks the World and
Chunk containers terminal before releasing any hold, then releases each actual
occurrence and frees retained list capacity. It preserves q, active flags,
membership scalars and Entity.world. Source operations cannot resume, and Store
serialization refuses terminal containers even after their lists become empty.

The owner can then retire native cache/pending descriptors, World contexts and
Store Chunk owners. External Entities still retain their real construction
World; external TileEntities do likewise. Final World destruction stays busy until
those bindings are released.
No save, callback or apparent unload is used to break the ownership cycle.

## Validation and limits

At the whole-batch checkpoint, activation and deferral passed 353 native checks
per host within all 149 configured MSVC cases and 151 cases on each sanitized
Linux GCC/Clang build, with zero skips and local JDK comparisons enabled. Actual late WorldAccess failure
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
claim. Batch activation has its separate native contract. Original nonempty
WorldAccess traces, concrete renderer/tracker receivers, player descendants, item
ticking and complete Entity save interoperability remain unfinished.
