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
keyboard methods, ticks the menu and connects Quit to the App run flag. Other
menu destinations, World entry and gameplay remain explicit dependencies.

The preceding startup/menu-initialization Windows MSVC Debug source passes all
460 selected CTests without failures or skips. The selected normal Session and GL-error startup profiles
reach the source startup return; RenderGlobal device controls also pass. A
separate headless Release ALL build passes, without utility runtime or server
qualification. The new default runtime passes four focused Windows tests for
GuiIngame, uploads, source events and the native menu. Its run completes 100
frames and 32 ticks with cleanup; an 854 by 480 capture was inspected. All 463 selected Windows CTests pass. Hosted CI, Linux and original-game
correspondence retain their separate source scopes.
Full source run/PX rendering and `Minecraft.d()` shutdown parity are unproven;
native cleanup is a separate implementation.

[Current status](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/status.md)
and the [testing record](https://github.com/frankischilling/minecraft-c/blob/feature/normal-population/docs/testing.md)
separate the newer Windows results, the failed `8288290` hosted run, the earlier
green hosted baseline, focused GL-diagnostic profiles and original comparisons. The earlier Windows 47-case and
Linux 53-case scopes remain recorded in [Build and tests](Build-and-tests).
Neither these component comparisons nor the current local checks establish
original-JVM or visual correspondence, World entry or gameplay.
The next session milestone remains loading one World, walking, changing Blocks,
and saving and reloading.
