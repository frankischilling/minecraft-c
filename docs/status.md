# Development status

At `a1c9e0c33a7a410df9e50202f4b4b672ede79320`,
[all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/40080961138)
pass: 211 MSVC, 214 GCC and 214 Clang tests, with no missing cases or skips.
Formatting covers 614 C/header files; controller mirrors and lifecycle interval
checks also pass. Linux includes actual software OpenGL tests and ASan/UBSan.
The preceding Windows build failed on MSVC 19.44's flexible-array diagnostic.
The image owner now points into its checked contiguous allocation, preserving
strict warnings and allocation accounting; that compiler's full build and tests pass.

The [native client](client/startup.md) draws the title screen on Windows with an
NVIDIA RTX 3060 and on Linux through x11 with Mesa llvmpipe. A later focused
checkpoint passes all eight frontend tests on MSVC and sanitized GCC/Clang,
including 156 resource checks, 96 Timer checks, 50 real OpenGL checks and the
scene parser controls. The ordered FontRenderer interface adds 262,379 checks
while preserving the existing 220 glyph checks. Each platform also completed a
bounded hidden 200-frame invocation with an actual exit0 and unchanged source,
assets and binary hashes. Windows controls pass window close, Quit, and a
resize to 512x384 followed by Quit, using the actual owned hidden window.

The preceding complete source snapshot passes all 211 MSVC and 213 Clang cases.
GCC passed 212 of 213 initially; the remaining strict named-data replay passed
once with unchanged code and fresh OS samples. The first capture retained a
measured 1,253 ms backward wall-clock step and remains failed at its chronological
admission guard. No timestamps or comparisons were relaxed. The final
minimized-window adjustment was then rebuilt and retested through the six
frontend cases and actual captures on all three compilers. Earlier full suites
below retain their own checkpoint scope.

Child menus, options persistence, audio, complete startup, gameplay and real
fullscreen/zero-drawable controls remain unfinished or unverified. The shared
[GuiMainMenu renderer](client/gui-main-menu.md) supports a matched conditional
scene. Exact raw-image comparisons still fail at one pixel on Windows and one
different pixel on Linux; transform arithmetic remains under investigation.
Production text still uses flattened glyph positions; ordered matrix replay
remains unfinished.
An original title draw and exact clock witness have
been observed in an arranged receiver; the first helper's teardown failed.
The second helper completed with natural target/observer exits; its controller
failed a JDI version-property check. A separate corrected file-only audit
passed without a redraw. The [reference note](reference/client-title.md)
records that lineage. These results do not qualify complete original startup
or rendering equality.

The [counted float parser](core/java-float.md) constructs binary32 values with
bounded integer arithmetic. Standalone builds on MSVC/GCC/Clang each pass 100
unit checks and 5,632 probe records. Public client-disabled integration also
passes 13 selected tests and the complete corpus per compiler, without graphics
targets or direct imports. The [legacy float formatter](core/java-float-format.md)
also matches 5,708 retained JDK rows and 49,532 separately derived model cases
per compiler. The [integer parser](core/java-integer.md) also passes 43 unit
checks and all 65,536 one-unit comparisons per compiler through the public
client-disabled core. GameSettings and options persistence remain unfinished.

The [WorldAccess interface](world/world-access.md) now dispatches typed
cell, range, sound, particle, Entity-added and TileEntity-dirty callbacks. Its
native controls pass 295 checks on MSVC and sanitized GCC/Clang, including live-list
edits, retained receivers, failure prefixes and redstone reentry across Worlds
sharing a Registry. The existing-World redstone probe also accepts declared
constructor inputs. Its 52 controls per compiler check consumption, failure
ordering and file preservation. Original nonempty WorldAccess traces, concrete presentation
and tracking receivers, and original redstone constructor-input admission remain
unfinished.

At `ef1434015420678559cfa3d3b2f21018c5b733ce`, the local public/JDK suites passed
all 619 tests across MSVC and sanitized GCC/Clang. All 587 C/header files passed
formatting; controller mirrors and lifecycle interval controls also passed.
The [hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40052114598)
passed all four jobs with the same 619-test and 587-file result. These results
precede the [canonical naming migration](reference/minecraft-names.md).

