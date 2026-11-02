# Client startup

Build and import resources using [Build and tests](Build-and-tests). The ordinary
`minecraft-client` executable creates a native Frame and embedded Canvas, starts
its graphics and audio services, and runs the source menu and input loop.
No original archive, launcher, JVM or account service is required after import.

Existing Worlds can be selected from the menu. The loaded client connects
physical input, Player and World ticks, targeting, terrain, Mob drawing, HUD,
particles and the first-person hand. The current native interaction scenarios
use Tiny render distance and authored saved inputs.

## Qualified interaction paths

- [Create World](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/create-world.md): actual name/seed fields, normal seed 777 generation and initial save, World/HUD frames, ordinary save/title and reselection with independent decoding of 390 Chunks and captured Player/WorldInfo values.
- [Mining and pickup](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/mining.md): fresh left click, held empty-hand Dirt break, cracks, destruction particles, dropped-Item drawing, save/reselection, keyboard pickup and held Dirt.
- [Dirt placement](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/placement.md): right-button use of collected Dirt, collision admission, terrain update, sound, consumption, statistic and swing, followed by another save/reselection.
- [Player drops](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/player-drop.md): the configured drop key removes one selected Item, publishes a genuine dropped Item and increments the drop statistic. Empty-hand input creates no Item.
- [Animal damage](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/animal-damage.md): component and original CPU checks cover actual Wolf bites, health, hurt resistance, knockback, sound and failure prefixes.
- [Animal death and drops](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/animal-death.md): finite native and original CPU checks cover attacker callbacks, Sheep/Pig/Cow/Chicken drops, real Item publication and failure cleanup. Complete graphical encounters remain unqualified.
- [Saving](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/world-saving.md): normal leave and separately qualified session-lock failure, with independent disk checks and native cleanup.
- [Selection](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/block-selection.md): Dirt and Snow outlines and finite saved-World reselection.

Each linked contract records its exact verification scope and retained failures.
Component tests, static original-bytecode identities, executed original
comparisons and native graphical scenarios provide separate evidence.

## Remaining work

The client remains pre-playable. General tools, combat, Inventory UI, remaining
Item/Block overrides, broader vehicle behavior, focus/native-close behavior,
unrestricted reselection and extended play still need work or qualification.
The retained loader currently admits eight World loads per application session.
Linux loaded gameplay, default-Far performance and complete original pixel/audio
and save-interoperability comparisons remain unqualified.

[Current status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md)
and the [parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md)
track the implementation and its evidence. Historical checkpoints remain in
[History](History).
