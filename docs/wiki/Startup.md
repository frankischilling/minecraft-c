# Client startup

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
World, completes 289 terrain samples and publishes its real Player/input before
renderer attachment at bytecode 220. Version-zero conversion stops at 52.
Create/Rename stop at `NEW162`/`NEW189`; Delete stops at preparation 21 without
skipping its prefix. Complete World entry and gameplay remain unfinished. See the
[World-selection contract](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/client/world-selection.md).

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
other profiles fail explicitly. The current full suite and formatting check
pass: all 475 selected Windows CTests without failures or skips and formatting
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
Neither these component comparisons nor the current local checks establish
original-JVM or visual correspondence, World entry or gameplay.
The next session milestone remains loading one World, walking, changing Blocks,
and saving and reloading.
