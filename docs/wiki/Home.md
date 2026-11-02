# minecraft-c

minecraft-c is a C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows
and Linux. Single-player is the current priority. Multiplayer and a dedicated
headless server remain part of the target. Usernames are local; account
authentication is excluded.

The client is still pre-playable. Existing-save loading, movement, targeting,
empty-hand Dirt mining, pickup, held-block drawing and collected-Dirt placement
have finite native scenarios. The Create World screen also completes a genuine
seed 777 generation/save/title/reselection scenario. These scenarios include ordinary save/title
transitions, reselection and independent decoding of saved state.

The configured E key now opens the actual nonpausing Player Inventory.
Windows SDK profiles move saved Items, draw the full Player preview and held
Dirt, close a carried Stack through real World publication, save exact
Inventory/Item NBT and finish cleanup. A second profile adds the Pumpkin HUD
overlay and separate helmet/held-Item renderer children. Original Container
and selected Inventory drawing observations keep their finite comparison
scopes. See [Inventory behavior and evidence](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/inventory.md).

The [Workbench scenario](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/workbench.md) now crafts and places a Table, opens its actual 3x3 screen,
makes and equips a Wooden Pickaxe, closes with leftover ingredients and performs
ordinary Save and Quit. The Windows run checks all 40 Inventory cells, all three
saved Items, 403 Chunks, the placed Table, unrelated region bytes and cleanup.
Source inputs and imported assets remain unchanged. The reached
[Creeper environmental-damage path](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/entity/creeper-damage.md) has native controls and static evidence;
it has no executed-original damage comparison.

Remaining Item/Block actions, broader combat and Inventory behavior, unrestricted
reselection, default-Far performance and extended play remain unfinished or
unqualified. Live-open Workbench terminal cleanup safely returns `BUSY`;
distance/removal close, cold-preview print continuation, original pixels,
Linux SDK gameplay and FPS remain unqualified. A retained Windows Tiny scenario
completes ten saved-World loads and saves, then preserves an eleventh conversion failure before storage
acquisition. The loader has no fixed count limit, but old World graphs remain
allocated until shutdown. Original assets must be imported locally; the runtime
then needs no JVM.

- [Client startup and controls](Startup)
- [Build, resources and tests](Build-and-tests)
- [Current status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md)
- [Parity matrix and verification limits](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md)
- [World simulation](World-simulation)
- [Storage and reference evidence](Storage-and-reference)
- [Architecture](Architecture)
- [Earlier checkpoints](History)

Detailed implementation notes and test evidence live with the source. A passing
component or finite graphical scenario does not establish complete gameplay,
whole-frame equality or original-world round-trip compatibility.
