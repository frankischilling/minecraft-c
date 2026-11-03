# minecraft-c

[Torch World rendering](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/torch-world.md)
now supplies the type 2 wrapper and shared primitive for Torch and both Redstone
Torch registrations. Native replay matches 132 rows from 66 original calls and
14,640 appended words. Smoke and Flame constructor/tick/draw comparisons also
pass. The loaded Torch scene passes 1,624 checks in 13.87 seconds: a separately
entered World draw visits both callback-created identities once, then ordinary
execution returns five World/HUD frames with mesh reuse, dirty recompile and
cleanup. The identity proof and ordinary frame continuation have distinct scopes.
Original particle evidence retains independent constructor entropy and exact
deterministic components. Original pixels and general FPS remain unqualified.

Final public CTest runs pass 660 Windows tests in 338.89 seconds and 653 Linux
Clang sanitizer tests in 162.88 seconds. Both preserve4,548 source inputs and 532
products; all 1,445 runtime files match across hosts. Four Workbench shutdown
profiles pass 8,137 checks in 59.92 seconds, and the three harvest scenes pass
in 33.02 seconds. Their detailed receipts and historical scopes remain in the
[testing record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md).

The preceding Cactus commit `193192e1` passes
[all four hosted jobs](https://github.com/frankischilling/minecraft-c/actions/runs/41678998367):
608 Windows, 665 GCC and 665 Clang tests, plus formatting. That hosted result
precedes the current Torch and particle changes.

[Cactus World rendering](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/cactus-world.md)
now completes section compilation and a finite Windows scene with Cactus on
Sand. Exact native CPU replay matches 25 original calls; loaded frames check
picking, selection, mesh reuse, WorldAccess recompile and native cleanup.
Original pixels, extended play and FPS remain unqualified.

[Specialized Block Items](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/inventory-geometry.md)
now reach Cactus, Stairs, Fence and Piston inventory rendering. Native geometry
matches 7,596 active words across 216 original batches; Piston textures match
34,612 rows. Native HUD controls draw all six registered specialized Items.
World rendering, original pixels, unrestricted play and general FPS retain
their separate unfinished scopes.

The Cactus runtime checkpoint passes all 654 Windows MSVC Release and all 648 Linux
Clang ASan/UBSan public tests, including the new finite Cactus controls.
Both runs preserve 4,507 sources and 529 products; all 1,442 runtime files
match across hosts. Earlier focus failures remain separate observations.
Unrestricted single-player play, desktop stability and general FPS remain open.

[Block inventory rendering](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/block-inventory.md)
now resolves color and bounds methods for all 69 represented Block kinds.
[TallGrass harvesting](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/tall-grass-harvest.md)
includes real Seed drops, drawing, pickup, held rendering and Inventory in a
finite Windows source-input scene. Sand has its own complete scene.

The [Light-drain comparison](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/tests/benchmarks/light_drain.md)
retains 42 CPU samples: median drain time falls from 3.2821 to 3.0601 ms
(6.76%), with the declared work/state checks unchanged. Gameplay FPS remains
unqualified for this optimization.

[Leaves, Snow-layer and Ice harvesting](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/block-harvest.md)
now matches 63 complete original controller scenarios. The Windows Leaf/Shears
scene also returns actual mining, tinted particles and the dropped Leaf Item,
with strict NBT and checked native cleanup. These are finite scenarios;
unrestricted play and original pixels remain unqualified.

Owned provider presence uses its existing index outside mutation scopes. At
512 loaded Chunks, median presence hits fall from 124.02 to 15.16 ns and
saved-light reads from 212.00 to 87.42 ns. All CPU controls and samples remain
recorded. Its failed frame cohort supplies no additional FPS gain claim.

[World query CPU measurements](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/performance/world-queries.md)
now cover faster block, metadata and light reads with unchanged query results,
Random state and ownership. These cached-query measurements do not establish
gameplay FPS improvement. [Held Item mining](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/item/mining.md)
now runs speed, harvesting, wear, breakage and statistics through the actual
single-player controller, with finite original comparisons.

A separate matched Tiny sky-view pair measures 5.37 to 19.95 FPS, with p95
host frame time falling from 625.66 to 109.69 ms. The complete longer repeat
also retains rejected baseline intervals; general FPS and Windows desktop
stability remain unqualified. The same runtime passes all 632 Linux sanitizer
public tests. Windows public cohorts retain their 633/637 and 636/637 results
and unexplained zero-draw assertions.

minecraft-c is a C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows
and Linux. Single-player is the current priority. Multiplayer and a dedicated
headless server remain part of the target. Usernames are local; account
authentication is excluded.

The client is still pre-playable. Existing-save loading, movement, targeting,
empty-hand Dirt mining, pickup, held-block drawing and collected-Dirt placement
have finite native scenarios. The Create World screen also completes a genuine
seed 777 generation/save/title/reselection scenario. These scenarios include ordinary save/title
transitions, reselection and independent decoding of saved state.

[Automatic Workbench close](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/workbench-auto-close.md)
now runs from the actual Player tick when its Table disappears or the Player
moves too far away. Four Windows client tests pass across five fresh processes,
including exact saves and failure/fresh-default controls. Selected structural
rows match unchanged-original GS calls. Native cleanup also counts the actors
held by complete pickup particles.

[Native shutdown with an open Workbench](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/workbench-open-shutdown.md)
passes four Windows profiles for clean closure and actual Container, Player
and World hold retries. The same Container survives through a sealed Player
owner, with Source fields and stopped save bytes unchanged. Ordinary Screen
retirement retains its BUSY check; cleanup adds no Source close or save.

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
unqualified. Complete Source shutdown, GC timing, cold-preview print continuation, original pixels,
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
