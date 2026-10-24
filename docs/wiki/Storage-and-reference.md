# Storage and original references

The [fresh World population comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/natural-population.md)
uses genuine original constructors and the production GAME implementation for
seed23 origin and ring profiles on both sides. Ring retains nine normal Chunks,
populates four and ends with 6,968 ordered light regions. Both profiles compare
complete retained arrays, cache/light ordering, numerical arrays and aliases,
and Random continuations. Public ring replay checks full original numerical
and cache golden records; public origin replay checks the older cache oracle.
Both public native replays use separately declared minimal resources. The older
MCC1 callback replay keeps its imported-effect scope.

The [populated Chunk save/readback profile](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/population-persistence.md)
adds two genuine cache saves and four direct loads in the same World. Complete
typed NBT joins the actual saved Chunk arrays; fresh readbacks use each
implementation's real block map. The second save and every read preserve all
raw disk bytes. The [full-save profile](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/population-full-save.md)
adds held-World metadata, empty named collections and exact metadata rotation.
The separate [named MapData profile](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/population-named-data.md)
compares a dirty default value through two full saves and a separate manager
load on both sides and all three local compilers. It retains the same World
and checks the fresh loaded value, shared name owner, colors, complete typed
NBT and every captured file. Nonempty Players, terrain updates, trackers and
packets, broader subtypes, source I/O failures, unrestricted ordering,
dedicated runtime, unloading and restart remain open.

Storage layers have separate contracts and ownership:

- [Raw NBT](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/storage/nbt.md) owns decoded documents, shared mutable byte-array handles and separate immutable source copies.
  Shared decoded arrays retain their document after its caller hold is released;
  mutation requires exclusive access.
- [Compression](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/storage/compression.md) consumes borrowed byte spans and
  returns owned buffers; limits and compressed-stream boundaries are explicit.
- [SavedData](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/saved-data.md) owns ID counters, the named cache and ordered values.
  GAME managers bind the client/server base type; MapData values and save records
  retain shared colors owners. Copied native path bindings have their own lifetime.
- [Read-only region records](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/storage/region.md) validate borrowed spans.
  World writes use the separate [storage](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/storage.md) and
  [saving](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/saving.md) contracts.
- [Minecart inventory and storage](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/minecart-inventory.md) document
  semantic NBT and optional per-occurrence preservation; native preservation
  policy has its own finite tests.

The [binary audit](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/binaries.md) identifies the supplied
client and server archives by hash. Original archives and imported resources
remain local; the [resource record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/resources.md) describes
observed inputs without claiming rendered or audible parity.

The [plant lifecycle comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/plant-lifecycle.md) matches
59 prepared cases per side: full bounded block/metadata maps, both four-field
Random continuations and ordered actual Item drops. Public replay uses fixtures
derived from those original captures. Prepared World services execute unchanged
Block hooks and selected tree bodies; original World/Chunk, WorldAccess, neighbor
and light-propagation parity remain outside this result. Native
[plant contracts](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/plants.md) describe their own GAME services.

See [testing](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md), [development status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md),
the [parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md) and
[World simulation](https://github.com/frankischilling/minecraft-c/wiki/World-simulation) for current evidence and remaining work.

<a id="storage-foundation-checkpoint"></a>
<a id="lifecycle-qualification"></a>
<a id="normal-generation-checkpoint"></a>
<a id="cache-and-region-checks"></a>

The [complete former storage/reference page](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-storage-reference.md)
preserves the historical [storage foundation](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-storage-reference.md#storage-foundation-checkpoint),
[lifecycle qualification](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-storage-reference.md#lifecycle-qualification),
[Overworld generation](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-storage-reference.md#normal-generation-checkpoint)
and [cache/region](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-storage-reference.md#cache-and-region-checks)
checkpoints, together with their technical details and evidence limits.
