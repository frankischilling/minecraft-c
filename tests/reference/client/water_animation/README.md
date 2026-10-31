# Water texture animation

This package compares the supplied unchanged client `vs.a()V` with
`mc_client_water_texture_animate` on the genuine constructed water owner.
No OpenGL context, texture upload, Minecraft application, World or substitute
game class is involved. Binding `aw.a(ji)` and texture-manager registration
remain separate unfinished operations.

Run with explicit local inputs:

```text
python -B tests/reference/client/water_animation/compare.py \
 --client-jar <b1.7.3.jar> --native <mc_test_water_animation> \
 --java <JDK17-bin/java> --javac <same-JDK17-bin/javac> \
 --work-dir <ignored-directory>
```

The original adapter constructs actual `vs`/`aw`/Block objects, calls the
unchanged animation method, and reads actual arrays and the bootstrap
Math/Random holder. The fixture explicitly warms that genuine holder and
invokes its real public `Random.setSeed`; the private native observer performs
the corresponding state setup on its actual shared Math owner. Neither side
replaces an RNG, supplies random outputs to animation, or modifies game code.
This declared fixture controls only the animation comparison. Production
construction and animation use the normal borrowed profile and real lazy
Math holder; unbound entropy/allocation and delayed failure have separate
native controls. Constructor-clock/OS entropy equivalence is not asserted.

The trace independently verifies the holder's complete48-bit LCG continuation:
two steps for every reached Math call, with256 draws for a completed frame
and one draw before the short-j1 failure. Gaussian state stays unchanged.
All four float arrays, each raw RGBA byte, counter wrap, actual reference
identities and swaps are compared exactly. Native diagnostics separately
assert source array/Math failure PCs; Java stack traces establish actual vs
failures without inventing a bytecode index from a source line number.

The23 authored cases cover evolving waves, anaglyph mode, wrap, gradual
underflow, signed zero, infinities/clamps, noncanonical quiet NaNs, and nullable
or short arrays. Array mutations are declared finite field fixtures; normal
public owners expose readonly arrays. Native fixtures retain/restore the
actual constructor allocations for retirement rather than faking ownership.

One measured profile detail matters for raw NaNs: captured JDK17 x86-64
interpreter fadd keeps the right operand's payload when both operands are
NaNs. Ordinary C compilation chose the left payload. The C body performs the
actual binary32 addition, then explicitly preserves the selected sign/payload
and quiet bit. This follows the observed profile; it does not claim that Java
specifies a universal NaN payload. No tolerance, payload normalization or
image-only comparison hides these states. The original mismatching run stays
private as regression evidence.

Class/Code and whole-JAR pins are metadata in `source-pins.json`. Only the
three required private language/achievement resources are extracted after
hash verification, under a fresh ignored work directory. Runtime binding
verifies every selected game class comes from the supplied archive and the
Math/Random classes come from bootstrap java.base; per-run JDK class hashes
and explicit JDK17 tool versions are retained. Helpers compile to one fresh
class on a helper-plus-original-JAR classpath. Ambient Java options are
rejected and CLASSPATH is cleared. Runtime initialization logs reject LWJGL,
Minecraft and stats-file workers. No original class, resource, capture or
binary belongs in this public package.

The controller reuses the reviewed bounded statistics capture: explicit empty
reader errors, completed readers, actual process reaping, bounded output,
exit0 and empty stderr are required. Actual stdout banners are preserved with
`splitlines` only. Trace parsing checks exact ASCII records, lengths, IDs,
counter/holder continuation and terminal inventory. The public authored
wrapper runs native contracts and exact exit2/one-line malformed admissions;
crash or sanitizer diagnostics cannot count as rejection. Driver controls run
without a JAR under normal and optimized Python.
