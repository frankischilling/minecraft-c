# minecraft-c

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
world-file ownership, local resource import and native builds.
The storage foundation is implemented through `mc_persistence` and its cache
acquisition adapter, with acceptance still in DEVELOPMENT. All 57 public tests
pass without skips on native MSVC and WSL GCC/Clang, with Linux ASan/UBSan;
clang-format18 passes for 111 C/header files. Hosted development checks pass 58
public/numerical tests per compiler. Fresh native Windows and WSL/GCC whole-world
development smokes and 69 Java-free harness guards per host pass. Exact-source
full MSVC/GCC/Clang acceptance remains pending. See the
[storage foundation](Storage-and-reference#storage-foundation-in-development),
[ownership contract](https://github.com/frankischilling/minecraft-c/blob/main/docs/world/storage.md)
and [storage evidence](https://github.com/frankischilling/minecraft-c/blob/main/docs/reference/storage.md).

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
