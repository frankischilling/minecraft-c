# Client startup

The [RenderGlobal Entity entry](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/render-global-entities.md) now consumes its actual constructor I2
delay after terrain, lighting and eye positioning. The loaded-World frame then
executes particles and translucent terrain before reaching the unfinished weather
renderer at PX932. Native N controls separately
exercise both dispatcher binds and real Player traversal. A playable session
and composed original/native N comparison remain unfinished.

The default native executable runs the constructed source App and initialized
main menu. Explicit diagnostic flags retain the separate title and fixed-camera
World inspection. A playable session remains unfinished.

The [normal startup owner](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/normal-startup.md)
connects Display, the default game directory, save manager, Settings, selected
Pack, RenderEngine, Font, color maps, EntityRenderer and RenderManager. It adopts
the actual Stats writer/callback, invokes the logo, creates keyboard/mouse state
and publishes MouseHelper at `C380`. Empty Controllers discovery returns before
the GL diagnostics and setup. Startup then constructs the capability checker,
binds SoundManager, registers nine texture effects and constructs RenderGlobal
and particles, starts the resource download thread and runs the final GL
diagnostic. The default no-connection path now constructs GuiIngame and the main
menu, publishes the screen through `displayGuiScreen`, performs the typed
statistics save and file rotation, and initializes the scaled screen and buttons.
`Minecraft.a()` returns at offset 749. The bounded startup probe then stops
before the clock read in `Minecraft.run` at offset 32. The production
[menu runtime](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/menu-runtime.md)
continues from the same graph with canonical Timer/input owners, no-World ticks
and source menu drawing.

Controllers owns a real SDL joystick subsystem reference. The official HIDAPI
and virtual joystick drivers are enabled; nonempty physical-device mapping
stops at the explicit SDK Native12 frontier. Polling does not pump or remove
events from the shared queue.

Each GL diagnostic consumes exactly one glGetError result. A nonzero result
uses the counted System.out/PrintStream pipeline with three fresh receiver
reads. Source return, physical output and swallowed I/O errors remain distinct.
Retained calls keep their consumed query, evaluated Strings and captured
streams through cleanup retries without replaying the query or output.

The [resource worker](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/resource-download.md)
uses native HTTP/XML and retains App dependencies until stop/join completes.
The [source screen owners](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/source-screens.md)
cover splash loading, initialization and buttons. The initial main-menu
publication is connected. The default runtime drains actual source mouse and
keyboard methods, ticks the menu and connects Quit to the App run flag.
Singleplayer now constructs the genuine RQ, initializes its real sorted save
List and scrolling owner, and draws selectable rows. Cancel returns to the
exact parent. A synchronous screen change keeps the original source receiver
for the remaining input. Select constructs and publishes the actual `os`, then
enters the source World loader. Existing-save loading constructs the canonical
World, completes 289 terrain samples, attaches renderer/particles and inserts
the real Player. View publication and healthy null-screen/focus handling also
return; the same tick executes gameplay input through `Minecraft.k:1073`
and the admitted World/frame segment.
The actual READY renderer also completes picking through `px.a(F):452`.
Existing-World ticks invoke that body at `k:33`. The admitted provider branch,
controller music tick, movement-key dispatch, hotbar and Settings input also
run. Direct same-graph checks also execute renderer/equip and real entity
updates, weather and actual weighted Biome spawn selection before the missing
concrete Mob constructor at `cq.a:773`.
Frame sound, lighting, suffocation and controller interpolation are connected.
Focused mouse look uses the actual SDK/CU/Player fields. Inactive pause entry
constructs OZ; actual World drawing completes fog, camera, Sky and real
ChunkRenderer culling. It now runs the dirty-chunk selector, constructs the
actual ChunkCache and compiles standard Block geometry into source lists
through the same RenderBlocks/Tessellator. Empty and standard Block sections
complete. Current frame source invokes real
[terrain submission](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/terrain-submission.md)
at `px.a(FJ)V:486`; standard lighting496 and camera eye positioning504 now return.
The loaded Windows profile reaches Entity/TileEntity rendering510, passes in
18.59 seconds and preserves all 72 focus histories/resources/saves. Its inspected
back buffer shows authored grass terrain and Sky without HUD. Three focused
lighting tests pass in 2.05 seconds, zero failures/skips. Hosted source 0c0cc5a
passes all four jobs and 1,520 tests; [Build and tests](Build-and-tests) records
the distinct device scopes. Specialized Block shapes and TileEntity
rendering remain unfinished. The whole `n.e()V` sky-change
callback and shared U class active use are connected. See the
[chunk-compilation contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/chunk-compilation.md).
Whole ordinary World ticks, terrain frames and gameplay actions remain unfinished.
Version-zero conversion stops at 52.
Create/Rename stop at `NEW162`/`NEW189`; Delete stops at preparation 21 without
skipping its prefix. Complete World entry and gameplay remain unfinished. See the
[World-selection contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/world-selection.md).

