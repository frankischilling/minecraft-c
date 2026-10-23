# Development status

The [synchronous wire hooks](world/wire.md) now use actual World mutations,
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
Set structure. Full World-level wire and native snapshot qualification,
callback-driven cross-World reentry, wider persistence/failure
schedules and the default identity source remain required.

The [wire position Set](reference/wire-position-set.md) has reclaimable
Registry storage, full coordinate equality, bucket/tree ordering and snapshots
that survive clear/refill. Finite original comparisons pass conditionally on
measured identity hashes on MSVC/GCC/Clang. Windows development tests pass;
five Linux mounted-source timeouts pass separately from an identical native
source copy. The reference record preserves the failed full-suite results and
development-source attribution. Its [committed collection checkpoint](reference/wire-position-set.md#committed-collection-checkpoint)
separately passes all 607 hosted public/JDK tests and predates wire integration.

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
  [Piston Tiles and geometry](world/tiles/piston.md).
- [Item and FallingSand movement](entity/movement.md), including collision and
  reached World effects.
- [Normal population](world/population/normal.md), its feature bodies and the
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
