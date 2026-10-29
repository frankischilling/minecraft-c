# Native client startup

`minecraft-client` is a C17 executable using SDL3 and OpenGL 3.3 core. Its new
default path constructs the source App, completes the no-connection startup and
continues through the [no-World menu runtime](menu-runtime.md). World entry and
gameplay remain unfinished. Four focused Windows tests pass, including a
100-frame, 32-tick native menu run with cleanup and an inspected 854 by 480
capture. All 463 selected Windows CTests pass. Hosted CI, Linux and original
correspondence retain their separate source scopes in the testing record.

After [resource import](../reference/resources.md), it requires neither the
original JAR, Java nor the launcher. Build with `MC_BUILD_CLIENT=ON`, then run:

```sh
./build/minecraft-client --assets /absolute/path/to/imported-assets --username Player
```

Visual Studio builds place the executable under `build/Debug/`. The import root
and native default [game directory](game-directory.md) are separate. Normal
startup loads Settings from that game directory, saves options during sound
setup and saves statistics during screen publication. It does not substitute
the import directory for the game directory or select a temporary fallback.

The default initial window is 854 by 480. `--width` and `--height` select initial
dimensions; physical pixels can differ on a high-DPI display. `--frames N`
limits a native observation, `--hidden` requires that limit, and `--capture`
requests a completed-frame PNG. These are development options, not original game
settings. The default path currently rejects `--game-directory` and
`--gui-scale` overrides; it uses the actual default directory and loaded scale.

## Source startup and menu

The [normal startup owner](normal-startup.md) constructs the actual Timer,
loading/achievement/model and sound owners, Display, save manager,
[Settings](game-settings.md), selected Pack, RenderEngine, bitmap Font, color
maps, EntityRenderer, RenderManager and statistics. It creates input, registers
nine texture effects, constructs RenderGlobal and particles, starts the resource
worker, then publishes GuiIngame and the initialized main menu.

The bounded startup probe intentionally stops at `Minecraft.run`32 after source
startup returns749. The production native bridge continues from that same
completed graph. The [architecture](../architecture.md),
[GameDirectory source contract](../reference/game-directory.md) and
[development status](../status.md) separate implementation from verification.

The menu loop uses the canonical [Timer](timer.md), SDK input queues and source
screen owners. It handles mouse events before keyboard events and advances the
actual GuiMainMenu/GuiIngame counters. The [drawing bridge](gui-main-menu.md)
reads the source splash, button labels, positions and flags, checks resource
identities and canonical Font widths, and uses the existing OpenGL kernels.
The admitted default-pack/no-World profile is narrower than complete rendering.

Quit and window close use the actual App run flag. Singleplayer, Multiplayer,
Mods and Texture Packs, and Options retain explicit unfinished destination
screens. Fullscreen, source F2 screenshots, debug timing and focused gameplay
input also retain explicit dependencies. [Option actions](game-options.md),
[achievement notices](achievement-notice.md) and
[Player statistics](player-statistics.md) have their own component scopes.

Native teardown retires the live graph and joins workers. It does not establish
the full original `Minecraft.d()` shutdown sequence. Complete source frame/PX
rendering, World entry, dynamic terrain, menus and gameplay remain unverified
or unfinished.

## Explicit diagnostic frontend

`--inspect-menu`, `--time-millis` or `--inspect-world` select the existing
separate diagnostic entry. Its title renderer and finite
[World inspection](world-renderer.md) retain their own options and test history.
The World view is a fixed inspection, without Player controls or implicit save.

An isolated diagnostic menu can use an explicit directory and fixed splash time:

```sh
./build/minecraft-client --assets /absolute/path/to/imported-assets --inspect-menu --game-directory /absolute/path/to/fresh-diagnostic-game --time-millis 0 --frames 2
```

That diagnostic frontend supports its earlier scale/settings experiments.
Those overrides and earlier captures do not qualify the default source runtime.

## Diagnostic runs and tests


`--frames N`, `--hidden` and `--capture OUTPUT.png` provide bounded native
observations. Hidden runs require a frame limit. In the separate diagnostic
frontend, `--time-millis N` fixes only the splash clock. It does not fix default
Random entropy or represent an original clock observation.

## Earlier diagnostic qualification

The results below belong to the explicit diagnostic frontend and its earlier
source snapshots. They do not qualify the new default constructed-source loop.

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
