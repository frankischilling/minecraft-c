# World simulation

The [synchronous wire hooks](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/wire.md)
now run real World mutations, recursive strength updates and shared notification
snapshots. The 447-check native contract covers both sides, a 25-cell path,
vertical connections, loops, drops and partial failures. Development suites pass
200 MSVC and 202 sanitized Clang tests. Original World-level comparison,
cross-chunk/dedicated scenarios and default identity generation remain required.

The [wire position Set](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/wire-position-set.md)
has full coordinate equality, ordered bucket/tree operations and reclaimable
Registry storage. Shallow snapshots survive clear/refill and nested use. Local
MSVC/GCC/Clang comparisons match sixteen original collection histories under
measured identity inputs. The reference record retains development-source and
test limits. The committed collection checkpoint separately passes 607 hosted
public/JDK tests and predates lifecycle integration. Default identity generation
and original World notification-order acceptance remain unfinished.

[Dedicated construction](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/dedicated-world-construction.md)
now has actual Server and World owners, separate subclass fields and a
duplicate-preserving Entity index. Its public original-only fixture and
production native replay preserve the measured constructor inputs and complete
finite state. The fresh reference workflow captures and audits the supplied
original before comparing a new native run. The
[clean checkpoint](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/dedicated-world-construction.md#committed-checkpoint)
at `9c87827` passes 622 local tests on MSVC/GCC/Clang and 601 hosted public/JDK
tests. Fresh schema-2 audits and comparisons pass for seed23, seventeen calls,
eighteen boundaries and fifteen interned original states, including population,
full saves, protected and distant drops, maintenance and reload. All eighteen
actual Store-token boundaries match on each side. The fixture retains its
inherited dirty source and null tested commit. The finite profile has empty
Entity, Tile, named-data and index collections. Server startup, networking,
general scheduling, nonempty behavior and unrestricted unload/restart remain
unfinished.

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
and all disk bytes in the same World. The
[full-save profile](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/population-full-save.md)
adds metadata and empty named collections. The separate
[named MapData profile](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/population-named-data.md)
puts one default dirty value through two full saves and a fresh manager load.
Both original/native sides and public replay match on all three local compilers.
Its twenty-four complete boundaries retain the same World and actual manager,
value, String and colors owners. Nonempty Players, map terrain updates,
trackers/packets, broader subtypes, source file-failure semantics, unrestricted
ordering, dedicated runtime, unload and restart remain separate requirements.

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
| SavedData | [Manager/MapData ownership and type binding](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/saved-data.md), [held-World named comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/population-named-data.md) |

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