At the previous [standard Block geometry checkpoint](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/render-blocks.md),
six focused MSVC tests passed, including 1,680 face/color rows, 25 flat rows,
48 AO rows, 25 Block colors and seven actual static AO queries. The loaded UI
passed 702 checks and preserved resources, saves and all 72 focus histories
before submission was connected at PX486. Hosted [source checkpoint 6ccdc82](https://github.com/frankischilling/minecraft-c/actions/runs/40947497797)
passed all four jobs and 1,490 tests (Windows 478/GCC 506/Clang 506), zero failures
or skips. Every preceding hosted name remained, with five additions per
platform. That record's runtime/test sources were those of 6ccdc82; its later
record edits changed documentation and file bindings. Remaining suite totals describe earlier
checkpoints.

Selected Windows checks pass 950 original cache rows, 42 whole selector profiles,
a complete empty DK call, eight sky-change profiles, three lighting rows and
108 JDK17 sort histories.
That checkpoint's loaded UI reaches the earlier nested geometry failure
while preserving saves, resources and all 72 focus histories. The final Windows Debug ALL build and all
512 selected CTests pass after the lease-publication fix in 215.93 seconds.
All 506 prior test names remain, with six additions; seven separate JDK/Sky
device cases also pass.

Qualified source checkpoint
[`91da6e2`](https://github.com/frankischilling/minecraft-c/actions/runs/40937788071)
passes all four hosted jobs and 1,475 tests (Windows 473/GCC 501/Clang 501),
with zero failures or skips and no preceding hosted names dropped. Hosted
Windows executes five additions because its device-only `client.chunk_compile`
case is disabled; local Windows and both Linux software-GL profiles execute all
six. No local Linux runtime tests ran after host-disk exhaustion. The hosted
GCC/Clang runtime passes have their own evidence in [Build and tests](Build-and-tests).

The preceding fog/Sky/culling checkpoint passes all 506 selected Windows CTests
and 489 public cases per Linux sanitizer graph, plus six JDK-reference cases per compiler.
Windows/GCC13/Clang18 ALL builds pass. Original World/provider, fog, Sky and
culling fixtures retain their declared finite scopes. Six source-shader GPU
profiles match all 30 pixels exactly on Windows NVIDIA and Linux Mesa.
Existing-save entry checks paused and direct unpaused segments, source failure
frontiers, save/resource preservation and cleanup. The preceding generated-
terrain entry result retains its separate scope.
The [testing scope](Build-and-tests) separates native, private CPU-wrapper and
finite original-image results from complete playability.

Published [`e65860c`](https://github.com/frankischilling/minecraft-c/actions/runs/40920088558)
passes all four hosted jobs and 1,458 tests: Windows 468, GCC 495 and Clang 495,
with zero failures or skips. It precedes the new compilation/callback increment.

The preceding startup/menu-initialization Windows MSVC Debug source passes all
460 selected CTests without failures or skips. The selected normal Session and GL-error startup profiles
reach the source startup return; RenderGlobal device controls also pass. A
separate headless Release ALL build passes, without utility runtime or server
qualification. The preceding default runtime passes four focused Windows tests for
GuiIngame, uploads, source events and the native menu. Its run completes 100
frames and 32 ticks with cleanup; an 854 by 480 capture was inspected. That
source passes all 463 selected Windows CTests. The later World-selection loop
passes isolated empty/populated, scrolling and double-click Windows profiles.
Empty, populated and bottom-scroll captures were
inspected. Retaining an old RQ blocks teardown while preserving the App, then
cleanup succeeds after its release. DateFormat matches 2,926 JDK17 vectors on
Windows and Linux for English/en_US, UTC/Eastern and UTC years 1967..2099;
other profiles fail explicitly. That earlier full suite and formatting check
passed: all 475 selected Windows CTests without failures or skips and formatting
for 1,379 files. Hosted CI and original-game correspondence retain their source scopes.
Full source run/PX rendering and `Minecraft.d()` shutdown parity are unproven;
native cleanup is a separate implementation.

The later [`d71bd94` hosted run](https://github.com/frankischilling/minecraft-c/actions/runs/40815149868)
passes Windows and Clang; GCC has one SDK transport refusal at SDL video
initialization. A private pinned-SDL reproduction identifies default Xvfb's
last-client reset as a cause of the same failure pattern. Three default runs
fail and one passes; three `-noreset` runs complete all 2,400 window cycles.
Linux CI now keeps the server between clients, with its default screen, four
workers and suite unchanged. SDK initialization failures now print the actual
SDL error. [Build and tests](Build-and-tests) records the controls and exact
`9d83665` green checkpoint: all 1,362 selected tests pass across Windows, GCC and
Clang, without skips or removed test names. It predates source World loading;
the old hosted error strings remain unrecorded.

[Current status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md)
and the [testing record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md)
separate the newer Windows results, the `de4290b` executable-fingerprint failure,
the failed `8288290` hosted run, earlier green hosted baselines, focused
GL-diagnostic profiles and original comparisons. The earlier Windows 47-case and
Linux 53-case scopes remain recorded in [Build and tests](Build-and-tests).
These component comparisons and local checks do not establish complete original
frames or playable gameplay. Existing-save loading now completes; walking,
changing Blocks, and saving and reloading through the normal session remain
required.
