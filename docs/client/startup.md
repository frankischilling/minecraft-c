# Native client startup

`minecraft-client` is a C17 native executable using SDL3 and OpenGL 3.3 core.
It draws the Beta 1.7.3 title screen from locally imported textures,
translations and font data. It does not yet start a playable World. After asset
import, normal startup needs neither the original JAR nor a Java runtime or
launcher. A separate [World inspection](world-renderer.md) renders a fixed area
of a real generated World; its diagnostic flag does not start gameplay.

Build with the default `MC_BUILD_CLIENT=ON`, then supply the asset import root:

```sh
./build/minecraft-client --assets /absolute/path/to/imported-assets --game-directory /absolute/path/to/game
```

For Visual Studio builds, use `build/Debug/minecraft-client.exe`. The import
root contains `jar/`, `resources/` and the import manifest. It is separate from
the game directory containing `options.txt`. See
[resource import](../reference/resources.md) and [GameDirectory](game-directory.md).
Without `--game-directory`, the client selects and, when required, creates the
native default game directory. Controlled runs should use a fresh explicit
directory.

SDL creates a resizable 854-by-480 window by default. `--width` and `--height`
set its initial dimensions; physical pixels can differ on a high-DPI display.
F11 toggles fullscreen. Window close and the title's Quit Game button exit.

## Settings at startup

The App now owns the [achievement notice/model constructor graph](achievement-notice.md)
before Display and Settings creation. Its Entity and render-class owners survive
menu/World changes, and each notice/model child retires before its Registry and
Entity namespace. This supplies construction and notification state; full startup,
notice drawing and Player ticking remain unfinished. The App also constructs
its real water-effect owner before statistics initialization and adopts a real
Session and StatsWriter, then installs the inventory-description callback after
writer publication. [Player statistics](player-statistics.md) describes that
ownership, reached dispatch and remaining filename profile.

The App creates Display first, then a real GameDirectory owner and one
file-backed [GameSettings](game-settings.md) owner. Settings load before the
menu's font texture, lazy title textures and GUI layout. A directory or native
settings failure stops startup. There is no asset-directory, temporary-directory
or SDL preference-path fallback. Source-caught load failures retain defaults
and any fields already changed, then allow later startup work to use that same
owner. No replacement settings object hides the failure.

GUI layout and resize read the retained `guiScale`. A supplied `--gui-scale N`
applies after load, including when N is 0 or negative. Zero requests automatic
scaling; a negative value leaves the scale factor at 1 in the source loop.
Without the flag, the loaded value is used. The override changes memory only.
Startup, resize and retirement do not save or rewrite `options.txt`.

Loaded initial anaglyph reaches the font-atlas upload during menu creation,
later background/logo/button texture uploads, and FontRenderer's palette
construction at first use. The menu retains that initial flag for its lifetime.
This does not provide the effectful texture-refresh setter or live RenderEngine
settings reads. Rendering still uses Default imported resources; a loaded
texture-pack name does not select or scan custom packs.

Bad-option and caught load messages go to stdout. The adapter writes counted
UTF-16 text as UTF-8 with the host line separator, retains embedded NUL,
combines surrogate pairs and substitutes `?` for an unpaired surrogate.
Native failures have their separate status and diagnostic path. No Java
Throwable or stack trace is fabricated.

The original startup also constructs its save manager before settings and
performs texture-pack, color-map, audio, input and screen-switch work around
these consumers. Those descendants and their exact order remain unfinished.
Original audio setup can save options; this App has no audio owner and performs
no such save. The [source contract](../reference/game-directory.md) separates
this implemented connection from complete original startup.

## Title rendering and input

The title uses the original font atlas's blue-channel advance calculation,
translated button labels, tiled dirt background, two logo quads and animated
splash text. Splash selection preserves file order and duplicate entries and
uses the shared Java Random implementation. Calendar greetings follow selection.
GUI scaling and mouse conversion use the original integer arithmetic.
The [GuiMainMenu renderer](gui-main-menu.md) supplies the shared title draw body
for the application and its conditional observation probe.
The [Timer](timer.md) supplies elapsed 20 Hz GUI ticks. Each tick drains mouse
events before keyboard events. Timer and GUI input continue when a native
drawable has zero size, using the last positive input geometry while drawing
waits. Inactive windows sleep ten milliseconds at the end of a frame, including
frames waiting for a drawable; loss of focus also leaves fullscreen. Actual
input comparisons with the original and fullscreen behavior remain unverified.

