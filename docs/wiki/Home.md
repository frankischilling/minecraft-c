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
then executes gameplay input through `Minecraft.k:1073` before the unfinished
World tick at1074. Movement key states, hotbar selection, wheel input and GUI
toggles are connected; terrain drawing, Player movement and gameplay actions
remain unfinished. The real READY renderer also
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

All 488 selected Windows CTests pass locally after the Debug ALL build, with
zero failures or skips. The two new input/controller replays also pass on
GCC13 and Clang18 with ASan/UBSan and leak detection. They compare 63 original
inventory-wheel rows and 60 controller rows. Complete World rendering and play
remain unfinished. [Input scope](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/gameplay-input.md)
separates the connected dispatch from unfinished movement and gameplay.

Published [`3e05fcf`](https://github.com/frankischilling/minecraft-c/actions/runs/40853180366)
passes all four hosted jobs and 1,391 tests: Windows 449, GCC 471 and Clang 471,
with zero failures or skips and matching source trees. It includes renderer
attachment and image workers. Picking has later finite local comparisons;
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
