# minecraft-c

minecraft-c is a C17 implementation of Minecraft Beta 1.7.3 for x86-64 Windows
and Linux. There is no playable client or dedicated server yet. The native
runtime requires no JVM after local resource import.

The default client constructs the source App, initializes its main menu and
opens genuine World selection. Save metadata, sorting, scrolling, row selection
and same-parent Cancel use the actual source owners. Existing-save loading now
completes all 289 terrain samples, attaches the renderer and particle manager,
inserts the real Player and publishes that Player as the view entity. Healthy
null-screen handling and mouse focus also run. The native selection profile
then executes gameplay input through `Minecraft.k:1073` and the admitted
World/frame segment. Direct unpaused checks on that same graph execute renderer,
held-item/entity ticking, weather and real Biome spawn selection before missing
concrete Mob construction at `cq.a:773`. Focused mouse look and source pause
entry run. The hidden inactive frame opens OZ and completes colors, fog, camera,
actual Sky drawing and W/SR/ChunkRenderer culling before missing compilation 396. Terrain drawing and gameplay actions
remain unfinished. See the
[source frame scope](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/renderer-frames.md).
The real READY renderer also
completes [picking](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/picking.md)
through return452; existing-World ticks call it at `k:33`.

Renderer attachment creates the source ChunkRenderer grid and bounding-box
lists. It does not compile terrain meshes. Skin and cloak requests use actual
asynchronous image owners, with separate factory and worker results. Native
cleanup retains dependencies through worker stop/join and refuses retirement
while external owners remain.

Version-zero conversion, new-World spawn completion, remaining menus, native
Canvas integration, gameplay, multiplayer interoperability and the standalone
server remain unfinished or unqualified. Diagnostic title and fixed-camera
terrain inspection remain explicit options.

All 506 selected Windows CTests pass after Debug ALL. Audio-enabled Linux
GCC13/Clang18 ALL builds and 489 selected public tests each pass with sanitizers,
leak detection and real GL. Six JDK-reference cases pass per compiler. The
separate local Windows Sky device replay matches 48 original post-call states.
[Qualification](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md#source-fog-sky-and-chunk-culling-qualification)
separates 366 World/provider leaves, 48 fog states, 30 exact GPU samples, eight
culling rows and four Sky prefix outcomes. NV-absent Linux state comparisons
retain their source-derived selector scope. Terrain frames and play remain
unfinished.

Published [`06aa85f`](https://github.com/frankischilling/minecraft-c/actions/runs/40902957232)
passes all four jobs and 1,448 tests: Windows 466/GCC 491/Clang 491, zero failures
or skips. It precedes the current fog/Sky/culling extension; new hosted results
remain pending.

Published [`07e41f6`](https://github.com/frankischilling/minecraft-c/actions/runs/40879213780)
passes all four hosted jobs and 1,427 tests: Windows 461, GCC 483 and Clang 483,
with zero failures or skips. It precedes the focused-frame/pause/scheduler extension;
[Build and tests](Build-and-tests) separates those scopes.

- [Client startup](Startup) covers the default runtime and remaining session work.
- [World simulation](World-simulation) links simulation contracts and reference evidence.
- [Build and tests](Build-and-tests) records commands and qualified results.
- [Architecture](Architecture) describes module ownership and boundaries.
- [Storage and reference work](Storage-and-reference) covers saves and reference testing.

The repository's
[status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md)
and [parity matrix](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/parity.md)
distinguish implemented behavior from verified scenarios. Earlier checkpoint
records remain in [History](History) and the linked testing documents.
