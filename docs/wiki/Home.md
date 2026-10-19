# minecraft-c

The [recipe initialization](https://github.com/frankischilling/minecraft-c/blob/main/docs/item/recipes.md)
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
