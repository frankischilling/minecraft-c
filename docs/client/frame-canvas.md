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
backend recognizes the focused child HWND under its foreground Frame. The X11
ConfigureNotify path frees the child array returned by XQueryTree and handles
a failed query before using its outputs. CMake compiles corrected copies of
these translation units and leaves cached dependency
sources untouched. File-specific include paths preserve the other SDL compile
commands and compiler-cache keys.

## Scope and limits

`client.frame_canvas_owner` exercises cold/foreign identities, native peers,
Graphics, retained event retries, callback reentrancy and checked ownership.
The four imported-resource standalone profiles use fresh isolated game
directories. They cover hidden and visible menu hosting, focus loss/recovery,
cursor positioning after resize without a Mouse poll, both native warp failure
prefixes, resize, native close and terminal cleanup. Fault profiles run in
separate processes because the failed Input owner is terminal.

These are authored native integration checks. Prepared observations of the
original AWT/SDK/MouseHelper boundaries do not establish a complete original
launcher, Frame or gameplay comparison. High-DPI behavior, fullscreen,
created-Display reparenting, default Frame icons and extended play remain
unqualified. Capability-zero Cursor construction retains a separate unfinished
branch. Loaded-World close, Chunk output, complete metadata diagnostics,
source save/detach/reload and the remaining gameplay actions still require their
own implementation and verification. See the [test record](../testing.md).

Enable `MC_ENABLE_CLIENT_DISPLAY_TESTS` and supply imported resources through
`MC_APPLICATION_STARTUP_IMPORTED_RESOURCES` to register the four standalone
profiles. Build `mc_test_standalone_canvas` and `mc_test_frame_canvas`, then run
`ctest --test-dir build --output-on-failure -R 'client\.(frame_canvas_owner|standalone_canvas_)'`.
On Linux, use the same X11/software-graphics and sanitizer environment as the
other native graphical tests. Each profile preserves its output under the
build directory's `standalone-canvas-contracts` directory.
