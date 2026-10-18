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
unpublished normal generator composition, local resource import and native builds.
The [normal generator API](Architecture#normal-generator) composes the existing
stages through finishing with borrowed world resources. At exact source
`a08c12056a06261ed01d9f66999459f7ce9543b4`, all 41 selected local tests pass
on MSVC/GCC/Clang without skips, including four-seed original comparisons and
complete untouched controls. Hosted checks pass 36 public/numerical tests per
compiler and formatting for 79 C/header files. See the
[generation checkpoint](Storage-and-reference#normal-generation-checkpoint)
for evidence and its finite scope.
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

These versioned pages await combined integration of the continuing generation
and cache work. The published wiki remains at finishing checkpoint
`4e4a9f3853ac8db012bca9aaf179d956e288a686`; these updates are not yet published.
