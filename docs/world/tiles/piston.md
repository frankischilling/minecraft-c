# Piston Tile and moving collision geometry

The [Tile interface](../../../include/mc/world/runtime/tiles/tiles.h) owns real
Piston instances alongside Chest and Spawner. The [source map](../../reference/piston.md)
records the paired original methods, floating-point observations and remaining
dependencies. Explicit finalization, MovingPiston removal, its neighbor callback
and actual Tile ticking with supported Entity pushing have focused native coverage
on Windows and Linux. Earlier finalization observations match their documented
finite projection. The complete original tick input family is qualified; its C
replay comparison remains pending.

## Construction and class lifetime

Default construction leaves all source fields zero. The full constructor stores
carried Block ID, metadata, facing, extending and render-head flag, without
clamping, binding a World, installing a Tile, dirtying a Chunk or drawing Random.
Both progress fields remain positive zero. Callers keep a native Tile hold across
each operation and any synchronous descendant.

Base Tile initialization publishes the two actual registration Maps before a
Piston active use initializes its separate shared static List. Registering the
Piston Class identity alone does not initialize that List. Its native descriptor
starts empty with capacity zero. A cold-only requested-byte ceiling controls its
allocation; the first failure latches and is not retried. This native capacity
policy does not reproduce a particular JDK's allocation size or OOM timing.

Passive class snapshots expose phases, completed registration counts, List
identity, current backing capacity, raw modification count and requested bytes.
They perform no active use. Multiple Worlds using the same runtime Class owner
observe the same List. Its unused allocated slots are NULL. A borrowed backing
view expires on growth; historical Java array identities are not represented.

A failed push can leave owning Entity occurrences in this List. Native terminal
shutdown may call `mc_tile_piston_class_discard_entities` only after retiring all
Tile owners, with exclusive classes and no later gameplay. This breaks native
Entity/World/class ownership cycles. It is separate from the source clear and
never runs automatically after a tick failure.

## State and storage

NBT read performs base Int `x`, `y`, `z`, followed by Int `blockId`, `blockData`,
`facing`, Float `progress` and Byte-derived Boolean `extending`. Missing fields
use their original defaults. A present wrong type fails at that reached getter,
keeping every earlier assignment. Reading progress assigns current then previous
progress. Read leaves World, invalidation and render-head flag unchanged.

Writer generates nine fields: String `id`, the three coordinates and five
Piston fields. It writes **previous progress**, even when current progress differs.
The render-head flag has no generated field. Native source preservation keeps
unrecognized fields, including an opaque input field with that spelling, through
the existing overlay policy. Piston has no inventory or Spawner name owner.

Semantic fields retain raw binary32 representations, including signaling NaNs.
The normal NBT encoder canonicalizes encoded NaNs as Java data streams do.
Semantic record trees and documents rebuilt through the codec therefore have
different representation scopes. Whole-record provenance, graph budgets,
existing Chunk Maps and World membership Lists use the same actual Tile owner.

## Explicit finalization

The [finalizer interface](../../../include/mc/world/runtime/tiles/piston.h)
implements client `uk.l()` and server `mu.k()`. Previous progress below one
enters the body; one, greater values and every NaN return without a World call.
The guard uses raw binary32 classification without changing NaN payloads.

Entered execution stores current then previous progress as one, removes the
Tile at the current World coordinates, invalidates the finalizing receiver and
independently queries the current Block ID. Only equality with the live moving
Block's ID reaches the notifying carried-ID/metadata setter. Its ordinary
boolean result is discarded. Later failures preserve completed effects.

The finalizing Tile remains held through removal and synchronous reentry.
World removal may fetch a different Tile, so that identity has its own hold and
invalidation. MovingPiston removal now performs its actual Tile lookup and
Piston dispatch, or the inherited container removal body for absent/wrong kinds.
Current and deepest nested finalization/removal PCs accompany the full reached
Tile, query, Registry and setter error records.

Tile acquisition checks the currently reached provider against its actual
World, cache, pending set and Store binding. An unsupported replacement fails
before acquiring through a stale coordinator. This check belongs to the reached
provider call, after any earlier progress stores.

MovingPiston's neighbor callback reads the actual World `B` field first. True
returns before Tile acquisition. False executes the actual acquiring Tile
getter and discards its nullable result after releasing the temporary native
hold. The getter retains its own acquisition, added-hook and lazy invalid-Map
cleanup effects. Null World fails at hook1; reached getter failures retain
hook12 and the full typed Tile cause. The source neighbor ID is unused.

