# Piston Tile and moving collision geometry

The [Tile interface](../../../include/mc/world/runtime/tiles/tiles.h) owns real
Piston instances alongside Chest and Spawner. The [source map](../../reference/piston.md)
records the paired original methods, floating-point observations and remaining
dependencies. Explicit finalization has constructed native coverage on Windows
and Linux; active original comparison remains pending. Active piston ticking
and Entity pushing remain unfinished.

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
identity and requested bytes. They perform no active use. Multiple Worlds using
the same runtime Class owner observe the same List. Class retirement waits for
all Worlds, Tiles and borrowed literal owners; geometry never puts Entities in
the static List.

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

The `world.piston_finalization` contract covers installed owners,
aliasing, list duplicates, ordinary false results and retained failure prefixes.
It passes in fresh hosted MSVC and sanitized GCC/Clang builds. The ordinary-false
fixture retains a moving Block at an isolated interior coordinate, so a later
restoration does not enter its unfinished neighbor hook. Existing interpolation
and geometry replays do not qualify this active body; tick, pushing, neighbor
and state-shape work remain separate.

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

Fresh hosted builds pass all 168 MSVC and 170 GCC/Clang configured cases without
skips, including `world.piston_finalization`. All 479 C files and headers pass
the requested format. Controller mirrors and lifecycle checks also pass. Full
local suites and active original finalization/removal comparison remain unrun.

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
