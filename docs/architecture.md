# Runtime boundaries

The [native client](client/startup.md) owns the resource source, shared Registry,
asset cache, font layout and SDL/OpenGL display. The display draws supplied
quads and manages context/texture ownership; it has no World or screen logic.
Image decoding and font layout work without an OpenGL context. Graphics and audio device libraries
remain outside the simulation and server link graph. The default constructed
App now drives the no-World source menu and genuine
[World-selection screen](client/world-selection.md). The
[selected World loader](client/world-loading.md) constructs the canonical source
SaveHandler/World and publishes a real Player and movement input after terrain
preparation. Renderer/particle attachment and real Player publication run.
Remaining child menus and gameplay remain unfinished. A separate
[World inspection](client/world-renderer.md) owns actual World construction,
finite CPU section meshes and independent GPU buffers. It retains no Chunk view
after meshing and does not substitute a camera for a Player.

The separate [normal startup owner](client/normal-startup.md) constructs and
publishes the real source graph through SoundManager binding, nine texture
effects, RenderGlobal/viewport and the particle renderer. It uses canonical
File/stream ownership for Settings saves. External scene/effect occurrences
preserve their App/Engine/Display dependencies through retryable retirement.
The resource worker and initial GUI publication are connected. The production
[menu runtime](client/menu-runtime.md) owns native bootstrap, Timer/frame adoption,
source screen input/ticks and the same-resource drawing bridge. Explicit flags
retain the diagnostic title/World inspection. Synchronous transitions retain
the original input receiver and exact parent identity; old screens keep their
App dependencies through BUSY cleanup. Focused Windows empty/populated save-list
loops pass. DateFormat matches 2,926 JDK17 vectors on Windows and Linux within
its admitted profiles. All 468 selected Windows CTests and formatting for 1,356 files pass;
[earlier hosted results](testing.md#hosted-runtime-checkpoint-de4290b) retain
their own scope. Full source run/PX/render/shutdown parity remains unproven.

The [first-person component](client/first-person.md) reads genuine Player state
and the actual App's late Settings/World fields. Its finite view can draw copied
section geometry through those matrices while retaining its real World. The
source-frame camera now has its own canonical implementation, including
portal/third-person and the W frustum namespace. [World colors](world/colors.md)
feed source clear-color and Sky drawing. The canonical source program consumes
actual fog state; W/SR and real DK bounds drive culling. [Terrain compilation
and drawing](client/renderer-frames.md) remain unfinished. The inspection component retains its separate finite scope.

The [achievement notice](client/achievement-notice.md) constructs its actual
Item renderer, Biped, direct Vec3 positions and two distinct RenderBlocks owners.
The complete private application storage is shared by production and CPU tests.
Entity/render classes live above menu and World lifetimes; the concrete inventory
description callback reloads the same application's current Settings. Runtime
formatted Strings retire on replacement. This construction/state layer requires
no SDL headers or graphics/audio runtime. The no-notice draw branch returns
without GL or clock work; active notice and Item drawing remain unfinished. The
[active Player tick](entity/player-active-tick.md) uses these same application owners.

The [GameSettings owner](client/game-settings.md) keeps options persistence
separate from display and audio. It owns retained UTF-16 Strings, KeyBindings
and binding arrays, and uses the shared Reader/Writer interfaces for real
files. It can build with `MC_BUILD_CLIENT=OFF`. The application loads this owner
after Display and uses its initial scale and anaglyph before title consumers.
Options screens and real effectful receivers remain unfinished.

[MovementInput](client/movement-input.md) borrows the actual live named bindings
and owns its ten key cells. [Entity turning](entity/turn.md) mutates genuine
constructed Entity angle fields with the original double scaling and previous
angle adjustments. [Local Player construction](client/player-construction.md)
owns the real base/Living/Player state, Inventory/Container aliases, Session
Strings and MouseFilters. [Player input attachment](client/player-input.md)
retains the same MovementInput owner and forwards actual key/reset/sneaking
operations. Separate [World publication](world/player-publication.md) retains
each Player occurrence. The application owns a real Session and statistics
writer. The World Entity pass is implemented separately; normal application
World entry and gameplay adoption remain unfinished.

[Material immersion and acceleration](world/material-movement.md) use the same
World, Entity, Block and Material owners. Their Vec3 pool belongs to the shared
Entity classes owner and survives individual Worlds. Pool reset and client clear
are explicit operations; retained vectors prevent class retirement. The reached
fluid and Stairs dispatch preserves virtual calls and sequential velocity stores.
Living travel and the active Player bodies use these same owners. World
scheduling and gameplay adoption remain unfinished.

[Entity environment helpers](entity/environment.md) preserve the actual water,
lava, stored-water, clearance and relative-acceleration operations. The caller
supplies its real Registry or trigonometric context independently of a nullable
World. Item water testing keeps its genuine bounds override; canonical
LocalPlayer collision queries use the two inherited null-box methods. These
helpers neither update the stored water flag nor advance a tick.

[LocalPlayer movement](entity/player-movement.md) extends the actual base mover
with walking, step sounds and complete current Block step-on inheritance.
Fresh sound Strings retain the same identity through WorldAccess callbacks and
free their managed storage after the last hold. Player fall, damage and close
methods retain their reached source prefixes and explicit remaining descendants.
All callers rebuild with the expanded movement/effect error records; Entity,
World and Registry storage layouts are unchanged. [Travel](entity/living-travel.md),
[base environment ticking](entity/base-environment-tick.md),
[inventory updates](entity/inventory-tick.md) and [action bodies](entity/player-action.md)
compose actual descendants. The [application statistics owners](client/player-statistics.md)
provide the concrete dc dispatch. [Active Player ticking](entity/player-active-tick.md)
composes the Player/Living wrappers and local action on the same owner.
[World Entity updates](world/entity-update.md) now invoke those bodies and Item
updates through actual World lists, presence checks and Chunk membership.
World time/block/weather scheduling and the playable client remain unfinished.

The separate [weather descendant](world/weather.md) updates actual WorldInfo
timers, toggles and live rain/thunder interpolation in source order. Its private
scalar kernels preserve the public WorldInfo phase guards. The enclosing World
tick and dedicated-server weather entry remain unfinished.

[Player spawn/reset](entity/player-spawn-reset.md) composes the real size and
collision search bodies before health/death stores. Its shared position helper
preserves the observed two-NaN arithmetic order across compilers. It does not
publish an application Player or complete World entry.

[Item pickup](entity/item-pickup.md) runs Inventory insertion, achievements, sound,
the concrete Player notification and the later Item death check. The
[pickup particle and effect manager](client/particles.md) own real em construction,
four distinct effect Lists and the captured manager lifetime. Application f/p/j
fields preserve the source World/RenderEngine/manager reads; a null manager fails
after particle construction. The selected dn startup operation is implemented,
and adopted by normal startup. Nonempty effect drawing and World/gameplay
frame scheduling remain unfinished.

[Item ticking](entity/item-tick.md) executes hl.w_, inherited environment update,
six-neighbor push-out, movement, friction and aging on constructed Items. Its
three isolated native/original profiles retain their own scope. The client
World Entity pass dispatches the implemented body; application ticking and
the later World scheduler remain unfinished.

The water effect has a CPU animation body on its existing constructor-owned
float/RGBA arrays and shared Math profile. Normal startup registers all nine
effects; the menu runtime invokes their actual frame updates and atlas uploads.
Original GPU correspondence and gameplay rendering remain unqualified.

[Crafting inventories and containers](inventory/crafting.md) own the concrete
Player inventory, crafting grid/result and Player/Workbench Slot graphs.
Synchronous input notifications execute the real sorted recipe scan and publish
fresh preview Stacks. The shared library requires no graphics or audio-device
initialization; its Player input/settings dependencies use plain C owners.
Slot clicks and crafting-output effects remain unfinished.

The [sound resource owners](client/sound.md) retain grouped entries, resolved
file URLs and actual raw/MUS input streams. Their native retirement stays
separate from source close behavior. Optional C Vorbis decoding has no display
or audio-device initialization and remains outside the server link graph.
The [SoundManager](client/sound-manager.md) owns call ordering, shared setup and
nested settings saves. The [Vorbis codec](client/vorbis-codec.md) adds the
original page/buffer policy. The [OpenAL backend](client/audio-backend.md) owns
actual devices, channels, caches and workers; startup adoption and original
device correspondence remain unfinished.

[Dedicated World construction](world/construction.md#dedicated-construction)
selects its cache during the base virtual provider call and retains separate
subclass fields. Its Entity index preserves duplicate-key insertion and typed
holds. The [Server constructor](server/construction.md) owns default fields,
lists and an inert lifetime thread. Full server startup and ticking remain
unfinished.

[Power queries](world/power.md) own the ordered World/support/conductor/output
chain. Concrete Block dispatch and full redstone connectivity keep the receiver's
defining Registry separate from the supplied World. The independent horizontal
class owns its lazy tables and failure prefixes. Detailed held support shares
the public predicate body while retaining complete acquisition errors. Queries
preserve source effects and do not advance redstone state.

[Pressure plates](world/pressure-plates.md) share one real update helper
between contact and scheduled ticks. The Block's defining classes own the pooled
query bounds; the passed World owns the actual entity slices. Category a uses
mutable R, while Living/Player use fresh [Class-query Lists](world/class-entities.md)
and passive same-owner ancestry. Ordered mutation, support-neighbor reentry,
WorldAccess callbacks, sound and scheduling keep their separate failure effects and
diagnostics. The running game loop remains unfinished.

[Rails](world/rails.md) keep their connection-helper List and coordinate owners
separate from actual World state. Shape changes and reconnects use notifying
metadata writes, with ordinary synchronous callbacks and retained partial
effects. Inherited lifecycle and depth-guarded powered search read the live
defining Registry independently of the supplied World. Detector contact/tick
uses real pooled bounds and a fresh Class-query List with constructed
Minecart membership and normal publication. Explicit current/deepest rail sites
retain complete typed lower errors without inferring entry from class names or
PC values. This source and native-contract coverage does not establish
executed-original rail equality or a running game loop.

[Minecart inventory](entity/minecart-inventory.md) owns each actual Stack
occurrence in the constructor h36 or loaded h27 array. Generic Entity NBT calls
reach the concrete late hooks; fresh semantic saves read actual fields in source
order. Optional native storage keeps root generations and per-slot metadata
separate from Stack identity, including aliases and borrowed older arrays. The
[source and validation map](reference/minecart-inventory-nbt.md#native-validation)
records the native cases; original inventory/NBT/storage equality remains pending.

The following movement/collision checkpoint predates Piston tick/finalization
and pressure-plate integration.

[Item/Falling movement](entity/movement.md) operates on existing Entity fields
and the same permanent box. Held pooled copies and collision views retain mutable
identities across repeated queries. Each source call reloads its actual World
receiver, while reached code statics require the exact canonical Entity class
owner. Typed effects, Block inheritance dispatch and actual World queries keep
independent error frames. Other Entity behaviors, concrete sound WorldAccess
receivers and unowned passenger recursion retain their separate limits. The
active LocalPlayer entry is described above. Movement does not implement Piston pushing or the game loop.

The [collision module](world/collision/boxes.md) owns the actual constructor
K descriptor independently of source K. Each typed occurrence retains either
its actual pooled bounds or the Entity owning its permanent bounds. Source
clear, checked overlapping growth and quiescent native retirement have
separate effects. Returned views retain mutable List identity and World
lifetime. Block getter dispatch uses real registered kinds and live singleton
bounds; metadata and multipart descendants preserve their source order.

World traversal captures the actual Block array before each acquiring ID
getter. It then gathers actual nearby results and performs two separate
indexed Entity reads. Item/FallingSand inherit real null virtuals; other
Entity constructors remain dependencies.

[Positive Piston geometry](world/tile-entities/piston.md) keeps the actual TileEntity held
through interpolation and field reads. The shifted helper captures the Block
array and invokes the actual carried Block getter before Direction active use. Six
ordered stores mutate that same pooled box; later failures preserve earlier
coordinate and pool effects. Piston's shared List and Direction's four arrays
initialize independently under their actual class owners. Piston ticking,
finalization, Entity pushing and the separate state-shape setter remain unfinished.

The following nearby checkpoint predates collision integration.

The [nearby Entity module](world/collision/entities.md) owns actual World R
contents through a separate typed occurrence ledger. It captures the constructor
List independently of the current source field. A query clears current R,
uses real nonloading presence and acquiring getters, then captures current R
for each actual normal/Blank gather call. Returned native views lease the
descriptor and World while retaining the List's mutable source contents.
Ordinary clear and terminal native retirement have separate effects.

This supplies the candidate collection needed by later collision work. World K,
concrete Block contribution shapes, Entity movement and FallingSand physics
remain unfinished.

The following bounds checkpoint predates nearby integration.

The [shared bounds module](entity/bounds.md) owns the existing Entity classes
pool, actual temporary identities and native holds that survive client clear.
Movement geometry captures all arguments before possible reuse of its source
slot. Entity's permanent constructor box stays separate. Original outer reset
routes are mapped; native Entity movement and the game loop remain unfinished.

The [shared Biome registry](world/biome-registry.md) stores actual objects,
spawn lists, record/Class references and its mutable table on the existing
class owner. Its direct sampler retains reference arrays and invokes the real
lookup after each climate-cell store. Class-bound Dimension setup retains actual
noise-backed/fixed managers with independent Registry borrows and reference caches.
The [temperature-array module](world/temperature.md) keeps output allocations
independent of manager replacement and preserves raw writes before a later
auxiliary-buffer failure. The [full Overworld controller](world/population/overworld.md)
uses those concrete methods and owns the actual provider w independently of the
current manager. The installed GAME cache adapter uses the same provider Random,
late Registry/Biome statics and notifying snow writes. The shared falling flag
uses the existing Registry cell and real declaring-class initialization.
The [snow-layer module](world/snow.md) keeps virtual opacity separate from the
fresh World material query. Neighbor support loss and saved Block-light melting
use the genuine zero-quantity drop path and synchronous notifying removal.
Each failed call retains its actual material/query child and reached source
frames. Player harvesting and rendering remain separate unfinished operations.

The [falling Block module](world/falling.md) uses the actual scheduler and
notifying setters, with complete immediate and Entity-construction branches.
FallingSand has an immutable canonical subtype identity, genuine constructors
and its own NBT fields; Item storage and Math draws are not substituted.
Its later Entity movement and tick descendants remain separate unfinished work.
Numerical climate APIs keep their separate bounded-buffer contract. Complete
natural population and reached World descendants remain unfinished. Direct
controller errors select one typed child; nested acquisition retains an acyclic
current/deepest-site projection, not an unbounded recursive Throwable graph.

The [concrete World height and lighting operations](world/runtime-lighting.md)
use its current provider, actual C list and class-owned shared A. Regions retain
stable addresses through pointer-array growth and merging. Native retirement
keeps C until final free. [Light propagation](world/light-propagation.md) removes
the newest region before invoking it, retains its detached lifetime through
reentry and writes actual Chunk nibbles before live WorldAccess callbacks.
The fixed inspection view acquires its padded footprint and drains before
building final meshes. Ordinary startup and dynamic rendering remain unfinished.

[World Player attachment](world/player-publication.md) reads actual Info.Player
and invokes the Player-positive publication path. A nonnull saved tag retains
the completed base Entity NBT effects before the explicit missing Living reader;
it is neither cleared nor published after failure. A null tag reaches actual
Player membership, sleeping state, Chunk and World ledgers, then live callbacks.
Each duplicate occurrence owns its own hold. Terminal teardown validates all
typed ledgers before releasing any occurrence.

Project-owned runtime code uses C17. `mc_core` contains numerical compatibility,
the shared Random profile and allocation-free modified UTF-8 text helpers.
Its [counted float parser](core/java-float.md) uses bounded integer arithmetic
to construct binary32 values without CRT parsing or a floating intermediate.
Its profile uses the private `mc_platform_time` library and C17 atomics/threads.
`mc_storage` depends on core and contains raw NBT, gzip/zlib memory compression,
level metadata and region images.
Its independent NBT tree owner keeps immutable source provenance separate from
decoded-document lifetime. `mc_item` builds on storage with stable
[ItemStack fields, holds and serialization](item/stack.md).
`mc_block` owns the distinct [material and palette identities](reference/materials.md)
and their receiver methods. Every world in one material-class lifetime must
borrow the same stable owner; material identity is separate from block IDs.
The lower `mc_entity_classes` library owns passive canonical Class cells and
EntityList namespace metadata. The client xw/em cells append selectors 27/28;
the earlier selectors 22..26 and the 24 EntityList registrations retain their
identities. Registry can retain that same owner for its
Biome handles without depending on concrete Entity constructors or simulation.
See the [Class/NBT lifetime contract](entity/nbt.md).
Passive Entity NBT/load diagnostics also stay below simulation. The borrowed
Compound overlay lives in storage; actual Entity provenance/factory/save calls
stay in simulation. GAME loading adopts directly into the held Chunk, and its
Store-owned journal records completed source occurrences. GAME saving builds
both live lists before lower composition, without invoking simulation from
persistence. See [ordinary Item persistence](reference/entity-game-storage.md).
`mc_registry` builds on that owner and the shared Random profile with actual
[Block](block/registry.md) and [Item](item/registry.md) class construction. It
retains source-ordered static, base and subtype state through their recursive
initialization cycle, including failed prefixes. The same owner executes actual
[Translation](resources/translation.md), [key loading](statistics/keys.md) and
[statistic registration and arrays](statistics/registry.md), followed by real
[achievement construction](statistics/achievements.md), using the reached
[String/null formatter](reference/string-format.md) in core. Its real
[recipe and smelting managers](item/recipes.md) share that lifetime and retain
actual recipe lists, ingredient/output Stacks and crafting-statistic arrays.
Ordinary Item reads use a separate source-facing interface on that same owner:
real active use precedes static selection or current-array capture, while a
later signed element read uses the captured array without reinitialization.
The [Dungeon subsystem](world/population/dungeon.md) uses those reads with real
notifying World writes, held Chest/Spawner identities and new gameplay Stacks.
Its local Stack request budget is separate from retained World ownership.
The [prepared whole-placement comparison](reference/dungeon-placement.md)
matches 48 original calls on all three native hosts, including actual immutable
Map keys inspected independently of TileEntity coordinates. Natural population and
complete closure remain pending.
Ordinary Statistics and Block initialization can return; other entry orders
preserve their actual null-singleton failures. Full World startup and gameplay
binding remain unfinished.
`mc_world_info` owns per-World [metadata fields and shared references](world/info.md)
above storage, with a private platform-clock dependency for actual no-list
serialization. It borrows no store or World runtime. Runtime contexts can retain
an Info and borrow its actual scalar cells; persistence accepts the typed
no-list record without making the store the live metadata authority.
`mc_persistence` composes storage with the C world model and explicit native
filesystem ownership. `mc_world_saved_data` adds [ID counters and named values](world/saved-data.md),
with distinct cache, ordered-list and ID-map owners. It copies the handler path
and ID-key spans; named keys retain immutable UTF-16 String owners. GAME
managers bind the client or server base type and construct the corresponding
MapData class. Values and save records retain shared mutable colors handles,
including actual decoded payloads, without borrowing the supplying Store or
WorldInfo. The [named MapData comparison](reference/population-named-data.md)
covers one default value in a held World. Complete World startup, trackers,
map updates and broader named subtype behavior remain unfinished.
`mc_simulation` builds on persistence and items with the
world runtime and [Entity construction](entity/construction.md). Keeping these
mutually dependent owners together avoids a storage-to-Entity dependency.
The [native base-World constructor](world/construction.md) keeps the same
43-field runtime through RAW allocation, ordered construction, failure and
retirement. Typed WorldProvider/WorldChunkManager owners and the concrete Overworld ChunkProvider
retain actual shared identities; block reads use the installed cache
coordinator. Direct, copied and already-loaded normal construction can finish.
Fresh spawn, full bootstrap/subclass startup and complete population remain
unfinished. [Standalone Dimension observations](reference/dimension.md) have
their own finite scope.
These libraries initialize no display, graphics, audio,
networking, assets or Java. Tests and research helpers are
separate executables/scripts; none is a hidden game engine.

The [float lookup/floor module](core/math.md) uses a caller-owned 65536-float
table initialized once with the measured system double-sine backend. Later
lookups share it as const data; the Java cast/compare floor needs no table.
There is no hidden global initializer or Java dependency in this module.

`mc_world` depends on `mc_core` and implements the original seeded
Perlin/octave [noise module](world/noise.md) and the separate
[2D simplex/octave family](world/simplex.md). The noise primitives own no allocations
or platform services. Caller-supplied layer arrays keep construction and shared-Random order
explicit. Bulk sampling preserves the original height-one path and Y-cell cache.

The [climate and biome module](world/biome.md) composes independently seeded
simplex channels with original climate processing and float-table lookup.
Caller-owned managers retain mutable climate/biome buffers under an explicit
allocation budget. External temperature/direct biome outputs stay separate
from those retained fields; calls stage growth before publishing changes.

The [scheduled-state modules](world/scheduling.md) own records, exact full-key
membership and historical ordered-tree state separately. Shared record identities
survive time changes and membership removal. Current comparison searches and
structural-first lookup remain distinct after time mutation. World scheduling,
admission and tick callbacks remain separate implementation work.

The [Overworld terrain module](world/terrain.md) composes all original seeded
Perlin groups into density, then interpolates base blocks. Immutable constructor
state is separate from borrowed workspace/output spans and processed climate.
Its fixed normal shape retains original flat layouts and buffer suffix behavior.

The [Overworld surface pass](world/surface.md) reuses immutable terrain layers,
accepts current top/filler byte pairs and advances a separate caller-owned
Random. Field sampling and material application remain usable independently;
composed calls validate all borrowed storage before any mutation.

The [Overworld cave pass](world/caves.md) uses a separate caller-owned Random,
immutable math table and existing block prefix. It owns the fixed neighbor
schedule, ordered local path Randoms, geometry and selective material changes.
Checked preflight precedes mutation; successful calls allocate nothing.
Private room/path helpers serve both normal runtime and focused reference adapters.

The [chunk voxel-data module](world/chunk.md) borrows the block buffer and embeds
independent packed metadata/light arrays plus column heights. It provides bounded
nibble access and local height/direct-skylight rebuilding without allocation.
World cache queries, lighting queues, dirty state and chunk lifecycle remain
separate responsibilities.

The [light-region queue](world/lighting.md) owns ordered storage and original
region merging after world admission. Caller-owned capacity makes allocation
explicit. Shared Java helpers preserve wrapped coordinate/volume arithmetic.
The newest five-entry search, in-place merges and million-entry abort policy
remain distinct from world admission and lighting propagation.

The [finishing boundary](world/finishing.md) adds a caller-owned packed-key cache,
world-height lookup, light-request admission and complete normal finishing.
Stable chunk identities let queries see rebuilt heights when that identity is
already cached. Fresh generated chunks finish before explicit publication.
Publication covers map replacement and insertion-list append only; loading,
unloading, lifecycle callbacks, population and light propagation remain separate.

The [Overworld generation module](world/generation.md) composes fresh chunk data,
direct climate/biome sampling, base terrain, surface, caves and full finishing.
`mc_overworld_generator_init` binds the world seed, the world's initialized biome
manager and immutable trig data. Each `mc_overworld_generator_generate` call receives
that same world's lighting context. The generator retains all 82 terrain layers,
fixed scratch and separate generator/cave Randoms. Its separately owned biome
output uses the manager's allocation owner; `mc_overworld_generator_free` releases
only that output. Direct sampling updates `a/b/c`
while preserving `d`. Each call receives all 13 current biome material pairs
and current opacity explicitly. Fresh caller-owned results remain unpublished;
both coordinates must lie in `INT32_MIN + 8` through `INT32_MAX - 9`, the finite
ordinary cave-scheduler domain. Checked resource/allocation failures preserve
state; defensive downstream invariant failures report an unusable result.

Overworld composition is verified at
`a08c12056a06261ed01d9f66999459f7ce9543b4`: all 41 selected local tests pass
without skips on MSVC/GCC/Clang, including four-seed original comparisons and
complete untouched controls. [Generation evidence](reference/generation.md)
records the exact sources, manifest hashes and finite headless conditions.
This checkpoint uses base worlds `fd`/`dj` and caches `ok`/`jb`. Actual
`MinecraftServer` construction uses `dp`/`he`. The separate
[acquisition coordinator](world/cache.md) now has a retained-provider comparison
using the actual persistent client and dedicated-owner constructor routes.
[Cache evidence](reference/cache.md) distinguishes this finite API context from
running-server behavior. Separate [callback evidence](reference/cache-callbacks.md)
covers successful original loading and natural population, replaying observed
cache/world effects around C coordinator transitions. Expanded exact source
`28a55b52c001c4c114a11ca7df844fc1f286d328` passes all 56 selected tests without
skips on MSVC/GCC/Clang, with Linux sanitizers, independent audit and hosted checks.
The [read-only region reader](storage/region.md) retains bounded framing,
compression and raw NBT composition, with exact-source finite acceptance at
`092301ae8282d50a734df9018f791925aa9be99b`. It does not establish a world schema
or a disk chunk decoder.

The newer [world storage module](world/storage.md) owns stable decoded chunks,
native region transport and the world-store composition. Its
[image writer](storage/region-writer.md) accepts explicit compressed bytes and
timestamps; the [metadata codec](storage/level.md) owns schema interpretation
separately from gzip, fallback and file rotation. Storage loads preserve opaque
state; the acquisition adapter refuses unsupported entity/TileEntity activation.
At `9ad690d83592f127b9bb39d7233e8eef06fd0029`, all 71 selected tests pass without
skips on native MSVC/GCC/Clang, with Linux sanitizers and independent provenance
and copied-world semantic audits. The
[storage checkpoint](reference/storage.md#exact-source-storage-checkpoint)
defines its finite original-to-C and C-to-original scenarios. That checkpoint does not add save/unload
scheduling or live entity/player serializers.

The [lifecycle module](world/lifecycle.md) traverses the live insertion list,
saves eligible chunks and selects pending unloads through the bounded Integer
set's actual mutation history. The store registers borrowed cache/fallback
bindings and counts explicit leases. Unpublication never frees an owner;
individual retirement refuses active, mapped, listed, fallback or leased
identities and recovers capacity only after those references end.

`world/runtime` belongs to `mc_simulation`. It binds acquisition, generation
retention, storage and lifecycle callbacks to one stable world clock and fresh
metadata views. Generation reserves an owner slot before running and returns a
stable retained copy; later payload-allocation failure preserves generation's
completed effects. Loaded chunks preserve their source NBT. Runtime destruction
refuses live calls, leases, persistent Entity/TileEntity bindings or nonempty
cache/list/pending state and does not
save or free store chunks. Empty entity/TileEntity records are native; other bodies
remain explicit external seams. Its [qualification ledger](reference/lifecycle.md#exact-source-lifecycle-checkpoint)
is separate from the accepted storage checkpoint.

The runtime optionally borrows the existing application Random profile. Its
default-construction bridge enters that profile once and retains the exact
typed result separately from storage diagnostics. This foundation binds already
loaded metadata; it does not reproduce original World construction's earlier
Random, clock and Q-field sequence. Standalone ItemEntity constructors use the
shared profile, retain a world at its field store, and remain separate from
world insertion, ticking and loaded chunk entity activation.

The reference controller retains at most 32768 target object mirrors from their
first serialized IDs through a capture session. A separate synthetic JDI check
covers debugger disposal, late helper labels, capacity and duplicate rejection.
This reference-tool ownership is independent of native chunk retirement.

The public header is the boundary for Java arithmetic and explicitly seeded
`Random`. Callers own each generator. Invalid divisors and bounds return failure
without changing outputs or consuming randomness; game-specific exception behavior
belongs to the caller. See [evidence](reference/java-semantics.md) for operation
coverage and the remaining semantics audit.

Each Random owner also retains cached Gaussian bits and validity. Fresh native
initialization clears both; Java-style reseeding clears validity while keeping
the stored double. Gaussian misses use the core's strict log and SSE2 square
root under the [documented FP contract](reference/random-gaussian.md). Native
owners and complete continuation copies include this state.

The [shared Random profile](reference/random-profile.md) owns constructor entropy
and a separate atomic Math stream above individual worlds and registries. A
successful full-width uniquifier CAS precedes one real platform clock sample.
Lazy Math initialization publishes one retained owner; each double has separate
26-bit and 27-bit atomic LCG steps that may interleave across callers. Queries
and destruction require quiescence. Windows QPC and Linux monotonic backends
preserve their distinct conversions and native failure policies. Explicit seed
and reseed stay entropy-free. Game startup and population/entity/registry wiring
to this owner remain unfinished.

The separate private [millisecond wall clock](reference/wall-clock.md) uses
Windows FILETIME or Linux CLOCK_REALTIME. It has checked integer conversions,
no retained state and no monotonicity guarantee. World startup has not yet
been connected to it.

The [NBT boundary](storage/nbt.md) owns retained decoded trees and budgets their
allocations. It accepts bytes and produces bytes without compression, streams
or world schemas. Strings preserve UTF-16 code units, and parser failures leave
caller result outputs unchanged. The separate compression module owns its
decoded bytes and validates whole member checksums. Callers compose these
interfaces; neither implicitly selects a world schema or opens a file.

## System and subsystem folders

Runtime implementations live at `src/<system>/<subsystem>/`; public headers mirror
that path under `include/mc/`, and tests mirror it under `tests/`. This keeps a
subsystem's implementation and interface easy to locate as the project grows.
Create new folders with actual implementations rather than empty placeholders.

```text
include/mc/core/java/java.h
src/core/java/java.c
include/mc/core/java/random_profile.h
src/core/java/random_profile.c
src/core/java/random_profile_internal.h
src/platform/time/nanotime.h
src/platform/time/nanotime.c
src/platform/time/millis.h
src/platform/time/millis.c
include/mc/world/info/info.h
src/world/info/info.c
tests/world/info/test_info.c
tests/core/java/test_random_profile.c
tests/platform/time/test_nanotime.c
tests/platform/time/test_millis.c
include/mc/core/java/int_set.h
src/core/java/int_set.c
tests/core/java/test_java.c
tests/core/java/test_int_set.c
include/mc/core/math/math.h
src/core/math/math.c
tests/core/math/test_math.c
tests/core/math/math_probe.c
tests/reference/math/TrigMathObservation.java
scripts/reference/math/compare_math.py
include/mc/core/text/mutf8.h
src/core/text/mutf8.c
tests/core/text/test_mutf8.c
include/mc/storage/nbt/nbt.h
src/storage/nbt/nbt.c
tests/storage/nbt/test_nbt.c
tests/storage/nbt/nbt_probe.c
include/mc/storage/compression/compression.h
src/storage/compression/compression.c
tests/storage/compression/test_compression.c
tests/storage/compression/compression_probe.c
include/mc/storage/level/level.h
src/storage/level/level.c
tests/storage/level/test_level.c
include/mc/storage/region/writer.h
src/storage/region/writer.c
tests/storage/region/test_writer.c
include/mc/world/storage/chunk.h
include/mc/world/storage/regions.h
include/mc/world/storage/store.h
src/world/storage/chunk.c
src/world/storage/regions.c
src/world/storage/store.c
src/world/storage/files.c
include/mc/world/chunk/references.h
include/mc/world/cache/lifecycle.h
src/world/cache/lifecycle.c
tests/world/cache/test_lifecycle.c
include/mc/world/runtime/runtime.h
src/world/runtime/runtime.c
tests/world/runtime/test_runtime.c
tests/world/storage/test_chunk.c
include/mc/world/noise/noise.h
src/world/noise/noise.c
tests/world/noise/test_noise.c
tests/world/noise/noise_probe.c
tests/reference/noise/NoiseProbe.java
scripts/reference/noise/compare_noise.py
include/mc/world/noise/simplex.h
src/world/noise/simplex.c
tests/world/noise/test_simplex.c
tests/world/noise/simplex_probe.c
tests/reference/simplex/SimplexProbe.java
scripts/reference/simplex/compare_simplex.py
include/mc/world/biome/biome.h
src/world/biome/biome.c
tests/world/biome/test_biome.c
tests/world/biome/biome_probe.c
tests/reference/biome/BiomeProbe.java
scripts/reference/biome/compare_biome.py
include/mc/world/terrain/terrain.h
src/world/terrain/terrain.c
tests/world/terrain/test_terrain.c
tests/world/terrain/terrain_probe.c
tests/reference/density/DensityProbe.java
scripts/reference/density/compare_density.py
include/mc/world/terrain/surface.h
src/world/terrain/surface.c
tests/world/terrain/test_surface.c
tests/world/terrain/surface_probe.c
tests/reference/surface/SurfaceProbe.java
scripts/reference/surface/compare_surface.py
include/mc/world/terrain/caves.h
src/world/terrain/caves.c
src/world/terrain/caves_internal.h
tests/world/terrain/test_caves.c
tests/world/terrain/caves_probe.c
tests/reference/caves/CavesObservation.java
scripts/reference/caves/compare_caves.py
include/mc/world/generation/overworld.h
src/world/generation/overworld.c
tests/world/generation/test_overworld.c
tests/world/generation/overworld_probe.c
tests/reference/generation/OverworldGenerationObservation.java
tests/reference/generation/OverworldGenerationController.java
scripts/reference/generation/compare_overworld.py
scripts/reference/generation/export_overworld.py
include/mc/world/chunk/data.h
include/mc/world/chunk/nibbles.h
src/world/chunk/data.c
src/world/chunk/nibbles.c
tests/world/chunk/test_data.c
tests/reference/java/JavaSemanticsProbe.java
tests/reference/nbt/NbtProbe.java
tests/tools/resources/test_import_assets.py
scripts/development/format_code.py
scripts/resources/import_assets.py
scripts/reference/binaries/inspect_jars.py
scripts/reference/java/compare_java.py
scripts/reference/nbt/compare_nbt.py
scripts/reference/server/run_smoke.py
```

Formatting follows the repository `.clang-format` and is
repeatable through `scripts/development/format_code.py` and checked in CI with
clang-format 18. The formatter scans project C headers/sources only, keeping
ignored decompiled research outside its input set.

## Planned game systems

As implementation proceeds, the intended boundaries are:

| Module | Responsibility | Dependency rule |
| --- | --- | --- |
| Core | Defined Java numerical/text/serialization behavior | Usable by headless tools and server |
| World and simulation | Chunks, blocks, entities, ordered tick updates, generation | No rendering or audio initialization; preserve evidenced side distinctions |
| Persistence | NBT, region files, world/player/entity state | Preserve unimplemented fields before writable compatibility paths |
| Networking | Original packet encoding, stream framing, connection states | Bounded parsing and explicit buffering; original protocol |
| Platform | Clock, files, sockets, window/input/audio device services | Native Linux/Windows implementations with explicit ownership |
| Renderer and audio | OpenGL 3.3 core presentation, resource loading, audio events | Linked by graphical client only |
| Client | Menus, input sampling, single-player and multiplayer sessions | Render frequency must not change simulation state |
| Dedicated server | Headless configuration, administration, save and shutdown | No display, OpenGL context, or client audio dependency |

Add interfaces as reference-backed game behavior is implemented.
CMake build options apply strict diagnostics
and floating-point rules to project code; future dependencies need their own
licensing and build review. The client uses pinned SDL3 for its window and
OpenGL context. Storage uses pinned static zlib; its
[license and build configuration](dependencies.md) remain separate from project code.

Use checked allocation, bounded buffers, and explicit ownership/cleanup paths.
Begin with reproducible simulation order. Parallel research/build tasks do not
authorize concurrent simulation updates without equivalence evidence.
