# minecraft-c

The [falling subsystem](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/falling.md)
now implements sand/gravel Block hooks, immediate descent, actual FallingSand
constructors/publication and subtype NBT. All 158 configured MSVC cases pass in two partitions;
nineteen affected cases pass per sanitized GCC/Clang build, including 230
constructor/NBT and 300 runtime checks per host. Later Entity movement/ticks,
full original replay and complete population acceptance remain unfinished.

The following Dungeon checkpoint predates falling integration.

The [Dungeon subsystem](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/dungeon.md)
now builds rooms through notifying World writes, fills actual Chest inventories
and assigns the shared Spawner name. Its 3,655 native checks pass per compiler,
including real prepared save/unload/reload. Ordinary Item runtime access passes
292 checks; 924 finite original helper comparisons match loot, names and Random
continuations. All 153/155/155 configured MSVC/GCC/Clang cases pass. Complete
original placement, observer/hook closure and natural population remain open.

At the preceding ordinary Item persistence checkpoint:

The [ordinary GAME Item persistence](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/entity-game-storage.md)
path now loads through the bound factory, directly adopts into the held Chunk,
and saves every real Item occurrence before Tiles. Its 468 native disk checks
and 255 overlay checks pass on all three compilers. Four original ten-case
captures supply 24 exact known two-Item tag-list comparisons against the native
saved records. Unknown-field retention remains an explicit native policy.

The [Item Entity NBT subsystem](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/nbt.md)
adds canonical Class ownership, ordered EntityList registration, actual Item
read/write and factory calls, and whole-batch World activation/deferral. Twenty
prepared cases per side match both originals on MSVC and sanitized GCC/Clang.
The [captured-original regression](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/entity-nbt.md)
checks 4,844 comparison rows without requiring game archives or a JDK. Other
concrete subclasses, ticking and
complete population remain unfinished.
All 151 configured MSVC cases and 153 cases on each sanitized GCC/Clang build
pass, with zero skips and local JDK comparisons enabled.

At the preceding live Tile checkpoint:

The [live Tile subsystem](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/tiles.md)
adds actual Chest and Spawner owners, World/Chunk occurrences, typed failures
and staged GAME imports before block normalization. The
[live GAME save path](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/saving.md)
now writes actual Chest/Spawner/Stack state over the same stored Chunk, with
source q/error prefixes and selected Info growth. All 144 configured public
tests pass on MSVC and all 146 on each sanitized GCC/Clang build, including
468 Tile and 1,003 save checks per host. Reload uses an explicit native
dedicated-policy coordinator; natural dedicated startup remains unfinished.
Other Tile constructors, GAME Entity load/save integration, whole World
metadata, original/native save replay and complete population remain open.

The [population World queries](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/queries.md)
and [plant predicates](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/plants.md)
implement downward solid/liquid scans, light, sky visibility and nine concrete
plant receiver kinds. The [plant feature subsystem](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/plants.md)
executes six complete feature families over actual World/Registry/Random owners.
The [tree features](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/trees.md)
add oak, birch, both taiga forms and big oak. The
[geology features](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/ore-clay.md)
and [lake/spring features](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/lakes-springs.md)
add ore, clay, lakes and springs through the same actual owners.
At the preceding checkpoint, all 142 configured public tests passed on MSVC and
all 144 on each sanitized GCC/Clang build, including 6,110 combined query, predicate, plant, tree and
geology checks per host. Separate original query observations retain 44 calls
per side; ten selected original tree calls cover the five forms on both sides.
That checkpoint left original/native feature replay, Dungeons, snow and the
complete controller unfinished. The current
[normal controller](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/normal.md)
and [Dungeon contracts](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/dungeon.md)
record subsequent work. Full natural population remains unfinished.

The [Entity publication contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/entities.md)
adds real World/Chunk occurrences, duplicates, cross-World bindings and terminal
native disposal. Fluid drops can publish their actual ItemEntity and continue.
All 67 selected native cases pass on MSVC and sanitized GCC/Clang. Nonempty Entity
saving, player/dedicated descendants and complete synchronous population remain
unfinished; terminal disposal never makes erased Entity state serializable.

The [concrete World height and lighting operations](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/runtime-lighting.md)
use the actual provider, C records and shared A. All 33 selected cases pass on
MSVC/GCC/Clang, including 177 height and 2,000,382 queue checks with Linux
sanitizers. These native client-World contracts add no new original World
comparison. Live Chunk finishing, propagation, complete generation, population,
synchronous mutations and persistence acceptance remain unfinished.

