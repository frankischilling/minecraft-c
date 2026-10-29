# Default menu runtime

The default executable now uses the constructed source App and its initialized
GuiMainMenu. `runtime/bootstrap.c` creates the actual constructor and startup
services, local Session and imported-resource namespaces. The bounded startup
probe still stops before `Minecraft.run`32 after `Minecraft.a()` returns749.
The production `runtime/menu_loop.c` continues from that completed graph.

This is a no-World menu increment. All 463 selected Windows MSVC Debug CTests pass,
including GuiIngame, texture uploads, source screen events and the native menu loop. The
menu run completes 100 frames and 32 ticks with cleanup; its 854 by 480 capture
was inspected. Hosted CI, Linux and original-game correspondence retain their
separate source scopes in the [testing record](../testing.md).

## Launch and files

```sh
./build/minecraft-client --assets /absolute/path/to/imported-assets --username Player
```

Visual Studio builds place the executable under `build/Debug/`. The import root
contains the `jar/` class resources and `sdk/` icons. The runtime uses the real
native default game directory and loaded Settings. Normal startup can write
options during SoundManager setup, save statistics during screen publication
and scan/register local sound resources. It does not use the import directory
as the game directory. Usernames remain local and require no account login.
Import the icons with the verified `--lwjgl-jar` prerequisite described in
[resource import](../reference/resources.md#local-import-and-validation).

`--width`, `--height`, `--frames`, `--hidden` and `--capture` provide native launch
and bounded-observation controls. Hidden runs require a frame limit. The normal
path currently rejects `--game-directory` and `--gui-scale` overrides.
`--inspect-menu`, `--time-millis` or `--inspect-world` select the separate
diagnostic frontend, whose results and options retain their own scope.

## Frames, ticks and input

The menu loop uses the existing Timer and canonical SDK Display/input owners.
Its reached no-World tick services statistics and GuiIngame, handles screen
mouse events before keyboard events, and calls the actual screen-particle and
GuiMainMenu ticks. The screen's float counter and GuiIngame counters live in
their real owners. RenderEngine texture updates call the registered effects and
upload their pixels through the source texture path. Input draining adds no
second SDL queue or implicit pump.

Drawing reads the actual screen dimensions, splash and button list through the
[source-screen bridge](gui-main-menu.md#drawing-the-source-screen). It checks
shared resource identities and canonical bitmap Font widths before using the
existing OpenGL 3.3 kernels. The admitted profile has the default texture pack,
no World and the same captured/current Font. Empty screen particles and the
genuine no-notice achievement branch complete without inventing active drawing.

The original menu action dispatch is connected. Quit clears the actual App run
flag; the other buttons retain explicit unfinished screen-construction routes.
Window close uses the same run flag. Fullscreen, screenshots through the source
F2 action, debug timing, connection screens, World entry and gameplay input still
have explicit dependencies. Native `--capture` is a separate observation option.

The FPS path reads and resets the real `dk.b` counter. First use initializes its
same-namespace Tessellator reference and zero field; it does not fabricate a
completed chunk renderer or a constant reported update count.

Account authentication is excluded by the development specification. At
`Minecraft.k`11, tick6000 would start `kp`, whose `run()` builds the account-session
URL at11..50, requests it at53..65 and updates license status `H` at77 after a400
response. The local-name runtime omits that account worker and records the
omission in native diagnostics. It supplies no request, response or completed
worker. A prepared counter-boundary test checks continued menu operation.

## Scope and retirement

Native cleanup stops and joins workers and retires screens, renderers, effects,
sound and shared resources in dependency order. External descendants preserve
their live App dependencies through BUSY and retry. This cleanup does not
establish the original `Minecraft.d()` shutdown sequence.

The full `Minecraft.run`, `Minecraft.k`, EntityRenderer/PX rendering and shutdown
methods are not qualified as complete original equivalents. World simulation,
dynamic terrain rendering, remaining menus and all gameplay still require
integration. A rendered interactive menu does not establish a playable client.