The renamed code also passes all 619 local tests and 587-file formatting,
including the schema-2 redstone producer's native controls. Controller mirrors
and lifecycle interval checks pass on Windows and Linux. Recorded fixtures keep
their source pins, with explicit historical/current module families and a
separate immutable WorldProvider recorder. These checks cover the represented
native contracts and finite replays; original nonempty WorldAccess and redstone
constructor-input correspondence remain unfinished.

The [synchronous redstone hooks](world/redstone.md) now use actual World mutations,
recursive strength updates and independently retained notification snapshots.
The native contracts cover both sides, a 25-cell path, rise/decay, vertical
connections, loops, drops and partial failures. A second 350-check case covers
negative/cross-chunk paths across four Worlds, dedicated loading gates and a
snapshot retained while another World uses the shared Registry. It passes on
MSVC and sanitized GCC/Clang. Code checkpoint `2150299` passes all 613 hosted
public/JDK tests and 583-file formatting. A bounded existing-World probe
matches 600 complete arrays per compiler against original observations while
retaining absolute World/cave Random differences. Linux Debug traces also match
2,087 ordered engine calls per side, including arguments, returns, flags and
Set structure, plus 605 local observations of 151 retained snapshots. The
comparison declares its storage and final-counter adaptations; twelve file-only
Windows corruption controls reject the intended errors. A separate GCC Debug
step comparison matches upward/downward redstone paths and all retained arrays,
while keeping absolute World/cave Random differences. Full World-level
redstone qualification, original callback-driven reentry comparison, wider persistence/failure
schedules and the default identity source remain required.

