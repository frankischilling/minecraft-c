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
actual Sky drawing and W/SR/ChunkRenderer culling. It now enters the dirty-chunk
selector, constructs the real ChunkCache and compiles standard Blocks through
the same RenderBlocks and Tessellator. Current frame source calls real
[terrain submission](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/terrain-submission.md)
at `px.a(FJ)V:486`; [standard lighting](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/standard-lighting.md)
and the actual camera eye getter now return before Entity/TileEntity rendering510.
Native Windows lighting replay matches 86 original state/parameter rows. Three
focused tests pass in 2.05 seconds; hosted lighting qualification is pending.
The loaded UI passes in 18.59 seconds with all 72 focus histories/resources/saves. Its inspected
back buffer shows authored grass terrain and Sky without HUD. Specialized Block shapes, later frame
drawing and gameplay actions remain unfinished. See the
[source frame scope](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/renderer-frames.md).
The real READY renderer also
completes [picking](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/picking.md)
through return452; existing-World ticks call it at `k:33`.

Renderer attachment creates the source ChunkRenderer grid and bounding-box
lists. Empty and standard Block sections now complete source compilation;
specialized Block shapes and TileEntity rendering remain unfinished. The
submission extension uses the same retained graph. Its preceding terrain
checkpoint passes all four hosted jobs and 1,513 tests; see
[Build and tests](Build-and-tests) for the distinct device scopes.
The concrete sky-change callback runs
the whole `n.e()V`, and standard lighting includes the shared U class's actual
buffer initialization. The [chunk-compilation contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/chunk-compilation.md)
records source effects, retained failures and remaining children. Skin and cloak
requests use actual asynchronous image owners, with separate factory and worker
results. Native
cleanup retains dependencies through worker stop/join and refuses retirement
while external owners remain.

Version-zero conversion, new-World spawn completion, remaining menus, native
Canvas integration, gameplay, multiplayer interoperability and the standalone
server remain unfinished or unqualified. Diagnostic title and fixed-camera
terrain inspection remain explicit options.

Selected Windows checks pass 950 original cache rows, 42 whole selector profiles,
a complete empty DK call, eight sky-change profiles, three lighting rows and
108 JDK17 sort histories.
The loaded graph reaches the geometry frontier and preserves all 72 focus
histories. After the lease-publication fix, Windows Debug ALL and all 512
selected CTests pass in 215.93 seconds, retaining all 506 prior test names and
adding six. Seven separate JDK/Sky device cases also pass.

At the previous [standard Block geometry checkpoint](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/render-blocks.md),
six focused MSVC tests passed, including 1,680 face/color rows, 25 flat rows,
48 AO rows, 25 Block colors and seven actual static AO queries. The loaded UI
passed 702 checks and preserved resources, saves and all 72 focus histories
before submission was connected at PX486. Hosted [source checkpoint 6ccdc82](https://github.com/frankischilling/minecraft-c/actions/runs/40947497797)
passed all four jobs and 1,490 tests (Windows 478/GCC 506/Clang 506), zero failures
or skips. Every preceding hosted name remained, with five additions per
platform. That record's runtime/test sources were those of 6ccdc82; its later
record edits changed documentation and file bindings. Current submission passes its specified Windows profiles; hosted
qualification is pending. Remaining suite totals describe earlier
checkpoints.

Qualified source checkpoint
[`91da6e2`](https://github.com/frankischilling/minecraft-c/actions/runs/40937788071)
passes all four hosted jobs and 1,475 tests: Windows 473/GCC 501/Clang 501,
with zero failures or skips and every preceding hosted test name retained.
Hosted Linux runtime passes are separate from the local build evidence; no
local Linux runtime tests ran after host-disk exhaustion.
[Build and tests](Build-and-tests) records exact times and the device-profile
difference: hosted Windows executes five additions, while local Windows and
both Linux software-GL profiles execute all six.

At the preceding fog/Sky/culling checkpoint, all 506 selected Windows CTests
pass after Debug ALL. Audio-enabled Linux GCC13/Clang18 ALL builds and 489
selected public tests each pass with sanitizers,
leak detection and real GL. Six JDK-reference cases pass per compiler. The
separate local Windows Sky device replay matches 48 original post-call states.
[Qualification](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md#source-fog-sky-and-chunk-culling-qualification)
separates 366 World/provider leaves, 48 fog states, 30 exact GPU samples, eight
culling rows and four Sky prefix outcomes. NV-absent Linux state comparisons
retain their source-derived selector scope. Terrain frames and play remain
unfinished.

Published [`e65860c`](https://github.com/frankischilling/minecraft-c/actions/runs/40920088558)
passes all four jobs and 1,458 tests: Windows 468/GCC 495/Clang 495, with zero
failures or skips. It precedes the new compilation and callback work.

Earlier published [`06aa85f`](https://github.com/frankischilling/minecraft-c/actions/runs/40902957232)
passes all four jobs and 1,448 tests: Windows 466/GCC 491/Clang 491, zero failures
or skips. It precedes the fog/Sky/culling checkpoint.

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