The `world.piston_finalization` contract passes 460 checks per compiler,
including installed owners, aliases, duplicates, ordinary false results,
neighbor guards, different-cell invalid Chest cleanup and retained failure
prefixes. The ordinary-false fixture keeps its shared moving neighbor, so
later notifying restoration executes that real getter. Earlier active replay
matches the 57 finite projections per side/compiler, including source-backed
failure-prefix joins. Full native errors and Java Throwables remain separate.
The separate MovingPiston state-shape operation is described below.

## Tick and Entity pushing

`mc_tile_piston_tick` implements client `uk.n_()` and server `mu.g_()` with the
private `a(FF)` push body. It copies raw current progress to previous before any
floating-point admission or World call. Invalidity adds no early source guard.

Progress at least one pushes with progress one and amount .25 first. It then
removes the current Tile, invalidates the receiver, queries the current Block ID
and compares it with the live moving Block's ID. Equality reaches the notifying
carried-ID/metadata setter; its ordinary Boolean result is discarded. This path
does not force either stored progress field to one.

Other progress values execute separately rounded binary32 addition of .5 and
store the result, including NaN. Only a result at least one is capped at one.
Retracting returns there. Extending pushes at the new progress with the separate
binary32 result `(current - previous) + .0625`.

Push obtains actual carrier geometry before entering the reached World nearby
query. A null box or empty nearby result returns without clearing the shared List.
A nonempty result appends its actual ordered occurrences, including
duplicates, to the class List. A fresh fail-fast iterator traverses that shared
List. Each axis reads its current Direction array and facing, multiplies amount
in binary32, widens to binary64 and passes the three offsets to actual Entity
movement. Clear runs only after normal iteration. A later failure keeps earlier
moves, appended occurrences, progress stores and World effects.

The held Tile survives removal and synchronous descendants. Temporary Entity,
nearby-view and pooled-bounds holds are released without resetting the pool or
undoing gameplay effects. Errors retain separate tick/push PCs and complete
reached Registry, Tile, geometry, nearby, movement and mutation records.

The `world.piston_tick` contract passes 575 checks per compiler. It covers raw
progress and null-World paths, code-owner geometry, all axes, duplicates, real
Item/Falling movement, failed-prefix List history and completion effects. These
are constructed native contracts. Full original tick replay and other Entity
movement dispatches remain unfinished;
[current movement limits](../../entity/movement.md) still apply.

## Singleton state shape

