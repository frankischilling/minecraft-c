# Standalone Frame and Canvas

The ordinary client entry owns a decorated Frame titled Minecraft and a
separate, borderless Canvas peer. Win32 `SetParent` or X11 `XReparentWindow`
embeds the real child window. Native parent queries check the relationship;
SDL popup or modal-parent flags do not stand in for embedding.

The startup branch follows `Minecraft.a()` through Canvas `getGraphics` at
11, black at 23, `fillRect` at 37, `dispose` at 41 and Display `setParent` at
48. The native Graphics owner uses a real GDI DC or X11 GC before GL context
creation. X11 accepts the peer's TrueColor or DirectColor visual only after
checking that its actual RGB-zero pixel is black. Failed Graphics calls retain
their native owner for checked retirement.

Display borrows the child window and owns its GL context. Its source parent,
Component listener and initial/current/effective DisplayMode occurrences remain
distinct. A parent-backed Display keeps its initial desktop current Mode;
creation and reshape use a separate Mode with the Canvas dimensions. Source
Display width/height getters read the live parent Component. Pixel dimensions,
App dimensions and stored SDK dimensions do not replace those fields.

The process-wide event owner first retains the entire observed SDL batch,
then dispatches Frame and Canvas window listeners. Native layout can remove
superseded SDL resize records, so callbacks must not run during batch transfer.
Numeric port identities and retained dispatch progress prevent a successful
listener from running again when a later global close listener fails. Retired
or newly attached ports cannot inherit that delivery.

Frame resizing lays out the child and updates its logical Component dimensions.
The borderless child also has SDL's resizable flag. This avoids X11's
nonresizable-window resize workaround moving it by cached top-level coordinates;
its actual parent-relative origin must stay at zero on both axes.
Display's Component listener requests an effective-Mode reshape. The game loop
then reads the Component dimensions in source order, clamps App dimensions and
reinitializes the current screen. Frame close invokes the existing App stop
method once. The current call finishes before native terminal cleanup releases
Display, App and finally the Frame peers. This host does not emulate Java
Thread.join or System.exit.

## Mouse and focus

The nonnull MouseHelper release method reads Component width and height,
divides each by two, calls Mouse `setCursorPosition`, then `setGrabbed(false)`.
The original NULL Component failure remains at the first width invocation.

Cursor positioning stores the actual Mouse and current-event coordinates before
its native warp. Grabbed or unsupported-capability paths instead store the
saved grab coordinates. Linux capabilities come from both actual Xcursor ARGB
and animation queries; Windows retains the SDK's literal capability one.
Releasing a grab preserves earlier source stores if a later native call fails.

Native cursor conversion and polling query the actual Win32 client rectangle
or X11 window attributes. SDL's size cache can lag a requested X11 resize until
ConfigureNotify is pumped. Neither cached Mouse dimensions nor that SDL cache
can supply the reached native height. Cursor positioning itself does not poll
Mouse, pump events, reset deltas or consume the input queue.

Two checked SDL source corrections support the native host. The Windows event
backend recognizes the focused child HWND under its foreground Frame. When the
actual focused window is a known SDL `WS_CHILD` under that same foreground
root, the parent yields rather than stealing the child's SDL keyboard focus.
Foreign roots, ordinary native/IME children and popup/toplevel behavior keep
their existing paths. The X11 ConfigureNotify path frees the child array
returned by XQueryTree and handles
a failed query before using its outputs. CMake compiles corrected copies of
these translation units and leaves cached dependency
sources untouched. File-specific include paths preserve the other SDL compile
commands and compiler-cache keys.

## Scope and limits

`client.frame_canvas_owner` exercises cold/foreign identities, native peers,
Graphics, retained event retries, callback reentrancy and checked ownership.
The five imported-resource standalone profiles use fresh isolated game
directories. The original four are `hidden`, `visible`, `visible-fp-position`
and `visible-fp-ungrab`. They cover hidden and visible menu hosting, focus
loss/recovery, cursor positioning after resize without a Mouse poll, both native
warp failure prefixes, resize, native close and terminal cleanup. The fifth,
`visible-reactivation`, repeatedly switches between a real auxiliary window and
the Frame/Canvas host. Fault profiles still run in separate processes because
the failed Input owner is terminal. Together with the owner test, these make
six selected tests.

All six Windows owner/standalone tests pass in the current seven-test selection,
which also includes the two-mode Save and quit test. The final isolated Linux
invocation passes the same six Canvas tests in 10.602 seconds on private Xvfb
with Openbox, llvmpipe LLVM 20.1.2 and Mesa 25.2.8, using a 4.5 core context.
ASan leak detection and UBSan halt-on-error are enabled. Source, product and
asset hashes match across that run; its ten recorded cleanup entries pass and
the private X authority is removed.

An earlier isolated run passes in 9.82 seconds and has an independent read-only
audit. It records 64 real auxiliary-window to Frame/Canvas focus cycles and
128 WM transitions, with SDK keys, grab and native warp actually exercised.
Reported capabilities are seven with warp capability one; the warp checks are
not skipped. Raw WM state names the Frame while native input focus belongs to
its Canvas. Source, product, asset, tool and CTest-registration hashes match
across that run. Its ten recorded cleanup entries pass; a later read-only audit
finds no active members in the eleven recorded command sessions. Cleanup
evidence covers those recorded PIDs and sessions. These runs qualify the stated
isolated X11 configuration. Earlier WSLg warp and native auxiliary-window focus
failures remain retained and unresolved.

These are authored native integration checks. Prepared observations of the
original AWT/SDK/MouseHelper boundaries do not establish a complete original
launcher, Frame or gameplay comparison. High-DPI behavior, fullscreen,
created-Display reparenting, default Frame icons and extended play remain
unqualified. Capability-zero Cursor construction retains a separate unfinished
branch. Loaded-World native close, complete metadata diagnostics, ordinary World
reselection and the remaining gameplay actions keep their separate requirements.
The [Windows Tiny Save and quit profiles](world-saving.md) now have bounded
normal leave and session-failure/cleanup evidence. They supply no Linux visible
save/title or complete original World/launcher comparison. See the
[test record](../testing.md).

Enable `MC_ENABLE_CLIENT_DISPLAY_TESTS` and supply imported resources through
`MC_APPLICATION_STARTUP_IMPORTED_RESOURCES` to register the five standalone
profiles. Build `mc_test_standalone_canvas` and `mc_test_frame_canvas`, then run
`ctest --test-dir build --output-on-failure -R '^client\.(frame_canvas_owner|standalone_canvas_)'`.

To reproduce the isolated Linux scope, first start a private authenticated Xvfb
server and run Openbox on that server. Export its `DISPLAY` and `XAUTHORITY`;
CTest does not create the server or start the window manager. With those
processes already running, select the six tests under the measured environment:

```sh
env SDL_VIDEO_DRIVER=x11 LIBGL_ALWAYS_SOFTWARE=1 \
    ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
    ctest --test-dir build --output-on-failure \
      -R '^client\.(frame_canvas_owner|standalone_canvas_(hidden|visible|visible-fp-position|visible-fp-ungrab|visible-reactivation))$'
```

Each standalone profile preserves its output under the build directory's
`standalone-canvas-contracts` directory. Retain and retire the Xvfb, Openbox and
child process identities belonging to that run; the isolated result supplies
no WSLg desktop qualification.