Singleplayer, Multiplayer, Mods and Texture Packs, and Options remain unfinished;
their buttons currently report an unavailable screen. Options/Controls UI,
physical key mapping, effectful settings setters, audio, custom texture packs
and gameplay remain open. Loaded performance mode does not yet select source
frame pacing. The title alone does not establish complete presentation or input
parity with the original client.

## Diagnostic runs and tests

`--frames N` exits after a finite number of drawn frames. `--hidden` requires
that limit. `--capture OUTPUT.png` writes the first completed frame before swap;
`--time-millis N` fixes only the splash animation's clock input. It does not fix
the default Random constructor or pretend to observe the original clock.
These flags support bounded startup checks; they are not original game options.

The new `client.game_directory` and optional graphics `client.startup` tests
pass on Windows MSVC x64 Debug. The directory test passes 57 native checks and
six independent file checks. The App completes nine natural-exit runs on an
NVIDIA RTX 3060 core context, passing all eight exact decoded-pixel relations
for the 854-by-480 physical profile. These cover scale overrides, initial
anaglyph, bad-line continuation and caught load errors using authored inputs.
No original image is compared. The later hosted run below also passes
`client.game_directory` on all three compilers and `client.startup` on Linux.
Ordinary default-directory and broader File/locale correspondence remain
unqualified.
See [test scope](../testing.md#startup-settings-and-game-directory).

The preceding [hosted checkpoint `ea1aefc`](https://github.com/frankischilling/minecraft-c/actions/runs/40150298841)
passes 224 MSVC, 228 GCC and 228 Clang cases, 680 in total, and 644-file
formatting. Its Linux jobs request software OpenGL under Xvfb; their actual
graphics vendor is not recorded. These are native startup tests, with no
original-image or ordinary-default comparison. The newer
[option actions](game-options.md#float-and-discrete-actions) pass one separate
Windows headless profile: 702 native checks and eight file-check categories.
No new App/client-mode run is part of that local profile. The subsequent
[hosted `2afd39a` checkpoint](https://github.com/frankischilling/minecraft-c/actions/runs/40154472178)
passes all 683 cases, including native actions on MSVC and sanitized GCC/Clang.
Real effect owners remain unqualified. The [sound resource foundation](sound.md)
is separate from App playback. Older rendering results below retain their
source and scene scopes.

Public tests cover the PNG reader, ScaledResolution, FontRenderer, Timer and resource
ownership, caching and failure handling. The optional `client.display` test
requires an actual desktop OpenGL context. It checks authored texture
orientation, nearest sampling, repeat, anaglyph conversion, alpha cutoff,
blending, local modelview geometry, source z clipping, both mixed-path
equal-depth orders and two-context ownership. It has no original
assets or original-client comparison.

The client resource loader currently admits only 8-bit RGB, palette and RGBA
PNG inputs. The generic PNG module also decodes grayscale and 16-bit samples,
but its conversion differs from Java ImageIO for those formats. They remain
explicitly unsupported by the client while that conversion is unfinished.
FontRenderer also rejects formatted non-ASCII text until the original
whole-String and default-locale lowercasing behavior is implemented.

Actual hidden title frames have rendered on Windows with an NVIDIA RTX 3060
and on Linux with Mesa llvmpipe. Those checks establish finite startup and
rendering results on those environments. Interactive play and audio remain
unfinished. Full regression results from earlier runtime checkpoints do not
qualify this later client increment.

The later shared-renderer frontend passes eight tests on MSVC and sanitized GCC/Clang,
plus a bounded 200-frame run on each. Windows posted-input controls additionally
verify window close, the title's Quit button, and resizing the real client area
to 512x384 before clicking Quit. They retain actual process status and owned
window identity, and exercise positive drawable sizes. Fullscreen and zero-sized
drawable behavior still need actual runtime qualification.

The [GuiMainMenu probe](gui-main-menu.md) draws through the same module as the
application. Its ordered FontRenderer commands retain source matrix operations,
glyph order and individual advances. A conditional Windows/NVIDIA scene matches
all 409,920 original RGBA pixels after reversing only the original row order.
Both Linux compilers retain one differing pixel against that Windows baseline.
Its cause remains unresolved; a same-driver original Linux baseline is still
needed. These results do not qualify complete original startup or input.
