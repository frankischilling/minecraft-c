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

## Retained Windows focus events

The event owner retains a native batch before dispatching Frame listeners.
A Frame focus-gained record can therefore reach its listener after a peer
window has already acquired focus. The listener previously focused Canvas
unconditionally, taking focus back from that peer. It now redirects focus only
while SDL still names this Frame as the keyboard owner. Current Canvas focus
also remains undisturbed.

An asset-independent Windows control creates the real Frame, embedded Canvas,
GL context and an auxiliary peer. Each of 200 cycles first gives Frame genuine
native focus, then activates the peer before the event-owner pump. The peer
must retain native, foreground and SDL keyboard focus after that pump; a
witnessed theft fails immediately. The positive control then activates Frame,
pumps its listener and requires native and SDL keyboard focus on Canvas, plus
actual polled pointer position `426,120`. It does not assign SDK coordinates or
fabricate focus events. All three window rectangles must fit physical monitor
2, and all three window IDs must disappear during checked teardown.

The strengthened control fails with the old listener: peer focus is confirmed
before pumping and Canvas owns it afterward. Restoring the guard passes all
200 cycles. The runner retains bounded stdout/stderr, timeout status, monitor
inventory and before/after fingerprints of the executable, sibling DLLs and
selected sources/helpers. These fingerprints are observation continuity checks,
not complete compiler or dependency provenance. Failed attempts remain retained.

```powershell
cmake --build BUILD --config Release --target mc_test_embedded_focus
python -B tests/tools/client/test_embedded_focus.py --executable BUILD/Release/mc_test_embedded_focus.exe --artifacts .local/embedded-focus
```

This control requires Windows and a physical second monitor. To register it
with CTest, also enable `MC_ENABLE_EMBEDDED_FOCUS_TESTS` and
`MC_ENABLE_CLIENT_DISPLAY_TESTS`. Its desktop resource lock prevents overlap
with the World-selection scenarios; separate test invocations must also run
serially.

The visible World-selection fixture now pumps the event owner before its native
pointer warp and observes actual mouse focus and local coordinates before
queueing button events. This corrects fixture preparation without changing SDK
polling. The fixed client passes save/title and ten actual reload epochs at
both 64-unit and 300-unit Windows game directories, with Frame and Canvas on
monitor 2. Five selected owner, screen-event and Python control CTests pass;
strict GCC syntax checking passes for the changed World-selection fixture.

The earlier observation of native Canvas/foreground ownership with no SDL
keyboard owner remains unresolved. The [native focus trace](../testing/native-focus-trace.md)
records 35 instrumented and 20 untraced current-build save/title cycles on
monitor 2. Every cycle passes; the selected traces contain no completed-step
Canvas/native-foreground/SDL-null candidate. Instrumentation can alter timing,
and the earlier failed binary lacks matching source/link proof. These finite
observations do not establish a causal repair, sustained interactive play,
original AWT parity, Linux focus behavior or an FPS improvement.
