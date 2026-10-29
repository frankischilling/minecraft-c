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
then stops at gameplay input `Minecraft.k:337`; terrain drawing, movement and
the connected World tick remain unfinished.

Renderer attachment creates the source ChunkRenderer grid and bounding-box
lists. It does not compile terrain meshes. Skin and cloak requests use actual
asynchronous image owners, with separate factory and worker results. Native
cleanup retains dependencies through worker stop/join and refuses retirement
while external owners remain.

Version-zero conversion, new-World spawn completion, remaining menus, native
Canvas integration, gameplay, multiplayer interoperability and the standalone
server remain unfinished or unqualified. Diagnostic title and fixed-camera
terrain inspection remain explicit options.

All 482 selected Windows CTests pass locally, with zero failures or skips,
alongside the client/headless builds, 86 strict compiler checks and 30 selected
Linux sanitizer cases. Complete World rendering and play remain unfinished.

The preceding published
[`add19c6` hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40829676327)
passes all 1,374 selected tests: Windows 444, GCC 465 and Clang 465, with zero
failures or skips and matching checkout trees. That checkpoint precedes the
renderer-attachment and image-worker extension described above. Local, hosted
and finite original-game checks have separate scopes in [Build and tests](Build-and-tests).

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
