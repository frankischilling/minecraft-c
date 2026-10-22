# World simulation

The native World modules use real Registry owners, Chunk storage, acquiring
readers and owning Lists. Their tests exercise source-ordered effects and
failure prefixes. They do not provide a playable client, server or complete
gameplay loop.

## Minecart inventory, NBT and native storage

The [inventory contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/minecart-inventory.md) and [native validation](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/minecart-inventory-nbt.md#native-validation) cover seven inventory operations, actual generic NBT hooks,
named construction and fresh semantic saves. Optional per-occurrence provenance
keeps unknown fields separate from shared Stack identity. The full local suites
pass 180 MSVC and 182 GCC/Clang tests, 544 total, with zero failures, errors or
skips. Inventory passes 598 checks/40 cases, NBT 558/40 and storage 335/14;
314 independent disk checks cover six regions and eight metadata files.
The current constructor/detector regressions pass 7,270/164 and 208/24.
Original/native inventory, NBT and storage equality remains pending.

## Rails

The [rail contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/rails.md)
and [paired source map](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/rails.md)
cover all thirteen connection-helper methods and ten shapes, inherited added
and neighbor hooks, detector contact/tick/update, and powered search with the
source's depth guard. Notifying writes retain earlier changes when a later
operation fails. Typed diagnostics preserve the selected current and deepest
sites and the actual lower errors.

The native rail fixture passes 814 checks across 144 literal real-World cases
per host. A separate pure error-transport fixture passes 1,444 checks across 94
constructed records; those records are not observed gameplay errors. Complete
suites at that rail checkpoint passed 175 MSVC and 177 GCC/Clang tests. Later
Minecart validation has its own source and hosted checkpoint below.

Ordinary positive rail-drop publication, gameplay rail physics, scheduled
draining and rendering remain unverified. There is no executed-original rail
comparison.

## Minecart construction and detection

The construction and hosted results in this section are earlier checkpoints.

The [Minecart contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/minecart.md)
implements the class array graph, both constructors and real World/Chunk
publication. Actual Class-query membership now drives positive detector contact,
deferred scheduling and deactivation. The local contract tests pass 7,270
constructor checks across 164 cases and 196 detector checks across 24 literal
cases per compiler. These are constructed native scenarios, not original
Minecart observations.

[Hosted validation at `39edf12`](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/minecart-construction.md#committed-and-hosted-checkpoint)
passes all 177 MSVC and 179 GCC/Clang tests, zero failures/errors/skips,
527-file formatting and the separate mirror/interval checks. Linux uses
ASan/UBSan and leak checks. The linked source map keeps the earlier failed
Windows build and local D result separate from that committed checkpoint.

A separate [original/Windows MSVC constructor comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/minecart-construction.md#constructor-observations-and-qualification)
at `39edf12` matches all ten literal null-World inputs per client/server side,
with zero mismatches across the full typed declared projections. Both capture
sets passed all 22 protocol and ownership rules. These observations contain
no World publication, detector, rail, inventory or NBT call. The later native
inventory/NBT/storage contracts are separate. Concrete movement/collision,
active Minecart ticks and rail travel remain
unfinished. The C runtime and native contract tests require no JVM; original
archives and Java tools remain optional reference-development dependencies.

## Power and reference observations

The [power contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/power.md)
explains ordered World queries, conductor paths and concrete Block outputs.
Its [source map and evidence](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/power.md)
record a separate original/native MSVC comparison at source `9f9d3a2`.

All 5,365 literal rows per client/server side agree on seven whole snapshot
fields before and after each call: block bytes, live and saved metadata bytes,
live-metadata nullness, Chunk coordinates and loaded state. Completion agrees
for every row; booleans agree for the 5,164 normally completed rows per side.
The 201 failed rows per side contribute no boolean comparison. Exact error
objects, status identities, source PCs, object graphs, setup and absolute
Chunk dirty/populated history are outside that projection. It establishes no
rail, redstone-propagation or whole-game equality.

## Other systems and current scope

[Normal population](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/normal.md)
describes the terrain controller and its reached World operations. The
[architecture](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/architecture.md),
[testing guide](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md) and
[parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md)
separate implemented bodies, constructed native tests and finite original
observations. Original JARs and Java reference tools are optional development
dependencies; they are not required by the C runtime or bundled with it.

The deeper [systems](Architecture), [build](Build-and-tests) and
[storage/reference](Storage-and-reference) wiki pages retain earlier checkpoint
records. Use their current-status notes before interpreting old counts or
unfinished-work statements. [History](History) preserves the former overview
and navigation without treating them as current status.
