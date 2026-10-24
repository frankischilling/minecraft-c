# Systems and subsystems

The rewrite uses C17 libraries with explicit module boundaries and ownership.
The [runtime architecture](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/architecture.md) records those contracts;
public headers and implementations follow the same system/subsystem layout.

Current runtime navigation:

- [Native client startup](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/startup.md)
  owns SDL input, resources, GUI and OpenGL presentation. The shared
  [float parser](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/core/java-float.md)
  and [output writer](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/core/java-writer.md)
  remain usable without client graphics dependencies. The writer owns its
  encoder and PrintWriter policy; caller-owned byte sinks and native file
  adapters retain separate lifetimes.
  [GameSettings](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/game-settings.md)
  owns retained options, KeyBindings and binding arrays and uses the shared
  Reader/Writer for real-file persistence. The application has not yet adopted
  it or the effectful option setters. Its new binding-code action selects the
  current array entry, stores the code and invokes the same real save body.
  Source null/index failures and native errors remain distinct; later save
  failures retain the code and file prefix. The raw binding field edit still
  does not save. Native Windows action contracts pass; Linux action execution
  and an original-action comparison remain pending. No Controls screen has
  adopted it.
  [GameOption queries and labels](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/game-options.md)
  keep metadata and value queries headless while client label composition uses
  the real shared Translation and Assets owners. Direct singleton admission
  remains separate from wrapper initialization. Hosted checkpoint `2e02a61`
  passes their native contracts on MSVC and sanitized Linux GCC/Clang;
  original-label correspondence remains pending.
- [Server construction](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/server/construction.md)
  owns default fields and native lifetime; dedicated World construction selects
  the actual cache and retains separate subclass and Entity-index owners.
- [World construction](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/construction.md),
  [queries](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/queries.md) and
  [mutations](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/mutations.md) define actual World and Chunk services.
- [Plant callbacks and growth](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/plants.md) dispatch through concrete
  Block receivers, including survival, drops, saplings and farmland.
- [Overworld population](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/population/overworld.md) documents feature
  placement, generation dependencies and remaining acceptance work.
- [Entity movement](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/movement.md) and
  [Minecart inventory](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/minecart-inventory.md) retain separate
  ownership, mutation and persistence contracts.
- [World storage](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/storage.md) and
  [saving](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/saving.md) describe stable Chunk ownership and writes.
- [SavedData](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/world/saved-data.md) owns ID counters, named MapData and ordered aliases through side-bound managers.
  [Shared NBT byte arrays](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/storage/nbt.md#decode-and-ownership) retain decoded payloads separately from immutable provenance.
  The [named comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/population-named-data.md) records the finite held-World scope.

The [World simulation page](https://github.com/frankischilling/minecraft-c/wiki/World-simulation) provides related navigation.
Finite original plant comparisons have the exact scope recorded in the
[plant lifecycle evidence](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/reference/plant-lifecycle.md). They compare
original Block bodies with prepared services against native GAME observations;
original World/Chunk, WorldAccess, neighbor and light-propagation parity is unproved.

See [development status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md),
[testing](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md) and the [parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md)
for current results and unfinished behavior. There is no playable C client or
dedicated server yet.

<a id="save-unload-and-retirement"></a>
<a id="normal-generator"></a>

The [complete former architecture page](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-architecture.md)
preserves earlier technical checkpoints and their original evidence limits,
including [save, unload and retirement](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-architecture.md#save-unload-and-retirement)
and the [Overworld generator](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/history/wiki-architecture.md#normal-generator).
