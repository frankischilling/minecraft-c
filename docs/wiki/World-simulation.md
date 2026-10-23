# World simulation

The [fresh World population comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/natural-population.md)
matches seed23 origin and ring profiles on client/base-server sides. The
ten-call ring includes negative coordinates, nine normal Chunks, four populated
Chunks and 6,968 final ordered light regions. Both profiles check complete
retained arrays, cache/light ordering, provider numerical state, array aliases
and Random continuations. Public ring replay has a full original numerical
and cache golden oracle; origin public replay keeps its cache-only oracle.
Both native replays use separately declared minimal resources. Other seeds,
arbitrary recursive schedules and full persistence retain separate acceptance
requirements.

The [populated Chunk save/readback comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/population-persistence.md)
extends the origin prefix with two forced cache saves and four direct region
loads. It checks complete typed NBT, normalized arrays, held Info and cache state,
and all disk bytes. It keeps the same World alive; full metadata save, unload
and restart remain separate requirements.

The native World modules use actual Registry owners, Chunk storage, acquiring
readers and owning Lists. They preserve source order and completed effects when
a later operation fails. Their contracts describe which bodies are implemented,
which scenarios have been compared with the original programs, and which
dependencies remain unfinished.

Plant lifecycle callbacks, farmland and World block-light queries are
implemented. Complete local MSVC/GCC/Clang suites pass. On each compiler, all 59
prepared original cases per side match block/metadata maps, both Random streams
and ordered deterministic drops. Those dependency fixtures do not establish
original World/Chunk construction, general notification or natural population.
The [plant evidence](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/plant-lifecycle.md)
records the comparison scope and native full-height removal regression.

| System | Contracts and reference evidence |
| --- | --- |
| Plants and light queries | [Plant behavior](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/plants.md), [World queries](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/queries.md) |
| Minecarts | [Construction and detection](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/minecart.md), [inventory, NBT and storage](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/minecart-inventory.md) |
| Rails and power | [Rail topology and lifecycle](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/rails.md), [power queries and outputs](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/power.md), [pressure plates](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/pressure-plates.md) |
| Pistons | [Tile, finalization, collision and pushing](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/tiles/piston.md), [source map and finite comparisons](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/piston.md) |
| Entity movement | [Item and FallingSand movement](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/movement.md), [source map and observations](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/entity-movement.md) |
| Population and saves | [Normal controller](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/normal.md), [live save path](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/saving.md), [native storage](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/storage.md) |

The [Minecart constructor comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/minecart-construction.md#constructor-observations-and-qualification)
and [inventory/semantic NBT comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/minecart-inventory-nbt.md#finite-inventory-and-semantic-nbt-comparison)
retain separate finite original/Windows MSVC results. Their null-World inputs
do not establish publication, movement, Player interaction or rail travel.
Inventory comparison on GCC and Clang remains pending. The
[power comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/power.md#validation-boundary)
has its own declared snapshot and completion projection; an original-runtime
rail comparison remains unverified.

Complete redstone propagation, active Minecart ticks and rail physics, full
population and persistence acceptance, multiplayer interoperability and playable
client/server integration remain unfinished.

Use the [current status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md),
[testing guide](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md)
and [parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md)
for current validation. Detailed past counts and source checkpoints are retained
in the [population and runtime history](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/population.md).
The [architecture](Architecture), [build](Build-and-tests) and
[storage/reference](Storage-and-reference) wiki pages provide further navigation.
