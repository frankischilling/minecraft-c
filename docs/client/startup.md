# Native client startup

`minecraft-client` is a C17 native executable using SDL3 and OpenGL 3.3 core.
The current client draws the Beta 1.7.3 title screen from locally imported
textures, translations and font data. It does not yet start a playable World.
The supplied original JAR is used only by the import tool; normal client startup
uses the imported files and needs no Java runtime or launcher.

Build with the default `MC_BUILD_CLIENT=ON`, then supply the asset import root:

```sh
./build/minecraft-client --assets /absolute/path/to/imported-assets
```

For Visual Studio builds, use `build/Debug/minecraft-client.exe`. The import
root contains `jar/`, `resources/` and the import manifest. See
[resource import](../reference/resources.md) for the local importer. SDL creates a
resizable 854-by-480 window by default. `--width`, `--height` and `--gui-scale`
override the initial dimensions and GUI scale. F11 toggles fullscreen; the
window close action and title's Quit Game button exit.

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
their buttons currently report an unavailable screen. Audio,
[application use of GameSettings](game-settings.md), child screens,
complete startup order, custom texture packs and gameplay remain open. The title
alone does not establish presentation or
input parity with the original client.

## Diagnostic runs and tests

`--frames N` exits after a finite number of drawn frames. `--hidden` requires
that limit. `--capture OUTPUT.png` writes the first completed frame before swap;
`--time-millis N` fixes only the splash animation's clock input. It does not fix
the default Random constructor or pretend to observe the original clock.
These flags support bounded startup checks; they are not original game options.

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
