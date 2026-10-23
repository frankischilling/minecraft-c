# Development status

The production GAME World matches the original seed-23 origin population on
client and base-server sides: complete retained Chunk arrays, ordered light
regions, provider numerical state and Random continuations. The
[fresh World evidence](reference/natural-population.md) records one populated
origin per side and its remaining limits. Broader population acceptance remains
open.

Plant lifecycle callbacks, farmland behavior and World block-light queries are
implemented. At plant checkpoint `2e9c6e0`, local suites pass 183 MSVC, 185 GCC and 185 Clang tests without
failures or skips, including a fresh plant comparison on each compiler. Linux
uses ASan, UBSan and leak checks. All 549 C/header files pass clang-format 18.
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