The [Block state-shape entries](../../../src/world/runtime/collision/blocks_internal.h)
implement actual virtual `a(BlockAccess,III)` calls with an actual World as the
BlockAccess receiver. Dispatch covers all 16 paired declarations, including the
base empty body and the inherited powered-rail override. The
[source map](../../reference/piston.md#singleton-state-shape) lists the paired
classes and instruction PCs. Door and trapdoor shapes reuse their actual value
helpers; other carriers retain their original setters and early returns.

MovingPiston obtains and retains the actual Piston Tile through the acquiring
World/Chunk lookup. Absence or a real non-Piston type returns before this body
writes its singleton bounds. The Block array is captured before the Tile's
carried ID getter and signed index check. A null carrier or that same MovingPiston Block
object returns. A different Block object with the same numerical ID receives
its virtual state-shape call. A nonnull carrier at ID zero also receives that call.

The complete carrier call precedes the held Tile's progress, extending and
facing getters. Interpolation at factor positive zero still executes all three
binary32 arithmetic operations. Extending replaces the result with binary32
`1 - progress`. Carried metadata does not replace the actual World's metadata
when a carrier's source shape getter reaches it.

Six stores write `bs`, `bt`, `bu`, `bv`, `bw`, `bx`, or minX, minY, minZ, maxX,
maxY, maxZ. Each store first reads the captured carrier's current field, then
independently reads and indexes the current Direction array. Int-to-float
conversion precedes float multiplication; float-to-double widening precedes
double subtraction. Zero Direction components still take part in arithmetic.

This operation mutates the actual Block singleton fields without obtaining or
resetting pooled bounds. Failure preserves completed carrier mutations,
Direction initialization, singleton stores and World effects. The captured Tile
hold is released on every return; the native error keeps the current
MovingPiston PC and deepest reached Block, Tile or Registry cause. Explicit
code-owner calls admit a null World until a source Tile, Block ID or metadata
invocation reaches that receiver. The singleton arithmetic does not use an Entity
binding.

The expanded `world.piston_geometry` contract includes source setter tuples,
all six directions, extending progress, null and same-object carriers,
numerical-ID aliases, signed-index and null-array failures, partial singleton
stores, carrier mutation before progress failure, exact NaN widening, wrong Tile
types, lazy invalid-Tile Map cleanup and native hold/pool preservation. All 386
geometry checks pass per compiler within the complete public suites described
in [testing](../../testing.md). Original-runtime state-shape equivalence remains
unverified.

## Collision order

MovingPiston's collision getter obtains a held Tile through the actual acquiring
World/Chunk lookup. Absence and real non-Piston kinds yield null. A real Piston
remains held through interpolation at factor positive zero, extending, carried
ID and facing getters, and the shifted-box descendant.

Interpolation clamps only factors greater than one, then evaluates
`previous + (current - previous) * factor` with separately rounded binary32
operations. Factor zero still executes subtraction, multiplication and addition.
The qualified composed NaN selection is left for subtraction and right for
multiplication/addition, preserving sign and quieted payload. This is the
captured interpreter's tested Piston body, not a general Java float ABI.

The shifted helper returns null for carried ID zero or its own moving Block ID
before reading the Block array. Otherwise it captures that array and invokes
the selected Block's actual virtual getter. Genuine null shapes return before
Direction active use. Stored metadata is not substituted into the carrier.

For a real box, independent `wj/od` Direction initialization creates four actual
six-int arrays. Each coordinate reloads its current array reference and performs
the reached signed index check. Mutation order is minX, maxX, minY, maxY, minZ,
maxZ. Each offset uses int-to-float, float multiplication, float-to-double and
double subtraction on the same pooled box. Zero components still multiply
infinities. A later failure keeps earlier coordinate and pool effects.

The [World collision collector](../collision/boxes.md) can retain that same box
in `K`. Pool reset may overwrite a held box; holds protect lifetime, not values.
The native error record keeps enclosing collision, shifted-helper, Tile and
Direction frames separate through reached failures.

## Verification scope

Fresh builds pass all 169 MSVC and all 171 tests on each GCC/Clang build, with zero skips.
The configured original-JAR options remain off. Existing
per-test deadlines and output policies remain unchanged. The Linux runs use an
exact 940-file source copy on Linux storage; the source and original working
files match before and after execution. [Testing](../../testing.md) records the
mounted-source timeout controls and limits.

The focused checkpoint before the state-shape addition passed nine selected
contracts on MSVC and GCC/Clang with Linux ASan/UBSan/leak checks: 4,751 bounds,
575 tick, 460 finalization, 296 Piston Tile, 144 geometry, 218 mutation,
732 collision, 544 movement and 202 World dependency checks per host.
Independent audits join that checkpoint's source, tools, fresh products,
complete streams and actual process closures. These builds
registered 167/169/169 cases and executed only the selected nine; they do not
establish a current full-suite or hosted result.

The separately qualified original tick capture admits all 114 ordered cases and
228 before/after snapshot phases per side. Native/original tick comparison is
pending. Its full graphs and original action throws remain retained. Intact-field
MSVC diagnostics match input/action classifications and selected progress bits,
but malformed observer records and duplicate keys prevent full protocol
qualification. Observation qualification does not establish C equality.

The earlier committed `85441fe` checkpoint passed all 168 MSVC and 170 GCC/Clang
configured cases without skips, plus formatting for 479 C files and headers,
controller mirrors and lifecycle checks. Those historical hosted results
predate the neighbor callback. Original finalization/removal observations cover
57 ordered inputs per side. Independent native qualification verifies 4,809
before/after file-pin joins. Finite comparison matches all 342 rows, 684
snapshots, 2,100 complete cached-array images and 1,050 full delta lists across
six side/compiler processes. The [source map](../../reference/piston.md#active-finalization-family)
records the compared projection and its material omissions; whole-graph parity
and constructor/generation history are not established.

`block.directions` checks array values, separate initialization, failed prefixes,
captured identities and native retirement. `world.piston_geometry` exercises all
six faces, pool aliases, late failures, exact exceptional coordinates and both
extending states through installed Tiles and real World collision collection.
`world.piston_tiles` exercises ordered NBT, native ownership and actual Store
write/readback with unknown-field retention.

The earlier geometry checkpoint passed all 165 MSVC and 167 GCC/Clang configured
cases without skips. Its three new cases passed 295 Direction, 296 Tile and 144
geometry checks per host. Linux uses ASan/UBSan/leak checks and strict
floating-point compilation.

The [interpolation replay](../../../tests/tools/world/tiles/compare_piston_interpolation.py)
and [geometry replay](../../../tests/tools/world/collision/compare_piston_geometry.py)
consume completed retained original captures and qualified native executables.
Their reports state exclusions and retain all raw process streams. Original
source-unused-World getter comparisons and constructed nonnull-World contracts
remain distinct; they do not establish complete original World traversal or
active piston mechanics.
