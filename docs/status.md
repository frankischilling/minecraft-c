# Development status

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
prefix. Both original/native sides and public replay pass on MSVC, GCC and
Clang. It checks all saved typed NBT, five loaded arrays, held Info,
cache/numerical state and raw disk preservation. The separate
[held World full-save profile](reference/population-full-save.md) now reaches
actual metadata and empty named SavedData lists. Both original/native sides and
public replay pass on MSVC, GCC and Clang. Player and named subtype
serialization, source file-failure semantics, unload and restart remain open.

At code checkpoint `cd41262`, local suites pass 194 MSVC, 196 GCC and 196 Clang
tests: 586 executions without failures, errors or skips. Every compiler includes
all four fresh population profiles and plant comparisons. Linux uses ASan,
UBSan and leak checks. All 551 C/header files pass clang-format 18. The
[source audit](reference/population-full-save.md#committed-checkpoint) verifies
1,057 files, all 24 fresh/public captures and 24 resource-variant pairings.

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
