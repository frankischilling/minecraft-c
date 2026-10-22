# Systems and subsystems

[MovingPiston singleton state shape](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/tiles/piston.md#singleton-state-shape)
implements all 15 concrete overrides and the base empty body. It holds the actual
Tile and captured carrier, performs real carrier shape dispatch, then reads
progress/extending/facing and writes six singleton fields in source order.
Independent Direction reads and late failures retain completed effects. This
operation leaves pooled bounds untouched. Tick/push, explicit finalization,
neighbor callbacks and standalone bounds have their separate implemented bodies.
Original state-shape runtime equivalence remains unverified.

The following finalization checkpoint predates tick and state-shape integration.

Explicit Piston finalization preserves the raw progress guard, ordered progress
stores, actual World Tile removal, receiver invalidation, current Block query
and notifying carried-ID/metadata restoration. MovingPiston removal invokes the
actual Piston finalizer or inherited container fallback. Held owners preserve
reentrant lifetimes, and errors retain completed effects and distinct source
frames. MovingPiston's neighbor hook reads the current World B flag before any
Tile getter. Its false branch performs real acquiring lookup, preserves the
reached typed failures and lazy invalid-Map cleanup, then releases the nullable
temporary Tile hold. Shared neighbor and different-cell Chest aliases exercise
these effects in fresh local native contracts. Qualified original/native
observations match the enumerated finite field, owner, array and action-prefix
projection on all three compilers. Native-only lifetimes and omitted graph
identities remain separate from Java equality. Tick, pushing, the separate
state-shape update and wider gameplay remain unfinished.

[Item/Falling movement](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/movement.md) operates on existing Entity fields
and the same permanent box. Held pooled copies and collision views retain mutable
identities across repeated queries. Each source call reloads its actual World
receiver, while reached code statics require the exact canonical Entity class
owner. Typed effects, Block inheritance dispatch and actual World queries keep
independent error frames. Other constructors, active ticks, positive pressure/
detector helpers, nonempty sound observers and unowned passenger recursion remain
unfinished. Movement does not implement Piston pushing or the game loop.

The following collision and Piston checkpoint predates Entity movement integration.

The [collision module](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/collision/boxes.md)
preserves actual K List and pooled/permanent box identities through a typed
occurrence ledger. Views retain lifetime while allowing source mutation.
Registered Block shapes, multipart order, captured-array acquisition and real
nearby traversal are implemented for the documented native descendants.
[Positive Piston geometry](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/tiles/piston.md)
holds the actual Tile through field reads, invokes the real carrier getter and
shifts the same pooled box through independently initialized Direction arrays.
Other Entity constructors and movement, active piston ticking, finalization,
pushing and the separate state-shape setter remain unfinished.

The following nearby checkpoint predates collision integration.

The [nearby Entity module](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/collision/entities.md)
captures the actual constructor R List, owns each duplicate Entity occurrence,
and returns held views of live mutable contents. The query reloads current R
after real provider acquisition and at return. Actual Blank dispatch occurs
before normal header and nullable argument access. Clear retains capacity;
terminal retirement releases typed contents without gameplay removal or saves.
World K and concrete collision shapes remain separate unfinished work.

The following bounds checkpoint predates nearby integration.

The [shared bounds module](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/bounds.md)
owns real pooled boxes, cursor reset and client clear through the existing
Entity classes owner. Native holds preserve detached boxes and let aliases
observe slot reuse. Complete movement geometry is implemented; ray/vector/text
helpers, native outer reset routes and Entity physics remain unfinished.

The [snow-layer module](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/snow.md)
implements support, neighbor survival and light melting with actual World
queries and notifying setters. Virtual opacity precedes a separate material
query. Removal reaches the inherited zero-quantity drop without Random draws
or Item Entities. Player harvesting and rendering remain unfinished.

The [falling module](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/falling.md)
implements complete inherited Block hooks and ticks, using the real scheduler,
notifying setters and actual FallingSand allocation/publication. Immutable
canonical subtype identity selects genuine constructors and NBT/provenance;
no Item storage or Math draws substitute for FallingSand. Later Entity movement,
landing and tick descendants remain unfinished.

The [temperature-array module](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/temperature.md)
implements concrete normal/fixed Dimension methods with independent output-array
ownership. Raw temperature writes precede auxiliary allocation and publication;
fixed fills preserve larger suffixes. Actual arrays survive manager replacement
and root retirement through their own holds. The
[normal controller](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/normal.md)
now executes the complete schedule through the actual GAME cache adapter, using
the same provider Random, late statics, actual w and notifying snow writes.
Falling accesses the existing Registry cell. Finite nested failure projections
preserve explicitly carried source sites and causes. Full descendant closure,
natural terrain and original whole-controller qualification remain unfinished.

The [Entity Class and NBT modules](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/nbt.md)
keep passive canonical Class identity in a lower library shared by Registry and
simulation. The actual EntityList namespace owns four separately published
Maps and retains partial initialization failures. Biome handles resolve through
an explicit same-owner binding. Item NBT methods use actual constructed Entity
and Stack owners; factory calls preserve constructor effects before semantic
read. World batch methods retain every occurrence before observer dispatch,
with a separate deferral ledger and complete failure diagnostics.

Ordinary GAME loading uses that same bound factory and the actual direct Chunk
insertion, then attaches independent provenance to the Entity and its current
Stack. A Store-owned occurrence journal distinguishes consumed known records
from unknown factory-NULL inputs. Saving calls every actual Boolean serializer,
including duplicates and false/dead results, before building Tiles and passing
both completed live lists to lower composition. A phase-neutral borrowed NBT
overlay preserves unknown fields without replacing current Stack identity or
introducing a reverse storage-to-simulation dependency.

The [World query module](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/queries.md)
retains the acquired Chunk through each scan, reads actual arrays and materials,
and shares the positive-sky Chunk.a cell across Worlds. Its provider.b getter
preserves pending work on cached hits; Entity publication uses the same getter.
The [plant module](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/plants.md)
dispatches nine actual receiver kinds with distinct placement and survival
bodies, fresh nested causes and source-ordered static, material and opacity
reads. The [plant feature subsystem](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/plants.md)
executes six complete feature families with the actual World and supplied Random,
late fields, quiet setters and preserved partial effects. Complete population
ordering, other features and callback closure remain unfinished.

The [tree feature subsystem](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/trees.md)
implements all five tree bodies through those same World operations. Small
trees borrow the supplied Random; big oak retains its own Random, node rows
and source World binding through reuse, rejection and failure. Its constructor
precedes the separate population height query. Original comparison, the
numeric backend and complete population orchestration remain unqualified.

The [ore/clay](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/ore-clay.md)
and [lake/spring](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/lakes-springs.md)
bodies use actual Registry, World, Chunk and Random owners. Lakes validate before
quiet writes. Springs notify before setting the immediate flag, then invoke the
actual selected tick and preserve reached flags on failure. Recursive fluid
requests retain real E/F records and constructor sequence effects. Complete
population ordering, unsupported descendants and original comparisons remain
unfinished.

The [Dungeon subsystem](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/dungeon.md)
uses notifying writes, retained concrete Chest/Spawner identities and actual
gameplay Stack ownership. Ordinary Item active use and captured-array reads
preserve the loot helper's source order. Its separate native Stack request
budget excludes retained World graphs. Native integration and finite original
helper comparisons pass. The finite whole-placement experiment also matches
all 48 original prepared calls on all three native hosts. Passive Map entry
inspection captures immutable keys separately from mutable Tile coordinates.
Natural population and full closure remain pending.

The [Entity publication methods](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/entities.md)
retain the same Entity in actual ordered World.b and Chunk slices. Duplicate and
cross-World occurrences preserve source identity and construction bindings.
Terminal native disposal validates all leases before releasing any occurrence;
nonempty and terminal Chunks refuse serialization. The [fluid implementation](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/fluids.md)
now continues after real drop publication. Active piston mechanics, fire/portal,
nonempty observers, falling and complete population remain unfinished. Whole
batch activation and deferral now have their own supported native contract.

The [World scheduling foundation](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/scheduling.md#world-insertion)
shares one lazy sequence across Worlds, constructs before every gate and retains
the actual presence and F/E insertion order. Immediate calls dispatch empty tick
bodies and explicitly stop at unimplemented concrete descendants. Retirement
keeps pending records inspectable until final free. Scheduled processing,
fluid/falling closure, full population and scheduled persistence remain unfinished.

The [World mutation foundation](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/mutations.md)
preserves distinct Chunk setter schedules, incremental height/skylight repair,
live registry captures and ordered observer/neighbor calls. Log/leaf removal
and inherited empty hooks are implemented. Other concrete descendants remain
explicit failures; full population and synchronous gameplay closure remain open.

The [Block class construction contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/block/registry.md)
and the [Item construction contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/item/registry.md)
describe the real recursive initialization cycle, 96 Block and 106 named Item
chains, subtype/tool arrays, conditional fire setup and five shared-profile
Random constructions. Ordinary Block initialization now completes its reached
statistics, achievement and recipe dependencies; other entry orders retain
their documented failures. Full startup and gameplay remain open.

The [World construction contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/construction.md)
describes one stable 43-field runtime, ordered profile/clock and metadata
construction, typed Dimension/biome ownership and a native normal provider.
Direct, copy and already-loaded normal paths can complete their represented
constructor tails. Fresh spawn, full registries/subclass bootstrap, population
and live-player saving remain unfinished. The
[Dimension observations](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/dimension.md)
qualify standalone factory/array/table/angle scenarios separately from World
startup.

The [shared World registry](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/registry.md)
is bound before the first allocation without initializing Block. Native borrows
protect its lifetime across RAW, failed and retired Worlds. Material and spawn
queries perform real active use after acquiring the block/top result. Numerical
biome IDs and generator composition remain separate from the concrete
[shared Biome registry](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/biome-registry.md).
That module owns actual objects, spawn lists/Class handles and reference arrays,
and performs the real lookup after climate stores. Class-bound Dimension setup
retains actual normal/fixed managers, independent Registry borrows and source-
ordered reference caches. Retained managers can outlive their Worlds. Live
generation and population joins remain unfinished.

The [concrete World lighting operations](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/runtime-lighting.md)
use current provider presence/getter calls and actual C/A storage. Distinct
owned regions preserve their addresses through merging and list growth. Native
retirement retains C until final free. The
[generated Chunk owner](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/chunk-objects.md)
adopts the original block allocation, retains actual child identities, and runs
live finishing through these World operations. All 38 selected native cases
pass on MSVC/GCC/Clang; full generation and propagation remain unfinished.

The [World block access contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/blocks.md)
adds real acquired metadata reads and quiet writes, normal dirty marking and
blank no-op success. Block-ID handlers, notifying wrappers and complete
registry/bootstrap remain unfinished.

Project-owned runtime code uses C17. Implementations live under
`src/<system>/<subsystem>/`; public headers mirror those paths under `include/mc/`,
and tests mirror them under `tests/`. Create folders with actual implementations
rather than empty placeholders.

| Current module | Responsibility | Runtime dependencies |
| --- | --- | --- |
| `core/java` | Defined integer/cast/Random behavior and bounded Integer-set history | C runtime |
| `core/math` | Initialized float trig lookup and Java cast/compare double floor | Caller-owned table, system double sine during initialization |
| `core/text` | Modified UTF-8 with explicit UTF-16 units | C runtime |
| `storage/nbt` | Owned raw tag trees, original IDs 0..10, bounded read/write | Shared core |
| `storage/compression` | Owned gzip/zlib memory output, explicit limits and checksums | Static zlib |
| `storage/level` | Exact metadata types/defaults and independent NBT construction | Explicit source tree and save clock; no file access |
| `storage/region` | Read-only diagnostics and owned mutable region images | Explicit compressed bytes, timestamps and allocation limits |
| `world/noise` | Original seeded Perlin/octave and 2D simplex/octave sampling | Shared core, caller-owned layers |
| `world/biome` | Original climate processing, float-table biome lookup and retained arrays | Shared core/noise, explicit allocation budget |
| `world/terrain` | Normal density, base interpolation, surface replacement and cave carving | Shared core/noise/math, immutable layers/table, explicit Random and borrowed spans |
| `world/chunk` | Packed voxel arrays, heights, direct skylight and normal finishing | Caller-owned data, borrowed blocks/opacity and explicit world context |
| `world/cache` | Packed-key lookup, acquisition, live save/detach/unload coordination | Caller-owned map/list/pending, stable identities, clock and explicit callback/reference bindings |
| `world/lighting` | Original region merging, ordered queue storage and world admission | Shared Java arithmetic, caller-owned cache/queue/depth |
| `world/generation` | Complete unpublished normal generator entry through finishing | Same-world borrowed biome manager, immutable trig, current materials/opacity and existing lighting context |
| `world/storage` | Stable stored chunks, native files, metadata/session ownership, reservations, leases and individual retirement | `mc_persistence`; explicit roots, borrowed region owner and registered cache/fallback views |
| `world/runtime` | Acquisition/generation/storage/lifecycle composition with stable live world scalars | `mc_persistence`; borrowed store/cache/ordered pending and explicit callback owners |

The shared libraries initialize no window, graphics, audio, networking, assets,
or Java. The raw NBT parser and compression module remain separate interfaces;
callers explicitly compose them. Their raw interfaces imply no world schema
or file write. `mc_persistence` adds the explicit storage composition described
below, depending on the shared storage and world libraries.
Noise sampling preserves original constructor ordering and bulk-specific
behavior; [its interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/noise.md)
and [reference evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/noise.md)
describe the exact tested scope. Normal composition is documented below;
terrain-feature population, propagation and other-dimension generation remain open.

The [math interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/core/math.md)
keeps the 256 KiB table caller-owned and immutable after exclusive initialization.
Lookup methods retain float multiply/phase/conversion order, and the separate
floor helper retains Java narrowing and wrapping. Complete table comparisons
scope the measured native math backends; this is a cave prerequisite.
The separate [simplex interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/simplex.md)
preserves its own floor, wrapped cell sum and independent octave factors. Its
numerical tests do not establish complete biome climate or classification.

The separate [climate/biome module](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/biome.md)
combines three independent seeded simplex channels with original processing and
table lookup. Mutable managers preserve array growth, clear numeric suffixes
and retain biome suffixes under a caller-selected allocation budget. Its
[reference evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/biome.md)
separates numerical fixtures from world startup, terrain and full biome behavior.

The [normal terrain module](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/terrain.md)
constructs all 82 original Perlin layers in order, consumes caller-supplied
processed climate and interpolates a fixed density grid into base blocks.
Constructor state is immutable after initialization; workspace and outputs are
borrowed. Population and light propagation remain separate from this stage.

The [surface interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/surface.md)
uses those existing layers with current material byte pairs and a separate
mutable Random. It preserves Z/X/Y traversal, unconditional bedrock draws and
sea/sandstone state transitions. Field sampling and prepared-field application
can also run independently. The normal generator below composes this pass.

The [normal cave interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/caves.md)
borrows an immutable math table, existing block prefix and separate cave Random.
It preserves the original range-8 neighbor schedule, local path draw order,
water screening, selective carving, lava and grass repair. Checked preflight
precedes mutation; successful calls allocate nothing. Private room/path helpers
are shared by normal runtime and focused reference adapters.

The [chunk data interface](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/chunk.md)
keeps blocks borrowed and embeds separate metadata, sky, block-light and height
arrays. Checked nibble access preserves the opposite four-bit value. Local
height/direct-skylight rebuilding retains the original boundary and repeat-pass
behavior without allocating memory. The finishing boundary below adds
world-height queries, lighting admission and dirty state; lifecycle composition
has the separate boundary below.

The [light-region queue](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/lighting.md)
preserves the newest five-entry search, in-place merges and original wrapped
volume arithmetic. Caller-owned storage reports a smaller capacity failure and
the original million-entry abort separately. World admission and propagation
remain outside this module.

The [finishing contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/finishing.md)
composes local height/direct sky rebuilding with all 1024 ordered world-height
queries, admitted sky intervals and the final dirty effect. Cache lookup retains
the original packed coordinate collisions and unsigned height bytes. Explicit
publication replaces the map value and appends the same identity to a separate
insertion list. The separate
[acquisition coordinator](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/cache.md)
adds load/generation callbacks, active registration and live population gates.
The storage adapter below supplies disk decoding. The lifecycle section adds
synchronous saving/unloads; terrain-feature bodies, tick-driven scheduling and
light propagation remain open.

## Storage foundation

The [world storage contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/storage.md)
defines `mc_persistence` ownership. A stored chunk owns five fixed arrays,
runtime state and retained NBT at stable addresses. Cache maps and insertion
lists borrow its identity; every borrowed reference must be retired before
freeing the owner. Decode enforces exact known types and fixed array shapes.
Normalization uses an explicit frozen registry map and changes only blocks.

The region image writer owns allocation and retained bytes in memory. A separate
native region owner maps signed chunk coordinates to files and local slots,
retains a bounded cache, compresses records and stages checked file replacement.
One world store owns metadata and loaded chunks while borrowing that region
owner. It handles primary/backup metadata loading, session-token checks,
metadata rotation, dimension roots and committed growth accounting. Its adapter
connects real disk loads to the acquisition coordinator; generation is allowed
only for missing records. Corrupt records and opaque entities/tiles cannot be
silently activated.

Unknown metadata and chunk fields remain opaque. This is a C preservation
policy, stronger than some original serializers; it does not implement entity
or player factories. Unsupported edits to opaque records are refused. These
APIs do not provide crash durability, concurrent-writer protection or complete
save/unload scheduling.

At `9ad690d83592f127b9bb39d7233e8eef06fd0029`, the finite storage foundation
passes all 71 selected tests without skips on native MSVC and WSL GCC/Clang,
with Linux ASan/UBSan. Independent platform, source/provenance and copied-world
semantic audits pass. Hosted checks pass 59 tests per compiler and formatting
for 111 C/header files. Both copied-world directions include opaque Boat/Sign
transport; 74 Java-free world-harness guards pass in every suite. See the
[storage evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/storage.md)
and [wiki checkpoint](Storage-and-reference#storage-foundation-checkpoint) for
exact manifests, evidence limits and the remaining simulation/scheduler work.

## Save, unload and retirement

The [lifecycle contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/lifecycle.md)
borrows live cache/list/pending state and explicit callback owners. Forced saving
performs extra-save before eligibility, incremental attempt 24 returns false,
and completed forced saving flushes. Unload selects at most 100 actual pending
keys, detaches and saves retained identities, then removes the retained key and
first matching list occurrence. Provider maintenance returns its own Boolean.

The bounded Integer set preserves the pinned modern JDKs' bucket/tree mutation
history. Membership or insertion order cannot restore its traversal. Native
retirement rescans registered map/list/fallback references and refuses active or
leased chunks. Unpublication alone never frees. External deferred owners need
leases, and callers must end every untracked borrowed view before retirement.

The runtime holds time/spawn in stable storage and overlays fresh metadata for
each save, preserving live SizeOnDisk and loaded last_played. Metadata-first
size lag and serialized LastPlayed remain distinct. The runtime's generation
adapter reserves a slot before running and returns a stable copy owned by the
store. Acquisition later publishes that identity in the cache; loaded NBT stays
intact.
Runtime free refuses nonempty state or active calls/leases and performs no save.
Empty entity/tile bodies are native; nonempty bodies remain explicit external
seams. See [qualification and limits](Storage-and-reference#lifecycle-qualification).

The reference controller retains at most 32768 target mirrors from their first
serialized IDs through a session. Its standalone JDI regression covers debugger
disposal, pre-label records, capacity and duplicate rejection. This verification
machinery does not change native owner retirement or establish Java GC lifetime.
The observed dedicated101 helper intervals include JDI stop time and permit
600 seconds at both validator call sites, matching the controller budget. Other
profiles and control/preparation/constructor intervals retain 180 seconds; each
event still belongs to its own interval. The asset-independent interval
regression exercises these actual checks separately from capture qualification.

## Normal generator

The [normal generator API](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/generation.md)
composes fresh chunk data, direct climate/biome sampling, density/base terrain,
surface replacement, caves and full finishing. `mc_normal_generator_init`
borrows the world's already initialized biome manager and immutable trig table.
The generator retains all 82 terrain layers, fixed scratch and separate
generator/cave Random states. Its separate biome output uses the manager's
allocation owner; direct sampling updates `a/b/c` while preserving manager `d`.
`mc_normal_generator_free` releases only the separate biome output.

Each `mc_normal_generator_generate` call receives that same world's stable
lighting/cache context, all 13 current biome top/filler pairs and current
opacity. Materials remain mutable world inputs; the generator supplies no
default table. The caller owns each fresh result wrapper and block buffer,
including its retained capacity suffix. Results finish before publication, so
world queries see the existing cache. Both coordinates must lie in
`INT32_MIN + 8` through `INT32_MAX - 9`, the finite ordinary cave-scheduler
domain. Checked resource/allocation failures preserve state; a defensive
downstream invariant failure reports an unusable result.

The API is verified for the measured unpopulated entry at
`a08c12056a06261ed01d9f66999459f7ce9543b4`. All 41 selected local tests pass
without skips on MSVC/GCC/Clang, including four-seed comparisons in all three
pairings and complete untouched controls. Linux builds use ASan/UBSan. The
[evidence page](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/generation.md)
records exact manifest/source fingerprints and finite observation conditions.

These observations use client base world `fd`/cache `ok` and server base world
`dj`/cache `jb`. Actual `MinecraftServer` construction uses `dp`/`he`.
The separate retained-provider acquisition comparison observes actual persistent
client and dedicated-owner construction. Its finite constructor/API corpus passes
exact-source MSVC/GCC/Clang checks at `9a50ac67997082c89dbc43cf4274fba1427c1565`.
[Callback evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/cache-callbacks.md)
adds genuine successful loads and natural population in base worlds, retaining
observed effects within the cache/world boundary while C computes transitions.
Expanded exact source `28a55b52c001c4c114a11ca7df844fc1f286d328` passes all 56
selected tests without skips on MSVC/GCC/Clang, with Linux sanitizers, independent
audit and hosted 48-test/89-file checks. That checkpoint predates the storage
foundation above. Full persistence acceptance, terrain-feature bodies,
propagation, other-dimension generation and running-server integration remain open.

The planned game systems include ordered world/simulation updates, remaining
platform sockets/clock/input services, OpenGL 3.3 rendering, audio, original
protocol networking, client sessions, and a dedicated server. Native storage
file helpers exist; these broader game systems are not implemented yet. The server must remain independent of client display/audio
initialization, and shared simulation must retain evidenced original side
differences.

Strict project diagnostics and floating-point rules are configured in CMake.
`.clang-format` specifies four spaces, 100 columns, attached braces and separated definitions; CI
checks project C files with clang-format18. Reference tooling is kept under
`scripts/reference/<subsystem>/` and `tests/reference/<subsystem>/`. Ignored
original research inputs are excluded from formatting and publication.

Read the versioned
[architecture](https://github.com/frankischilling/minecraft-c/blob/main/docs/architecture.md)
and [roadmap](https://github.com/frankischilling/minecraft-c/blob/main/docs/roadmap.md)
for current module details and remaining dependencies. The
[parity matrix](https://github.com/frankischilling/minecraft-c/blob/main/docs/parity.md)
records scenario-specific evidence; a shared-library test is not gameplay parity.
