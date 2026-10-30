# Standard lighting state

The source World frame executes the complete `u.b()V` lighting setup at
`px.a(FJ)V:496`, after terrain submission. It then obtains the captured camera's
pooled eye position at 504 and reaches Entity/TileEntity rendering at 510.
That renderer remains unfinished. There is no completed gameplay frame yet.

## Source order and ownership

The shared U class initializes its actual `ge.e(16)` direct FloatBuffer before
the first Display operation. Setup enables lighting, lights 0 and 1, and color
material, then selects front-and-back ambient/diffuse tracking. Two direction
vectors and their normalized results use four entries in the canonical BT pool.
MathHelper initializes inside the first normalization at `bt.c:29`, after the
first five Display effects. No caller resets the pool.

Nine helper calls clear, put four floats into, and flip the same U buffer.
Uploads borrow its remaining span without advancing the cursor. The final
buffer has position 0, limit 4 and capacity 16, containing `(0.4, 0.4, 0.4, 1)`
in its first four cells. Cells 4 through 15 retain their previous bytes.

Both lights receive direction, diffuse, ambient and specular parameters.
POSITION uses the modelview matrix at upload time, even when projection is
selected. Later matrix changes leave stored eye-space positions unchanged.
Setup ends with flat shading and global ambient `(0.4, 0.4, 0.4, 1)`.

The call retains the same graphics, Entity and Math namespaces when reached.
It releases native holds on return without restoring source state. Typed errors
retain the U operation, nested helper or BT/Math cause, and counts of completed
Display calls, buffer fills and pool acquisitions. A failed child preserves all
earlier changes, including sticky GL errors. The World frame holds the returned
eye vector until its own retirement, before releasing the camera.

## Verification and remaining drawing

The [original observations](../../tests/reference/client/lighting_enable/README.md)
cover five fresh processes: repeated first-child failure without a context,
cold setup, seeded buffers and vectors, nonidentity modelview with projection
selected, modelview replacement after upload, and two warm calls without a reset.
Native Windows MSVC Debug replay matches all 86 state and parameter rows exactly.
The measured original device is an NVIDIA GeForce RTX 3060 with driver 581.42.

Three focused Windows CTests pass in 2.05 seconds. Additional native controls
cover default light state, nonconsuming and short buffer spans, invalid inputs,
unsupported FP controls, compilation rejection, sticky GL errors and a retained
MathHelper allocation failure after five Display effects and one pooled vector.
These native resource limits are separate from the original observations.

The loaded-World test passes in 18.59 seconds with all 72 original focus
histories and unchanged saves/resources. Its actual frame reaches N510 after
lighting and eye-position return. The inspected back buffer still shows authored
grass terrain and Sky without HUD. The real `minecraft-client` executable also
renders and captures two normal menu frames from imported assets, with an
isolated game directory and no JVM/JAR runtime, then exits cleanly.

Hosted [source checkpoint 0c0cc5a](https://github.com/frankischilling/minecraft-c/actions/runs/40968986848)
passes all four jobs and 1,520 tests: Windows 484/GCC 518/Clang 518, with zero
failures/skips and no dropped preceding names. GCC and Clang run the 86-row
lighting replay and device controls under sanitizers. Hosted Windows runs the
CPU control; the local Windows device evidence above remains separate.

Lit geometry now uses the canonical [vertex-lighting backend](lighting-raster.md),
with separate normal, material, raster and compiled-list observations. The
86-row U capture above retains its state-only scope. The completed prefix
leaves lighting enabled; native cleanup does not reset it. Entities, particles
and the later frame tail remain unfinished. These finite comparisons do not
establish repeated interactive frames.
