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

## Active matrix observation

A separate experiment invokes a prepared, target-owned sampler through JDI
on the suspended GL thread. The sampler only reads GL matrices and three state
values into its own buffers. It does not set matrices, replay lists, replace
the clock or modify game fields. These getter invocations can affect driver
work and timing; this is an active observation profile, separate from the
passive capture above.

The actual draw completed with 38 matrix samples around rotation, scale and
16 font-list batches. All 38 helper invocations returned; target and observer
exited naturally with status 0 and complete owned-process cleanup. The clock
trace retained 13 records and the combined profile delivered 46 events. The
constructor selected `Thematic!`; its actual clock result was
`1792842596462`. Earlier captures remain unchanged.

An independent file-only model compared 40 arithmetic recipes at every sampled
boundary, checking 1,216 binary32 and 1,216 binary64 query coefficients per
recipe. Two indistinguishable angle recipes fit. Both round radians to binary32
before computing sine/cosine, then use binary32 matrix storage. Translation
rounds each product and addition separately, sums the x/y/z terms before adding
the old translation, and applies every glyph advance. A recipe using exact-degree
trigonometry differs in the rotation's sine coefficient. Independently decoded
font pixels reproduce all 256 constructor advances and all 16 batch counts.

This is a fit to reached command boundaries on the recorded NVIDIA backend.
Individual glyph-prefix matrices, vertex-transform contraction and other driver
implementations remain unobserved. Measured coefficients are comparison data,
not production renderer inputs.

## Native evidence and conditional comparison

Native Windows and Linux runs establish finite rendering and exit behavior.
Separate authored GL tests cover texture sampling, alpha cutoff, blending,
depth ordering and context ownership. Windows posted-input controls cover
window close, Quit, and positive-size resize followed by Quit. These input
checks do not establish original input semantics.

A matched conditional scene now uses the actual earlier clock, resources,
button state and the shared production renderer. Exact comparison of all
409,920 raw RGBA pixels matches every byte on Windows/NVIDIA after ordered
binary32 modelview replay. Only the original capture's row order is reversed.
Both Linux compilers retain one differing pixel at `(658,85)` against that
Windows baseline; alpha matches everywhere. The Linux comparison remains
failed, with its cause and a same-driver original Linux baseline unresolved.

A separate original Linux experiment creates and queries the requested
single-buffer RGBA8/depth24 Pbuffer on Mesa llvmpipe. Its original title method
returns and the passive observer records the actual clock, but `glFinish`
reports an X `BadMatch` before pixel readback. No raw frame is available from
that failed capture. Its target exits 1, its observer exits 0, and both owned
process trees close. This does not qualify a Linux original/native comparison.

The production renderer computes the selected arithmetic recipe from normal
GUI inputs and replays each local glyph and advance. It accepts no captured
coefficients or expected pixels. Actual display tests cover modelview geometry,
source z clipping, mixed rendering paths and equal-depth ordering in both
directions. The finite Windows match does not prove a universal legacy-driver
precision model. See the [GuiMainMenu contract](../client/gui-main-menu.md) for
the matrix operations and native checks. Full startup ordering, Random
constructor history, audio, child screens, focus/fullscreen and gameplay remain
open.
