# minecraft-c

The single-player client can create and load saved Worlds, run Player and World
ticks, draw terrain and Mobs, and reach mining, placement, pickup, crafting and
save/reselection in finite Windows scenarios. Complete Beta 1.7.3 parity and
unrestricted play remain unfinished. Single-player is the current focus.

[Food and soup use](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/item/food-use.md)
now preserves immediate healing, count changes and Bowl replacement through the
ordinary ItemStack/controller path. All 28 selected original method observations
match Windows and sanitized Linux native replay. The separate
[Registry random-tick measurement](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/performance/registry-random-ticks.md)
reduces CPU time for its fixed-work workloads while retaining every sample;
it establishes no scene-FPS improvement.

The saved Windows Food scene also completes actual Apple/Soup clicks, health
5 to 9 to 19, Bowl replacement, HUD/held drawing and save/reselection with
independent NBT readback. Linux loaded rendering reaches its pre-input gate;
Linux Food interaction and persistence remain unqualified.

[Hoe and Seeds use](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/item/farming-use.md)
now tills Farmland and plants Crops through the ordinary ItemStack wrapper.
Original-method comparisons and separate native controls cover source order,
count, wear, statistics and failure effects. A saved Windows scenario completes
actual right-clicks, hotbar selection, mesh updates, Save and Quit and reselection,
with independently decoded Region/Inventory values. Broader farming remains open.

[Crops rendering](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/crops-world.md)
now matches 44 original calls and four metadata stages through real section
compilation. A saved Farmland/Crops scene checks the loaded Player camera,
picking/outline, all 48 original position/UV records, mesh reuse, light-driven
recompile and cleanup. Full farming and original pixels remain unfinished.

[Ladder rendering](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/ladder-world.md)
now matches 35 original calls and all four orientations through real section
compilation. A saved mounted Ladder scene checks the loaded Player camera,
picking, outline, immutable mesh reuse, light-notification recompile and cleanup.
Water, Lava, Torch and Cactus have their own original comparisons and loaded
scenes. Other World render families still need implementation.

RenderBlocks and section compilation use a private ChunkCache Block accessor
that constructs complete diagnostics only on failure. The public accessor
retains its success record. An independent 84-sample Windows confirmation
reduces median CPU time for 65,536 real reads from 8.39885 to 0.74005 ms (91.19%).
Values, failure details, owners, Random state and Chunk planes remain checked.
The [CPU measurement](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/tests/benchmarks/chunk_cache.md)
has a separate scope from frame performance.

[Narrower frame profiles](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/tests/benchmarks/client_frames.md)
identify actual section compilation and random Chunk ticking as major measured
CPU costs. Every random Chunk still performs all eighty Block samples. Natural
spawning, loading, generation, saves and source deadlines remain active.
These attribution intervals do not compare runtime speed.

The random Chunk sampler now builds full diagnostics only on failure while
retaining all samples, ownership and Random state. Independent CPU confirmation
reduces median time by 14.92% for air and 9.60% for stone across 361 Chunks and
28,880 samples. The [raw Chunk benchmark](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/tests/benchmarks/world_random_tick.md)
keeps all observations and outliers. These results do not measure scene FPS.

Original pixels/audio, broader persistence, Linux loaded gameplay, sustained
performance, remaining Block/Item actions and extended play still need work.
Multiplayer interoperability and the dedicated server remain later milestones.
Account authentication is the only approved gameplay exception. Import original
resources locally; the normal C runtime then needs no JVM or original archive.

- [Client startup, controls and qualified interactions](Startup)
- [Build, resource import and current tests](Build-and-tests)
- [Current development status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md)
- [Parity matrix and verification limits](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md)
- [World simulation](World-simulation)
- [Storage and original reference evidence](Storage-and-reference)
- [Architecture](Architecture)
- [Earlier checkpoints](History)

Detailed behavior and reproducible evidence live with the source. Component
checks and finite graphical scenarios keep their declared verification limits.
