# Flowing and still fluids

The concrete flowing and still fluid bodies now execute through the actual Block
addition, neighbor and tick dispatch in
[fluids.c](../../src/world/runtime/mutation/fluids.c). This is partial synchronous
gameplay support. Selected Block quantity/item/damage bodies and drop construction
prefixes execute; entity insertion, moving-piston TileEntity lookup, fire/portal
effects and nonempty observers remain dependencies where reached. Population and
complete original/native fluid parity are not established by these bodies alone.

## Source behavior

Each real flowing Block retains its constructor-created `a`, `b` and `c` scratch.
Water and lava use different receivers; Worlds sharing a Registry share those
receivers. Route search captures array references at the original operations and
reloads live entries between recursive calls and spills. It does not replace
scratch with per-call arrays or choose a single route when costs tie.

Raw level reads material before metadata. Source regeneration preserves the
repeated above/below queries and the original center-metadata read. Direction
order is -X, +X, -Z, +Z. Downward writes use the distinct source setters and do not
acquire the horizontal spill's drop behavior. Horizontal spill reacquires the
displaced Block and its metadata before invoking its final drop wrapper.

Flow-to-still conversion quietly replaces ID/metadata, then performs range and
cell observation. Still-to-flow conversion stores `World.o=true`, quietly
replaces, observes and schedules, then stores false only after normal return.
It does not restore an earlier flag on failure. Actual nested addition can
schedule before that conversion's explicit schedule call. Future scheduled ticks
are retained, without inventing a later drain.

The caller's tick Random is distinct from `World.r`. Lava slowdown and still-lava
ignition use the supplied Random. Nested immediate scheduling uses the actual
World Random. Fizz separately reloads that Random for its two float draws, then
uses the shared application Math stream for sixteen double draws. The sound
literal resolves before those float draws; each particle literal resolves before
its two Math draws. Float operations preserve their original rounding points.

Lava contact checks water in -Z, +Z, -X, +X, +Y order and short-circuits. Contact
can fizz even when metadata does not select a replacement. Still-lava ignition
retains the original cumulative coordinate walk and six neighbor material tests.
Any reached unfinished fire, portal or other concrete handler fails explicitly
after its earlier effects.

## World dependencies and ownership

[Solid support](../../include/mc/world/runtime/blocks.h) captures Registry `m`
before acquiring the block, indexes the captured array, accepts a NULL Block as
false, then calls material `h` and the selected Block predicate. It does not use
opacity or a final block-ID property table. Slabs read their live subtype flag.

[Sound and particle calls](../../include/mc/world/runtime/presentation.h) walk the
actual World observer list. An empty list completes without dereferencing the
nullable borrowed String, touching Registry state or consuming Random. Nonempty
untyped observer dispatch remains an explicit dependency. These calls do not
implement rendering or audio engines.

Runtime literals use the existing Registry String pool and allocation ledger.
Resolving them neither initializes Block classes nor imposes a READY gate.
Errors describe the reached runtime allocation, without stale constructor or
initializer attribution.

[Entity class bindings](../../include/mc/world/runtime/entities.h) retain the
exact application's matching side/profile owner before the first World
allocation. Binding does not initialize classes or reset IDs, Math or Random.
The class owner cannot be destroyed while a WorldClasses binding holds it.
Entity publication and nonempty persistence still need their own concrete owners.

Calls hold the exact World through synchronous reentry. Failure retains reached
bytes, flags, scratch, scheduling and random effects. Flat diagnostics keep the
current concrete caller and deepest reached hook origin; they do not impose a
gameplay recursion limit or claim a complete recursive stack trace.

## Reference mapping and validation limits

Authority is the unchanged [client and server binaries](../reference/binaries.md).
The two sides have matching operation order for the following concrete bodies:

| Operation | Client | Server |
| --- | --- | --- |
| Flowing tick and addition | `om.a`, `om.c` | `jc.a`, `jc.c` |
| Flow-to-still conversion | `om.j` | `jc.i` |
| Still tick, neighbor and conversion | `nx.a`, `nx.b`, `nx.j` | `is.a`, `is.b`, `is.i` |
| Common mixing and fizz | `rp.j`, `rp.i` | `kv.i`, `kv.h` |
| World solid support | `fd.h(III)Z` | `dj.e(III)Z` |

The native solid-support test constructs real client and base-server GAME Worlds,
uses loaded populated records, checks genuine Registry initialization reentry,
live predicates and failure order, and performs actual maintenance with disk
reload. Its four region records include two whole-file unchanged limit fixtures.
Those are constructed native contracts. They do not establish original World
equivalence, all fluid branches, nonempty observers or full population acceptance.

Separate [original support and presentation observations](../reference/world-solid-support.md)
completed 56 subject calls per side, including nine genuine prepared World cases.
Their exported Contexts contain array fingerprints. They have no native replay
or complete fluid/population qualification claim.

All 65 selected cases pass on MSVC and Linux GCC/Clang, with Linux address,
undefined-behavior and leak checks. Each host retains the same 634 selected
inputs and 56 products. The fluid test performs 487 checks on MSVC/GCC and 486
on Clang; conditional assertions follow genuine World Random draws. It covers
source/below flow, routes, regeneration, still/flow conversion, immediate reentry,
mixing, supplied versus World Random, actual drop/ignition failure prefixes and
natural scheduler/cache limits on both sides. All sixteen loaded records receive
actual GAME save clocks and LastUpdate 1000 through 1003, then reload through a
separate disk owner.

Solid support passes 4,359 checks and runtime literals pass 4,409. Existing
Entity construction, scheduling and mutation cases pass 1,276, 424 and 218.
Literal cases exercise real entered callbacks, immutable allocation limits,
retained prefixes and fresh diagnostics after a genuine initializer failure.
These are selected native runs, not the complete local-original inventory or
all concrete drop-body executions.

Independent paired source reviews cover fluid bodies, Entity binding and all
69 Block kinds across five drop roles. Drop dispatch does not imply completed
publication: base/crop paths stop explicitly at World insertion PCs 119/160,
and moving-piston lookup stops at PC 14. No successful substitute or allowed-ID
restriction closes those remaining calls.