The [redstone position Set](reference/redstone-position-set.md) has reclaimable
Registry storage, full coordinate equality, bucket/tree ordering and snapshots
that survive clear/refill. Finite original comparisons pass conditionally on
measured identity hashes on MSVC/GCC/Clang. Windows development tests pass;
five Linux mounted-source timeouts pass separately from an identical native
source copy. The reference record preserves the failed full-suite results and
development-source attribution. Its [committed collection checkpoint](reference/redstone-position-set.md#committed-collection-checkpoint)
separately passes all 607 hosted public/JDK tests and predates redstone integration.

The [dedicated checkpoint](reference/dedicated-world-construction.md#committed-checkpoint)
at `9c87827` passes all 622 local tests: 206 MSVC, 208 GCC and 208 Clang, with
zero failures, errors or skips. All 1,099 source files match their committed Git
blobs. Fresh schema-2 original audits and native comparisons pass on every
compiler, covering seed23, seventeen calls, eighteen boundaries and actual Store
tokens on both sides. Linux enables ASan, UBSan and leak checks. The hosted run
passes 601 public/JDK tests and 569-file formatting; its actual merge checkout
has the same entire tree as the code checkpoint. Mirrors and interval controls
pass on all local and hosted builds.

Public capture, audit, comparison and replay tools retain the complete finite
observations and nine constructor inputs. The original-only fixture keeps its
inherited dirty-source attribution. Public replay runs the production probe with
minimal resources; fresh reference runs audit the supplied original and use its
exact extracted resources. The reference record defines the empty-collection
profile and remaining server-startup, scheduling and restart limits.

Synchronous fluid and scheduler diagnostics use checked heap workspaces. The
dedicated capture exposed large diagnostic copies exhausting the Windows
stack during valid fluid recursion. A public enclosed-column test checks both
sides, complete block/metadata results, 34 schedule requests, unchanged Random
state and balanced ownership. Earlier development suites passed 613 executions:
203 MSVC, 205 GCC and 205 Clang, with no failures, errors or skips. Linux enabled
ASan, UBSan and leak checks. All compilers passed controller mirrors and
556 positive/2,011 negative interval controls; all 569 C/header files passed
clang-format 18. Those counts precede the public dedicated workflow and retain
their dirty source and null tested commits. The
[reference record](reference/dedicated-world-construction.md) corrects the earlier
byte-equality claim. The eighteen public Python checks and eight actual-native
corruption controls also pass in the current checkpoint. Earlier named-data
results below retain their own qualified source and scope.

The production GAME World matches the original seed-23 origin and ring
population profiles on client and base-server sides. The ten-call ring retains
nine normal Chunks, populates four and ends with 6,968 ordered light regions.
The comparisons include complete retained arrays, cache state, provider
numerical state, array aliases and Random continuations. The
[fresh World evidence](reference/natural-population.md) defines the finite
schedules and their limits. Broader population acceptance remains open.

The public ring replay checks full original-derived numerical and cache golden
records. The public origin replay keeps its cache-only oracle. Both run the real
native probe with separately declared minimal resource files.

The [populated Chunk persistence comparison](reference/population-persistence.md)
adds two real forced cache saves and four direct region loads after the origin
prefix. It checks saved typed NBT, five loaded arrays, held Info,
cache/numerical state and raw disk preservation. The
[held World full-save profile](reference/population-full-save.md) adds metadata
and empty named SavedData lists. The separate
[named MapData profile](reference/population-named-data.md) publishes one dirty
default value under a different lookup key, performs two full saves and loads
its file through a fresh manager. Both original/native sides and all five
public replays pass on MSVC, GCC and Clang. Map terrain updates, trackers and
packets, broader manager/subtype behavior, nonempty Players, source file-failure
semantics, ordering, dedicated runtime, unload and restart remain open.

At code checkpoint `5b57bc1`, local suites pass 199 MSVC, 201 GCC and 201 Clang
tests: 601 executions without failures, errors or skips. Every compiler includes
five fresh population profiles, their public replays and the plant comparison.
Linux uses ASan, UBSan and leak checks. All 559 C/header files pass
clang-format 18; every compiler passes mirrors and 556 positive/2,011 negative
lifecycle-interval controls. The [source and capture audit](reference/population-named-data.md#committed-checkpoint)
verifies 1,073 clean-source files, 30 fresh/public captures and 30 resource-variant
pairings. [Hosted verification](https://github.com/frankischilling/minecraft-c/actions/runs/39964552445)
passes all 583 public/JDK executions and formatting/mirror/interval checks;
all four actual checkout trees match the code checkpoint. The eighteen
supplied-archive comparisons retain their local scope.

Plant lifecycle callbacks, farmland behavior and World block-light queries are
implemented.
The [plant](world/plants.md) and [World-query](world/queries.md) contracts describe
the implemented behavior and its limits.

Original client and server projections match all 59 prepared cases per side
on each compiler. The comparison covers block/metadata maps, both Random
continuations and ordered deterministic Item fields. These fixtures execute
unchanged original Block and selected tree bodies with prepared World services;
original World/Chunk construction, general notification and natural population
retain separate acceptance requirements. See the [evidence](reference/plant-lifecycle.md).

Other implemented modules have separate contracts and verification scopes:

- [Minecart construction](entity/minecart.md) and
  [inventory, semantic NBT and storage](entity/minecart-inventory.md).
- [Rails](world/rails.md), [World power queries](world/power.md),
  [pressure plates](world/pressure-plates.md) and
  [Piston TileEntities and geometry](world/tile-entities/piston.md).
- [Item and FallingSand movement](entity/movement.md), including collision and
  reached World effects.
- [Overworld population](world/population/overworld.md), its feature bodies and the
  [live save path](world/saving.md).

The [finite Minecart constructor comparison](reference/minecart-construction.md#constructor-observations-and-qualification)
matches ten null-World inputs per side against Windows MSVC. A separate
[inventory and semantic NBT comparison](reference/minecart-inventory-nbt.md#finite-inventory-and-semantic-nbt-comparison)
matches its complete declared projections on both sides. GCC/Clang inventory
comparisons remain pending. These results do not establish Minecart movement,
Player interaction or rail physics.

Whole-game behavior remains unfinished. Active Minecart ticks and rail travel,
complete redstone propagation, population and persistence acceptance, all four
multiplayer pairings, native graphical play, controls and audio remain required.
There is no playable C client or dedicated server. Account authentication is the
only approved gameplay exception.

See [testing](testing.md), the [parity matrix](parity.md) and
[roadmap](roadmap.md) for current requirements. Detailed earlier results remain
in the [population and runtime history](history/population.md) and
[earlier integration history](history.md).
