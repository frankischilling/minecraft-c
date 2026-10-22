# World simulation

The native World modules use real Registry owners, Chunk storage, acquiring
readers and owning Lists. Their tests exercise source-ordered effects and
failure prefixes. They do not provide a playable client, server or complete
gameplay loop.

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
suites pass 175 MSVC and 177 GCC/Clang tests without skips, with Linux sanitizers
and leak checks. Formatting passes for 515 C/header files.

Positive Minecart construction and detection, ordinary positive rail-drop
publication, gameplay rail physics, scheduled draining and rendering remain
unverified. There is no executed-original rail comparison.

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
