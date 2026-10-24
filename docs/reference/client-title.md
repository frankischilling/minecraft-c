# Original title draw and native frontend

The reference is the supplied client JAR, 1,465,375 bytes, SHA-256
`af1fa04b8006d3ef78c7e24f8de4aa56f439a74d7f314827529062d5bab6db4c`.
The unchanged title draw is `fu.a(IIF)V`, with 274 Code bytes. Bytecode and
original observations guide the [native frontend](../client/startup.md).
The current implementation and the reference have separate validation scopes;
matched frame comparison remains unfinished.

## Reached original behavior

The no-World title draws a tiled background, two textured logo rectangles,
rotated yellow splash, version and copyright text, and its button list. It
does not draw a panorama. The draw reads its own `System.currentTimeMillis`
result at PC 133 for the splash animation. A fixed partial tick does not fix
that clock input.

Original `sj` owns font widths and legacy display lists; `ji` owns texture
loading and uploads, and `nw` supplies the tessellator. Genuine constructor
and class-initialization effects matter even in a draw-only experiment.
The native implementation uses OpenGL 3.3 core quads and its own font layout.
Equivalent resources or coherent-looking images alone do not prove equivalent
draw results.

## Executed original method

A private Windows experiment reached the unchanged method through genuine
original font, texture, button and title objects. The concrete Minecraft `kq`
receiver was allocated without running its constructor. Other recorded setup
included original constructors and reached Registry initialization. This was
an arranged receiver, not complete startup or an original window/input loop.

The draw used LWJGL 2.9.0 and an 854-by-480 offscreen Pbuffer on an NVIDIA
RTX 3060. The context reported OpenGL 4.6.0, driver 581.42, RGBA8, depth 24,
stencil 0 and single buffering. The native display requests a different
framebuffer profile. Live driver DLL mapping was not measured.

A passive JDI observer checked the actual class, loader, method Code and
constant pool. It stopped at the original clock call, enabled a method-exit
event for that thread, recorded the actual native return and disabled the
event. It did not replace the clock or write game fields. The retained
13-record trace covers the two surrounding gates, draw entry/exit, clock
return, VM death and disconnect. The second draw observed wall milliseconds
`1792830623516` and produced 1,639,680 raw RGBA bytes, SHA-256
`8535a396ecbda2334b0b183f369dba1535001bfc16a129697f13c32d1d7ce878`.
No alpha or color normalization was applied.

The first helper failed at teardown when original `ge.a()` threw
`BufferOverflowException`. Inspection of original `Minecraft.d()V` showed
that it catches and discards Throwable around that call. A second helper
recorded that same source-suppressed exception and completed Pbuffer cleanup.
It did not invoke the complete Minecraft shutdown method. Both target and
observer then exited naturally with status 0, and all eight recorded GL
error checks were empty.

The second controller still failed its final version check: it compared
JDI `VirtualMachine.version()`, defined as `java.version`, with
`java.runtime.version`. Those actual values were `17.0.17` and `17.0.17+10`.
A separate file-only audit corrected that exact comparison against the pinned
release file and passed the retained source, trace, frame, GL, cleanup and
process checks. The failed controller report remains unchanged; no redraw
occurred during the corrected audit.

## Native evidence and open comparison

Native Windows and Linux runs establish finite rendering and exit behavior.
Separate authored GL tests cover texture sampling, alpha cutoff, blending,
depth ordering and context ownership. Windows posted-input controls cover
window close, Quit, and positive-size resize followed by Quit. These checks
do not compare original input semantics or title pixels.

A matched comparison still needs the same complete conditional scene inputs,
the actual original clock result, compatible framebuffer state, and raw native
readback. It must exercise the production title renderer rather than a second
probe-only implementation. Full startup ordering, Random constructor history,
audio, child screens, focus/fullscreen and gameplay remain open.
