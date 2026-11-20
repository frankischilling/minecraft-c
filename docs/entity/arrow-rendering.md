# Client Arrow rendering

The RenderManager's existing MC renderer now enters the complete Arrow draw
body and bridge. MC remains an ordinary BW renderer with its genuine parent
models and owner graph; Arrow remains an Entity without a LivingEntity layout.
The existing TH interpolation, brightness, class lookup and subsequent BW
postdraw retain their source order.

## Source operations

The supplied client method has 551 Code bytes and no exception handler. Both
previous angles comparing equal to zero return at 18, before texture, Display,
NW or Math work. Signed zero follows that guard; NaN proceeds. The bridge casts
at 2, while a null actor fails only at the typed method's first field read.

The body binds `/item/arrows.png`, pushes the current matrix, narrows each caller
coordinate to float, and reads old/current angles again for the two rotations.
It captures the actual NW singleton once at 88. Shake uses the current integer
`b`, float subtraction and the genuine IN sine helper only when positive.

The source transforms, current GL normals, rescale-normal flag and exact UVs
feed six real NW begin/draw calls and 24 source vertex calls: two end faces and
four shaft faces. NW's ordinary quad conversion produces six records per face,
including its actual duplicated and untouched words. MC sets the current GL
normal; it does not enable a synthesized per-vertex NW normal array or reset
NW's existing translation offsets. The method disables rescale-normal and pops
the current matrix only after every preceding call succeeds.

[Source identities](../../tests/reference/client/arrow_draw/README.md) pin the
complete selected methods and texture metadata. They are static evidence,
separate from executed original comparisons.

## Captured receiver lifetime

The source's mutable NW static field and the native allocation owner are
separate. A completed native allocation stays owned by its defining class
namespace even if that source field becomes null. Calls on a previously
captured genuine local still use that same receiver. Native inspection describes
the owned successful allocation or failed constructor candidate while reporting
the actual source field independently. Namespace destruction frees those owned
allocations once and never follows a borrowed replacement pointer.

The foreign-namespace replacement case is a native ownership control with an
explicitly live external owner. It does not establish unrestricted Java field
assignment or class-loader interoperability. Normal MC calls still use the
actual singleton captured from their graphics namespace.

## Failures and checks

The retained draw owner keeps the actual renderer, Arrow and reached services.
A source-null NW result fails at the first begin call, 244, after the preceding
transforms and current normal. Completed texture, matrix, rescale, normal and NW
effects survive later failures. Checked retirement does not invent a pop,
disable, draw completion or replay to undo a source prefix. First failure and
native cleanup refusal remain distinct.

The authored Windows contracts pass 15 fresh processes and 3,087 checks on an
RTX 3060 with OpenGL 3.3, NVIDIA driver 581.42. They cover ordinary and shaking
draws, fresh angle reads after a callback, captured singleton replacement,
the zero-angle guard, work limits, preexisting batches, null children and
retirement. The hidden windows use the requested monitor-2 bounds. The CPU
contracts pass on Windows and Clang Debug Linux with ASan/UBSan and leak
detection; they include 53 bridge, guard and allocation-lifetime checks.
Thirty-seven pure transport corruptions are rejected.

The first CPU fixture lacked Block initialization before TH's later Item active
use. Its retained failure identifies TH494 and the actual null Item constructor
read. The fixture now enters the genuine Block initializer outside the observed
Arrow method. A subsequent native run completed all checks but its controller
expected the device bootstrap print order; the CPU order was corrected exactly,
with the original failure and unchanged capture retained separately. Earlier
test-only compile and platform-link failures are also retained.

These are authored native contracts. No executed original MC draw, original
pixel comparison, natural Skeleton shot, complete loaded World frame or FPS
result follows from them. Arrow flight, collision and pickup remain separate
unfinished paths. The configured full-suite results and outstanding failures
are recorded in [testing](../testing.md).
