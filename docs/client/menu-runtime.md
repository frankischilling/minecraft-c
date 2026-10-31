# Default menu runtime

The default executable now uses the constructed source App and its initialized
GuiMainMenu. `runtime/bootstrap.c` creates the actual constructor and startup
services, local Session and imported-resource namespaces. The bounded startup
probe still stops before `Minecraft.run`32 after `Minecraft.a()` returns749.
The production `runtime/menu_loop.c` continues from that completed graph.

Singleplayer now opens the genuine [World-selection screen](world-selection.md).
Focused Windows empty and populated save-list runs pass, including drawing,
selection, same-parent Cancel, the World-load frontier and retained-screen
cleanup. Sixteen-save scrolling and real double-click profiles also pass.
Their captures were inspected. At the earlier menu checkpoint, all 468 selected
Windows CTests and formatting for 1,356 files passed. Local and hosted results
retain their exact source scopes in the [testing record](../testing.md).

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

The original menu action dispatch is connected. Singleplayer synchronously
constructs and initializes RQ; its real save rows can be selected during drawing.
Cancel returns to the same parent. Source input retains its original receiver
through the trailing input even after App.r changes. Quit and window close clear
the actual App run flag. Select constructs the actual controller and enters the
[source World loader](world-loading.md). Version-zero saves stop at conversion;
an existing-save load publishes the World, Player and movement input, attaches
the renderer and completes picking. Its healthy no-screen path now executes
[gameplay input](gameplay-input.md) through1073 and the admitted
[World/frame segment](renderer-ticking.md). The current finite loaded profile
also returns through World scheduling, terrain and the qualified Sheep draw;
other gameplay and rendering branches remain unfinished.
Create, Rename and Delete retain the precise
[unfinished prefixes](world-selection.md#transitions-and-lifetime).
Fullscreen, source F2 screenshots, debug timing, connection screens and several
gameplay actions retain explicit dependencies. Native `--capture` is a separate
observation option.

The FPS path reads and resets the real `dk.b` counter. First use initializes its
same-namespace Tessellator reference and zero field; it does not fabricate a
completed chunk renderer or a constant reported update count.

The frame's `Minecraft.run:262` call now lets the existing World light body
execute its source batch of at most 499 records. A zero native work allowance
previously stopped as soon as real queued work reached propagation. The
separate `load-light` control returns from two frames with explicitly prepared
paused entry flags, drains 500 actual records as 499 then 1, and checks saved
BlockLight, queue identity, World time, Random state and normal WorldAccess
callbacks. It does not qualify pause-screen transitions.

The focused repeat now proves two actual [Sheep draws](mob-drawing.md) across
four returned World/HUD frames, after Mob path following and the real
`Minecraft.k:1328` tick. The former retained TH call at `n.a:516` is a historical
stop. N now checks each returned TH child's retirement before continuing at
332 or 519. Copied scalar receipts prove the exact declared Sheep, canonical
XY renderer, distinct MW/EA models, first compilation and reuse, and enabled
terrain shadow. Their identity integers are never dereferenced after child
retirement.

The fixture's saved Entity lists are empty. After the first returned loaded
frame, a declared native input constructs and publishes one genuine Sheep in
the Player's view through ordinary constructor, position and World APIs.
Constructor entropy remains raw. The next frames use real elapsed time, the
normal Timer and World ticks. One frame consumes the N delay and another
returns from traversal while the unticked Sheep is culled. Neither counts as
a Sheep draw. The following two prove all twelve stable part/list identities,
72 compiled faces and 288 source vertex calls, followed by list reuse and
checked terminal cleanup. Display lists separately contain 36 NW-expanded
vertices per part, compared with 24 source vertex calls per part.

This repeat declares an existing Player whose genuinely loaded statistics
already contain the inventory achievement. A Player without that entry can
reach the unfinished active tutorial-notice draw at `vu.a:108` before the Mob
draw. Serialized Mob loading is not qualified by the Entity-empty saves.
The RenderManager retains its original 28 renderer owners; hand lookup adds
the actual LocalPlayer key sharing the Player renderer, producing exactly 29
Map entries. Other Mob renderers and nonempty TileEntity rows remain reached
dependencies. The direct original/native drawing comparison has the separate
prepared-context and device limits described in [Sheep drawing](mob-drawing.md).

Account authentication is excluded by the development specification. At
`Minecraft.k`11, tick6000 would start `kp`, whose `run()` builds the account-session
URL at11..50, requests it at53..65 and updates license status `H` at77 after a400
response. The local-name runtime omits that account worker and records the
omission in native diagnostics. It supplies no request, response or completed
worker. A prepared counter-boundary test checks continued menu operation.

## Scope and retirement

Native cleanup stops and joins workers and retires screens, renderers, effects,
sound and shared resources in dependency order. External descendants preserve
their live App dependencies through BUSY and retry, including an old RQ retained
after a later screen is published. This cleanup does not
establish the original `Minecraft.d()` shutdown sequence.

The full `Minecraft.run`, `Minecraft.k`, EntityRenderer/PX rendering and shutdown
methods are not qualified as complete original equivalents. The returned
Sheep fixture is finite native composition evidence. Other Entity and
TileEntity rendering, nondefault frame branches, remaining menus and gameplay
actions still require implementation or qualification. It does not establish
a playable client or whole-frame original/native parity.