The [World/Dimension Biome managers](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/biome-registry.md#world-and-dimension-managers)
retain actual shared references, cached arrays and independent Registry borrows.
All 28 selected cases pass on MSVC/GCC/Clang, including 304 World-manager and
386 fixed-manager checks with Linux sanitizers. These native contracts add no
new original World comparison. Live generation, lighting, population, complete
mutations and persistence remain unfinished.

The [shared Biome module](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/biome-registry.md)
adds actual objects, spawn lists/Class handles and a source-ordered reference
sampler. Both original graphs and 3,824 queries per side match MSVC/GCC/Clang.
All 26 selected cases pass, including 101,396 registry and 609 sampling checks,
with Linux sanitizers. Live generation, population and full persistence remain
unfinished.

The [World registry integration](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/registry.md)
adds shared owner lifetimes, material identities and ordered Dimension spawn
predicates. All 22 selected cases pass on MSVC/GCC/Clang, including 150 new
constructed checks and Linux sanitizers. Actual missing-chunk generation,
the World/Biome join, population and full persistence remain unfinished. These
native checks supply no new executed-original World comparison.

The preceding [recipe initialization](https://github.com/frankischilling/minecraft-c/blob/main/docs/item/recipes.md)
checkpoint constructs all 151 recipes and ten smelts, and completes the crafting
statistics array. Thirty original captures cover five entry orders on both
sides. All ten numeric graphs and catalogs, plus both Stack projections, match
MSVC/GCC/Clang. All 30 selected Windows and 32 cases per Linux compiler pass,
with Linux sanitizers. Ordinary Statistics and Block initialization returns;
Item and direct managers preserve their recorded null-reference failures.
Full World startup, crafting/furnace gameplay, population, synchronous mutations
and persistence acceptance remain unfinished.

The project targets Minecraft Beta 1.7.3 in C17, with a graphical single-player
and multiplayer client plus a dedicated headless server. Players select local
usernames; account authentication is excluded.

There is no playable C client or server yet. Current code provides numerical/text
compatibility, float lookup/floor helpers, raw NBT, bounded gzip/zlib memory
compression, seeded Perlin/octave noise, 2D simplex/octave noise,
climate/biome classification, normal density/base
terrain, surface replacement, normal cave carving, chunk voxel-data helpers,
ordered light-region storage, cached height queries, normal chunk finishing,
unpublished normal generator composition, synchronous cache acquisition,
region inspection and writing, world metadata, stable stored chunks, native
world-file ownership, synchronous save/unload coordination, tracked retirement,
native world-runtime composition, local resource import and native builds.
The storage foundation is implemented through `mc_persistence` and its cache
acquisition adapter. At `9ad690d83592f127b9bb39d7233e8eef06fd0029`, all 71
selected tests pass without skips on native MSVC and WSL GCC/Clang, with Linux
ASan/UBSan. Independent platform, source/provenance and copied-world semantic
audits pass. Hosted checks pass 59 public/numerical tests per compiler and
clang-format18 for 111 C/header files. Both copied-world directions include
opaque Boat/Sign transport. These finite storage results do not implement
entity/player simulation, gameplay save/unload or running-server persistence.
See the [storage checkpoint](Storage-and-reference#storage-foundation-checkpoint),
[ownership contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/storage.md)
and [storage evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/storage.md).

The [lifecycle boundary](Architecture#save-unload-and-retirement) adds live
ordered saving, pending-key history, metadata-first accounting and safe owner
retirement/reload. The [finite qualification](Storage-and-reference#lifecycle-qualification)
for source `46546d8322de52beb6c3a51f30e827c0260d2b30` selects the complete
Windows v10, GCC v9 and Clang v9 suites. Each passed 92 tests without failures
or skips; independent source/semantic audits, storage rechecks and complete
capture preservation also pass, with Linux ASan/UBSan. Empty entity/tile records are native;
Boat/Sign bodies remain external callback evidence. General deferred cleanup,
terrain-feature bodies, world ticking, gameplay dimension transitions,
running-server persistence and crash recovery remain open. The v7 and v8
matrices are excluded. Windows v9 is also excluded after an original setup
skip; its successful rows are not carried into the fresh Windows v10 suite.
The bounded debugger-mirror and scoped helper-interval
corrections have separate Windows/Linux regression passes alongside the
qualified full suites above.

The [normal generator API](Architecture#normal-generator) composes the existing
stages through finishing with borrowed world resources. At exact source
`a08c12056a06261ed01d9f66999459f7ce9543b4`, all 41 selected local tests pass
on MSVC/GCC/Clang without skips, including four-seed original comparisons and
complete untouched controls. Hosted checks pass 36 public/numerical tests per
compiler and formatting for 79 C/header files. See the
[generation checkpoint](Storage-and-reference#normal-generation-checkpoint)
for evidence and its finite scope.
The expanded generation/cache/region checkpoint at
`28a55b52c001c4c114a11ca7df844fc1f286d328` passes all 56 selected local tests
without skips on those toolchains, with Linux sanitizers and independent audit.
Hosted checks pass 48 tests per compiler and formatting for 89 C/header files.
See [cache and region evidence](Storage-and-reference#cache-and-region-checks)
for successful original loads, population callbacks and their remaining scope.
The [repository README](https://github.com/frankischilling/minecraft-c#readme),
[parity matrix](https://github.com/frankischilling/minecraft-c/blob/main/docs/parity.md)
and [feature inventory](https://github.com/frankischilling/minecraft-c/blob/main/docs/inventory.md)
are the reviewable sources for current behavior and evidence.

Original Minecraft binaries/assets, decompiled source, launcher files and worlds
are excluded from source and releases. New project work is MIT licensed. See
[build and tests](Build-and-tests) for development setup and its current limits.

Detailed pages cover [systems and subsystems](Architecture) and
[storage and original-reference checks](Storage-and-reference). The
[development status](https://github.com/frankischilling/minecraft-c/blob/main/docs/status.md)
records the current branch, tested commits and remaining work.

These pages are versioned under `docs/wiki/`. Exact tested sources, retained
evidence and remaining work are recorded in the linked development documents.

The native
[translation consumer](https://github.com/frankischilling/minecraft-c/blob/main/docs/resources/translation.md)
now reads imported language files on the shared class owner. Its complete
633-property observations match both originals on MSVC/GCC/Clang. Separate
[Properties evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/properties.md)
covers 252 recorded JDK byte-stream loads and 980 queries. These prerequisites
do not complete statistics, registry bootstrap or population/persistence.

The native
[achievement-key loader](https://github.com/frankischilling/minecraft-c/blob/main/docs/statistics/keys.md)
also matches all 446 entries and 453 lookups from both original JARs. Its
[line reader](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/reader.md)
matches 800 recorded JDK owners and 7,136 actions. Fourteen selected cases pass
on MSVC/GCC/Clang with Linux sanitizers. Complete statistics, registry startup
and population/persistence remain unfinished.
