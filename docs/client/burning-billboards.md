# Burning Entity billboards

The shared Entity post-draw body now draws fire around burning actors. It reads
the real FireBlock static object's texture field and binds the terrain atlas
through the existing texture owner. No replacement Block or world lookup
supplies that texture.

The billboard scales with Entity width times 1.4 and divides height by that
scale. It uses the actual bounds minimum for its vertical offset and reads
the render manager's current view yaw. Each layer has four source vertex calls;
the existing tessellator expands the quad into six raw vertices. Layers
alternate atlas rows, flip U every two layers, narrow by 0.9 and advance in
depth while height and vertical offset decrease by 0.45.

Lighting is disabled before the Block access. The helper then pushes its
matrix, translates and scales, binds the atlas, reads bounds and yaw, sets
white, and begins the real tessellator batch. Completion draws that batch,
pops the matrix and enables lighting. It does not change blending, alpha or
depth settings, or set tessellator color, normal or translation fields.

Failures retain the reached matrix, texture, lighting and tessellator state.
Checked native retirement releases its own holders without finishing source
drawing or restoring later GL state. A preexisting tessellator writer remains
owned by its original caller. Native work limits bound nonprogressing height;
they do not turn that loop into an original return.

Seven authored profiles cover Spider, Slime, Squid and Creeper dimensions,
including zero height and both U directions. Eight fresh-process controls
cover work limits, NULL bounds, an existing batch, an external floating-point
fault, a NULL draw buffer, NaN height and infinite height. The ordinary tests
generate their own terrain atlas and Chunks and need no original assets or JVM.

The [reference packet](../../tests/reference/client/burning_billboard/README.md)
keeps unchanged-original observations separate from authored expectations.
Its prepared App identity and real World/render owners do not constitute
original App construction or a gameplay loop.

## Validation scope

Windows MSVC Debug and Linux Clang18 ASan/UBSan each pass 3,085 native checks
across the seven profiles and eight controls. The matching Mesa comparison
pairs 145,690 typed checks, 4,761 identities, fourteen actual receiver/actor
operands, seventy slot/invocation results and 2,784 raw tessellator words.
All eight words per vertex are compared, including inactive channels retained
from earlier shadow work. The thirteen actual shadow/fire draws retain the
existing four-ULP matrix rule. Ten copied-observation corruptions are rejected.

The ordinary original run, read-only JDI run and independent run without the
observer each return all seven calls without GL errors. Their fourteen raw
images agree exactly. The captures audit 747 supplied classes, 245 initialized
original classes per VM, 36 reached Code identities and 46 actual slot snapshots.
Raw constructor entropy stays independent across processes.

A separate Windows ordinary observation matches all fourteen native RGBA
images exactly on the recorded RTX3060 and NVIDIA581.42 driver. This compares
seven prepared before/after scenes across the actual SDK compatibility and
native core contexts. The frozen helper's later Linux-only metadata export
throws on Windows after those observations; that failure and the completed
SDK cleanup are retained. Its outer launcher finishes the class-origin audit,
without claiming that the frozen helper's whole main method returned.

All seven Linux baselines also match exactly. Its drawn images retain
84/4/37/19/64/18/7 differing pixels. No channel is omitted or image tolerance
added; full Linux image parity remains unqualified. The fixture preserves
incoming lighting while drawing its authored background, then disables it
before the observed call, in the same order as the original helper.

## Rotation precision

The live original comparison exposed a camera arithmetic difference. The
native backend retained double radians through trigonometry; the observed
Mesa rotation narrows radians before its float sine and cosine. One baseline
word changed, then later billboard rotation amplified that difference beyond
the existing four-ULP matrix limit. The backend now preserves that float
boundary, with a regression checking the actual observed baseline word.

The comparison limit is unchanged. Separate ordinary SDK observations show
that the recorded NVIDIA driver also differs from Mesa and CPU trigonometry
in other angles and compositions. A matching Mesa comparison cannot establish
full NVIDIA live-matrix parity. Original/native image parity remains a
separate check.

This implements the rendering helper. Fire propagation, complete damage/death,
general Frame/Canvas behavior, storage and a playable single-player session
retain their own unfinished work.
