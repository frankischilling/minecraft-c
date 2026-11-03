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

- [Water and Lava rendering](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/fluid-world.md): 104 original CPU calls match native replay. Saved Water/Lava scenes return World/HUD frames, select Sand through liquid, reuse compiled meshes and recompile after a light notification. Position/UV words match the original scene profiles; evolving colors retain separate observations.
- [Create World](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/create-world.md): actual name/seed fields, normal seed 777 generation and initial save, World/HUD frames, ordinary save/title and reselection with independent decoding of 390 Chunks and captured Player/WorldInfo values.
- [Mining and pickup](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/mining.md): fresh left click, held empty-hand Dirt break, cracks, destruction particles, dropped-Item drawing, save/reselection, keyboard pickup and held Dirt.
- [Dirt placement](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/placement.md): right-button use of collected Dirt, collision admission, terrain update, sound, consumption, statistic and swing, followed by another save/reselection.
- [Player drops](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/player-drop.md): the configured drop key removes one selected Item, publishes a genuine dropped Item and increments the drop statistic. Empty-hand input creates no Item.
- [Player attacks](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/player-attack.md): Entity-target left click, empty-hand animal damage, score/kill callbacks and owned-Wolf alert retain earlier 747-check/five-original-row Windows/Linux component snapshots. The frozen Windows unarmed SDK profile passes 2,106 checks in 13.22 seconds, completing the click, twenty death particles, age-21 removal, independently audited autosave and cleanup. Earlier missing-GY, immutable-disk, focus/Pause and unconsumed-click failures remain historical. Instrumented Slime/Cow/Wolf failures record foreign Remote Desktop foreground activation; older owners remain unknown. The new 611/606 Windows/Linux public-suite passes do not establish global focus stability or a Linux saved-World SDK pass. Hosted checks apply to their exact tested commit.
- [Animal damage](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/animal-damage.md): component and original CPU checks cover actual Wolf bites, health, hurt resistance, knockback, sound and failure prefixes.
- [Held Item components](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/item/combat.md): all 38 actual Item Classes, source durability/Inventory clearing and direct original Sword/Bow extrusion have finite original comparisons, with a 202-ID inventory and independent Cow-death audits. The damaged-Item bar matches nine profiles/576 active words, distinguishing original 24 draws from native zero-draw CPU prefixes. The frozen Windows SDK Dirt profile passes 1,446 checks in 14.20 seconds; Iron Sword break passes 3,631 in 16.74 seconds. Each has three before/after hand frames, the actual click, death/removal and exact live-40-slot/NBT autosave checks. Latest runs publish Wool without pickup; a separate earlier 13.38-second Sword pass proves pickup/slot-0 clone. Sixteen public Inventory corruption controls run in CI. These finite scenarios do not establish original whole-frame, unrestricted-play or FPS equality.
- [Animal death and drops](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/animal-death.md): finite native and original CPU checks cover attacker callbacks, Sheep/Pig/Cow/Chicken drops, real Item publication and failure cleanup. Complete graphical encounters remain unqualified.
- [Saving](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/world-saving.md): normal leave and separately qualified session-lock failure, with independent disk checks and native cleanup.
- [Selection](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/block-selection.md): Dirt and Snow outlines; a retained Windows Tiny scenario completes ten saved-World loads and saves, then preserves an eleventh conversion failure before storage acquisition.

Each linked contract records its exact verification scope and retained failures.
Component tests, static original-bytecode identities, executed original
comparisons and native graphical scenarios provide separate evidence.

## Remaining work

The client remains pre-playable. General tools, combat, Inventory UI, remaining
Item/Block overrides, broader vehicle behavior, focus/native-close behavior,
unrestricted reselection and extended play still need work or qualification.
The loader has no fixed session count limit. Old World graphs remain allocated
until shutdown; reclaiming them during play remains unfinished.
Linux loaded gameplay, default-Far performance and complete original pixel/audio
and save-interoperability comparisons remain unqualified.

[Current status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md)
and the [parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md)
track the implementation and its evidence. Historical checkpoints remain in
[History](History).
