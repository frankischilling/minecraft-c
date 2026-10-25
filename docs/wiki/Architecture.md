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
  Reader/Writer for real-file persistence. The App now loads one owner after
  Display and before title font/texture/layout consumers. Its
  [GameDirectory](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/game-directory.md)
  owner performs real native path selection and creation and stays live above
  settings. Loaded scale and initial anaglyph reach the title; explicit scale
  overrides include zero, and startup/retirement perform no save. Windows
  native tests pass 57 directory checks plus six file checks and nine App cases
  with eight exact decoded-pixel relations. Hosted `ea1aefc` also passes
  GameDirectory on MSVC/GCC/Clang and startup on Linux within 680 complete cases
  and 644-file formatting. General File/locale behavior, ordinary defaults,
  custom packs, audio and source pacing remain unfinished.
  The binding-code action selects the
  current array entry, stores the code and invokes the same real save body.
  Source null/index failures and native errors remain distinct; later save
  failures retain the code and file prefix. The raw binding field edit still
  does not save. Hosted checkpoint `fce1539` passes native action contracts on
  MSVC and sanitized GCC/Clang. Original-action comparison and Controls screen
  adoption remain pending.
  [GameOption queries and labels](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/game-options.md)
  keep metadata and value queries headless while client label composition uses
  the real shared Translation and Assets owners. Direct singleton admission
  remains separate from wrapper initialization. Hosted checkpoint `ea1aefc`
  passes their native contracts on MSVC and sanitized Linux GCC/Clang;
  original-label correspondence remains pending. New float/discrete actions
  pass one Windows headless profile with 702 native checks and eight file-check
  categories, using real stores and completed-path save. Six audio/renderer
  paths stop after assignment at the first missing receiver field; no callback
  or later save is substituted. Hosted `2afd39a` adds the native actions on
  sanitized GCC/Clang within 683 complete passing cases. Original-action
  comparison, Options UI and real effect owners remain unfinished.
- [Sound resources](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/sound.md)
  own SoundPool entries, retained file URLs, actual raw/MUS streams and sequential
  Vorbis decoding. Their six selected Windows contracts include nonzero
  float conversion matching the original scalar operations and bounded planar
  interleaving, silence inputs and authored nonzero streams. A focused Ubuntu GCC ASan/UBSan
  decoder run passes 563 checks after exact private-copy Vorbis corrections;
  a corrected Clang run also passes 563 after the private Ogg null-buffer guard.
  The `aa39f8f` checkpoint passes all 695 native cases on Windows/GCC/Clang.
  A separate nonzero regression passes 5,469 checks on sanitized Clang and
  5,480 on MSVC after a bounded floor-deviation correction. Both runs pass
  independent source, dependency and capture readback.
  An arranged original mono control returns null/EOS where native decoding
  emits 6,400 bytes from the same input. A separate nonzero mono comparison
  returns 4,096 samples on both sides but differs in four samples by one PCM
  step. Complete capture readback confirms failed exact equality, without
  tolerance. Original codec policy, wider PCM comparisons,
  device playback, shared SoundManager setup and real GUI/WorldAccess adoption
  remain unfinished.
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
