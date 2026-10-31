# Display and input ownership

The staged Display API separates a counted pending Mode and title from actual
window, input and GL construction. Mode construction stores signed source
dimensions without clamping. `setDisplayMode` publishes the same Mode before
the created-window test; `setTitle` retains the same String before the live
setter. Source dimensions, native window coordinates, framebuffer pixels and
App `d/e` remain separate values.

`mc_client_display_stage_allocate` has no video or device effects. Bind a real
READY `mc_client_sdk_icons` occurrence before first SDK active use. A resource
provider rooted at the imported asset directory supplies the bounded RGBA files.
The provider and its context must outlive every icon occurrence and pending
failed-close retry. The staged Display and installed native icon child hold
independent occurrences. Windows installs separate 32-pixel and 16-pixel native
icons; the selected X11 body installs the first 32-pixel image with real color
and mask Pixmaps and WM_HINTS.

Asset import accepts `--lwjgl-jar` in addition to the existing game JAR and
resource arguments. It verifies the complete supplied SDK archive, utility
class, Code bodies and constant-pool operands, decodes modified UTF8 to exact
UTF16 code units, and emits only the two low-byte RGBA assets and their private
provenance. The running client reads those imported files. It does not inspect
a JAR, start Java or depend on a launcher. SDK pixels and class files do not
belong in the public source tree. See [the icon notice](../../licenses/lwjgl-icons.txt).

One main-thread SDL transport owns pumping and event removal. Every Display has
a port. Audited string and array payloads are copied before the SDL head is
removed and remain alive through the consumer's ticket. Taking a record never
pumps. Unsupported opaque payloads remain at the SDL head with a typed failure;
native allocation bounds preserve earlier retained records. Ordinary input
consumption and explicit terminal retirement are separate operations.

The SDK EventQueue owns 200 records independently of that native transport.
Mouse and Keyboard use actual 22-byte and 18-byte frames and 1100-byte and
900-byte read buffers. Full queues return the source false result without
consuming the incoming frame. Read/copy follows the actual flip, limit, put and
compact sequence, including partial records. Mouse grab applies the proven
Windows or Linux queue reset at its source layer; it preserves unrelated key
and window records and does not replay deliberately cleared mouse events.

The concrete ordinary profile creates real Windows/X11 input attachments,
owned names/maps/buffers, backend state and text input. Physical-device presence
is observed separately from logical device creation. Update performs the actual
visibility/dirty conditional swap, resize stores, optional message pump, Mouse
poll/cursor, Keyboard poll and deferred Controllers check in order. Errors
retain complete typed children, caller sites and completed swap/pump/store
effects. Native policy or platform failures are not fabricated Java exceptions.

Calls require the SDL main thread and live, exclusive owners. Native callbacks
cannot reenter or retire active Display, input, transport or icon operations.
BUSY preserves admitted occurrences and outputs. Failed partial construction
remains owned. Terminal retirement may preserve a failed platform child for
retry. When destructive cleanup consumes the Display while reporting an earlier
GL error, the actual slot is NULL and `native_owner_retained` is false; callers
must inspect both rather than assume every false result preserves the owner.

The coherent startup graph compiles on MSVC, Clang and GCC. The selected local
matrix passes the SDK queue/icon and protocol/importer controls; see the exact
[qualification scope](../testing.md#normal-startup-owner-qualification). Complete
physical input, SDK window/update behavior and original correspondence remain
unqualified. The finite source profile excludes created-window mode
reconfiguration, fullscreen/AWT parents, nonempty physical Controller mapping,
explicit animated Cursor, nondefault SDK properties, IME preedit, other window drivers and complete
Java SDK reflection/shutdown/exception behavior. SDL transport timestamps retain
SDL's origin; raw Windows/X11 event, timestamp, coalescing and pixel parity are
not claimed. The verified game mouse filter uses wall time and App `P`, so no
unjustified epoch conversion is introduced.

## Startup GL setup and diagnostics

The normal startup caller now performs its selected GL setup through offset
460 and both surrounding diagnostics. Depth/alpha and projection/modelview
operations use this same Display and source surface. The current source path
stops before cx470; broader rendering, lighting/fog and the game loop have
their own unfinished methods.

Each `Minecraft.c(String)` call performs one glGetError, without a pre-drain,
post-drain or frame reset. A nonzero result uses the finite GLU mapping and
three independently captured System.out receivers. StringBuilder text/int
formatting preserves counted UTF-16 and argument order, including arguments
evaluated before a null receiver fails at its println invocation. The call
retains completed effects and full child errors until explicit native cleanup.
The [Core output owner](../core/java-writer.md#systemout-and-base-printstream)
supplies real buffered output; the caller does not substitute PrintWriter or
write a cached global stdout pointer directly.

Fresh Windows normal and nonzero-error startup profiles pass 103 and 109
checks. Focused Linux device tests use real Display contexts with ASan, UBSan
and leak detection. Neither result establishes original GL/SDK pixel, event
or timestamp correspondence, or Linux default-directory/Session startup.

The old diagnostic frontend consumes the same retained event transport, with
its existing menu and inspection behavior. Loading draw still stops at the
separately recorded `Display.update` call; this package alone does not implement
the full application loop, World entry or a playable client.
